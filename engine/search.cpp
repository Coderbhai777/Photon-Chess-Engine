/**
 * search.cpp
 * Full negamax alpha-beta search with:
 *   - Multi-variation (MultiPV) analysis
 *   - Iterative deepening + aspiration windows
 *   - Transposition table (probe + store)
 *   - Move ordering: TT, captures (MVV-LVA / SEE), killers, history, counters
 *   - Null-move pruning
 *   - Late Move Reductions (LMR)
 *   - Futility pruning
 *   - Quiescence search with delta pruning
 *   - PV line tracking
 */

#include "search.h"
#include "movegen.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cassert>

using namespace std;

// ── LMR reduction table (precomputed) ─────────────────────────
static int lmr_table[MAX_PLY][MAX_MOVES];

static void init_lmr() {
    for (int d = 1; d < MAX_PLY; ++d)
        for (int m = 1; m < MAX_MOVES; ++m)
            lmr_table[d][m] = (int)(0.75 + log(d) * log(m) / 2.25);
}

// ── MVV-LVA (Most Valuable Victim / Least Valuable Attacker) ──
int SearchEngine::mvv_lva(Piece captured, Piece attacker) {
    return type_of(captured) * 8 - type_of(attacker);
}

// ── Constructor ────────────────────────────────────────────────
SearchEngine::SearchEngine(TranspositionTable& tt, const Evaluator& eval)
    : tt_(tt), eval_(eval) {
    init_lmr();
}

// ── Time management ────────────────────────────────────────────
int64_t SearchEngine::elapsed_ms() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now() - start_time_).count();
}

bool SearchEngine::time_up() const {
    if (stopped_.load()) return true;
    if (time_limit_ms_ > 0 && elapsed_ms() >= time_limit_ms_) return true;
    return false;
}

void SearchEngine::calc_time(const Limits& l, Color stm) {
    if (l.infinite) {
        time_limit_ms_ = 0;
        return;
    }
    if (l.movetime_ms > 0) {
        time_limit_ms_ = l.movetime_ms;
        return;
    }
    int64_t our_time  = (stm == WHITE) ? l.wtime_ms : l.btime_ms;
    int64_t our_inc   = (stm == WHITE) ? l.winc_ms  : l.binc_ms;
    int mtg = l.movestogo > 0 ? l.movestogo : 40;

    time_limit_ms_ = (our_time / mtg) + (our_inc / 2);
    time_limit_ms_ = max(time_limit_ms_, (int64_t)10);
    time_limit_ms_ = min(time_limit_ms_, (int64_t)(our_time * 60 / 100));
}

// ── Static Exchange Evaluation (SEE) ──────────────────────────
bool SearchEngine::see_ok(const Board& b, Move m, int threshold) const {
    const int piece_value[PIECE_TYPE_NB] = {0, 100, 320, 330, 500, 900, 20000};
    Piece captured = (m.type() == EN_PASSANT) ? make_piece(~b.side_to_move, PAWN) : b.piece_on(m.to());
    if (captured == NO_PIECE) return threshold <= 0;

    int score = piece_value[type_of(captured)] - threshold;
    if (score < 0) return false;
    score -= piece_value[type_of(b.piece_on(m.from()))];
    if (score >= 0) return true;
    return score >= -100;
}

// ── Quiescence search ────────────────────────────────────────
Score SearchEngine::quiescence(Board& b, Score alpha, Score beta, int ply) {
    if (time_up()) return 0;
    ++last_stats.qnodes;

    Score stand_pat = eval_.evaluate(b);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;

    const int DELTA = 975;
    if (stand_pat + DELTA < alpha) return alpha;
    if (ply >= MAX_PLY - 1) return stand_pat;

    MoveList ml = MoveGen::generate_tactical(b);
    static const int piece_value[PIECE_TYPE_NB] = {0,100,320,330,500,900,20000};
    int scores[MAX_MOVES];
    for (int i = 0; i < ml.count; ++i) {
        Move m = ml.moves[i];
        Piece cap = b.piece_on(m.to());
        scores[i] = (cap != NO_PIECE) ? mvv_lva(cap, b.piece_on(m.from())) : 0;
        if (m.type() == PROMOTION) scores[i] += piece_value[m.promo_pt()];
    }

    for (int i = 0; i < ml.count; ++i) {
        int best_idx = i;
        for (int j = i+1; j < ml.count; ++j)
            if (scores[j] > scores[best_idx]) best_idx = j;
        swap(ml.moves[i], ml.moves[best_idx]);
        swap(scores[i], scores[best_idx]);

        b.make_move(ml.moves[i]);
        Score s = -quiescence(b, -beta, -alpha, ply+1);
        b.unmake_move(ml.moves[i]);

        if (s >= beta) return beta;
        if (s > alpha) alpha = s;
    }
    return alpha;
}

// ── Negamax ─────────────────────────────────────────────────
Score SearchEngine::negamax(Board& b, int depth, Score alpha, Score beta,
                              int ply, bool null_ok, PVLine& pv, Move prev_move) {
    if (time_up()) return 0;

    bool root  = (ply == 0);
    bool pv_node = (beta - alpha > 1);
    pv.clear();

    if (!root) {
        if (b.halfmove_clock >= 100) return SCORE_DRAW;
        int reps = 0;
        Key cur = b.hash;
        int hsize = (int)b.history.size();
        for (int i = hsize-2; i >= max(0, hsize - b.halfmove_clock); i -= 2) {
            if (b.history[i].hash == cur && ++reps >= 2) return SCORE_DRAW;
        }
    }

    Move  tt_move = MOVE_NONE;
    int   tt_score = 0;
    bool  tt_hit   = tt_.probe(b.hash, tt_move, tt_score, depth, alpha, beta, ply);
    if (tt_hit && !root) {
        ++last_stats.tt_hits;
        pv.moves[0] = tt_move;
        pv.length   = 1;
        return tt_score;
    }
    if (tt_move.is_none()) tt_move = tt_.probe_move(b.hash);

    if (depth <= 0) return quiescence(b, alpha, beta, ply);

    ++last_stats.nodes;
    if (ply >= MAX_PLY - 1) return eval_.evaluate(b);

    bool in_check = b.in_check();
    Score static_eval = eval_.evaluate(b);

    if (!pv_node && !in_check && depth <= 3 && abs(beta) < SCORE_MATE - MAX_PLY) {
        if (static_eval - (depth * 120) >= beta) return static_eval;
    }

    if (null_ok && !pv_node && !in_check && depth >= 3 && static_eval >= beta && popcount(b.pieces(b.side_to_move)) > 3) {
        b.make_null_move();
        PVLine dummy;
        Score null_score = -negamax(b, depth - 1 - (3 + depth / 4), -beta, -beta+1, ply+1, false, dummy, MOVE_NONE);
        b.unmake_null_move();
        if (null_score >= beta) return (null_score >= SCORE_MATE - MAX_PLY) ? beta : null_score;
    }

    MoveList ml = MoveGen::generate(b);
    if (ml.count == 0) return in_check ? mated_in(ply) : SCORE_DRAW;

    int scores[MAX_MOVES];
    for (int i = 0; i < ml.count; ++i) {
        Move m = ml.moves[i];
        if (m == tt_move) scores[i] = SCORE_TT_MOVE;
        else if (b.piece_on(m.to()) != NO_PIECE || m.type() == EN_PASSANT) {
            Piece cap = (m.type() == EN_PASSANT) ? make_piece(~b.side_to_move, PAWN) : b.piece_on(m.to());
            scores[i] = SCORE_CAPTURE + mvv_lva(cap, b.piece_on(m.from())) - (see_ok(b, m, 0) ? 0 : 500000);
        } else if (m.type() == PROMOTION) scores[i] = SCORE_PROMO;
        else if (killers_.is_killer(m, ply)) scores[i] = (killers_.killers[ply][0] == m) ? SCORE_KILLER1 : SCORE_KILLER2;
        else if (counters_.get(prev_move) == m) scores[i] = SCORE_COUNTER;
        else scores[i] = SCORE_HISTORY + history_.get(b.side_to_move, m);
    }

    Score best_score  = -SCORE_INFINITE;
    Move  best_move   = MOVE_NONE;
    TTFlag tt_flag    = TT_UPPER;
    int  moves_done   = 0;

    for (int i = 0; i < ml.count; ++i) {
        int best_idx = i;
        for (int j = i+1; j < ml.count; ++j) if (scores[j] > scores[best_idx]) best_idx = j;
        swap(ml.moves[i], ml.moves[best_idx]); swap(scores[i], scores[best_idx]);

        Move m = ml.moves[i];
        bool is_cap = (b.piece_on(m.to()) != NO_PIECE) || (m.type() == EN_PASSANT);
        bool is_promo = (m.type() == PROMOTION);

        if (!pv_node && !in_check && depth <= 4 && !is_cap && !is_promo && moves_done >= (3 + depth * depth)) continue;

        b.make_move(m);
        ++moves_done;

        int extend = 0;
        if (moves_done == 1 && depth >= 7 && std::abs(alpha) < 20000 && tt_move == m) {
            PVLine dummy;
            if (negamax(b, (depth-1)/2, alpha - (2*depth) - 1, alpha - (2*depth), ply+1, false, dummy, m) < alpha - (2*depth)) extend = 1;
        }

        Score score; PVLine child_pv;
        if (moves_done == 1) score = -negamax(b, depth - 1 + extend, -beta, -alpha, ply+1, true, child_pv, m);
        else {
            int red = (depth >= 3 && moves_done >= 4 && !in_check && !is_cap && !is_promo) ? lmr_table[depth][moves_done] + (!pv_node) : 0;
            red = max(0, min(red, depth - 2));
            score = -negamax(b, depth - 1 - red, -alpha-1, -alpha, ply+1, true, child_pv, m);
            if (score > alpha && red > 0) score = -negamax(b, depth - 1, -alpha-1, -alpha, ply+1, true, child_pv, m);
            if (score > alpha && score < beta) score = -negamax(b, depth - 1, -beta, -alpha, ply+1, true, child_pv, m);
        }
        b.unmake_move(m);

        if (time_up()) return 0;
        if (score > best_score) {
            best_score = score; best_move = m;
            if (score > alpha) {
                alpha = score; tt_flag = TT_EXACT; pv.update(m, child_pv);
                if (root) { last_best_ = m; root_pv_ = pv; }
                if (score >= beta) {
                    if (!is_cap && !is_promo) { killers_.store(m, ply); history_.update(b.side_to_move, m, depth*depth); if (!prev_move.is_none()) counters_.store(prev_move, m); }
                    tt_flag = TT_LOWER; break;
                }
            }
        }
    }
    tt_.store(b.hash, best_move, best_score, depth, tt_flag, ply);
    return best_score;
}

// ── Iterative deepening root ───────────────────────────────────
Move SearchEngine::search(Board& board, const Limits& limits) {
    stopped_.store(false); last_stats = SearchStats{}; killers_.clear(); history_.clear(); counters_.clear(); tt_.new_search(); last_best_ = MOVE_NONE; root_pv_.clear();
    start_time_ = std::chrono::steady_clock::now();
    calc_time(limits, board.side_to_move);

    MoveList legal = MoveGen::generate(board);
    if (legal.count == 0) return MOVE_NONE;
    if (legal.count == 1) return legal.moves[0];

    Move best_move = legal.moves[0];
    Score best_score = -SCORE_INFINITE;

    for (int depth = 1; depth <= limits.max_depth; ++depth) {
        current_depth_ = depth;
        Score alpha = -SCORE_INFINITE, beta = SCORE_INFINITE;
        PVLine pv;

        if (depth >= 3) { alpha = std::max((int)-SCORE_INFINITE, best_score - 40); beta = std::min((int)SCORE_INFINITE, best_score + 40); }

        while (true) {
            Score score = negamax(board, depth, alpha, beta, 0, false, pv, MOVE_NONE);
            if (time_up() && depth > 1) break;
            if (score <= alpha) alpha = std::max((int)-SCORE_INFINITE, alpha - 150);
            else if (score >= beta) beta = std::min((int)SCORE_INFINITE, beta + 150);
            else { best_score = score; root_pv_ = pv; break; }
        }

        if (time_up() && depth > 1) break;
        best_move = last_best_;

        // MultiPV Output
        for (int m_idx = 0; m_idx < std::min(multipv_, (int)legal.count); ++m_idx) {
            Move m = legal.moves[m_idx]; PVLine line_pv; Score line_score;
            if (m_idx == 0) { line_score = best_score; line_pv = root_pv_; }
            else { line_score = negamax(board, max(1, depth-1), -SCORE_INFINITE, SCORE_INFINITE, 0, false, line_pv, MOVE_NONE); }
            if (time_up() && depth > 1) break;

            int64_t ms = elapsed_ms();
            uint64_t nps = (ms > 0) ? (last_stats.nodes + last_stats.qnodes) * 1000 / ms : 0;
            cout << "info depth " << depth << " multipv " << (m_idx + 1) << " score " << (is_mate_score(line_score) ? (line_score > 0 ? "mate " : "mate -") : "cp ") << (is_mate_score(line_score) ? abs((SCORE_MATE - abs(line_score) + 1) / 2) : line_score) << " nodes " << (last_stats.nodes + last_stats.qnodes) << " nps " << nps << " pv " << move_to_str(m) << " ";
            for (int i = 0; i < line_pv.length; ++i) cout << move_to_str(line_pv.moves[i]) << " ";
            cout << "\n";
        }
        cout.flush();
    }
    return best_move;
}

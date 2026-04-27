/**
 * search.h
 * Search engine: iterative deepening, negamax alpha-beta,
 * quiescence search, and all related heuristics.
 */

#pragma once
#include "board.h"
#include "movegen.h"
#include "tt.h"
#include "eval.h"
#include <atomic>
#include <chrono>
#include <cstring>

// ── Move ordering scores ───────────────────────────────────────
enum MoveScore : int {
    SCORE_TT_MOVE    = 10'000'000,
    SCORE_CAPTURE    =  1'000'000,   // base; +MVV-LVA on top
    SCORE_PROMO      =    900'000,
    SCORE_KILLER1    =    800'000,
    SCORE_KILLER2    =    790'000,
    SCORE_COUNTER    =    780'000,
    SCORE_HISTORY    =          0,   // add history value
    SCORE_LOSING_CAP =   -100'000
};

// ── PV line ────────────────────────────────────────────────────
struct PVLine {
    Move  moves[MAX_PLY] = {};
    int   length = 0;

    void clear()              { length = 0; }
    void update(Move m, const PVLine& child) {
        moves[0] = m;
        std::memcpy(moves+1, child.moves, child.length * sizeof(Move));
        length = child.length + 1;
    }
};

// ── Search limits ──────────────────────────────────────────────
struct Limits {
    int    max_depth    = MAX_PLY;      // max search depth
    int64_t movetime_ms = 0;            // 0 = no limit
    int64_t wtime_ms    = 0;
    int64_t btime_ms    = 0;
    int     winc_ms     = 0;
    int     binc_ms     = 0;
    int     movestogo   = 0;
    bool    infinite    = false;
};

// ── Search statistics ──────────────────────────────────────────
struct SearchStats {
    uint64_t nodes     = 0;
    uint64_t qnodes    = 0;
    uint64_t tt_hits   = 0;
    uint64_t null_cuts = 0;
    uint64_t lmr_red   = 0;
    uint64_t futility  = 0;
};

// ── Killer moves  (2 per ply) ──────────────────────────────────
struct KillerTable {
    Move killers[MAX_PLY][2] = {};

    void store(Move m, int ply) {
        if (killers[ply][0] != m) {
            killers[ply][1] = killers[ply][0];
            killers[ply][0] = m;
        }
    }
    bool is_killer(Move m, int ply) const {
        return killers[ply][0] == m || killers[ply][1] == m;
    }
    void clear() { std::memset(killers, 0, sizeof(killers)); }
};

// ── History heuristic ──────────────────────────────────────────
struct HistoryTable {
    int table[COLOR_NB][64][64] = {};   // [color][from][to]

    void update(Color c, Move m, int bonus) {
        int& v = table[c][m.from()][m.to()];
        v += bonus - v * std::abs(bonus) / 16384;  // gravity
    }
    int get(Color c, Move m) const {
        return table[c][m.from()][m.to()];
    }
    void clear() { std::memset(table, 0, sizeof(table)); }
};

// ── Counter-move heuristic ─────────────────────────────────────
struct CounterMoveTable {
    Move table[64][64] = {};  // [prev_from][prev_to]

    void store(Move prev, Move response) {
        table[prev.from()][prev.to()] = response;
    }
    Move get(Move prev) const {
        return prev.is_none() ? MOVE_NONE : table[prev.from()][prev.to()];
    }
    void clear() { std::memset(table, 0, sizeof(table)); }
};

// ── Search engine ──────────────────────────────────────────────
class SearchEngine {
public:
    explicit SearchEngine(TranspositionTable& tt, const Evaluator& eval);

    // Run iterative deepening; returns best move
    // Prints UCI info lines during search
    Move search(Board& board, const Limits& limits);

    void stop() { stopped_.store(true); }
    bool is_stopped() const { return stopped_.load(); }

    SearchStats last_stats;

private:
    TranspositionTable& tt_;
    const Evaluator&    eval_;
    std::atomic<bool>   stopped_{false};

    KillerTable       killers_;
    HistoryTable      history_;
    CounterMoveTable  counters_;

    PVLine            root_pv_;
    int               current_depth_ = 0;
    Move              last_best_     = MOVE_NONE;
    int               multipv_       = 1;

    // Time management
    int64_t time_limit_ms_ = 0;
    std::chrono::steady_clock::time_point start_time_;

    int64_t elapsed_ms() const;
    bool    time_up()    const;
    void    calc_time(const Limits& l, Color stm);

    // Core search functions
    Score negamax(Board& b, int depth, Score alpha, Score beta, int ply,
                  bool null_ok, PVLine& pv, Move prev_move);
    Score quiescence(Board& b, Score alpha, Score beta, int ply);

    // Move ordering
    void score_moves(const Board& b, MoveList& ml, Move tt_move,
                     int ply, Move prev_move) const;

    void set_multipv(int n) { multipv_ = n; }

    // Incremental sort: pick highest score move (partial selection sort)
    static Move pick_best(MoveList& ml, int start_idx, const int* scores, int& best_idx);

    // MVV-LVA table
    static int mvv_lva(Piece captured, Piece attacker);

    // SEE (Static Exchange Evaluation) for move ordering captures
    bool see_ok(const Board& b, Move m, int threshold) const;
};

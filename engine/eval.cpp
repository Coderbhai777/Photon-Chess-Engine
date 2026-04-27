/**
 * eval.cpp
 * Classical phase-aware evaluation:
 *   material + PST + mobility + king safety + pawn structure + bonuses.
 * Phase interpolation between MG and EG scores.
 */

#include "eval.h"
#include <algorithm>
#include <cmath>

using std::max; using std::min;

// ---- Piece-Square Tables (from Chessprogramming wiki / PeSTO) ---
// White's perspective; Black mirrors by rank-flip.
// Values are centipawns relative to centre advantage.

namespace PST {

const int mg_pawn_table[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    98,134, 61, 95, 68,126, 34,-11,
    -6,  7, 26, 31, 65, 56, 25,-20,
   -14, 13,  6, 21, 23, 12, 17,-23,
   -27, -2, -5, 12, 17,  6, 10,-25,
   -26, -4,-14,  2,  4, -5,  2,-37,
   -35, -1,-20,-23,-15, 24, 38,-22,
     0,  0,  0,  0,  0,  0,  0,  0
};
const int eg_pawn_table[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
   178,173,158,134,147,132,165,187,
    94,100, 85, 67, 56, 53, 82, 84,
    32, 24, 13,  5, -2,  4, 17, 17,
    13,  9, -3, -7, -7, -8,  3, -1,
     4,  7, -6,  1,  0, -5, -1, -8,
    13,  8,  8, 10, 13,  0,  2, -7,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int mg_knight_table[64] = {
   -167,-89,-34,-49, 61,-97,-15,-107,
    -73,-41, 72, 36, 23, 62,  7, -17,
    -47, 60, 37, 65, 84,129, 73,  44,
     -9, 17, 19, 53, 37, 69, 18,  22,
    -13,  4, 16, 13, 28, 19, 21,  -8,
    -23, -9, 12, 10, 19, 17, 25, -16,
    -29,-53,-12, -3, -1, 18,-14, -19,
   -105,-21,-58,-33,-17,-28,-19, -23
};
const int eg_knight_table[64] = {
    -58,-38,-13,-28,-31,-27,-63,-99,
    -25, -8,-25, -2, -9,-25,-24,-52,
    -24,-20, 10,  9, -1,  -9,-19,-41,
    -17,  3, 22, 22, 22,  11,  8,-18,
    -18, -6, 16, 25, 16,  17,  4,-18,
    -23, -3, -1, 15, 10,  -3,-20,-22,
    -42,-20,-10, -5, -2,-20,-23,-44,
    -29,-51,-23,-15,-22,-18,-50,-64
};

const int mg_bishop_table[64] = {
    -29,  4,-82,-37,-25,-42,  7, -8,
    -26, 16,-18,-13, 30, 59, 18,-47,
    -16, 37, 43, 40, 35, 50, 37, -2,
     -4,  5, 19, 50, 37, 37,  7, -2,
     -6, 13, 13, 26, 34, 12, 10,  4,
      0, 15, 15, 15, 14,  27,  0,  1,
      4, 15, 16,  0,  7, 21, 33,  1,
    -33, -3,-14,-21,-13,-12,-39,-21
};
const int eg_bishop_table[64] = {
    -14,-21,-11, -8, -7, -9,-17,-24,
     -8, -4,  7,-12, -3,-13, -4,-14,
      2, -8,  0, -1, -2,  6,  0,  4,
     -3,  9, 12,  9, 14, 10,  3,  2,
     -6,  3, 13, 19,  7, 10, -3, -9,
    -12, -3,  8, 10, 13,  3, -7,-15,
    -14,-18, -7, -1,  4, -9,-15,-27,
    -23, -9,-23, -5, -9,-16, -5,-17
};

const int mg_rook_table[64] = {
     32, 42, 32, 51, 63,  9, 31, 43,
     27, 32, 58, 62, 80, 67, 26, 44,
     -5, 19, 26, 36, 17, 45, 61, 16,
    -24,-11,  7, 26, 24, 35, -8,-20,
    -36,-26,-12, -1,  9, -7,  6,-23,
    -45,-25,-16,-17,  3,  0, -5,-33,
    -44,-16,-20, -9, -1, 11, -6,-71,
    -19,-13,  1, 17, 16,  7,-37,-26
};
const int eg_rook_table[64] = {
     13, 10, 18, 15, 12, 12,  8,  5,
     11, 13, 13, 11, -3,  3,  8,  3,
      7,  7,  7,  5,  4, -3, -5, -3,
      4,  3, 13,  1,  2,  1, -1,  2,
      3,  5,  8,  4, -5, -6, -8, -11,
     -4,  0, -5, -1, -7,-12, -8,-16,
     -6, -6,  0,  2, -9, -9,-11, -3,
     -9,  2,  3, -1, -5,-13,  4,-20
};

const int mg_queen_table[64] = {
     -9, 22, 22, 27, 27, 19, 10, 20,
    -17, 20, 32, 41, 58, 25, 30,  0,
    -20,  6, 9, 49, 47, 35,  19,  9,
      3, 22, 24, 45, 57, 40, 57, 36,
    -18, 28, 19, 47, 31, 34, 39, 23,
    -16,-27, 15,  6,  9, 17, 10,  5,
    -22,-23,-30,-16,-16,-23,-36,-32,
    -33,-28,-22,-43, -5,-32,-20,-41
};
const int eg_queen_table[64] = {
    -74,-35,-18,-18,-11,  15,  4,-17,
    -12, 17, 14, 17, 17,  17, 38,  23,
     10, 17, 23, 15, 20,  45, 44,  13,
     -8, 22, 24, 27, 26,  33, 26,   3,
    -18, -4, 21, 24, 27,  23,  9,  -11,
    -19, -3, 11, 21, 23,  16, 17,   5,
    -16,-12, -1, -9,  4,  -2, -4,-19,
    -22,-24,-17,-26,  8,-12, -9,-14
};

const int mg_king_table[64] = {
    -65, 23, 16,-15,-56,-34,  2, 13,
     29, -1,-20, -7, -8, -4,-38,-29,
     -9, 24,  2,-16,-20,  6, 22,-22,
    -17,-20,-12,-27,-30,-25,-14,-36,
    -49, -1,-27,-39,-46,-44,-33,-51,
    -14,-14,-22,-46,-44,-30,-15,-27,
      1,  7, -8,-64,-43,-16,  9,  8,
    -15, 36, 12,-54,  8,-28, 24, 14
};
const int eg_king_table[64] = {
    -74,-35,-18,-18,-11, 15,  4,-17,
    -12, 17, 14, 17, 17, 17, 38,  23,
     10, 17, 23, 15, 20, 45, 44,  13,
     -8, 22, 24, 27, 26, 33, 26,   3,
    -18, -4, 21, 24, 27, 23,  9, -11,
    -19, -3, 11, 21, 23, 16, 17,   5,
    -16,-12, -1, -9,  4, -2, -4, -19,
    -22,-24,-17,-26,  8,-12, -9, -14
};

const int* mg_table(PieceType pt) {
    switch(pt) {
        case PAWN:   return mg_pawn_table;
        case KNIGHT: return mg_knight_table;
        case BISHOP: return mg_bishop_table;
        case ROOK:   return mg_rook_table;
        case QUEEN:  return mg_queen_table;
        case KING:   return mg_king_table;
        default:     return nullptr;
    }
}
const int* eg_table(PieceType pt) {
    switch(pt) {
        case PAWN:   return eg_pawn_table;
        case KNIGHT: return eg_knight_table;
        case BISHOP: return eg_bishop_table;
        case ROOK:   return eg_rook_table;
        case QUEEN:  return eg_queen_table;
        case KING:   return eg_king_table;
        default:     return nullptr;
    }
}

} // namespace PST

// ── Phase constants ────────────────────────────────────────────
// Total phase = 24 (4 knights=2, 4 bishops=2, 4 rooks=4, 2 queens=8 → 4+4+16+16... simplified)
static const int PHASE_WEIGHTS[PIECE_TYPE_NB] = {0,0,1,1,2,4,0};
static const int TOTAL_PHASE = 24;

// ── PST lookup (handles colour flip) ──────────────────────────
static int pst_mg(Color c, PieceType pt, Square s) {
    const int* tbl = PST::mg_table(pt);
    if (!tbl) return 0;
    // Black mirrors vertically
    int idx = (c == WHITE) ? s : (s ^ 56);
    return tbl[idx];
}
static int pst_eg(Color c, PieceType pt, Square s) {
    const int* tbl = PST::eg_table(pt);
    if (!tbl) return 0;
    int idx = (c == WHITE) ? s : (s ^ 56);
    return tbl[idx];
}

// ── Evaluate one side ──────────────────────────────────────────
Score Evaluator::eval_side(const Board& b, Color us, int phase, int total_phase) const {
    Color them = ~us;
    int mg = 0, eg = 0;
    Bitboard occ = b.all_pieces();

    // -- Material + PST
    for (int pt_int = PAWN; pt_int <= QUEEN; ++pt_int) {
        PieceType pt = PieceType(pt_int);
        Bitboard bb = b.pieces(us, pt);
        while (bb) {
            Square s = pop_lsb(bb);
            mg += weights.material_mg[pt] + pst_mg(us, pt, s);
            eg += weights.material_eg[pt] + pst_eg(us, pt, s);
        }
    }

    // King PST only
    {
        Square ksq = b.king_sq(us);
        mg += pst_mg(us, KING, ksq);
        eg += pst_eg(us, KING, ksq);
    }

    // -- Bishop pair bonus
    if (popcount(b.pieces(us, BISHOP)) >= 2) {
        mg += weights.bishop_pair_mg;
        eg += weights.bishop_pair_eg;
    }

    // -- Pawns: isolated, doubled, passed
    Bitboard my_pawns   = b.pieces(us, PAWN);
    Bitboard their_pawns= b.pieces(them, PAWN);

    Bitboard pp = my_pawns;
    while (pp) {
        Square s = pop_lsb(pp);
        int f = file_of(s);
        int r = rank_of(s);      // 0-7 in our perspective (White: rank0=rank1)
        int rel_rank = (us == WHITE) ? r : (7-r);

        // Doubled
        if (my_pawns & file_bb(f) & ~sq_bb(s)) {
            mg += weights.doubled_pawn_mg;
            eg += weights.doubled_pawn_eg;
        }

        // Isolated
        Bitboard adj_files = 0;
        if (f > 0) adj_files |= file_bb(f-1);
        if (f < 7) adj_files |= file_bb(f+1);
        if (!(my_pawns & adj_files)) {
            mg += weights.isolated_pawn_mg;
            eg += weights.isolated_pawn_eg;
        }

        // Passed pawn: no enemy pawns on same or adjacent files ahead
        Bitboard ahead_mask = 0;
        if (us == WHITE) {
            for (int rr = r+1; rr <= 7; ++rr)
                ahead_mask |= rank_bb(rr) & (file_bb(f) | adj_files);
        } else {
            for (int rr = r-1; rr >= 0; --rr)
                ahead_mask |= rank_bb(rr) & (file_bb(f) | adj_files);
        }
        if (!(their_pawns & ahead_mask) && rel_rank >= 1) {
            mg += weights.passed_pawn_mg[rel_rank];
            eg += weights.passed_pawn_eg[rel_rank];
        }
    }

    // -- Rooks on open / half-open files
    Bitboard rooks = b.pieces(us, ROOK);
    while (rooks) {
        Square s = pop_lsb(rooks);
        int f = file_of(s);
        if (!(my_pawns & file_bb(f))) {
            if (!(their_pawns & file_bb(f))) {
                mg += weights.rook_open_file_mg;
                eg += weights.rook_open_file_eg;
            } else {
                mg += weights.rook_half_open_mg;
                eg += weights.rook_half_open_eg;
            }
        }
    }

    // -- Mobility (count attacks on non-own squares)
    auto mobility = [&](PieceType pt) -> int {
        Bitboard bb2 = b.pieces(us, pt);
        int count = 0;
        while (bb2) {
            Square s = pop_lsb(bb2);
            Bitboard atk = 0;
            switch(pt) {
                case KNIGHT: atk = Attacks::knight(s); break;
                case BISHOP: atk = Attacks::bishop(s, occ); break;
                case ROOK:   atk = Attacks::rook(s, occ);   break;
                case QUEEN:  atk = Attacks::queen(s, occ);  break;
                default: break;
            }
            count += popcount(atk & ~b.pieces(us));
        }
        return count;
    };

    for (int pt_int = KNIGHT; pt_int <= QUEEN; ++pt_int) {
        PieceType pt = PieceType(pt_int);
        int mob = mobility(pt);
        mg += mob * weights.mobility_mg[pt];
        eg += mob * weights.mobility_eg[pt];
    }

    // -- King safety (simplified: count attackers near opponent king zone)
    Square their_king = b.king_sq(them);
    Bitboard king_zone = Attacks::king(their_king) | sq_bb(their_king);
    int attack_count = 0;
    int attack_weight = 0;

    for (int pt_int = KNIGHT; pt_int <= QUEEN; ++pt_int) {
        PieceType pt = PieceType(pt_int);
        Bitboard att_pieces = b.pieces(us, pt);
        while (att_pieces) {
            Square s = pop_lsb(att_pieces);
            Bitboard atk = 0;
            switch(pt) {
                case KNIGHT: atk = Attacks::knight(s); break;
                case BISHOP: atk = Attacks::bishop(s, occ); break;
                case ROOK:   atk = Attacks::rook(s, occ);  break;
                case QUEEN:  atk = Attacks::queen(s, occ); break;
                default: break;
            }
            if (atk & king_zone) {
                ++attack_count;
                attack_weight += weights.king_attack_weight[pt];
            }
        }
    }
    // Penalty only meaningful in MG
    static const int safety_table[100] = {
        0,  0,  1,  2,  3,  5,  7, 9, 12, 15, 18, 22, 26, 30, 35,
       39, 44, 50, 56, 62, 68, 75, 82, 85, 89, 97,105,113,122,131,
      140,150,169,180
    };
    int safety_idx = min(attack_weight, 99);
    mg -= safety_table[safety_idx];

    // -- Passed pawns (Stockfish-inspired endgame strength)
    Bitboard p = b.pieces(us, PAWN);
    Bitboard op = b.pieces(them, PAWN);
    while (p) {
        Square s = pop_lsb(p);
        int r = rank_of(s);
        int f = file_of(s);
        Bitboard ahead_mask = 0;
        // Check files f-1, f, f+1 for opponent pawns ahead
        for (int rank = (us==WHITE ? r+1 : 0); rank <= (us==WHITE ? 7 : r-1); ++rank) {
            Bitboard r_mask = rank_bb(rank);
            ahead_mask |= (file_bb(f) | (f>0?file_bb(f-1):0) | (f<7?file_bb(f+1):0)) & r_mask;
        }
        if (!(ahead_mask & op)) {
            int dist = (us == WHITE) ? r : 7 - r;
            eg += dist * dist * 8; // Quad bonus for advanced passers
            mg += dist * 5;
        }
    }

    // -- Taper
    int score = (mg * phase + eg * (TOTAL_PHASE - phase)) / TOTAL_PHASE;
    return score;
}

// ── Public evaluate ────────────────────────────────────────────
Score Evaluator::evaluate(const Board& b) const {
    // Compute game phase
    int phase = 0;
    for (int pt_int = KNIGHT; pt_int <= QUEEN; ++pt_int) {
        PieceType pt = PieceType(pt_int);
        phase += PHASE_WEIGHTS[pt] * popcount(b.pieces_by_type[pt]);
    }
    phase = min(phase, TOTAL_PHASE);

    Score white = eval_side(b, WHITE, phase, TOTAL_PHASE);
    Score black = eval_side(b, BLACK, phase, TOTAL_PHASE);
    Score score = white - black;

    // Contempt / tempo bonus
    score += (b.side_to_move == WHITE) ? 10 : -10;

    // Return from side-to-move perspective
    return (b.side_to_move == WHITE) ? score : -score;
}

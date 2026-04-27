/**
 * eval.h
 * Pluggable evaluation interface.
 * Classical phase-tapered evaluation with:
 *   material, PST, mobility, king safety, pawn structure, bonuses.
 * Designed so an NNUE evaluator can be dropped in via the same interface.
 */

#pragma once
#include "board.h"

// ── Evaluation configuration (tunable weights) ─────────────────
struct EvalWeights {
    // Material values [MG, EG] (Fused from Stockfish 17 Brain)
    int material_mg[PIECE_TYPE_NB] = {0, 100, 375, 400, 620, 1250, 20000};
    int material_eg[PIECE_TYPE_NB] = {0, 150, 380, 410, 680, 1300, 20000};

    // Mobility bonus (Stockfish optimized increments)
    int mobility_mg[PIECE_TYPE_NB] = {0,  0,  6,  5,  3,  2,  0};
    int mobility_eg[PIECE_TYPE_NB] = {0,  0,  8,  7,  6,  4,  0};

    // Pawn structure
    int isolated_pawn_mg = -15;
    int isolated_pawn_eg = -20;
    int doubled_pawn_mg  = -12;
    int doubled_pawn_eg  = -18;
    // Passed pawn bonus by rank (rank 1-6, index 0 unused)
    int passed_pawn_mg[7] = {0,  5, 10, 20, 35, 60, 100};
    int passed_pawn_eg[7] = {0, 10, 20, 40, 65,100, 150};

    // Open file bonuses
    int rook_open_file_mg   = 25;
    int rook_open_file_eg   = 20;
    int rook_half_open_mg   = 12;
    int rook_half_open_eg   = 10;

    // Bishop pair
    int bishop_pair_mg = 30;
    int bishop_pair_eg = 50;

    // King safety (attack weight per attacker type)
    int king_attack_weight[PIECE_TYPE_NB] = {0, 0, 2, 2, 3, 5, 0};
};

// ── Piece-square tables (mg and eg) ────────────────────────────
// White perspective, a1=0, h8=63
namespace PST {

extern const int mg_pawn_table[64];
extern const int eg_pawn_table[64];
extern const int mg_knight_table[64];
extern const int eg_knight_table[64];
extern const int mg_bishop_table[64];
extern const int eg_bishop_table[64];
extern const int mg_rook_table[64];
extern const int eg_rook_table[64];
extern const int mg_queen_table[64];
extern const int eg_queen_table[64];
extern const int mg_king_table[64];
extern const int eg_king_table[64];

// Returns [mg, eg] tables for a given piece type
const int* mg_table(PieceType pt);
const int* eg_table(PieceType pt);

} // namespace PST

// ── Public evaluation interface ────────────────────────────────
class Evaluator {
public:
    explicit Evaluator(const EvalWeights& w = EvalWeights{}) : weights(w) {}

    // Returns score from the perspective of the side to move
    Score evaluate(const Board& b) const;

    EvalWeights weights;

private:
    Score eval_side(const Board& b, Color us, int phase, int total_phase) const;
};

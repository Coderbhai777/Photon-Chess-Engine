/**
 * movegen.h
 * Legal move generation using bitboards.
 * Produces a MoveList (plain array + count) for performance.
 */

#pragma once
#include "board.h"

// ── Move list ──────────────────────────────────────────────────
struct MoveList {
    Move  moves[MAX_MOVES];
    int   count = 0;

    void push(Move m) { moves[count++] = m; }
    Move* begin() { return moves; }
    Move* end()   { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end()   const { return moves + count; }
};

// ── Generator ──────────────────────────────────────────────────
namespace MoveGen {

// Generate all LEGAL moves for the side to move
MoveList generate(const Board& b);

// Generate only captures + promotions (for quiescence search)
MoveList generate_tactical(const Board& b);

} // namespace MoveGen

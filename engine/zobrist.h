/**
 * zobrist.h
 * Zobrist hashing for incremental position hash updates.
 * Keys are seeded deterministically for reproducibility.
 */

#pragma once
#include "types.h"

namespace Zobrist {

// Random keys: [piece][square], side-to-move, castling rights x16, en-passant file x8
extern Key piece_keys[PIECE_NB][64];
extern Key side_key;
extern Key castling_keys[16];
extern Key ep_keys[8];

void init();

} // namespace Zobrist

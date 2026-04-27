/**
 * attacks.h
 * Precomputed attack tables: knight, king, pawn, and magic bitboard sliding attacks.
 * Initialize once at startup via Attacks::init().
 */

#pragma once
#include "types.h"

namespace Attacks {

// ── Raw lookup tables ──────────────────────────────────────────
extern Bitboard knight_attacks[64];
extern Bitboard king_attacks[64];
extern Bitboard pawn_attacks[2][64];   // [color][square]

// Magic numbers and masks
struct Magic {
    Bitboard mask;    // relevant occupancy squares
    Bitboard magic;   // magic multiplier
    int      shift;   // 64 - popcount(mask)
    Bitboard* table;  // pointer into attack table
};

extern Magic bishop_magic[64];
extern Magic rook_magic[64];

// ── Initialisation ─────────────────────────────────────────────
void init();

// ── Accessors ──────────────────────────────────────────────────
inline Bitboard knight(Square s)               { return knight_attacks[s]; }
inline Bitboard king(Square s)                 { return king_attacks[s];   }
inline Bitboard pawn(Color c, Square s)        { return pawn_attacks[c][s];}

inline Bitboard bishop(Square s, Bitboard occ) {
    const Magic& m = bishop_magic[s];
    return m.table[((occ & m.mask) * m.magic) >> m.shift];
}

inline Bitboard rook(Square s, Bitboard occ) {
    const Magic& m = rook_magic[s];
    return m.table[((occ & m.mask) * m.magic) >> m.shift];
}

inline Bitboard queen(Square s, Bitboard occ) {
    return bishop(s, occ) | rook(s, occ);
}

// Between two squares (exclusive endpoints)
extern Bitboard between_bb[64][64];
// Line through two squares (entire diagonal/rank/file, inclusive of endpoints)
extern Bitboard line_bb[64][64];

} // namespace Attacks

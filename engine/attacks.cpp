/**
 * attacks.cpp
 * Initialises all precomputed attack tables used by the move generator.
 * Uses "fancy" magic bitboards for sliding pieces.
 * Generates magics at startup with validation — guarantees correctness.
 */

#include "attacks.h"
#include <cstring>
#include <random>
#include <iostream>

namespace Attacks {

// Storage
Bitboard knight_attacks[64];
Bitboard king_attacks[64];
Bitboard pawn_attacks[2][64];
Bitboard between_bb[64][64];
Bitboard line_bb[64][64];

Magic bishop_magic[64];
Magic rook_magic[64];

// Individual lookup tables per square (fixed size for simplicity)
static Bitboard bishop_table_storage[64][512];
static Bitboard rook_table_storage[64][4096];

// ── Helper: generate occupancy subset by index ─────────────────
static Bitboard occupancy_subset(int index, Bitboard mask) {
    Bitboard occ = 0;
    Bitboard tmp = mask;
    int bits = popcount(mask);
    for (int i = 0; i < bits; ++i) {
        Square s = pop_lsb(tmp);
        if (index & (1 << i)) occ |= sq_bb(s);
    }
    return occ;
}

// ── Slow reference sliding attacks (used only during init) ──────
static Bitboard slow_bishop(Square s, Bitboard occ) {
    Bitboard atk = 0;
    int r = rank_of(s), f = file_of(s);
    for (int dr : {1,-1}) for (int df : {1,-1}) {
        int nr=r+dr, nf=f+df;
        while (nr>=0 && nr<8 && nf>=0 && nf<8) {
            Square t = make_square(nf,nr);
            atk |= sq_bb(t);
            if (occ & sq_bb(t)) break;
            nr+=dr; nf+=df;
        }
    }
    return atk;
}

static Bitboard slow_rook(Square s, Bitboard occ) {
    Bitboard atk = 0;
    int r = rank_of(s), f = file_of(s);
    auto ray = [&](int dr, int df) {
        int nr=r+dr, nf=f+df;
        while (nr>=0 && nr<8 && nf>=0 && nf<8) {
            Square t = make_square(nf,nr);
            atk |= sq_bb(t);
            if (occ & sq_bb(t)) break;
            nr+=dr; nf+=df;
        }
    };
    ray(1,0); ray(-1,0); ray(0,1); ray(0,-1);
    return atk;
}

// ── Mask generators (relevant occupancy bits) ───────────────────
static Bitboard bishop_mask(Square s) {
    Bitboard edges = ((RANK1_BB | RANK8_BB) & ~rank_bb(rank_of(s)))
                   | ((FILEA_BB | FILEH_BB) & ~file_bb(file_of(s)));
    return slow_bishop(s, 0) & ~edges;
}

static Bitboard rook_mask(Square s) {
    Bitboard edges = ((rank_bb(0) | rank_bb(7)) & ~rank_bb(rank_of(s)))
                   | ((file_bb(0) | file_bb(7)) & ~file_bb(file_of(s)));
    return slow_rook(s, 0) & ~edges;
}

// ── Random number generator for magic search ────────────────────
static uint64_t rng_state = 0xDEADBEEFCAFEBABEULL;

static uint64_t rng64() {
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 0x2545F4914F6CDD1DULL;
}

// Sparse random number (few set bits — better for magic search)
static uint64_t sparse_rand() {
    return rng64() & rng64() & rng64();
}

// ── Try a magic number for a square ─────────────────────────────
// Returns true if the magic works (no destructive collisions).
static bool try_magic(Square s, Bitboard mask, int table_size,
                       Bitboard* table, Bitboard magic,
                       Bitboard(*atk_fn)(Square, Bitboard))
{
    int n = 1 << popcount(mask);
    int shift = 64 - popcount(mask);

    // Clear table
    for (int i = 0; i < table_size; ++i) table[i] = 0;

    for (int i = 0; i < n; ++i) {
        Bitboard occ = occupancy_subset(i, mask);
        int idx = (int)((occ * magic) >> shift);
        Bitboard attacks = atk_fn(s, occ);

        if (table[idx] == 0) {
            table[idx] = attacks;
        } else if (table[idx] != attacks) {
            return false;  // Destructive collision!
        }
    }
    return true;
}

// ── Find a valid magic for a given square ────────────────────────
static Bitboard find_magic(Square s, Bitboard mask, int table_size,
                            Bitboard* table,
                            Bitboard(*atk_fn)(Square, Bitboard))
{
    for (int attempt = 0; attempt < 100000000; ++attempt) {
        Bitboard magic = sparse_rand();
        // Quick reject: magic * mask must have enough bits in top
        if (popcount((mask * magic) & 0xFF00000000000000ULL) < 6) continue;

        if (try_magic(s, mask, table_size, table, magic, atk_fn))
            return magic;
    }
    // Should never happen with enough attempts
    std::cerr << "FATAL: Failed to find magic for square " << s << "\n";
    return 0;
}

// ── Init magic for one square ────────────────────────────────────
static void init_one_magic(Square s, Magic& m, Bitboard* tbl, int max_table_size,
                            Bitboard(*mask_fn)(Square),
                            Bitboard(*atk_fn)(Square, Bitboard))
{
    m.mask  = mask_fn(s);
    m.shift = 64 - popcount(m.mask);
    m.table = tbl;
    m.magic = find_magic(s, m.mask, max_table_size, tbl, atk_fn);
}

// ── Between / line tables ────────────────────────────────────────
static void init_between_and_line() {
    for (Square s1 = A1; s1 <= H8; ++s1) {
        for (Square s2 = A1; s2 <= H8; ++s2) {
            between_bb[s1][s2] = 0;
            line_bb[s1][s2]    = 0;

            if (s1 == s2) continue;

            // Rook lines
            if (slow_rook(s1, 0) & sq_bb(s2)) {
                between_bb[s1][s2] = slow_rook(s1, sq_bb(s2)) & slow_rook(s2, sq_bb(s1));
                line_bb[s1][s2]    = (slow_rook(s1, 0) & slow_rook(s2, 0)) | sq_bb(s1) | sq_bb(s2);
            }
            // Bishop lines
            if (slow_bishop(s1, 0) & sq_bb(s2)) {
                between_bb[s1][s2] = slow_bishop(s1, sq_bb(s2)) & slow_bishop(s2, sq_bb(s1));
                line_bb[s1][s2]    = (slow_bishop(s1, 0) & slow_bishop(s2, 0)) | sq_bb(s1) | sq_bb(s2);
            }
        }
    }
}

// ── Public init ──────────────────────────────────────────────────
void init() {
    // Knight attacks
    for (Square s = A1; s <= H8; ++s) {
        Bitboard b = sq_bb(s);
        knight_attacks[s] =
            (b << 17 & ~FILEA_BB) | (b << 15 & ~FILEH_BB) |
            (b << 10 & ~(FILEA_BB|(FILEA_BB<<1))) |
            (b <<  6 & ~(FILEH_BB|(FILEH_BB>>1))) |
            (b >> 17 & ~FILEH_BB) | (b >> 15 & ~FILEA_BB) |
            (b >> 10 & ~(FILEH_BB|(FILEH_BB>>1))) |
            (b >>  6 & ~(FILEA_BB|(FILEA_BB<<1)));
    }

    // King attacks
    for (Square s = A1; s <= H8; ++s) {
        Bitboard b = sq_bb(s);
        king_attacks[s] =
            shift(b, NORTH) | shift(b, SOUTH) |
            shift(b, EAST)  | shift(b, WEST)  |
            shift(b, NORTH_EAST) | shift(b, NORTH_WEST) |
            shift(b, SOUTH_EAST) | shift(b, SOUTH_WEST);
    }

    // Pawn attacks
    for (Square s = A1; s <= H8; ++s) {
        Bitboard b = sq_bb(s);
        pawn_attacks[WHITE][s] = shift(b, NORTH_EAST) | shift(b, NORTH_WEST);
        pawn_attacks[BLACK][s] = shift(b, SOUTH_EAST) | shift(b, SOUTH_WEST);
    }

    // Magic sliding attacks — find magics from scratch with validation
    for (Square s = A1; s <= H8; ++s) {
        init_one_magic(s, bishop_magic[s], bishop_table_storage[s], 512,
                       bishop_mask, slow_bishop);
    }
    for (Square s = A1; s <= H8; ++s) {
        init_one_magic(s, rook_magic[s], rook_table_storage[s], 4096,
                       rook_mask, slow_rook);
    }

    init_between_and_line();
}

} // namespace Attacks

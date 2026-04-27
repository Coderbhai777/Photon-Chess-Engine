/**
 * zobrist.cpp
 * Deterministic PRNG seeded with a fixed value for reproducible hashes.
 */

#include "zobrist.h"
#include <cstdlib>

namespace Zobrist {

Key piece_keys[PIECE_NB][64];
Key side_key;
Key castling_keys[16];
Key ep_keys[8];

// Simple splitmix64 PRNG (fast, high-quality)
static uint64_t state = 0x123456789ABCDEF0ULL;
static uint64_t next_rand() {
    state += 0x9e3779b97f4a7c15ULL;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void init() {
    for (int p = 0; p < PIECE_NB; ++p)
        for (int s = 0; s < 64; ++s)
            piece_keys[p][s] = next_rand();

    side_key = next_rand();

    for (int i = 0; i < 16; ++i)
        castling_keys[i] = next_rand();

    for (int i = 0; i < 8; ++i)
        ep_keys[i] = next_rand();
}

} // namespace Zobrist

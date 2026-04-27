/**
 * tt.cpp
 * Transposition Table implementation.
 */

#include "tt.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <cassert>

// ── Construction ───────────────────────────────────────────────
TranspositionTable::TranspositionTable(size_t mb) {
    resize(mb);
}

TranspositionTable::~TranspositionTable() {
    delete[] table_;
}

void TranspositionTable::resize(size_t mb) {
    delete[] table_;
    // Round down to power of 2
    size_t bytes   = mb * 1024 * 1024;
    size_t entries = bytes / sizeof(TTEntry);
    // Round down to power of 2
    size_ = 1;
    while (size_ * 2 <= entries) size_ <<= 1;

    table_ = new TTEntry[size_];
    clear();
}

void TranspositionTable::clear() {
    for (size_t i = 0; i < size_; ++i) table_[i].clear();
    age_ = 0;
}

void TranspositionTable::new_search() {
    ++age_;
}

// ── Mate score normalisation ───────────────────────────────────
// Store mate scores relative to the root; restore relative to current node.
int TranspositionTable::score_to_tt(int s, int ply) {
    if (s >= SCORE_MATE - MAX_PLY) return s + ply;
    if (s <= -SCORE_MATE + MAX_PLY) return s - ply;
    return s;
}

int TranspositionTable::score_from_tt(int s, int ply) {
    if (s >= SCORE_MATE - MAX_PLY) return s - ply;
    if (s <= -SCORE_MATE + MAX_PLY) return s + ply;
    return s;
}

// ── Store ──────────────────────────────────────────────────────
void TranspositionTable::store(Key key, Move best, int score, int depth, TTFlag flag, int ply) {
    TTEntry& e = table_[index(key)];

    // Replacement policy: always replace if same key, or prefer higher depth / newer age
    bool replace = (e.key == key)
                 || (e.flag == TT_NONE)
                 || (e.age  != age_)
                 || (depth  >= e.depth);

    if (!replace) return;

    e.key       = key;
    e.best_move = best;
    e.score     = (int16_t)score_to_tt(score, ply);
    e.depth     = (int8_t)depth;
    e.flag      = flag;
    e.age       = age_;
}

// ── Probe ─────────────────────────────────────────────────────
bool TranspositionTable::probe(Key key, Move& best, int& score, int depth,
                                int alpha, int beta, int ply) const {
    const TTEntry& e = table_[index(key)];
    if (e.key != key || e.flag == TT_NONE) return false;

    best = e.best_move;

    if (e.depth < depth) return false;  // insufficient depth

    int s = score_from_tt(e.score, ply);
    score = s;

    if (e.flag == TT_EXACT)               return true;
    if (e.flag == TT_LOWER && s >= beta)  return true;
    if (e.flag == TT_UPPER && s <= alpha) return true;
    return false;
}

Move TranspositionTable::probe_move(Key key) const {
    const TTEntry& e = table_[index(key)];
    return (e.key == key) ? e.best_move : MOVE_NONE;
}

int TranspositionTable::hashfull() const {
    // Sample first 1000 entries
    int used = 0;
    size_t sample = std::min(size_, (size_t)1000);
    for (size_t i = 0; i < sample; ++i)
        if (table_[i].flag != TT_NONE && table_[i].age == age_)
            ++used;
    return (int)(used * 1000 / sample);
}

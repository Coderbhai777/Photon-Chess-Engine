/**
 * tt.h
 * Transposition Table with Zobrist keys, clustered buckets, and aging.
 * Stores: flag (exact/lower/upper), depth, score, best move.
 */

#pragma once
#include "types.h"

// ── TT entry flag ──────────────────────────────────────────────
enum TTFlag : uint8_t {
    TT_NONE  = 0,
    TT_EXACT = 1,   // PV node
    TT_LOWER = 2,   // Cut node (score >= beta) — lower bound
    TT_UPPER = 3    // All node (score <= alpha) — upper bound
};

// ── Single TT entry  (16 bytes) ───────────────────────────────
struct TTEntry {
    Key      key;        // 8 bytes — full Zobrist key for verification
    Move     best_move;  // 2 bytes — best / refutation move
    int16_t  score;      // 2 bytes — stored score
    int8_t   depth;      // 1 byte  — depth this was searched at
    TTFlag   flag;       // 1 byte  — bound type
    uint8_t  age;        // 1 byte  — generation (for replacement)
    uint8_t  pad;        // 1 byte  — padding

    void clear() { key=0; best_move=MOVE_NONE; score=0; depth=-1; flag=TT_NONE; age=0; pad=0; }
};

// ── Transposition Table ────────────────────────────────────────
class TranspositionTable {
public:
    explicit TranspositionTable(size_t mb = 64);
    ~TranspositionTable();

    void  resize(size_t mb);
    void  clear();
    void  new_search();  // increment generation/age

    // Store an entry
    void store(Key key, Move best, int score, int depth, TTFlag flag, int ply);

    // Probe — returns false if nothing useful found
    bool probe(Key key, Move& best, int& score, int depth, int alpha, int beta, int ply) const;

    // Partial probe: only retrieves best move (for move ordering)
    Move probe_move(Key key) const;

    // How full (per mille) — for UCI hash info
    int hashfull() const;

private:
    TTEntry* table_ = nullptr;
    size_t   size_  = 0;     // number of entries (power of 2)
    uint8_t  age_   = 0;

    size_t index(Key k) const { return k & (size_ - 1); }

    // Adjust mate scores for ply
    static int score_to_tt(int s, int ply);
    static int score_from_tt(int s, int ply);
};

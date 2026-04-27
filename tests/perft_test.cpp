/**
 * perft_test.cpp
 * Perft correctness validation against known node counts.
 * Run: perft_test [depth]
 */

#include "../engine/board.h"
#include "../engine/movegen.h"
#include "../engine/attacks.h"
#include "../engine/zobrist.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstring>

using namespace std;

// ── Perft function ──────────────────────────────────────────────
static uint64_t perft(Board& b, int depth) {
    if (depth == 0) return 1;
    MoveList ml = MoveGen::generate(b);
    if (depth == 1) return ml.count;

    uint64_t nodes = 0;
    for (int i = 0; i < ml.count; ++i) {
        b.make_move(ml.moves[i]);
        nodes += perft(b, depth - 1);
        b.unmake_move(ml.moves[i]);
    }
    return nodes;
}

// ── Test positions with known results ──────────────────────────
struct PerftTest {
    const char* name;
    const char* fen;
    uint64_t expected[7]; // depth 1..6
};

static const PerftTest PERFT_TESTS[] = {
    {
        "Starting position",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        {20, 400, 8902, 197281, 4865609, 119060324}
    },
    {
        "Kiwipete",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        {48, 2039, 97862, 4085603, 193690690, 0}
    },
    {
        "Position 3",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        {14, 191, 2812, 43238, 674624, 11030083}
    },
    {
        "Position 4",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        {6, 264, 9467, 422333, 15833292, 0}
    },
    {
        "Position 5",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        {44, 1486, 62379, 2103487, 89941194, 0}
    },
    {
        "Position 6",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        {46, 2079, 89890, 3894594, 164075551, 0}
    }
};

// ── Run all tests up to given depth ────────────────────────────
static bool run_tests(int max_depth) {
    bool all_pass = true;

    for (const auto& test : PERFT_TESTS) {
        cout << "\n[ " << test.name << " ]\n";
        cout << "FEN: " << test.fen << "\n";
        cout << string(60, '-') << "\n";

        Board b;
        b.set_fen(test.fen);

        for (int d = 1; d <= min(max_depth, 6); ++d) {
            if (test.expected[d-1] == 0) break;

            auto t0 = chrono::steady_clock::now();
            uint64_t got = perft(b, d);
            auto t1 = chrono::steady_clock::now();
            int64_t ms = chrono::duration_cast<chrono::milliseconds>(t1-t0).count();
            uint64_t nps = (ms > 0) ? got * 1000 / ms : got;

            bool pass = (got == test.expected[d-1]);
            all_pass &= pass;

            cout << "  Depth " << d << ": "
                 << setw(12) << got
                 << "  expected: " << setw(12) << test.expected[d-1]
                 << "  " << (pass ? " OK " : "FAIL")
                 << "  " << ms << "ms"
                 << "  " << nps/1000 << "K nps"
                 << "\n";
        }
    }

    cout << "\n" << string(60, '=') << "\n";
    cout << "Result: " << (all_pass ? "ALL PASS" : "FAILURES DETECTED") << "\n";
    return all_pass;
}

// ── Main ────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    Attacks::init();
    Zobrist::init();

    int max_depth = 5;
    if (argc > 1) max_depth = atoi(argv[1]);

    cout << "Photon Chess Engine — Perft Test Suite\n";
    cout << string(60, '=') << "\n";
    cout << "Testing up to depth " << max_depth << "\n";

    bool result = run_tests(max_depth);
    return result ? 0 : 1;
}

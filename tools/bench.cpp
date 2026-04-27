/**
 * bench.cpp
 * Standalone benchmark tool.
 * Measures nodes/sec and depth reached for a set of positions.
 */

#include "../engine/board.h"
#include "../engine/search.h"
#include "../engine/tt.h"
#include "../engine/eval.h"
#include "../engine/attacks.h"
#include "../engine/zobrist.h"
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace std;

static const char* BENCH_FENS[] = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    nullptr
};

int main(int argc, char* argv[]) {
    Attacks::init();
    Zobrist::init();

    int depth = 10;
    if (argc > 1) depth = atoi(argv[1]);

    TranspositionTable tt(64);
    Evaluator eval;
    SearchEngine engine(tt, eval);

    Limits l;
    l.max_depth  = depth;
    l.infinite   = true;

    uint64_t total_nodes = 0;
    auto t_start = chrono::steady_clock::now();

    cout << "Photon Chess Engine — Benchmark (depth " << depth << ")\n";
    cout << string(70, '=') << "\n";
    cout << left << setw(50) << "Position" << setw(10) << "Nodes" << "Best\n";
    cout << string(70, '-') << "\n";

    for (int i = 0; BENCH_FENS[i]; ++i) {
        Board b;
        b.set_fen(BENCH_FENS[i]);
        tt.clear();

        auto t0 = chrono::steady_clock::now();
        Move best = engine.search(b, l);
        auto t1 = chrono::steady_clock::now();
        int64_t ms = chrono::duration_cast<chrono::milliseconds>(t1-t0).count();

        uint64_t n = engine.last_stats.nodes + engine.last_stats.qnodes;
        total_nodes += n;

        string best_str;
        if (!best.is_none()) {
            best_str = sq_to_str(best.from()) + sq_to_str(best.to());
            if (best.type() == PROMOTION) {
                const char* promo = "nbrq";
                best_str += promo[best.promo_pt() - KNIGHT];
            }
        }

        string fen_short = string(BENCH_FENS[i]).substr(0, 47) + "...";
        cout << left << setw(50) << fen_short
             << setw(10) << n
             << best_str << "\n";
    }

    auto t_end = chrono::steady_clock::now();
    int64_t total_ms = chrono::duration_cast<chrono::milliseconds>(t_end-t_start).count();
    uint64_t nps = (total_ms > 0) ? total_nodes * 1000 / total_ms : total_nodes;

    cout << string(70, '=') << "\n";
    cout << "Total nodes : " << total_nodes << "\n";
    cout << "Total time  : " << total_ms << " ms\n";
    cout << "Nodes/sec   : " << nps << "\n";

    return 0;
}

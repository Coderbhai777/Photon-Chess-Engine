/**
 * selfplay.cpp
 * Self-play harness: plays N games between two engine instances
 * (same binary, different depths) and reports W/L/D statistics.
 * Useful for regression testing and ELO estimation.
 */

#include "../engine/board.h"
#include "../engine/search.h"
#include "../engine/movegen.h"
#include "../engine/attacks.h"
#include "../engine/zobrist.h"
#include "../engine/tt.h"
#include "../engine/eval.h"
#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <iomanip>

using namespace std;

// Game result
enum Result { WIN_WHITE, WIN_BLACK, DRAW };

// Play one game between two engine configs
static Result play_game(int depth1, int depth2, bool verbose) {
    TranspositionTable tt1(16), tt2(16);
    Evaluator eval1, eval2;
    SearchEngine eng1(tt1, eval1), eng2(tt2, eval2);

    Board b;
    b.set_fen(START_FEN);

    int move_count = 0;
    vector<Key> position_history;

    auto mv_to_str = [](Move m) {
        if (m.is_none()) return string("none");
        string s = sq_to_str(m.from()) + sq_to_str(m.to());
        if (m.type() == PROMOTION) {
            const char* p = "nbrq";
            s += p[m.promo_pt()-KNIGHT];
        }
        return s;
    };

    while (true) {
        // Check 3-fold repetition
        int reps = 0;
        for (Key k : position_history)
            if (k == b.hash) ++reps;
        if (reps >= 2) return DRAW;

        // 50-move rule
        if (b.halfmove_clock >= 100) return DRAW;

        // Check for legal moves
        MoveList ml = MoveGen::generate(b);
        if (ml.count == 0) {
            if (b.in_check())
                return (b.side_to_move == WHITE) ? WIN_BLACK : WIN_WHITE;
            return DRAW;
        }

        // Move limit
        if (move_count >= 500) return DRAW;

        Limits l;
        bool white_to_move = (b.side_to_move == WHITE);
        l.max_depth = white_to_move ? depth1 : depth2;

        SearchEngine& eng = white_to_move ? eng1 : eng2;
        TranspositionTable& tt = white_to_move ? tt1 : tt2;
        tt.new_search();

        Move best = eng.search(b, l);
        if (best.is_none()) return DRAW;

        if (verbose) {
            cout << (white_to_move ? "W" : "B") << " plays " << mv_to_str(best) << "\n";
        }

        position_history.push_back(b.hash);
        b.make_move(best);
        ++move_count;
    }
}

int main(int argc, char* argv[]) {
    Attacks::init();
    Zobrist::init();

    int num_games = 20;
    int depth1    = 5;    // "engine 1" depth
    int depth2    = 4;    // "engine 2" depth
    bool verbose  = false;

    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "--games"  && i+1<argc) num_games = atoi(argv[++i]);
        if (a == "--depth1" && i+1<argc) depth1    = atoi(argv[++i]);
        if (a == "--depth2" && i+1<argc) depth2    = atoi(argv[++i]);
        if (a == "--verbose")            verbose   = true;
    }

    cout << "Photon Self-Play Harness\n";
    cout << "Engine 1 (White): depth " << depth1 << "\n";
    cout << "Engine 2 (Black): depth " << depth2 << "\n";
    cout << "Games: " << num_games << "\n";
    cout << string(50, '=') << "\n";

    int w=0, l=0, d=0;
    for (int g = 0; g < num_games; ++g) {
        // Alternate colours
        int d1 = (g % 2 == 0) ? depth1 : depth2;
        int d2 = (g % 2 == 0) ? depth2 : depth1;

        Result r = play_game(d1, d2, verbose);

        bool eng1_is_white = (g % 2 == 0);
        if (r == WIN_WHITE)       { if (eng1_is_white) ++w; else ++l; }
        else if (r == WIN_BLACK)  { if (eng1_is_white) ++l; else ++w; }
        else                      { ++d; }

        cout << "Game " << setw(3) << (g+1) << ": "
             << (r==WIN_WHITE ? "1-0" : r==WIN_BLACK ? "0-1" : "1/2")
             << "  [W=" << w << " D=" << d << " L=" << l << "]\n";
        cout.flush();
    }

    cout << string(50, '=') << "\n";
    double score = w + d * 0.5;
    double pct   = (num_games > 0) ? score / num_games * 100.0 : 50.0;

    // Elo difference estimate: Δelo = 400 * log10(score / (1 - score))
    double elo_diff = 0.0;
    if (score > 0 && score < num_games)
        elo_diff = 400.0 * log10(score / (num_games - score));

    cout << fixed << setprecision(1);
    cout << "Final score (E1): " << score << "/" << num_games
         << " (" << pct << "%)\n";
    cout << "ELO diff estimate: " << (elo_diff >= 0 ? "+" : "") << elo_diff << "\n";
    return 0;
}

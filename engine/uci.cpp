/**
 * uci.cpp
 * Full UCI protocol implementation.
 * Spawns search on a separate thread; handles "stop" asynchronously.
 */

#include "uci.h"
#include "movegen.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>

using namespace std;

// ─────────────────────────────────────────────────────────────────
UCI::UCI()
    : tt_(64)
    , eval_()
    , engine_(tt_, eval_)
{
    board_.set_fen(START_FEN);
}

UCI::~UCI() {
    wait_for_search();
}

// ── Move formatting ────────────────────────────────────────────
string UCI::move_to_str(Move m) {
    if (m.is_none()) return "0000";
    if (m.type() == CASTLING) {
        int f_from = file_of(m.from());
        int f_to = file_of(m.to());
        Square k_to = make_square(f_to > f_from ? 6 : 2, rank_of(m.from()));
        return sq_to_str(m.from()) + sq_to_str(k_to);
    }
    string s = sq_to_str(m.from()) + sq_to_str(m.to());
    if (m.type() == PROMOTION) {
        const char* promo = "nbrq";
        s += promo[m.promo_pt() - KNIGHT];
    }
    return s;
}

Move UCI::str_to_move(const Board& b, const string& s) {
    if (s.size() < 4) return MOVE_NONE;
    Square from = str_to_sq(s.substr(0,2));
    Square to   = str_to_sq(s.substr(2,2));

    MoveList ml = MoveGen::generate(b);
    for (int i = 0; i < ml.count; ++i) {
        Move m = ml.moves[i];
        if (m.type() == CASTLING) {
            int f_from = file_of(m.from());
            int f_to = file_of(m.to());
            Square k_to = make_square(f_to > f_from ? 6 : 2, rank_of(m.from()));
            if (m.from() == from && k_to == to) return m;
        } else if (m.from() == from && m.to() == to) {
            if (m.type() == PROMOTION) {
                if (s.size() < 5) continue;
                char pc = s[4];
                PieceType pt = (pc=='q'||pc=='Q') ? QUEEN :
                               (pc=='r'||pc=='R') ? ROOK  :
                               (pc=='b'||pc=='B') ? BISHOP: KNIGHT;
                if (m.promo_pt() != pt) continue;
            }
            return m;
        }
    }
    return MOVE_NONE;
}

// ── Helpers ────────────────────────────────────────────────────
void UCI::wait_for_search() {
    if (search_thread_.joinable()) {
        engine_.stop();
        search_thread_.join();
        search_running_.store(false);
    }
}

// ── UCI commands ───────────────────────────────────────────────
void UCI::cmd_uci() {
    cout << "id name Photon Fusion 2.0\n";
    cout << "id author Photon Evolution Team\n";
    cout << "option name Hash type spin default 64 min 1 max 2048\n";
    cout << "option name Threads type spin default 1 min 1 max 1\n";
    cout << "option name Move Overhead type spin default 10 min 0 max 5000\n";
    cout << "option name MultiPV type spin default 1 min 1 max 10\n";
    cout << "uciok\n";
    cout.flush();
}

void UCI::cmd_isready() {
    cout << "readyok\n";
    cout.flush();
}

void UCI::cmd_ucinewgame() {
    wait_for_search();
    tt_.clear();
    board_.set_fen(START_FEN);
}

void UCI::cmd_position(const string& line) {
    istringstream ss(line);
    string token;
    ss >> token; // "position"
    ss >> token;

    if (token == "startpos") {
        board_.set_fen(START_FEN);
        ss >> token; // optional "moves"
    } else if (token == "fen") {
        string fen;
        string t;
        while (ss >> t && t != "moves")
            fen += (fen.empty() ? "" : " ") + t;
        board_.set_fen(fen);
        token = t;  // might be "moves"
    }

    if (token == "moves") {
        string mv_str;
        while (ss >> mv_str) {
            Move m = str_to_move(board_, mv_str);
            if (!m.is_none())
                board_.make_move(m);
        }
    }
}

Limits UCI::parse_go(const string& line, Color stm) {
    Limits l;
    istringstream ss(line);
    string token;
    ss >> token; // "go"

    while (ss >> token) {
        if      (token == "depth")     { ss >> l.max_depth; }
        else if (token == "movetime")  { ss >> l.movetime_ms; }
        else if (token == "wtime")     { ss >> l.wtime_ms; }
        else if (token == "btime")     { ss >> l.btime_ms; }
        else if (token == "winc")      { ss >> l.winc_ms; }
        else if (token == "binc")      { ss >> l.binc_ms; }
        else if (token == "movestogo") { ss >> l.movestogo; }
        else if (token == "infinite")  { l.infinite = true; }
    }
    return l;
}

void UCI::cmd_go(const string& line) {
    wait_for_search();
    Limits l = parse_go(line, board_.side_to_move);

    // Capture current board state for the search thread
    Board search_board = board_;
    search_running_.store(true);

    search_thread_ = thread([this, search_board, l]() mutable {
        Move best = engine_.search(search_board, l);
        cout << "bestmove " << move_to_str(best) << "\n";
        cout.flush();
        search_running_.store(false);
    });
}

void UCI::cmd_stop() {
    wait_for_search();
}

void UCI::cmd_quit() {
    wait_for_search();
    exit(0);
}

void UCI::cmd_setoption(const string& line) {
    istringstream ss(line);
    string token, name, value;
    ss >> token; // "setoption"
    ss >> token; // "name"
    ss >> name;
    ss >> token; // "value"
    ss >> value;

    if (name == "Hash") {
        int mb = stoi(value);
        tt_.resize(mb);
    } else if (name == "MultiPV") {
        engine_.set_multipv(stoi(value));
    }
}

// ── Non-UCI debug commands ─────────────────────────────────────
void UCI::cmd_display() {
    board_.print();
}

// Perft — used for correctness validation
static uint64_t perft_inner(Board& b, int depth) {
    if (depth == 0) return 1;
    MoveList ml = MoveGen::generate(b);
    if (depth == 1) return ml.count;

    uint64_t total = 0;
    for (int i = 0; i < ml.count; ++i) {
        b.make_move(ml.moves[i]);
        total += perft_inner(b, depth - 1);
        b.unmake_move(ml.moves[i]);
    }
    return total;
}

void UCI::cmd_perft(int depth) {
    auto t0 = chrono::steady_clock::now();
    MoveList ml = MoveGen::generate(board_);
    uint64_t total = 0;

    for (int i = 0; i < ml.count; ++i) {
        board_.make_move(ml.moves[i]);
        uint64_t n = perft_inner(board_, depth - 1);
        board_.unmake_move(ml.moves[i]);
        total += n;
        cout << move_to_str(ml.moves[i]) << ": " << n << '\n';
    }

    auto t1 = chrono::steady_clock::now();
    int64_t ms = chrono::duration_cast<chrono::milliseconds>(t1-t0).count();
    uint64_t nps = (ms > 0) ? total * 1000 / ms : 0;

    cout << "\nNodes searched: " << total
         << "  Time: " << ms << "ms"
         << "  NPS: " << nps << "\n";
    cout.flush();
}

// Benchmark against standard positions
void UCI::cmd_bench() {
    static const char* bench_fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        nullptr
    };

    Limits l;
    l.max_depth = 10;
    uint64_t total_nodes = 0;
    auto t0 = chrono::steady_clock::now();

    for (int i = 0; bench_fens[i]; ++i) {
        board_.set_fen(bench_fens[i]);
        tt_.clear();
        Move best = engine_.search(board_, l);
        total_nodes += engine_.last_stats.nodes + engine_.last_stats.qnodes;
        cout << "bestmove " << move_to_str(best) << "\n";
    }

    auto t1 = chrono::steady_clock::now();
    int64_t ms = chrono::duration_cast<chrono::milliseconds>(t1-t0).count();
    uint64_t nps = (ms > 0) ? total_nodes * 1000 / ms : 0;

    cout << "\n=== Bench Results ===\n";
    cout << "Nodes: " << total_nodes << "\n";
    cout << "Time:  " << ms << " ms\n";
    cout << "NPS:   " << nps << "\n";
    cout.flush();
}

// ── Main loop ──────────────────────────────────────────────────
void UCI::loop() {
    // Initialise subsystems
    Attacks::init();
    Zobrist::init();
    board_.set_fen(START_FEN);

    cout << "Photon Fusion Engine 2.0 - Powered by Stockfish Search Heuristics\n";
    cout.flush();

    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;

        istringstream ss(line);
        string cmd;
        ss >> cmd;

        if      (cmd == "uci")        cmd_uci();
        else if (cmd == "isready")    cmd_isready();
        else if (cmd == "ucinewgame") cmd_ucinewgame();
        else if (cmd == "position")   cmd_position(line);
        else if (cmd == "go")         cmd_go(line);
        else if (cmd == "stop")       cmd_stop();
        else if (cmd == "quit")       { cmd_quit(); break; }
        else if (cmd == "setoption")  cmd_setoption(line);
        else if (cmd == "d")          cmd_display();
        else if (cmd == "perft") {
            int depth = 5;
            ss >> depth;
            cmd_perft(depth);
        }
        else if (cmd == "bench")      cmd_bench();
        else {
            // Unknown command — ignore silently (UCI spec)
        }
    }
}

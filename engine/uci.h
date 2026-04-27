/**
 * uci.h
 * UCI (Universal Chess Interface) protocol implementation.
 * Parses commands from stdin and drives the search/board state.
 */

#pragma once
#include "board.h"
#include "search.h"
#include "tt.h"
#include "eval.h"
#include <string>
#include <thread>
#include <atomic>

class UCI {
public:
    UCI();
    ~UCI();

    // Main loop — reads from stdin until "quit"
    void loop();

private:
    Board              board_;
    TranspositionTable tt_;
    Evaluator          eval_;
    SearchEngine       engine_;

    std::thread        search_thread_;
    std::atomic<bool>  search_running_{false};

    // ── Command handlers
    void cmd_uci();
    void cmd_isready();
    void cmd_ucinewgame();
    void cmd_position(const std::string& line);
    void cmd_go(const std::string& line);
    void cmd_stop();
    void cmd_quit();
    void cmd_setoption(const std::string& line);
    void cmd_display();          // non-UCI: "d" to print board
    void cmd_perft(int depth);   // non-UCI: "perft N"
    void cmd_bench();            // non-UCI: "bench"

    // Helpers
    void wait_for_search();
    static Limits parse_go(const std::string& line, Color stm);
    static std::string move_to_str(Move m);
    static Move str_to_move(const Board& b, const std::string& s);
};

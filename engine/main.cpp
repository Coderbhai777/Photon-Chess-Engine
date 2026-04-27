/**
 * main.cpp
 * Entry point — initialises the engine and starts the UCI loop.
 */

#include "uci.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // Initialise all precomputed tables
    Attacks::init();
    Zobrist::init();

    UCI uci;
    uci.loop();
    return 0;
}

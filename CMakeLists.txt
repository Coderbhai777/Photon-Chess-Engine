cmake_minimum_required(VERSION 3.16)
project(PhotonChess CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Compiler flags ──────────────────────────────────────────────
if(MSVC)
    set(ENGINE_FLAGS /O2 /Oi /arch:AVX2 /GL /W4)
    set(ENGINE_LINK_FLAGS /LTCG)
else()
    set(ENGINE_FLAGS -O3 -march=native -ffast-math -funroll-loops -Wall -Wextra)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

# ── Engine library (shared between all targets) ─────────────────
set(ENGINE_SOURCES
    engine/attacks.cpp
    engine/zobrist.cpp
    engine/board.cpp
    engine/movegen.cpp
    engine/eval.cpp
    engine/tt.cpp
    engine/search.cpp
    engine/uci.cpp
)

add_library(photon_engine STATIC ${ENGINE_SOURCES})
target_compile_options(photon_engine PRIVATE ${ENGINE_FLAGS})
target_include_directories(photon_engine PUBLIC ${CMAKE_SOURCE_DIR}/engine)

# ── Main UCI binary ─────────────────────────────────────────────
add_executable(photon engine/main.cpp)
target_link_libraries(photon photon_engine)
target_compile_options(photon PRIVATE ${ENGINE_FLAGS})

# Link threads
find_package(Threads REQUIRED)
target_link_libraries(photon Threads::Threads)
target_link_libraries(photon_engine Threads::Threads)

# ── Perft test binary ───────────────────────────────────────────
add_executable(perft_test tests/perft_test.cpp)
target_link_libraries(perft_test photon_engine Threads::Threads)
target_compile_options(perft_test PRIVATE ${ENGINE_FLAGS})

# ── Benchmark binary ────────────────────────────────────────────
add_executable(bench tools/bench.cpp)
target_link_libraries(bench photon_engine Threads::Threads)
target_compile_options(bench PRIVATE ${ENGINE_FLAGS})

# ── Self-play binary ────────────────────────────────────────────
add_executable(selfplay tools/selfplay.cpp)
target_link_libraries(selfplay photon_engine Threads::Threads)
target_compile_options(selfplay PRIVATE ${ENGINE_FLAGS})

# ── CTest integration ───────────────────────────────────────────
enable_testing()
add_test(NAME PerftDepth4 COMMAND perft_test 4)
add_test(NAME PerftDepth5 COMMAND perft_test 5)

# ── Install ─────────────────────────────────────────────────────
install(TARGETS photon perft_test bench selfplay
        RUNTIME DESTINATION bin)

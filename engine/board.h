/**
 * board.h
 * Position representation and make/unmake move logic.
 * Stores piece bitboards, castling rights, en-passant square,
 * halfmove clock, Zobrist hash, and a state stack for unmake.
 */

#pragma once
#include "types.h"
#include "attacks.h"
#include "zobrist.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------
// Irreversible state snapshot stored before each make_move()
// ---------------------------------------------------------------
struct StateInfo {
    Key          hash;
    Square       ep_square;
    int          castling_rights;
    int          halfmove_clock;
    Piece        captured;      // piece on to-square before move
};

// ---------------------------------------------------------------
// Board
// ---------------------------------------------------------------
class Board {
public:
    // ── Piece placement ────────────────────────────────────────
    Bitboard pieces_by_type[PIECE_TYPE_NB] = {};  // [PAWN..KING]
    Bitboard pieces_by_color[COLOR_NB]     = {};  // [WHITE, BLACK]
    Piece    board[64]                     = {};   // piece on each square

    // ── Game state ─────────────────────────────────────────────
    Color   side_to_move   = WHITE;
    int     castling_rights = ANY_CASTLING;
    Square  ep_square       = SQ_NONE;
    int     halfmove_clock  = 0;
    int     fullmove_number = 1;
    Key     hash            = 0;

    // State stack for unmake
    std::vector<StateInfo> history;

    // ── Constructors ────────────────────────────────────────────
    Board() {}

    // Load a FEN string
    bool set_fen(const std::string& fen);

    // FEN of current position
    std::string to_fen() const;

    // ── Accessors ───────────────────────────────────────────────
    Bitboard all_pieces()    const { return pieces_by_color[WHITE] | pieces_by_color[BLACK]; }
    Bitboard pieces(Color c) const { return pieces_by_color[c]; }
    Bitboard pieces(PieceType pt)  const { return pieces_by_type[pt]; }
    Bitboard pieces(Color c, PieceType pt) const {
        return pieces_by_color[c] & pieces_by_type[pt];
    }
    Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const {
        return pieces_by_color[c] & (pieces_by_type[pt1] | pieces_by_type[pt2]);
    }

    Piece   piece_on(Square s) const { return board[s]; }
    bool    empty(Square s)    const { return board[s] == NO_PIECE; }

    Square  king_sq(Color c) const {
        return lsb(pieces(c, KING));
    }

    // ── Check & pin detection ───────────────────────────────────
    Bitboard attackers_to(Square s, Bitboard occ) const;
    bool     in_check()    const { return !!checkers(); }
    Bitboard checkers()    const;

    // Pinned pieces for the side to move
    Bitboard pinned_pieces(Color c) const;

    // ── Make / Unmake ───────────────────────────────────────────
    void make_move(Move m);
    void unmake_move(Move m);

    // Null move (for null-move pruning)
    void make_null_move();
    void unmake_null_move();

    // ── Move legality ────────────────────────────────────────────
    bool is_legal(Move m) const;

    // ── Debug ────────────────────────────────────────────────────
    void print() const;

private:
    void put_piece(Piece p, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);
    void update_castling_rights(Square from, Square to);
};

// Starting position FEN
inline const char* START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

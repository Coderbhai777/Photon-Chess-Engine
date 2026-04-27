/**
 * types.h
 * Core type definitions for the Photon Chess Engine.
 * Defines bitboards, squares, pieces, moves, and basic constants.
 */

#pragma once
#include <cstdint>
#include <string>
#include <cassert>

// ───────────────────────────────────────────────
// Primitive types
// ───────────────────────────────────────────────
using Bitboard = uint64_t;
using Key      = uint64_t;   // Zobrist hash
using Score    = int;

// ───────────────────────────────────────────────
// Squares  (a1=0 … h8=63, little-endian rank-file)
// ───────────────────────────────────────────────
enum Square : int {
    A1,B1,C1,D1,E1,F1,G1,H1,
    A2,B2,C2,D2,E2,F2,G2,H2,
    A3,B3,C3,D3,E3,F3,G3,H3,
    A4,B4,C4,D4,E4,F4,G4,H4,
    A5,B5,C5,D5,E5,F5,G5,H5,
    A6,B6,C6,D6,E6,F6,G6,H6,
    A7,B7,C7,D7,E7,F7,G7,H7,
    A8,B8,C8,D8,E8,F8,G8,H8,
    SQ_NONE = 64
};

inline Square operator+(Square s, int i) { return Square(int(s)+i); }
inline Square operator-(Square s, int i) { return Square(int(s)-i); }
inline Square& operator++(Square& s)     { return s = Square(int(s)+1); }

inline int file_of(Square s) { return s & 7; }
inline int rank_of(Square s) { return s >> 3; }
inline Square make_square(int file, int rank) { return Square((rank<<3)|file); }

// ───────────────────────────────────────────────
// Colors
// ───────────────────────────────────────────────
enum Color : int { WHITE = 0, BLACK = 1, COLOR_NB = 2 };
inline Color operator~(Color c) { return Color(c ^ 1); }

// ───────────────────────────────────────────────
// Piece types & pieces
// ───────────────────────────────────────────────
enum PieceType : int {
    NO_PIECE_TYPE = 0,
    PAWN=1, KNIGHT=2, BISHOP=3, ROOK=4, QUEEN=5, KING=6,
    PIECE_TYPE_NB = 7
};

enum Piece : int {
    NO_PIECE = 0,
    W_PAWN=1, W_KNIGHT=2, W_BISHOP=3, W_ROOK=4, W_QUEEN=5, W_KING=6,
    B_PAWN=9, B_KNIGHT=10,B_BISHOP=11,B_ROOK=12,B_QUEEN=13,B_KING=14,
    PIECE_NB = 16
};

inline Piece make_piece(Color c, PieceType pt) {
    return Piece((c << 3) | pt);
}
inline PieceType type_of(Piece p)  { return PieceType(p & 7); }
inline Color     color_of(Piece p) { return Color(p >> 3); }

// ───────────────────────────────────────────────
// Move encoding (16-bit)
//   bits  0- 5: from square
//   bits  6-11: to square
//   bits 12-13: promotion piece type - 2 (0=knight,1=bishop,2=rook,3=queen)
//   bits 14-15: move type  (0=normal,1=promotion,2=en passant,3=castling)
// ───────────────────────────────────────────────
enum MoveType : int {
    NORMAL      = 0,
    PROMOTION   = 1 << 14,
    EN_PASSANT  = 2 << 14,
    CASTLING    = 3 << 14
};

struct Move {
    uint16_t data = 0;

    Move() = default;
    explicit Move(uint16_t d) : data(d) {}

    static Move make(Square from, Square to, MoveType mt = NORMAL, PieceType promo = KNIGHT) {
        return Move(uint16_t(mt | ((promo - KNIGHT) << 12) | (to << 6) | from));
    }

    Square    from()    const { return Square(data & 0x3F); }
    Square    to()      const { return Square((data >> 6) & 0x3F); }
    MoveType  type()    const { return MoveType(data & (3 << 14)); }
    PieceType promo_pt()const { return PieceType(((data >> 12) & 3) + KNIGHT); }

    bool is_none()      const { return data == 0; }
    bool operator==(Move o) const { return data == o.data; }
    bool operator!=(Move o) const { return data != o.data; }
};

inline const Move MOVE_NONE = Move(0);

// ───────────────────────────────────────────────
// Castling rights bitmask
// ───────────────────────────────────────────────
enum CastlingRights : int {
    NO_CASTLING     = 0,
    WHITE_OO        = 1,
    WHITE_OOO       = 2,
    BLACK_OO        = 4,
    BLACK_OOO       = 8,
    ANY_CASTLING    = 15
};

// ───────────────────────────────────────────────
// Directions
// ───────────────────────────────────────────────
enum Direction : int {
    NORTH =  8, SOUTH = -8,
    EAST  =  1, WEST  = -1,
    NORTH_EAST = 9, NORTH_WEST = 7,
    SOUTH_EAST =-7, SOUTH_WEST =-9
};

// ───────────────────────────────────────────────
// Bitboard helpers
// ───────────────────────────────────────────────
inline Bitboard sq_bb(Square s) { return Bitboard(1) << s; }

inline int popcount(Bitboard b) {
#ifdef _MSC_VER
    return (int)__popcnt64(b);
#else
    return __builtin_popcountll(b);
#endif
}

inline Square lsb(Bitboard b) {
    assert(b);
#ifdef _MSC_VER
    unsigned long idx;
    _BitScanForward64(&idx, b);
    return Square(idx);
#else
    return Square(__builtin_ctzll(b));
#endif
}

inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// Rank / file masks
constexpr Bitboard RANK1_BB = 0x00000000000000FFULL;
constexpr Bitboard RANK2_BB = 0x000000000000FF00ULL;
constexpr Bitboard RANK3_BB = 0x0000000000FF0000ULL;
constexpr Bitboard RANK4_BB = 0x00000000FF000000ULL;
constexpr Bitboard RANK5_BB = 0x000000FF00000000ULL;
constexpr Bitboard RANK6_BB = 0x0000FF0000000000ULL;
constexpr Bitboard RANK7_BB = 0x00FF000000000000ULL;
constexpr Bitboard RANK8_BB = 0xFF00000000000000ULL;
constexpr Bitboard FILEA_BB = 0x0101010101010101ULL;
constexpr Bitboard FILEH_BB = 0x8080808080808080ULL;

inline Bitboard rank_bb(int r) { return RANK1_BB << (r * 8); }
inline Bitboard file_bb(int f) { return FILEA_BB << f; }

// Shift with bounds safety
inline Bitboard shift(Bitboard b, Direction d) {
    switch (d) {
        case NORTH:      return b << 8;
        case SOUTH:      return b >> 8;
        case EAST:       return (b & ~FILEH_BB) << 1;
        case WEST:       return (b & ~FILEA_BB) >> 1;
        case NORTH_EAST: return (b & ~FILEH_BB) << 9;
        case NORTH_WEST: return (b & ~FILEA_BB) << 7;
        case SOUTH_EAST: return (b & ~FILEH_BB) >> 7;
        case SOUTH_WEST: return (b & ~FILEA_BB) >> 9;
        default:         return 0;
    }
}

// ───────────────────────────────────────────────
// Score constants
// ───────────────────────────────────────────────
constexpr Score SCORE_INFINITE  =  32767;
constexpr Score SCORE_NONE      = -32768;
constexpr Score SCORE_MATE      =  32000;
constexpr Score SCORE_MATED     = -32000;
constexpr Score SCORE_DRAW      =  0;
constexpr int   MAX_PLY         =  128;
constexpr int   MAX_MOVES       =  256;

inline bool is_mate_score(Score s) {
    return std::abs(s) > SCORE_MATE - MAX_PLY;
}

// Mate in N plies
inline Score mate_in(int ply)  { return SCORE_MATE - ply; }
inline Score mated_in(int ply) { return -SCORE_MATE + ply; }

// ───────────────────────────────────────────────
// String helpers
// ───────────────────────────────────────────────
inline std::string sq_to_str(Square s) {
    if (s == SQ_NONE) return "-";
    char buf[3] = { char('a' + file_of(s)), char('1' + rank_of(s)), '\0' };
    return std::string(buf);
}

inline Square str_to_sq(const std::string& s) {
    if (s.size() < 2) return SQ_NONE;
    return make_square(s[0]-'a', s[1]-'1');
}

/**
 * board.cpp
 * Full implementation of Board: FEN parsing, make/unmake,
 * attack/check detection, and Zobrist hash maintenance.
 */

#include "board.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <stdexcept>

// ── Piece helpers ──────────────────────────────────────────────
void Board::put_piece(Piece p, Square s) {
    board[s] = p;
    pieces_by_type[type_of(p)]   |= sq_bb(s);
    pieces_by_color[color_of(p)] |= sq_bb(s);
    hash ^= Zobrist::piece_keys[p][s];
}

void Board::remove_piece(Square s) {
    Piece p = board[s];
    board[s] = NO_PIECE;
    pieces_by_type[type_of(p)]   &= ~sq_bb(s);
    pieces_by_color[color_of(p)] &= ~sq_bb(s);
    hash ^= Zobrist::piece_keys[p][s];
}

void Board::move_piece(Square from, Square to) {
    Piece p = board[from];
    hash ^= Zobrist::piece_keys[p][from] ^ Zobrist::piece_keys[p][to];
    board[to]  = p;
    board[from]= NO_PIECE;
    Bitboard mask = sq_bb(from) | sq_bb(to);
    pieces_by_type[type_of(p)]   ^= mask;
    pieces_by_color[color_of(p)] ^= mask;
}

// ── Castling rights update ─────────────────────────────────────
// Any move from/to a corner or king square revokes rights
static constexpr int CASTLING_RIGHTS_MASK[64] = {
    13,15,15,15,12,15,15,14,  // a1=~WHITE_OOO=0xD e1=~(WK|WQ)=0xC h1=~WHITE_OO=0xE
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,
     7,15,15,15, 3,15,15,11   // a8=~BLACK_OOO=0x7 e8=~(BK|BQ)=0x3 h8=~BLACK_OO=0xB
};

void Board::update_castling_rights(Square from, Square to) {
    int mask = CASTLING_RIGHTS_MASK[from] & CASTLING_RIGHTS_MASK[to];
    hash ^= Zobrist::castling_keys[castling_rights];
    castling_rights &= mask;
    hash ^= Zobrist::castling_keys[castling_rights];
}

// ── FEN parsing ────────────────────────────────────────────────
bool Board::set_fen(const std::string& fen) {
    // Clear
    for (auto& p : board) p = NO_PIECE;
    for (auto& b : pieces_by_type)  b = 0;
    for (auto& b : pieces_by_color) b = 0;
    hash = 0;
    history.clear();

    std::istringstream ss(fen);
    std::string placement, stm_str, castle_str, ep_str;
    int hmclock=0, fmnum=1;
    ss >> placement >> stm_str >> castle_str >> ep_str >> hmclock >> fmnum;

    // Piece placement
    int rank=7, file=0;
    for (char c : placement) {
        if (c == '/') { --rank; file=0; }
        else if (c >= '1' && c <= '8') file += c - '0';
        else {
            static const std::string sym = "  PNBRQK  pnbrqk";
            Piece p = NO_PIECE;
            Color col = std::islower(c) ? BLACK : WHITE;
            char uc = std::toupper(c);
            PieceType pt = NO_PIECE_TYPE;
            switch(uc) {
                case 'P': pt=PAWN;   break;
                case 'N': pt=KNIGHT; break;
                case 'B': pt=BISHOP; break;
                case 'R': pt=ROOK;   break;
                case 'Q': pt=QUEEN;  break;
                case 'K': pt=KING;   break;
            }
            if (pt != NO_PIECE_TYPE) {
                p = make_piece(col, pt);
                put_piece(p, make_square(file, rank));
            }
            ++file;
        }
    }

    // Side to move
    side_to_move = (stm_str == "b") ? BLACK : WHITE;
    if (side_to_move == BLACK) hash ^= Zobrist::side_key;

    // Castling rights
    castling_rights = NO_CASTLING;
    for (char c : castle_str) {
        switch(c) {
            case 'K': castling_rights |= WHITE_OO;  break;
            case 'Q': castling_rights |= WHITE_OOO; break;
            case 'k': castling_rights |= BLACK_OO;  break;
            case 'q': castling_rights |= BLACK_OOO; break;
        }
    }
    hash ^= Zobrist::castling_keys[castling_rights];

    // En passant
    ep_square = (ep_str != "-") ? str_to_sq(ep_str) : SQ_NONE;
    if (ep_square != SQ_NONE)
        hash ^= Zobrist::ep_keys[file_of(ep_square)];

    halfmove_clock  = hmclock;
    fullmove_number = fmnum;
    return true;
}

// ── to_fen ─────────────────────────────────────────────────────
std::string Board::to_fen() const {
    static const char piece_chars[] = " PNBRQK  pnbrqk";
    std::string fen;
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            Piece p = board[make_square(f,r)];
            if (p == NO_PIECE) { ++empty; }
            else {
                if (empty) { fen += char('0'+empty); empty=0; }
                fen += piece_chars[p];
            }
        }
        if (empty) fen += char('0'+empty);
        if (r > 0) fen += '/';
    }
    fen += ' ';
    fen += (side_to_move == WHITE) ? 'w' : 'b';
    fen += ' ';
    if (castling_rights == NO_CASTLING) fen += '-';
    else {
        if (castling_rights & WHITE_OO)  fen += 'K';
        if (castling_rights & WHITE_OOO) fen += 'Q';
        if (castling_rights & BLACK_OO)  fen += 'k';
        if (castling_rights & BLACK_OOO) fen += 'q';
    }
    fen += ' ';
    fen += (ep_square == SQ_NONE) ? "-" : sq_to_str(ep_square);
    fen += ' ';
    fen += std::to_string(halfmove_clock);
    fen += ' ';
    fen += std::to_string(fullmove_number);
    return fen;
}

// ── Attack / check detection ───────────────────────────────────
Bitboard Board::attackers_to(Square s, Bitboard occ) const {
    return (Attacks::pawn(BLACK, s)           & pieces(WHITE, PAWN))
         | (Attacks::pawn(WHITE, s)           & pieces(BLACK, PAWN))
         | (Attacks::knight(s)                & pieces_by_type[KNIGHT])
         | (Attacks::bishop(s, occ)           & (pieces_by_type[BISHOP] | pieces_by_type[QUEEN]))
         | (Attacks::rook(s, occ)             & (pieces_by_type[ROOK]   | pieces_by_type[QUEEN]))
         | (Attacks::king(s)                  & pieces_by_type[KING]);
}

Bitboard Board::checkers() const {
    Color us  = side_to_move;
    Color them = ~us;
    Square ksq = king_sq(us);
    Bitboard occ = all_pieces();
    return attackers_to(ksq, occ) & pieces(them);
}

Bitboard Board::pinned_pieces(Color c) const {
    Color them = ~c;
    Square ksq = king_sq(c);
    Bitboard occ = all_pieces();
    Bitboard pinned = 0;

    // Sliders of the opponent that could pin along a ray to our king
    Bitboard snipers =
        ((Attacks::rook(ksq, 0)   & (pieces(them, ROOK)   | pieces(them, QUEEN)))
       | (Attacks::bishop(ksq, 0) & (pieces(them, BISHOP) | pieces(them, QUEEN))));

    Bitboard b = snipers;
    while (b) {
        Square sniper = pop_lsb(b);
        Bitboard between = Attacks::between_bb[ksq][sniper] & occ;
        if (between && !(between & (between - 1))) {   // exactly one piece between
            if (between & pieces(c))
                pinned |= between;
        }
    }
    return pinned;
}

// ── Legality check ─────────────────────────────────────────────
// Simulate the occupancy changes and check if our king is attacked.
bool Board::is_legal(Move m) const {
    Color us  = side_to_move;
    Color them = ~us;
    Square from = m.from(), to = m.to();
    Bitboard occ = all_pieces();
    Square ksq = king_sq(us);

    // ── En passant ─────────────────────────────────────────
    if (m.type() == EN_PASSANT) {
        Square cap_sq = Square(to + (us == WHITE ? -8 : 8));
        Bitboard occ_after = (occ ^ sq_bb(from) ^ sq_bb(cap_sq)) | sq_bb(to);
        return !(Attacks::rook(ksq, occ_after)   & pieces(them, ROOK,  QUEEN))
             && !(Attacks::bishop(ksq, occ_after) & pieces(them, BISHOP, QUEEN));
    }

    // ── King moves (including castling) ────────────────────
    if (type_of(board[from]) == KING) {
        Square actual_to;
        Bitboard occ_after;

        if (m.type() == CASTLING) {
            bool kingside = file_of(to) > file_of(from);
            actual_to = make_square(kingside ? 6 : 2, rank_of(from));
            Square rook_to = make_square(kingside ? 5 : 3, rank_of(from));
            occ_after = (occ ^ sq_bb(from) ^ sq_bb(to)) | sq_bb(actual_to) | sq_bb(rook_to);
        } else {
            actual_to = to;
            occ_after = (occ ^ sq_bb(from)) | sq_bb(to);
        }
        // Mask out pieces we capture (they can't attack us)
        Bitboard not_captured = ~sq_bb(to);
        if (Attacks::pawn(us, actual_to)          & pieces(them, PAWN)   & not_captured) return false;
        if (Attacks::knight(actual_to)            & pieces(them, KNIGHT) & not_captured) return false;
        if (Attacks::king(actual_to)              & pieces(them, KING))                  return false;
        if (Attacks::bishop(actual_to, occ_after) & (pieces(them, BISHOP) | pieces(them, QUEEN)) & not_captured) return false;
        if (Attacks::rook(actual_to, occ_after)   & (pieces(them, ROOK)   | pieces(them, QUEEN)) & not_captured) return false;
        return true;
    }

    // ── Non-king moves ─────────────────────────────────────
    // Only need to check sliding pieces that could attack through the vacated 'from' square
    Bitboard occ_after = (occ ^ sq_bb(from)) | sq_bb(to);
    Bitboard not_captured = ~sq_bb(to);
    if (Attacks::bishop(ksq, occ_after) & (pieces(them, BISHOP) | pieces(them, QUEEN)) & not_captured)
        return false;
    if (Attacks::rook(ksq, occ_after)   & (pieces(them, ROOK)   | pieces(them, QUEEN)) & not_captured)
        return false;
    return true;
}

// ── make_move ─────────────────────────────────────────────────
void Board::make_move(Move m) {
    // Save state
    StateInfo st;
    st.hash            = hash;
    st.ep_square       = ep_square;
    st.castling_rights = castling_rights;
    st.halfmove_clock  = halfmove_clock;
    st.captured        = NO_PIECE;
    history.push_back(st);

    Color us   = side_to_move;
    Color them = ~us;
    Square from = m.from(), to = m.to();
    Piece  moving = board[from];
    Piece  captured = board[to];

    // Remove existing en-passant hash contribution
    if (ep_square != SQ_NONE)
        hash ^= Zobrist::ep_keys[file_of(ep_square)];
    ep_square = SQ_NONE;

    // Halfmove clock
    ++halfmove_clock;

    switch (m.type()) {
    // ── Normal 
    case NORMAL:
        if (captured != NO_PIECE) {
            history.back().captured = captured;
            remove_piece(to);
            halfmove_clock = 0;
        }
        move_piece(from, to);
        if (type_of(moving) == PAWN) {
            halfmove_clock = 0;
            // Double push — set ep square
            if (std::abs(rank_of(to) - rank_of(from)) == 2) {
                ep_square = Square((from + to) / 2);
                hash ^= Zobrist::ep_keys[file_of(ep_square)];
            }
        }
        break;

    // ── Promotion ───────────────────────────────────────────────
    case PROMOTION:
        halfmove_clock = 0;
        if (captured != NO_PIECE) {
            history.back().captured = captured;
            remove_piece(to);
        }
        remove_piece(from);
        put_piece(make_piece(us, m.promo_pt()), to);
        break;

    // ── En passant ──────────────────────────────────────────────
    case EN_PASSANT: {
        halfmove_clock = 0;
        Square cap_sq = Square(to + (us == WHITE ? -8 : 8));
        history.back().captured = board[cap_sq];
        remove_piece(cap_sq);
        move_piece(from, to);
        break;
    }

    // ── Castling ────────────────────────────────────────────────
    case CASTLING: {
        // 'to' encodes the rook's starting square
        bool kingside = file_of(to) > file_of(from);
        Square rook_from = to;
        Square king_to   = make_square(kingside ? 6 : 2, rank_of(from));
        Square rook_to   = make_square(kingside ? 5 : 3, rank_of(from));
        move_piece(from,      king_to);
        move_piece(rook_from, rook_to);
        break;
    }
    }

    update_castling_rights(from, to);

    // Switch side
    side_to_move = them;
    hash ^= Zobrist::side_key;

    if (us == BLACK) ++fullmove_number;
}

// ── unmake_move ────────────────────────────────────────────────
void Board::unmake_move(Move m) {
    StateInfo& st = history.back();

    side_to_move = ~side_to_move;
    Color us   = side_to_move;
    Color them = ~us;

    Square from = m.from(), to = m.to();

    switch (m.type()) {
    case NORMAL:
        move_piece(to, from);
        if (st.captured != NO_PIECE)
            put_piece(st.captured, to);
        break;

    case PROMOTION:
        remove_piece(to);
        put_piece(make_piece(us, PAWN), from);
        if (st.captured != NO_PIECE)
            put_piece(st.captured, to);
        break;

    case EN_PASSANT: {
        move_piece(to, from);
        Square cap_sq = Square(to + (us == WHITE ? -8 : 8));
        put_piece(st.captured, cap_sq);
        break;
    }

    case CASTLING: {
        bool kingside = file_of(to) > file_of(from);
        Square rook_from = to;
        Square king_to   = make_square(kingside ? 6 : 2, rank_of(from));
        Square rook_to   = make_square(kingside ? 5 : 3, rank_of(from));
        move_piece(king_to, from);
        move_piece(rook_to, rook_from);
        break;
    }
    }

    // Restore state
    hash            = st.hash;
    ep_square       = st.ep_square;
    castling_rights = st.castling_rights;
    halfmove_clock  = st.halfmove_clock;

    if (us == BLACK) --fullmove_number;
    history.pop_back();
}

// ── Null move ─────────────────────────────────────────────────
void Board::make_null_move() {
    StateInfo st;
    st.hash = hash;
    st.ep_square = ep_square;
    st.castling_rights = castling_rights;
    st.halfmove_clock  = halfmove_clock;
    st.captured = NO_PIECE;
    history.push_back(st);

    if (ep_square != SQ_NONE) {
        hash ^= Zobrist::ep_keys[file_of(ep_square)];
        ep_square = SQ_NONE;
    }
    hash ^= Zobrist::side_key;
    side_to_move = ~side_to_move;
    ++halfmove_clock;
}

void Board::unmake_null_move() {
    StateInfo& st = history.back();
    hash            = st.hash;
    ep_square       = st.ep_square;
    castling_rights = st.castling_rights;
    halfmove_clock  = st.halfmove_clock;
    side_to_move    = ~side_to_move;
    history.pop_back();
}

// ── Debug print ────────────────────────────────────────────────
void Board::print() const {
    static const char* pcs = ".PNBRQK..pnbrqk";
    std::cout << "\n    a b c d e f g h\n";
    std::cout << "  +-----------------+\n";
    for (int r = 7; r >= 0; --r) {
        std::cout << (r+1) << " | ";
        for (int f = 0; f < 8; ++f) {
            Piece p = board[make_square(f,r)];
            std::cout << pcs[p] << ' ';
        }
        std::cout << "| " << (r+1) << '\n';
    }
    std::cout << "  +-----------------+\n";
    std::cout << "    a b c d e f g h\n\n";
    std::cout << "FEN: " << to_fen() << "\n";
    std::cout << "Hash: 0x" << std::hex << hash << std::dec << "\n\n";
}

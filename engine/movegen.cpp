/**
 * movegen.cpp
 * Full legal move generation for all piece types, including:
 *   - Pawn pushes, captures, promotions, en passant
 *   - Knight / bishop / rook / queen / king normal moves
 *   - Castling (both sides)
 * Handles pins and checks directly.
 */

#include "movegen.h"

namespace MoveGen {

// ── Internal helpers ───────────────────────────────────────────
static void add_promotions(MoveList& ml, Square from, Square to, bool capture) {
    for (PieceType pt : {QUEEN, ROOK, BISHOP, KNIGHT})
        ml.push(Move::make(from, to, PROMOTION, pt));
}

// Generate moves from a set of destination squares (for one piece)
static void serialize(MoveList& ml, Square from, Bitboard targets) {
    while (targets) ml.push(Move::make(from, pop_lsb(targets)));
}

// ── Core generator ─────────────────────────────────────────────
static void generate_all(const Board& b, MoveList& ml, bool tactical_only) {
    Color us   = b.side_to_move;
    Color them = ~us;
    Bitboard occ  = b.all_pieces();
    Bitboard mine = b.pieces(us);
    Bitboard theirs = b.pieces(them);

    Square ksq = b.king_sq(us);
    Bitboard checkers = b.checkers();
    int num_checkers = popcount(checkers);

    // If double check – only king moves are legal
    Bitboard capture_mask = theirs;
    Bitboard push_mask    = ~occ;

    if (num_checkers == 2) {
        // Only king moves
        Bitboard king_targets = Attacks::king(ksq) & ~mine;
        if (tactical_only) king_targets &= theirs;
        Square sq;
        Bitboard tmp = king_targets;
        while (tmp) {
            sq = pop_lsb(tmp);
            Move mv = Move::make(ksq, sq);
            if (b.is_legal(mv)) ml.push(mv);
        }
        return;
    }

    if (num_checkers == 1) {
        // Must capture checker or block
        Square checker_sq = lsb(checkers);
        capture_mask = checkers;
        push_mask    = Attacks::between_bb[ksq][checker_sq];
    }

    Bitboard valid_targets = capture_mask | push_mask;
    if (tactical_only) valid_targets &= theirs;

    Bitboard pinned = b.pinned_pieces(us);

    // ── Pawns ──────────────────────────────────────────────────
    Bitboard pawns = b.pieces(us, PAWN);
    Direction up     = (us == WHITE) ? NORTH      : SOUTH;
    Direction up_e   = (us == WHITE) ? NORTH_EAST : SOUTH_EAST;
    Direction up_w   = (us == WHITE) ? NORTH_WEST : SOUTH_WEST;
    Bitboard rank7   = (us == WHITE) ? RANK7_BB   : RANK2_BB;
    Bitboard rank3   = (us == WHITE) ? RANK3_BB   : RANK6_BB;

    // Unpinned pawns
    Bitboard free_pawns   = pawns & ~pinned;
    Bitboard pinned_pawns = pawns &  pinned;

    // Single pushes (and promotions — promotions are always generated, even in tactical_only)
    {
        Bitboard single = shift(free_pawns, up) & ~occ;
        Bitboard promo_rank = (us == WHITE) ? RANK8_BB : RANK1_BB;
        Bitboard promo  = single & promo_rank;
        Bitboard nonpro = single & ~promo_rank;

        if (!tactical_only) {
            nonpro &= push_mask;
            while (nonpro) {
                Square to = pop_lsb(nonpro);
                Square from = Square(to - up);
                Move mv = Move::make(from, to);
                if (b.is_legal(mv)) ml.push(mv);
            }
            // Double pushes
            Bitboard dbl = shift(single & rank3, up) & ~occ & push_mask;
            while (dbl) {
                Square to = pop_lsb(dbl);
                Square from = Square(to - 2*up);
                Move mv = Move::make(from, to);
                if (b.is_legal(mv)) ml.push(mv);
            }
        }
        // Promotions by push (always generated — they're tactical)
        promo &= push_mask;
        while (promo) {
            Square to = pop_lsb(promo);
            Square from = Square(to - up);
            // Check legality with queen promo (if queen legal, all are)
            Move test = Move::make(from, to, PROMOTION, QUEEN);
            if (b.is_legal(test))
                add_promotions(ml, from, to, false);
        }
    }

    // Captures
    {
        Bitboard cap_e = shift(free_pawns, up_e) & theirs & valid_targets;
        Bitboard cap_w = shift(free_pawns, up_w) & theirs & valid_targets;
        Bitboard promo_rank = (us == WHITE) ? RANK8_BB : RANK1_BB;
        // Split promotion captures from normal captures
        Bitboard promo_e = cap_e & promo_rank;
        Bitboard promo_w = cap_w & promo_rank;
        cap_e &= ~promo_rank;
        cap_w &= ~promo_rank;

        while (cap_e) { Square to=pop_lsb(cap_e); Move mv=Move::make(Square(to-up_e),to); if(b.is_legal(mv)) ml.push(mv); }
        while (cap_w) { Square to=pop_lsb(cap_w); Move mv=Move::make(Square(to-up_w),to); if(b.is_legal(mv)) ml.push(mv); }
        while (promo_e) {
            Square to=pop_lsb(promo_e);
            Square from=Square(to-up_e);
            Move test = Move::make(from, to, PROMOTION, QUEEN);
            if (b.is_legal(test)) add_promotions(ml, from, to, true);
        }
        while (promo_w) {
            Square to=pop_lsb(promo_w);
            Square from=Square(to-up_w);
            Move test = Move::make(from, to, PROMOTION, QUEEN);
            if (b.is_legal(test)) add_promotions(ml, from, to, true);
        }
    }

    // Pinned pawns – must move along pin ray
    {
        Bitboard pp = pinned_pawns;
        Bitboard promo_rank = (us == WHITE) ? RANK8_BB : RANK1_BB;
        Bitboard start_rank = (us == WHITE) ? RANK2_BB : RANK7_BB;
        while (pp) {
            Square from = pop_lsb(pp);
            Bitboard pin_ray = Attacks::line_bb[ksq][from];

            if (!tactical_only) {
                // Single push along pin ray
                Square to1 = Square(from + up);
                if (to1 < SQ_NONE && to1 >= A1 && b.empty(to1) && (pin_ray & sq_bb(to1))) {
                    if (sq_bb(to1) & push_mask) {
                        if (sq_bb(to1) & promo_rank) {
                            add_promotions(ml, from, to1, false);
                        } else {
                            ml.push(Move::make(from, to1));
                        }
                    }
                    // Double push from starting rank
                    if (sq_bb(from) & start_rank) {
                        Square to2 = Square(to1 + up);
                        if (to2 < SQ_NONE && to2 >= A1 && b.empty(to2) && (pin_ray & sq_bb(to2)) && (sq_bb(to2) & push_mask))
                            ml.push(Move::make(from, to2));
                    }
                }
            } else {
                // In tactical_only: still generate promotions along pin ray
                Square to1 = Square(from + up);
                if (to1 < SQ_NONE && to1 >= A1 && b.empty(to1) && (pin_ray & sq_bb(to1))
                    && (sq_bb(to1) & push_mask) && (sq_bb(to1) & promo_rank)) {
                    add_promotions(ml, from, to1, false);
                }
            }

            // Captures along pin ray
            Bitboard cap = Attacks::pawn(us, from) & theirs & pin_ray & capture_mask;
            while (cap) {
                Square to = pop_lsb(cap);
                if (sq_bb(to) & promo_rank) add_promotions(ml, from, to, true);
                else ml.push(Move::make(from, to));
            }
        }
    }

    // En passant
    if (b.ep_square != SQ_NONE) {
        Bitboard ep_attackers = pawns & Attacks::pawn(them, b.ep_square);
        while (ep_attackers) {
            Square from = pop_lsb(ep_attackers);
            Move mv = Move::make(from, b.ep_square, EN_PASSANT);
            if (b.is_legal(mv)) ml.push(mv);
        }
    }

    // ── Knights ────────────────────────────────────────────────
    Bitboard knights = b.pieces(us, KNIGHT) & ~pinned; // pinned knights can't move
    while (knights) {
        Square s = pop_lsb(knights);
        Bitboard targets = Attacks::knight(s) & ~mine & valid_targets;
        while (targets) {
            Square to = pop_lsb(targets);
            Move mv = Move::make(s, to);
            if (b.is_legal(mv)) ml.push(mv);
        }
    }

    // ── Bishops ────────────────────────────────────────────────
    Bitboard bishops = b.pieces(us, BISHOP);
    while (bishops) {
        Square s = pop_lsb(bishops);
        Bitboard targets = Attacks::bishop(s, occ) & ~mine & valid_targets;
        if (pinned & sq_bb(s)) targets &= Attacks::line_bb[ksq][s];
        while (targets) {
            Square to = pop_lsb(targets);
            Move mv = Move::make(s, to);
            if (b.is_legal(mv)) ml.push(mv);
        }
    }

    // ── Rooks ──────────────────────────────────────────────────
    Bitboard rooks = b.pieces(us, ROOK);
    while (rooks) {
        Square s = pop_lsb(rooks);
        Bitboard targets = Attacks::rook(s, occ) & ~mine & valid_targets;
        if (pinned & sq_bb(s)) targets &= Attacks::line_bb[ksq][s];
        while (targets) {
            Square to = pop_lsb(targets);
            Move mv = Move::make(s, to);
            if (b.is_legal(mv)) ml.push(mv);
        }
    }

    // ── Queens ─────────────────────────────────────────────────
    Bitboard queens = b.pieces(us, QUEEN);
    while (queens) {
        Square s = pop_lsb(queens);
        Bitboard targets = Attacks::queen(s, occ) & ~mine & valid_targets;
        if (pinned & sq_bb(s)) targets &= Attacks::line_bb[ksq][s];
        while (targets) {
            Square to = pop_lsb(targets);
            Move mv = Move::make(s, to);
            if (b.is_legal(mv)) ml.push(mv);
        }
    }

    // ── King ───────────────────────────────────────────────────
    {
        Bitboard targets = Attacks::king(ksq) & ~mine;
        if (tactical_only) targets &= theirs;
        while (targets) {
            Square to = pop_lsb(targets);
            Move mv = Move::make(ksq, to);
            if (b.is_legal(mv)) ml.push(mv);
        }
    }

    // ── Castling ───────────────────────────────────────────────
    if (!tactical_only && num_checkers == 0) {
        int rights = b.castling_rights;
        if (us == WHITE) {
            // Kingside
            if ((rights & WHITE_OO) &&
                b.empty(F1) && b.empty(G1) &&
                !(b.attackers_to(F1, occ) & theirs) &&
                !(b.attackers_to(G1, occ) & theirs))
            {
                ml.push(Move::make(E1, H1, CASTLING));
            }
            // Queenside
            if ((rights & WHITE_OOO) &&
                b.empty(D1) && b.empty(C1) && b.empty(B1) &&
                !(b.attackers_to(D1, occ) & theirs) &&
                !(b.attackers_to(C1, occ) & theirs))
            {
                ml.push(Move::make(E1, A1, CASTLING));
            }
        } else {
            if ((rights & BLACK_OO) &&
                b.empty(F8) && b.empty(G8) &&
                !(b.attackers_to(F8, occ) & theirs) &&
                !(b.attackers_to(G8, occ) & theirs))
            {
                ml.push(Move::make(E8, H8, CASTLING));
            }
            if ((rights & BLACK_OOO) &&
                b.empty(D8) && b.empty(C8) && b.empty(B8) &&
                !(b.attackers_to(D8, occ) & theirs) &&
                !(b.attackers_to(C8, occ) & theirs))
            {
                ml.push(Move::make(E8, A8, CASTLING));
            }
        }
    }
}

MoveList generate(const Board& b) {
    MoveList ml;
    generate_all(b, ml, false);
    return ml;
}

MoveList generate_tactical(const Board& b) {
    MoveList ml;
    generate_all(b, ml, true);
    return ml;
}

} // namespace MoveGen

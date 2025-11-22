#pragma once

#include "core/core.h"
#include "core/square_set/occupancy.h"

struct CompoundMove {
    Square to : 8;  // TODO C++20: Can use default initialization for bit-fields
    uint8_t promo = 0;
    FromTo second = {Square(0), Square(0)};
    CompoundMove() = default;
    CompoundMove(Square to, uint8_t promo, FromTo second) : to(to), promo(promo), second(second) {}
};

inline int noPromo(MoveKind kind) {
    constexpr MoveKind noPromoKinds[] = {MoveKind::Quiet_Move,
                                         MoveKind::Double_Push,
                                         MoveKind::O_O,
                                         MoveKind::O_O_O,
                                         MoveKind::Capture,
                                         MoveKind::En_Passant,
                                         MoveKind::Quiet_Move,
                                         MoveKind::Quiet_Move,
                                         MoveKind::Quiet_Move,
                                         MoveKind::Quiet_Move,
                                         MoveKind::Quiet_Move,
                                         MoveKind::Quiet_Move,
                                         MoveKind::Capture,
                                         MoveKind::Capture,
                                         MoveKind::Capture,
                                         MoveKind::Capture};
    return index(noPromoKinds[index(kind)]);
}

/** The MovesTable class holds precomputed move data for all pieces. */

class alignas(64) MovesTable {
public:
    MovesTable();

    static const MovesTable& instance() { return movesTable; }

    static SquareSet attackers(Square to) { return instance()._attackers[to]; }
    static SquareSet castlingClear(Color color, MoveKind side) {
        return instance()._castlingClear[int(color)][index(side)];
    }
    static CompoundMove compoundMove(Move move) {
        return movesTable._compound[index(move.kind)][move.to];
    }
    static SquareSet enPassantFrom(Color color, Square from) {
        return movesTable._enPassantFrom[int(color)][int(file(from))];
    }
    static Occupancy occupancyDelta(Move move) {
        return movesTable._occupancyDelta[noPromo(move.kind)][move.from][move.to];
    }

    static SquareSet path(Square from, Square to) { return movesTable.paths[from][to]; }

    static SquareSet possibleCaptures(Piece piece, Square from) {
        return movesTable._captures[index(piece)][from];
    }

    static SquareSet possibleMoves(Piece piece, Square from) {
        return movesTable._moves[index(piece)][from];
    }

private:
    static MovesTable movesTable;

    // precomputed possible moves for each piece type on each square
    SquareSet _moves[kNumPieces][kNumSquares];

    // precomputed possible captures for each piece type on each square
    SquareSet _captures[kNumPieces][kNumSquares];

    // precomputed possible squares that each square can be attacked from
    SquareSet _attackers[kNumSquares];

    // precomputed delta in occupancy as result of a move, but only for non-promotion moves
    // to save on memory: convert promotion kinds using the noPromo function
    Occupancy _occupancyDelta[kNumNoPromoMoveKinds][kNumSquares]
                             [kNumSquares];  // moveKind, from, to

    // precomputed horizontal, diagonal and vertical paths from each square to each other square
    SquareSet paths[kNumSquares][kNumSquares];  // from, to

    // precomputed squares required to be clear for castling
    SquareSet _castlingClear[2][index(MoveKind::O_O_O) + 1];  // color, moveKind

    // precomputed from squares for en passant targets
    SquareSet _enPassantFrom[2][kNumFiles];  // color, file

    // precompute compound moves for castling, en passant and promotions
    CompoundMove _compound[kNumMoveKinds][kNumSquares];  // moveKind, to square

    void initializePieceMovesAndCaptures();
    void initializeAttackers();
    void initializeOccupancyDeltas();
    void initializePaths();
    void initializeCastlingMasks();
    void initializeEnPassantFrom();
    void initializeCompound();
};

inline bool clearPath(SquareSet occupancy, Square from, Square to) {
    return (occupancy & MovesTable::path(from, to)).empty();
}

#pragma once

#include <climits>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "common.h"
#include "square_set.h"

/**
 * Returns the set of squares between two squares, exclusive of the `to` square.
 */
SquareSet path(Square from, Square to);

/**
 * Returns the set of squares between two squares, inclusive of the `to` square.
 */
SquareSet ipath(Square from, Square to);


struct PieceSet {
    uint16_t pieces = 0;
    PieceSet() = default;
    constexpr PieceSet(Piece piece) : pieces(1 << index(piece)) {}
    constexpr PieceSet(PieceType pieceType)
        : pieces((1 << index(addColor(pieceType, Color::w))) |
                 (1 << index(addColor(pieceType, Color::b)))) {}
    constexpr PieceSet(std::initializer_list<Piece> piecesList) {
        for (auto piece : piecesList) pieces |= 1 << index(piece);
    }
    constexpr PieceSet(std::initializer_list<PieceType> types) {
        for (auto pieceType : types)
            pieces |= (1 << index(addColor(pieceType, Color::w))) |
                (1 << index(addColor(pieceType, Color::b)));
    }
    constexpr PieceSet(uint16_t pieces) : pieces(pieces) {}
    constexpr PieceSet operator|(PieceSet other) const { return PieceSet(pieces | other.pieces); }
    bool contains(Piece piece) const { return pieces & (1 << index(piece)); }
};

static constexpr PieceSet sliders = {Piece::B, Piece::b, Piece::R, Piece::r, Piece::Q, Piece::q};

class Occupancy {
    SquareSet _theirs;
    SquareSet _ours;

    // Private constructor to prevent creation of invalid occupancies
    constexpr Occupancy(SquareSet theirs, SquareSet ours) : _theirs(theirs), _ours(ours) {}

public:
    constexpr Occupancy() = default;
    Occupancy(const Board& board, Color activeColor)
        : Occupancy(occupancy(board, !activeColor), occupancy(board, activeColor)) {}

    // Deltas may have corresponding elements set in both theirs and ourss
    static Occupancy delta(SquareSet theirs, SquareSet ours) { return Occupancy(theirs, ours); }

    SquareSet theirs() const { return _theirs; }
    SquareSet ours() const { return _ours; }

    SquareSet operator()(void) const { return SquareSet{_theirs | _ours}; }
    Occupancy operator!() const { return {_ours, _theirs}; }
    Occupancy operator^(Occupancy other) const {
        return {_theirs ^ other._theirs, _ours ^ other._ours};
    }
    Occupancy operator^=(Occupancy other) {
        _theirs ^= other._theirs;
        _ours ^= other._ours;
        return *this;
    }
    bool operator==(Occupancy other) const {
        return _theirs == other._theirs && _ours == other._ours;
    }
    bool operator!=(Occupancy other) const { return !(*this == other); }
};

using CastlingMove = std::array<FromTo, 2>;  // King and Rook moves for castling
struct CastlingInfo {
    const Piece rook;
    const Piece king;
    const CastlingMask kingSideMask;
    const CastlingMask queenSideMask;

    const CastlingMove kingSide;
    const CastlingMove queenSide;

    constexpr CastlingInfo(Color color)
        : rook(addColor(PieceType::ROOK, color)),
          king(addColor(PieceType::KING, color)),
          kingSideMask(color == Color::w ? CastlingMask::K : CastlingMask::k),
          queenSideMask(color == Color::w ? CastlingMask::Q : CastlingMask::q),

          kingSide(color == Color::w ? CastlingMove{FromTo{e1, g1}, FromTo{h1, f1}}
                                     : CastlingMove{FromTo{e8, g8}, FromTo{h8, f8}}),
          queenSide(color == Color::w ? CastlingMove{FromTo{e1, c1}, FromTo{a1, d1}}
                                      : CastlingMove{FromTo{e8, c8}, FromTo{a8, d8}}) {};
};

struct MoveError : public std::exception {
    std::string message;
    MoveError(std::string message) : message(message) {}
    const char* what() const noexcept override { return message.c_str(); }
};

/**
 * Returns true if the given square is attacked by a piece of the given opponent color.
 */
bool isAttacked(const Board& board, Square square, Occupancy occupancy);
bool isAttacked(const Board& board, SquareSet squares, Occupancy occupancy);
bool isAttacked(const Board& board, SquareSet squares, Color opponentColor);

/**
 * Returns the set of squares that is attacking the given square, including pieces of both
 * sides.
 */
SquareSet attackers(const Board& board, Square square, SquareSet occupancy);

/**
 * Returns whether the piece on 'from' can attack the square 'to', ignoring occupancy or en passant.
 */
bool attacks(const Board& board, Square from, Square to);

/** Returns the set of pieces that would result in the king being checked,
    if the piece where to be removed from the board. */
SquareSet pinnedPieces(const Board& board, Occupancy occupancy, Square kingSquare);

struct SearchState {
    SearchState(const Board& board, Turn turn);
    Color active() const { return turn.activeColor(); }

    Occupancy occupancy;
    SquareSet pawns;
    Turn turn;
    Square kingSquare;
    bool inCheck;
    SquareSet pinned;
};

/**
 * Returns true if the given move does not leave the king in check.
 */
bool doesNotCheck(Board& board, const SearchState& state, Move move);

/**
 * Returns the set of squares that needs to be empty for castling to be legal.
 */
SquareSet castlingPath(Color color, MoveKind side);

/**
 * Compute the delta in occupancy due to the given move
 */
Occupancy occupancyDelta(Move move);

/**
 * Computes all legal moves from a given chess position. This function checks for moves
 * that do not leave or place the king of the active color in check, including the special
 * case of castling.
 */
MoveVector allLegalMovesAndCaptures(Turn turn, Board& board);

/**
 * Returns true if the given side has a pawn that may promote on the next move.
 * Does not check for legality of the promotion move.
 */
bool mayHavePromoMove(Color side, Board& board, Occupancy occupancy);

using MoveFun = std::function<void(Board&, MoveWithPieces)>;
void forAllLegalQuiescentMoves(Turn turn, Board& board, int depthleft, MoveFun action);
void forAllLegalMovesAndCaptures(Board& board, SearchState& state, MoveFun action);
inline void forAllLegalMovesAndCaptures(Turn turn, Board& board, MoveFun action) {
    auto state = SearchState(board, turn);
    forAllLegalMovesAndCaptures(board, state, action);
}

size_t countLegalMovesAndCaptures(Board& board, SearchState& state);

MoveVector allLegalQuiescentMoves(Turn turn, Board& board, int depthleft);

struct UndoPosition {
    UndoPosition() : board(), turn(Color::w) {}
    UndoPosition(BoardChange board, Turn turn) : board(board), turn(turn) {}
    BoardChange board;
    Turn turn;
};

/**
 * Decompose a possibly complex move (castling, promotion, en passant) into two simpler moves
 * that allow making and unmaking the change to the board without complex conditional logic.
 */
BoardChange prepareMove(Board& board, Move move);

/**
 * Updates the board with the given move, which may be a capture.
 * Does not perform any legality checks. Any captured piece is returned.
 */
BoardChange makeMove(Board& board, Move move);
BoardChange makeMove(Board& board, BoardChange change);

/**
 * Like the above, but also updates per turn state (active color, castling availability,
 * en passant target, halfmove clock, and fullmove number).
 */
UndoPosition makeMove(Position& position, Move move);
UndoPosition makeMove(Position& position, BoardChange change, Move move);

/**
 * Undoes the given move, restoring the captured piece to the captured square.
 */
void unmakeMove(Board& board, BoardChange undo);
void unmakeMove(Position& position, UndoPosition undo);

/**
 * Like the above, but also updates per turn state (active color, castling availability,
 * en passant target, halfmove clock, and fullmove number).
 */
[[nodiscard]] Position applyMove(Position position, Move move);

Turn applyMove(Turn turn, MoveWithPieces mwp);
void applyMove(SearchState& state, MoveWithPieces mwp);

/**
 *  Returns the castling mask for the castling rights cancelled by the given move.
 */
CastlingMask castlingMask(Square from, Square to);

namespace for_test {

/**
 * Returns a set of squares to which a piece can potentially move. This function does not account
 * for the legality of the move in terms of check conditions and such, but merely provides possible
 * moves based on the movement rules for each piece type.
 */
SquareSet possibleMoves(Piece piece, Square from);

/**
 * Returns a set of squares on which a piece can potentially make a capture. It does not take into
 * account en passant or legality in terms of check conditions and such.
 */

SquareSet possibleCaptures(Piece piece, Square from);

}  // namespace for_test

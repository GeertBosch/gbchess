#pragma once

#include <climits>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include "common.h"
#include "occupancy.h"
#include "square_set.h"

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

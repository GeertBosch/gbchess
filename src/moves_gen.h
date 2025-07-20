#pragma once

#include "common.h"
#include "moves.h"

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
 * Computes all legal moves from a given chess position. This function checks for moves
 * that do not leave or place the king of the active color in check, including the special
 * case of castling.
 */
MoveVector allLegalMoves(Turn turn, Board board);
MoveVector allLegalCaptures(Turn turn, Board board);
MoveVector allLegalMovesAndCaptures(Turn turn, Board& board);

MoveVector allLegalQuiescentMoves(Turn turn, Board& board, int depthleft);

size_t countLegalMovesAndCaptures(Board& board, SearchState& state);

using MoveFun = std::function<void(Board&, MoveWithPieces)>;
void forAllLegalQuiescentMoves(Turn turn, Board& board, int depthleft, MoveFun action);
void forAllLegalMovesAndCaptures(Board& board, SearchState& state, MoveFun action);

inline void forAllLegalMovesAndCaptures(Turn turn, Board& board, MoveFun action) {
    auto state = SearchState(board, turn);
    forAllLegalMovesAndCaptures(board, state, action);
}

/**
 * Returns true if the given move does not leave the king in check.
 */
bool doesNotCheck(Board& board, const SearchState& state, Move move);

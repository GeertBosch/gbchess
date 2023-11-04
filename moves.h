#include <set>

#include "common.h"

/**
 * This availableMoves function iterates over each square on the board. If a piece of the
 * active color is found, it calculates its possible moves using the possibleMoves function
 * you already have. For each possible destination square, it checks if the move would
 * result in self-blocking or moving through other pieces using the movesThroughPieces
 * function. If neither condition is true, the move is added to the set.
 */
std::set<Move> availableMoves(const ChessBoard& board, char activeColor);

/**
 * This function follows the same structure as availableMoves but focuses on captures. It
 * loops through each square on the board, determines if there's a piece of the active color
 * on it, and then finds its possible captures. The result is filtered to exclude self-
 * captures, and those that move through other pieces, adding valid captures to the result
 * set.
 */
std::set<Move> availableCaptures(const ChessBoard& board, char activeColor);

/**
 * Calculates all possible moves for a given chess piece on the board.
 * This function does not account for the legality of the move in terms of check conditions,
 * but merely provides possible moves based on the movement rules for each piece type.
 *
 * @param piece The type of the chess piece ('R', 'N', 'B', 'Q', 'K', or 'P' for white,
 *              'r', 'n', 'b', 'q', 'k', or 'p' for black).
 * @param from The starting square of the piece for which to calculate possible moves.
 * @return A set of squares to which the piece can potentially move.
 */
std::set<Square> possibleMoves(char piece, const Square& from);

/**
 * Calculates all possible capture moves for a given chess piece on the board.
 * This function focuses on the potential captures a piece can make, including pawn's
 * diagonal captures. It does not take into account en passant or legality in terms of
 * check conditions.
 *
 * @param piece The type of the chess piece ('R', 'N', 'B', 'Q', 'K', or 'P' for white,
 *              'r', 'n', 'b', 'q', 'k', or 'p' for black).
 * @param from The starting square of the piece for which to calculate possible capture moves.
 * @return A set of squares from which the piece can potentially make a capture.
 */
std::set<Square> possibleCaptures(char piece, const Square& from);

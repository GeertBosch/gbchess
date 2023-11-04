#include <map>
#include "common.h"

/**
 * This function iterates over each square in the board, uses the pieceValues map to find
 * the value of the piece on that square, and adjusts the total value accordingly. White
 * pieces have positive values, and black pieces have negative values, so the returned value
 * represents the advantage to the white player: positive for white's advantage, negative
 * for black's advantage.
 */
float evaluateBoard(const ChessBoard& board);

/**
 * Evaluates the best moves from a given chess position up to a certain depth.
 * Each move is evaluated based on the static evaluation of the board or by recursive calls
 * to this function, decreasing the depth until it reaches zero.
 *
 * @param position The current chess position to evaluate.
 * @param depth The depth to which the evaluation should be performed.
 * @return A map of moves to their evaluation score.
 */
std::map<Move, float> computeBestMoves(const ChessPosition& position, int depth);

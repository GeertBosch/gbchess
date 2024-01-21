#include <map>
#include <sstream>

#include "common.h"

static float worstEval = -999;
static float drawEval = 0;
static float bestEval = 999;

/**
 * The evaluation is always from the perspective of the active color. Higher evaluations are better,
 * zero indicates a draw. Units of evaluation are roughly the value of a pawn.
 */
struct EvaluatedMove {
    Move move;  // Defaults to an invalid move
    float evaluation;
    bool check;
    bool mate;
    int depth;

    EvaluatedMove() : depth(0) {}
    EvaluatedMove(Move move, bool check, bool mate, float evaluation, int depth)
        : move(move), evaluation(evaluation), check(check), mate(mate), depth(depth) {}
    EvaluatedMove& operator=(const EvaluatedMove& other) = default;
    EvaluatedMove operator-() const {
        auto ret = *this;
        ret.evaluation *= -1;

        return ret;
    }

    operator std::string() const;

    bool operator<(const EvaluatedMove& rhs) const {
        if (!move)
            return true;
        if (evaluation < rhs.evaluation)
            return true;
        if (rhs.evaluation < evaluation)
            return false;
        return depth > rhs.depth;
    }
};

extern uint64_t evalCount;
extern uint64_t cacheCount;

/**
 * This function iterates over each square in the board, uses the pieceValues map to find
 * the value of the piece on that square, and adjusts the total value accordingly. White
 * pieces have positive values, and black pieces have negative values, so the returned value
 * represents the advantage to the white player: positive for white's advantage, negative
 * for black's advantage.
 */
float evaluateBoard(const Board& board);

/**
 * Evaluates the best moves from a given chess position up to a certain depth.
 * Each move is evaluated based on the static evaluation of the board or by recursive calls
 * to this function, decreasing the depth until it reaches zero. It also accounts for checkmate
 * and stalemate situations.
 *
 * @param position The current chess position to evaluate.
 * @param depth The depth to which the evaluation should be performed.
 * @return A map of moves to their evaluation score.
 */
EvaluatedMove computeBestMove(ComputedMoveVector& moves, int depth);

/**
 *  a debugging function to walk the move generation tree of strictly legal moves to count all the
 *  leaf nodes of a certain depth, which can be compared to predetermined values and used to isolate
 *  bugs. (See https://www.chessprogramming.org/Perft)
 */
uint64_t perft(Turn turn, Board& board, int depth);

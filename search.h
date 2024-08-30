#include "common.h"
#include "eval.h"

extern uint64_t evalCount;
extern uint64_t cacheCount;

/**
 * Evaluates the best moves from a given chess position up to a certain depth.
 * Each move is evaluated based on the static evaluation of the board or by recursive calls
 * to this function, decreasing the depth until it reaches zero. It also accounts for checkmate
 * and stalemate situations.
 */
Eval computeBestMove(Position& position, int maxdepth);

/**
 * Interrupt computeBestMove and make it return the best move found sofar. This is the only function
 * that may be called concurrently with any others in the evaluation module.
 *
 */
void stop();

/**
 * Search all tactical moves necessary to achieve a quiet position and return the best score
 */
Score quiesce(Position& position, Score alpha, Score beta, int depthleft);

MoveVector principalVariation(Position position, int depth);

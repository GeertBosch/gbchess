#include "common.h"
#include "eval.h"

#pragma once

namespace search {
extern uint64_t evalCount;
extern uint64_t cacheCount;

/** Returns true iff search should be abandoned. String passed is UCI info string. */
using InfoFn = std::function<bool(std::string info)>;

/**
 * Evaluates the best moves from a given chess position up to a certain depth.
 * Each move is evaluated based on the static evaluation of the board or by recursive calls
 * to this function, decreasing the depth until it reaches zero. It also accounts for checkmate
 * and stalemate situations.
 */
Eval computeBestMove(Position& position, int maxdepth, InfoFn info = nullptr);

/**
 * Interrupt computeBestMove and make it return the best move found sofar.
 */
void stop();

/**
 * Search all tactical moves necessary to achieve a quiet position and return the best score
 */
Score quiesce(Position& position, int depthleft);

MoveVector principalVariation(Position position, int depth);
}  // namespace search
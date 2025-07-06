#pragma once

#include <functional>

#include "common.h"
#include "eval.h"
#include "pv.h"


namespace search {
extern uint64_t evalCount;
extern uint64_t cacheCount;
extern uint64_t quiescenceCount;

constexpr bool transpositionTableDebug = false;
constexpr bool alphaBetaDebug = false;


/** Returns true iff search should be abandoned. String passed is UCI info string. */
using InfoFn = std::function<bool(std::string info)>;

/**
 * Evaluates the best variation from a given chess position up to the given maxdepth number of half
 * moves (plies). The evaluation of each position is done using a quiescence search, which aims to
 * only evaluate positions where there are no captures, promotions or checks. Evaluations use the
 * following approach:
 *   - Static piece values
 *   - Piece-square tables
 *   - Tapered evaluation
 *
 * The search is done using the alpha-beta algorithm with fail-soft negamax search, using the
 * following optimizations:
 *   - Iterative deepening
 *   - Aspiration windows
 *   - Move ordering using Most Valuable Victim / Least Valuable Attacker (MVV/LVA)
 *   - Transposition table
 * For accurate game play regarding repeated moves and the fifty-move rule, the search should
 * pass the start position and the moves played so far.
 *
 * The info function is called with UCI info strings, and should return true if the search should
 * be abandoned. This is the only way to interrupt the search.
 */
PrincipalVariation computeBestMove(Position position,
                                   int maxdepth,
                                   MoveVector moves = {},
                                   InfoFn info = nullptr);

/**
 * Reset internal caches such as the transposition table to prepare for a new game.
 */
void newGame();

/**
 * Search all tactical moves necessary to achieve a quiet position and return the best score
 */
Score quiesce(Position& position, int depthleft);

}  // namespace search
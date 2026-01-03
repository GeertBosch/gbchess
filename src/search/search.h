#pragma once

#include <functional>

#include "core/core.h"
#include "eval/eval.h"
#include "pv.h"


namespace search {
extern uint64_t evalCount;
extern uint64_t cacheCount;
extern uint64_t quiescenceCount;
extern uint64_t nullMoveAttempts;
extern uint64_t nullMoveCutoffs;
extern uint64_t lmrReductions;
extern uint64_t lmrResearches;
extern uint64_t betaCutoffs;
extern uint64_t firstMoveCutoffs;
extern int maxSelDepth;

// Diagnostic counters for null move skipping reasons
extern uint64_t nullMoveSkippedInCheck;
extern uint64_t nullMoveSkippedMate;
extern uint64_t nullMoveSkippedEndgame;
extern uint64_t nullMoveSkippedPV;
extern uint64_t nullMoveSkippedBehind;
extern uint64_t futilityPruned;
extern uint64_t ttCutoffs;
extern uint64_t ttRefinements;
extern uint64_t countermoveAttempts;
extern uint64_t countermoveHits;

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

/**
 * Calculates MVV-LVA (Most Valuable Victim - Least Valuable Attacker) score for a capture move.
 * Returns the MVV-LVA score component (not including base capture score).
 * Used as a fallback when Static Exchange Evaluation is disabled.
 */
int scoreMVVLVA(const Board& board, Move move);

}  // namespace search
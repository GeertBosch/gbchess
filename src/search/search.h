#pragma once

#include <functional>
#include <iosfwd>

#include "core/core.h"
#include "eval/eval.h"
#include "pv.h"


namespace search {
extern int maxSelDepth;

extern uint64_t evalCount;
extern uint64_t nodeCount;
extern uint64_t cacheCount;
extern uint64_t quiescenceCount;

// Diagnostic counters
extern uint64_t betaCutoffs;
extern uint64_t countermoveAttempts;
extern uint64_t countermoveHits;
extern uint64_t firstMoveCutoffs;
extern uint64_t futilityPruned;
extern uint64_t lmrAttempts;
extern uint64_t lmrResearches;
extern uint64_t nullMoveAttempts;
extern uint64_t nullMoveCutoffs;
extern uint64_t pvsAttempts;
extern uint64_t pvsResearches;
extern uint64_t rootBetaCutoffs;
extern uint64_t rootCutoffMove1;
extern uint64_t rootCutoffMoveLe3;
extern uint64_t rootCutoffMoveGt3;
extern uint64_t ply1BetaCutoffs;
extern uint64_t ply1CutoffMove1;
extern uint64_t ply1CutoffMoveLe3;
extern uint64_t ply1CutoffMoveGt3;
extern uint64_t rootMoveVisits;
extern uint64_t rootSearchCalls;
extern uint64_t aspirationAttempts;
extern uint64_t aspirationFailLow;
extern uint64_t aspirationFailHigh;
extern uint64_t aspirationFallbackFullWindow;
extern uint64_t qsTTCutoffs;
extern uint64_t qsTTRefinements;
extern uint64_t ttCutoffs;
extern uint64_t ttRefinements;

constexpr bool transpositionTableDebug = false;

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
 * The search save state consists of the following parts:
 *     ┌────────────────────────────────────────────────────────┐
 *     │  uint64_t  numEntries                                  │
 *     │  uint8_t   numGenerations                              │  Transposition Table
 *     │  Entry     ttTable[numEntries]                         │
 *     ├────────────────────────────────────────────────────────┤
 *     │  size_t    history[2][kNumSquares][kNumSquares]        │  History Heuristic Table
 *     ├────────────────────────────────────────────────────────┤
 *     │  Move      killerMoves[maxKillerDepth][maxKillerMoves] │  Killer Move Table
 *     ├────────────────────────────────────────────────────────┤
 *     │  Move      countermoves[2][kNumSquares]                │  Best countermove per color/square
 *     └────────────────────────────────────────────────────────┘
 */

/**
 * Save current search state (TT, repetitions, countermoves, killer moves) to a stream.
 * Returns true on success, false on failure.
 */
bool saveState(std::ostream& out);

/**
 * Clear internal search state such as the transposition table, countermoves, and killer moves.
 */
void clearState();

/**
 * Restore search state from a previously saved stream.
 * Returns true on success, false on failure.
 */
bool restoreState(std::istream& in);

void debugPosition(Position position);
/**
 * Search all tactical moves necessary to achieve a quiet position and return the best score
 */
Score quiesce(Position& position, int depthleft);

}  // namespace search
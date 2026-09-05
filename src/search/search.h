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
extern uint64_t qsNodeCount;

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

/**
 * Mark the specfied position as of interest for debugging, so that any lookup or insert of the
 * position in the transposition table will log debug info.
 */
void debugPosition(Position position);

/**
 * Return a string representation of the transposition table entry for the given position, or an
 * empty string if no entry is found.
 */
std::string lookupPosition(Position position);

/**
 * Remove the specified position from the transposition table. Useful for debugging problematic
 * cached information. Returns true iff an entry was found and removed.
 */
bool invalidatePosition(Position position);

/**
 * The static evaluation of `position`, in centipawns relative to the side to move.
 *
 * The search's single entry into evaluation, and deliberately the only one. It used to be four
 * separate expressions, two of them gated on options::useNNUE with a piece-square fallback and two
 * of them calling the network unconditionally, so UseNNUE=false did not in fact turn the network
 * off; this honours the option at every site rather than preserving that inconsistency.
 *
 * Which evaluation runs is a runtime choice: the piece-square tables when UseNNUE is off, the
 * Stockfish 16.1 network when UseSF16 is on, and the SF12 network otherwise. Both networks report
 * a White-relative score, so only the negation for the side to move is shared between them. The
 * networks load on first use, so an engine that never asks for one never reads its file.
 */
Score staticEval(const Position& position);

/**
 * Load the evaluation network the current options select, if it is not loaded already.
 *
 * Reading a network is a cost of how the engine is configured, not of the move it is about to
 * play, and the SF16.1 Big network is ~116 MB and takes about a quarter of a second. Left to
 * happen on first use, that quarter second lands inside the first search of a process and is
 * charged to that move's clock: under any real time control the first `go` spends its whole
 * budget loading, searches no nodes at all, and has no move to return.
 *
 * Loading stays lazy in the *option* -- an engine that never selects a network never reads its
 * file -- but the laziness must end somewhere outside a search. Call this whenever the selected
 * network may have changed and no clock is running: at startup, and after an option is set.
 *
 * A network that cannot be read is not reported here. The failure surfaces where it already did,
 * on the search that asks for the network, which is a place the engine turns into a UCI response
 * rather than a terminated process.
 */
void warmEvaluation();

/**
 * Search all tactical moves necessary to achieve a quiet position and return the best score
 */
Score quiesce(Position& position, int depthleft);

/**
 * Print cumulative search statistics (node counts, cache hits, pruning rates, etc.) to stderr.
 * Intended to be called on engine exit so the stats cover the full session.
 */
void printStats(std::ostream& out);

}  // namespace search
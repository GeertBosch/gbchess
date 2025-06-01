#include <array>
#include <cstddef>

#pragma once

namespace options {

/**
 * Below options allow disabling various search optimizations.
 */
constexpr int defaultDepth = 12;
constexpr int quiescenceDepth = 9;
constexpr bool iterativeDeepening = true;                 // Aspiration windows need this
constexpr bool lateMoveReductions = true;                 // Reduce moves at the end of the search
constexpr bool historyStore = true;                       // Use beta cutoffs for move ordering
constexpr std::array<int, 2> aspirationWindows{30, 125};  // Given in centipawns, {} disables
constexpr int aspirationWindowMinDepth = 2;               // Minimum depth to use aspiration windows
constexpr int promotionMinDepthLeft = 7;                  // Minimum depth left for promos in QS
constexpr int currmoveMinDepthLeft = 6;                   // Min depth left for currmove progress
constexpr size_t transpositionTableEntries = 1ull << 18;  // Zero means not enabled
constexpr int defaultMoveTime = 8'000;                    // Max time for a move in milliseconds

constexpr bool incrementalEvaluation = true;  // During quiescence, compute evaluation using deltas

constexpr bool transpositionTableDebug = false;
constexpr bool alphaBetaDebug = false;

};  // namespace options

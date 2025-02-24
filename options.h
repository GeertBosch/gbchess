#include <array>
#include <cstddef>

#pragma once

namespace options {

/**
 * Below options allow disabling various search optimizations.
 */
constexpr int defaultDepth = 8;
constexpr int quiescenceDepth = 9;
constexpr bool iterativeDeepening = true;                 // Aspiration windows need this
constexpr std::array<int, 2> aspirationWindows{30, 125};  // Given in centipawns, {} disables
constexpr int aspirationWindowMinDepth = 2;               // Minimum depth to use aspiration windows
constexpr int promotionMinDepthLeft = 7;                  // Minimum depth left for promos in QS
constexpr size_t transpositionTableEntries = 1ull << 15;  // Zero means not enabled

constexpr bool incrementalEvaluation = true;  // During quiescence, compute evaluation using deltas
constexpr bool transpositionTableDebug = false;

};  // namespace options

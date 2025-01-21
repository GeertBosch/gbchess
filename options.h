#include <array>
#include <cstddef>

#pragma once

namespace options {

/**
 * Below options allow disabling various search optimizations.
 *
 */
constexpr int defaultDepth = 7;
constexpr int quiescenceDepth = 4;
constexpr bool iterativeDeepening = true;                 // Aspiration windows need this
constexpr std::array<int, 2> aspirationWindows{30, 125};  // Given in centipawns, {} disables
constexpr size_t transpositionTableEntries = 1ull << 15;  // Zero means not enabled

constexpr size_t stopCheckIterations = 4096;  // Power of two is most efficient
constexpr bool transpositionTableDebug = false;

};  // namespace options

#include <cstddef>

#pragma once

namespace options {

constexpr int defaultDepth = 5;
constexpr int quiescenceDepth = 3;
constexpr size_t transpositionTableEntries = 1ull << 15;
constexpr size_t stopCheckIterations = 4096;  // Power of two is most efficient.

constexpr bool transpositionTableDebug = false;

};  // namespace options

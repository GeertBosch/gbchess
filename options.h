#include <cstddef>

#pragma once

namespace options {

constexpr int defaultDepth = 5;
constexpr int quiescenceDepth = 2;
constexpr size_t transpositionTableEntries = 1ull << 20;
constexpr size_t stopCheckIterations = 4096;  // Power of two is most efficient.

};  // namespace options

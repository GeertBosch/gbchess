#pragma once

#include "common.h"
#include "util/uint128_t.h"
#include <functional>

using NodeCount = uint128_t;
using ProgressCallback = std::function<void(NodeCount)>;

/**
 * A debugging function to walk the move generation tree of strictly legal moves to count all the
 * leaf nodes of a certain depth, which can be compared to predetermined values and used to isolate
 * bugs. (See https://www.chessprogramming.org/Perft)
 */
NodeCount perft(Position position, int depth, const ProgressCallback& callback = nullptr);

/**
 * Access to cached node count for performance reporting
 */
NodeCount getPerftCached();
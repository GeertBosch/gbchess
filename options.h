#pragma once

#include <array>
#include <cstddef>

namespace options {

/**
 * Below options allow disabling or tuning various search optimizations.
 */
constexpr int defaultDepth = 9;                             // Search depth in half-moves (plies)
constexpr int quiescenceDepth = 5;                          // Depth of the quiescence search (QS)
constexpr bool iterativeDeepening = true;                   // Aspiration windows need this
constexpr bool lateMoveReductions = true;                   // Reduce moves at the end of the search
constexpr bool staticExchangeEvaluation = true;             // Use static exchange evaluation
constexpr bool useKillerMoves = true;                       // Use the killer move heuristic
constexpr bool historyStore = true;                         // Use beta cutoffs for move ordering
constexpr std::array<int, 2> aspirationWindows{30, 125};    // Given in centipawns, {} disables
constexpr int aspirationWindowMinDepth = 2;                 // Minimum depth for aspiration windows
constexpr int promotionMinDepthLeft = quiescenceDepth - 2;  // Minimum depth left for promos in QS
constexpr int currmoveMinDepthLeft = 1;                     // Min depth left for currmove progress
constexpr size_t transpositionTableEntries = 1ull << 17;    // Zero means not enabled
constexpr int defaultMoveTime = 20'000;                     // Max time for a move in milliseconds
constexpr bool hash128 = true;                              // Use a 128-bit hash for  positions
constexpr bool cachePerft = hash128;                        // Allow caching perft results
constexpr bool useNNUE = true;                              // Use NNUE evaluation
constexpr bool incrementalEvaluation = true;                // Compute QS evaluation using deltas
constexpr bool nullMovePruning = true;                      // Enable null move pruning
constexpr int nullMoveReduction = 3;                        // Depth reduction for null move search
constexpr int nullMoveMinDepth = 3;                         // Minimum depth to try null move

};  // namespace options

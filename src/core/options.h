#pragma once

#include <array>
#include <cstddef>

namespace options {

/**
 * Below options allow disabling or tuning various search optimizations.
 */
constexpr int defaultDepth = 10;                            // Search depth in half-moves (plies)
constexpr int quiescenceDepth = 5;                          // Depth of the quiescence search (QS)
constexpr bool iterativeDeepening = true;                   // Aspiration windows need this
constexpr bool lateMoveReductions = true;                   // Reduce moves at the end of the search
constexpr bool staticExchangeEvaluation = true;             // Use static exchange evaluation
constexpr bool useKillerMoves = true;                       // Use the killer move heuristic
constexpr bool historyStore = true;                         // Use beta cutoffs for move ordering
constexpr std::array<int, 2> aspirationWindows{30, 125};    // Given in centipawns, {} disables
constexpr int aspirationWindowMinDepth = 2;                 // Minimum depth for aspiration windows
constexpr int promotionMinDepthLeft = quiescenceDepth - 2;  // Minimum depth left for promos in QS
constexpr int checksMinDepthLeft = quiescenceDepth;         // Minimum depth left for checks in QS
constexpr int checksMaxPiecesLeft = 12;                     // Max pieces left for checks in QS
constexpr int currmoveMinDepthLeft = 1;                     // Min depth left for currmove progress
constexpr size_t transpositionTableEntries = 1ull << 20;    // Zero means not enabled
constexpr int defaultMoveTime = 20'000;                     // Max time for a move in milliseconds
constexpr bool useNNUE = true;                              // Use NNUE evaluation
constexpr bool incrementalNNUE = false;       // Use incremental NNUE updates (experimental)
constexpr bool incrementalEvaluation = true;  // Compute QS evaluation using deltas
constexpr bool nullMovePruning = true;        // Enable null move pruning
constexpr int nullMoveReduction = 3;          // Depth reduction for null move search
constexpr int nullMoveMinDepth = 2;           // Minimum depth to try null move
constexpr bool futilityPruning = true;        // Enable futility pruning
constexpr int futilityMaxDepth = 7;           // Maximum depth for futility pruning
constexpr bool useCountermove = false;        // Use the countermove heuristic
constexpr bool verboseSearch = false;         // Print detailed search tree for debugging

/**
 * Caching options for the perft program
 */
constexpr bool cachePerft = true;           // Allow caching
constexpr size_t cachePerftMB = 2 * 1024;   // Memory for perft cache in MB
constexpr size_t cachePerftMinNodes = 100;  // Minimum nodes to cache perft results
constexpr int perftProgressMillis = 100;    // Milliseconds between progress updates
};  // namespace options

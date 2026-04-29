#pragma once

#include <array>
#include <cstddef>

namespace options {

/**
 * Below options allow disabling or tuning various search optimizations.
 */
constexpr int defaultDepth = 15;                            // Search depth in half-moves (plies)
constexpr int quiescenceDepth = 5;                          // Depth of the quiescence search (QS)
constexpr bool iterativeDeepening = true;                   // Aspiration windows need this
constexpr bool lateMoveReductions = true;                   // Reduce search depth of late moves
constexpr bool staticExchangeEvaluation = true;             // Use static exchange evaluation
constexpr int maxKillerMoves = 2;                           // Max killer moves per depth (0=off)
constexpr int maxKillerDepth = 256;                         // Max depth for killer moves (0=off)
constexpr bool historyHeuristic = true;                     // Use beta cutoffs for move ordering
constexpr std::array<int, 0> aspirationWindows{};           // Given in centipawns, {} disables
constexpr int aspirationWindowMinDepth = 2;                 // Minimum depth for aspiration windows
constexpr int promotionMinDepthLeft = 3;                    // Minimum depth left for promos in QS
constexpr int checksMinDepthLeft = quiescenceDepth;         // Minimum depth left for checks in QS
constexpr int checksMaxPiecesLeft = 12;                     // Max pieces left for checks in QS
constexpr int currmoveMinDepthLeft = 1;                     // Min depth left for currmove progress
constexpr size_t transpositionTableMB = 16;                 // Zero means not enabled
constexpr bool useNNUE = true;                              // Use NNUE evaluation
constexpr bool incrementalEvaluation = true;                // Compute QS evaluation using deltas
constexpr bool principleVariationSearch = true;             // Use PVS for moves after the first one
constexpr bool reverseFutilityPruning = true;               // Enable futility pruning
constexpr int reverseFutilityMaxDepthLeft = 3;              // Max depth left for futility pruning
constexpr bool nullMovePruning = true;           // Enable null move pruning
constexpr int nullMoveReduction = 3;             // Depth reduction for null move search
constexpr int nullMoveMinDepth = 2;              // Min depth to try null move
constexpr bool useCountermove = true;            // Use the countermove heuristic
constexpr bool useQsTT = true;                   // Use transposition table in quiescence search
constexpr int useOracleMaxDepth = 0;             // Max depth for oracle ordering (0 to disable)
constexpr int oracleMoveDepth = 3;               // Fixed depth used by oracle to score each move
constexpr bool rootPVS = true;                   // Use null-window probe for non-first root moves
constexpr int rootPVSMaxDepthLeft = 3;           // Enable rootPVS only up to this root depth left
constexpr int qsFrontierInMainMaxDepthLeft = 1;  // Allow QsFrontier TT use in shallow non-PV nodes
constexpr bool moveLevelFutilityPruning = true;  // Prune late quiet moves at shallow non-PV nodes
constexpr int moveFutilityMaxDepthLeft = 3;      // Max depth left for move-level futility pruning
constexpr int moveFutilityMinMoveCount = 2;      // Start pruning from this move count (1-based)
constexpr int moveFutilityMarginSlope = 150;     // Margin slope in cp per depth
constexpr int moveFutilityMarginBase = 240;      // Margin base in cp
constexpr bool internalIterativeDeepening = true;  // Internal iterative deepening with no TT move
constexpr int iidMinDepthLeft = 4;                 // Min depth left to trigger IID
constexpr int iidDepthReduction = 2;               // Depth reduction for IID probe
constexpr bool traceSearchTree = false;            // Print tree diagnostics to stderr
constexpr int traceSearchMaxPly = 2;               // Max ply to print in tree diagnostics
constexpr int fixedNodesSearch = 250'000;          // Fixed nodes per search (0 to disable)

/**
 * Caching options for the perft program
 */
constexpr bool cachePerft = true;           // Allow caching
constexpr size_t cachePerftMB = 2 * 1024;   // Memory for perft cache in MB
constexpr int cachePerftMinNodes = 100;     // Minimum nodes to cache perft results
constexpr int perftProgressMillis = 100;    // Milliseconds between progress updates
};  // namespace options

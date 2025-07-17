#pragma once

#include <cstdint>
#include <string>

namespace nnue {

// Global timing variables for performance analysis (nanoseconds)
extern uint64_t g_transformTimeNanos;
extern uint64_t g_perspectiveTimeNanos;
extern uint64_t g_affineTimeNanos;
extern uint64_t g_totalEvaluations;
extern uint64_t g_totalActiveFeatures;

// Global variables for incremental analysis
extern uint64_t g_totalMoves;
extern uint64_t g_totalFeatureChanges;

/**
 * Print timing statistics for NNUE evaluation performance analysis.
 */
void printTimingStats();

/**
 * Reset timing statistics to zero.
 */
void resetTimingStats();

/**
 * Calculate and display the computational complexity of NNUE evaluation steps.
 */
void analyzeComputationalComplexity();

/**
 * Record a move with the number of feature changes for incremental analysis.
 */
void recordMove(size_t featureChanges);

/**
 * Get average feature changes per move if data is available.
 */
double getAverageFeatureChanges();

}  // namespace nnue

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

}  // namespace nnue

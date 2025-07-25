#include "nnue_stats.h"
#include "nnue.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace nnue {

// Global timing variables for performance analysis
uint64_t g_transformTimeNanos = 0;
uint64_t g_perspectiveTimeNanos = 0;
uint64_t g_affineTimeNanos = 0;
uint64_t g_totalEvaluations = 0;
uint64_t g_totalActiveFeatures = 0;

// Global variables for incremental analysis
uint64_t g_totalMoves = 0;
uint64_t g_totalFeatureChanges = 0;

namespace {

/** Format percentage to one decimal place */
std::string formatPercent(double value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << value << "%";
    return oss.str();
}

/** Format nanoseconds per operation if timing data is available */
std::string formatNanosPerOp(uint64_t totalNanos, size_t operations) {
    if (g_totalEvaluations == 0 || operations == 0) return "";
    double nanosPerOp = totalNanos / (double)(g_totalEvaluations * operations);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3) << " (" << nanosPerOp << " ns/op)";
    return oss.str();
}

/** Get actual average active features if available, otherwise return estimate */
size_t getActiveFeatures() {
    return g_totalEvaluations ? std::round(g_totalActiveFeatures / (double)g_totalEvaluations) : 35;
}

/** Format a header string centered in a 72-character wide line with '=' padding */
std::string formatHeader(std::string text) {
    const size_t targetWidth = 72;

    // If text is too long to add padding, return without padding
    if (text.length() + 2 >= targetWidth) return text;

    // Add space if text and target have different parity to ensure even division
    if (text.length() % 2 != targetWidth % 2) text += " ";

    // Calculate padding and create padding string once
    std::string padding((targetWidth - text.length()) / 2, '=');

    return padding + text + padding;
}

}  // namespace

void printTimingStats() {
    if (g_totalEvaluations == 0) {
        std::cout << "No NNUE evaluations performed yet." << std::endl;
        return;
    }

    uint64_t totalTime = g_transformTimeNanos + g_perspectiveTimeNanos + g_affineTimeNanos;

    std::cout << "\n" << formatHeader(" NNUE Evaluation Timing Statistics ") << std::endl;
    std::cout << "Total evaluations: " << g_totalEvaluations << std::endl;
    std::cout << "Total time: " << totalTime / 1000000.0 << " ms" << std::endl;
    std::cout << "Average time per evaluation: " << (totalTime / g_totalEvaluations) << " ns"
              << std::endl;
    std::cout << "Evaluations per second: " << (g_totalEvaluations * 1000000000.0 / totalTime)
              << std::endl;
    std::cout << "Total active features: " << g_totalActiveFeatures << std::endl;
    std::cout << "Average active features per evaluation: "
              << std::round(g_totalActiveFeatures / (double)g_totalEvaluations) << std::endl;
    std::cout << "\nBreakdown by step:" << std::endl;
    std::cout << "  Transform:      " << (g_transformTimeNanos / g_totalEvaluations) << " ns/eval ("
              << formatPercent(100.0 * g_transformTimeNanos / totalTime) << ")" << std::endl;
    std::cout << "  Perspective:    " << (g_perspectiveTimeNanos / g_totalEvaluations)
              << " ns/eval (" << formatPercent(100.0 * g_perspectiveTimeNanos / totalTime) << ")"
              << std::endl;
    std::cout << "  Affine layers:  " << (g_affineTimeNanos / g_totalEvaluations) << " ns/eval ("
              << formatPercent(100.0 * g_affineTimeNanos / totalTime) << ")" << std::endl;
    std::cout << formatHeader("") << "\n" << std::endl;
}

void resetTimingStats() {
    g_transformTimeNanos = 0;
    g_perspectiveTimeNanos = 0;
    g_affineTimeNanos = 0;
    g_totalEvaluations = 0;
    g_totalActiveFeatures = 0;
}

/**
 * Calculate and display the computational complexity of NNUE evaluation steps.
 */
void analyzeComputationalComplexity() {
    std::cout << "\n" << formatHeader(" NNUE Computational Complexity Analysis ") << std::endl;

    // InputTransform step
    size_t inputDims = InputTransform::kInputDimensions;    // 41024
    size_t halfDims = InputTransform::kHalfDimensions;      // 256
    size_t outputDims = InputTransform::kOutputDimensions;  // 512

    std::cout << "\n1. InputTransform (Feature extraction to Accumulator):" << std::endl;
    std::cout << "   Input dimensions: " << inputDims << std::endl;
    std::cout << "   Output dimensions: " << outputDims << " (2 × " << halfDims << ")" << std::endl;

    // Use actual measured features if available
    size_t activeFeatures = getActiveFeatures();
    size_t transformOps = activeFeatures * outputDims;  // additions only
    if (g_totalEvaluations > 0) {
        std::cout << "   Actual average active features: " << activeFeatures << std::endl;
    } else {
        std::cout << "   Typical active features: ~" << activeFeatures << std::endl;
    }
    std::cout << "   Operations per transform: ~" << transformOps << " additions"
              << formatNanosPerOp(g_transformTimeNanos, transformOps) << std::endl;
    std::cout << "   Incremental potential: HIGH (only changed features need recomputation)"
              << std::endl;

    // Perspective selection - reorganizing 512 values (both perspectives)
    std::cout << "\n2. Perspective Selection:" << std::endl;
    std::cout << "   Operations: " << outputDims << " copy operations"
              << formatNanosPerOp(g_perspectiveTimeNanos, outputDims) << std::endl;

    // Affine layers
    std::cout << "\n3. Affine Layers:" << std::endl;

    // Layer 1: 512 → 32 (NOT incremental - dense matrix multiplication)
    size_t layer1_in = outputDims;  // 512 (full perspective output)
    size_t layer1_out = 32;
    size_t layer1_mults = layer1_in * layer1_out;    // 512 × 32 = 16,384
    size_t layer1_adds = layer1_mults + layer1_out;  // multiplications + bias additions
    std::cout << "   Layer 1 (512→32): " << layer1_mults << " multiplications, " << layer1_adds
              << " additions (NOT incremental - dense matrix)" << std::endl;

    // Layer 2: 32 → 32 (NOT incremental - depends on full layer 1 output)
    size_t layer2_in = 32;
    size_t layer2_out = 32;
    size_t layer2_mults = layer2_in * layer2_out;  // 32 × 32 = 1,024
    size_t layer2_adds = layer2_mults + layer2_out;
    std::cout << "   Layer 2 (32→32): " << layer2_mults << " multiplications, " << layer2_adds
              << " additions" << std::endl;

    // Layer 3: 32 → 1 (NOT incremental - depends on full layer 2 output)
    size_t layer3_in = 32;
    size_t layer3_out = 1;
    size_t layer3_mults = layer3_in * layer3_out;  // 32 × 1 = 32
    size_t layer3_adds = layer3_mults + layer3_out;
    std::cout << "   Layer 3 (32→1): " << layer3_mults << " multiplications, " << layer3_adds
              << " additions" << std::endl;

    // Total affine operations
    size_t total_mults = layer1_mults + layer2_mults + layer3_mults;
    size_t total_adds = layer1_adds + layer2_adds + layer3_adds;

    std::cout << "   Total affine: " << total_mults << " multiplications, " << total_adds
              << " additions" << formatNanosPerOp(g_affineTimeNanos, total_mults + total_adds)
              << std::endl;
    std::cout << "   Incremental potential: NONE in affine layers (all require full dense matrix "
                 "computation)"
              << std::endl;

    // Summary
    std::cout << "\n4. Summary per evaluation:" << std::endl;
    std::cout << "   Transform: ~" << transformOps << " operations (incremental helps here)"
              << std::endl;
    std::cout << "   All affine layers: ~" << (total_mults + total_adds)
              << " operations (incremental CANNOT help - all dense matrices)" << std::endl;
    std::cout << "   Total operations: ~" << (transformOps + total_mults + total_adds)
              << " arithmetic operations" << std::endl;

    size_t incrementalOps = transformOps;  // Only transform can be incremental
    size_t totalOps = transformOps + total_mults + total_adds;
    std::cout << "   Actually incremental: ~" << incrementalOps << " operations ("
              << formatPercent(100.0 * incrementalOps / totalOps) << " of total)" << std::endl;

    // Performance analysis
    std::cout << "\n5. Performance perspective:" << std::endl;

    // Use actual eval rate if we have timing data, otherwise default to 100K
    double evalRate = 100000.0;  // Default 100K eval/sec
    if (g_totalEvaluations > 0) {
        uint64_t totalTime = g_transformTimeNanos + g_perspectiveTimeNanos + g_affineTimeNanos;
        if (totalTime > 0) {
            evalRate = 1e9 * g_totalEvaluations / totalTime;
        }
    }

    std::cout << "   At ~" << static_cast<int>(evalRate) << " eval/sec: ~"
              << totalOps * evalRate / 1000000.0 << " million ops/sec" << std::endl;

    // Use timing percentages instead of operation percentages
    uint64_t totalTime = g_transformTimeNanos + g_perspectiveTimeNanos + g_affineTimeNanos;
    if (g_totalEvaluations > 0 && totalTime > 0) {
        double transformPercent = 100.0 * g_transformTimeNanos / totalTime;
        double affinePercent = 100.0 * g_affineTimeNanos / totalTime;

        std::cout << "   Transform takes " << formatPercent(transformPercent)
                  << " of evaluation time (this is what incremental can help with)" << std::endl;
        std::cout << "   Affine layers take " << formatPercent(affinePercent)
                  << " of evaluation time (incremental CANNOT help with this)" << std::endl;
        std::cout << "   Potentially incremental computation: ~" << formatPercent(transformPercent)
                  << " of evaluation time" << std::endl;
        std::cout << "   With incremental updates, only ~"
                  << formatPercent(100.0 - transformPercent) << " of computation would remain"
                  << std::endl;
    } else {
        std::cout << "   Potentially incremental: "
                  << formatPercent(100.0 * incrementalOps / totalOps) << " of total operations"
                  << std::endl;
        std::cout << "   With incremental updates, only "
                  << formatPercent(100.0 * (totalOps - incrementalOps) / totalOps)
                  << " of computation would remain" << std::endl;
    }

    // Incremental update analysis
    std::cout << "\n" << formatHeader(" Incremental Update Analysis ") << std::endl;

    double avgFeatureChanges = ::nnue::getAverageFeatureChanges();
    std::cout << "\n6. Realistic incremental savings:" << std::endl;
    std::cout << "   Average feature changes per move: ~" << (int)avgFeatureChanges
              << (g_totalMoves ? " (measured)" : " (typical estimate)") << std::endl;

    // For transform: only changed features need recomputation
    size_t incrementalTransformOps = (size_t)(avgFeatureChanges * outputDims);
    double transformSavings = (transformOps > incrementalTransformOps)
        ? 100.0 * (transformOps - incrementalTransformOps) / transformOps
        : 0.0;

    std::cout << "   Transform incremental ops: ~" << incrementalTransformOps << " (saves "
              << formatPercent(transformSavings) << ")" << std::endl;
    std::cout << "   Affine layers: " << (total_mults + total_adds)
              << " ops (NO savings possible - dense matrix computation required)" << std::endl;

    size_t totalIncrementalOps = incrementalTransformOps + (total_mults + total_adds);
    double totalSavings = (totalOps > totalIncrementalOps)
        ? 100.0 * (totalOps - totalIncrementalOps) / totalOps
        : 0.0;

    std::cout << "   Total with incremental: ~" << totalIncrementalOps << " operations"
              << std::endl;
    std::cout << "   Overall computational savings: " << formatPercent(totalSavings) << std::endl;
    std::cout << "   Realistic speedup potential: " << std::fixed << std::setprecision(1)
              << (totalOps / (double)totalIncrementalOps) << "x (modest improvement)" << std::endl;
}

void recordMove(size_t featureChanges) {
    ++g_totalMoves;
    g_totalFeatureChanges += featureChanges;
}

double getAverageFeatureChanges() {
    return g_totalMoves ? (double)g_totalFeatureChanges / g_totalMoves : 4.0;  // Typical estimate
}

}  // namespace nnue

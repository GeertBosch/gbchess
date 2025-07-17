#pragma once

#include "common.h"
#include "eval.h"
#include "nnue.h"
#include <array>
#include <optional>

namespace nnue {

/**
 * EvaluationContext maintains incremental NNUE state across move sequences.
 *
 * Key design principles:
 * 1. Accumulator is maintained incrementally with each move
 * 2. Affine layers are computed fully each time (dense matrices cannot be incremental)
 * 3. RAII-style move application with automatic rollback
 * 4. Minimal overhead when incremental updates aren't beneficial
 */
struct EvaluationContext {
    // Core incremental state
    Accumulator accumulator;                   // 512-element accumulator (both perspectives)
    bool accumulatorValid = false;             // Whether accumulator is valid

    // Network reference
    const NNUE& network;

    // Feature tracking for statistics
    std::vector<size_t> activeFeatures;  // Currently active feature indices

    explicit EvaluationContext(const NNUE& nnue, const Position& position);

    /**
     * RAII-style move application that automatically updates incremental state.
     */
    class MoveScope {
        EvaluationContext& context;
        std::vector<size_t> removedFeatures;
        std::vector<size_t> addedFeatures;
        Accumulator savedAccumulator;
        bool wasAccumulatorValid;

    public:
        MoveScope(EvaluationContext& ctx, Move move, const BoardChange& change);
        ~MoveScope();  // Automatically restores state

        // Get evaluation for the position after the move
        int32_t evaluate();
        Score evaluateAsScore();
    };

    /**
     * Apply a move using incremental change description.
     */
    MoveScope applyMove(Move move, const BoardChange& change);

    /**
     * Get evaluation for current position (non-incremental fallback).
     */
    int32_t evaluatePosition(const Position& position);

private:
    void updateAccumulator(const std::vector<size_t>& removedFeatures,
                           const std::vector<size_t>& addedFeatures);
    std::vector<size_t> extractFeatures(const Position& position);

    /**
     * Efficiently compute feature changes directly from move description.
     * Avoids the expensive process of extracting all features from full positions.
     */
    void computeFeatureChangesFromMove(Move move,
                                       const BoardChange& change,
                                       std::vector<size_t>& removedFeatures,
                                       std::vector<size_t>& addedFeatures);
};

/**
 * Extract feature indices from a position for incremental updates.
 * This identifies which HalfKP features are active.
 */
std::vector<size_t> extractActiveFeatures(const Position& position);

/**
 * Compute the difference between two feature sets for incremental updates.
 */
std::pair<std::vector<size_t>, std::vector<size_t>> computeFeatureDiff(
    const std::vector<size_t>& oldFeatures, const std::vector<size_t>& newFeatures);

}  // namespace nnue

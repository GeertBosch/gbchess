#include "nnue_incremental.h"
#include "nnue_stats.h"
#include <algorithm>
#include <chrono>

namespace nnue {

namespace {

/** Update timing point and return elapsed nanoseconds */
uint64_t updateTiming(std::chrono::high_resolution_clock::time_point& timePoint) {
    using namespace std::chrono;
    auto nextTimePoint = high_resolution_clock::now();
    uint64_t elapsed = duration_cast<nanoseconds>(nextTimePoint - timePoint).count();
    timePoint = nextTimePoint;
    return elapsed;
}

}  // namespace

EvaluationContext::EvaluationContext(const NNUE& nnue, const Position& position)
    : accumulator(nnue.input), network(nnue) {

    // Initialize accumulator from scratch for the starting position
    activeFeatures = extractActiveFeatures(position);

    // Update accumulator based on active features
    for (size_t feature : activeFeatures) {
        for (size_t i = 0; i < accumulator.kDimensions; ++i) {
            accumulator.values[i] +=
                nnue.input.weights[i * InputTransform::kInputDimensions + feature];
        }
    }

    accumulatorValid = true;
}

EvaluationContext::MoveScope::MoveScope(EvaluationContext& ctx,
                                        Move move,
                                        const BoardChange& change)
    : context(ctx), savedAccumulator(ctx.network.input), wasAccumulatorValid(ctx.accumulatorValid) {

    // Compute feature changes directly from move and change description
    // This is much more efficient than extracting all features from full positions
    context.computeFeatureChangesFromMove(move, change, removedFeatures, addedFeatures);

    // Record statistics
    recordMove(removedFeatures.size() + addedFeatures.size());

    // Save state for rollback
    if (wasAccumulatorValid) {
        savedAccumulator = context.accumulator;
    }

    // Apply incremental updates
    context.updateAccumulator(removedFeatures, addedFeatures);
    context.accumulatorValid = true;

    // Note: activeFeatures tracking would need to be updated here for completeness
    // For now, this is a simplified implementation focused on accumulator updates
}

EvaluationContext::MoveScope::~MoveScope() {
    // Restore accumulator state
    if (wasAccumulatorValid) {
        context.accumulator = savedAccumulator;
        context.accumulatorValid = true;
    } else {
        context.accumulatorValid = false;
    }

    // Note: activeFeatures restoration would require storing old features
    // For now, this is a simplified implementation
}

int32_t EvaluationContext::MoveScope::evaluate() {
    // For now, fall back to non-incremental evaluation until we implement
    // the missing helper functions (selectPerspective, affineForward, etc.)
    // This is a placeholder implementation that ensures the code compiles.

    // TODO: We need to store the current position to evaluate it properly
    // For now, return a placeholder value to make compilation work
    ++g_totalEvaluations;
    return 0;  // Placeholder - should use proper NNUE evaluation
}

Score EvaluationContext::MoveScope::evaluateAsScore() {
    return Score::fromCP(evaluate());
}

int32_t EvaluationContext::evaluatePosition(const Position& position) {
    // Fallback to non-incremental evaluation
    return nnue::evaluate(position, network);
}

void EvaluationContext::updateAccumulator(const std::vector<size_t>& removedFeatures,
                                          const std::vector<size_t>& addedFeatures) {
    using namespace std::chrono;
    auto timePoint = high_resolution_clock::now();

    // Remove old features
    for (size_t feature : removedFeatures) {
        for (size_t i = 0; i < accumulator.kDimensions; ++i) {
            accumulator.values[i] -=
                network.input.weights[i * InputTransform::kInputDimensions + feature];
        }
    }

    // Add new features
    for (size_t feature : addedFeatures) {
        for (size_t i = 0; i < accumulator.kDimensions; ++i) {
            accumulator.values[i] +=
                network.input.weights[i * InputTransform::kInputDimensions + feature];
        }
    }

    g_transformTimeNanos += updateTiming(timePoint);
}

std::vector<size_t> extractActiveFeatures(const Position& /* position */) {
    std::vector<size_t> features;
    // This would implement HalfKP feature extraction
    // For now, return placeholder to make compilation work
    return features;
}

std::pair<std::vector<size_t>, std::vector<size_t>> computeFeatureDiff(
    const std::vector<size_t>& oldFeatures, const std::vector<size_t>& newFeatures) {
    std::vector<size_t> removed, added;

    // Find features in old but not in new (removed)
    std::set_difference(oldFeatures.begin(),
                        oldFeatures.end(),
                        newFeatures.begin(),
                        newFeatures.end(),
                        std::back_inserter(removed));

    // Find features in new but not in old (added)
    std::set_difference(newFeatures.begin(),
                        newFeatures.end(),
                        oldFeatures.begin(),
                        oldFeatures.end(),
                        std::back_inserter(added));

    return {removed, added};
}

void EvaluationContext::computeFeatureChangesFromMove(Move move,
                                                      const BoardChange& change,
                                                      std::vector<size_t>& removedFeatures,
                                                      std::vector<size_t>& addedFeatures) {
    removedFeatures.clear();
    addedFeatures.clear();

    // This would implement efficient HalfKP feature change computation directly from move data
    // Key insight: only features related to the moving pieces and affected squares change
    //
    // For a normal move:
    // - Remove features for the piece on the from square
    // - Add features for the piece on the to square
    // - If capture, remove features for the captured piece
    // - Handle special moves (castling, en passant, promotion)
    //
    // This is much more efficient than extracting all ~29 features from full positions
    // and computing set differences, as only ~2-6 features typically change per move.

    // For now, this is a placeholder implementation
    // TODO: Implement proper HalfKP feature change computation using:
    // - move.from() and move.to() squares
    // - change.captured piece information
    // - change.promo for promotions
    // - change.first/second for complex moves like castling

    // Placeholder: assume 4 feature changes (typical estimate)
    // In reality, this would compute the actual changed feature indices
    (void)move;    // Suppress unused parameter warning
    (void)change;  // Suppress unused parameter warning
}

EvaluationContext::MoveScope EvaluationContext::applyMove(Move move, const BoardChange& change) {
    return MoveScope(*this, move, change);
}

}  // namespace nnue

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "common.h"

namespace nnue {

// Rough overview of the NNUE architecture:
//
// Position → Feature Extraction → InputTransform → Accumulator[512×int16_t]
//                                                        ↓
//                          Input Slice[256×uint8_t] ← ClippedReLU (perspective selection)
//                                                        ↓
//                               AffineLayer[32×int32_t] ← 32←256
//                                                        ↓
//                                ClippedReLU[32×uint8_t]
//                                                        ↓
//                               AffineLayer[32×int32_t] ← 32←32
//                                                        ↓
//                                ClippedReLU[32×uint8_t]
//                                                        ↓
//                               AffineLayer[1×int32_t] ← 1←32
//                                                        ↓
//                                     Final Score (centipawns)
//
// The InputTransform takes HalfKP features and produces a 512-dimensional accumulator
// (256 dimensions each for white and black perspectives). The network then selects
// one perspective based on the side to move, applies ClippedReLU, and processes
// through 3 affine layers with ClippedReLU activations to produce the final evaluation.

struct InputTransform {
    constexpr static size_t kInputDimensions = 41024;
    constexpr static size_t kHalfDimensions = 256;
    constexpr static size_t kOutputDimensions = 2 * kHalfDimensions;
    constexpr static uint32_t kHash = 0x5d69d5b8u ^ kOutputDimensions;  // 0x5d69d7b8u
    constexpr static size_t kFileSize = sizeof(uint32_t) + sizeof(int16_t) * kHalfDimensions +
        sizeof(int16_t) * kHalfDimensions * kInputDimensions;
    std::vector<int16_t> bias;     // [kHalfDimensions]
    std::vector<int16_t> weights;  // [kHalfDimensions][kInputDimensions]
    InputTransform() : bias(kHalfDimensions), weights(kHalfDimensions * kInputDimensions) {}
};

/** Feature represents a single HalfKP feature with both white and black perspective indices */
struct Feature {
    size_t whiteIndex;  // Feature index from white perspective
    size_t blackIndex;  // Feature index from black perspective

    constexpr Feature(size_t white, size_t black) : whiteIndex(white), blackIndex(black) {}
    constexpr Feature(PieceType type)  // Create a feature from a piece type
        : whiteIndex(index(type) * 128 + 1), blackIndex(whiteIndex) {}
    constexpr Feature(Color color)  // Create a feature from a color
        : whiteIndex(index(color) * 64), blackIndex(whiteIndex ^ 64) {}
    constexpr Feature(Piece piece)  // Create a feature from a non-king piece
        : Feature{Feature(type(piece)) + Feature(color(piece))} {}
    Feature operator+(const Feature& other) const {
        return Feature(whiteIndex + other.whiteIndex, blackIndex + other.blackIndex);
    }
    Feature operator*(const Feature& other) const {  // For deriving king features
        return Feature(whiteIndex * other.whiteIndex, blackIndex * other.blackIndex);
    }
    constexpr bool operator==(const Feature& other) const {
        return whiteIndex == other.whiteIndex && blackIndex == other.blackIndex;
    }
};

/** Accumulator holds the result of affine transformation of input features of InputTransform */
struct Accumulator {
    constexpr static size_t kDimensions = InputTransform::kOutputDimensions;
    std::vector<int16_t> values;  // [kDimensions]

    /** Initialize accumulator with bias values from InputTransform */
    explicit Accumulator(const InputTransform& inputTransform);
};

/** Affine layer: y = weights * x + bias */
struct AffineLayer {
    using Weight = int8_t;
    using Bias = int32_t;
    AffineLayer(size_t in, size_t out) : in(in), out(out), weights(in * out), bias(out) {}
    const size_t in;
    const size_t out;
    std::vector<Weight> weights;  // flattened row-major: [out][in]
    std::vector<Bias> bias;
};

struct Network {
    static constexpr std::array<size_t, 3> out = {32, 32, 1};
    static constexpr uint32_t kHash = 0x63337156u;
    std::vector<AffineLayer> layers = {AffineLayer(InputTransform::kOutputDimensions, out[0]),
                                       AffineLayer(out[0], out[1]),
                                       AffineLayer(out[1], out[2])};
};
struct FileHeader {
    static constexpr uint32_t kVersion = 0x7af32f16;
    static constexpr uint32_t kHash = InputTransform::kHash ^ Network::kHash;
    std::string name;  // name string for diagnostic purposes
};

/** NNUE network representation (no incremental accumulator yet) */
struct NNUE {
    static constexpr std::array<size_t, 3> out = {32, 32, 1};
    FileHeader header;
    InputTransform input;
    Network network;
};

// ClippedReLU activation function constants
constexpr int kWeightScaleBits = 6;

/**
 * Apply ClippedReLU to a vector of int16_t values, producing uint8_t outputs.
 */
std::vector<uint8_t> applyClippedReLU(const std::vector<int16_t>& input);

/**
 * Select perspective from accumulator based on side to move.
 * Returns 256 values from either white perspective (first half) or black perspective (second half).
 */
std::vector<int16_t> selectPerspective(const Accumulator& accumulator, Color sideToMove);

/**
 * Apply affine transformation: output = weights * input + bias
 * Returns int32_t values before activation.
 */
std::vector<int32_t> affineForward(const std::vector<uint8_t>& input, const AffineLayer& layer);

/**
 * Complete NNUE evaluation for a position.
 * Returns the final evaluation score in centipawns, from the white perspective.
 */
int32_t evaluate(const Position& position, const NNUE& network);

/**
 * Extract active features from a position using HalfKP feature set.
 * Returns a vector of Feature objects containing both white and black perspective indices.
 */
std::vector<Feature> extractActiveFeatures(const Position& position);

/**
 * Transform position features through InputTransform to produce an Accumulator.
 * No overflow occurs by construction - 16-bit ints are sufficient for any
 * possible combination of enabled input features.
 */
Accumulator transform(const Position& position, const InputTransform& inputTransform);

NNUE loadNNUE(const std::string& filename);

// Global timing variables for performance analysis (nanoseconds)
extern uint64_t g_transformTimeNanos;
extern uint64_t g_perspectiveTimeNanos;
extern uint64_t g_affineTimeNanos;
extern uint64_t g_clippedReluTimeNanos;
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
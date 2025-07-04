#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <sys/types.h>
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

constexpr bool nnueDebug = false;  // Set to true for detailed debug output

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

/** Accumulator holds the result of affine transformation of input features of InputTransform */
struct alignas(64) Accumulator {
    constexpr static size_t kDimensions = InputTransform::kOutputDimensions;
    std::array<int16_t, kDimensions> values;

    /** Initialize accumulator with bias values from InputTransform */
    explicit Accumulator(const InputTransform& inputTransform);
};

/** Affine layer: y = weights * x + bias */
template <size_t in, size_t out>
struct AffineLayer {
    using Weight = int8_t;
    using Bias = int32_t;
    using Input = std::array<uint8_t, in>;
    using Output = std::array<Bias, out>;
    using Clipped = std::array<uint8_t, out>;

    static constexpr size_t kInputSize = in;
    static constexpr size_t kOutputSize = out;

    static constexpr size_t kInputDimensions = in;
    static constexpr size_t kOutputDimensions = out;
    static constexpr size_t kWeightDimensions = in * out;

    std::array<std::array<Weight, in>, out> weights;  // [out][in] - row-major
    std::array<Bias, out> bias;
};

struct alignas(64) Network {
    static constexpr std::array<size_t, 3> out = {32, 32, 1};
    static constexpr uint32_t kHash = 0x63337156u;
    using Layer0 = AffineLayer<InputTransform::kOutputDimensions, out[0]>;
    using Layer1 = AffineLayer<out[0], out[1]>;
    using Layer2 = AffineLayer<out[1], out[2]>;
    Layer0 layer0;
    Layer1 layer1;
    Layer2 layer2;
};

struct FileHeader {
    static constexpr uint32_t kVersion = 0x7af32f16;
    static constexpr uint32_t kHash = InputTransform::kHash ^ Network::kHash;
    std::string name;  // name string for diagnostic purposes
};

/** NNUE network representation (no incremental accumulator yet) */
struct NNUE {
    FileHeader header;
    InputTransform input;
    Network network;
};

// ClippedReLU activation function constants
constexpr int kWeightScaleBits = 6;

/**
 * Complete NNUE evaluation for a position.
 * Returns the final evaluation score in centipawns, from the white perspective.
 */
int32_t evaluate(const Position& position, const NNUE& network);

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
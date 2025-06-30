#include "common.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "moves.h"
#include "nnue.h"

namespace nnue {

// Global timing variables for performance analysis
uint64_t g_transformTimeNanos = 0;
uint64_t g_perspectiveTimeNanos = 0;
uint64_t g_affineTimeNanos = 0;
uint64_t g_clippedReluTimeNanos = 0;
uint64_t g_totalEvaluations = 0;
uint64_t g_totalActiveFeatures = 0;

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
    oss << std::fixed << std::setprecision(2) << " (" << nanosPerOp << " ns/op)";
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

/** Read binary data (little endian) */
template <typename T>
T readBinary(std::ifstream& in) {
    T val;
    in.read(reinterpret_cast<char*>(&val), sizeof(T));
    if (!in) throw std::runtime_error("Unexpected EOF");
    return val;
}
/** Read binary vector (little endian) */
template <typename T>
void readBinaryVector(std::ifstream& in, T& vector) {
    in.read(reinterpret_cast<char*>(vector.data()), vector.size() * sizeof(typename T::value_type));
    if (!in) throw std::runtime_error("Unexpected EOF");
}

template <typename T>
std::string toHex(const T& val) {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(2 * sizeof(T)) << std::setfill('0') << val << std::dec;
    return ss.str();
}

bool isPrintableAscii(const std::string& str) {
    for (char c : str)
        if (c < 32 || c > 126) return false;

    return true;
}

constexpr Square rotate(Square square) {  // rotate 180 degrees, switching player perspective
    return Square(kNumSquares - 1 - square);
}

void checkHash(uint32_t expected, uint32_t actual, const std::string& what) {
    if (expected != actual)
        throw std::runtime_error("Invalid " + what + " hash: expected " + toHex(expected) +
                                 ", got " + toHex(actual));
}

FileHeader readHeader(std::ifstream& file) {
    FileHeader header;

    auto version = readBinary<uint32_t>(file);
    auto hash = readBinary<uint32_t>(file);
    auto size = readBinary<uint32_t>(file);
    header.name.resize(size);
    file.read(header.name.data(), size);  // Now read a string with the size just decoded before

    // If version or hash do not match, we can still try to read the name
    bool versionOK = version == nnue::FileHeader::kVersion;
    bool hashOK = hash == nnue::FileHeader::kHash;
    bool nameOK = isPrintableAscii(header.name);
    std::string ok[2] = {"not OK", "OK"};

    // For early diagnostics
    std::cout << "Version: " << toHex(version) << ", " << ok[versionOK] << "\n"
              << "Hash: " << toHex(hash) << ", " << ok[hashOK] << "\n"
              << "Name: " << (nameOK ? header.name : "not OK, " + std::to_string(size) + " bytes")
              << "\n";

    // Check for errors
    if (!nameOK) throw std::runtime_error("Invalid NNUE name (not printable ASCII)");
    if (!versionOK) throw std::runtime_error("Unsupported NNUE version: " + toHex(version));
    checkHash(nnue::FileHeader::kHash, hash, "NNUE file");

    return header;
}

void readInputTransform(std::ifstream& file, InputTransform& input) {
    checkHash(InputTransform::kHash, readBinary<uint32_t>(file), "InputTransform");
    readBinaryVector(file, input.bias);
    readBinaryVector(file, input.weights);
}

void readAffineTransform(std::ifstream& file, AffineLayer& layer) {
    readBinaryVector(file, layer.bias);
    readBinaryVector(file, layer.weights);
}

void readNetwork(std::ifstream& file, Network& net) {
    checkHash(Network::kHash, readBinary<uint32_t>(file), "Network");

    for (size_t i = 0; i < net.layers.size(); ++i) readAffineTransform(file, net.layers[i]);
}
}  // namespace

Accumulator::Accumulator(const InputTransform& inputTransform) : values(kDimensions) {
    for (size_t i = 0; i < InputTransform::kHalfDimensions; ++i) {
        values[i] = inputTransform.bias[i];                                    // White perspective
        values[i + InputTransform::kHalfDimensions] = inputTransform.bias[i];  // Black perspective
    }
}

NNUE loadNNUE(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open NNUE file: " + filename);
    NNUE net;

    net.header = readHeader(file);
    readInputTransform(file, net.input);
    readNetwork(file, net.network);
    std::cout << "NNUE network loaded successfully from: " << filename << "\n";

    return net;
}

/**
 * Extract active features from a position using HalfKP feature set.
 * Returns a vector of Feature objects containing both white and black perspective indices.
 */
std::vector<Feature> extractActiveFeatures(const Position& position) {
    std::vector<Feature> features;
    const auto& board = position.board;

    // Find king positions
    auto whiteKingSquares = find(board, Piece::K);
    auto blackKingSquares = find(board, Piece::k);

    if (whiteKingSquares.empty() || blackKingSquares.empty()) return features;  // Invalid position

    Square whiteKing = *whiteKingSquares.begin();
    Square blackKing = *blackKingSquares.begin();

    // Get all occupied squares and remove kings (they don't contribute to features in HalfKP)
    auto nonKingSquares = occupancy(board) - whiteKingSquares - blackKingSquares;

    auto kings = Feature(whiteKing, rotate(blackKing)) * Feature(PieceType::KING);
    for (Square square : nonKingSquares)
        features.emplace_back(Feature(square, rotate(square)) + Feature(board[square]) + kings);

    return features;
}

void addFeature(Accumulator& accumulator,
                const InputTransform& inputTransform,
                const Feature& feature) {
    size_t whiteOffset = feature.whiteIndex * InputTransform::kHalfDimensions;
    size_t blackOffset = feature.blackIndex * InputTransform::kHalfDimensions;

    for (size_t j = 0; j < InputTransform::kHalfDimensions; ++j) {
        accumulator.values[j] += inputTransform.weights[whiteOffset + j];
        accumulator.values[j + InputTransform::kHalfDimensions] +=
            inputTransform.weights[blackOffset + j];
    }
}

Accumulator transform(const Position& position, const InputTransform& inputTransform) {
    Accumulator accumulator(inputTransform);

    auto activeFeatures = extractActiveFeatures(position);
    g_totalActiveFeatures += activeFeatures.size();  // Track feature count
    for (auto feature : activeFeatures) addFeature(accumulator, inputTransform, feature);

    return accumulator;
}

std::vector<uint8_t> applyClippedReLU(const std::vector<int16_t>& input, int shift = 0) {
    std::vector<uint8_t> output;
    output.reserve(input.size());

    for (int16_t value : input) {
        // Stockfish uses max(0, min(127, value)) for accumulator -> uint8_t conversion
        output.push_back(static_cast<uint8_t>(std::clamp(int(value >> shift), 0, 127)));
    }

    return output;
}

std::vector<int16_t> selectPerspective(const Accumulator& accumulator, Color sideToMove) {
    std::vector<int16_t> perspective;
    perspective.resize(InputTransform::kOutputDimensions);

    auto in1 = accumulator.values.begin();
    auto in2 = accumulator.values.begin() + InputTransform::kHalfDimensions;
    auto out = perspective.begin();

    if (sideToMove == Color::b) std::swap(in1, in2);  // Swap perspectives if black to move
    // This way, we always take the first half for us and second half for them
    for (size_t i = 0; i < InputTransform::kHalfDimensions; ++i) {
        *out++ = static_cast<int16_t>(*in1++);
    }
    for (size_t i = 0; i < InputTransform::kHalfDimensions; ++i) {
        *out++ = static_cast<int16_t>(*in2++);
    }
    return perspective;
}

std::vector<int32_t> affineForward(const std::vector<uint8_t>& input, const AffineLayer& layer) {
    std::vector<int32_t> output(layer.out, 0);  // Explicitly initialize to 0

    // Add weighted inputs: output[i] = bias[i] + sum(weights[i][j] * input[j])
    for (size_t i = 0; i < layer.out; ++i) {
        size_t offset = i * layer.in;
        int32_t sum = layer.bias[i];  // Start with bias

        for (size_t j = 0; j < layer.in; ++j) {
            sum += layer.weights[offset + j] * input[j];
        }
        output[i] = sum;
    }

    return output;
}

int32_t evaluate(const Position& position, const NNUE& network) {
    using namespace std::chrono;

    ++g_totalEvaluations;

    // Step 1: Get accumulator from input transform
    auto timePoint = high_resolution_clock::now();
    Accumulator accumulator = transform(position, network.input);
    auto nextTimePoint = high_resolution_clock::now();
    g_transformTimeNanos += duration_cast<nanoseconds>(nextTimePoint - timePoint).count();

    // Step 2: Select perspective first, then apply ClippedReLU
    timePoint = nextTimePoint;  // Reuse previous end time
    std::vector<int16_t> perspective = selectPerspective(accumulator, position.active());
    nextTimePoint = high_resolution_clock::now();
    g_perspectiveTimeNanos += duration_cast<nanoseconds>(nextTimePoint - timePoint).count();

    timePoint = nextTimePoint;  // Reuse previous end time
    std::vector<uint8_t> layerInput = applyClippedReLU(perspective, 0);
    nextTimePoint = high_resolution_clock::now();
    g_clippedReluTimeNanos += duration_cast<nanoseconds>(nextTimePoint - timePoint).count();

    // Step 3: Process through each affine layer with ClippedReLU activations
    std::vector<int32_t> layerOutput;
    for (size_t i = 0; i < network.network.layers.size(); ++i) {
        const auto& layer = network.network.layers[i];

        // Apply affine transformation
        timePoint = nextTimePoint;  // Reuse previous end time
        layerOutput = affineForward(layerInput, layer);
        nextTimePoint = high_resolution_clock::now();
        g_affineTimeNanos += duration_cast<nanoseconds>(nextTimePoint - timePoint).count();

        if (i == network.network.layers.size()) break;

        // Apply ClippedReLU for hidden layers (not the final output layer)
        timePoint = nextTimePoint;  // Reuse previous end time
        std::vector<int16_t> int16Output;
        int16Output.reserve(layerOutput.size());
        for (int32_t value : layerOutput) {
            int16Output.push_back(static_cast<int16_t>(
                std::clamp(value,
                           static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
                           static_cast<int32_t>(std::numeric_limits<int16_t>::max()))));
        }
        layerInput = applyClippedReLU(int16Output, 6);
        nextTimePoint = high_resolution_clock::now();
        g_clippedReluTimeNanos += duration_cast<nanoseconds>(nextTimePoint - timePoint).count();
    }
    // if (debug) std::cout << "Final layer output: " << layerOutput[0] << std::endl;

    double kScale = 0.0300682;  // Scale factor for centipawns closely approximating Stockfish 12
    return layerOutput[0] * (position.active() == Color::b ? -kScale : kScale);
}

void printTimingStats() {
    if (g_totalEvaluations == 0) {
        std::cout << "No NNUE evaluations performed yet." << std::endl;
        return;
    }

    uint64_t totalTime =
        g_transformTimeNanos + g_perspectiveTimeNanos + g_affineTimeNanos + g_clippedReluTimeNanos;

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
    std::cout << "  ClippedReLU:    " << (g_clippedReluTimeNanos / g_totalEvaluations)
              << " ns/eval (" << formatPercent(100.0 * g_clippedReluTimeNanos / totalTime) << ")"
              << std::endl;
    std::cout << formatHeader("") << "\n" << std::endl;
}

void resetTimingStats() {
    g_transformTimeNanos = 0;
    g_perspectiveTimeNanos = 0;
    g_affineTimeNanos = 0;
    g_clippedReluTimeNanos = 0;
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

    // Perspective selection - reorganizing 512 values (both perspectives)
    std::cout << "\n2. Perspective Selection:" << std::endl;
    std::cout << "   Operations: " << outputDims << " copy operations"
              << formatNanosPerOp(g_perspectiveTimeNanos, outputDims) << std::endl;

    // Affine layers
    std::cout << "\n3. Affine Layers:" << std::endl;

    // Layer 1: 512 → 32
    size_t layer1_in = outputDims;  // 512 (full perspective output)
    size_t layer1_out = 32;
    size_t layer1_mults = layer1_in * layer1_out;    // 512 × 32 = 16,384
    size_t layer1_adds = layer1_mults + layer1_out;  // multiplications + bias additions
    std::cout << "   Layer 1 (512→32): " << layer1_mults << " multiplications, " << layer1_adds
              << " additions" << std::endl;

    // Layer 2: 32 → 32
    size_t layer2_in = 32;
    size_t layer2_out = 32;
    size_t layer2_mults = layer2_in * layer2_out;  // 32 × 32 = 1,024
    size_t layer2_adds = layer2_mults + layer2_out;
    std::cout << "   Layer 2 (32→32): " << layer2_mults << " multiplications, " << layer2_adds
              << " additions" << std::endl;

    // Layer 3: 32 → 1
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

    // Summary
    std::cout << "\n4. Summary per evaluation:" << std::endl;
    std::cout << "   Transform: ~" << transformOps << " operations (incremental helps here)"
              << std::endl;
    std::cout << "   Affine layers: ~" << (total_mults + total_adds)
              << " operations (incremental doesn't help)" << std::endl;
    std::cout << "   Total operations: ~" << (transformOps + total_mults + total_adds)
              << " arithmetic operations" << std::endl;

    // Performance analysis
    std::cout << "\n5. Performance perspective:" << std::endl;

    // Use actual eval rate if we have timing data, otherwise default to 100K
    double evalRate = 100000.0;  // Default 100K eval/sec
    if (g_totalEvaluations > 0) {
        uint64_t totalTime = g_transformTimeNanos + g_perspectiveTimeNanos + g_affineTimeNanos +
            g_clippedReluTimeNanos;
        if (totalTime > 0) {
            evalRate = 1e9 * g_totalEvaluations / totalTime;
        }
    }

    std::cout << "   At ~" << static_cast<int>(evalRate) << " eval/sec: ~"
              << (transformOps + total_mults + total_adds) * evalRate / 1000000.0
              << " million ops/sec" << std::endl;

    // Use timing percentages instead of operation percentages
    if (g_totalEvaluations > 0) {
        uint64_t totalTime = g_transformTimeNanos + g_perspectiveTimeNanos + g_affineTimeNanos +
            g_clippedReluTimeNanos;
        if (totalTime > 0) {
            double transformPercent = 100.0 * g_transformTimeNanos / totalTime;
            double networkPropagationPercent = 100.0 *
                (g_perspectiveTimeNanos + g_affineTimeNanos + g_clippedReluTimeNanos) / totalTime;
            std::cout << "   Transform takes " << formatPercent(transformPercent)
                      << " of evaluation time" << std::endl;
            std::cout << "   Incremental evaluation can eliminate the transform cost," << std::endl;
            std::cout << "   but network propagation (" << formatPercent(networkPropagationPercent)
                      << " of time) still needs optimization" << std::endl;
        } else {
            std::cout << "   Transform is only "
                      << formatPercent(100.0 * transformOps /
                                       (transformOps + total_mults + total_adds))
                      << " of total operations" << std::endl;
            std::cout << "   Incremental evaluation can eliminate the transform cost," << std::endl;
            std::cout << "   but network propagation ("
                      << formatPercent(100.0 * (total_mults + total_adds) /
                                       (transformOps + total_mults + total_adds))
                      << " of ops) still needs optimization" << std::endl;
        }
    } else {
        std::cout << "   Transform is only "
                  << formatPercent(100.0 * transformOps / (transformOps + total_mults + total_adds))
                  << " of total operations" << std::endl;
        std::cout << "   Incremental evaluation can eliminate the transform cost," << std::endl;
        std::cout << "   but network propagation ("
                  << formatPercent(100.0 * (total_mults + total_adds) /
                                   (transformOps + total_mults + total_adds))
                  << " of ops) still needs optimization" << std::endl;
    }

    std::cout << formatHeader("") << "\n" << std::endl;
}

}  // namespace nnue
#include "eval/nnue/nnue.h"
#include "core/core.h"
#include "eval/nnue/nnue_stats.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "core/square_set/square_set.h"

namespace nnue {

namespace {

// Activation function constant
constexpr int kWeightScaleBits = 6;

/** Update timing point and return elapsed nanoseconds */
uint64_t updateTiming(std::chrono::high_resolution_clock::time_point& timePoint) {
    using namespace std::chrono;
    auto nextTimePoint = high_resolution_clock::now();
    uint64_t elapsed = duration_cast<nanoseconds>(nextTimePoint - timePoint).count();
    timePoint = nextTimePoint;
    return elapsed;
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

template <typename Layer>
void readAffineTransform(std::ifstream& file, Layer& layer) {
    readBinaryVector(file, layer.bias);
    // Read weights as a flat array and cast to 2D structure
    file.read(reinterpret_cast<char*>(layer.weights.data()),
              Layer::kWeightDimensions * sizeof(typename Layer::Weight));
    if (!file) throw std::runtime_error("Unexpected EOF");
}

void readNetwork(std::ifstream& file, Network& net) {
    checkHash(Network::kHash, readBinary<uint32_t>(file), "Network");

    readAffineTransform(file, net.layer0);
    readAffineTransform(file, net.layer1);
    readAffineTransform(file, net.layer2);
}

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
    constexpr Feature operator+(const Feature& other) const {
        return Feature(whiteIndex + other.whiteIndex, blackIndex + other.blackIndex);
    }
    constexpr Feature operator*(const Feature& other) const {  // For deriving king features
        return Feature(whiteIndex * other.whiteIndex, blackIndex * other.blackIndex);
    }
    constexpr bool operator==(const Feature& other) const {
        return whiteIndex == other.whiteIndex && blackIndex == other.blackIndex;
    }
};

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
uint8_t activate(int16_t value) {
    // Stockfish uses max(0, min(127, value)) for accumulator -> uint8_t conversion
    return static_cast<uint8_t>(std::clamp(value, int16_t(0), int16_t(127)));
}

uint8_t activate(int32_t value) {
    return static_cast<uint8_t>(std::clamp(value >> kWeightScaleBits, 0, 127));
}

/**
 * Select perspective from accumulator based on side to move.
 * Returns 512 values from either white perspective or black perspective.
 */
auto selectPerspective(const Accumulator& accumulator, Color sideToMove) {
    typename Network::Layer0::Input perspective;

    auto in1 = accumulator.values.begin();
    auto in2 = accumulator.values.begin() + InputTransform::kHalfDimensions;
    auto out = perspective.begin();

    // Swap perspectives if black to move, so the first half is for us and second half for them
    if (sideToMove == Color::b) std::swap(in1, in2);

    for (size_t i = 0; i < InputTransform::kHalfDimensions; ++i) *out++ = activate(*in1++);
    for (size_t i = 0; i < InputTransform::kHalfDimensions; ++i) *out++ = activate(*in2++);

    return perspective;
}

/**
 * Compute the inner product of two vectors. Returns the sum of element-wise products as int32_t.
 */
template <typename V, typename U>
int32_t innerProduct(const V& v, const U& u) {
    int32_t sum = 0;

    for (size_t i = 0; i < v.size(); ++i) sum += v[i] * u[i];

    return sum;
}

/**
 * Apply affine transformation: output = weights * input + bias
 * Returns int32_t values without activation.
 */
template <typename Layer>
auto affineForward(const typename Layer::Input& input, const Layer& layer) {
    typename Layer::Output output;

    for (size_t i = 0; i < Layer::kOutputSize; ++i)
        output[i] = innerProduct(input, layer.weights[i]) + layer.bias[i];

    return output;
}

/**
 * Apply affine transformation with activation: output = activate(weights * input + bias)
 * Returns values after clipped ReLU activation.
 */
template <typename Layer>
auto affineForwardAndActivate(const typename Layer::Input& input, const Layer& layer) {
    typename Layer::Clipped output;

    for (size_t i = 0; i < Layer::kOutputSize; ++i)
        output[i] = activate(innerProduct(input, layer.weights[i]) + layer.bias[i]);

    return output;
}
}  // namespace

Accumulator::Accumulator(const InputTransform& inputTransform) {
    auto out1 = values.begin(), out2 = values.begin() + InputTransform::kHalfDimensions;

    for (auto bias : inputTransform.bias) *out1++ = *out2++ = bias;
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

Accumulator transform(const Position& position, const InputTransform& inputTransform) {
    Accumulator accumulator(inputTransform);

    auto activeFeatures = extractActiveFeatures(position);
    g_totalActiveFeatures += activeFeatures.size();  // Track feature count
    for (auto feature : activeFeatures) addFeature(accumulator, inputTransform, feature);

    return accumulator;
}

int32_t evaluate(const Position& position, const NNUE& nnue) {
    using namespace std::chrono;
    const Network& network = nnue.network;

    ++g_totalEvaluations;

    // Step 1: Get accumulator from input transform
    auto timePoint = high_resolution_clock::now();
    Accumulator accumulator = transform(position, nnue.input);
    g_transformTimeNanos += updateTiming(timePoint);

    // Step 2: Select perspective first while applying clippedReLU
    typename Network::Layer0::Input input0 = selectPerspective(accumulator, position.active());
    g_perspectiveTimeNanos += updateTiming(timePoint);

    // Step 3: Process through each affine layer with ClippedReLU activations
    auto input1 = affineForwardAndActivate(input0, network.layer0);
    auto input2 = affineForwardAndActivate(input1, network.layer1);
    auto output = affineForward(input2, network.layer2);
    g_affineTimeNanos += updateTiming(timePoint);

    if constexpr (nnueDebug) std::cout << "Final layer output: " << output[0] << std::endl;

    double kScale = 0.0300682;  // Scale factor for centipawns closely approximating Stockfish 12
    return output[0] * (position.active() == Color::b ? -kScale : kScale);
}

}  // namespace nnue
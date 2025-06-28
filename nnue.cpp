#include "common.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "moves.h"
#include "nnue.h"

namespace nnue {

namespace {
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
    // Step 1: Get accumulator from input transform
    Accumulator accumulator = transform(position, network.input);

    // Step 2: Select perspective first, then apply ClippedReLU
    std::vector<int16_t> perspective = selectPerspective(accumulator, position.active());
    std::vector<uint8_t> layerInput = applyClippedReLU(perspective, 0);

    // Step 3: Process through each affine layer with ClippedReLU activations
    std::vector<int32_t> layerOutput;
    for (size_t i = 0; i < network.network.layers.size(); ++i) {
        const auto& layer = network.network.layers[i];

        // Apply affine transformation
        layerOutput = affineForward(layerInput, layer);

        if (i == network.network.layers.size()) break;

        // Apply ClippedReLU for hidden layers (not the final output layer)
        std::vector<int16_t> int16Output;
        int16Output.reserve(layerOutput.size());
        for (int32_t value : layerOutput) {
            int16Output.push_back(static_cast<int16_t>(
                std::clamp(value,
                           static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
                           static_cast<int32_t>(std::numeric_limits<int16_t>::max()))));
        }
        layerInput = applyClippedReLU(int16Output, 6);
    }
    if (debug) std::cout << "Final layer output: " << layerOutput[0] << std::endl;

    double kScale = 0.030044;  // Scale factor for centipawns
    return layerOutput[0] * (position.active() == Color::b ? -kScale : kScale);
}

}  // namespace nnue
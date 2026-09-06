#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "core/core.h"
#include "engine/fen/fen.h"
#include "eval/eval.h"
#include "eval/nnue/nnue.h"
#include "move/move.h"
#include "move/move_gen.h"

// A network is far too large to copy by accident, so it may only be moved.
static_assert(!std::is_copy_constructible_v<nnue::Network>);
static_assert(!std::is_copy_assignable_v<nnue::Network>);
static_assert(std::is_move_constructible_v<nnue::Network>);

namespace {

/** The default Big network of Stockfish 16.1, used when no file name is given. */
const std::string kBigNetworkFile = "nn-b1a57edbea57.nnue";

/** Serialize a header as it appears in a file: three little endian words plus the description. */
std::string makeHeader(uint32_t version, uint32_t hash, uint32_t length, const std::string& desc) {
    std::string bytes;
    for (uint32_t word : {version, hash, length})
        for (int i = 0; i < 4; ++i) bytes += char(word >> (8 * i));

    return bytes + desc;
}

/** A well formed header for the Big network, from which the negative cases are derived. */
std::string goodHeader(const std::string& desc = "test network") {
    return makeHeader(
        nnue::FileHeader::kVersion, nnue::kBigArchitecture.hash(), desc.size(), desc);
}

/** Check that reading the given bytes is rejected, and report the diagnostic it produced. */
void expectRejected(const std::string& what, const std::string& bytes) {
    std::istringstream in(bytes);
    try {
        nnue::readFileHeader(in);
    } catch (const std::runtime_error& e) {
        std::cout << "Rejected " << what << ": " << e.what() << "\n";
        return;
    }
    std::cerr << "Accepted " << what << ", but it should have been rejected\n";
    assert(false && "malformed header accepted");
}

void testSyntheticHeader() {
    std::istringstream in(goodHeader());
    auto header = nnue::readFileHeader(in);
    assert(header.version == nnue::FileHeader::kVersion);
    assert(header.hash == nnue::kBigArchitecture.hash());
    assert(header.description == "test network");

    // Nothing beyond the header is consumed, so the rest of the file is left for the caller.
    std::istringstream trailing(goodHeader() + "parameters");
    nnue::readFileHeader(trailing);
    std::string rest;
    trailing >> rest;
    assert(rest == "parameters");

    std::cout << "Synthetic header accepted and fully consumed\n";
}

void testMalformedHeaders() {
    auto good = goodHeader();

    expectRejected("empty input", "");
    expectRejected("truncated version", good.substr(0, 2));
    expectRejected("truncated architecture hash", good.substr(0, 6));
    expectRejected("truncated description length", good.substr(0, 10));
    expectRejected("truncated description", good.substr(0, good.size() - 1));

    auto desc = std::string("test network");
    auto hash = nnue::kBigArchitecture.hash();
    expectRejected("Stockfish 12 version", makeHeader(0x7af32f16u, hash, desc.size(), desc));
    expectRejected("byte swapped version",
                   makeHeader(0x202ff37au, hash, desc.size(), desc));  // catches endianness slips
    expectRejected("wrong architecture hash",
                   makeHeader(nnue::FileHeader::kVersion, hash ^ 1, desc.size(), desc));
    expectRejected("differently sized accumulator",
                   makeHeader(nnue::FileHeader::kVersion,
                              nnue::Architecture{1024, 15, 32}.hash(),
                              desc.size(),
                              desc));
    expectRejected("empty description", makeHeader(nnue::FileHeader::kVersion, hash, 0, ""));
    expectRejected("absurd description length",
                   makeHeader(nnue::FileHeader::kVersion, hash, ~0u, desc));
    expectRejected("binary description",
                   makeHeader(nnue::FileHeader::kVersion, hash, 4, std::string("a\0bc", 4)));
}

/**
 * Encode one value as signed LEB128, independently of the decoder under test: emit seven bits at
 * a time, least significant group first, stopping once the remaining bits are all sign bits and
 * the group carries the sign in its top bit.
 */
template <typename Int>
void encodeLeb128(std::string& out, Int value) {
    for (;;) {
        auto group = uint8_t(value & 0x7f);
        value >>= 7;  // arithmetic, so a negative value converges to -1
        bool last = (value == 0 && !(group & 0x40)) || (value == -1 && (group & 0x40));
        out += char(last ? group : group | 0x80);
        if (last) return;
    }
}

/** Wrap encoded values in the block framing the format uses: magic string and byte count. */
std::string leb128Block(const std::string& encoded) {
    std::string bytes(nnue::kLeb128Magic);
    for (int i = 0; i < 4; ++i) bytes += char(uint32_t(encoded.size()) >> (8 * i));

    return bytes + encoded;
}

template <typename Int>
std::string leb128Block(const std::vector<Int>& values) {
    std::string encoded;
    for (auto value : values) encodeLeb128(encoded, value);

    return leb128Block(encoded);
}

/** Decode a block of `count` values, returning them, and check the stream ends up at its end. */
template <typename Int>
std::vector<Int> decodeAll(const std::string& block, size_t count) {
    std::istringstream in(block);
    std::vector<Int> values(count);
    nnue::readLeb128(in, values.data(), count);
    assert(in.peek() == std::istringstream::traits_type::eof());

    return values;
}

/** Check that decoding `count` values from the given bytes fails. */
template <typename Int>
void expectBlockRejected(const std::string& what, const std::string& block, size_t count) {
    std::istringstream in(block);
    std::vector<Int> values(count + 1);  // a slot to spare, so an overrun corrupts nothing
    try {
        nnue::readLeb128(in, values.data(), count);
    } catch (const std::runtime_error& e) {
        std::cout << "Rejected " << what << ": " << e.what() << "\n";
        return;
    }
    std::cerr << "Accepted " << what << ", but it should have been rejected\n";
    assert(false && "malformed LEB128 block accepted");
}

void testLeb128() {
    // Encodings written out by hand from the format definition, covering the boundaries where a
    // value needs another byte and where the sign bit of the last group must be extended.
    const struct {
        int32_t value;
        const char* bytes;
        size_t size;
    } kVectors[] = {
        {0, "\x00", 1},
        {1, "\x01", 1},
        {-1, "\x7f", 1},
        {63, "\x3f", 1},                        // largest single byte value
        {-64, "\x40", 1},                       // smallest single byte value, sign extended
        {64, "\xc0\x00", 2},                    // needs a second byte to stay positive
        {-65, "\xbf\x7f", 2},                   //
        {127, "\xff\x00", 2},                   //
        {-128, "\x80\x7f", 2},                  //
        {8191, "\xff\x3f", 2},                  // largest two byte value
        {-8192, "\x80\x40", 2},                 // smallest two byte value
        {32767, "\xff\xff\x01", 3},             // INT16_MAX, sign lives outside the last group
        {-32768, "\x80\x80\x7e", 3},            // INT16_MIN
        {-1000000, "\xc0\xfb\x42", 3},          //
        {2147483647, "\xff\xff\xff\xff\x07", 5},  // INT32_MAX
        {-2147483648, "\x80\x80\x80\x80\x78", 5},  // INT32_MIN
    };

    for (auto& vector : kVectors) {
        std::string expected(vector.bytes, vector.size);
        std::string encoded;
        encodeLeb128(encoded, vector.value);
        assert(encoded == expected && "encoder disagrees with the hand written encoding");

        auto asInt32 = decodeAll<int32_t>(leb128Block(expected), 1);
        assert(asInt32[0] == vector.value);

        // The same bytes decode to the same value as an int16_t where one fits, which exercises
        // the sign extension against a different word size.
        if (vector.value < -32768 || vector.value > 32767) continue;
        auto asInt16 = decodeAll<int16_t>(leb128Block(expected), 1);
        assert(asInt16[0] == int16_t(vector.value));
    }

    // A block holds many values back to back, with no framing of its own between them.
    std::vector<int16_t> values;
    for (int i = -600; i <= 600; ++i) values.push_back(int16_t(i * 53));
    assert(decodeAll<int16_t>(leb128Block(values), values.size()) == values);

    // Values are also decoded correctly when one straddles the decoder's read buffer, which the
    // long run above is far too short to reach.
    std::vector<int32_t> wide(20000, -123456789);
    assert(decodeAll<int32_t>(leb128Block(wide), wide.size()) == wide);

    auto block = leb128Block(values);
    expectBlockRejected<int16_t>("uncompressed block", "0123456789ABCDEFG", values.size());
    expectBlockRejected<int16_t>("truncated marker", block.substr(0, 5), values.size());
    expectBlockRejected<int16_t>("truncated block size", block.substr(0, 19), values.size());
    expectBlockRejected<int16_t>("truncated block body", block.substr(0, block.size() - 3),
                                 values.size());
    expectBlockRejected<int16_t>("block with too few values", block, values.size() + 1);
    expectBlockRejected<int16_t>("block with too many values", block, values.size() - 1);
    expectBlockRejected<int16_t>("value that never ends", leb128Block(std::string(8, '\xff')), 1);
    expectBlockRejected<int16_t>("value too wide for its type",
                                 leb128Block(std::string("\xff\xff\xff\x00", 4)), 1);

    std::cout << "Signed LEB128 decoder matches its hand written test vectors\n";
}

/** A tiny architecture, sharing everything but its size with the real one, for synthetic files. */
constexpr nnue::Architecture kTinyArchitecture = {32, 15, 32, 64};

/** Values that are cheap to reproduce and vary enough to catch a swapped or shifted array. */
int32_t pseudoRandom(size_t index, int32_t range) {
    return int32_t((index * 2654435761u >> 11) % uint32_t(2 * range + 1)) - range;
}

template <typename Int>
std::vector<Int> pseudoRandomValues(size_t count, size_t seed, int32_t range) {
    std::vector<Int> values(count);
    for (size_t i = 0; i < count; ++i) values[i] = Int(pseudoRandom(i + seed, range));

    return values;
}

/** Serialize values as an uncompressed little endian array, as the layer stacks store them. */
template <typename Int>
std::string rawArray(const std::vector<Int>& values) {
    std::string bytes;
    for (auto value : values)
        for (size_t i = 0; i < sizeof(Int); ++i) bytes += char(uint8_t(value >> (8 * i)));

    return bytes;
}

/** A little endian uint32, used for the structure hashes between parameter blocks. */
std::string rawUint32(uint32_t value) {
    return rawArray(std::vector<int32_t>{int32_t(value)});
}

/**
 * A complete synthetic network file for kTinyArchitecture, along with the parameters that went
 * into it, so that a decoded network can be compared against what was written.
 */
struct SyntheticNetwork {
    std::string bytes;
    std::vector<int16_t> transformerBiases, transformerWeights;
    std::vector<int32_t> psqtWeights;
    std::vector<int32_t> fc0Biases;  // of the first layer stack only, the rest vary by index
    std::vector<int8_t> fc2Weights;  // of the last layer stack only
    size_t transformerHashOffset = 0;
    std::vector<size_t> stackHashOffsets;
};

SyntheticNetwork makeSyntheticNetwork() {
    const auto& arch = kTinyArchitecture;
    SyntheticNetwork net;

    std::string desc = "synthetic network";
    net.bytes = makeHeader(nnue::FileHeader::kVersion, arch.hash(), desc.size(), desc);

    net.transformerBiases = pseudoRandomValues<int16_t>(arch.l1, 1, 1000);
    net.transformerWeights = pseudoRandomValues<int16_t>(arch.inputDimensions * arch.l1, 2, 1500);
    net.psqtWeights =
        pseudoRandomValues<int32_t>(arch.inputDimensions * arch.kPSQTBuckets, 3, 60000);

    net.transformerHashOffset = net.bytes.size();
    net.bytes += rawUint32(arch.featureTransformerHash());
    net.bytes += leb128Block(net.transformerBiases);
    net.bytes += leb128Block(net.transformerWeights);
    net.bytes += leb128Block(net.psqtWeights);

    for (uint32_t stack = 0; stack < arch.kLayerStacks; ++stack) {
        auto biases = [&](uint32_t count, size_t seed) {
            return pseudoRandomValues<int32_t>(count, seed + 100 * stack, 4000);
        };
        auto weights = [&](uint32_t count, size_t seed) {
            return pseudoRandomValues<int8_t>(count, seed + 100 * stack, 127);
        };

        auto fc0Biases = biases(arch.l2 + 1, 4);
        auto fc2Weights = weights(arch.l3, 9);
        if (!stack) net.fc0Biases = fc0Biases;
        if (stack + 1 == arch.kLayerStacks) net.fc2Weights = fc2Weights;

        net.stackHashOffsets.push_back(net.bytes.size());
        net.bytes += rawUint32(arch.networkHash());
        net.bytes += rawArray(fc0Biases);
        net.bytes += rawArray(weights((arch.l2 + 1) * arch.padded(arch.l1), 5));
        net.bytes += rawArray(biases(arch.l3, 6));
        net.bytes += rawArray(weights(arch.l3 * arch.padded(2 * arch.l2), 7));
        net.bytes += rawArray(biases(1, 8));
        net.bytes += rawArray(fc2Weights);
    }

    return net;
}

/** Check the shape of a layer, which the reader derives from the architecture rather than reads. */
void checkLayerShape(const nnue::AffineLayer& layer, uint32_t inputs, uint32_t outputs) {
    assert(layer.inputs == inputs);
    assert(layer.paddedInputs == nnue::Architecture::padded(inputs));
    assert(layer.outputs == outputs);
    assert(layer.biases.size() == outputs);
    assert(layer.weights.size() == size_t(outputs) * layer.paddedInputs);
}

/** Check that every parameter array of a network has exactly the size its architecture implies. */
void checkNetworkShape(const nnue::Network& network) {
    const auto& arch = network.arch;
    assert(network.transformer.biases.size() == arch.l1);
    assert(network.transformer.weights.size() == size_t(arch.inputDimensions) * arch.l1);
    assert(network.transformer.psqtWeights.size() ==
           size_t(arch.inputDimensions) * arch.kPSQTBuckets);
    assert(network.stacks.size() == arch.kLayerStacks);

    for (const auto& stack : network.stacks) {
        checkLayerShape(stack.fc0, arch.l1, arch.l2 + 1);
        checkLayerShape(stack.fc1, 2 * arch.l2, arch.l3);
        checkLayerShape(stack.fc2, arch.l3, 1);
    }
}

/** Check that reading a synthetic network from the given bytes fails. */
void expectNetworkRejected(const std::string& what, const std::string& bytes) {
    std::istringstream in(bytes);
    try {
        nnue::readNetwork(in, kTinyArchitecture);
    } catch (const std::runtime_error& e) {
        std::cout << "Rejected " << what << ": " << e.what() << "\n";
        return;
    }
    std::cerr << "Accepted " << what << ", but it should have been rejected\n";
    assert(false && "malformed network accepted");
}

void testSyntheticNetwork() {
    auto net = makeSyntheticNetwork();

    std::istringstream in(net.bytes);
    auto network = nnue::readNetwork(in, kTinyArchitecture);
    assert(network.header.description == "synthetic network");
    checkNetworkShape(network);

    // Every parameter comes back exactly as written, in the order it was written.
    assert(network.transformer.biases == net.transformerBiases);
    assert(network.transformer.weights == net.transformerWeights);
    assert(network.transformer.psqtWeights == net.psqtWeights);
    assert(network.stacks.front().fc0.biases == net.fc0Biases);
    assert(network.stacks.back().fc2.weights == net.fc2Weights);

    // Weight rows are laid out one output at a time, which the accessor and the flat array agree
    // on only if nothing was scrambled while reading.
    const auto& fc1 = network.stacks[3].fc1;
    for (uint32_t i = 0; i < fc1.outputs; ++i)
        for (uint32_t j = 0; j < fc1.paddedInputs; ++j)
            assert(fc1.weight(i, j) == fc1.weights[i * fc1.paddedInputs + j]);

    expectNetworkRejected("network with trailing bytes", net.bytes + '\0');
    expectNetworkRejected("network truncated mid transformer", net.bytes.substr(0, 200));
    expectNetworkRejected("network truncated mid stack", net.bytes.substr(0, net.bytes.size() - 1));
    expectNetworkRejected("network missing its last stack",
                          net.bytes.substr(0, net.stackHashOffsets.back()));

    // The structure hashes guard the two kinds of parameter block against a topology mismatch.
    auto corrupt = [&](size_t offset, const std::string& what) {
        auto bytes = net.bytes;
        bytes[offset] ^= 1;
        expectNetworkRejected(what, bytes);
    };
    corrupt(net.transformerHashOffset, "wrong transformer hash");
    corrupt(net.stackHashOffsets.front(), "wrong hash on the first stack");
    corrupt(net.stackHashOffsets.back(), "wrong hash on the last stack");

    std::cout << "Synthetic network round trips through the reader\n";
}

/** Sum of an array, wide enough to hold the total of every parameter of the Big network. */
template <typename Int>
int64_t sum(const std::vector<Int>& values) {
    return std::accumulate(values.begin(), values.end(), int64_t(0));
}

/** Defined with the transform tests below, but only a real network file can drive it. */
void testGoldenTransforms(const nnue::Network& network);
void testGoldenPropagations(const nnue::Network& network);
void testGoldenEvaluations(const nnue::Network& network);
void testColorSymmetry(const nnue::Network& network);
void testEvaluationClamp(const nnue::Network& network);
void testIncrementalAccumulators(const nnue::Network& network);
void testSparseAffineForward(const nnue::Network& network);

void testNetworkFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open NNUE file: " + filename);
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0);

    auto network = nnue::readNetwork(file);
    std::cout << "Read " << filename << ": " << network.header.description << "\n";

    size_t parameters = network.transformer.biases.size() + network.transformer.weights.size() +
                        network.transformer.psqtWeights.size();
    for (const auto& stack : network.stacks)
        for (const auto* layer : {&stack.fc0, &stack.fc1, &stack.fc2})
            parameters += layer->biases.size() + layer->weights.size();

    std::cout << "  " << parameters << " parameters from " << fileSize << " bytes, ending at EOF\n";
    assert(parameters == 58190984);  // the whole Big network, transformer and all eight stacks

    assert(network.header.version == nnue::FileHeader::kVersion);
    assert(network.header.hash == 0x1c103072u);  // published hash of the Stockfish 16.1 Big network
    assert(network.header.description.find("nnue-pytorch") != std::string::npos);
    checkNetworkShape(network);

    // Checksums over the transformer, produced by an independent decoding of the file rather than
    // by this reader, so that a systematic misreading shows up here.
    assert(sum(network.transformer.biases) == -37537);
    assert(sum(network.transformer.weights) == -135417008);
    assert(sum(network.transformer.psqtWeights) == 31113357);
    assert(network.transformer.biases.front() == -99 && network.transformer.biases.back() == 217);
    assert(network.transformer.weights.back() == -289);
    assert(network.transformer.psqtWeights.front() == 2016);
    assert(network.transformer.psqtWeights.back() == -136);

    // Likewise for each layer stack, whose parameters are stored uncompressed.
    const int64_t kFc0BiasSums[] = {1979, 19746, 8351, -7589, -11612, -11464, -865, -12607};
    const int64_t kFc0WeightSums[] = {32846, 11011, 12808, 9967, 17726, 12622, 14841, 27351};
    const int64_t kFc1BiasSums[] = {5144, -39147, 10541, -20178, -14700, -22328, -18653, -30911};
    const int64_t kFc1WeightSums[] = {-79, 266, -1150, -89, -271, -563, -1906, -1717};
    const int64_t kFc2Biases[] = {1857, 591, 290, 1730, 1167, -258, 1920, 1661};
    const int64_t kFc2WeightSums[] = {24, -109, -50, -151, 36, -74, -186, -207};

    for (size_t i = 0; i < network.stacks.size(); ++i) {
        const auto& stack = network.stacks[i];
        assert(sum(stack.fc0.biases) == kFc0BiasSums[i]);
        assert(sum(stack.fc0.weights) == kFc0WeightSums[i]);
        assert(sum(stack.fc1.biases) == kFc1BiasSums[i]);
        assert(sum(stack.fc1.weights) == kFc1WeightSums[i]);
        assert(stack.fc2.biases.front() == kFc2Biases[i]);
        assert(sum(stack.fc2.weights) == kFc2WeightSums[i]);
    }

    // The two columns padding each fc1 row hold no weights and are zero throughout, except for a
    // single pair at the very end of the file, which the trainer evidently left uninitialized.
    // Reading them at all is what proves we take the file's padded row stride seriously.
    for (size_t i = 0; i < network.stacks.size(); ++i) {
        const auto& fc1 = network.stacks[i].fc1;
        for (uint32_t row = 0; row < fc1.outputs; ++row)
            for (uint32_t column = fc1.inputs; column < fc1.paddedInputs; ++column) {
                bool last = i + 1 == network.stacks.size() && row + 1 == fc1.outputs;
                assert(fc1.weight(row, column) == 0 || last);
            }
    }
    assert(network.stacks.back().fc1.weight(31, 30) == 64);
    assert(network.stacks.back().fc1.weight(31, 31) == 28);

    // With a real transformer in hand, check it against Stockfish 16.1 itself.
    testGoldenTransforms(network);
    testGoldenPropagations(network);
    testSparseAffineForward(network);
    testGoldenEvaluations(network);
    testColorSymmetry(network);
    testEvaluationClamp(network);
    testIncrementalAccumulators(network);

    // The header alone still has to be rejected when cut short, as before.
    file.clear();
    file.seekg(0);
    std::string prefix(network.header.description.size() + 11, '\0');
    file.read(prefix.data(), prefix.size());
    assert(file && "network file is too short to hold its own header");
    expectRejected("truncated " + filename, prefix);
}

// ---------------------------------------------------------------------------------------------
// HalfKAv2_hm features
// ---------------------------------------------------------------------------------------------

using nnue::activeFeatures;
using nnue::featureIndex;
using nnue::kBucketStride;
using nnue::kKingBuckets;
using nnue::kPieceCategories;

/** Feature indices of a position, sorted, so that tests can compare them as sets. */
std::vector<uint16_t> featuresOf(const std::string& fen, Color perspective) {
    auto features = activeFeatures(fen::parsePosition(fen), perspective);
    std::vector<uint16_t> sorted(features.begin(), features.end());
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

/** The bucket a king lands in, being the part of its own feature index above the bucket stride. */
uint16_t bucketOf(Square king, Color perspective) {
    return featureIndex(king, addColor(PieceType::KING, perspective), king, perspective) /
           kBucketStride;
}

/** The piece category of a feature index, between the king bucket and the oriented square. */
uint16_t categoryOf(Piece piece, Color perspective) {
    // Put the king on e1 or e8, which needs no mirroring, and the piece on the square that then
    // orients to a1, so that the square contributes nothing to the index.
    auto king = perspective == Color::w ? e1 : e8;
    auto square = perspective == Color::w ? a1 : a8;
    return featureIndex(square, piece, king, perspective) % kBucketStride / kNumSquares;
}

/** A king that has not moved is in the last bucket, where the hand computed cases below live. */
constexpr uint16_t kHomeBucket = 31 * kBucketStride;             // 21824
constexpr uint16_t kKingCategory = kHomeBucket + 10 * kNumSquares;  // kings of either color

/** Kings are active features in HalfKAv2_hm, unlike in the HalfKP feature set of Stockfish 12. */
void testKingsOnly() {
    // White king on e1 and black king on e8: neither side mirrors, and both are in bucket 31.
    const std::string kings = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";

    auto white = featuresOf(kings, Color::w);
    assert(white.size() == 2 && "a bare king and king position still has two features");
    assert(white[0] == kKingCategory + e1);
    assert(white[1] == kKingCategory + e8);

    // Black sees the board upside down, so its own king is again on e1 and white's on e8: the
    // position is symmetric and the two perspectives agree feature for feature.
    assert(featuresOf(kings, Color::b) == white);
}

/** One pawn beside the kings, checked index by index from the feature set's definition. */
void testPawnAndKings() {
    const std::string position = "4k3/8/8/8/8/8/4P3/4K3 w - - 0 1";  // white pawn on e2

    // White's own pawns are category 0, and nothing is mirrored, so the pawn keeps square e2.
    assert(featuresOf(position, Color::w) ==
           (std::vector<uint16_t>{uint16_t(kHomeBucket + 0 * kNumSquares + e2),
                                  uint16_t(kKingCategory + e1),
                                  uint16_t(kKingCategory + e8)}));

    // To black the same pawn is an enemy pawn, category 1, and the rank flip moves it to e7.
    assert(featuresOf(position, Color::b) ==
           (std::vector<uint16_t>{uint16_t(kHomeBucket + 1 * kNumSquares + e7),
                                  uint16_t(kKingCategory + e1),
                                  uint16_t(kKingCategory + e8)}));
}

/** A fuller hand computed case, exercising both the orientation and the friend/enemy mapping. */
void testHandComputedIndices() {
    const std::string position = "3qk3/8/8/8/8/8/4P3/4K3 w - - 0 1";  // adds a black queen on d8

    // From white the queen is an enemy queen, category 9, on the square it stands on.
    assert(featuresOf(position, Color::w) ==
           (std::vector<uint16_t>{uint16_t(kHomeBucket + 0 * kNumSquares + e2),
                                  uint16_t(kHomeBucket + 9 * kNumSquares + d8),
                                  uint16_t(kKingCategory + e1),
                                  uint16_t(kKingCategory + e8)}));

    // From black it is its own queen, category 8, and the rank flip moves d8 to d1.
    assert(featuresOf(position, Color::b) ==
           (std::vector<uint16_t>{uint16_t(kHomeBucket + 1 * kNumSquares + e7),
                                  uint16_t(kHomeBucket + 8 * kNumSquares + d1),
                                  uint16_t(kKingCategory + e1),
                                  uint16_t(kKingCategory + e8)}));
}

/** Categories pair up by piece type, the perspective side's own first, with kings sharing one. */
void testPieceCategories() {
    for (auto pieceType : {PieceType::PAWN,
                           PieceType::KNIGHT,
                           PieceType::BISHOP,
                           PieceType::ROOK,
                           PieceType::QUEEN}) {
        auto white = addColor(pieceType, Color::w), black = addColor(pieceType, Color::b);
        assert(categoryOf(white, Color::w) == 2 * index(pieceType));
        assert(categoryOf(black, Color::w) == 2 * index(pieceType) + 1);

        // Ownership is relative to the perspective, so from black the two swap.
        assert(categoryOf(black, Color::b) == 2 * index(pieceType));
        assert(categoryOf(white, Color::b) == 2 * index(pieceType) + 1);
    }

    // Both kings fall in the last category, whichever side is looking.
    for (auto perspective : {Color::w, Color::b})
        for (auto king : {Piece::K, Piece::k})
            assert(categoryOf(king, perspective) == kPieceCategories - 1);
}

/** The horizontal mirroring, which is what halves 64 king squares to 32 buckets. */
void testKingBuckets() {
    std::set<uint16_t> whiteBuckets;
    for (Square king : squares) {
        whiteBuckets.insert(bucketOf(king, Color::w));
        assert(bucketOf(king, Color::w) < kKingBuckets);

        // A king on files a to d shares its bucket with its mirror image on files h to e.
        assert(bucketOf(king, Color::w) == bucketOf(Square(king ^ 7), Color::w));
        assert(bucketOf(king, Color::b) == bucketOf(Square(king ^ 7), Color::b));

        // Black's board is upside down, so its buckets are white's with the ranks flipped.
        assert(bucketOf(king, Color::b) == bucketOf(Square(king ^ 56), Color::w));
    }
    assert(whiteBuckets.size() == kKingBuckets && "every bucket is reachable");

    // Spot checks either side of the mirroring boundary, on the layout the network is trained in:
    // buckets run four to a rank, from 0 at h8 down to 31 at e1.
    assert(bucketOf(h8, Color::w) == 0);
    assert(bucketOf(e8, Color::w) == 3);
    assert(bucketOf(h1, Color::w) == 28);
    assert(bucketOf(e1, Color::w) == 31);
    assert(bucketOf(a1, Color::w) == 28 && "a1 mirrors onto h1");
    assert(bucketOf(d1, Color::w) == 31 && "d1 mirrors onto e1");

    // Black's unmoved king matches white's, on either side of the boundary.
    assert(bucketOf(e8, Color::b) == 31);
    assert(bucketOf(d8, Color::b) == 31);
    assert(bucketOf(a8, Color::b) == 28);
    assert(bucketOf(h8, Color::b) == 28);
}

/** Mirror a board horizontally, which the feature set is built not to distinguish. */
Position mirrored(const Position& position) {
    Position result = position;
    for (Square square : squares) result.board[Square(square ^ 7)] = position.board[square];
    return result;
}

/** A position and its mirror image set exactly the same features, from either perspective. */
void testMirrorInvariance() {
    for (const auto* fen : {"3k4/8/8/8/8/8/8/R2K4 w - - 0 1",     // white king left of the boundary
                            "8/2p5/8/8/8/8/5P2/2K2k2 w - - 0 1",  // both kings left of it
                            "7k/8/8/8/8/8/8/7K w - - 0 1",        // both on the h file
                            fen::initialPosition}) {
        auto position = fen::parsePosition(fen);
        for (auto perspective : {Color::w, Color::b}) {
            auto features = activeFeatures(position, perspective);
            auto image = activeFeatures(mirrored(position), perspective);
            std::vector<uint16_t> a(features.begin(), features.end());
            std::vector<uint16_t> b(image.begin(), image.end());
            std::sort(a.begin(), a.end());
            std::sort(b.begin(), b.end());
            assert(a == b && "mirrored positions share their features");
        }
    }
}

/** Whole board cases: the right number of features, all of them addressing the network. */
void testFeatureRanges() {
    auto initial = featuresOf(fen::initialPosition, Color::w);
    assert(initial.size() == 32 && "every piece contributes exactly one feature");

    // The starting position is symmetric under swapping colors and flipping ranks, which is
    // precisely what changing perspective does, so both sides see the same features.
    assert(featuresOf(fen::initialPosition, Color::b) == initial);

    for (const auto* fen : {fen::initialPosition,
                            "4k3/8/8/8/8/8/8/4K3 w - - 0 1",
                            "3k4/8/8/8/8/8/8/R2K4 w - - 0 1",
                            "8/2p5/8/8/8/8/5P2/2K2k2 w - - 0 1",
                            "r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
                            "8/PPPPPPPk/8/8/8/8/pppppppK/8 w - - 0 1"}) {
        auto position = fen::parsePosition(fen);
        for (auto perspective : {Color::w, Color::b}) {
            auto features = activeFeatures(position, perspective);
            assert(features.size <= nnue::ActiveFeatures::kMaxSize);
            for (auto index : features)
                assert(index < nnue::Architecture::kInputDimensions &&
                       "every feature must address a row of the transformer");
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Feature transform
// ---------------------------------------------------------------------------------------------

using nnue::Accumulator;
using nnue::refresh;
using nnue::transform;

/** FNV-1a-64 over bytes, the hash the Stockfish reference below was made to print. */
uint64_t fnv1a64(const std::vector<uint8_t>& bytes) {
    uint64_t hash = 0xcbf29ce484222325ull;
    for (auto byte : bytes) {
        hash ^= byte;
        hash *= 0x100000001b3ull;
    }
    return hash;
}

/** A feature transformer small enough to work out its arithmetic by hand: three features, l1 4. */
nnue::FeatureTransformer handTransformer() {
    nnue::FeatureTransformer transformer;
    transformer.biases = {1, 2, 3, 4};
    transformer.weights = {
        10,  20,  30,  40,   // feature 0
        -1,  -2,  -3,  -4,   // feature 1
        100, 100, 100, 100,  // feature 2
    };
    transformer.psqtWeights = {
        1,  2,  3,  4,  5,  6,  7,  8,   // feature 0
        0,  0,  0,  0,  0,  0,  0,  0,   // feature 1
        10, 20, 30, 40, 50, 60, 70, 80,  // feature 2
    };
    return transformer;
}

/**
 * Fresh accumulation reads both parameter arrays feature major, so a feature's contribution is a
 * contiguous run of each. Getting that stride wrong is the mistake the whole phase guards against,
 * and here it is pinned without needing a network file at all.
 */
void testFreshAccumulation() {
    auto transformer = handTransformer();

    // An empty position accumulates nothing, leaving the biases as they are.
    assert(refresh(transformer, {}).values == std::vector<int16_t>({1, 2, 3, 4}));
    assert((refresh(transformer, {}).psqt == std::array<int32_t, 8>{}));

    nnue::ActiveFeatures features;
    features.add(0);
    features.add(2);
    auto accumulator = refresh(transformer, features);
    assert(accumulator.values == std::vector<int16_t>({111, 122, 133, 144}));
    assert((accumulator.psqt == std::array<int32_t, 8>({11, 22, 33, 44, 55, 66, 77, 88})));

    // Accumulation is a plain sum, so the same feature twice counts twice and order is immaterial.
    nnue::ActiveFeatures twice;
    twice.add(1);
    twice.add(1);
    assert(refresh(transformer, twice).values == std::vector<int16_t>({-1, -2, -3, -4}));

    nnue::ActiveFeatures reversed;
    reversed.add(2);
    reversed.add(0);
    assert(refresh(transformer, reversed).values == accumulator.values);
}

/** The clipping, the pairing of the two halves, the side to move ordering and the PSQT halving. */
void testTransformArithmetic() {
    // Two entries per half, so each perspective produces two output bytes.
    Accumulator white, black;
    white.values = {200, -5, 100, 64};    // halves (200, -5) and (100, 64)
    black.values = {130, 300, 127, -3};   // halves (130, 300) and (127, -3)
    white.psqt = {10, -3, 7, 0, 5, 5, 5, 5};
    black.psqt = {4, 4, 4, 4, 4, 4, 4, 4};

    // White's bytes are 127 * 100 / 128 == 99 and 0, anything paired with a clipped away value
    // being zero; black's are 127 * 127 / 128 == 126, the largest byte the transform can produce,
    // and 0. Whichever side is to move contributes the first half of the output.
    auto forWhite = transform(white, black, Color::w, 0);
    auto forBlack = transform(white, black, Color::b, 0);
    assert(forWhite.features == std::vector<uint8_t>({99, 0, 126, 0}));
    assert(forBlack.features == std::vector<uint8_t>({126, 0, 99, 0}));

    // The PSQT term is the difference of the two perspectives, halved, truncating towards zero.
    assert(transform(white, black, Color::w, 0).psqt == 3);    // (10 - 4) / 2
    assert(transform(white, black, Color::w, 1).psqt == -3);   // (-3 - 4) / 2, not -4
    assert(transform(white, black, Color::w, 2).psqt == 1);    // (7 - 4) / 2
    assert(transform(white, black, Color::b, 1).psqt == 3);    // (4 - -3) / 2
    assert(transform(white, black, Color::b, 3).psqt == 2);    // (4 - 0) / 2

    // Only the PSQT term depends on the bucket; the transformed bytes never do.
    for (uint32_t bucket = 0; bucket < nnue::Architecture::kPSQTBuckets; ++bucket)
        assert(transform(white, black, Color::b, bucket).features == forBlack.features);
}

/**
 * What Stockfish 16.1 itself computes for a position, as its exact internal integers.
 *
 * Produced by a throwaway patch to a Stockfish 16.1 checkout (tag sf_16.1, commit e67cc97) adding
 * a UCI command that runs the Big feature transformer over the current position and prints these
 * numbers, rather than the centipawn score its "eval" command formats. The patch is deliberately
 * not committed here and CI does not need Stockfish: only these few numbers are the oracle.
 *
 * Both a SIMD (x86-64-avx2) and a scalar (general-64) build of that patch were checked to produce
 * these values, so they pin the arithmetic rather than one vectorization of it.
 */
struct GoldenTransform {
    const char* fen;
    uint64_t hash;   // FNV-1a-64 over all l1 output bytes
    uint32_t sum;    // their plain sum, which localizes a mismatch to ordering rather than values
    uint8_t bytes[4];  // output[0], [l1/2 - 1], [l1/2] and [l1 - 1], straddling the perspectives
    int32_t psqt[nnue::Architecture::kPSQTBuckets];
};

const GoldenTransform kGoldenTransforms[] = {
    // An ordinary opening. Both kings stand on e1 and e8, so neither perspective is mirrored, and
    // the position is symmetric under swapping colors, which is exactly what changing perspective
    // does: the two accumulators match and the PSQT term is zero in every bucket.
    {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     0xd9464fb8181c219dull,
     6526,
     {0, 0, 0, 0},
     {0, 0, 0, 0, 0, 0, 0, 0}},

    // A few moves later, with black to move, which swaps the halves of the output.
    {"r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 4 4",
     0x0b8d73ce1682dfc8ull,
     6125,
     {0, 0, 0, 0},
     {239, -665, -815, -926, -782, -830, -810, -737}},

    // A middlegame with white castled queenside, putting its king on c1: white's perspective is
    // horizontally mirrored while black's, with its king on g8, is not.
    {"r4rk1/pp2ppbp/2np1np1/q1p5/4P3/2N1BP2/PPPQN1PP/2KR1B1R w - - 4 12",
     0x4a3fe0812f6eb663ull,
     7606,
     {0, 0, 4, 0},
     {33629, 8521, 8525, 7929, 8115, 8187, 8886, 9477}},

    // The same position with the colors swapped and the ranks flipped, so black is to move and it
    // is black's perspective that is mirrored. The feature set is built so that this is the same
    // position seen from the other side, and Stockfish indeed produces the identical output above.
    {"2kr1b1r/pppqn1pp/2n1bp2/4p3/Q1P5/2NP1NP1/PP2PPBP/R4RK1 b - - 4 12",
     0x4a3fe0812f6eb663ull,
     7606,
     {0, 0, 4, 0},
     {33629, 8521, 8525, 7929, 8115, 8187, 8886, 9477}},

    // A sparse endgame: three pieces, so only three accumulator rows are added to the biases.
    {"8/5k2/8/8/3P4/8/5K2/8 w - - 0 1",
     0x8da28a7c6a1c5cdcull,
     4251,
     {0, 0, 0, 0},
     {-1517, 2663, 3032, 3040, 3099, 3289, 3343, 3238}},

    // A sparse endgame with black to move and both kings on files a to d, so both perspectives
    // are mirrored at once.
    {"8/2k5/2p5/8/1P6/8/3K4/6R1 b - - 0 1",
     0x629d8f967636f771ull,
     5196,
     {0, 0, 23, 0},
     {-10498, -17386, -19216, -18475, -18007, -17765, -18174, -18107}},

    // Lopsided material, which drives the PSQT term far from zero in every bucket.
    {"4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1",
     0x8845d602eb113a61ull,
     8570,
     {53, 0, 0, 0},
     {21181, 35303, 38419, 36846, 35936, 35404, 36196, 35787}},
};

/** Plain sum of transformed bytes, which no single byte of the output can dominate. */
uint32_t byteSum(const std::vector<uint8_t>& bytes) {
    return uint32_t(std::accumulate(bytes.begin(), bytes.end(), uint64_t(0)));
}

/**
 * The cross implementation checkpoint: every transformed byte and every PSQT value must match
 * Stockfish 16.1 exactly. Between them these cover the file decoding, the transformer's parameter
 * layout, feature indexing, king mirroring, the piece perspective mapping, fresh accumulation,
 * clipping, the pairwise product, the side to move ordering and the PSQT arithmetic.
 */
void testGoldenTransforms(const nnue::Network& network) {
    auto l1 = network.arch.l1;

    for (const auto& golden : kGoldenTransforms) {
        auto position = fen::parsePosition(golden.fen);
        auto white = refresh(network.transformer, position, Color::w);
        auto black = refresh(network.transformer, position, Color::b);
        assert(white.values.size() == l1 && black.values.size() == l1);

        for (uint32_t bucket = 0; bucket < nnue::Architecture::kPSQTBuckets; ++bucket) {
            auto transformed = transform(white, black, position.active(), bucket);
            assert(transformed.features.size() == l1);

            // Report before asserting: a mismatch here is worth seeing in full.
            auto hash = fnv1a64(transformed.features);
            if (hash != golden.hash || byteSum(transformed.features) != golden.sum)
                std::cerr << golden.fen << "\n  expected hash " << std::hex << golden.hash
                          << " got " << hash << std::dec << ", expected sum " << golden.sum
                          << " got " << byteSum(transformed.features) << "\n";

            assert(hash == golden.hash && "transformed bytes must match Stockfish 16.1 exactly");
            assert(byteSum(transformed.features) == golden.sum);
            assert(transformed.features[0] == golden.bytes[0]);
            assert(transformed.features[l1 / 2 - 1] == golden.bytes[1]);
            assert(transformed.features[l1 / 2] == golden.bytes[2]);
            assert(transformed.features[l1 - 1] == golden.bytes[3]);

            if (transformed.psqt != golden.psqt[bucket])
                std::cerr << golden.fen << " bucket " << bucket << ": expected PSQT "
                          << golden.psqt[bucket] << ", got " << transformed.psqt << "\n";
            assert(transformed.psqt == golden.psqt[bucket]);

            // The convenience overload refreshes both perspectives itself, and must not differ.
            auto direct = transform(network.transformer, position, bucket);
            assert(direct.features == transformed.features && direct.psqt == transformed.psqt);
        }
    }
    std::cout << "  " << std::size(kGoldenTransforms)
              << " positions transform bit identically to Stockfish 16.1\n";

    // Not a check, just a note on what a deliberately plain scalar refresh costs today.
    auto position = fen::parsePosition(kGoldenTransforms[2].fen);
    constexpr int kRepeats = 100;
    int64_t total = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kRepeats; ++i) total += transform(network.transformer, position, 0).psqt;
    auto elapsed = std::chrono::steady_clock::now() - start;
    assert(total == kRepeats * int64_t(kGoldenTransforms[2].psqt[0]) && "the work was really done");
    std::cout << "  fresh transform of a 30 piece position: "
              << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / kRepeats
              << " us\n";
}

// ---------------------------------------------------------------------------------------------
// Layer stack propagation
// ---------------------------------------------------------------------------------------------

using nnue::affineForward;
using nnue::AffineLayer;
using nnue::clippedReLU;
using nnue::LayerStack;
using nnue::Propagation;
using nnue::propagate;
using nnue::sqrClippedReLU;

/**
 * A layer of the given shape, with `rows` holding its weights unpadded, one output at a time.
 *
 * The padding columns are filled with `padFill` rather than left zero: a real network pads with
 * zeros, so only a deliberately poisoned padding can tell a reader that ignores it from one that
 * happens to add nothing.
 */
AffineLayer makeLayer(uint32_t inputs, uint32_t outputs, const std::vector<int32_t>& biases,
                      const std::vector<int8_t>& rows, int8_t padFill = 0) {
    assert(biases.size() == outputs && rows.size() == size_t(inputs) * outputs);

    AffineLayer layer;
    layer.inputs = inputs;
    layer.paddedInputs = nnue::Architecture::padded(inputs);
    layer.outputs = outputs;
    layer.biases = biases;
    layer.weights.assign(size_t(outputs) * layer.paddedInputs, padFill);
    for (uint32_t i = 0; i < outputs; ++i)
        for (uint32_t j = 0; j < inputs; ++j)
            layer.weights[i * layer.paddedInputs + j] = rows[i * inputs + j];

    return layer;
}

void testAffineForward() {
    // Three outputs over two inputs, chosen so that each row exercises a different sign pattern.
    auto layer = makeLayer(2, 3, {10, -100, 0}, {1, 2, 3, -4, -5, 0});
    auto out = affineForward(layer, {7, 11});
    assert(out.size() == 3);
    assert(out[0] == 10 + 1 * 7 + 2 * 11);     // 39
    assert(out[1] == -100 + 3 * 7 - 4 * 11);   // -123
    assert(out[2] == 0 - 5 * 7 + 0 * 11);      // -35

    // A zero input leaves nothing but the biases, whatever the weights are.
    assert(affineForward(layer, {0, 0}) == std::vector<int32_t>({10, -100, 0}));

    // Inputs are unsigned: a byte of 255 is 255, not -1. The largest term a real layer can
    // produce is 127 * 127, and the widest layer sums 2560 of them without leaving int32.
    auto extreme = makeLayer(1, 1, {0}, {127});
    assert(affineForward(extreme, {255})[0] == 127 * 255);
    auto wide = makeLayer(2560, 1, {0}, std::vector<int8_t>(2560, 127));
    assert(affineForward(wide, std::vector<uint8_t>(2560, 127))[0] == 2560 * 127 * 127);

    // The padding columns are never read, even when they are not the zeros a real network has.
    auto poisoned = makeLayer(2, 3, {10, -100, 0}, {1, 2, 3, -4, -5, 0}, 127);
    assert(poisoned.paddedInputs == 32 && poisoned.weight(0, 2) == 127);
    assert(affineForward(poisoned, {7, 11}) == out);
}

void testActivations() {
    // The plain clipped ReLU is an arithmetic shift by 6 clamped to a byte, so it floors at zero
    // and saturates at 127. 127 << 6 is the last value the shift alone lands on 127.
    assert(clippedReLU(0) == 0);
    assert(clippedReLU(63) == 0 && clippedReLU(64) == 1 && clippedReLU(65) == 1);
    assert(clippedReLU(8127) == 126 && clippedReLU(8128) == 127);
    assert(clippedReLU(8192) == 127 && clippedReLU(1 << 30) == 127);
    assert(clippedReLU(-1) == 0 && clippedReLU(-64) == 0 && clippedReLU(-(1 << 30)) == 0);

    // The squared clipped ReLU shifts by 19 and takes a minimum instead of a clamp, having no
    // negative values to floor: squaring discards the sign, so -x and x give the same answer.
    assert(sqrClippedReLU(0) == 0);
    assert(sqrClippedReLU(724) == 0 && sqrClippedReLU(725) == 1);  // 725 * 725 just clears 1 << 19
    assert(sqrClippedReLU(8159) == 126 && sqrClippedReLU(8160) == 127);
    assert(sqrClippedReLU(8192) == 127 && sqrClippedReLU(100000) == 127);
    for (int32_t value : {1, 725, 2232, 8159, 8160, 100000})
        assert(sqrClippedReLU(-value) == sqrClippedReLU(value));

    // The square of a plausible layer output does not fit an int32, so the shift must be 64 bit.
    assert(sqrClippedReLU(50000) == 127 && sqrClippedReLU(-50000) == 127);
    assert(int64_t(50000) * 50000 > int64_t(1) << 31 && "the 64 bit product is the point");
}

/**
 * A layer stack small enough to work out every intermediate by hand: four features, l2 2, l3 2.
 *
 * The biases carry most of the arithmetic so that each expected value below is a round number in
 * the units the activation that follows it works in.
 */
LayerStack handStack() {
    LayerStack stack;
    // fc0[0] = 7928 + 100 * 2 = 8128, fc0[1] = 3715 + 127 * 3 = 4096,
    // fc0[2] = 8001 + 127 * 1 = 8128, the last being the forward skip.
    stack.fc0 = makeLayer(4, 3, {7928, 3715, 8001},
                          {100, 0, 0, 0,  //
                           0, 127, 0, 0,  //
                           0, 0, 0, 127},
                          127);
    // fc1[0] = 64 + 64 * 126 = 8128, fc1[1] = 96 + 100 * 32 = 3296.
    stack.fc1 = makeLayer(4, 2, {64, 96},
                          {64, 0, 0, 0,  //
                           0, 100, 0, 0},
                          127);
    // fc2 = 1000 + 2 * 127 - 1 * 51 = 1203.
    stack.fc2 = makeLayer(2, 1, {1000}, {2, -1}, 127);
    return stack;
}

void testSyntheticStack() {
    auto stack = handStack();
    std::vector<uint8_t> features = {2, 3, 0, 1};

    Propagation p;
    auto output = propagate(stack, features, &p);

    assert(p.fc0 == std::vector<int32_t>({8128, 4096, 8128}));

    // The squared half comes first: (8128^2) >> 19 == 126 and (4096^2) >> 19 == 32, followed by
    // the plain half, 8128 >> 6 == 127 and 4096 >> 6 == 64. Both halves read the same fc0 values,
    // and fc0[2] appears in neither, being the forward skip.
    assert(p.fc1Input == std::vector<uint8_t>({126, 32, 127, 64}));

    assert(p.fc1 == std::vector<int32_t>({8128, 3296}));
    assert(p.fc2Input == std::vector<uint8_t>({127, 51}));  // 3296 >> 6 == 51, truncating
    assert(p.fc2 == 1203);

    // The skip is fc0[2] rescaled by 600 * 16 / (127 * 64), which is exactly 1 for 8128.
    assert(p.forwardSkip == 9600);
    assert(p.output == 1203 + 9600 && output == p.output);

    // Tracing must not change the answer, and a stack is a pure function of its input.
    assert(propagate(stack, features) == output);
    assert(propagate(stack, {0, 0, 0, 0}) != output && "the features must actually reach fc0");

    std::cout << "Hand computed layer stack propagates to " << output << "\n";
}

/** FNV-1a-64 over int32 values as little endian bytes: the same hash over a wider array. */
uint64_t fnv1a64(const std::vector<int32_t>& values) {
    std::vector<uint8_t> bytes;
    bytes.reserve(values.size() * sizeof(int32_t));
    for (auto value : values)
        for (size_t i = 0; i < sizeof(int32_t); ++i)
            bytes.push_back(uint8_t(uint32_t(value) >> (8 * i)));

    return fnv1a64(bytes);
}

/**
 * What Stockfish 16.1 computes inside one layer stack, for one of the golden positions.
 *
 * Same provenance as kGoldenTransforms above: the nndump patch on tag sf_16.1, checked to agree
 * between an x86-64-avx2 and a general-64 build. Each layer is pinned by a hash and a sum of its
 * whole output rather than by the values themselves, which would run to five thousand numbers;
 * between them the hash catches a changed value and the sum catches a reordering that a hash
 * alone could not distinguish from noise. The three scalars at the end are exact.
 *
 * Every layer is checked, not just the last. Two errors that cancel - a transposed weight matrix
 * read by a mirrored index, say - would leave the final output right and every step wrong.
 */
struct GoldenPropagation {
    uint64_t fc0Hash;  // over the l2 + 1 outputs of fc0
    int64_t fc0Sum;
    int32_t fc0First;       // fc0[0], the first of the 2560 wide dot products
    int32_t fc0Skip;        // fc0[l2], the forward skip as the layer leaves it
    uint64_t fc1InputHash;  // over the 2 * l2 activation bytes fc1 reads
    int64_t fc1InputSum;
    uint64_t fc1Hash;  // over the l3 outputs of fc1
    int64_t fc1Sum;
    uint64_t fc2InputHash;  // over the l3 activation bytes fc2 reads
    int64_t fc2InputSum;
    int32_t fc2;          // fc2's single output
    int32_t forwardSkip;  // fc0Skip rescaled into the output's units
    int32_t output;       // the stack's result, fc2 + forwardSkip
};

/** One row per bucket, for each of the seven positions of kGoldenTransforms, in that order. */
const GoldenPropagation
    kGoldenPropagations[std::size(kGoldenTransforms)][nnue::Architecture::kLayerStacks] = {
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    {
        {0x4ac047b70a5fc77cull, 68768, 2232, 1515,
         0x27cddc05eaf94ae9ull, 2460, 0xe41cf993997ac243ull, 79687,
         0x7151a106fefe16e4ull, 1747, -3298, 1789, -1509},
        {0x0277265f060dfd95ull, 82408, 20409, 3118,
         0x354d5b9d54dc3b5cull, 2127, 0x2ed73cd94a716b3bull, -60459,
         0x6b91bf66732bb1faull, 821, 1323, 3682, 5005},
        {0x9b401c8400005964ull, 52356, 7623, 1133,
         0xccce6a56c608c5ccull, 1559, 0xffd5f7990754174full, -54844,
         0x350bf9720bb7650dull, 726, 199, 1338, 1537},
        {0xd8404c0d23c6657bull, 20406, 12363, 2663,
         0x0cb675e9057f13a1ull, 976, 0x675080ef9f477eb8ull, -52085,
         0x7c8d45303aef1360ull, 493, -2559, 3145, 586},
        {0x3aaeae5c1685d7adull, -12068, 4812, 2311,
         0x928bafbfd713f1c2ull, 691, 0x25a9dafbaba16281ull, -44316,
         0x75fa0792648a1569ull, 502, -1111, 2729, 1618},
        {0xb636b08f0407e087ull, -8372, 2538, 1646,
         0xdba200322485c10cull, 527, 0xd9e3d01c2f14a768ull, -37597,
         0xcd7f993931d6241dull, 352, -1516, 1944, 428},
        {0xcdda25ba7699c286ull, 14086, 203, 2732,
         0x79b87bd9620e5dc1ull, 544, 0x74eb4667c6669459ull, -61241,
         0x046837dc70b58228ull, 409, -2737, 3226, 489},
        {0xf98fb2d7596b2d4cull, -7552, -1018, 2643,
         0xc250266a6cefae0full, 452, 0xfedcf3db734d22f9ull, -69433,
         0xbbc209fa5518528eull, 303, -2548, 3121, 573},
    },
    // r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 4 4
    {
        {0x4585d258589934f5ull, 47089, 4868, 1214,
         0xe16ba3b4a596fe35ull, 2594, 0x4aa2e88a2ed78929ull, 67189,
         0x5fdf1cb3558fe88dull, 1660, -19502, 1433, -18069},
        {0x851b409085076f0cull, 81674, 19471, 3250,
         0xbaa673148e6dc8e1ull, 2206, 0x79e42c0d14818e28ull, -57535,
         0x038a23910cb5e83aull, 855, 663, 3838, 4501},
        {0xa3b3d8da202517e4ull, 48257, 8149, 1559,
         0x4c07a53a65290975ull, 1516, 0x8a8e6d4366556d3bull, -52597,
         0x5b404b5179622136ull, 697, -1684, 1841, 157},
        {0x3f7b1feee7b82a57ull, 18842, 11763, 2679,
         0x8aa59cee4edf84aeull, 847, 0x83aa893a8c23ee14ull, -42980,
         0x45ece28b1062f6a9ull, 434, -3239, 3164, -75},
        {0x267d6b012d7be399ull, -15158, 3012, 1505,
         0x35951d67b6d2e9f1ull, 550, 0xb6543f7c06ab0a82ull, -41422,
         0x8d901961f2ddb56eull, 467, -1749, 1777, 28},
        {0x5258ddb3a12b9706ull, -9103, 2332, 689,
         0x024c37cef43a4081ull, 390, 0x2a7bcb362862b2b4ull, -33819,
         0x1435a2847811839aull, 339, -717, 813, 96},
        {0x53f4de5eecf87816ull, 7211, -66, 1288,
         0x1c6208a37e0027bbull, 382, 0xf595a801c462c7b2ull, -48845,
         0x475f9b309cb31c2aull, 379, -1106, 1521, 415},
        {0x4ac90f547f63fe70ull, -6198, -1280, 1111,
         0x6f47d217b379f9b7ull, 268, 0x5807ea5070345eedull, -58296,
         0x1fc7c169626bbcbcull, 281, -1113, 1312, 199},
    },
    // r4rk1/pp2ppbp/2np1np1/q1p5/4P3/2N1BP2/PPPQN1PP/2KR1B1R w - - 4 12
    {
        {0x3fcddb9b95df248bull, 85414, 15771, -2410,
         0xde54013503cad8d9ull, 2736, 0xdda97cba7a6c1ab6ull, 58622,
         0x25fa8eee1700d851ull, 1550, -30330, -2846, -33176},
        {0x5d770c215d6e3849ull, 115172, 39765, 1798,
         0xf296b80153b58503ull, 2224, 0x8d620ae77af38d6eull, -46890,
         0xeb37318f0bb67d77ull, 872, 2099, 2123, 4222},
        {0x5e31a187830fab90ull, 41631, 23498, -1331,
         0x84304a8b6970e1f6ull, 1855, 0xed295df0fea87c74ull, -67030,
         0x13ea96af40373654ull, 697, 5815, -1572, 4243},
        {0x4b344c22d36099e1ull, 9534, 11165, 458,
         0x17b2b8d86e86fb96ull, 1491, 0xef3c8d33005b5653ull, -51109,
         0x53e9d5dc4b41d217ull, 938, 3262, 540, 3802},
        {0x59b8ffd08f1ed469ull, -24980, 2900, 445,
         0x68b3e77fbb51dd20ull, 1105, 0x2ec9c89bec99d1a5ull, -35927,
         0xdd8f83e56425e833ull, 912, 5743, 525, 6268},
        {0x5a25004f77ece916ull, -16519, 2046, 178,
         0x3054c08f3b7fc5f5ull, 838, 0x2bf95ad728fa2da8ull, -54326,
         0xe167afd7bea3dcc3ull, 794, 7770, 210, 7980},
        {0x462dd9c87bd7184dull, -40854, -1797, 1558,
         0xe406b86508f52cdaull, 905, 0x5d6d524447bec194ull, -99471,
         0x54b4edfe95851897ull, 464, 4954, 1840, 6794},
        {0xfafaf9f14c675099ull, -23748, -2474, 824,
         0xe2319c52d0aa7993ull, 940, 0xf6a37ecfa081ee5eull, -121682,
         0x9f49dd17e55c8909ull, 420, 3828, 973, 4801},
    },
    // 2kr1b1r/pppqn1pp/2n1bp2/4p3/Q1P5/2NP1NP1/PP2PPBP/R4RK1 b - - 4 12
    {
        {0x3fcddb9b95df248bull, 85414, 15771, -2410,
         0xde54013503cad8d9ull, 2736, 0xdda97cba7a6c1ab6ull, 58622,
         0x25fa8eee1700d851ull, 1550, -30330, -2846, -33176},
        {0x5d770c215d6e3849ull, 115172, 39765, 1798,
         0xf296b80153b58503ull, 2224, 0x8d620ae77af38d6eull, -46890,
         0xeb37318f0bb67d77ull, 872, 2099, 2123, 4222},
        {0x5e31a187830fab90ull, 41631, 23498, -1331,
         0x84304a8b6970e1f6ull, 1855, 0xed295df0fea87c74ull, -67030,
         0x13ea96af40373654ull, 697, 5815, -1572, 4243},
        {0x4b344c22d36099e1ull, 9534, 11165, 458,
         0x17b2b8d86e86fb96ull, 1491, 0xef3c8d33005b5653ull, -51109,
         0x53e9d5dc4b41d217ull, 938, 3262, 540, 3802},
        {0x59b8ffd08f1ed469ull, -24980, 2900, 445,
         0x68b3e77fbb51dd20ull, 1105, 0x2ec9c89bec99d1a5ull, -35927,
         0xdd8f83e56425e833ull, 912, 5743, 525, 6268},
        {0x5a25004f77ece916ull, -16519, 2046, 178,
         0x3054c08f3b7fc5f5ull, 838, 0x2bf95ad728fa2da8ull, -54326,
         0xe167afd7bea3dcc3ull, 794, 7770, 210, 7980},
        {0x462dd9c87bd7184dull, -40854, -1797, 1558,
         0xe406b86508f52cdaull, 905, 0x5d6d524447bec194ull, -99471,
         0x54b4edfe95851897ull, 464, 4954, 1840, 6794},
        {0xfafaf9f14c675099ull, -23748, -2474, 824,
         0xe2319c52d0aa7993ull, 940, 0xf6a37ecfa081ee5eull, -121682,
         0x9f49dd17e55c8909ull, 420, 3828, 973, 4801},
    },
    // 8/5k2/8/8/3P4/8/5K2/8 w - - 0 1
    {
        {0x7e6e4156c6f82a60ull, 3964, 514, 3881,
         0xd6d94659ed67e080ull, 917, 0xf41a5c68fdaf8e62ull, -13209,
         0x8acfb75fb0985331ull, 836, -2781, 4583, 1802},
        {0x5464836930ee8c02ull, 1145, -4210, 384,
         0x1e9b63a83ec446c8ull, 1109, 0x7025f850571f42b6ull, -24269,
         0x52ae21c407d98d43ull, 756, 5681, 453, 6134},
        {0x91cd41c100d212eeull, 18344, -1402, 3401,
         0xb6f92345ae9cbd87ull, 916, 0x5eff425f69c00a47ull, -23379,
         0x8761beba747da895ull, 720, -3394, 4016, 622},
        {0x3dadd7af089fa8c9ull, 10702, 4986, 3246,
         0xb1295c1b67c24312ull, 1365, 0x2bf8ba9b94d3feefull, -2818,
         0x576335b594d5cda3ull, 878, -4117, 3833, -284},
        {0xe86d2714e1acb738ull, 3281, 6217, 2624,
         0x9cd85622c4767a96ull, 1101, 0x301733cc62de9875ull, -11524,
         0xe46871e0b44eedd1ull, 936, 882, 3099, 3981},
        {0x755ca452e5d0f891ull, 32613, 10050, 3374,
         0xda3002cc58325274ull, 1251, 0xab4b53e0f6c71b0aull, -19185,
         0x20d2255f7c515ca0ull, 633, -6089, 3985, -2104},
        {0xa8ef4069c5d87008ull, 92226, 6823, 6599,
         0x1bf6aad04033dedcull, 2035, 0x969cc8a15f94d1f0ull, -102351,
         0xb116aa04c0920ef4ull, 785, -2970, 7794, 4824},
        {0x1b893b70a5f2ace2ull, 75128, 10188, 8514,
         0xa21edb7ddfdd969eull, 2479, 0xe7d6013c55b1f0d3ull, -210112,
         0xfdd31f1608d94c43ull, 424, -1513, 10055, 8542},
    },
    // 8/2k5/2p5/8/1P6/8/3K4/6R1 b - - 0 1
    {
        {0x9d38d2617d730c08ull, -6321, -3224, 1329,
         0x1fb773b141beca30ull, 1543, 0x7aaeacc036f6e0deull, -15433,
         0x2bb5f237495e3cc5ull, 1576, -37585, 1569, -36016},
        {0xa7d4d3b0adc75d61ull, 20372, 10707, 6794,
         0x969dd155ca3b8e0aull, 1607, 0xebf1dbcce74b8b34ull, 48166,
         0xe7d6ab6bbb469449ull, 1256, -23355, 8024, -15331},
        {0xc2cdea101fbd5318ull, 6248, 4229, 7078,
         0xafcb5058dc2eca45ull, 1148, 0x209278fee6b2d354ull, 5974,
         0x231b8809a9df7ad8ull, 1345, -20508, 8359, -12149},
        {0xc0de8fa5a5bb252dull, -2201, 3355, 4226,
         0xd6aa4043b506c3c3ull, 1116, 0xac3858d2be9e61f7ull, -25128,
         0x05607c76462c8aabull, 1112, -10480, 4991, -5489},
        {0x7e7cbed43f2c738aull, 18431, 5379, 2429,
         0xb2cffc759608c416ull, 1207, 0x3e59bd699ed83f00ull, -10503,
         0xabea9408467c940bull, 966, -1256, 2868, 1612},
        {0x9d77a095041ee856ull, 32661, 9794, 1982,
         0x11863588e2c82899ull, 1624, 0x36262c0fc617fa60ull, -77867,
         0xa681d9b75c3ed9a7ull, 488, -792, 2340, 1548},
        {0x1c0643d1beeda1c1ull, 83179, -8253, 3379,
         0x689e81719ee8f28full, 1734, 0x4796131384f3241aull, -89776,
         0xca29f7319e8fb082ull, 987, -6119, 3990, -2129},
        {0x257ed34a26f5ce1aull, 17111, 2836, 6791,
         0x81280c1d6716a804ull, 1373, 0x2b419d47198cdad7ull, -113541,
         0xfe0dce28ea3d9faaull, 389, -6870, 8020, 1150},
    },
    // 4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1
    {
        {0x146412d97e6ec946ull, 52285, -196, 2352,
         0x998092d7cfbe0dadull, 2334, 0xe13aed7ae17b7d68ull, 41495,
         0xd4a4d34ce266ef64ull, 1287, 23531, 2777, 26308},
        {0xeda62ae29ebfcc3dull, 111458, 36999, 292,
         0x6605e676e7032b13ull, 3040, 0x0183e593fd440168ull, -19369,
         0x1207765d32bf5961ull, 992, 8622, 344, 8966},
        {0x7fdb21ee791cbd8cull, 51172, 18292, 313,
         0x2d219ba6edcf12deull, 2103, 0x28f254fc88c32fafull, -61093,
         0x99bd994aaccd4c09ull, 992, 11566, 369, 11935},
        {0x927d9a380674c32full, -9055, 3388, 808,
         0x6afcfb3b86ad4caeull, 1689, 0x8a699d44f20d7ecaull, -25700,
         0x8c38872250861dfeull, 1005, -3368, 954, -2414},
        {0x4acf1ed9272b0e85ull, -25303, -17496, -2803,
         0xba62880e09d04df9ull, 2398, 0x9611795e7c3ae509ull, -65951,
         0x7b2ede567578b3b8ull, 1155, -503, -3310, -3813},
        {0x2f2f3a6bdc2f07f4ull, -45791, 6950, -6123,
         0x4e3e1e63cd348d50ull, 2185, 0xffd91c1202aff8e0ull, -107858,
         0x09f7a6c117eb5852ull, 1407, -1008, -7231, -8239},
        {0x41c8965511a3496aull, -56526, 14867, -7095,
         0x73a6118a92481784ull, 2047, 0x55a777c68ce67526ull, -164749,
         0x3402aedcf80af9a6ull, 679, -8722, -8379, -17101},
        {0xabc03649c7a8ba61ull, -16966, 13643, -9260,
         0x1ea7023637db8a6aull, 2207, 0x35728a8462095c1aull, -219666,
         0x0e6d56b8f2cfe254ull, 519, 2585, -10937, -8352},
    },
};

/**
 * The phase 6 checkpoint: every intermediate of every layer stack, for every golden position.
 *
 * Each position is transformed once and propagated through all eight stacks. That is not how a
 * position is ever evaluated - Stockfish picks one stack by piece count - but it runs all eight
 * sets of parameters over the same input, so a stack read at the wrong file offset cannot hide
 * behind a position that never selects it.
 */
void testGoldenPropagations(const nnue::Network& network) {
    for (size_t index = 0; index < std::size(kGoldenTransforms); ++index) {
        const char* fen = kGoldenTransforms[index].fen;
        auto transformed = transform(network.transformer, fen::parsePosition(fen), 0);

        for (uint32_t bucket = 0; bucket < nnue::Architecture::kLayerStacks; ++bucket) {
            const auto& golden = kGoldenPropagations[index][bucket];
            Propagation p;
            auto output = propagate(network.stacks[bucket], transformed.features, &p);

            // Report before asserting: which layer first disagrees is the entire diagnostic, and
            // it is lost once the assertion aborts.
            auto agrees = [&](const char* what, uint64_t hash, uint64_t wantHash, int64_t got,
                              int64_t want) {
                if (hash != wantHash || got != want)
                    std::cerr << fen << " bucket " << bucket << ": " << what << " expected hash "
                              << std::hex << wantHash << " got " << hash << std::dec
                              << ", expected sum " << want << " got " << got << "\n";
                return hash == wantHash && got == want;
            };

            assert(p.fc0.size() == network.arch.l2 + 1);
            assert(agrees("fc0", fnv1a64(p.fc0), golden.fc0Hash, sum(p.fc0), golden.fc0Sum));
            assert(p.fc0.front() == golden.fc0First);
            assert(p.fc0.back() == golden.fc0Skip);

            assert(p.fc1Input.size() == 2 * network.arch.l2);
            assert(agrees("fc1 input", fnv1a64(p.fc1Input), golden.fc1InputHash, sum(p.fc1Input),
                          golden.fc1InputSum));

            assert(p.fc1.size() == network.arch.l3);
            assert(agrees("fc1", fnv1a64(p.fc1), golden.fc1Hash, sum(p.fc1), golden.fc1Sum));

            assert(p.fc2Input.size() == network.arch.l3);
            assert(agrees("fc2 input", fnv1a64(p.fc2Input), golden.fc2InputHash, sum(p.fc2Input),
                          golden.fc2InputSum));

            if (p.fc2 != golden.fc2 || p.forwardSkip != golden.forwardSkip)
                std::cerr << fen << " bucket " << bucket << ": expected fc2 " << golden.fc2
                          << " and skip " << golden.forwardSkip << ", got " << p.fc2 << " and "
                          << p.forwardSkip << "\n";
            assert(p.fc2 == golden.fc2);
            assert(p.forwardSkip == golden.forwardSkip);
            assert(p.output == golden.output && output == golden.output);

            // The untraced call is the same computation, and tracing must not have changed it.
            assert(propagate(network.stacks[bucket], transformed.features) == golden.output);
        }
    }

    std::cout << "  " << std::size(kGoldenTransforms) << " positions propagate bit identically "
              << "through all " << nnue::Architecture::kLayerStacks << " stacks\n";

    // As with the transform, a note on what the deliberately scalar stack costs today: fc0 is
    // 2560 int8 multiply-accumulates per output, and dominates the other two layers entirely.
    auto transformed =
        transform(network.transformer, fen::parsePosition(kGoldenTransforms[2].fen), 0);
    constexpr int kRepeats = 100;
    int64_t sink = 0;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kRepeats; ++i) sink += propagate(network.stacks[7], transformed.features);
    auto elapsed = std::chrono::steady_clock::now() - start;
    assert(sink == kRepeats * int64_t(kGoldenPropagations[2][7].output) && "the work was done");
    std::cout << "  propagation through one layer stack: "
              << std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / kRepeats
              << " ns\n";
}

// ---------------------------------------------------------------------------------------------
// The sparse fc0 kernel, against the canonical one
// ---------------------------------------------------------------------------------------------

using nnue::affineForwardSparse;
using nnue::ColumnMajorLayer;
using nnue::transpose;

/**
 * The transposed weights and the sparse kernel, held against the row major ones on every golden
 * position and on synthetic layers the real network cannot reach.
 *
 * The premise of the kernel is a claim about the data - that most of what reaches fc0 is zero -
 * and the point of testing it against the canonical kernel rather than against a table of its own
 * is that the two must agree whatever the input turns out to look like. The canonical path stays
 * the one the golden tables pin, so this is a comparison against something already known exact.
 */
void testSparseAffineForward(const nnue::Network& network) {
    // The transposition itself: every weight of every stack, in its new place.
    for (const auto& stack : network.stacks) {
        const auto& columns = stack.fc0Columns;
        assert(!columns.empty() && columns.inputs == stack.fc0.inputs);
        assert(columns.biases == stack.fc0.biases);
        for (uint32_t i = 0; i < stack.fc0.outputs; ++i)
            for (uint32_t j = 0; j < stack.fc0.inputs; ++j)
                assert(columns.column(j)[i] == stack.fc0.weight(i, j));
    }

    // Only a layer the kernel is specialized for gets a transposed twin at all; anything else
    // keeps the canonical path, which is what an empty ColumnMajorLayer means.
    assert(transpose(makeLayer(4, 3, {0, 0, 0}, std::vector<int8_t>(12, 1))).empty());
    assert(!transpose(network.stacks[0].fc0).empty());
    assert(transpose(network.stacks[0].fc1).empty());  // 32 outputs, not 16

    size_t zeros = 0, total = 0;
    for (size_t index = 0; index < std::size(kGoldenTransforms); ++index) {
        auto position = fen::parsePosition(kGoldenTransforms[index].fen);
        auto transformed = transform(network.transformer, position, 0);
        for (auto feature : transformed.features) zeros += feature == 0;
        total += transformed.features.size();

        for (uint32_t bucket = 0; bucket < nnue::Architecture::kLayerStacks; ++bucket) {
            const auto& stack = network.stacks[bucket];
            auto canonical = affineForward(stack.fc0, transformed.features);

            std::vector<int32_t> sparse(canonical.size());
            affineForwardSparse(stack.fc0Columns,
                                transformed.features.data(),
                                transformed.features.size(),
                                sparse.data());
            assert(sparse == canonical && "the two fc0 kernels must agree value for value");

            // And so must a whole propagation: a stack stripped of its transposed weights takes
            // the canonical path, which is how the golden tables above were established.
            nnue::LayerStack scalarStack;
            scalarStack.fc0 = stack.fc0;
            scalarStack.fc1 = stack.fc1;
            scalarStack.fc2 = stack.fc2;
            assert(scalarStack.fc0Columns.empty());
            assert(propagate(scalarStack, transformed.features) ==
                   propagate(stack, transformed.features));
        }
    }

    // Synthetic layers, for the inputs a real network never presents: a width that is not a whole
    // number of scan steps, so the kernel's tail runs, and an input of nothing but zeros, which
    // must leave the biases exactly.
    for (uint32_t inputs : {1u, 15u, 16u, 17u, 31u, 33u, 64u}) {
        std::vector<int8_t> rows(size_t(inputs) * ColumnMajorLayer::kOutputs);
        for (size_t i = 0; i < rows.size(); ++i) rows[i] = int8_t(pseudoRandom(i, 127));
        std::vector<int32_t> biases(ColumnMajorLayer::kOutputs);
        for (size_t i = 0; i < biases.size(); ++i) biases[i] = pseudoRandom(i + 7, 1000);

        auto layer = makeLayer(inputs, ColumnMajorLayer::kOutputs, biases, rows, 127);
        auto columns = transpose(layer);
        assert(!columns.empty());

        // Mostly zero, as a transform's output is, but with the nonzero bytes falling in every
        // position within a scan step across the widths above.
        std::vector<uint8_t> input(inputs);
        for (uint32_t j = 0; j < inputs; ++j) input[j] = (j % 3 == 0) ? uint8_t(1 + j % 126) : 0;

        std::vector<int32_t> sparse(layer.outputs);
        affineForwardSparse(columns, input.data(), input.size(), sparse.data());
        assert(sparse == affineForward(layer, input));

        std::vector<uint8_t> nothing(inputs, 0);
        affineForwardSparse(columns, nothing.data(), nothing.size(), sparse.data());
        assert(sparse == biases && "no input at all leaves the biases untouched");
    }

    std::cout << "  fc0's two kernels agree on every golden position, " << 100 * zeros / total
              << "% of whose transformed features are zero\n";
}

// ---------------------------------------------------------------------------------------------
// Whole network evaluation
// ---------------------------------------------------------------------------------------------

using nnue::Evaluation;
using nnue::evaluate;
using nnue::evaluateValue;
using nnue::kMaxEvaluation;
using nnue::kNormalizeToPawnValue;
using nnue::kOutputScale;
using nnue::materialBucket;

/** Bucket selection alone, which no golden data is needed to check. */
void testMaterialBuckets() {
    // (pieceCount - 1) / 4, so a bucket spans four counts and the boundaries are exactly where an
    // off by one moves: two kings alone are bucket 0, and a full board is bucket 7.
    const struct {
        uint32_t pieces;
        uint32_t bucket;
    } kCases[] = {{2, 0},  {3, 0},  {4, 0},  {5, 1},  {8, 1},  {9, 2},
                  {12, 2}, {13, 3}, {16, 3}, {17, 4}, {20, 4}, {21, 5},
                  {24, 5}, {25, 6}, {28, 6}, {29, 7}, {32, 7}};
    for (const auto& c : kCases) assert(materialBucket(c.pieces) == c.bucket);

    // The same over real boards, which is where a piece count that forgot the kings would show.
    assert(materialBucket(fen::parsePosition(fen::initialPosition)) == 7);
    assert(materialBucket(fen::parsePosition("4k3/8/8/8/8/8/8/4K3 w - - 0 1")) == 0);
    assert(materialBucket(fen::parsePosition("4k3/4p3/8/8/8/8/4P3/4K3 w - - 0 1")) == 0);
    assert(materialBucket(fen::parsePosition("4k3/3ppp2/8/8/8/8/4P3/4K3 w - - 0 1")) == 1);
    assert(materialBucket(fen::parsePosition("rnbqkbnr/8/8/8/8/8/8/RNBQKBNR w KQkq - 0 1")) == 3);
}

/**
 * What Stockfish 16.1 finally says about the golden positions, from its nndump.
 *
 * The eight values per position are all eight buckets, not just the one the position selects: the
 * combination (psqt + positional) / OutputScale is the same arithmetic in every bucket, and the
 * seven a position never picks are exactly where a sign or a rounding of our own would survive.
 * Both columns are relative to the side to move, as Stockfish's own are.
 */
struct GoldenEvaluation {
    uint32_t pieceCount;
    uint32_t bucket;  // the layer stack that piece count selects
    int32_t value[nnue::Architecture::kLayerStacks];
    int32_t cp[nnue::Architecture::kLayerStacks];
};

/** One row per position of kGoldenTransforms, in that order. */
const GoldenEvaluation kGoldenEvaluations[std::size(kGoldenTransforms)] = {
    // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
    {32, 7, {-94, 312, 96, 36, 101, 26, 30, 35}, {-26, 87, 26, 10, 28, 7, 8, 9}},

    // r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 4 4
    {32, 7, {-1114, 239, -41, -62, -47, -45, -24, -33}, {-312, 67, -11, -17, -13, -12, -6, -9}},

    // r4rk1/pp2ppbp/2np1np1/q1p5/4P3/2N1BP2/PPPQN1PP/2KR1B1R w - - 4 12
    {30, 7, {28, 796, 798, 733, 898, 1010, 980, 892}, {7, 223, 224, 205, 252, 283, 275, 250}},

    // The same position with the colors swapped: black is to move, and every value is identical,
    // because a value is what the side to move thinks of it.
    // 2kr1b1r/pppqn1pp/2n1bp2/4p3/Q1P5/2NP1NP1/PP2PPBP/R4RK1 b - - 4 12
    {30, 7, {28, 796, 798, 733, 898, 1010, 980, 892}, {7, 223, 224, 205, 252, 283, 275, 250}},

    // 8/5k2/8/8/3P4/8/5K2/8 w - - 0 1
    {3, 0, {17, 549, 228, 172, 442, 74, 510, 736}, {4, 154, 64, 48, 124, 20, 143, 206}},

    // 8/2k5/2p5/8/1P6/8/3K4/6R1 b - - 0 1
    {5,
     1,
     {-2907, -2044, -1960, -1497, -1024, -1013, -1268, -1059},
     {-816, -574, -550, -420, -287, -284, -356, -297}},

    // 4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1
    {4,
     0,
     {2968, 2766, 3147, 2152, 2007, 1697, 1193, 1714},
     {833, 776, 883, 604, 563, 476, 335, 481}},
};

/**
 * The phase 7 checkpoint: the value and the centipawn score Stockfish 16.1 itself reports.
 *
 * The intermediates were already pinned by the phases above, so what is new here is the piece
 * count, the bucket it selects, the addition of the two terms, and the two divisions. Every one
 * of those is a place where a plausible variant - rounding to nearest, a bucket off by one, a
 * White relative value - produces numbers that look entirely reasonable and are wrong.
 */
void testGoldenEvaluations(const nnue::Network& network) {
    for (size_t index = 0; index < std::size(kGoldenTransforms); ++index) {
        const char* fen = kGoldenTransforms[index].fen;
        const auto& golden = kGoldenEvaluations[index];
        auto position = fen::parsePosition(fen);

        assert(materialBucket(golden.pieceCount) == golden.bucket);
        assert(materialBucket(position) == golden.bucket);

        for (uint32_t bucket = 0; bucket < nnue::Architecture::kLayerStacks; ++bucket) {
            auto transformed = transform(network.transformer, position, bucket);
            auto positional = propagate(network.stacks[bucket], transformed.features);
            auto value = (transformed.psqt + positional) / kOutputScale;
            if (value != golden.value[bucket])
                std::cerr << fen << " bucket " << bucket << ": expected value "
                          << golden.value[bucket] << ", got " << value << "\n";

            assert(value == golden.value[bucket]);
            assert(value * 100 / kNormalizeToPawnValue == golden.cp[bucket]);
        }

        // And what the network actually says, through the single stack the position selects.
        Evaluation trace;
        auto value = evaluateValue(network, position, &trace);
        assert(value == golden.value[golden.bucket]);
        assert(trace.pieceCount == golden.pieceCount);
        assert(trace.bucket == golden.bucket);
        assert(trace.value == value);
        assert(trace.psqt == kGoldenTransforms[index].psqt[golden.bucket]);
        assert(trace.positional == kGoldenPropagations[index][golden.bucket].output);

        // Tracing must not have changed the computation, as with propagate().
        assert(evaluateValue(network, position) == value);

        // In centipawns the score leaves the side to move's frame for White's, and only there.
        auto cp = golden.cp[golden.bucket];
        if (position.active() == Color::b) cp = -cp;
        assert(evaluate(network, position) == cp);
    }

    std::cout << "  " << std::size(kGoldenTransforms)
              << " positions evaluate to Stockfish 16.1's exact Value in all "
              << nnue::Architecture::kLayerStacks << " buckets\n";
}

/** The same position with the colors exchanged: ranks flipped and every piece recolored. */
Position colorSwapped(const Position& position) {
    Position result;
    for (Square square : squares) {
        auto piece = position.board[square];
        if (piece != Piece::_)
            result.board[Square(square ^ 56)] = addColor(type(piece), !color(piece));
    }
    result.turn = Turn(!position.active());
    return result;
}

/**
 * Swapping the colors of a position leaves the value alone and negates the score.
 *
 * Those are two different claims and both are load bearing. The value is the side to move's, and
 * a color swap swaps who that is, so it cannot change; the centipawn score is White's, so it must
 * change sign. An implementation that negated in the wrong place would satisfy one and not both.
 */
void testColorSymmetry(const nnue::Network& network) {
    for (const auto& golden : kGoldenTransforms) {
        auto position = fen::parsePosition(golden.fen);
        auto swapped = colorSwapped(position);
        assert(materialBucket(swapped) == materialBucket(position));
        assert(evaluateValue(network, swapped) == evaluateValue(network, position));
        assert(evaluate(network, swapped) == -evaluate(network, position));
    }
    std::cout << "  color swapped positions keep their value and negate their score\n";
}

/** A position lost beyond what a Score can hold, which is what the clamp exists for. */
void testEvaluationClamp(const nnue::Network& network) {
    auto position = fen::parsePosition("4k3/8/8/8/8/QQQQQQQQ/QQQQQQQQ/RRRRKRRR w - - 0 1");
    Evaluation trace;
    evaluateValue(network, position, &trace);

    auto unclamped = trace.value * 100 / kNormalizeToPawnValue;
    assert(unclamped > kMaxEvaluation && "the clamp needs something to clamp to be a test");
    assert(evaluate(network, position) == kMaxEvaluation);
    assert(evaluate(network, colorSwapped(position)) == -kMaxEvaluation);

    // What comes out is a Score the search can hold, and it stays clear of the mate band.
    assert(Score::fromCP(evaluate(network, position)) < Score::mateIn(99));
    std::cout << "  a hopeless position clamps at " << kMaxEvaluation << " cp, from " << unclamped
              << "\n";
}

// ---------------------------------------------------------------------------------------------
// Incremental accumulators
// ---------------------------------------------------------------------------------------------

using nnue::AccumulatorStack;
using nnue::PieceChanges;
using nnue::pieceChanges;

/** A placement written the way the cases below name one, so that they compare as sets. */
std::string to_string(const PieceChanges::Placement& placement) {
    return std::string(1, pieceChars[index(placement.piece)]) + to_string(placement.square);
}

/** The removals and arrivals of a change, sorted, so that their order here is not a test. */
std::vector<std::string> placementsOf(const PieceChanges& changes, bool removed) {
    std::vector<std::string> names;
    auto count = removed ? changes.removedCount : changes.addedCount;
    for (uint8_t i = 0; i < count; ++i)
        names.push_back(to_string(removed ? changes.removed[i] : changes.added[i]));

    std::sort(names.begin(), names.end());
    return names;
}

/**
 * Check what one move takes off the board and what it puts back, naming pieces as "Pe2".
 *
 * This is the whole of what an incremental update needs to know about a move, and it is derived
 * from a BoardChange rather than from the move kind, so every kind of compound move is a case
 * worth writing out: the derivation has no branch on MoveKind to read.
 */
void expectChanges(const std::string& fen, Move move, std::vector<std::string> removed,
                   std::vector<std::string> added) {
    auto position = fen::parsePosition(fen);
    auto change = moves::prepareMove(position.board, move);
    auto changes = pieceChanges(position.board, change);

    std::sort(removed.begin(), removed.end());
    std::sort(added.begin(), added.end());
    if (placementsOf(changes, true) != removed || placementsOf(changes, false) != added) {
        std::cerr << fen << " " << to_string(move) << ": removed";
        for (const auto& name : placementsOf(changes, true)) std::cerr << " " << name;
        std::cerr << ", added";
        for (const auto& name : placementsOf(changes, false)) std::cerr << " " << name;
        std::cerr << "\n";
    }
    assert(placementsOf(changes, true) == removed);
    assert(placementsOf(changes, false) == added);
}

/** Every kind of move, in terms of the pieces it moves rather than of its kind. */
void testPieceChanges() {
    const std::string start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    expectChanges(start, {g1, f3, MoveKind::Quiet_Move}, {"Ng1"}, {"Nf3"});
    expectChanges(start, {e2, e4, MoveKind::Double_Push}, {"Pe2"}, {"Pe4"});

    // Kiwipete: captures on both sides, and castling either way.
    const std::string kiwi = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1";
    expectChanges(kiwi, {e5, g6, MoveKind::Capture}, {"Ne5", "pg6"}, {"Ng6"});
    expectChanges(kiwi, {e1, g1, MoveKind::O_O}, {"Ke1", "Rh1"}, {"Kg1", "Rf1"});
    expectChanges(kiwi, {e1, c1, MoveKind::O_O_O}, {"Ke1", "Ra1"}, {"Kc1", "Rd1"});
    expectChanges(kiwi, {e8, g8, MoveKind::O_O}, {"ke8", "rh8"}, {"kg8", "rf8"});

    // En passant takes a pawn off a square the capturing pawn never comes to rest on.
    const std::string ep = "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1";
    expectChanges(ep, {e5, d6, MoveKind::En_Passant}, {"Pe5", "pd5"}, {"Pd6"});
    const std::string epBlack = "4k3/8/8/8/4pP2/8/8/4K3 b - f3 0 1";
    expectChanges(epBlack, {e4, f3, MoveKind::En_Passant}, {"pe4", "Pf4"}, {"pf3"});

    // A promotion lands a piece that never left, capture or no capture.
    const std::string promo = "3r1n2/4P3/8/8/8/8/8/k3K3 w - - 0 1";
    expectChanges(promo, {e7, e8, MoveKind::Queen_Promotion}, {"Pe7"}, {"Qe8"});
    expectChanges(promo, {e7, d8, MoveKind::Rook_Promotion_Capture}, {"Pe7", "rd8"}, {"Rd8"});
    expectChanges(promo, {e7, f8, MoveKind::Knight_Promotion_Capture}, {"Pe7", "nf8"}, {"Nf8"});

    std::cout << "  every kind of move names the pieces it moves\n";
}

/**
 * Walk every legal move to `depth`, keeping `stack` in step, and check it at every position.
 *
 * Two claims, and they are not the same one. First, an incrementally maintained accumulator equals
 * a fresh one, which is what makes the evaluation below it unchanged. Second, it still does after
 * the moves made from it have been unmade, which is what makes a stack the right shape for a
 * search: the entry a node comes back to must be the one it built on the way down.
 */
size_t walkPositions(const nnue::Network& network, Position& position,
                     AccumulatorStack& stack, int depth) {
    const auto& transformer = network.transformer;
    assert(stack.top() == refreshBoth(transformer, position));
    assert(evaluate(network, position, stack.top()) == evaluate(network, position));

    size_t nodes = 1;
    if (depth) {
        for (auto move : moves::allLegalMovesAndCaptures(position.turn, position.board)) {
            auto change = moves::prepareMove(position.board, move);
            auto changes = pieceChanges(position.board, change);
            auto undo = moves::makeMove(position, change, move);

            stack.push(transformer, position, changes);
            nodes += walkPositions(network, position, stack, depth - 1);
            stack.pop();

            moves::unmakeMove(position, undo);
        }
    }

    assert(stack.top() == refreshBoth(transformer, position) &&
           "unmaking the moves made from a position must uncover its accumulators intact");
    return nodes;
}

void testIncrementalAccumulators(const nnue::Network& network) {
    testPieceChanges();

    // An inactive stack is what every caller outside a search sees, and it must cost nothing.
    AccumulatorStack stack;
    assert(!stack.active() && stack.size() == 0);
    stack.pop();
    assert(!stack.active() && "popping an inactive stack does nothing");

    // Positions chosen for the moves they allow: castling and captures, en passant on both sides,
    // and promotions, all of which are the compound moves an update rule can get wrong.
    const std::string fens[] = {
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "8/PPPk4/8/8/8/8/4Kppp/8 w - - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    };

    size_t nodes = 0;
    for (const auto& fen : fens) {
        auto position = fen::parsePosition(fen);
        stack.reset(network.transformer, position);
        assert(stack.active() && stack.size() == 1);
        nodes += walkPositions(network, position, stack, 2);
        assert(stack.size() == 1 && "the walk must leave the root, and only the root, behind");
    }
    std::cout << "  " << nodes
              << " positions keep their accumulators exactly through make/unmake\n";

    stack.clear();
    assert(!stack.active() && "a cleared stack is inactive again");

    // Not a check, just the number phase 9 exists for: what carrying an accumulator across one
    // move costs against building it from scratch, both for the same position. A debug build
    // refreshes inside every push to check it, so there the two would be timing the same work.
    if (debug) return;

    auto position = fen::parsePosition(fens[0]);
    auto change = moves::prepareMove(position.board, Move{e5, g6, MoveKind::Capture});
    auto changes = pieceChanges(position.board, change);
    stack.reset(network.transformer, position);
    moves::makeMove(position, change, Move{e5, g6, MoveKind::Capture});

    constexpr int kRepeats = 1000;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kRepeats; ++i) {
        stack.push(network.transformer, position, changes);
        stack.pop();
    }
    auto incremental = std::chrono::steady_clock::now() - start;

    start = std::chrono::steady_clock::now();
    for (int i = 0; i < kRepeats; ++i) {
        stack.push(network.transformer, position);
        stack.pop();
    }
    auto fresh = std::chrono::steady_clock::now() - start;

    auto nanos = [](auto elapsed) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / kRepeats;
    };
    std::cout << "  both perspectives of a capture: " << nanos(incremental)
              << " ns incrementally against " << nanos(fresh) << " ns from scratch\n";
}

/** Print, rather than check, what the network makes of a position: the --verbose mode. */
void printEvaluations(const std::string& filename, const std::vector<std::string>& fens) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open NNUE file: " + filename);
    auto network = nnue::readNetwork(file);

    for (const auto& fen : fens) {
        auto position = fen::parsePosition(fen);
        Evaluation trace;
        evaluateValue(network, position, &trace);
        std::cout << fen << "\n  " << trace.pieceCount << " pieces, bucket " << trace.bucket
                  << ": psqt " << trace.psqt << " + positional " << trace.positional << " = value "
                  << trace.value << ", " << evaluate(network, position) << " cp for white\n";
    }
}

/** A position without a king of the perspective's color cannot be described at all. */
void testMissingKing() {
    Position position;
    position.board[e2] = Piece::P;
    position.board[e8] = Piece::k;
    try {
        activeFeatures(position, Color::w);
    } catch (const std::runtime_error& e) {
        std::cout << "Rejected position without a white king: " << e.what() << "\n";
        assert(activeFeatures(position, Color::b).size == 2 && "black is still fine");
        return;
    }
    assert(false && "position without a white king accepted");
}

}  // namespace

int main(int argc, char* argv[]) try {
    testSyntheticHeader();
    testMalformedHeaders();
    testLeb128();
    testSyntheticNetwork();

    testKingsOnly();
    testPawnAndKings();
    testHandComputedIndices();
    testPieceCategories();
    testKingBuckets();
    testMirrorInvariance();
    testFeatureRanges();
    testMissingKing();

    testFreshAccumulation();
    testTransformArithmetic();
    testAffineForward();
    testActivations();
    testSyntheticStack();
    testMaterialBuckets();

    // Remaining arguments name network files, except that "--verbose <fen>" asks for one
    // position to be evaluated and printed rather than checked, which is how an arbitrary
    // position is compared against Stockfish by hand.
    std::vector<std::string> files, fens;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg != "--verbose")
            files.push_back(arg);
        else if (++i < argc)
            fens.push_back(argv[i]);
        else
            throw std::runtime_error("--verbose needs a FEN");
    }
    if (files.empty()) files.push_back(kBigNetworkFile);

    for (const auto& file : files) testNetworkFile(file);
    if (!fens.empty()) printEvaluations(files.front(), fens);

    std::cout << "All NNUE tests passed!\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "NNUE test failed: " << e.what() << "\n";
    return 1;
}

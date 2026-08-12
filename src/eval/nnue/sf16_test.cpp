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
#include "eval/nnue/sf16.h"

// A network is far too large to copy by accident, so it may only be moved.
static_assert(!std::is_copy_constructible_v<nnue::sf16::Network>);
static_assert(!std::is_copy_assignable_v<nnue::sf16::Network>);
static_assert(std::is_move_constructible_v<nnue::sf16::Network>);

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
        nnue::sf16::FileHeader::kVersion, nnue::sf16::kBigArchitecture.hash(), desc.size(), desc);
}

/** Check that reading the given bytes is rejected, and report the diagnostic it produced. */
void expectRejected(const std::string& what, const std::string& bytes) {
    std::istringstream in(bytes);
    try {
        nnue::sf16::readFileHeader(in);
    } catch (const std::runtime_error& e) {
        std::cout << "Rejected " << what << ": " << e.what() << "\n";
        return;
    }
    std::cerr << "Accepted " << what << ", but it should have been rejected\n";
    assert(false && "malformed header accepted");
}

void testSyntheticHeader() {
    std::istringstream in(goodHeader());
    auto header = nnue::sf16::readFileHeader(in);
    assert(header.version == nnue::sf16::FileHeader::kVersion);
    assert(header.hash == nnue::sf16::kBigArchitecture.hash());
    assert(header.description == "test network");

    // Nothing beyond the header is consumed, so the rest of the file is left for the caller.
    std::istringstream trailing(goodHeader() + "parameters");
    nnue::sf16::readFileHeader(trailing);
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
    auto hash = nnue::sf16::kBigArchitecture.hash();
    expectRejected("SF12 version", makeHeader(0x7af32f16u, hash, desc.size(), desc));
    expectRejected("byte swapped version",
                   makeHeader(0x202ff37au, hash, desc.size(), desc));  // catches endianness slips
    expectRejected("wrong architecture hash",
                   makeHeader(nnue::sf16::FileHeader::kVersion, hash ^ 1, desc.size(), desc));
    expectRejected("differently sized accumulator",
                   makeHeader(nnue::sf16::FileHeader::kVersion,
                              nnue::sf16::Architecture{1024, 15, 32}.hash(),
                              desc.size(),
                              desc));
    expectRejected("empty description", makeHeader(nnue::sf16::FileHeader::kVersion, hash, 0, ""));
    expectRejected("absurd description length",
                   makeHeader(nnue::sf16::FileHeader::kVersion, hash, ~0u, desc));
    expectRejected("binary description",
                   makeHeader(nnue::sf16::FileHeader::kVersion, hash, 4, std::string("a\0bc", 4)));
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
    std::string bytes(nnue::sf16::kLeb128Magic);
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
    nnue::sf16::readLeb128(in, values.data(), count);
    assert(in.peek() == std::istringstream::traits_type::eof());

    return values;
}

/** Check that decoding `count` values from the given bytes fails. */
template <typename Int>
void expectBlockRejected(const std::string& what, const std::string& block, size_t count) {
    std::istringstream in(block);
    std::vector<Int> values(count + 1);  // a slot to spare, so an overrun corrupts nothing
    try {
        nnue::sf16::readLeb128(in, values.data(), count);
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
constexpr nnue::sf16::Architecture kTinyArchitecture = {32, 15, 32, 64};

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
    net.bytes = makeHeader(nnue::sf16::FileHeader::kVersion, arch.hash(), desc.size(), desc);

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
void checkLayerShape(const nnue::sf16::AffineLayer& layer, uint32_t inputs, uint32_t outputs) {
    assert(layer.inputs == inputs);
    assert(layer.paddedInputs == nnue::sf16::Architecture::padded(inputs));
    assert(layer.outputs == outputs);
    assert(layer.biases.size() == outputs);
    assert(layer.weights.size() == size_t(outputs) * layer.paddedInputs);
}

/** Check that every parameter array of a network has exactly the size its architecture implies. */
void checkNetworkShape(const nnue::sf16::Network& network) {
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
        nnue::sf16::readNetwork(in, kTinyArchitecture);
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
    auto network = nnue::sf16::readNetwork(in, kTinyArchitecture);
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
void testGoldenTransforms(const nnue::sf16::Network& network);

void testNetworkFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open NNUE file: " + filename);
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0);

    auto network = nnue::sf16::readNetwork(file);
    std::cout << "Read " << filename << ": " << network.header.description << "\n";

    size_t parameters = network.transformer.biases.size() + network.transformer.weights.size() +
                        network.transformer.psqtWeights.size();
    for (const auto& stack : network.stacks)
        for (const auto* layer : {&stack.fc0, &stack.fc1, &stack.fc2})
            parameters += layer->biases.size() + layer->weights.size();

    std::cout << "  " << parameters << " parameters from " << fileSize << " bytes, ending at EOF\n";
    assert(parameters == 58190984);  // the whole Big network, transformer and all eight stacks

    assert(network.header.version == nnue::sf16::FileHeader::kVersion);
    assert(network.header.hash == 0x1c103072u);  // published hash of the SF16.1 Big network
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

using nnue::sf16::activeFeatures;
using nnue::sf16::featureIndex;
using nnue::sf16::kBucketStride;
using nnue::sf16::kKingBuckets;
using nnue::sf16::kPieceCategories;

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

/** Kings are active features in HalfKAv2_hm, unlike in the HalfKP feature set of SF12. */
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
            assert(features.size <= nnue::sf16::ActiveFeatures::kMaxSize);
            for (auto index : features)
                assert(index < nnue::sf16::Architecture::kInputDimensions &&
                       "every feature must address a row of the transformer");
        }
    }
}

// ---------------------------------------------------------------------------------------------
// Feature transform
// ---------------------------------------------------------------------------------------------

using nnue::sf16::Accumulator;
using nnue::sf16::refresh;
using nnue::sf16::transform;

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
nnue::sf16::FeatureTransformer handTransformer() {
    nnue::sf16::FeatureTransformer transformer;
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

    nnue::sf16::ActiveFeatures features;
    features.add(0);
    features.add(2);
    auto accumulator = refresh(transformer, features);
    assert(accumulator.values == std::vector<int16_t>({111, 122, 133, 144}));
    assert((accumulator.psqt == std::array<int32_t, 8>({11, 22, 33, 44, 55, 66, 77, 88})));

    // Accumulation is a plain sum, so the same feature twice counts twice and order is immaterial.
    nnue::sf16::ActiveFeatures twice;
    twice.add(1);
    twice.add(1);
    assert(refresh(transformer, twice).values == std::vector<int16_t>({-1, -2, -3, -4}));

    nnue::sf16::ActiveFeatures reversed;
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
    for (uint32_t bucket = 0; bucket < nnue::sf16::Architecture::kPSQTBuckets; ++bucket)
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
    int32_t psqt[nnue::sf16::Architecture::kPSQTBuckets];
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
void testGoldenTransforms(const nnue::sf16::Network& network) {
    auto l1 = network.arch.l1;

    for (const auto& golden : kGoldenTransforms) {
        auto position = fen::parsePosition(golden.fen);
        auto white = refresh(network.transformer, position, Color::w);
        auto black = refresh(network.transformer, position, Color::b);
        assert(white.values.size() == l1 && black.values.size() == l1);

        for (uint32_t bucket = 0; bucket < nnue::sf16::Architecture::kPSQTBuckets; ++bucket) {
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

    if (argc <= 1)
        testNetworkFile(kBigNetworkFile);
    else
        while (++argv, --argc) testNetworkFile(*argv);

    std::cout << "All SF16 NNUE tests passed!\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "SF16 NNUE test failed: " << e.what() << "\n";
    return 1;
}

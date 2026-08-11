#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "core/core.h"
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

    // The header alone still has to be rejected when cut short, as before.
    file.clear();
    file.seekg(0);
    std::string prefix(network.header.description.size() + 11, '\0');
    file.read(prefix.data(), prefix.size());
    assert(file && "network file is too short to hold its own header");
    expectRejected("truncated " + filename, prefix);
}

}  // namespace

int main(int argc, char* argv[]) try {
    testSyntheticHeader();
    testMalformedHeaders();
    testLeb128();
    testSyntheticNetwork();

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

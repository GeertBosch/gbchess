#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#include "eval/nnue/sf16.h"

namespace nnue::sf16 {

namespace {

std::string toHex(uint32_t value) {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return ss.str();
}

/** Read a 32-bit little endian value, independent of the host byte order. */
uint32_t readUint32(std::istream& in, const std::string& what) {
    unsigned char bytes[4];
    in.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (!in) throw std::runtime_error("Truncated SF16 NNUE file: no " + what);

    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i) value |= uint32_t(bytes[i]) << (8 * i);

    return value;
}

/** Read `count` little endian values of an integer type, as the uncompressed blocks store them. */
template <typename Int>
void readLittleEndian(std::istream& in, Int* out, size_t count, const std::string& what) {
    auto bytes = reinterpret_cast<unsigned char*>(out);
    in.read(reinterpret_cast<char*>(bytes), count * sizeof(Int));
    if (!in) throw std::runtime_error("Truncated SF16 NNUE network: " + what + " cut short");

    // Assemble each value from its own bytes in place, so the result does not depend on host order.
    for (size_t i = 0; i < count; ++i) {
        std::make_unsigned_t<Int> value = 0;
        for (size_t byte = sizeof(Int); byte--;)
            value = std::make_unsigned_t<Int>(value << 8) | bytes[i * sizeof(Int) + byte];

        out[i] = Int(value);
    }
}

/**
 * Byte source for the body of a compressed block, refilling from the stream as needed.
 * Reading past the byte count the block declared is an error rather than a read of the bytes
 * that follow it, so a corrupt block cannot silently eat the next one.
 */
class BlockReader {
public:
    BlockReader(std::istream& in, uint32_t size) : in(in), left(size) {}

    uint8_t next() {
        if (!left) throw std::runtime_error("Malformed SF16 NNUE block: ran past its declared size");
        if (pos == filled) {
            filled = std::min<uint32_t>(left, sizeof(buffer));
            in.read(buffer, filled);
            if (!in) throw std::runtime_error("Truncated SF16 NNUE block");
            pos = 0;
        }
        --left;
        return uint8_t(buffer[pos++]);
    }

    /** Bytes of the block not yet consumed, which must be zero once it is fully decoded. */
    uint32_t remaining() const { return left; }

private:
    std::istream& in;
    uint32_t left;
    char buffer[4096];
    uint32_t filled = 0;
    uint32_t pos = 0;
};

/** Decode one signed LEB128 value, sign extending the last group of bits as the format requires. */
template <typename Int>
Int decodeLeb128(BlockReader& block) {
    static_assert(std::is_signed_v<Int>, "signed LEB128 decodes to a signed type");
    constexpr size_t kBits = 8 * sizeof(Int);
    using Unsigned = std::make_unsigned_t<Int>;

    Unsigned value = 0;
    for (size_t shift = 0; shift < kBits; shift += 7) {
        uint8_t byte = block.next();
        value |= Unsigned(Unsigned(byte & 0x7f) << shift);
        if (byte & 0x80) continue;

        // The value ends here. Its sign lives in bit 6 of the final byte, unless that byte
        // carried the topmost bits of the result, in which case the sign is already in place.
        // Build the mask in uint64_t: for narrow Int, ~Unsigned(0) promotes to a negative int
        // and shifting that is undefined.
        if (shift + 7 < kBits && (byte & 0x40)) value |= Unsigned(~uint64_t(0) << (shift + 7));

        return Int(value);
    }
    throw std::runtime_error("Malformed SF16 NNUE block: LEB128 value too long for its type");
}

bool isPrintableAscii(const std::string& str) {
    for (char c : str)
        if (c < 32 || c > 126) return false;

    return true;
}

}  // namespace

template <typename Int>
void readLeb128(std::istream& in, Int* out, size_t count) {
    constexpr size_t kMagicSize = sizeof(kLeb128Magic) - 1;

    char magic[kMagicSize];
    in.read(magic, kMagicSize);
    if (!in) throw std::runtime_error("Truncated SF16 NNUE block: no compression marker");
    if (std::memcmp(magic, kLeb128Magic, kMagicSize) != 0)
        throw std::runtime_error("SF16 NNUE parameters are not LEB128 compressed");

    BlockReader block(in, readUint32(in, "compressed block size"));
    for (size_t i = 0; i < count; ++i) out[i] = decodeLeb128<Int>(block);

    if (block.remaining())
        throw std::runtime_error("Malformed SF16 NNUE block: " +
                                 std::to_string(block.remaining()) + " bytes left after " +
                                 std::to_string(count) + " values");
}

template void readLeb128<int16_t>(std::istream&, int16_t*, size_t);
template void readLeb128<int32_t>(std::istream&, int32_t*, size_t);

FileHeader readFileHeader(std::istream& in, const Architecture& arch) {
    FileHeader header;

    header.version = readUint32(in, "version");
    header.hash = readUint32(in, "architecture hash");
    auto length = readUint32(in, "description length");

    if (header.version != FileHeader::kVersion)
        throw std::runtime_error("Unsupported SF16 NNUE version: " + toHex(header.version) +
                                 ", expected " + toHex(FileHeader::kVersion));
    if (header.hash != arch.hash())
        throw std::runtime_error("Unsupported SF16 NNUE architecture: " + toHex(header.hash) +
                                 ", expected " + toHex(arch.hash()));
    if (!length || length > FileHeader::kMaxDescriptionLength)
        throw std::runtime_error("Implausible SF16 NNUE description length: " +
                                 std::to_string(length));

    header.description.resize(length);
    in.read(header.description.data(), length);
    if (!in) throw std::runtime_error("Truncated SF16 NNUE header: description cut short");
    if (!isPrintableAscii(header.description))
        throw std::runtime_error("Invalid SF16 NNUE description (not printable ASCII)");

    return header;
}

namespace {

/** Check the structure hash that precedes each parameter block, naming it in the diagnostic. */
void expectStructureHash(std::istream& in, uint32_t expected, const std::string& what) {
    auto hash = readUint32(in, "structure hash of " + what);
    if (hash != expected)
        throw std::runtime_error("Unexpected SF16 NNUE structure hash for " + what + ": " +
                                 toHex(hash) + ", expected " + toHex(expected));
}

FeatureTransformer readFeatureTransformer(std::istream& in, const Architecture& arch) {
    FeatureTransformer transformer;

    transformer.biases.resize(arch.l1);
    readLeb128(in, transformer.biases.data(), transformer.biases.size());

    transformer.weights.resize(size_t(arch.inputDimensions) * arch.l1);
    readLeb128(in, transformer.weights.data(), transformer.weights.size());

    transformer.psqtWeights.resize(size_t(arch.inputDimensions) * Architecture::kPSQTBuckets);
    readLeb128(in, transformer.psqtWeights.data(), transformer.psqtWeights.size());

    return transformer;
}

AffineLayer readAffineLayer(std::istream& in, uint32_t inputs, uint32_t outputs,
                            const std::string& what) {
    AffineLayer layer;
    layer.inputs = inputs;
    layer.paddedInputs = Architecture::padded(inputs);
    layer.outputs = outputs;

    layer.biases.resize(outputs);
    readLittleEndian(in, layer.biases.data(), layer.biases.size(), what + " biases");

    layer.weights.resize(size_t(outputs) * layer.paddedInputs);
    readLittleEndian(in, layer.weights.data(), layer.weights.size(), what + " weights");

    return layer;
}

LayerStack readLayerStack(std::istream& in, const Architecture& arch, const std::string& what) {
    expectStructureHash(in, arch.networkHash(), what);

    LayerStack stack;
    stack.fc0 = readAffineLayer(in, arch.l1, arch.l2 + 1, what + " fc0");
    stack.fc1 = readAffineLayer(in, 2 * arch.l2, arch.l3, what + " fc1");
    stack.fc2 = readAffineLayer(in, arch.l3, 1, what + " fc2");

    return stack;
}

}  // namespace

Network readNetwork(std::istream& in, const Architecture& arch) {
    Network network;
    network.arch = arch;
    network.header = readFileHeader(in, arch);

    expectStructureHash(in, arch.featureTransformerHash(), "the feature transformer");
    network.transformer = readFeatureTransformer(in, arch);

    for (size_t i = 0; i < network.stacks.size(); ++i)
        network.stacks[i] = readLayerStack(in, arch, "layer stack " + std::to_string(i));

    // Stockfish likewise insists on reaching end of file here: anything left over means we have
    // misread the file, or it holds something we do not understand.
    if (in.peek() != std::istream::traits_type::eof())
        throw std::runtime_error("Trailing bytes after the last SF16 NNUE layer stack");

    return network;
}

namespace {

/**
 * Piece category of `piece` seen from `perspective`, in units of squares. The five non-king types
 * take two categories each, the perspective side's own first, and both kings share the last one.
 */
constexpr uint16_t pieceCategory(Piece piece, Color perspective) {
    if (type(piece) == PieceType::KING) return kPieceCategories - 1;

    return uint16_t(2 * index(type(piece)) + (color(piece) != perspective));
}

/**
 * Orient `square` for `perspective`, whose own king stands on `kingSquare`. Squares are rank
 * major, so flipping all ranks is xor 56 and flipping all files is xor 7.
 */
constexpr Square orient(Square square, Square kingSquare, Color perspective) {
    int flip = perspective == Color::w ? 0 : kNumSquares - kNumFiles;  // black plays up the board
    if (file(kingSquare) < kNumFiles / 2) flip ^= kNumFiles - 1;       // mirror onto files e to h

    return Square(square ^ flip);
}

/**
 * Bucket of an already oriented king square, which by construction lies on files e to h. Buckets
 * run four to a rank from 0 at h8 to 31 at e1, so a king that has not moved is in the last one.
 */
constexpr uint16_t kingBucket(Square orientedKing) {
    dassert(file(orientedKing) >= kNumFiles / 2);

    return uint16_t((kNumRanks - 1 - rank(orientedKing)) * (kNumFiles / 2) +
                    (kNumFiles - 1 - file(orientedKing)));
}

/** The square of `color`'s king, which a position described by this feature set must have. */
Square kingSquare(const Board& board, Color color) {
    auto king = addColor(PieceType::KING, color);
    for (Square square : squares)
        if (board[square] == king) return square;

    throw std::runtime_error("Position has no " + to_string(color) + " king");
}

}  // namespace

uint16_t featureIndex(Square square, Piece piece, Square king, Color perspective) {
    auto bucket = kingBucket(orient(king, king, perspective));

    return uint16_t(bucket * kBucketStride + pieceCategory(piece, perspective) * kNumSquares +
                    orient(square, king, perspective));
}

ActiveFeatures activeFeatures(const Position& position, Color perspective) {
    const auto& board = position.board;
    auto king = kingSquare(board, perspective);

    ActiveFeatures features;
    for (Square square : squares)
        if (board[square] != Piece::_)
            features.add(featureIndex(square, board[square], king, perspective));

    return features;
}

Accumulator refresh(const FeatureTransformer& transformer, const ActiveFeatures& features) {
    auto l1 = transformer.biases.size();
    dassert(l1 && transformer.weights.size() % l1 == 0);

    Accumulator accumulator;
    accumulator.values = transformer.biases;

    for (auto feature : features) {
        dassert((size_t(feature) + 1) * l1 <= transformer.weights.size());
        const auto* weights = transformer.weights.data() + size_t(feature) * l1;

        // Accumulator entries stay 16 bit, wrapping around rather than saturating if a position
        // ever managed to overflow one. That is what Stockfish's packed 16 bit adds do, and it is
        // the behavior the network was trained against, so we must not widen the sum here.
        for (size_t i = 0; i < l1; ++i)
            accumulator.values[i] = int16_t(uint16_t(accumulator.values[i]) + uint16_t(weights[i]));

        const auto* psqt =
            transformer.psqtWeights.data() + size_t(feature) * Architecture::kPSQTBuckets;
        for (size_t bucket = 0; bucket < Architecture::kPSQTBuckets; ++bucket)
            accumulator.psqt[bucket] += psqt[bucket];
    }

    return accumulator;
}

Accumulator refresh(const FeatureTransformer& transformer, const Position& position,
                    Color perspective) {
    return refresh(transformer, activeFeatures(position, perspective));
}

Transformed transform(const Accumulator& white, const Accumulator& black, Color sideToMove,
                      uint32_t bucket) {
    dassert(bucket < Architecture::kPSQTBuckets);
    dassert(white.values.size() == black.values.size());

    // The side to move comes first, and is the only asymmetry between the two perspectives.
    const Accumulator* perspectives[2] = {&white, &black};
    if (sideToMove == Color::b) std::swap(perspectives[0], perspectives[1]);

    auto l1 = perspectives[0]->values.size();
    dassert(l1 % 2 == 0);
    auto half = l1 / 2;

    Transformed transformed;
    transformed.psqt = (perspectives[0]->psqt[bucket] - perspectives[1]->psqt[bucket]) / 2;
    transformed.features.resize(l1);

    for (size_t p = 0; p < 2; ++p) {
        const auto& values = perspectives[p]->values;
        for (size_t j = 0; j < half; ++j) {
            // Pairwise multiplication of the two halves, each clipped to a byte first. The product
            // of two clipped values needs 14 bits, so scaling it back down by 128 leaves a byte.
            int first = std::clamp<int>(values[j], 0, 127);
            int second = std::clamp<int>(values[j + half], 0, 127);
            transformed.features[p * half + j] = uint8_t(first * second / 128);
        }
    }

    return transformed;
}

Transformed transform(const FeatureTransformer& transformer, const Position& position,
                      uint32_t bucket) {
    return transform(refresh(transformer, position, Color::w),
                     refresh(transformer, position, Color::b),
                     position.active(),
                     bucket);
}

}  // namespace nnue::sf16

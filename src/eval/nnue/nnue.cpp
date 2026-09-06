#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#include "core/sse2.h"
#include "eval/nnue/nnue.h"

namespace nnue {

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
    if (!in) throw std::runtime_error("Truncated NNUE file: no " + what);

    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i) value |= uint32_t(bytes[i]) << (8 * i);

    return value;
}

/** Read `count` little endian values of an integer type, as the uncompressed blocks store them. */
template <typename Int>
void readLittleEndian(std::istream& in, Int* out, size_t count, const std::string& what) {
    auto bytes = reinterpret_cast<unsigned char*>(out);
    in.read(reinterpret_cast<char*>(bytes), count * sizeof(Int));
    if (!in) throw std::runtime_error("Truncated NNUE network: " + what + " cut short");

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
        if (!left) throw std::runtime_error("Malformed NNUE block: ran past its declared size");
        if (pos == filled) {
            filled = std::min<uint32_t>(left, sizeof(buffer));
            in.read(buffer, filled);
            if (!in) throw std::runtime_error("Truncated NNUE block");
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
    throw std::runtime_error("Malformed NNUE block: LEB128 value too long for its type");
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
    if (!in) throw std::runtime_error("Truncated NNUE block: no compression marker");
    if (std::memcmp(magic, kLeb128Magic, kMagicSize) != 0)
        throw std::runtime_error("NNUE parameters are not LEB128 compressed");

    BlockReader block(in, readUint32(in, "compressed block size"));
    for (size_t i = 0; i < count; ++i) out[i] = decodeLeb128<Int>(block);

    if (block.remaining())
        throw std::runtime_error("Malformed NNUE block: " +
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
        throw std::runtime_error("Unsupported NNUE version: " + toHex(header.version) +
                                 ", expected " + toHex(FileHeader::kVersion));
    if (header.hash != arch.hash())
        throw std::runtime_error("Unsupported NNUE architecture: " + toHex(header.hash) +
                                 ", expected " + toHex(arch.hash()));
    if (!length || length > FileHeader::kMaxDescriptionLength)
        throw std::runtime_error("Implausible NNUE description length: " +
                                 std::to_string(length));

    header.description.resize(length);
    in.read(header.description.data(), length);
    if (!in) throw std::runtime_error("Truncated NNUE header: description cut short");
    if (!isPrintableAscii(header.description))
        throw std::runtime_error("Invalid NNUE description (not printable ASCII)");

    return header;
}

namespace {

/** Check the structure hash that precedes each parameter block, naming it in the diagnostic. */
void expectStructureHash(std::istream& in, uint32_t expected, const std::string& what) {
    auto hash = readUint32(in, "structure hash of " + what);
    if (hash != expected)
        throw std::runtime_error("Unexpected NNUE structure hash for " + what + ": " +
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
    stack.fc0Columns = transpose(stack.fc0);

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
        throw std::runtime_error("Trailing bytes after the last NNUE layer stack");

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

/**
 * Add one feature's transformer row and PSQT contribution to `accumulator`, in place.
 *
 * Accumulator entries stay 16 bit, wrapping around rather than saturating if a position ever
 * managed to overflow one. That is what Stockfish's packed 16 bit adds do, and it is the behavior
 * the network was trained against, so we must not widen the sum here. It is also why subtracting
 * a row undoes adding it exactly, which is what makes an incremental update sound at all.
 */
void addFeature(Accumulator& accumulator, const FeatureTransformer& transformer,
                uint16_t feature) {
    auto l1 = accumulator.values.size();
    dassert((size_t(feature) + 1) * l1 <= transformer.weights.size());
    const auto* weights = transformer.weights.data() + size_t(feature) * l1;

    for (size_t i = 0; i < l1; ++i) {
        auto value = uint16_t(accumulator.values[i]);
        accumulator.values[i] = int16_t(uint16_t(value + uint16_t(weights[i])));
    }

    const auto* psqt =
        transformer.psqtWeights.data() + size_t(feature) * Architecture::kPSQTBuckets;
    for (size_t bucket = 0; bucket < Architecture::kPSQTBuckets; ++bucket)
        accumulator.psqt[bucket] += psqt[bucket];
}

/**
 * Rebuild `accumulator` from the transformer's biases and `features`, reusing whatever storage it
 * already holds. A stack that refreshes a perspective every time a king moves does that often
 * enough for the allocation a fresh Accumulator would need to be worth not making.
 */
void accumulate(Accumulator& accumulator, const FeatureTransformer& transformer,
                const ActiveFeatures& features) {
    dassert(transformer.biases.size() &&
            transformer.weights.size() % transformer.biases.size() == 0);

    accumulator.values.assign(transformer.biases.begin(), transformer.biases.end());
    accumulator.psqt = {};
    for (auto feature : features) addFeature(accumulator, transformer, feature);
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
    Accumulator accumulator;
    accumulate(accumulator, transformer, features);
    return accumulator;
}

Accumulator refresh(const FeatureTransformer& transformer, const Position& position,
                    Color perspective) {
    return refresh(transformer, activeFeatures(position, perspective));
}

namespace {

/** Rebuild one perspective of `position` in place, as refresh() does but without allocating. */
void refreshInto(Accumulator& accumulator, const FeatureTransformer& transformer,
                 const Position& position, Color perspective) {
    accumulate(accumulator, transformer, activeFeatures(position, perspective));
}

/**
 * Write `src` plus the `added` rows less the `removed` rows into `dst`, over `count` values.
 *
 * One pass over the accumulator rather than one pass per row, which is the whole point of writing
 * it this way. A move touches at most five rows and an accumulator is 5KB, so applying the rows
 * one after another reads and writes that 5KB once per row; reading each row once and writing the
 * result once does the same arithmetic with a fraction of the memory traffic.
 *
 * The row counts are compile time constants so that the body is one straight line a compiler
 * vectorizes as it stands, which is why the caller dispatches on the shape of the move rather
 * than passing counts. Arithmetic stays 16 bit and wraps, as addFeature explains.
 */
template <size_t kAdded, size_t kRemoved>
void combineRows(int16_t* __restrict dst, const int16_t* __restrict src,
                 const int16_t* const* added, const int16_t* const* removed, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        auto value = uint16_t(src[i]);
        for (size_t a = 0; a < kAdded; ++a) value = uint16_t(value + uint16_t(added[a][i]));
        for (size_t r = 0; r < kRemoved; ++r) value = uint16_t(value - uint16_t(removed[r][i]));
        dst[i] = int16_t(value);
    }
}

/**
 * Carry `src` across one move into `dst`, for a perspective whose own king did not move and so
 * stands on `king` both before and after it. `dst` may hold anything: every value is written.
 *
 * Removals come first only for readability; the rows commute, as int16 addition does.
 */
void updatePerspective(Accumulator& dst, const Accumulator& src,
                       const FeatureTransformer& transformer, const PieceChanges& changes,
                       Square king, Color perspective) {
    auto l1 = src.values.size();
    dassert(l1 && transformer.weights.size() % l1 == 0);

    // Resolve the features into the rows they select before touching an accumulator, so that the
    // loop below is arithmetic and nothing else.
    const int16_t* added[std::tuple_size_v<decltype(changes.added)>];
    const int16_t* removed[std::tuple_size_v<decltype(changes.removed)>];
    auto row = [&](const PieceChanges::Placement& placement) {
        auto feature = featureIndex(placement.square, placement.piece, king, perspective);
        dassert((size_t(feature) + 1) * l1 <= transformer.weights.size());
        return transformer.weights.data() + size_t(feature) * l1;
    };
    auto psqtRow = [&](const PieceChanges::Placement& placement) {
        auto feature = featureIndex(placement.square, placement.piece, king, perspective);
        return transformer.psqtWeights.data() + size_t(feature) * Architecture::kPSQTBuckets;
    };

    for (uint8_t i = 0; i < changes.addedCount; ++i) added[i] = row(changes.added[i]);
    for (uint8_t i = 0; i < changes.removedCount; ++i) removed[i] = row(changes.removed[i]);

    dst.psqt = src.psqt;
    for (uint8_t i = 0; i < changes.addedCount; ++i) {
        const auto* psqt = psqtRow(changes.added[i]);
        for (size_t bucket = 0; bucket < Architecture::kPSQTBuckets; ++bucket)
            dst.psqt[bucket] += psqt[bucket];
    }
    for (uint8_t i = 0; i < changes.removedCount; ++i) {
        const auto* psqt = psqtRow(changes.removed[i]);
        for (size_t bucket = 0; bucket < Architecture::kPSQTBuckets; ++bucket)
            dst.psqt[bucket] -= psqt[bucket];
    }

    dst.values.resize(l1);
    int16_t* out = dst.values.data();
    const int16_t* in = src.values.data();

    // The shapes a legal move can have: a quiet move adds one row and removes one, a capture or
    // an en passant removes a second, and castling - which only reaches here from the perspective
    // whose king did not castle - moves a rook as well.
    switch (changes.addedCount * 4 + changes.removedCount) {
    case 1 * 4 + 1: combineRows<1, 1>(out, in, added, removed, l1); break;
    case 1 * 4 + 2: combineRows<1, 2>(out, in, added, removed, l1); break;
    case 2 * 4 + 1: combineRows<2, 1>(out, in, added, removed, l1); break;
    case 2 * 4 + 2: combineRows<2, 2>(out, in, added, removed, l1); break;
    default:
        // Nothing a chess move does, but the arithmetic is defined for any shape and a wrong
        // answer here would be a silent one.
        dst.values.assign(src.values.begin(), src.values.end());
        for (uint8_t i = 0; i < changes.removedCount; ++i)
            for (size_t j = 0; j < l1; ++j)
                dst.values[j] = int16_t(uint16_t(dst.values[j]) - uint16_t(removed[i][j]));
        for (uint8_t i = 0; i < changes.addedCount; ++i)
            for (size_t j = 0; j < l1; ++j)
                dst.values[j] = int16_t(uint16_t(dst.values[j]) + uint16_t(added[i][j]));
        break;
    }
}

/** Pieces standing on `board`, which is also the number of features either perspective sets. */
uint32_t countPieces(const Board& board) {
    uint32_t pieces = 0;
    for (auto piece : board) pieces += piece != Piece::_;
    return pieces;
}

}  // namespace

Accumulators refreshBoth(const FeatureTransformer& transformer, const Position& position) {
    Accumulators accumulators;
    refreshInto(accumulators.white, transformer, position, Color::w);
    refreshInto(accumulators.black, transformer, position, Color::b);
    return accumulators;
}

bool PieceChanges::movedKing(Color color) const {
    auto king = addColor(PieceType::KING, color);
    for (uint8_t i = 0; i < removedCount; ++i)
        if (removed[i].piece == king) return true;

    return false;
}

PieceChanges pieceChanges(const Board& board, const BoardChange& change) {
    PieceChanges changes;
    auto moving = board[change.first.from];
    dassert(moving != Piece::_ && "a move moves a piece");

    changes.remove(change.first.from, moving);
    if (change.captured != Piece::_) changes.remove(change.first.to, change.captured);

    // For every plain move the second half is a no-op standing on the square the first half landed
    // on, and the moving piece simply comes to rest there.
    if (change.second.from == change.second.to && change.promo == 0) {
        changes.add(change.first.to, moving);
        return changes;
    }

    if (change.second.from != change.first.to) {
        // Castling, whose second half moves a rook the first half did not touch, so the board
        // still shows it. The king stays where the first half put it.
        changes.add(change.first.to, moving);
        auto rook = board[change.second.from];
        changes.remove(change.second.from, rook);
        changes.add(change.second.to, Piece(index(rook) + change.promo));
    } else {
        // Promotion and en passant, whose second half moves the moving piece on from where the
        // first half left it: it never comes to rest on first.to, and may change type on the way.
        changes.add(change.second.to, Piece(index(moving) + change.promo));
    }

    return changes;
}

Accumulators& AccumulatorStack::grow() {
    if (count == entries.size()) entries.emplace_back();
    return entries[count++];
}

void AccumulatorStack::reset(const FeatureTransformer& transformer, const Position& position) {
    count = 0;
    grow();
    refreshInto(entries[0].white, transformer, position, Color::w);
    refreshInto(entries[0].black, transformer, position, Color::b);
}

void AccumulatorStack::clear() {
    entries.clear();
    entries.shrink_to_fit();
    count = 0;
}

const Accumulators& AccumulatorStack::top() const {
    dassert(active() && "an inactive stack has no top");
    return entries[count - 1];
}

void AccumulatorStack::push(const FeatureTransformer& transformer, const Position& position) {
    if (!active()) return;

    grow();
    refreshInto(entries[count - 1].white, transformer, position, Color::w);
    refreshInto(entries[count - 1].black, transformer, position, Color::b);
}

void AccumulatorStack::push(const FeatureTransformer& transformer, const Position& position,
                            const PieceChanges& changes) {
    if (!active()) return;

    // grow() may reallocate the vector, so nothing may hold a reference across it. The new entry
    // is a recycled one whose values are all overwritten below, and reusing the storage it holds
    // is what keeps a push free of allocation.
    grow();
    auto& top = entries[count - 1];
    const auto& below = entries[count - 2];

    for (auto perspective : {Color::w, Color::b})
        if (changes.movedKing(perspective))
            refreshInto(top[perspective], transformer, position, perspective);
        else
            updatePerspective(top[perspective],
                              below[perspective],
                              transformer,
                              changes,
                              kingSquare(position.board, perspective),
                              perspective);

    dassert(top == refreshBoth(transformer, position) &&
            "an incrementally updated accumulator must equal a fresh one");
}

void AccumulatorStack::pop() {
    if (!active()) return;

    dassert(count > 1 && "the bottom entry belongs to the stack's root, not to a move");
    --count;
}

namespace {

/**
 * Clip both halves of one perspective's `count` accumulator values and multiply them pairwise.
 *
 * Written over three restrict qualified pointers rather than over the accumulator: that is what
 * lets a compiler see that the halves and the output cannot overlap, drop the runtime alias check
 * it would otherwise emit around the loop, and vectorize the whole thing.
 */
void transformHalf(const int16_t* __restrict first, const int16_t* __restrict second,
                   uint8_t* __restrict output, size_t count) {
    for (size_t j = 0; j < count; ++j) {
        // Both clips land in [0, 127], so their product needs 14 bits and the whole step stays in
        // int16 - which is what lets a vectorized loop keep 8 values per register instead of
        // widening to 4. Shifting the product down by 7 leaves the byte the network wants.
        int16_t a = std::clamp<int16_t>(first[j], 0, 127);
        int16_t b = std::clamp<int16_t>(second[j], 0, 127);
        output[j] = uint8_t(int16_t(a * b) >> 7);
    }
}

}  // namespace

void transform(const Accumulator& white, const Accumulator& black, Color sideToMove,
               uint32_t bucket, Transformed& transformed) {
    dassert(bucket < Architecture::kPSQTBuckets);
    dassert(white.values.size() == black.values.size());

    // The side to move comes first, and is the only asymmetry between the two perspectives.
    const Accumulator* perspectives[2] = {&white, &black};
    if (sideToMove == Color::b) std::swap(perspectives[0], perspectives[1]);

    auto l1 = perspectives[0]->values.size();
    dassert(l1 % 2 == 0);
    auto half = l1 / 2;

    transformed.psqt = (perspectives[0]->psqt[bucket] - perspectives[1]->psqt[bucket]) / 2;
    transformed.features.resize(l1);

    for (size_t p = 0; p < 2; ++p) {
        const int16_t* values = perspectives[p]->values.data();
        transformHalf(values, values + half, transformed.features.data() + p * half, half);
    }
}

Transformed transform(const Accumulator& white, const Accumulator& black, Color sideToMove,
                      uint32_t bucket) {
    Transformed transformed;
    transform(white, black, sideToMove, bucket, transformed);

    return transformed;
}

Transformed transform(const FeatureTransformer& transformer, const Position& position,
                      uint32_t bucket) {
    return transform(refresh(transformer, position, Color::w),
                     refresh(transformer, position, Color::b),
                     position.active(),
                     bucket);
}

void affineForward(const AffineLayer& layer, const uint8_t* input, int32_t* output) {
    dassert(layer.biases.size() == layer.outputs);
    dassert(layer.weights.size() == size_t(layer.outputs) * layer.paddedInputs);

    for (uint32_t i = 0; i < layer.outputs; ++i) {
        const int8_t* row = layer.weights.data() + size_t(i) * layer.paddedInputs;
        int32_t sum = layer.biases[i];
        for (uint32_t j = 0; j < layer.inputs; ++j) sum += int32_t(row[j]) * input[j];
        output[i] = sum;
    }
}

std::vector<int32_t> affineForward(const AffineLayer& layer, const std::vector<uint8_t>& input) {
    dassert(input.size() == layer.inputs);

    std::vector<int32_t> output(layer.outputs);
    affineForward(layer, input.data(), output.data());

    return output;
}

ColumnMajorLayer transpose(const AffineLayer& layer) {
    ColumnMajorLayer columns;
    if (layer.outputs != ColumnMajorLayer::kOutputs) return columns;

    columns.inputs = layer.inputs;
    columns.biases = layer.biases;
    columns.weights.resize(size_t(layer.inputs) * ColumnMajorLayer::kOutputs);
    for (uint32_t i = 0; i < layer.outputs; ++i)
        for (uint32_t j = 0; j < layer.inputs; ++j)
            columns.weights[size_t(j) * ColumnMajorLayer::kOutputs + i] = layer.weight(i, j);

    return columns;
}

namespace {

/** Bytes one nonzero scan step covers, being the width of an SSE2 register. */
constexpr size_t kScanWidth = 16;

/**
 * Bitmap of which of the `kScanWidth` bytes at `input` are not zero, bit 0 being the first.
 *
 * The bytes carry no sign bit to read - the transform's pairwise product cannot exceed 126 - so
 * this compares against zero and inverts rather than moving the mask straight out. That reads the
 * same under real SSE2 and under core/sse2emul.h, whose movemask reports "byte is not zero" where
 * the hardware reports "byte is negative": the two agree on exactly the all-ones and all-zeros
 * bytes a comparison produces.
 */
unsigned nonzeroMask(const uint8_t* input) {
    __m128i zeros = {0, 0};
    __m128i chunk;
    std::memcpy(&chunk, input, sizeof(chunk));

    return ~unsigned(_mm_movemask_epi8(_mm_cmpeq_epi8(chunk, zeros))) & ((1u << kScanWidth) - 1);
}

}  // namespace

void affineForwardSparse(const ColumnMajorLayer& columns, const uint8_t* input, size_t inputs,
                         int32_t* output) {
    constexpr size_t kOutputs = ColumnMajorLayer::kOutputs;
    dassert(!columns.empty() && inputs == columns.inputs);

    int32_t sums[kOutputs];
    for (size_t i = 0; i < kOutputs; ++i) sums[i] = columns.biases[i];

    // Scan a register's worth of inputs, then spend its nonzero ones before loading the next: the
    // whole scan never leaves the accumulators, and no list of indices is built to walk twice.
    size_t scanned = inputs / kScanWidth * kScanWidth;
    for (size_t base = 0; base < scanned; base += kScanWidth)
        for (unsigned mask = nonzeroMask(input + base); mask; mask &= mask - 1) {
            size_t j = base + size_t(__builtin_ctz(mask));
            const int8_t* column = columns.column(j);
            int32_t value = input[j];
            // A fixed count of kOutputs, which is the whole reason for the transposed layout: the
            // compiler vectorizes this into a handful of instructions with no loop at all.
            for (size_t i = 0; i < kOutputs; ++i) sums[i] += column[i] * value;
        }

    // Whatever a final partial register holds. Real networks have none, l1 being a multiple of 16.
    for (size_t j = scanned; j < inputs; ++j)
        if (int32_t value = input[j]) {
            const int8_t* column = columns.column(j);
            for (size_t i = 0; i < kOutputs; ++i) sums[i] += column[i] * value;
        }

    for (size_t i = 0; i < kOutputs; ++i) output[i] = sums[i];
}

uint8_t clippedReLU(int32_t value) {
    return uint8_t(std::clamp(value >> kWeightScaleBits, 0, 127));
}

uint8_t sqrClippedReLU(int32_t value) {
    // The square needs 64 bits: a layer output well within int32 range squares out of it.
    auto square = int64_t(value) * value;
    return uint8_t(std::min<int64_t>(127, square >> (2 * kWeightScaleBits + 7)));
}

int32_t propagate(const LayerStack& stack, const std::vector<uint8_t>& features,
                  Propagation* trace) {
    // The forward skip is fc0's last output, so the activations see one output fewer than fc0 has.
    dassert(stack.fc0.outputs > 1);
    auto activated = stack.fc0.outputs - 1;
    dassert(stack.fc1.inputs == 2 * activated);
    dassert(stack.fc2.inputs == stack.fc1.outputs);
    dassert(stack.fc2.outputs == 1);

    // Static so that its buffers are allocated once rather than on every node the search reaches.
    // Everything below overwrites what it holds, so nothing carries over from the previous call.
    static thread_local Propagation scratch;
    Propagation& p = trace ? *trace : scratch;

    dassert(features.size() == stack.fc0.inputs);
    p.fc0.resize(stack.fc0.outputs);
    if (stack.fc0Columns.empty())
        affineForward(stack.fc0, features.data(), p.fc0.data());
    else
        affineForwardSparse(stack.fc0Columns, features.data(), features.size(), p.fc0.data());

    // The same fc0 outputs twice over: squared and clipped first, then merely clipped. Feeding
    // the second layer both is how the network gets a nonlinearity that is not piecewise linear.
    p.fc1Input.resize(2 * activated);
    for (uint32_t i = 0; i < activated; ++i) {
        p.fc1Input[i] = sqrClippedReLU(p.fc0[i]);
        p.fc1Input[activated + i] = clippedReLU(p.fc0[i]);
    }

    p.fc1.resize(stack.fc1.outputs);
    affineForward(stack.fc1, p.fc1Input.data(), p.fc1.data());

    p.fc2Input.resize(stack.fc1.outputs);
    for (uint32_t i = 0; i < stack.fc1.outputs; ++i) p.fc2Input[i] = clippedReLU(p.fc1[i]);

    affineForward(stack.fc2, p.fc2Input.data(), &p.fc2);

    // fc0's last output bypasses the stack, and so is still in the layers' fixed point, where 1.0
    // is 127 << kWeightScaleBits. The result's own scale puts 1.0 at 600 * kOutputScale. Stockfish
    // scales it in 32 bits, which a trained network keeps far from overflowing but an untrained or
    // corrupt one need not; widening the product changes no value this can actually see.
    p.forwardSkip = int32_t(int64_t(p.fc0[activated]) * (600 * kOutputScale) /
                            (127 * (1 << kWeightScaleBits)));
    p.output = p.fc2 + p.forwardSkip;

    return p.output;
}


uint32_t materialBucket(uint32_t pieceCount) {
    dassert(pieceCount >= 2 && "a position holds at least the two kings");
    // Stockfish's own (pieceCount - 1) / 4. Only a board that is not a chess position can leave
    // the eight stacks, and the assert above is gone in an optimized build, so clamp rather than
    // index past them; the arithmetic is unsigned, so an impossible count of zero clamps too.
    return std::min((pieceCount - 1) / 4, Architecture::kLayerStacks - 1);
}

uint32_t materialBucket(const Position& position) {
    return materialBucket(countPieces(position.board));
}

int32_t evaluateValue(const Network& network, const Position& position,
                      const Accumulators& accumulators, Evaluation* trace) {
    Evaluation scratch;
    Evaluation& e = trace ? *trace : scratch;

    // Every piece contributes exactly one feature to either perspective, so this is also the
    // number of rows the accumulators handed in are the sum of.
    e.pieceCount = countPieces(position.board);
    e.bucket = materialBucket(e.pieceCount);

    // Static so that the 2560 byte feature buffer is allocated once rather than on every node.
    // transform() overwrites all of it, so nothing carries over from the previous call.
    static thread_local Transformed transformed;
    transform(accumulators.white, accumulators.black, position.active(), e.bucket, transformed);
    e.psqt = transformed.psqt;
    e.positional = propagate(network.stacks[e.bucket], transformed.features);

    // The two terms are in the same units and simply add; the division is what makes their sum a
    // Value. Stockfish truncates toward zero here, and a rounding of our own would be visible.
    e.value = (e.psqt + e.positional) / kOutputScale;

    return e.value;
}

int32_t evaluateValue(const Network& network, const Position& position, Evaluation* trace) {
    return evaluateValue(network, position, refreshBoth(network.transformer, position), trace);
}

int32_t evaluate(const Network& network, const Position& position,
                 const Accumulators& accumulators) {
    // Into centipawns, then out of the side to move's frame and into White's, which is the
    // convention the search expects. Truncation toward zero makes the order of the two
    // immaterial. The clamp only fires on a position no search needs a precise number for.
    auto cp = evaluateValue(network, position, accumulators) * 100 / kNormalizeToPawnValue;
    if (position.active() == Color::b) cp = -cp;

    return std::clamp(cp, -kMaxEvaluation, kMaxEvaluation);
}

int32_t evaluate(const Network& network, const Position& position) {
    return evaluate(network, position, refreshBoth(network.transformer, position));
}

}  // namespace nnue

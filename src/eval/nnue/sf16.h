#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

#include "core/core.h"

/**
 * Support for the Stockfish 16.1 NNUE file format and its HalfKAv2_hm feature set.
 *
 * This is a separate, passive implementation living next to the SF12 format support in nnue.h:
 * the two formats share nothing but the general shape of their file header, and gbchess itself
 * still evaluates with the SF12 network. This reads a network file into memory in the exact
 * layout the file uses, and turns a position into the feature indices that address it; nothing
 * here propagates a position through the network.
 *
 * The file layout is, all integers little endian:
 *
 *     uint32  version              format version, kVersion below
 *     uint32  hash                 architecture hash, identifying the network topology
 *     uint32  descriptionLength    length in bytes of the description that follows
 *     char[]  description          human readable provenance string, not NUL terminated
 *     uint32  transformerHash      structure hash of the feature transformer
 *     leb128  transformer parameters, three separately compressed blocks
 *     ...                          eight layer stacks, each a structure hash and raw parameters
 *
 * and nothing else: trailing bytes make the file malformed. See Network below for the details
 * of the parameter blocks.
 */
namespace nnue::sf16 {

/**
 * Hash contribution of an affine transform layer, mixing in its output dimensions.
 * Matches Stockfish's AffineTransform::get_hash_value.
 */
constexpr uint32_t affineHash(uint32_t prevHash, uint32_t outputDimensions) {
    uint32_t hash = 0xcc03dae4u + outputDimensions;
    hash ^= prevHash >> 1;
    hash ^= prevHash << 31;
    return hash;
}

/**
 * Hash contribution of a clipped ReLU activation layer.
 * Matches Stockfish's ClippedReLU::get_hash_value.
 */
constexpr uint32_t clippedReLUHash(uint32_t prevHash) {
    return 0x538d24c7u + prevHash;
}

/**
 * Description of one SF16.1 network topology, from which the architecture hash is derived.
 * Stockfish parameterizes the architecture over three layer sizes, with its Big and Small
 * networks differing only in those; the Big network is the default one and the one we target.
 *
 * The network transforms HalfKAv2_hm features into an accumulator of l1 values per perspective,
 * which feeds a stack of three affine layers with clipped ReLU activations in between. Only the
 * layer sizes and the two seed constants below take part in the hash, so this suffices to
 * identify a network file without knowing how the layers are laid out or evaluated.
 */
struct Architecture {
    /** Hash of the HalfKAv2_hm feature set, an arbitrary constant identifying the feature set. */
    static constexpr uint32_t kFeatureSetHash = 0x7f234cb8u;
    /** Hash seed of the input slice feeding the first affine layer, likewise arbitrary. */
    static constexpr uint32_t kInputSliceHash = 0xec42e90du;
    /** HalfKAv2_hm input features: 64 king squares times 704 piece-square states, mirrored. */
    static constexpr uint32_t kInputDimensions = 64 * 704 / 2;
    /** Number of PSQT buckets the feature transformer carries alongside the accumulator. */
    static constexpr uint32_t kPSQTBuckets = 8;
    /** Number of layer stacks, one of which evaluates a position depending on its piece count. */
    static constexpr uint32_t kLayerStacks = 8;
    /**
     * Granularity to which affine layer inputs are padded in the file. Stockfish sizes its weight
     * arrays for the widest SIMD register it might use and serializes the padding columns along
     * with the real ones, so the file holds padded rows even though the padding is always zero.
     */
    static constexpr uint32_t kPaddingWidth = 32;

    uint32_t l1;  // accumulator values per perspective, so 2 * l1 in total
    uint32_t l2;  // outputs of the first affine layer, less the extra output Stockfish appends
    uint32_t l3;  // outputs of the second affine layer
    /**
     * Number of input features. This does not take part in any hash, as the feature set
     * contributes a fixed constant, so tests may shrink it to build small synthetic networks.
     */
    uint32_t inputDimensions = kInputDimensions;

    /** Round up to a multiple of kPaddingWidth, as Stockfish pads affine layer inputs. */
    static constexpr uint32_t padded(uint32_t dimensions) {
        return (dimensions + kPaddingWidth - 1) / kPaddingWidth * kPaddingWidth;
    }

    /** Hash of the feature transformer: the feature set mixed with the accumulator size. */
    constexpr uint32_t featureTransformerHash() const { return kFeatureSetHash ^ (2 * l1); }

    /** Hash of the layer stack, accumulated layer by layer in forward order. */
    constexpr uint32_t networkHash() const {
        uint32_t hash = kInputSliceHash ^ (2 * l1);  // input slice
        hash = affineHash(hash, l2 + 1);             // first affine layer
        hash = clippedReLUHash(hash);                // activation
        hash = affineHash(hash, l3);                 // second affine layer
        hash = clippedReLUHash(hash);                // activation
        return affineHash(hash, 1);                  // output layer
    }

    /** The architecture hash as stored in the file header. */
    constexpr uint32_t hash() const { return featureTransformerHash() ^ networkHash(); }
};

/** The default "Big" network of Stockfish 16.1, distributed as nn-b1a57edbea57.nnue. */
constexpr Architecture kBigArchitecture = {2560, 15, 32};

static_assert(kBigArchitecture.hash() == 0x1c103072u,
              "SF16.1 Big architecture hash must match the published network");
static_assert(kBigArchitecture.featureTransformerHash() == 0x7f2358b8u,
              "SF16.1 Big feature transformer hash must match the published network");
static_assert(kBigArchitecture.networkHash() == 0x633368cau,
              "SF16.1 Big layer stack hash must match the published network");

/** The top-level header of an SF16.1 NNUE file. */
struct FileHeader {
    /** Format version of Stockfish 16.1 networks. SF12 used 0x7af32f16. */
    static constexpr uint32_t kVersion = 0x7af32f20u;
    /**
     * Upper bound on the description length we accept, an empty description being rejected too.
     * Real networks describe their provenance in about a hundred bytes; a much larger length
     * means we are not looking at a header at all, and we would rather report that than try to
     * allocate whatever the file asks for.
     */
    static constexpr uint32_t kMaxDescriptionLength = 4096;

    uint32_t version;
    uint32_t hash;
    std::string description;
};

/**
 * Read and validate the top-level header of an SF16.1 NNUE file for the given architecture.
 * Throws std::runtime_error if the stream ends early or the header does not describe such a
 * network, leaving the stream position unspecified in that case.
 */
FileHeader readFileHeader(std::istream& in, const Architecture& arch = kBigArchitecture);

/** The magic string introducing a signed LEB128 compressed parameter block. */
constexpr char kLeb128Magic[] = "COMPRESSED_LEB128";

/**
 * Read `count` signed values compressed with signed LEB128 from a block that starts with
 * kLeb128Magic and a little endian uint32 giving the size of the compressed data that follows.
 * The block must hold exactly `count` values: both a short and an overlong block are an error.
 * Throws std::runtime_error on any malformed input. Instantiated for int16_t and int32_t.
 */
template <typename Int>
void readLeb128(std::istream& in, Int* out, size_t count);

/**
 * The feature transformer, holding one accumulator row per input feature.
 *
 * Its parameters are the only compressed ones in the file, each array its own LEB128 block, and
 * they dominate the size of a network: the weights alone are inputDimensions * l1 values.
 */
struct FeatureTransformer {
    /** One bias per accumulator value, l1 of them. */
    std::vector<int16_t> biases;
    /** Accumulator contribution of each feature, row major: weights[feature * l1 + i]. */
    std::vector<int16_t> weights;
    /** Per feature PSQT contributions, row major: psqtWeights[feature * kPSQTBuckets + bucket]. */
    std::vector<int32_t> psqtWeights;
};

/**
 * One fully connected layer, holding its parameters in the order the file stores them: a bias per
 * output, then the weight matrix row by row with one row per output.
 *
 * Rows are padded out to `paddedInputs` columns; the columns beyond `inputs` are read as they
 * appear in the file and are zero in practice. This is the portable ordering the file is written
 * in. Stockfish permutes these weights while reading to suit the SIMD kernel it will run, which we
 * deliberately do not do: a plain row major matrix is what a scalar reference evaluation wants.
 */
struct AffineLayer {
    uint32_t inputs = 0;        // number of meaningful input values
    uint32_t paddedInputs = 0;  // columns per row as stored, inputs rounded up for SIMD
    uint32_t outputs = 0;       // number of outputs, and so of rows and biases

    std::vector<int32_t> biases;
    std::vector<int8_t> weights;

    /** Weight applied to input `j` when computing output `i`. */
    int8_t weight(uint32_t i, uint32_t j) const { return weights[i * paddedInputs + j]; }
};

/**
 * One of the eight layer stacks, which Stockfish selects between by piece count.
 *
 * The stack maps the l1 transformed features to a single value. The first layer produces l2 + 1
 * outputs, of which the last bypasses the rest of the stack and is added to its result; the other
 * l2 outputs are fed to the second layer both squared-and-clipped and merely clipped, hence its
 * 2 * l2 inputs. The clipping activations in between hold no parameters and appear nowhere in the
 * file, so a stack is exactly these three layers.
 */
struct LayerStack {
    AffineLayer fc0;  // l1 -> l2 + 1
    AffineLayer fc1;  // 2 * l2 -> l3
    AffineLayer fc2;  // l3 -> 1
};

/**
 * A complete network: every learned parameter of a network file, and nothing derived from them.
 *
 * This is large, around 116MB for the Big network, all of it in heap allocated vectors. Copying
 * one is never what a caller wants, so a Network only moves.
 */
struct Network {
    Network() = default;
    /** Declaring this leaves a Network movable while deleting both of its copy operations. */
    Network(Network&&) = default;

    Architecture arch = kBigArchitecture;
    FileHeader header;
    FeatureTransformer transformer;
    std::array<LayerStack, Architecture::kLayerStacks> stacks;
};

/**
 * Read a complete SF16.1 network file, validating its header, the structure hashes of the feature
 * transformer and of every layer stack, and that the file ends right after the last stack.
 * Throws std::runtime_error on anything unexpected, including trailing bytes.
 */
Network readNetwork(std::istream& in, const Architecture& arch = kBigArchitecture);

/**
 * The HalfKAv2_hm feature set, which selects the rows of the feature transformer a position sets.
 *
 * A feature is a (king bucket, piece category, square) triple seen from one side, so a position
 * has two feature lists, one per perspective, each holding exactly one feature per piece on the
 * board. Everything is expressed relative to the perspective side: its own pieces get the first
 * of the two categories of their type, and its own pawns always advance up the board.
 *
 * A square is oriented before it is used, by up to two flips:
 *
 *   - for black, ranks are flipped, so that black's own back rank becomes rank 1;
 *   - for either side, files are flipped when its own king would otherwise stand on files a to d,
 *     normalizing that king onto files e to h.
 *
 * That second flip is the "hm", horizontal mirroring, and it is what lets 64 possible king squares
 * address only 32 king buckets, halving the feature space. A position and its mirror image
 * therefore produce identical features.
 *
 * Unlike SF12's HalfKP, which drops both kings before building its features, HalfKAv2_hm gives
 * kings a piece category of their own, shared by both colors, and so counts them among the active
 * features: a bare king and king position still has two of them.
 */

/** Piece categories a square can be in: two per non-king piece type, plus one shared by kings. */
constexpr uint16_t kPieceCategories = 2 * (kNumPieceTypes - 1) + 1;  // 11
/** Feature indices spanned by one king bucket, one per piece category per square. */
constexpr uint16_t kBucketStride = kPieceCategories * kNumSquares;  // 704
/** King buckets, being the 64 king squares halved by the horizontal mirroring. */
constexpr uint16_t kKingBuckets = kNumSquares / 2;  // 32

static_assert(kKingBuckets * kBucketStride == Architecture::kInputDimensions,
              "the feature space must address the network's inputs exactly");

/**
 * The active features of a position from one perspective, in board order.
 *
 * Every piece contributes exactly one feature and a board holds at most 32 pieces, so this is a
 * fixed size array rather than a heap allocation.
 */
struct ActiveFeatures {
    /** Pieces that fit on a board, and so features that can be active at once. */
    static constexpr size_t kMaxSize = 32;

    std::array<uint16_t, kMaxSize> indices = {};
    uint8_t size = 0;

    void add(uint16_t index) {
        assert(size < kMaxSize);
        indices[size++] = index;
    }

    const uint16_t* begin() const { return indices.data(); }
    const uint16_t* end() const { return indices.data() + size; }
};

/**
 * Index of the feature for `piece` on `square`, seen from `perspective` whose king is on
 * `kingSquare`. The king square picks both the bucket and the orientation, so it is needed even
 * for the feature of the king itself.
 */
uint16_t featureIndex(Square square, Piece piece, Square kingSquare, Color perspective);

/**
 * The features of `position` that are active from `perspective`.
 * Throws std::runtime_error if the position has no king of that color.
 */
ActiveFeatures activeFeatures(const Position& position, Color perspective);

/**
 * The feature transformer's accumulated state for one perspective: the transformer's biases plus
 * the rows its active features select.
 *
 * Stockfish maintains this incrementally as it walks the search tree, adding and subtracting rows
 * as pieces move. We only ever build one from scratch, which is what Stockfish itself falls back
 * to whenever the perspective's own king moves and every feature index changes at once.
 */
struct Accumulator {
    /** One value per accumulator entry, arch.l1 of them. */
    std::vector<int16_t> values;
    /** PSQT contribution of the active features, which the transformer carries alongside. */
    std::array<int32_t, Architecture::kPSQTBuckets> psqt = {};
};

/**
 * Accumulate `features` into a fresh accumulator, starting from the transformer's biases.
 *
 * Both parameter arrays are feature major, so one feature contributes a contiguous run of each:
 * l1 accumulator values, and kPSQTBuckets PSQT values.
 */
Accumulator refresh(const FeatureTransformer& transformer, const ActiveFeatures& features);

/** Accumulate the features `position` sets from `perspective`, as refresh() above. */
Accumulator refresh(const FeatureTransformer& transformer, const Position& position,
                    Color perspective);

/**
 * The transformer's output: the values the layer stacks are evaluated on, and the PSQT term that
 * is added to their result.
 *
 * A perspective's l1 accumulator values are read as two halves, whose entries are clipped to a
 * byte and multiplied together pairwise, so each perspective contributes l1 / 2 output bytes. The
 * side to move comes first, which is the only place the network learns whose turn it is: the two
 * perspectives are otherwise built by identical rules.
 */
struct Transformed {
    /** Clipped and paired accumulator values, arch.l1 of them: side to move, then the opponent. */
    std::vector<uint8_t> features;
    /** Difference between the perspectives' PSQT accumulators in the requested bucket. */
    int32_t psqt = 0;
};

/**
 * Transform the accumulators of both perspectives, seen from `sideToMove`, reading the PSQT
 * accumulators in `bucket`.
 *
 * The bucket is the caller's to pick. Stockfish derives it from the piece count, but that is a
 * property of how a position is evaluated rather than of the transformer, and passing it in lets
 * one position exercise all eight.
 */
Transformed transform(const Accumulator& white, const Accumulator& black, Color sideToMove,
                      uint32_t bucket);

/** Refresh both perspectives of `position` and transform them, as transform() above. */
Transformed transform(const FeatureTransformer& transformer, const Position& position,
                      uint32_t bucket);

}  // namespace nnue::sf16

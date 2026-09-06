#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <string>
#include <vector>

#include "core/core.h"

/**
 * The evaluation: the Stockfish 16.1 NNUE file format, its HalfKAv2_hm feature set, and the
 * network that gbchess plays with.
 *
 * A network file is read into memory in the exact layout the file uses, and a position turns
 * into the feature indices that address it. Everything from there down to evaluate() is verified
 * bit for bit against Stockfish 16.1 itself; see SF16_NNUE_PLAN.md for how that oracle was built.
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
namespace nnue {

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
 * Description of one Stockfish 16.1 network topology, from which the architecture hash is
 * derived. Stockfish parameterizes the architecture over three layer sizes, with its Big and Small
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
              "Stockfish 16.1 Big architecture hash must match the published network");
static_assert(kBigArchitecture.featureTransformerHash() == 0x7f2358b8u,
              "Stockfish 16.1 Big feature transformer hash must match the published network");
static_assert(kBigArchitecture.networkHash() == 0x633368cau,
              "Stockfish 16.1 Big layer stack hash must match the published network");

/** The top-level header of a Stockfish 16.1 NNUE file. */
struct FileHeader {
    /** Format version of Stockfish 16.1 networks. Stockfish 12 used 0x7af32f16. */
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
 * Read and validate the top-level header of a Stockfish 16.1 NNUE file for the given
 * architecture. Throws std::runtime_error if the stream ends early or the header does not
 * describe such a network, leaving the stream position unspecified in that case.
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
 * An affine layer's weights transposed: the weights one *input* contributes, contiguously.
 *
 * The canonical layout holds one row per output, which is what a kernel that computes one output
 * at a time wants. A kernel that instead walks the inputs and skips the zero ones - and nine of
 * every ten bytes reaching fc0 are zero - touches every output for each input it keeps, so it
 * wants the opposite: the `kOutputs` weights of one input, in a single contiguous run.
 *
 * This is the second, permuted layout the port's design allows, and it is built *beside* the row
 * major layer rather than in place of it. The canonical weights stay exactly as the file stores
 * them, remain what affineForward() reads, and remain what the golden tests check, so the two
 * kernels can be compared against each other on every position.
 */
struct ColumnMajorLayer {
    /**
     * Outputs the sparse kernel handles, being fc0's l2 + 1 in both Stockfish 16.1 networks.
     *
     * Fixing this at compile time is the whole point: the kernel's inner loop is then a fixed
     * count that a compiler turns into straight line vector code on any target, which is what
     * makes it fast without a hand written intrinsic in sight. A layer of any other width simply
     * gets no column major twin and keeps the canonical path.
     */
    static constexpr uint32_t kOutputs = 16;

    uint32_t inputs = 0;
    std::vector<int32_t> biases;
    std::vector<int8_t> weights;  // weights[j * kOutputs + i] == layer.weight(i, j)

    /** Whether this layer has a transposed twin at all; see kOutputs. */
    bool empty() const { return weights.empty(); }

    /** The kOutputs weights that input `j` contributes. */
    const int8_t* column(size_t j) const { return weights.data() + j * kOutputs; }
};

/**
 * Transpose `layer` for affineForwardSparse(), or return an empty ColumnMajorLayer if its width
 * is not the one that kernel is specialized for.
 */
ColumnMajorLayer transpose(const AffineLayer& layer);

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

    /**
     * fc0 again, transposed for the sparse kernel, and the only layer worth transposing: fc0 does
     * 2560 multiplies per output where fc1 does 30. Empty when the network is not one the kernel
     * applies to, which leaves propagate() on the canonical path.
     */
    ColumnMajorLayer fc0Columns;
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
 * Read a complete Stockfish 16.1 network file, validating its header, the structure hashes of
 * the feature transformer and of every layer stack, and that the file ends right after the last
 * stack.
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
 * Unlike Stockfish 12's HalfKP, which drops both kings before building its features, HalfKAv2_hm
 * gives kings a piece category of their own, shared by both colors, and so counts them among the
 * active features: a bare king and king position still has two of them.
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
 * Both perspectives' accumulators, which together are everything an evaluation reads of a board.
 */
struct Accumulators {
    Accumulator white, black;

    Accumulator& operator[](Color perspective) { return perspective == Color::w ? white : black; }
    const Accumulator& operator[](Color perspective) const {
        return perspective == Color::w ? white : black;
    }
};

/**
 * Refresh both perspectives of `position`, as refresh() above.
 *
 * Not an overload of refresh(): a call taking a Position and one taking an ActiveFeatures would
 * differ only in an argument that is often written `{}`, and they return different types.
 */
Accumulators refreshBoth(const FeatureTransformer& transformer, const Position& position);

inline bool operator==(const Accumulator& left, const Accumulator& right) {
    return left.values == right.values && left.psqt == right.psqt;
}

inline bool operator==(const Accumulators& left, const Accumulators& right) {
    return left.white == right.white && left.black == right.black;
}

/**
 * The net effect of one move on where the pieces stand: what leaves the board and what arrives.
 *
 * A feature is a (piece, square) pair, so this is exactly the list of transformer rows a move
 * subtracts from an accumulator and the list it adds, up to the perspective that turns them into
 * indices. Writing the move out this way is what lets the update rule below ignore move kinds
 * entirely: a capture is a second removal, castling is a second arrival, en passant removes a pawn
 * from a square the mover never comes to rest on, and a promotion lands a piece that never left.
 */
struct PieceChanges {
    struct Placement {
        Square square = Square(0);
        Piece piece = Piece::_;
    };

    /** At most three: the moving piece, a captured piece and castling's rook. */
    std::array<Placement, 3> removed = {};
    /** At most two: the moving piece at its destination and castling's rook at its own. */
    std::array<Placement, 2> added = {};
    uint8_t removedCount = 0;
    uint8_t addedCount = 0;

    void remove(Square square, Piece piece) {
        assert(removedCount < removed.size());
        removed[removedCount++] = {square, piece};
    }

    void add(Square square, Piece piece) {
        assert(addedCount < added.size());
        added[addedCount++] = {square, piece};
    }

    /**
     * Whether `color`'s own king left a square, and so whether that perspective must be rebuilt
     * from scratch rather than updated: every one of its feature indices, the king bucket and the
     * mirroring flip alike, is expressed relative to where that king stands.
     */
    bool movedKing(Color color) const;
};

/**
 * The pieces that `change` moves, read off `board` as it stands before makeMove applies it.
 *
 * The moving piece is the one thing a BoardChange does not name, so this has to see the board
 * first; everything else it derives from the change alone. `change.first` is the simple half of
 * the move, capture included, and `change.second` is the compound half that castling, promotion
 * and en passant need, a no-op for every other move.
 */
PieceChanges pieceChanges(const Board& board, const BoardChange& change);

/**
 * The accumulators of every position along a search path, maintained incrementally.
 *
 * A search that keeps one of these pays for a full refresh at its root and whenever a king moves,
 * and for at most three feature rows per perspective on every other move. Entries form a stack
 * rather than a single running accumulator because unmakeMove has to uncover the accumulators of
 * the position it restores exactly as they were, and because a perspective whose king moved has
 * no delta to undo at all.
 *
 * A stack with nothing on it is *inactive*: push() and pop() do nothing, top() has nothing to
 * return, and so an engine that evaluates with another network, or any caller outside a search,
 * pays nothing for the existence of this one. reset() is what makes a stack active.
 */
class AccumulatorStack {
public:
    /** Whether a search is maintaining this stack; see the class comment. */
    bool active() const { return count != 0; }

    /** Positions currently on the stack, being the root plus one per move made from it. */
    size_t size() const { return count; }

    /** Accumulate `position` from scratch as the only entry, making the stack active. */
    void reset(const FeatureTransformer& transformer, const Position& position);

    /** Make the stack inactive again, releasing the memory its entries hold. */
    void clear();

    /** The accumulators of the position pushed last. The stack must be active. */
    const Accumulators& top() const;

    /**
     * Push the accumulators of `position`, the position that results from applying the move that
     * `changes` describes to the position currently on top. Does nothing while inactive.
     *
     * A debug build checks the result against a fresh refresh of `position`. That equality is the
     * entire correctness argument for the incremental update, it catches every class of indexing
     * and delta error this can make, and it costs nothing in an optimized build.
     */
    void push(const FeatureTransformer& transformer, const Position& position,
              const PieceChanges& changes);

    /** Push a fresh accumulation of `position`, for a caller with no PieceChanges to offer. */
    void push(const FeatureTransformer& transformer, const Position& position);

    /** Uncover the entry below the top one. Does nothing while inactive. */
    void pop();

private:
    /** The next entry, which is a recycled one whenever the stack has been this deep before. */
    Accumulators& grow();

    /**
     * Entries 0 to count - 1 are live. The vector is never shrunk by pop(), so the accumulator
     * a popped entry allocated is reused by the next push rather than freed and allocated again.
     */
    std::vector<Accumulators> entries;
    size_t count = 0;
};

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

/**
 * Transform into `transformed`, reusing the buffer it already holds, as transform() above.
 *
 * The features are l1 bytes and the search transforms once per evaluated node, so returning a
 * fresh vector every time is a malloc and a free per node. Both call the same loop.
 */
void transform(const Accumulator& white, const Accumulator& black, Color sideToMove,
               uint32_t bucket, Transformed& transformed);

/** Refresh both perspectives of `position` and transform them, as transform() above. */
Transformed transform(const FeatureTransformer& transformer, const Position& position,
                      uint32_t bucket);

/**
 * Fixed point scale of the affine layers: within a layer stack, 1.0 is 127 << kWeightScaleBits.
 *
 * Both activations shift by this to bring a layer's int32 output back into the byte range that
 * the next layer's int8 weights expect, which is what lets the three layers chain at all.
 */
constexpr int kWeightScaleBits = 6;

/**
 * Scale of the network's output, in which one pawn is nominally 600 * kOutputScale.
 *
 * Phase 6 needs it only to rescale the forward skip term below; dividing a whole evaluation by
 * it is the business of the layer above this one.
 */
constexpr int32_t kOutputScale = 16;

/**
 * Affine forward pass: one output per row, its bias plus the row's weighted sum of the input.
 *
 * The sum runs over `layer.inputs`, not the padded row width. The padding columns are zero in
 * every real network, so reading them too would give the same answer while quietly accepting a
 * layout that has drifted; summing only the meaningful columns does not.
 *
 * Inputs are unsigned bytes and weights are signed, every layer of a stack being fed by an
 * activation. The products fit an int32 accumulator by a wide margin: even 2560 inputs at the
 * extreme 127 * 127 leave the sum below 2^26.
 */
std::vector<int32_t> affineForward(const AffineLayer& layer, const std::vector<uint8_t>& input);

/**
 * Affine forward writing `layer.outputs` values through `output`, allocating nothing.
 *
 * The evaluation runs this once per layer per node, so the vector the overload above returns is a
 * malloc and a free the search cannot afford. Both call the same loop.
 */
void affineForward(const AffineLayer& layer, const uint8_t* input, int32_t* output);

/**
 * Affine forward over the transposed weights, visiting only the inputs that are not zero.
 *
 * Exactly affineForward(), value for value: a zero input contributes nothing to any output, so
 * skipping it is not an approximation. What makes it worth doing is what the feature transform
 * produces - its pairwise product is zero whenever either half clipped to zero, which is about
 * nine bytes in ten - and what the transposed layout allows, which is spending one nonzero input
 * on all kOutputs outputs from a single contiguous load.
 *
 * Writes ColumnMajorLayer::kOutputs values through `output`. `columns` must not be empty.
 */
void affineForwardSparse(const ColumnMajorLayer& columns, const uint8_t* input, size_t inputs,
                         int32_t* output);

/** Stockfish's ClippedReLU: shift back into byte range, saturating at 127 and at zero. */
uint8_t clippedReLU(int32_t value);

/**
 * Stockfish's SqrClippedReLU: the square, shifted down by 2 * kWeightScaleBits + 7 and capped.
 *
 * The extra 7 bits stand in for a division by 127 that the trainer compensates for. Squaring
 * discards the sign, so unlike the plain clipped ReLU this activation passes a negative input
 * through as a positive value rather than flooring it at zero.
 */
uint8_t sqrClippedReLU(int32_t value);

/**
 * Every intermediate of one propagation through a layer stack, in the order they are computed.
 *
 * Only the golden tests need these. Matching a stack's final output alone would let two
 * compensating errors - a transposed weight matrix and a mirrored input, say - cancel out and
 * pass; checking every value in between pins each layer separately.
 */
struct Propagation {
    /** fc0's outputs, l2 + 1 of them; the last is the forward skip and feeds no activation. */
    std::vector<int32_t> fc0;
    /** fc1's input: the first l2 of fc0 squared and clipped, then the same l2 merely clipped. */
    std::vector<uint8_t> fc1Input;
    /** fc1's outputs, l3 of them. */
    std::vector<int32_t> fc1;
    /** fc2's input: fc1's outputs clipped. */
    std::vector<uint8_t> fc2Input;
    /** fc2's single output. */
    int32_t fc2 = 0;
    /** fc0's last output, rescaled from the layers' fixed point into the network's own. */
    int32_t forwardSkip = 0;
    /** The stack's result, being fc2 plus the forward skip. */
    int32_t output = 0;
};

/**
 * Propagate transformed features through one layer stack, yielding its raw output.
 *
 * `features` are the bytes transform() produced, l1 of them. The result is in the network's
 * output units, which a caller turns into a score by adding the PSQT term and dividing by
 * kOutputScale; this function knows nothing of positions, buckets or centipawns.
 *
 * If `trace` is given it receives every intermediate. The traced and untraced calls run the
 * same code, so a test may trust what one reports about the other.
 */
int32_t propagate(const LayerStack& stack, const std::vector<uint8_t>& features,
                  Propagation* trace = nullptr);


/**
 * Scale between the network's own Value units and centipawns: Stockfish's NormalizeToPawnValue.
 *
 * A Value is not a centipawn. What makes "one pawn" come out near 100 is a constant the network's
 * training fixes and Stockfish 16.1 keeps in uci.cpp, where it is 356 for this network. gbchess's
 * search margins in options.h were tuned against the centipawn scale of the evaluation this one
 * replaced, so this is the number to revisit when they are, and it is deliberately one named
 * constant rather than a literal in an expression.
 */
constexpr int32_t kNormalizeToPawnValue = 356;

/**
 * Largest centipawn magnitude evaluate() reports.
 *
 * Score is an int16_t that asserts a magnitude of at most 9999 and reserves the band above 9900
 * for mate scores; a network looking at a hopelessly won position can name a larger number than
 * that. Clamping here keeps a legitimate blowout from aborting a debug build or, worse, quietly
 * reading as a mate the search would then believe.
 */
constexpr int32_t kMaxEvaluation = 9000;

/**
 * The layer stack Stockfish selects for a position holding `pieceCount` pieces.
 *
 * One number selects both the layer stack and the PSQT bucket: they are two halves of a single
 * bucketed evaluation rather than two things chosen independently. HalfKAv2_hm counts kings among
 * its features, so an ActiveFeatures::size is exactly the count this wants and no separate walk
 * over the board is needed on the evaluation path.
 */
uint32_t materialBucket(uint32_t pieceCount);

/** The bucket of `position`, counting the pieces on its board. */
uint32_t materialBucket(const Position& position);

/**
 * Everything an evaluation computes on its way to a value.
 *
 * As with Propagation, the traced and untraced calls run the same code, so a test may check what
 * one reports about the other. Unlike Propagation this is small enough to be worth printing.
 */
struct Evaluation {
    /** Pieces on the board, which is also the number of active features per perspective. */
    uint32_t pieceCount = 0;
    /** The layer stack and PSQT bucket that count selects. */
    uint32_t bucket = 0;
    /** The transformer's PSQT term in that bucket. */
    int32_t psqt = 0;
    /** The selected layer stack's output. */
    int32_t positional = 0;
    /** Their sum in the network's output units, relative to the side to move. */
    int32_t value = 0;
};

/**
 * Evaluate `position` with `network`, in Stockfish's internal Value units.
 *
 * The result is relative to the side to move, as it is in Stockfish: the transform puts the side
 * to move's perspective first, and nothing below that point knows which color it is looking at. A
 * position and its color swapped image therefore evaluate to the same number, not opposite ones.
 *
 * If `trace` is given it receives the bucket and the two terms the value is made of.
 */
int32_t evaluateValue(const Network& network, const Position& position,
                      Evaluation* trace = nullptr);

/**
 * Evaluate `position` from accumulators already computed for it, as evaluateValue() above.
 *
 * The accumulators must be the ones belonging to `position` itself. Everything below them reads
 * the board only through them, so a stale pair does not evaluate this position approximately: it
 * evaluates the position it was built from, and says nothing at all about this one.
 */
int32_t evaluateValue(const Network& network, const Position& position,
                      const Accumulators& accumulators, Evaluation* trace = nullptr);

/**
 * Evaluate `position` in centipawns, positive when White stands better.
 *
 * White-relative rather than side-to-move relative is the convention gbchess's search expects;
 * staticEval is the one place that negates it. The result is clamped to kMaxEvaluation.
 */
int32_t evaluate(const Network& network, const Position& position);

/** Evaluate `position` in centipawns from accumulators already computed for it, as above. */
int32_t evaluate(const Network& network, const Position& position,
                 const Accumulators& accumulators);

}  // namespace nnue

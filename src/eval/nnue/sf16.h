#pragma once

#include <cstdint>
#include <istream>
#include <string>

/**
 * Support for the Stockfish 16.1 NNUE file format.
 *
 * This is a separate, passive implementation living next to the SF12 format support in nnue.h:
 * the two formats share nothing but the general shape of their file header, and gbchess itself
 * still evaluates with the SF12 network. Only the top-level file header is decoded here.
 *
 * The file layout starts with, all integers little endian:
 *
 *     uint32  version              format version, kVersion below
 *     uint32  hash                 architecture hash, identifying the network topology
 *     uint32  descriptionLength    length in bytes of the description that follows
 *     char[]  description          human readable provenance string, not NUL terminated
 *
 * after which the feature transformer and network parameters follow, which we do not read yet.
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

    uint32_t l1;  // accumulator values per perspective, so 2 * l1 in total
    uint32_t l2;  // outputs of the first affine layer, less the extra output Stockfish appends
    uint32_t l3;  // outputs of the second affine layer

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
 * Read and validate the top-level header of an SF16.1 NNUE file for the Big architecture.
 * Throws std::runtime_error if the stream ends early or the header does not describe such a
 * network, leaving the stream position unspecified in that case.
 */
FileHeader readFileHeader(std::istream& in);

}  // namespace nnue::sf16

#include <cstdint>
#include <string>
#include <vector>

namespace nnue {
constexpr std::uint32_t kVersion = 0x7af32f16u;

// rough overview of the NNUE architecture:
// Feature Transformer
//   input 41024 dimensions
//   output 2*256==512 dimensions, output type is uint8_t
// Feature Transformer File Data
//   uint32_t hash; // 0x5d69d5b8 ^ output dimensions == 5d69d7b8
//   int16_t bias[256]; // 256 elements
//   int16_t weights[256][41024]; // 256 rows, 41024 columns
// Network Layers
//   InputSlice[512]
//   AffineLayer 32<-512
//   ClippedReLU[32]
//   AffineLayer 32<-32
//   ClippedReLU[32]
//   AffineLayer 1<-32
// Network Layers File Data
//   uint32_t hash // network hash
//
// Accounting of the total file size of 21022697 bytes:
//   Sum: 21022697 == 189 + 21004804 + 17704 bytes
//   Header: 189 bytes
//     Version: 4 bytes
//     Hash: 4 bytes
//     Name size: 4 bytes
//     Name: 177 bytes
//
//  Feature Transformer: 21004804 bytes
//     Hash: 4 bytes
//     Bias file size: 512
//     Weight file size: 21004288
//  Network Layers: 17704 == 4 + 16512 + 1152 + 36 bytes
//     Hash: 4 bytes
//     AffineTransform 32 <- 512: 16512 bytes
//         Bias file size: 128
//         Weight file size: 16384
//     AffineTransform 32 <- 32: 1152 bytes
//         Bias file size: 128
//         Weight file size: 1024
//     AffineTransform 1 <- 32: 36 bytes
//         Bias file size: 4
//         Weight file size: 32

struct InputTransform {
    constexpr static size_t kInputDimensions = 41024;
    constexpr static size_t kHalfDimensions = 256;
    constexpr static size_t kOutputDimensions = 2 * kHalfDimensions;
    constexpr static uint32_t kHash = 0x5d69d5b8u ^ kOutputDimensions;  // 0x5d69d7b8u
    constexpr static size_t kFileSize = sizeof(uint32_t) + sizeof(int16_t) * kHalfDimensions +
        sizeof(int16_t) * kHalfDimensions * kInputDimensions;
    std::vector<int16_t> bias;     // [kHalfDimensions]
    std::vector<int16_t> weights;  // [kHalfDimensions][kInputDimensions]
    InputTransform() : bias(kHalfDimensions), weights(kHalfDimensions * kInputDimensions) {}
};

/** Affine layer: y = weights * x + bias */
struct AffineLayer {
    AffineLayer(size_t in, size_t out) : in(in), out(out), weights(in * out), bias(out) {}
    const size_t in;
    const size_t out;
    std::vector<int8_t> weights;  // flattened row-major: [out][in]
    std::vector<int32_t> bias;
};

struct Network {
    static constexpr std::array<size_t, 3> out = {32, 32, 1};
    static constexpr uint32_t kHash = 0x63337156u;
    std::vector<AffineLayer> layers = {AffineLayer(InputTransform::kOutputDimensions, out[0]),
                                       AffineLayer(out[0], out[1]),
                                       AffineLayer(out[1], out[2])};
};
struct FileHeader {
    static constexpr uint32_t kVersion = 0x7af32f16;
    static constexpr uint32_t kHash = InputTransform::kHash ^ Network::kHash;
    std::string name;  // name string, only for diagnostic purposes
};

/** NNUE network representation (no incremental accumulator yet) */
struct NNUE {
    static constexpr std::array<size_t, 3> out = {32, 32, 1};
    FileHeader header;
    InputTransform input;
    Network network;
};

NNUE loadNNUE(const std::string& filename);
}  // namespace nnue
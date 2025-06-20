#include <cstdint>
#include <string>
#include <vector>

/** Affine layer: y = weights * x + bias */
struct AffineLayer {
    std::vector<int16_t> weights;  // flattened row-major: [out][in]
    std::vector<int32_t> bias;
    int input_size = 0;
    int output_size = 0;
};

/** NNUE network representation (no incremental accumulator yet) */
struct NNUE {
    AffineLayer input_transform;
    std::vector<AffineLayer> hidden_layers;
};

NNUE load_nnue(const std::string& filename);
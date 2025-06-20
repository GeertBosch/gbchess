#include <fstream>
#include <iostream>
#include <stdexcept>

#include "nnue.h"

/** Read binary data (little endian) */
template <typename T>
T read_binary(std::ifstream& in) {
    T val;
    in.read(reinterpret_cast<char*>(&val), sizeof(T));
    if (!in) throw std::runtime_error("Unexpected EOF");
    return val;
}

NNUE load_nnue(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open NNUE file: " + filename);

    // Verify header
    char magic[8];
    file.read(magic, 8);
    if (std::string(magic, 8) != "NNUEFILT") throw std::runtime_error("Invalid NNUE file format");

    uint32_t version = read_binary<uint32_t>(file);
    std::cerr << "Loading NNUE version: " << version << "\n";
    // Skip description (typically 128 bytes)
    file.ignore(128);

    NNUE net;

    // Input transform
    {
        auto& layer = net.input_transform;
        layer.input_size = read_binary<uint32_t>(file);
        layer.output_size = read_binary<uint32_t>(file);

        size_t weight_count = size_t(layer.input_size) * layer.output_size;
        layer.weights.resize(weight_count);
        layer.bias.resize(layer.output_size);

        file.read(reinterpret_cast<char*>(layer.weights.data()), weight_count * sizeof(int16_t));
        file.read(reinterpret_cast<char*>(layer.bias.data()), layer.output_size * sizeof(int32_t));
    }

    // Hidden layers (typically 2 or 3)
    while (file) {
        AffineLayer layer;
        layer.input_size = read_binary<uint32_t>(file);
        layer.output_size = read_binary<uint32_t>(file);

        size_t weight_count = size_t(layer.input_size) * layer.output_size;
        layer.weights.resize(weight_count);
        layer.bias.resize(layer.output_size);

        file.read(reinterpret_cast<char*>(layer.weights.data()), weight_count * sizeof(int16_t));
        file.read(reinterpret_cast<char*>(layer.bias.data()), layer.output_size * sizeof(int32_t));

        net.hidden_layers.push_back(std::move(layer));

        if (file.peek() == EOF) break;
    }

    return net;
}

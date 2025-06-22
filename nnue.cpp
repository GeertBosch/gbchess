#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "nnue.h"

namespace nnue {

namespace {
/** Read binary data (little endian) */
template <typename T>
T readBinary(std::ifstream& in) {
    T val;
    in.read(reinterpret_cast<char*>(&val), sizeof(T));
    if (!in) throw std::runtime_error("Unexpected EOF");
    return val;
}
/** Read binary vector (little endian) */
template <typename T>
void readBinaryVector(std::ifstream& in, T& vector) {
    in.read(reinterpret_cast<char*>(vector.data()), vector.size() * sizeof(typename T::value_type));
    if (!in) throw std::runtime_error("Unexpected EOF");
}

template <typename T>
std::string toHex(const T& val) {
    std::stringstream ss;
    ss << "0x" << std::hex << std::setw(2 * sizeof(T)) << std::setfill('0') << val << std::dec;
    return ss.str();
}

bool isPrintableAscii(const std::string& str) {
    for (char c : str)
        if (c < 32 || c > 126) return false;

    return true;
}
void checkHash(uint32_t expected, uint32_t actual, const std::string& what) {
    if (expected != actual)
        throw std::runtime_error("Invalid " + what + " hash: expected " + toHex(expected) +
                                 ", got " + toHex(actual));
}
FileHeader readHeader(std::ifstream& file) {
    FileHeader header;

    auto version = readBinary<uint32_t>(file);
    auto hash = readBinary<uint32_t>(file);
    auto size = readBinary<uint32_t>(file);
    header.name.resize(size);
    file.read(header.name.data(), size);  // Now read a string with the size just decoded before

    // If version or hash do not match, we can still try to read the name
    bool versionOK = version == nnue::FileHeader::kVersion;
    bool hashOK = hash == nnue::FileHeader::kHash;
    bool nameOK = isPrintableAscii(header.name);
    std::string ok[2] = {"not OK", "OK"};

    // For early diagnostics
    std::cout << "Version: " << std::hex << version << ", " << ok[versionOK] << "\n"
              << "Hash: " << std::hex << hash << ", " << ok[hashOK] << "\n"
              << "Name: " << (nameOK ? header.name : "not OK, " + std::to_string(size) + " bytes")
              << "\n";

    // Check for errors
    if (!nameOK) throw std::runtime_error("Invalid NNUE name (not printable ASCII)");
    if (!versionOK) throw std::runtime_error("Unsupported NNUE version: " + toHex(version));
    checkHash(nnue::FileHeader::kHash, hash, "NNUE file");

    return header;
}

void readInputTransform(std::ifstream& file, InputTransform& input) {
    checkHash(InputTransform::kHash, readBinary<uint32_t>(file), "InputTransform");
    readBinaryVector(file, input.bias);
    readBinaryVector(file, input.weights);
}

void readAffineTransform(std::ifstream& file, AffineLayer& layer) {
    readBinaryVector(file, layer.bias);
    readBinaryVector(file, layer.weights);
}

void readNetwork(std::ifstream& file, Network& net) {
    checkHash(Network::kHash, readBinary<uint32_t>(file), "Network");

    for (size_t i = 0; i < net.layers.size(); ++i) readAffineTransform(file, net.layers[i]);
}
}  // namespace

NNUE loadNNUE(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open NNUE file: " + filename);
    NNUE net;

    net.header = readHeader(file);
    readInputTransform(file, net.input);
    readNetwork(file, net.network);

    return net;
}
}  // namespace nnue
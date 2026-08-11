#include <iomanip>
#include <sstream>
#include <stdexcept>

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
    if (!in) throw std::runtime_error("Truncated SF16 NNUE header: no " + what);

    uint32_t value = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i) value |= uint32_t(bytes[i]) << (8 * i);

    return value;
}

bool isPrintableAscii(const std::string& str) {
    for (char c : str)
        if (c < 32 || c > 126) return false;

    return true;
}

}  // namespace

FileHeader readFileHeader(std::istream& in) {
    FileHeader header;

    header.version = readUint32(in, "version");
    header.hash = readUint32(in, "architecture hash");
    auto length = readUint32(in, "description length");

    if (header.version != FileHeader::kVersion)
        throw std::runtime_error("Unsupported SF16 NNUE version: " + toHex(header.version) +
                                 ", expected " + toHex(FileHeader::kVersion));
    if (header.hash != kBigArchitecture.hash())
        throw std::runtime_error("Unsupported SF16 NNUE architecture: " + toHex(header.hash) +
                                 ", expected " + toHex(kBigArchitecture.hash()));
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

}  // namespace nnue::sf16

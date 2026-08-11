#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "core/core.h"
#include "eval/nnue/sf16.h"

namespace {

/** The default Big network of Stockfish 16.1, used when no file name is given. */
const std::string kBigNetworkFile = "nn-b1a57edbea57.nnue";

/** Serialize a header as it appears in a file: three little endian words plus the description. */
std::string makeHeader(uint32_t version, uint32_t hash, uint32_t length, const std::string& desc) {
    std::string bytes;
    for (uint32_t word : {version, hash, length})
        for (int i = 0; i < 4; ++i) bytes += char(word >> (8 * i));

    return bytes + desc;
}

/** A well formed header for the Big network, from which the negative cases are derived. */
std::string goodHeader(const std::string& desc = "test network") {
    return makeHeader(
        nnue::sf16::FileHeader::kVersion, nnue::sf16::kBigArchitecture.hash(), desc.size(), desc);
}

/** Check that reading the given bytes is rejected, and report the diagnostic it produced. */
void expectRejected(const std::string& what, const std::string& bytes) {
    std::istringstream in(bytes);
    try {
        nnue::sf16::readFileHeader(in);
    } catch (const std::runtime_error& e) {
        std::cout << "Rejected " << what << ": " << e.what() << "\n";
        return;
    }
    std::cerr << "Accepted " << what << ", but it should have been rejected\n";
    assert(false && "malformed header accepted");
}

void testSyntheticHeader() {
    std::istringstream in(goodHeader());
    auto header = nnue::sf16::readFileHeader(in);
    assert(header.version == nnue::sf16::FileHeader::kVersion);
    assert(header.hash == nnue::sf16::kBigArchitecture.hash());
    assert(header.description == "test network");

    // Nothing beyond the header is consumed, so the rest of the file is left for the caller.
    std::istringstream trailing(goodHeader() + "parameters");
    nnue::sf16::readFileHeader(trailing);
    std::string rest;
    trailing >> rest;
    assert(rest == "parameters");

    std::cout << "Synthetic header accepted and fully consumed\n";
}

void testMalformedHeaders() {
    auto good = goodHeader();

    expectRejected("empty input", "");
    expectRejected("truncated version", good.substr(0, 2));
    expectRejected("truncated architecture hash", good.substr(0, 6));
    expectRejected("truncated description length", good.substr(0, 10));
    expectRejected("truncated description", good.substr(0, good.size() - 1));

    auto desc = std::string("test network");
    auto hash = nnue::sf16::kBigArchitecture.hash();
    expectRejected("SF12 version", makeHeader(0x7af32f16u, hash, desc.size(), desc));
    expectRejected("byte swapped version",
                   makeHeader(0x202ff37au, hash, desc.size(), desc));  // catches endianness slips
    expectRejected("wrong architecture hash",
                   makeHeader(nnue::sf16::FileHeader::kVersion, hash ^ 1, desc.size(), desc));
    expectRejected("differently sized accumulator",
                   makeHeader(nnue::sf16::FileHeader::kVersion,
                              nnue::sf16::Architecture{1024, 15, 32}.hash(),
                              desc.size(),
                              desc));
    expectRejected("empty description", makeHeader(nnue::sf16::FileHeader::kVersion, hash, 0, ""));
    expectRejected("absurd description length",
                   makeHeader(nnue::sf16::FileHeader::kVersion, hash, ~0u, desc));
    expectRejected("binary description",
                   makeHeader(nnue::sf16::FileHeader::kVersion, hash, 4, std::string("a\0bc", 4)));
}

void testNetworkFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open NNUE file: " + filename);

    auto header = nnue::sf16::readFileHeader(file);
    std::cout << "Read header of " << filename << ": " << header.description << "\n";

    assert(header.version == nnue::sf16::FileHeader::kVersion);
    assert(header.hash == 0x1c103072u);  // published hash of the SF16.1 Big network
    assert(header.description.size() > 16);
    assert(header.description.find("nnue-pytorch") != std::string::npos);

    // The same file with its last header byte missing must fail rather than silently truncate.
    file.seekg(0);
    std::string prefix(header.description.size() + 11, '\0');
    file.read(prefix.data(), prefix.size());
    assert(file && "network file is too short to hold its own header");
    expectRejected("truncated " + filename, prefix);
}

}  // namespace

int main(int argc, char* argv[]) try {
    testSyntheticHeader();
    testMalformedHeaders();

    if (argc <= 1)
        testNetworkFile(kBigNetworkFile);
    else
        while (++argv, --argc) testNetworkFile(*argv);

    std::cout << "All SF16 NNUE header tests passed!\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "SF16 NNUE header test failed: " << e.what() << "\n";
    return 1;
}

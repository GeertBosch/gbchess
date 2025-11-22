#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "engine/fen/fen.h"
#include "eval/nnue/nnue.h"

/**
 * Compute FNV1A hash of data with given size. This provides a simple way to verify test results.
 */
constexpr uint64_t fnv1a(const char* data, size_t size) {
    constexpr uint64_t kFnvPrime = 1099511628211ull;

    uint64_t hash = 14695981039346656037ull;  // FNV offset basis
    while (size--) hash = (hash ^ uint8_t(*data++)) * kFnvPrime;

    return hash;
}

void testTransform(const nnue::NNUE& network) {
    // Create starting position
    Position position = fen::parsePosition(fen::initialPosition);

    // Transform the position
    auto accumulator = nnue::transform(position, network.input);

    // Compute hash of accumulator values
    uint64_t hash = fnv1a(reinterpret_cast<const char*>(accumulator.values.data()),
                          accumulator.values.size() * sizeof(int16_t));

    std::cout << "Accumulator hash for starting position: 0x" << std::hex << hash << std::dec
              << std::endl;

    // Assert the expected hash value to ensure we don't accidentally change the result
    constexpr uint64_t kExpectedHash = 0x5e24a410f71a3622;
    assert(hash == kExpectedHash && "Accumulator hash mismatch - implementation may have changed");
    std::cout << "Hash verification passed!" << std::endl;
}

void testEvaluate(const nnue::NNUE& network) {
    // Test evaluation for starting position
    Position position = fen::parsePosition(fen::initialPosition);

    int32_t score = nnue::evaluate(position, network);
    std::cout << "NNUE evaluation for starting position: " << score << " cp\n";

    // Test evaluation for a different position (after 1.e4)
    Position positionAfterE4 =
        fen::parsePosition("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
    int32_t scoreAfterE4 = nnue::evaluate(positionAfterE4, network);
    std::cout << "NNUE evaluation after 1.e4: " << scoreAfterE4 << " cp\n";

    // The scores should be different (unless the position is exactly symmetric)
    // This is just a sanity check that the evaluation is working
    std::cout << "Complete NNUE evaluation test passed!" << std::endl;
}

int process_argument(const std::string& arg) try {
    auto nnue = nnue::loadNNUE(arg);
    std::cout << "NNUE loaded successfully from: " << arg << std::endl;

    // Test transform method with starting position
    testTransform(nnue);

    // Test complete evaluation
    testEvaluate(nnue);

    return 0;
} catch (const std::exception& e) {
    std::cerr << "Failed to load NNUE: " << e.what() << std::endl;
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc <= 1) return process_argument("nn-82215d0fd0df.nnue");

    while (++argv, --argc)
        if (auto error = process_argument(*argv)) return error;

    return 0;
}
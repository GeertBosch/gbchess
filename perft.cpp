#include <chrono>
#include <cstdlib>  // For std::exit
#include <iostream>
#include <string>

#include "eval.h"
#include "fen.h"
#include "moves.h"

void perftWithDivide(Position position, int depth, int expectedCount) {
    struct Division {
        Move move;
        uint64_t count;
    };
    std::vector<Division> divisions;

    auto startTime = std::chrono::high_resolution_clock::now();
    for (auto& [move, newPosition] : allLegalMoves(position)) {
        auto count = perft(newPosition, depth - 1);
        std::cout << static_cast<std::string>(move) << ": " << count << std::endl;
        divisions.push_back({move, count});
    }
    auto count = perft(position, depth);
    auto endTime = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto rate = count / (duration.count() / 1000'000.0);  // evals per second

    std::cout << count << " nodes in " << duration.count() / 1000 << " ms @ " << rate / 1000.0
              << "K nodes/sec" << std::endl;

    if (expectedCount && count != expectedCount) {
        std::cerr << "Expected " << expectedCount << " nodes, got " << count << std::endl;
        std::exit(1);
    }
}

/**
 *  Return whether the given string is a FEN string rather than a number. Doesn't check for
 *  validity, just that it can't possibly be a valid number.
 */
bool maybeFEN(const std::string& str) {
    return str.find('/') != std::string::npos;
}

int main(int argc, char** argv) {

    std::vector<Position> positions;

    while (argc >= 2 && maybeFEN(argv[1])) {
        positions.push_back(fen::parsePosition(argv[1]));
        argv++;
        argc--;
    }

    if (positions.empty()) {
        positions.push_back(fen::parsePosition(fen::initialPosition));
    }

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <depth> [expected-count]" << std::endl;
        std::cerr << "Usage: " << argv[0] << " {fen} <depth> [expected-count]" << std::endl;
        std::exit(1);
    }

    int depth = std::atoi(argv[1]);
    int expectedCount = argc > 2 ? std::atoi(argv[2]) : 0;

    for (auto& position : positions) perftWithDivide(position, depth, expectedCount);
}
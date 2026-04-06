/**
 * Simplified perft implementation for move generation testing.
 *
 * This is a streamlined version of perft.cpp that:
 * - Uses 64-bit node counts instead of 128-bit
 * - Doesn't use hash table caching
 * - Uses makeMove/unmakeMove instead of incremental updates
 * - Takes exactly 2 arguments: FEN position and depth
 * - Outputs moves with counts and total nodes searched
 *
 * Usage: perft_simple <fen|startpos> <depth>
 * Example: perft_simple startpos 3
 */

#include <cstdlib>
#include <exception>
#include <iostream>

#include "core/core.h"
#include "engine/fen/fen.h"
#include "move/move.h"
#include "move/move_gen.h"

// As there is no caching, each increment will require multiple nanoseconds of work, so the counter
// will not overflow for thousands of years, no need to check for overflow in processing.
using NodeCount = uint64_t;

bool quiet = false;  // suppress divide output if true

/**
 * Simple perft implementation without caching or incremental updates.
 * Uses makeMove/unmakeMove for board state management.
 */
NodeCount perft(Position position, unsigned depth) {
    if (depth == 0) return 1;
    if (depth == 1) return moves::countLegalMovesAndCaptures(position);

    auto nodes = 0;
    for (auto move : moves::allLegalMovesAndCaptures(position))
        nodes += perft(moves::applyMove(position, move), depth - 1);

    return nodes;
}

/**
 * Perft with divide - shows node count for each root move.
 */
NodeCount perftWithDivide(Position position, unsigned depth) {
    if (depth == 0) return 1;

    NodeCount totalNodes = 0;
    for (auto move : moves::allLegalMovesAndCaptures(position)) {
        auto undo = moves::makeMove(position, move);
        NodeCount nodes = perft(position, depth - 1);
        totalNodes += nodes;
        moves::unmakeMove(position, undo);

        if (!quiet) std::cout << to_string(move) << ": " << nodes << "\n";
    }

    return totalNodes;
}

void error(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(1);
}

void usage() {
    std::cerr << "Usage: perft_simple <fen|startpos> <depth>" << std::endl;
    std::cerr << "Example: perft_simple startpos 3" << std::endl;
    std::exit(1);
}

void run(const std::string& fen, unsigned depth) {
    Position position = fen::parsePosition(fen);
    auto nodes = perft(position, depth);
    std::cout << "Nodes searched: " << nodes << "\n";
}

int main(int argc, char** argv) try {
    if (argc > 1 && !std::strcmp(argv[1], "-q")) --argc, ++argv, quiet = true;
    if (argc != 3) usage();

    std::string fen = argv[1];
    int depth = std::atoi(argv[2]);

    if (depth < 0) error("depth must be non-negative");

    run(fen, depth);
    return 0;
} catch (const std::exception& e) {
    error(e.what());
}

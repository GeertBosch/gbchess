/**
 * Simplified perft implementation for chess position analysis.
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

#include "common.h"
#include "fen.h"
#include "move/move.h"
#include "move/move_gen.h"
#include <cstdlib>
#include <iostream>
#include <string>

using NodeCount = uint64_t;  // Will not overflow for thousands of years, no need to check

/**
 * Simple perft implementation without caching or incremental updates.
 * Uses makeMove/unmakeMove for board state management.
 */
NodeCount perft(Position& position, int depth) {
    if (depth == 0) return 1;

    NodeCount nodes = 0;
    auto moveList = moves::allLegalMovesAndCaptures(position.turn, position.board);

    for (const auto& move : moveList) {
        auto undo = moves::makeMove(position, move);
        nodes += perft(position, depth - 1);
        moves::unmakeMove(position, undo);
    }

    return nodes;
}

/**
 * Perft with divide - shows node count for each root move.
 */
void perftWithDivide(Position position, int depth) {
    if (depth == 0) {
        std::cout << "Nodes searched: 1" << std::endl;
        return;
    }

    NodeCount totalNodes = 0;
    auto moveList = moves::allLegalMovesAndCaptures(position.turn, position.board);

    for (const auto& move : moveList) {
        auto undo = moves::makeMove(position, move);
        NodeCount nodes = perft(position, depth - 1);
        moves::unmakeMove(position, undo);

        std::cout << to_string(move) << ": " << nodes << std::endl;
        totalNodes += nodes;
    }

    std::cout << "Nodes searched: " << totalNodes << std::endl;
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

void run(const std::string& fen, int depth) try {
    Position position = fen::parsePosition(fen);
    perftWithDivide(position, depth);
} catch (const std::exception& e) {
    error(e.what());
}

int main(int argc, char** argv) {
    if (argc != 3) usage();

    std::string fen = argv[1];
    int depth = std::atoi(argv[2]);

    if (depth < 0) error("depth must be non-negative");

    run(fen, depth);
    return 0;
}

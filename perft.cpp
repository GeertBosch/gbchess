#include <chrono>
#include <cstdint>
#include <cstdlib>  // For std::exit
#include <iostream>
#include <string>

#include "fen.h"
#include "moves.h"

bool quiet = false;

/**
 *  a debugging function to walk the move generation tree of strictly legal moves to count all the
 *  leaf nodes of a certain depth, which can be compared to predetermined values and used to isolate
 *  bugs. (See https://www.chessprogramming.org/Perft)
 */
uint64_t perft(Board& board, SearchState& state, int depth) {
    if (--depth <= 0) return depth ? 1 : countLegalMovesAndCaptures(board, state);

    uint64_t nodes = 0;
    auto newState = state;
    forAllLegalMovesAndCaptures(board, state, [&](Board& board, MoveWithPieces mwp) {
        auto delta = occupancyDelta(mwp.move);
        newState.occupancy = !(state.occupancy ^ delta);
        newState.pawns = SquareSet::find(board, addColor(PieceType::PAWN, !state.active()));
        newState.turn = applyMove(state.turn, mwp);
        newState.kingSquare =
            *SquareSet::find(board, addColor(PieceType::KING, !state.active())).begin();
        newState.inCheck = isAttacked(board, newState.kingSquare, newState.occupancy);
        newState.pinned = pinnedPieces(board, newState.occupancy, newState.kingSquare);
        nodes += perft(board, newState, depth);
    });
    return nodes;
}

void perftWithDivide(Position position, int depth, size_t expectedCount) {
    struct Division {
        Move move;
        uint64_t count;
    };
    std::vector<Division> divisions;
    std::cout << "Fen: " << fen::to_string(position) << std::endl;

    auto startTime = std::chrono::high_resolution_clock::now();
    uint64_t count = 0;
    for (auto move : allLegalMovesAndCaptures(position.turn, position.board)) {
        auto newPosition = applyMove(position, move);
        auto newState = SearchState(newPosition.board, newPosition.turn);
        auto newCount = perft(newPosition.board, newState, depth - 1);
        if (!quiet) std::cout << to_string(move) << ": " << newCount << "\n";
        divisions.push_back({move, newCount});
        count += newCount;
    }

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

bool maybeMove(const std::string& str) {
    return !fen::maybeFEN(str) && str[0] >= 'a' && str[0] <= 'h' && str[1] >= '1' &&
        str[1] <= '8' && str[2] >= 'a' && str[2] <= 'h' && str[3] >= '1' && str[3] <= '8';
}

void error(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(1);
}

void option(const std::string& arg) {
    if (arg == "-q" || arg == "--quiet")
        quiet = true;
    else
        error("Unknown option: " + arg);
}

void usage() {
    std::cerr << "Usage: perft [-q] <depth> [expected-count]" << std::endl;
    std::cerr << "Usage: perft [-q] [fen] <depth> [expected-count]" << std::endl;
    std::exit(1);
}

int main(int argc, char** argv) {
    std::vector<Position> positions;

    auto shift = [&argc, &argv]() -> char* { return --argc, *++argv; };
    auto arg = [&argc, &argv]() -> std::string { return argc < 2 ? "" : std::string(argv[1]); };

    while (arg()[0] == '-') option(shift());

    while (fen::maybeFEN(arg())) {
        positions.push_back(fen::parsePosition(shift()));
        if (arg() == "moves") shift();

        while (maybeMove(arg())) {
            Move move = fen::parseUCIMove(positions.back().board, shift());
            auto pos = applyMove(positions.back(), move);
            std::cout << "applied move " << to_string(move) << std::endl;
            positions.back() = pos;
        }
    }

    if (positions.empty()) positions.push_back(fen::parsePosition(fen::initialPosition));

    if (argc < 2) usage();

    int depth = std::atoi(shift());
    int expectedCount = argc > 1 ? std::atoi(shift()) : 0;

    for (auto& position : positions) perftWithDivide(position, depth, expectedCount);
}
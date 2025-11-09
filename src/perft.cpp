#include "common.h"
#include "options.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "fen.h"
#include "moves.h"
#include "moves_gen.h"
#include "perft_core.h"

bool quiet = false;

void error(const std::string& message) {
    std::cerr << "Error: " << message << std::endl;
    std::exit(1);
}

std::string pct(double some, double all) {
    double percentage = all > 0 ? some * 100.0 / all : 0.0;
    if (!all) return "";
    std::ostringstream oss;
    oss << " " << std::fixed << std::setprecision(1) << percentage << "%";
    return oss.str();
}

void perftWithDivide(Position position, int depth, NodeCount expectedCount) {
    struct Division {
        Move move;
        NodeCount count;
    };
    std::vector<Division> divisions;

    auto startTime = std::chrono::high_resolution_clock::now();
    NodeCount count = 0;
    for (auto move : moves::allLegalMovesAndCaptures(position.turn, position.board)) {
        auto newCount = perft(moves::applyMove(position, move), depth - 1, [move](NodeCount count) {
            if (!quiet)
                std::cerr << "\r" << to_string(move) << ": " << to_string(count) << std::flush;
        });
        if (!quiet) std::cerr << "\r" << std::string(20, ' ') << "\r";
        if (!quiet) std::cout << to_string(move) << ": " << to_string(newCount) << "\n";
        divisions.push_back({move, newCount});
        if (count + newCount < count) error("Node count overflow");
        count += newCount;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto rate = count / (duration.count() / 1000'000.0);  // evals per second
    auto megarate = std::round(rate / 100'000.0) / 10.0;  // mega evals per second

    std::cout << "Nodes searched: " << to_string(count) << "\n";
    if (expectedCount && count != expectedCount)
        error("Expected " + to_string(expectedCount) + " nodes, got " + to_string(count) + ".\n");

    if (quiet) return;

    std::cout << duration.count() / 1000 << " ms @ " << std::fixed << std::setprecision(1)
              << megarate << "M nodes/sec";
    if (options::cachePerft) std::cout << ", " << pct(getPerftCached(), count) << " cached";
    std::cout << "\n";
}

bool maybeMove(const std::string& str) {
    return !fen::maybeFEN(str) && str[0] >= 'a' && str[0] <= 'h' && str[1] >= '1' &&
        str[1] <= '8' && str[2] >= 'a' && str[2] <= 'h' && str[3] >= '1' && str[3] <= '8';
}

void usage(std::string message) {
    std::cerr << "Error: " << message << "\n";
    std::cerr << "Usage: perft [-q] <depth> [expected-count]" << "\n";
    std::cerr << "Usage: perft [-q] [fen] <depth> [expected-count]" << "\n";
    std::exit(1);
}

void option(const std::string& arg) {
    if (arg == "-q" || arg == "--quiet")
        quiet = true;
    else
        error("Unknown option: " + arg);
}

int main(int argc, char** argv) {
    std::vector<Position> positions;

    auto shift = [&argc, &argv]() -> char* { return --argc, *++argv; };
    auto arg = [&argc, &argv]() -> std::string { return argc < 2 ? "" : std::string(argv[1]); };

    while (arg()[0] == '-') option(shift());

    if (argc < 2) usage("missing depth argument");

    while (fen::maybeFEN(arg())) {
        positions.push_back(fen::parsePosition(shift()));
        if (arg() == "moves") shift();

        while (maybeMove(arg())) {
            Move move = fen::parseUCIMove(positions.back().board, shift());
            auto pos = moves::applyMove(positions.back(), move);
            std::cout << "applied move " << to_string(move) << "\n";
            positions.back() = pos;
        }
    }

    if (positions.empty()) positions.push_back(fen::parsePosition(fen::initialPosition));

    if (argc < 2) usage("missing depth argument");

    int depth = std::atoi(shift());
    NodeCount expectedCount = argc > 1 ? strtou128(shift()) : 0;

    for (auto& position : positions) perftWithDivide(position, depth, expectedCount);
    return 0;
}

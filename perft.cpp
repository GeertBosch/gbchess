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
    std::cout << "Fen: " << fen::to_string(position) << std::endl;

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

bool maybeMove(const std::string& str) {
    return !maybeFEN(str) && str[0] >= 'a' && str[0] <= 'h' && str[1] >= '1' && str[1] <= '8' &&
        str[2] >= 'a' && str[2] <= 'h' && str[3] >= '1' && str[3] <= '8';
}

Move parseMove(const Board& board, const std::string& str) {
    assert(str.length() >= 4);
    auto from = Square(str[1] - '1', str[0] - 'a');
    auto to = Square(str[3] - '1', str[2] - 'a');
    auto piece = board[from];
    auto kind = MoveKind::QUIET_MOVE;

    if (board[to] != Piece::NONE) {
        kind = MoveKind::CAPTURE;
    }

    if (piece == Piece::WHITE_PAWN || piece == Piece::BLACK_PAWN) {
        if (std::abs(to.rank() - from.rank()) == 2) kind = MoveKind::DOUBLE_PAWN_PUSH;
        if (to.file() != from.file() && board[to] == Piece::NONE) kind = MoveKind::EN_PASSANT;
    }
    if (piece == Piece::WHITE_KING || piece == Piece::BLACK_KING) {
        if (to.file() - from.file() == 2)
            kind = MoveKind::KING_CASTLE;
        else if (from.file() - to.file() == 2)
            kind = MoveKind::QUEEN_CASTLE;
    }

    return Move(Square(str[1] - '1', str[0] - 'a'), Square(str[3] - '1', str[2] - 'a'), kind);
}

int main(int argc, char** argv) {

    std::vector<Position> positions;

    while (argc >= 2 && maybeFEN(argv[1])) {
        positions.push_back(fen::parsePosition(argv[1]));
        argv++;
        argc--;
        if (argc > 2 && std::string(argv[1]) == "moves") {
            argv++;
            argc--;
        }
        while (argc >= 2 && maybeMove(argv[1])) {
            Move move = parseMove(positions.back().board, argv[1]);
            auto pos = applyMove(positions.back(), move);
            std::cout << "applied move " << static_cast<std::string>(move) << std::endl;
            positions.pop_back();
            positions.push_back(pos);
            argv++;
            argc--;
        }
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
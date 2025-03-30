#include <chrono>
#include <cstdint>
#include <cstdlib>  // For std::exit
#include <iostream>
#include <string>

#include "fen.h"
#include "moves.h"

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
        nodes += perft(board, newState, depth);
    });
    return nodes;
}

void perftWithDivide(Position position, int depth, int expectedCount) {
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
        if (debug) std::cout << static_cast<std::string>(move) << ": " << newCount << std::endl;
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
    auto from = Square(str[0] - 'a', str[1] - '1');
    auto to = Square(str[2] - 'a', str[3] - '1');
    auto piece = board[from];
    auto kind = MoveKind::QUIET_MOVE;

    if (board[to] != Piece::_) {
        kind = MoveKind::CAPTURE;
    }

    if (piece == Piece::P || piece == Piece::p) {
        if (std::abs(to.rank() - from.rank()) == 2) kind = MoveKind::DOUBLE_PAWN_PUSH;
        if (to.file() != from.file() && board[to] == Piece::_) kind = MoveKind::EN_PASSANT;
    }
    if (piece == Piece::K || piece == Piece::k) {
        if (to.file() - from.file() == 2)
            kind = MoveKind::KING_CASTLE;
        else if (from.file() - to.file() == 2)
            kind = MoveKind::QUEEN_CASTLE;
    }

    return Move(from, to, kind);
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
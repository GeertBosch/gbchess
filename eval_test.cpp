#include <chrono>
#include <cstdlib>  // For std::exit
#include <iostream>
#include <string>

#include "eval.h"
#include "fen.h"
#include "moves.h"

std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    os << "[";
    for (const auto& move : moves) {
        if (move) os << std::string(move) << ", ";
    }
    os << "]";
    return os;
}

template <typename F>
void printEvalRate(const F& fun) {
    auto startTime = std::chrono::high_resolution_clock::now();
    auto startEvals = evalCount;
    auto startCache = cacheCount;
    fun();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto endEvals = evalCount;
    auto endCache = cacheCount;

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto evals = endEvals - startEvals;
    auto cached = endCache - startCache;
    auto evalRate = evals / (duration.count() / 1000'000.0);  // evals per second

    std::cerr << evals << " evals, " << cached << " cached in " << duration.count() / 1000
              << " ms @ " << evalRate / 1000.0 << "K evals/sec" << std::endl;
}

void printAvailableMoves(const Position& position) {
    MoveVector moves;
    addAvailableMoves(moves, position.board, position.activeColor);
    std::cout << "Moves: " << moves << std::endl;
}

void printAvailableCaptures(const Position& position) {
    MoveVector captures;
    addAvailableCaptures(captures, position.board, position.activeColor);
    std::cout << "Captures: " << captures << std::endl;
}

void printBestMove(const Position& position, int maxdepth) {
    ComputedMoveVector moves;
    moves.push_back({Move(), position});
    auto bestMove = computeBestMove(moves, maxdepth);
    std::cout << "Best Move: " << static_cast<std::string>(bestMove) << std::endl;
}

/**
 * Prints the chess board to the specified output stream in grid notation.
 */
void printBoard(std::ostream& os, const Board& board) {
    for (int rank = 7; rank >= 0; --rank) {
        os << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            auto piece = board[Square(rank, file)];
            os << ' ' << to_char(piece);
        }
        os << std::endl;
    }
    os << "   ";
    for (char file = 'a'; file <= 'h'; ++file) {
        os << ' ' << file;
    }
    os << std::endl;
}


void testFromStdIn(int depth) {
    // While there is input on stdin, read a line, parse it as a FEN string and print the best move.
    while (std::cin) {
        std::string fen;
        std::getline(std::cin, fen);

        if (fen.empty())
            continue;

        // Parse the FEN string into a Position
        std::cerr << fen << std::endl;
        Position position = parseFEN(fen);
        auto startTime = std::chrono::high_resolution_clock::now();

        // Print the board in grid notation
        printBoard(std::cerr, position.board);

        // Compute the best move
        EvaluatedMove bestMove;
        printEvalRate([&]() {
            ComputedMoveVector moves;
            moves.push_back({Move(), position});
            bestMove = computeBestMove(moves, depth);
            // Print the best move and its evaluation
            std::cout << static_cast<std::string>(bestMove) << std::endl;
            std::cerr << "Solution: " << std::string(bestMove) << "\t";
        });
    }
}

void testEvaluatedMove() {
    {
        EvaluatedMove none;
        EvaluatedMove mateIn3 = {Move("f6"_sq, "e5"_sq, Move::QUIET), false, false, 999, 5};
        EvaluatedMove mateIn1 = {Move("e7"_sq, "g7"_sq, Move::QUIET), true, true, 999, 1};
        assert(none < mateIn3);
        assert(mateIn3 < mateIn1);
        assert(none < mateIn1);
    }
    {
        EvaluatedMove stalemate = {Move("f6"_sq, "e5"_sq, Move::QUIET), false, true, 0, 3};
        EvaluatedMove upQueen = {Move("f7"_sq, "a2"_sq, Move::CAPTURE), false, false, 9, 6};
        assert(stalemate < upQueen);
    }
    std::cout << "EvaluatedMove tests passed" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        int depth = std::stoi(argv[1]);
        testFromStdIn(depth);
        std::exit(0);
    }
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " [FEN-string] <search-depth>" << std::endl;
        std::exit(1);
    }

    testEvaluatedMove();

    std::string fen(argv[1]);
    int depth = std::stoi(argv[2]);

    // Parse the FEN string into a Position
    Position position = parseFEN(fen);

    // Print the board in grid notation
    printBoard(std::cout, position.board);

    // Evaluate the board
    std::cout << "Board Evaluation: " << evaluateBoard(position.board) << std::endl;

    printAvailableCaptures(position);
    printAvailableMoves(position);
    printEvalRate([&]() { printBestMove(position, depth); });

    return 0;
}

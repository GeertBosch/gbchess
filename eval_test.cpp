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
        os << std::string(move) << ", ";
    }
    os << "]";
    return os;
}

template <typename F>
void printEvalRate(const F& fun) {
    auto startTime = std::chrono::high_resolution_clock::now();
    auto startEvals = evalCount;
    fun();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto endEvals = evalCount;

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    auto evals = endEvals - startEvals;
    auto evalRate = evals / (duration.count() / 1000'000.0);  // evals per second

    std::cerr << evals << " evals in " << duration.count() / 1000 << " ms @ " << evalRate / 1000.0
              << "K evals/sec" << std::endl;
}

void printAvailableMoves(const ChessPosition& position) {
    MoveVector moves;
    addAvailableMoves(moves, position.board, position.activeColor);
    std::cout << "Moves: " << moves << std::endl;
}

void printAvailableCaptures(const ChessPosition& position) {
    MoveVector captures;
    addAvailableCaptures(captures, position.board, position.activeColor);
    std::cout << "Captures: " << captures << std::endl;
}

void printBestMove(const ChessPosition& position, int depth) {
    auto bestMove = computeBestMove(position, depth);
    std::cout << "Best Move: " << static_cast<std::string>(bestMove) << std::endl;
}


void testFromStdIn(int depth) {
    // While there is input on stdin, read a line, parse it as a FEN string and print the best move.
    while (std::cin) {
        std::string fen;
        std::getline(std::cin, fen);

        if (fen.empty())
            continue;

        // Parse the FEN string into a ChessPosition
        std::cerr << fen << std::endl;
        ChessPosition position = parseFEN(fen);
        auto startTime = std::chrono::high_resolution_clock::now();

        // Print the board in grid notation
        printBoard(std::cerr, position.board);

        // Compute the best move
        EvaluatedMove bestMove;
        printEvalRate([&]() {
            bestMove = computeBestMove(position, depth);
            // Print the best move and its evaluation
            std::cout << static_cast<std::string>(bestMove) << std::endl;
            std::cerr << "Solution: " << std::string(bestMove) << "\t";
        });
    }
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

    std::string fen(argv[1]);
    int depth = std::stoi(argv[2]);

    // Parse the FEN string into a ChessPosition
    ChessPosition position = parseFEN(fen);

    // Print the board in grid notation
    printBoard(std::cout, position.board);

    // Evaluate the board
    std::cout << "Board Evaluation: " << evaluateBoard(position.board) << std::endl;

    printAvailableCaptures(position);
    printAvailableMoves(position);
    printEvalRate([&]() { printBestMove(position, depth); });

    return 0;
}

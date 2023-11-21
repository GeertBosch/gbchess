#include <iostream>
#include <string>
#include <map>
#include <cstdlib> // For std::exit

#include "eval.h"
#include "fen.h"

// ... [Other includes and definitions] ...
void testFromStdIn(int depth) {
    // While there is input on stdin, read a line, parse it as a FEN string and print the best move.
    while (std::cin) {
        std::string fen;
        std::getline(std::cin, fen);

        if (fen.empty())
            continue;

        // Parse the FEN string into a ChessPosition
        ChessPosition position = parseFEN(fen);

        // Compute the best move
        auto bestMove = computeBestMove(position, depth);

        // Print the best moves and their evaluations
        std::cout << static_cast<std::string>(bestMove) << std::endl;
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
    float evaluation = evaluateBoard(position.board);
    std::cout << "Board Evaluation: " << evaluation << std::endl;

    // Compute the best move
    auto bestMove = computeBestMove(position, depth);

    // Print the best moves and their evaluations
    std::cout << "Best Move and Evaluation:" << std::endl;
    std::cout << static_cast<std::string>(bestMove) << std::endl;

    return 0;
}

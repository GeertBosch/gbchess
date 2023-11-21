#include <iostream>
#include <string>
#include <map>
#include <cstdlib> // For std::exit

#include "eval.h"
#include "fen.h"

// ... [Other includes and definitions] ...

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <FEN-string> <search-depth>" << std::endl;
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

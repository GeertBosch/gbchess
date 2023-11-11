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

    // Evaluate the board
    float evaluation = evaluateBoard(position.board);
    std::cout << "Board Evaluation: " << evaluation << std::endl;

    // Compute the best move
    auto bestMoves = computeBestMoves(position, depth);

    // Print the best moves and their evaluations
    std::cout << "Best Moves and Evaluations:" << std::endl;
    for (auto& bestMove : bestMoves) {
        std::string moveStr = static_cast<std::string>(bestMove.first); // Use the string conversion operator
        std::cout << moveStr << " " << bestMove.second << std::endl;
    }

    return 0;
}

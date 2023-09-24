#include <map>
#include <set>

#include "common.h"
#include "moves.h"

/**
 * This function iterates over each square in the board, uses the pieceValues map to find
 * the value of the piece on that square, and adjusts the total value accordingly. White
 * pieces have positive values, and black pieces have negative values, so the returned value
 * represents the advantage to the white player: positive for white's advantage, negative
 * for black's advantage.
 */
float evaluateBoard(const ChessBoard& board) {
    float value = 0.0f;
    const std::map<char, float> pieceValues = {
        {'P', 1.0f}, {'N', 3.0f}, {'B', 3.0f}, {'R', 5.0f}, {'Q', 9.0f}, // White pieces
        {'p', -1.0f}, {'n', -3.0f}, {'b', -3.0f}, {'r', -5.0f}, {'q', -9.0f} // Black pieces
    };

    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            Square square = {rank, file};
            char piece = board[square];
            if(pieceValues.count(piece)) value += pieceValues.at(piece);
        }
    }

    return value;
}

bool isAttacked(const ChessBoard& board, const Square& square) {
    char piece = board[square];
    if (piece == ' ') return false; // The square is empty, so it is not attacked.

    char opponentColor = std::isupper(piece) ? 'b' : 'w';
    auto captures = availableCaptures(board, opponentColor);

    for(const auto& move : captures) {
        if(move.to == square)
            return true; // The square is attacked by some opponent piece.
    }
    return false; // The square is not attacked by any opponent piece.
}

std::set<Square> findPieces(const ChessBoard& board, char piece) {
    std::set<Square> squares;
    for(int rank = 0; rank < 8; ++rank) {
        for(int file = 0; file < 8; ++file) {
            Square sq{rank, file};
            if(board[sq] == piece) {
                squares.insert(sq);
            }
        }
    }
    return squares;
}

bool isInCheck(const ChessBoard& board, char activeColor) {
    char king = (activeColor == 'w') ? 'K' : 'k';
    auto kingSquares = findPieces(board, king);

    if(kingSquares.empty()) return false; // No king of the active color is present.

    Square kingSquare = *kingSquares.begin(); // Get the square where the king is located.
    return isAttacked(board, kingSquare);
}

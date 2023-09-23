#include <map>

#include "common.h"

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

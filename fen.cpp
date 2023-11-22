#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "common.h"

ChessBoard parsePiecePlacement(const std::string& piecePlacement) {
    ChessBoard board;

    int rank = 0, file = 0;

    for (char ch : piecePlacement) {
        if (ch == '/') {
            rank++;
            file = 0;
        } else if (std::isdigit(ch)) {
            file += ch - '0';  // Move the file by the number of empty squares
        } else {
            Square sq{7 - rank, file}; // Adjust the rank based on how it's stored in the ChessBoard
            board[sq] = ch;
            file++;
        }
    }

    return board;
}

ChessPosition parseFEN(const std::string& fen) {
    std::stringstream ss(fen);
    ChessPosition position;
    std::string piecePlacementStr;
    std::string activeColorStr;
    std::string castlingAvailabilityStr;
    std::string enPassantTargetStr;
    std::string halfmoveClockStr;

    // Read piece placement from the FEN string
    std::getline(ss, piecePlacementStr, ' ');
    position.board = parsePiecePlacement(piecePlacementStr);

    // Read other components of the FEN string
    ss >> activeColorStr
       >> castlingAvailabilityStr
       >> enPassantTargetStr
       >> position.halfmoveClock
       >> position.fullmoveNumber;

    position.activeColor = activeColorStr == "b" ? Color::BLACK : Color::WHITE;

    for (char ch : castlingAvailabilityStr) {
        switch (ch) {
            case 'K':
                position.castlingAvailability |= CastlingAvailability::WHITE_KINGSIDE;
                break;
            case 'Q':
                position.castlingAvailability |= CastlingAvailability::WHITE_QUEENSIDE;
                break;
            case 'k':
                position.castlingAvailability |= CastlingAvailability::BLACK_KINGSIDE;
                break;
            case 'q':
                position.castlingAvailability |= CastlingAvailability::BLACK_QUEENSIDE;
                break;
        }
    }

    if (enPassantTargetStr != "-") {
        int file = enPassantTargetStr[0] - 'a';
        int rank = enPassantTargetStr[1] - '1';
        position.enPassantTarget = Square{rank, file};
    }

    return position;
}

std::string toString(const ChessBoard& board) {
    std::stringstream fen;
    for (int rank = 7; rank >= 0; --rank) { // Start from the 8th rank and go downwards
        int emptyCount = 0; // Count of consecutive empty squares
        for (int file = 0; file < 8; ++file) {
            Square sq{rank, file}; // Adjust the rank based on how it's stored in the ChessBoard
            char piece = board[sq];
            if (piece == ' ') { // Empty square
                ++emptyCount;
            } else {
                if (emptyCount > 0) {
                    fen << emptyCount; // Add the count of empty squares
                    emptyCount = 0; // Reset the count
                }
                fen << piece; // Add the piece
            }
        }
        if (emptyCount > 0) fen << emptyCount; // Add remaining empty squares at the end of the rank
        if (rank > 0) fen << '/'; // Separate ranks by '/'
    }
    return fen.str();
}

std::ostream& operator<<(std::ostream& os, const ChessBoard& board) {
    os << toString(board);
    return os;
}


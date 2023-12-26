#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "common.h"

namespace fen {
Board parsePiecePlacement(const std::string& piecePlacement) {
    Board board;

    int rank = 0, file = 0;

    for (char ch : piecePlacement) {
        if (ch == '/') {
            rank++;
            file = 0;
        } else if (std::isdigit(ch)) {
            file += ch - '0';  // Move the file by the number of empty squares
        } else {
            Square sq{7 - rank, file};  // Adjust the rank based on how it's stored in the Board
            board[sq] = toPiece(ch);
            file++;
        }
    }

    return board;
}

Position parsePosition(const std::string& fen) {
    std::stringstream ss(fen);
    Position position;
    std::string piecePlacementStr;
    std::string activeColorStr;
    std::string castlingAvailabilityStr;
    std::string enPassantTargetStr;
    std::string halfmoveClockStr;
    std::string fullmoveNumberStr;

    // Read piece placement from the FEN string
    std::getline(ss, piecePlacementStr, ' ');
    position.board = parsePiecePlacement(piecePlacementStr);

    // Read other components of the FEN string
    ss >> activeColorStr >> castlingAvailabilityStr >> enPassantTargetStr >> halfmoveClockStr >>
        fullmoveNumberStr;

    position.activeColor = activeColorStr == "b" ? Color::BLACK : Color::WHITE;
    position.castlingAvailability = CastlingMask::NONE;
    for (char ch : castlingAvailabilityStr) {
        switch (ch) {
        case 'K': position.castlingAvailability |= CastlingMask::WHITE_KINGSIDE; break;
        case 'Q': position.castlingAvailability |= CastlingMask::WHITE_QUEENSIDE; break;
        case 'k': position.castlingAvailability |= CastlingMask::BLACK_KINGSIDE; break;
        case 'q': position.castlingAvailability |= CastlingMask::BLACK_QUEENSIDE; break;
        }
    }

    if (enPassantTargetStr != "-") {
        int file = enPassantTargetStr[0] - 'a';
        int rank = enPassantTargetStr[1] - '1';
        position.enPassantTarget = Square{rank, file};
    }

    position.halfmoveClock = std::stoi(halfmoveClockStr);
    position.fullmoveNumber = std::stoi(fullmoveNumberStr);

    return position;
}

std::string to_string(const Board& board) {
    std::stringstream fen;
    for (int rank = 7; rank >= 0; --rank) {  // Start from the 8th rank and go downwards
        int emptyCount = 0;                  // Count of consecutive empty squares
        for (int file = 0; file < 8; ++file) {
            Square sq{rank, file};  // Adjust the rank based on how it's stored in the Board
            auto piece = board[sq];
            if (piece == Piece::NONE) {  // Empty square
                ++emptyCount;
            } else {
                if (emptyCount > 0) {
                    fen << emptyCount;  // Add the count of empty squares
                    emptyCount = 0;     // Reset the count
                }
                fen << to_char(piece);  // Add the piece
            }
        }
        if (emptyCount > 0)
            fen << emptyCount;  // Add remaining empty squares at the end of the rank
        if (rank > 0)
            fen << '/';  // Separate ranks by '/'
    }
    return fen.str();
}

std::string to_string(const Position& position) {
    std::stringstream fen;
    fen << to_string(position.board) << " ";
    fen << to_string(position.activeColor) << " ";
    fen << to_string(position.castlingAvailability) << " ";
    fen << (position.enPassantTarget.index() ? std::string(position.enPassantTarget) : "-") << " ";
    fen << (int)position.halfmoveClock << " ";
    fen << position.fullmoveNumber;
    return fen.str();
}

}  // namespace fen
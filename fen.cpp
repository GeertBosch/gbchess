#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include "common.h"

ChessPosition parseFEN(const std::string& fen) {
    ChessPosition position;

    std::istringstream iss(fen);
    std::vector<std::string> tokens;
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    if (tokens.size() != 6) {
        // Handle error: Incorrect FEN format
        throw std::runtime_error("Incorrect FEN format");
    }

    position.piecePlacement = tokens[0];
    position.activeColor = tokens[1][0];
    position.castlingAvailability = tokens[2];
    position.enPassantTarget = tokens[3];
    position.halfmoveClock = std::stoi(tokens[4]);
    position.fullmoveNumber = std::stoi(tokens[5]);

    return position;
}

// Test
int testparse() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    ChessPosition position = parseFEN(fen);
    
    std::cout << "Piece Placement: " << position.piecePlacement << "\n";
    std::cout << "Active Color: " << position.activeColor << "\n";
    std::cout << "Castling Availability: " << position.castlingAvailability << "\n";
    std::cout << "En Passant Target: " << position.enPassantTarget << "\n";
    std::cout << "Halfmove Clock: " << position.halfmoveClock << "\n";
    std::cout << "Fullmove Number: " << position.fullmoveNumber << "\n";

    return 0;
}

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
            board.squares[rank][file] = ch;
            file++;
        }
    }

    return board;
}

std::string toString(const ChessBoard& board) {
    std::stringstream fen;
    for (int rank = 7; rank >= 0; --rank) { // Start from the 8th rank and go downwards
        int emptyCount = 0; // Count of consecutive empty squares
        for (int file = 0; file < 8; ++file) {
            Square sq{7 - rank, file}; // Adjust the rank based on how it's stored in the ChessBoard
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

#ifdef fen_TEST

#include <cassert>

void testFEN() {
    std::vector<std::string> testStrings = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
        "8/8/8/8/8/8/8/8",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
        "rnbqk2r/pppp1ppp/8/4p3/4P3/8/PPP2PPP/RNBQK2R", // Fixed the string
        "4k3/8/8/8/8/8/8/4K3",
        "8/8/8/8/8/8/8/8",
        "4k3/8/8/3Q4/8/8/8/4K3",
        "4k3/8/8/3q4/8/8/8/4K3",
    };

    for (const std::string& fen : testStrings) {
        ChessBoard board = parsePiecePlacement(fen);
        std::string roundTripped = toString(board);
        assert(fen == roundTripped);
    }

    std::cout << "All FEN tests passed!" << std::endl;
}

int main() {
    testFEN();
    testparse();
    return 0;
}

#endif

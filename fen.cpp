#include <iostream>
#include <string>
#include <sstream>
#include <vector>

struct ChessPosition {
    std::string piecePlacement;
    char activeColor;
    std::string castlingAvailability;
    std::string enPassantTarget;
    int halfmoveClock;
    int fullmoveNumber;
};

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

struct ChessBoard {
    char squares[8][8];

    ChessBoard() {
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                squares[i][j] = ' ';
            }
        }
    }
};

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

// Test
int main() {
    const std::string piecePlacement = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
    ChessBoard board = parsePiecePlacement(piecePlacement);

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            std::cout << board.squares[i][j] << ' ';
        }
        std::cout << '\n';
    }

    return 0;
}


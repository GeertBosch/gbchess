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
int main() {
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


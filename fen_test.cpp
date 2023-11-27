#include <cassert>
#include <iostream>
#include <vector>

#include "fen.h"

// Test
int testparse() {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    Position position = parseFEN(fen);

    std::cout << "Piece Placement: " << position.board << "\n";
    std::cout << "Active Color: " << to_string(position.activeColor) << "\n";
    std::cout << "Castling Availability: " << position.castlingAvailability << "\n";
    std::cout << "En Passant Target: " << std::string(position.enPassantTarget) << "\n";
    std::cout << "Halfmove Clock: " << position.halfmoveClock << "\n";
    std::cout << "Fullmove Number: " << position.fullmoveNumber << "\n";

    return 0;
}

void testFEN() {
    std::vector<std::string> testStrings = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
        "8/8/8/8/8/8/8/8",
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
        "rnbqk2r/pppp1ppp/8/4p3/4P3/8/PPP2PPP/RNBQK2R",  // Fixed the string
        "4k3/8/8/8/8/8/8/4K3",
        "8/8/8/8/8/8/8/8",
        "4k3/8/8/3Q4/8/8/8/4K3",
        "4k3/8/8/3q4/8/8/8/4K3",
    };

    for (const std::string& fen : testStrings) {
        Board board = parsePiecePlacement(fen);
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

#include <cassert>
#include <iostream>
#include <vector>

#include "fen.h"

// Test
int testparse() {
    const std::string fen = fen::initialPosition;
    Position position = fen::parsePosition(fen);

    std::cout << "Piece Placement: " << fen::to_string(position.board) << "\n";
    std::cout << "Active Color: " << to_string(position.activeColor) << "\n";
    std::cout << "Castling Availability: " << (int)position.castlingAvailability << "\n";
    std::cout << "En Passant Target: "
              << (position.enPassantTarget.index() ? std::string(position.enPassantTarget) : "-")
              << "\n";
    std::cout << "Halfmove Clock: " << (int)position.halfmoveClock << "\n";
    std::cout << "Fullmove Number: " << position.fullmoveNumber << "\n";

    return 0;
}

void testInitialPosition() {
    Board board(fen::parsePiecePlacement(fen::initialPiecePlacement));
    assert(board[Position::whiteKing] == Piece::WHITE_KING);
    assert(board[Position::whiteKingSideRook] == Piece::WHITE_ROOK);
    assert(board[Position::whiteQueenSideRook] == Piece::WHITE_ROOK);
    assert(board[Position::blackKing] == Piece::BLACK_KING);
    assert(board[Position::blackKingSideRook] == Piece::BLACK_ROOK);
    assert(board[Position::blackQueenSideRook] == Piece::BLACK_ROOK);

    Position position = fen::parsePosition(fen::initialPosition);
    assert(position.board == board);
    assert(position.activeColor == Color::WHITE);
    assert(position.castlingAvailability == CastlingMask::ALL);
    assert(position.enPassantTarget == Square(0));
    assert(position.halfmoveClock == 0);
    assert(position.fullmoveNumber == 1);
}

void testFENPiecePlacement() {
    std::vector<std::string> testStrings = {
        fen::emptyPiecePlacement,
        fen::initialPiecePlacement,
        "rnbqk2r/pppp1ppp/8/4p3/4P3/8/PPP2PPP/RNBQK2R",
        "4k3/8/8/8/8/8/8/4K3",
        "4k3/8/8/3Q4/8/8/8/4K3",
        "4k3/8/8/3q4/8/8/8/4K3",
    };

    for (const std::string& fen : testStrings) {
        Board board = fen::parsePiecePlacement(fen);
        std::string roundTripped = fen::to_string(board);
        assert(fen == roundTripped);
    }
}

void testFENPosition() {
    std::vector<std::string> testStrings = {
        fen::initialPosition,
        "4k3/8/8/2q5/5Pp1/8/7P/4K2R b Kkq f3 0 42",
        "rnbqk2r/pppp1ppp/8/4p3/4P3/8/PPP2PPP/RNBQK2R w KQkq - 0 1",
        "4k3/8/8/3Q4/8/8/8/4K3 w - - 0 1",
    };

    for (const std::string& fen : testStrings) {
        Position position = fen::parsePosition(fen);
        std::string roundTripped = fen::to_string(position);
        assert(fen == roundTripped);
    }
}

int main() {
    testparse();
    testInitialPosition();
    testFENPiecePlacement();
    std::cout << "All FEN tests passed!" << std::endl;
    return 0;
}

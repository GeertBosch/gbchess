#include <cassert>
#include <iostream>
#include <vector>

#include "fen.h"

// Test
int testparse() {
    const std::string fen = fen::initialPosition;
    Position position = fen::parsePosition(fen);
    Turn turn = position.turn;

    std::cout << "Piece Placement: " << fen::to_string(position.board) << "\n";
    std::cout << "Active Color: " << to_string(turn.active()) << "\n";
    std::cout << "Castling Availability: " << (int)turn.castling() << "\n";
    std::cout << "En Passant Target: "
              << (turn.enPassant().index() ? std::string(turn.enPassant()) : "-") << "\n";
    std::cout << "Halfmove Clock: " << (int)turn.halfmove() << "\n";
    std::cout << "Fullmove Number: " << turn.fullmove() << "\n";

    return 0;
}

void testInitialPosition() {
    Board board(fen::parsePiecePlacement(fen::initialPiecePlacement));
    assert(board[Position::whiteKing] == Piece::K);
    assert(board[Position::whiteKingSideRook] == Piece::R);
    assert(board[Position::whiteQueenSideRook] == Piece::R);
    assert(board[Position::blackKing] == Piece::k);
    assert(board[Position::blackKingSideRook] == Piece::r);
    assert(board[Position::blackQueenSideRook] == Piece::r);

    Position position = fen::parsePosition(fen::initialPosition);
    assert(position.board == board);
    Turn turn = position.turn;
    assert(turn.active() == Color::WHITE);
    assert(turn.castling() == CastlingMask::KQkq);
    assert(turn.enPassant() == Square(0));
    assert(turn.halfmove() == 0);
    assert(turn.fullmove() == 1);
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
    testFENPosition();
    std::cout << "All FEN tests passed!" << std::endl;
    return 0;
}

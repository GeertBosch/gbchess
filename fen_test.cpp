#include <cassert>
#include <iostream>
#include <vector>

#include "fen.h"

void testPiece() {
    // Test toPiece
    assert(toPiece('P') == Piece::P);
    assert(toPiece('N') == Piece::N);
    assert(toPiece('B') == Piece::B);
    assert(toPiece('R') == Piece::R);
    assert(toPiece('Q') == Piece::Q);
    assert(toPiece('K') == Piece::K);
    assert(toPiece('p') == Piece::p);
    assert(toPiece('n') == Piece::n);
    assert(toPiece('b') == Piece::b);
    assert(toPiece('r') == Piece::r);
    assert(toPiece('q') == Piece::q);
    assert(toPiece('k') == Piece::k);
    assert(toPiece(' ') == Piece::_);

    // Test to_char
    assert(to_char(Piece::P) == 'P');
    assert(to_char(Piece::N) == 'N');
    assert(to_char(Piece::B) == 'B');
    assert(to_char(Piece::R) == 'R');
    assert(to_char(Piece::Q) == 'Q');
    assert(to_char(Piece::K) == 'K');
    assert(to_char(Piece::p) == 'p');
    assert(to_char(Piece::n) == 'n');
    assert(to_char(Piece::b) == 'b');
    assert(to_char(Piece::r) == 'r');
    assert(to_char(Piece::q) == 'q');
    assert(to_char(Piece::k) == 'k');
    assert(to_char(Piece::_) == '.');

    std::cout << "All Piece tests passed!" << std::endl;
}

int testparse() {
    const std::string fen = fen::initialPosition;
    Position position = fen::parsePosition(fen);
    Turn turn = position.turn;

    std::cout << "Piece Placement: " << fen::to_string(position.board) << "\n";
    std::cout << "Active Color: " << to_string(turn.activeColor()) << "\n";
    std::cout << "Castling Availability: " << (int)turn.castling() << "\n";
    std::cout << "En Passant Target: "
              << (turn.enPassant() ? to_string(turn.enPassant()) : "-") << "\n";
    std::cout << "Halfmove Clock: " << (int)turn.halfmove() << "\n";
    std::cout << "Fullmove Number: " << turn.fullmove() << "\n";

    return 0;
}

void testInitialPosition() {
    Board board(fen::parsePiecePlacement(fen::initialPiecePlacement));
    assert(board[e1] == Piece::K);
    assert(board[h1] == Piece::R);
    assert(board[a1] == Piece::R);
    assert(board[e8] == Piece::k);
    assert(board[h8] == Piece::r);
    assert(board[a8] == Piece::r);

    Position position = fen::parsePosition(fen::initialPosition);
    assert(position.board == board);
    Turn turn = position.turn;
    assert(turn.activeColor() == Color::w);
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
    testPiece();
    testparse();
    testInitialPosition();
    testFENPiecePlacement();
    testFENPosition();
    std::cout << "All FEN tests passed!" << std::endl;
    return 0;
}

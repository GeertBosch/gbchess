#include <cassert>
#include <iostream>

#include "common.h"

void testColor() {
    assert(color(Piece::P) == Color::w);
    assert(color(Piece::N) == Color::w);
    assert(color(Piece::B) == Color::w);
    assert(color(Piece::R) == Color::w);
    assert(color(Piece::Q) == Color::w);
    assert(color(Piece::K) == Color::w);
    assert(color(Piece::p) == Color::b);
    assert(color(Piece::n) == Color::b);
    assert(color(Piece::b) == Color::b);
    assert(color(Piece::r) == Color::b);
    assert(color(Piece::q) == Color::b);
    assert(color(Piece::k) == Color::b);

    assert(addColor(PieceType::PAWN, Color::w) == Piece::P);
    assert(addColor(PieceType::KNIGHT, Color::w) == Piece::N);
    assert(addColor(PieceType::BISHOP, Color::w) == Piece::B);
    assert(addColor(PieceType::ROOK, Color::w) == Piece::R);
    assert(addColor(PieceType::QUEEN, Color::w) == Piece::Q);
    assert(addColor(PieceType::KING, Color::w) == Piece::K);
    assert(addColor(PieceType::PAWN, Color::b) == Piece::p);
    assert(addColor(PieceType::KNIGHT, Color::b) == Piece::n);
    assert(addColor(PieceType::BISHOP, Color::b) == Piece::b);
    assert(addColor(PieceType::ROOK, Color::b) == Piece::r);
    assert(addColor(PieceType::QUEEN, Color::b) == Piece::q);
    assert(addColor(PieceType::KING, Color::b) == Piece::k);

    std::cout << "All Color tests passed!" << std::endl;
}

void testPieceType() {
    assert(type(Piece::P) == PieceType::PAWN);
    assert(type(Piece::N) == PieceType::KNIGHT);
    assert(type(Piece::B) == PieceType::BISHOP);
    assert(type(Piece::R) == PieceType::ROOK);
    assert(type(Piece::Q) == PieceType::QUEEN);
    assert(type(Piece::K) == PieceType::KING);
    assert(type(Piece::p) == PieceType::PAWN);
    assert(type(Piece::n) == PieceType::KNIGHT);
    assert(type(Piece::b) == PieceType::BISHOP);
    assert(type(Piece::r) == PieceType::ROOK);
    assert(type(Piece::q) == PieceType::QUEEN);
    assert(type(Piece::k) == PieceType::KING);

    std::cout << "All PieceType tests passed!" << std::endl;
}

void testMove() {
    // Test constructor
    Move move(a2, a4, MoveKind::Quiet_Move);
    assert(move.from == a2);
    assert(move.to == a4);
    assert(move.kind == MoveKind::Quiet_Move);

    // Test operator==
    Move capture = Move(a2, a4, MoveKind::Capture);
    assert(move == Move(a2, a4, MoveKind::Quiet_Move));
    assert(!(move == capture));

    // Test conversion to std::string
    Move promotion = Move(a7, a8, MoveKind::Queen_Promotion);
    assert(to_string(promotion) == "a7a8q");
    assert(to_string(Move()) == "0000");

    std::cout << "All Move tests passed!" << std::endl;
}
void testSquare() {
    // Test constructor with rank and file
    Square square1 = makeSquare(3, 2);
    assert(rank(square1) == 2);
    assert(file(square1) == 3);
    assert(square1 == 19);

    // Test constructor with index
    Square square2 = Square(42);
    assert(rank(square2) == 5);
    assert(file(square2) == 2);
    assert(square2 == 42);

    // Test operator==
    assert(square1 == Square(19));
    assert(square2 == Square(42));
    assert(!(square1 == square2));

    // Test conversion to std::string
    assert(to_string(square1) == "d3");
    assert(to_string(square2) == "c6");

    std::cout << "All Square tests passed!" << std::endl;
}

int main() {
    testColor();
    testPieceType();
    testMove();
    testSquare();
    std::cout << "All common tests passed!" << std::endl;
    return 0;
}
#include <cassert>
#include <iostream>

#include "moves_table.h"

void testPossibleMoves() {
    // Test rook moves
    {
        auto moves = possibleMoves(Piece::R, a1);
        assert(moves.size() == 14);
        assert(moves.contains(h1));
        assert(moves.contains(a8));
    }

    // Test bishop moves
    {
        auto moves = possibleMoves(Piece::b, a1);
        assert(moves.size() == 7);
        assert(moves.contains(h8));
    }

    // Test knight moves
    {
        auto moves = possibleMoves(Piece::N, a1);
        assert(moves.size() == 2);
        assert(moves.contains(c2));
        assert(moves.contains(b3));
    }

    // Test king moves
    {
        auto moves = possibleMoves(Piece::k, a1);
        assert(moves.size() == 3);
    }

    // Test queen moves
    {
        auto moves = possibleMoves(Piece::Q, a1);
        assert(moves.size() == 21);
    }

    // Test white pawn moves
    {
        auto moves = possibleMoves(Piece::P, a2);
        assert(moves.size() == 2);
        assert(moves.contains(a3));
        assert(moves.contains(a4));
    }

    // Test black pawn moves
    {
        auto moves = possibleMoves(Piece::p, a7);
        assert(moves.size() == 2);
        assert(moves.contains(a6));
        assert(moves.contains(a5));
    }

    std::cout << "All possibleMoves tests passed!" << std::endl;
}

void testPossibleCaptures() {
    // Knight tests
    auto moves = possibleCaptures(Piece::N, makeSquare(4, 4));
    assert(moves.contains(makeSquare(3, 2)));
    assert(moves.contains(makeSquare(5, 2)));
    assert(moves.contains(makeSquare(3, 6)));
    assert(moves.contains(makeSquare(5, 6)));
    assert(moves.contains(makeSquare(2, 3)));
    assert(moves.contains(makeSquare(6, 3)));
    assert(moves.contains(makeSquare(2, 5)));
    assert(moves.contains(makeSquare(6, 5)));

    // Edge cases for knight
    moves = possibleCaptures(Piece::n, makeSquare(7, 7));
    assert(moves.contains(makeSquare(6, 5)));
    assert(moves.contains(makeSquare(5, 6)));

    // Bishop tests
    moves = possibleCaptures(Piece::B, makeSquare(4, 4));
    assert(moves.contains(makeSquare(3, 3)));
    assert(moves.contains(makeSquare(2, 2)));
    // ... Add more assertions for all possible squares

    // Rook tests
    moves = possibleCaptures(Piece::r, makeSquare(4, 4));
    assert(moves.contains(makeSquare(0, 4)));
    assert(moves.contains(makeSquare(1, 4)));
    assert(moves.contains(makeSquare(2, 4)));
    assert(moves.contains(makeSquare(3, 4)));
    // ... Add more assertions for all possible squares

    // Queen tests (combination of Rook and Bishop)
    moves = possibleCaptures(Piece::Q, makeSquare(4, 4));
    assert(moves.contains(makeSquare(3, 3)));
    assert(moves.contains(makeSquare(0, 4)));
    // ... Add more assertions for all possible squares

    // King tests
    moves = possibleCaptures(Piece::k, makeSquare(4, 4));
    assert(moves.contains(makeSquare(4, 3)));
    assert(moves.contains(makeSquare(3, 4)));
    // ... Add more assertions for all possible squares

    // Pawn tests (White Pawn)
    moves = possibleCaptures(Piece::P, makeSquare(4, 4));
    assert(moves.contains(makeSquare(3, 5)));
    assert(moves.contains(makeSquare(5, 5)));

    // Pawn tests (Black Pawn)
    moves = possibleCaptures(Piece::p, makeSquare(4, 4));
    assert(moves.contains(makeSquare(3, 3)));
    assert(moves.contains(makeSquare(5, 3)));

    std::cout << "All possibleCaptures tests passed!" << std::endl;
}

int main() {
    testPossibleMoves();
    testPossibleCaptures();
    std::cout << "All moves_table tests passed!" << std::endl;
    return 0;
}
#include <cassert>
#include <iostream>

#include "moves.h"

void testPossibleMoves() {
    // Test rook moves
    {
        auto moves = possibleMoves('R', Square(0, 0));
        assert(moves.size() == 14);
        assert(moves.count(Square(0, 7)) == 1);
        assert(moves.count(Square(7, 0)) == 1);
    }

    // Test bishop moves
    {
        auto moves = possibleMoves('B', Square(0, 0));
        assert(moves.size() == 7);
        assert(moves.count(Square(7, 7)) == 1);
    }

    // Test knight moves
    {
        auto moves = possibleMoves('N', Square(0, 0));
        assert(moves.size() == 2);
        assert(moves.count(Square(1, 2)) == 1);
        assert(moves.count(Square(2, 1)) == 1);
    }

    // Test king moves
    {
        auto moves = possibleMoves('K', Square(0, 0));
        assert(moves.size() == 3);
    }

    // Test queen moves
    {
        auto moves = possibleMoves('Q', Square(0, 0));
        assert(moves.size() == 21);
    }

    // Test white pawn moves
    {
        auto moves = possibleMoves('P', Square(1, 0));
        assert(moves.size() == 2);
        assert(moves.count(Square(2, 0)) == 1);
        assert(moves.count(Square(3, 0)) == 1);
    }

    // Test black pawn moves
    {
        auto moves = possibleMoves('p', Square(6, 0));
        assert(moves.size() == 2);
        assert(moves.count(Square(5, 0)) == 1);
        assert(moves.count(Square(4, 0)) == 1);
    }

    std::cout << "All possibleMoves tests passed!" << std::endl;
}

void testPossibleCaptures() {
    // Knight tests
    std::set<Square> moves = possibleCaptures('N', {4, 4});
    assert(moves.count({2, 3}));
    assert(moves.count({2, 5}));
    assert(moves.count({6, 3}));
    assert(moves.count({6, 5}));
    assert(moves.count({3, 2}));
    assert(moves.count({3, 6}));
    assert(moves.count({5, 2}));
    assert(moves.count({5, 6}));

    // Edge cases for knight
    moves = possibleCaptures('N', {7, 7});
    assert(moves.count({5, 6}));
    assert(moves.count({6, 5}));

    // Rook tests
    moves = possibleCaptures('R', {4, 4});
    assert(moves.count({4, 0}));
    assert(moves.count({4, 1}));
    assert(moves.count({4, 2}));
    assert(moves.count({4, 3}));
    // ... Add more assertions for all possible squares

    // Bishop tests
    moves = possibleCaptures('B', {4, 4});
    assert(moves.count({3, 3}));
    assert(moves.count({2, 2}));
    // ... Add more assertions for all possible squares

    // Queen tests (combination of Rook and Bishop)
    moves = possibleCaptures('Q', {4, 4});
    assert(moves.count({3, 3}));
    assert(moves.count({4, 0}));
    // ... Add more assertions for all possible squares

    // King tests
    moves = possibleCaptures('K', {4, 4});
    assert(moves.count({3, 4}));
    assert(moves.count({4, 3}));
    // ... Add more assertions for all possible squares

    // Pawn tests (White Pawn)
    moves = possibleCaptures('P', {4, 4});
    assert(moves.count({3, 3}));
    assert(moves.count({3, 5}));

    // Pawn tests (Black Pawn)
    moves = possibleCaptures('p', {4, 4});
    assert(moves.count({5, 3}));
    assert(moves.count({5, 5}));

    std::cout << "All possibleCaptures tests passed!" << std::endl;
}

int main() {
    testPossibleMoves();
    testPossibleCaptures();
    return 0;
}


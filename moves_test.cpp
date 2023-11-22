#include <cassert>
#include <iostream>

#include "moves.h"

std::string toString(SquareSet squares) {
    std::string str;
    for (Square sq : squares) {
        str += static_cast<std::string>(sq);
        str += " ";
    }
    str.pop_back(); // Remove the last space
    return str;
}

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
    auto moves = possibleCaptures('N', {4, 4});
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

void testSquare() {
    // Test constructor with rank and file
    Square square1(2, 3);
    assert(square1.rank() == 2);
    assert(square1.file() == 3);
    assert(square1.index() == 19);

    // Test constructor with index
    Square square2(42);
    assert(square2.rank() == 5);
    assert(square2.file() == 2);
    assert(square2.index() == 42);

    // Test operator<
    assert(square1 < square2);
    assert(!(square2 < square1));
    assert(!(square1 < square1));

    // Test operator==
    assert(square1 == Square(19));
    assert(square2 == Square(42));
    assert(!(square1 == square2));

    // Test conversion to std::string
    assert(static_cast<std::string>(square1) == "d3");
    assert(static_cast<std::string>(square2) == "c6");

    std::cout << "All Square tests passed!" << std::endl;
}

void testSquareSet() {
    SquareSet set;

    // Test empty set
    assert(set.empty());
    assert(set.size() == 0);

    // Test insert and count
    set.insert(Square(0));
    assert(!set.empty());
    assert(set.size() == 1);
    assert(set.count(Square(0)) == 1);
    assert(set.count(Square(1)) == 0);

    // Test insert and count with multiple squares
    set.insert(Square(1));
    set.insert(Square(4));
    assert(set.size() == 3);
    assert(set.count(Square(0)) == 1);
    assert(set.count(Square(1)) == 1);
    assert(set.count(Square(2)) == 0);
    assert(set.count(Square(3)) == 0);
    assert(set.count(Square(4)) == 1);

    // Test iterator
    set.insert(Square(9));
    SquareSet::iterator it = set.begin();
    assert(*it == Square(0));
    ++it;
    assert(*it == Square(1));
    ++it;
    assert(*it == Square(4));
    ++it;
    assert(it != set.end());
    assert(*it == Square(9));
    ++it;
    assert(it == set.end());

    // Test end iterator
    it = set.end();
    assert(it == set.end());

    // Test copy constructor
    SquareSet set2;
    set2.insert(Square(3));
    set2.insert(Square(4));
    set2.insert(Square(5));
    set.insert(set2.begin(), set2.end());
    assert(set.size() == 6);
    assert(set.count(Square(3)) == 1);
    assert(set.count(Square(4)) == 1);
    assert(set.count(Square(5)) == 1);

    std::cout << "All SquareSet tests passed!" << std::endl;
}

int main() {
    testSquare();
    testSquareSet();
    testPossibleMoves();
    testPossibleCaptures();
    std::cout << "All move tests passed!" << std::endl;
    return 0;
}


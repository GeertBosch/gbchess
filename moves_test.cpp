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

void testApplyMove() {
    // Test pawn move
    {
        ChessBoard board;
        board[Square(1, 0)] = 'P';
        applyMove(board, Move(Square(1, 0), Square(2, 0)));
        assert(board[Square(2, 0)] == 'P');
        assert(board[Square(1, 0)] == ' ');
    }

    // Test pawn capture
    {
        ChessBoard board;
        board[Square(1, 0)] = 'P';
        board[Square(2, 1)] = 'r'; // White pawn captures black rook
        applyMove(board, Move(Square(1, 0), Square(2, 1)));
        assert(board[Square(2, 1)] == 'P');
        assert(board[Square(1, 0)] == ' ');
    }

    // Test pawn promotion move
    {
        // White pawn promotion
        ChessBoard board;
        board[Square(6, 0)] = 'P';
        applyMove(board, Move(Square(6, 0), Square(7, 0), PieceType::QUEEN));
        assert(board[Square(7, 0)] == 'Q');
        assert(board[Square(6, 0)] == ' ');

        // Black pawn promotion
        board[Square(1, 0)] = 'p';
        applyMove(board, Move(Square(1, 0), Square(0, 0), PieceType::ROOK));
        assert(board[Square(0, 0)] == 'r');
        assert(board[Square(1, 0)] == ' ');
    }

    // Test pawn promotion capture
    {
        // White pawn promotion
        ChessBoard board;
        board[Square(6, 0)] = 'P';
        board[Square(7, 1)] = 'r'; // White pawn captures black rook
        applyMove(board, Move(Square(6, 0), Square(7, 1), PieceType::BISHOP));
        assert(board[Square(7, 1)] == 'B');
        assert(board[Square(6, 0)] == ' ');

        // Black pawn promotion
        board[Square(1, 0)] = 'p';
        board[Square(0, 1)] = 'R'; // Black pawn captures white rook
        applyMove(board, Move(Square(1, 0), Square(0, 1), PieceType::KNIGHT));
        assert(board[Square(0, 1)] == 'n');
        assert(board[Square(1, 0)] == ' ');
    }

    // Test rook move
    {
        ChessBoard board;
        board[Square(7, 0)] = 'R';
        applyMove(board, Move(Square(7, 0), Square(7, 7)));
        assert(board[Square(7, 7)] == 'R');
        assert(board[Square(7, 0)] == ' ');
    }

    // Test rook capture
    {
        ChessBoard board;
        board[Square(7, 0)] = 'r';
        board[Square(0, 0)] = 'R'; // Black rook captures white rook
        applyMove(board, Move(Square(7, 0), Square(0, 0)));
        assert(board[Square(0, 0)] == 'r');
        assert(board[Square(7, 0)] == ' ');
    }

    // Test knight move
    {
        ChessBoard board;
        board[Square(0, 1)] = 'N';
        applyMove(board, Move(Square(0, 1), Square(2, 2)));
        assert(board[Square(2, 2)] == 'N');
        assert(board[Square(0, 1)] == ' ');
    }

    // Test knight capture
    {
        ChessBoard board;
        board[Square(0, 1)] = 'N';
        board[Square(2, 2)] = 'r'; // White knight captures black rook
        applyMove(board, Move(Square(0, 1), Square(2, 2)));
        assert(board[Square(2, 2)] == 'N');
        assert(board[Square(0, 1)] == ' ');
    }

    // Now test the ChessPosition applyMove function for a pawn move
    {
        ChessPosition position;
        position.board[Square(1, 0)] = 'P';
        position.activeColor = Color::WHITE;
        position.halfmoveClock = 1;
        applyMove(position, Move(Square(1, 0), Square(2, 0)));
        assert(position.board[Square(2, 0)] == 'P');
        assert(position.board[Square(1, 0)] == ' ');
        assert(position.activeColor == Color::BLACK);
        assert(position.halfmoveClock == 0);
    }

    // Test pawn capture
    {
        ChessPosition position;
        position.board[Square(1, 0)] = 'P';
        position.board[Square(2, 1)] = 'r'; // White pawn captures black rook
        position.activeColor = Color::WHITE;
        position.halfmoveClock = 1;
        applyMove(position, Move(Square(1, 0), Square(2, 1)));
        assert(position.board[Square(2, 1)] == 'P');
        assert(position.board[Square(1, 0)] == ' ');
        assert(position.activeColor == Color::BLACK);
        assert(position.halfmoveClock == 0);
    }

    // Test that the fullmove number is updated correctly on a black move
    {
        ChessPosition position;
        position.board[Square(1, 0)] = 'p';
        position.activeColor = Color::BLACK;
        position.halfmoveClock = 1;
        position.fullmoveNumber = 1;
        applyMove(position, Move(Square(1, 0), Square(2, 0)));
        assert(position.board[Square(2, 0)] == 'p');
        assert(position.board[Square(1, 0)] == ' ');
        assert(position.activeColor == Color::WHITE);
        assert(position.halfmoveClock == 0);
        assert(position.fullmoveNumber == 2);
    }

    // Test that capture with a non-pawn piece also resets the halfmoveClock
    {
        ChessPosition position;
        position.board[Square(1, 0)] = 'P';
        position.board[Square(2, 1)] = 'r'; // White pawn captures black rook
        position.activeColor = Color::WHITE;
        position.fullmoveNumber = 1;
        position.halfmoveClock = 1;
        applyMove(position, Move(Square(1, 0), Square(2, 1)));
        assert(position.board[Square(2, 1)] == 'P');
        assert(position.board[Square(1, 0)] == ' ');
        assert(position.activeColor == Color::BLACK);
        assert(position.halfmoveClock == 0); // reset due to capture
        assert(position.fullmoveNumber == 1); // not updated on white move
    }

    // Test that a move with a non-pawn piece does not reset the halfmoveClock
    {
        ChessPosition position;
        position.board[Square(0, 1)] = 'N';
        position.activeColor = Color::WHITE;
        position.halfmoveClock = 1;
        applyMove(position, Move(Square(0, 1), Square(2, 2)));
        assert(position.board[Square(2, 2)] == 'N');
        assert(position.board[Square(0, 1)] == ' ');
        assert(position.activeColor == Color::BLACK);
        assert(position.halfmoveClock == 2);
    }

    std::cout << "All applyMove tests passed!" << std::endl;
}

int main() {
    testSquare();
    testSquareSet();
    testPossibleMoves();
    testPossibleCaptures();
    testApplyMove();
    std::cout << "All move tests passed!" << std::endl;
    return 0;
}


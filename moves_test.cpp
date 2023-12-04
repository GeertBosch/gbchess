#include <algorithm>
#include <cassert>
#include <iostream>

#include "moves.h"

std::string toString(SquareSet squares) {
    std::string str;
    for (Square sq : squares) {
        str += static_cast<std::string>(sq);
        str += " ";
    }
    str.pop_back();  // Remove the last space
    return str;
}

void testPiece() {
    // Test toPiece
    assert(toPiece('P') == Piece::WHITE_PAWN);
    assert(toPiece('N') == Piece::WHITE_KNIGHT);
    assert(toPiece('B') == Piece::WHITE_BISHOP);
    assert(toPiece('R') == Piece::WHITE_ROOK);
    assert(toPiece('Q') == Piece::WHITE_QUEEN);
    assert(toPiece('K') == Piece::WHITE_KING);
    assert(toPiece('p') == Piece::BLACK_PAWN);
    assert(toPiece('n') == Piece::BLACK_KNIGHT);
    assert(toPiece('b') == Piece::BLACK_BISHOP);
    assert(toPiece('r') == Piece::BLACK_ROOK);
    assert(toPiece('q') == Piece::BLACK_QUEEN);
    assert(toPiece('k') == Piece::BLACK_KING);
    assert(toPiece(' ') == Piece::NONE);

    // Test to_char
    assert(to_char(Piece::WHITE_PAWN) == 'P');
    assert(to_char(Piece::WHITE_KNIGHT) == 'N');
    assert(to_char(Piece::WHITE_BISHOP) == 'B');
    assert(to_char(Piece::WHITE_ROOK) == 'R');
    assert(to_char(Piece::WHITE_QUEEN) == 'Q');
    assert(to_char(Piece::WHITE_KING) == 'K');
    assert(to_char(Piece::BLACK_PAWN) == 'p');
    assert(to_char(Piece::BLACK_KNIGHT) == 'n');
    assert(to_char(Piece::BLACK_BISHOP) == 'b');
    assert(to_char(Piece::BLACK_ROOK) == 'r');
    assert(to_char(Piece::BLACK_QUEEN) == 'q');
    assert(to_char(Piece::BLACK_KING) == 'k');
    assert(to_char(Piece::NONE) == '.');

    std::cout << "All Piece tests passed!" << std::endl;
}

void testPieceType() {
    assert(type(Piece::WHITE_PAWN) == PieceType::PAWN);
    assert(type(Piece::WHITE_KNIGHT) == PieceType::KNIGHT);
    assert(type(Piece::WHITE_BISHOP) == PieceType::BISHOP);
    assert(type(Piece::WHITE_ROOK) == PieceType::ROOK);
    assert(type(Piece::WHITE_QUEEN) == PieceType::QUEEN);
    assert(type(Piece::WHITE_KING) == PieceType::KING);
    assert(type(Piece::BLACK_PAWN) == PieceType::PAWN);
    assert(type(Piece::BLACK_KNIGHT) == PieceType::KNIGHT);
    assert(type(Piece::BLACK_BISHOP) == PieceType::BISHOP);
    assert(type(Piece::BLACK_ROOK) == PieceType::ROOK);
    assert(type(Piece::BLACK_QUEEN) == PieceType::QUEEN);
    assert(type(Piece::BLACK_KING) == PieceType::KING);

    std::cout << "All PieceType tests passed!" << std::endl;
}

void testColor() {
    assert(color(Piece::WHITE_PAWN) == Color::WHITE);
    assert(color(Piece::WHITE_KNIGHT) == Color::WHITE);
    assert(color(Piece::WHITE_BISHOP) == Color::WHITE);
    assert(color(Piece::WHITE_ROOK) == Color::WHITE);
    assert(color(Piece::WHITE_QUEEN) == Color::WHITE);
    assert(color(Piece::WHITE_KING) == Color::WHITE);
    assert(color(Piece::BLACK_PAWN) == Color::BLACK);
    assert(color(Piece::BLACK_KNIGHT) == Color::BLACK);
    assert(color(Piece::BLACK_BISHOP) == Color::BLACK);
    assert(color(Piece::BLACK_ROOK) == Color::BLACK);
    assert(color(Piece::BLACK_QUEEN) == Color::BLACK);
    assert(color(Piece::BLACK_KING) == Color::BLACK);

    assert(addColor(PieceType::PAWN, Color::WHITE) == Piece::WHITE_PAWN);
    assert(addColor(PieceType::KNIGHT, Color::WHITE) == Piece::WHITE_KNIGHT);
    assert(addColor(PieceType::BISHOP, Color::WHITE) == Piece::WHITE_BISHOP);
    assert(addColor(PieceType::ROOK, Color::WHITE) == Piece::WHITE_ROOK);
    assert(addColor(PieceType::QUEEN, Color::WHITE) == Piece::WHITE_QUEEN);
    assert(addColor(PieceType::KING, Color::WHITE) == Piece::WHITE_KING);
    assert(addColor(PieceType::PAWN, Color::BLACK) == Piece::BLACK_PAWN);
    assert(addColor(PieceType::KNIGHT, Color::BLACK) == Piece::BLACK_KNIGHT);
    assert(addColor(PieceType::BISHOP, Color::BLACK) == Piece::BLACK_BISHOP);
    assert(addColor(PieceType::ROOK, Color::BLACK) == Piece::BLACK_ROOK);
    assert(addColor(PieceType::QUEEN, Color::BLACK) == Piece::BLACK_QUEEN);
    assert(addColor(PieceType::KING, Color::BLACK) == Piece::BLACK_KING);

    std::cout << "All Color tests passed!" << std::endl;
}

void testPossibleMoves() {
    // Test rook moves
    {
        auto moves = possibleMoves(Piece::WHITE_ROOK, "a1"_sq);
        assert(moves.size() == 14);
        assert(moves.contains("h1"_sq));
        assert(moves.contains("a8"_sq));
    }

    // Test bishop moves
    {
        auto moves = possibleMoves(Piece::BLACK_BISHOP, "a1"_sq);
        assert(moves.size() == 7);
        assert(moves.contains("h8"_sq));
    }

    // Test knight moves
    {
        auto moves = possibleMoves(Piece::WHITE_KNIGHT, "a1"_sq);
        assert(moves.size() == 2);
        assert(moves.contains("c2"_sq));
        assert(moves.contains("b3"_sq));
    }

    // Test king moves
    {
        auto moves = possibleMoves(Piece::BLACK_KING, "a1"_sq);
        assert(moves.size() == 3);
    }

    // Test queen moves
    {
        auto moves = possibleMoves(Piece::WHITE_QUEEN, "a1"_sq);
        assert(moves.size() == 21);
    }

    // Test white pawn moves
    {
        auto moves = possibleMoves(Piece::WHITE_PAWN, "a2"_sq);
        assert(moves.size() == 2);
        assert(moves.contains("a3"_sq));
        assert(moves.contains("a4"_sq));
    }

    // Test black pawn moves
    {
        auto moves = possibleMoves(Piece::BLACK_PAWN, "a7"_sq);
        assert(moves.size() == 2);
        assert(moves.contains("a6"_sq));
        assert(moves.contains("a5"_sq));
    }

    std::cout << "All possibleMoves tests passed!" << std::endl;
}

void testPossibleCaptures() {
    // Knight tests
    auto moves = possibleCaptures(Piece::WHITE_KNIGHT, {4, 4});
    assert(moves.contains({2, 3}));
    assert(moves.contains({2, 5}));
    assert(moves.contains({6, 3}));
    assert(moves.contains({6, 5}));
    assert(moves.contains({3, 2}));
    assert(moves.contains({3, 6}));
    assert(moves.contains({5, 2}));
    assert(moves.contains({5, 6}));

    // Edge cases for knight
    moves = possibleCaptures(Piece::BLACK_KNIGHT, {7, 7});
    assert(moves.contains({5, 6}));
    assert(moves.contains({6, 5}));

    // Bishop tests
    moves = possibleCaptures(Piece::WHITE_BISHOP, {4, 4});
    assert(moves.contains({3, 3}));
    assert(moves.contains({2, 2}));
    // ... Add more assertions for all possible squares

    // Rook tests
    moves = possibleCaptures(Piece::BLACK_ROOK, {4, 4});
    assert(moves.contains({4, 0}));
    assert(moves.contains({4, 1}));
    assert(moves.contains({4, 2}));
    assert(moves.contains({4, 3}));
    // ... Add more assertions for all possible squares

    // Queen tests (combination of Rook and Bishop)
    moves = possibleCaptures(Piece::WHITE_QUEEN, {4, 4});
    assert(moves.contains({3, 3}));
    assert(moves.contains({4, 0}));
    // ... Add more assertions for all possible squares

    // King tests
    moves = possibleCaptures(Piece::BLACK_KING, {4, 4});
    assert(moves.contains({3, 4}));
    assert(moves.contains({4, 3}));
    // ... Add more assertions for all possible squares

    // Pawn tests (White Pawn)
    moves = possibleCaptures(Piece::WHITE_PAWN, {4, 4});
    assert(moves.contains({3, 3}));
    assert(moves.contains({3, 5}));

    // Pawn tests (Black Pawn)
    moves = possibleCaptures(Piece::BLACK_PAWN, {4, 4});
    assert(moves.contains({5, 3}));
    assert(moves.contains({5, 5}));

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
    assert(set.contains(Square(0)));
    assert(!set.contains(Square(1)));

    // Test insert and count with multiple squares
    set.insert(Square(1));
    set.insert(Square(4));
    assert(set.size() == 3);
    assert(set.contains(Square(0)));
    assert(set.contains(Square(1)));
    assert(!set.contains(Square(2)));
    assert(!set.contains(Square(3)));
    assert(set.contains(Square(4)));

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
    assert(set.contains(Square(3)));
    assert(set.contains(Square(4)));
    assert(set.contains(Square(5)));

    std::cout << "All SquareSet tests passed!" << std::endl;
}
void testOccupancy() {
    {
        Board board;
        board["a3"_sq] = Piece::WHITE_PAWN;
        board["b4"_sq] = Piece::WHITE_PAWN;
        board["a1"_sq] = Piece::WHITE_ROOK;
        board["d2"_sq] = Piece::WHITE_KNIGHT;
        board["c2"_sq] = Piece::WHITE_BISHOP;
        board["d1"_sq] = Piece::WHITE_QUEEN;
        board["e1"_sq] = Piece::WHITE_KING;
        board["f2"_sq] = Piece::WHITE_BISHOP;
        board["g2"_sq] = Piece::WHITE_KNIGHT;
        board["h5"_sq] = Piece::WHITE_ROOK;
        board["d7"_sq] = Piece::BLACK_PAWN;
        board["e5"_sq] = Piece::BLACK_PAWN;
        board["f7"_sq] = Piece::BLACK_PAWN;
        board["g7"_sq] = Piece::BLACK_PAWN;
        board["h7"_sq] = Piece::BLACK_PAWN;
        board["a8"_sq] = Piece::BLACK_ROOK;
        board["b8"_sq] = Piece::BLACK_KNIGHT;
        board["c8"_sq] = Piece::BLACK_BISHOP;
        board["d8"_sq] = Piece::BLACK_QUEEN;
        board["e8"_sq] = Piece::BLACK_KING;
        auto squares = SquareSet::occupancy(board);
        assert(squares.size() == 20);
    }
    std::cout << "All occupancy tests passed!" << std::endl;
}

void testPromotionKind() {
    {
        Square from = "a7"_sq;
        Square to = "a8"_sq;
        Move move(from, to, MoveKind::QUEEN_PROMOTION);
        assert(move.isPromotion());
        assert(promotionType(move.kind) == PieceType::QUEEN);
    }
}

void testAddAvailableMoves() {
    auto find = [](MoveVector& moves, Move move) -> bool {
        return std::find(moves.begin(), moves.end(), move) != moves.end();
    };
    // Test pawn moves
    {
        Board board;
        board["a2"_sq] = Piece::WHITE_PAWN;
        board["a4"_sq] = Piece::WHITE_PAWN;  // Block the pawn, so there's no two-square move
        MoveVector moves;
        addAvailableMoves(moves, board, Color::WHITE);
        assert(moves.size() == 2);
        assert(find(moves, Move("a2"_sq, "a3"_sq, Move::QUIET)));
        assert(find(moves, Move("a4"_sq, "a5"_sq, Move::QUIET)));
    }

    std::cout << "All addAvailableMoves tests passed!" << std::endl;
}

void testApplyMove() {
    // Test pawn move
    {
        Board board;
        board["a2"_sq] = Piece::WHITE_PAWN;
        applyMove(board, Move("a2"_sq, "a3"_sq, Move::QUIET));
        assert(board["a3"_sq] == Piece::WHITE_PAWN);
        assert(board["a2"_sq] == Piece::NONE);
    }

    // Test pawn capture
    {
        Board board;
        board["a2"_sq] = Piece::WHITE_PAWN;
        board["b3"_sq] = Piece::BLACK_ROOK;  // White pawn captures black rook
        applyMove(board, Move("a2"_sq, "b3"_sq, Move::CAPTURE));
        assert(board["b3"_sq] == Piece::WHITE_PAWN);
        assert(board["a2"_sq] == Piece::NONE);
    }

    // Test pawn promotion move
    {
        // White pawn promotion
        Board board;
        board["a7"_sq] = Piece::WHITE_PAWN;
        auto move = Move("a7"_sq, "a8"_sq, MoveKind::QUEEN_PROMOTION);
        applyMove(board, move);
        assert(board["a8"_sq] == Piece::WHITE_QUEEN);
        assert(board["a7"_sq] == Piece::NONE);

        // Black pawn promotion
        board["a2"_sq] = Piece::BLACK_PAWN;
        applyMove(board, Move("a2"_sq, "a1"_sq, MoveKind::ROOK_PROMOTION));
        assert(board["a1"_sq] == Piece::BLACK_ROOK);
        assert(board["a2"_sq] == Piece::NONE);
    }

    // Test pawn promotion capture
    {
        // White pawn promotion
        Board board;
        board["a7"_sq] = Piece::WHITE_PAWN;
        board["b8"_sq] = Piece::BLACK_ROOK;  // White pawn captures black rook
        applyMove(board, Move("a7"_sq, "b8"_sq, MoveKind::BISHOP_PROMOTION_CAPTURE));
        assert(board["b8"_sq] == Piece::WHITE_BISHOP);
        assert(board["a7"_sq] == Piece::NONE);

        // Black pawn promotion
        board["a2"_sq] = Piece::BLACK_PAWN;
        board["b1"_sq] = Piece::WHITE_ROOK;  // Black pawn captures white rook
        applyMove(board, Move("a2"_sq, "b1"_sq, MoveKind::KNIGHT_PROMOTION_CAPTURE));
        assert(board["b1"_sq] == Piece::BLACK_KNIGHT);
        assert(board["a2"_sq] == Piece::NONE);
    }

    // Test rook move
    {
        Board board;
        board["a8"_sq] = Piece::WHITE_ROOK;
        applyMove(board, Move("a8"_sq, "h8"_sq, Move::QUIET));
        assert(board["h8"_sq] == Piece::WHITE_ROOK);
        assert(board["a8"_sq] == Piece::NONE);
    }

    // Test rook capture
    {
        Board board;
        board["a8"_sq] = Piece::BLACK_ROOK;
        board["a1"_sq] = Piece::WHITE_ROOK;  // Black rook captures white rook
        applyMove(board, Move("a8"_sq, "a1"_sq, Move::CAPTURE));
        assert(board["a1"_sq] == Piece::BLACK_ROOK);
        assert(board["a8"_sq] == Piece::NONE);
    }

    // Test knight move
    {
        Board board;
        board["b1"_sq] = Piece::WHITE_KNIGHT;
        applyMove(board, Move("b1"_sq, "c3"_sq, Move::QUIET));
        assert(board["c3"_sq] == Piece::WHITE_KNIGHT);
        assert(board["b1"_sq] == Piece::NONE);
    }

    // Test knight capture
    {
        Board board;
        board["b1"_sq] = Piece::WHITE_KNIGHT;
        board["c3"_sq] = Piece::BLACK_ROOK;  // White knight captures black rook
        applyMove(board, Move("b1"_sq, "c3"_sq, Move::CAPTURE));
        assert(board["c3"_sq] == Piece::WHITE_KNIGHT);
        assert(board["b1"_sq] == Piece::NONE);
    }

    // Now test the Position applyMove function for a pawn move
    {
        Position position;
        position.board["a2"_sq] = Piece::WHITE_PAWN;
        position.activeColor = Color::WHITE;
        position.halfmoveClock = 1;
        position = applyMove(position, Move("a2"_sq, "a3"_sq, Move::QUIET));
        assert(position.board["a3"_sq] == Piece::WHITE_PAWN);
        assert(position.board["a2"_sq] == Piece::NONE);
        assert(position.activeColor == Color::BLACK);
        assert(position.halfmoveClock == 0);
    }

    // Test pawn capture
    {
        Position position;
        position.board["a2"_sq] = Piece::WHITE_PAWN;
        position.board["b3"_sq] = Piece::BLACK_ROOK;  // White pawn captures black rook
        position.activeColor = Color::WHITE;
        position.halfmoveClock = 1;
        position = applyMove(position, Move("a2"_sq, "b3"_sq, Move::CAPTURE));
        assert(position.board["b3"_sq] == Piece::WHITE_PAWN);
        assert(position.board["a2"_sq] == Piece::NONE);
        assert(position.activeColor == Color::BLACK);
        assert(position.halfmoveClock == 0);
    }

    // Test that the fullmove number is updated correctly on a black move
    {
        Position position;
        position.board["a2"_sq] = Piece::BLACK_PAWN;
        position.activeColor = Color::BLACK;
        position.halfmoveClock = 1;
        position.fullmoveNumber = 1;
        position = applyMove(position, Move("a2"_sq, "a3"_sq, Move::QUIET));
        assert(position.board["a3"_sq] == Piece::BLACK_PAWN);
        assert(position.board["a2"_sq] == Piece::NONE);
        assert(position.activeColor == Color::WHITE);
        assert(position.halfmoveClock == 0);
        assert(position.fullmoveNumber == 2);
    }

    // Test that capture with a non-pawn piece also resets the halfmoveClock
    {
        Position position;
        position.board["a2"_sq] = Piece::WHITE_PAWN;
        position.board["b3"_sq] = Piece::BLACK_ROOK;  // White pawn captures black rook
        position.activeColor = Color::WHITE;
        position.fullmoveNumber = 1;
        position.halfmoveClock = 1;
        position = applyMove(position, Move("a2"_sq, "b3"_sq, Move::CAPTURE));
        assert(position.board["b3"_sq] == Piece::WHITE_PAWN);
        assert(position.board["a2"_sq] == Piece::NONE);
        assert(position.activeColor == Color::BLACK);
        assert(position.halfmoveClock == 0);   // reset due to capture
        assert(position.fullmoveNumber == 1);  // not updated on white move
    }

    // Test that a move with a non-pawn piece does not reset the halfmoveClock
    {
        Position position;
        position.board["b1"_sq] = Piece::WHITE_KNIGHT;
        position.activeColor = Color::WHITE;
        position.halfmoveClock = 1;
        position = applyMove(position, Move("b1"_sq, "c3"_sq, Move::QUIET));
        assert(position.board["c3"_sq] == Piece::WHITE_KNIGHT);
        assert(position.board["b1"_sq] == Piece::NONE);
        assert(position.activeColor == Color::BLACK);
        assert(position.halfmoveClock == 2);
    }

    std::cout << "All applyMove tests passed!" << std::endl;
}

void testIsAttacked() {
    Square whiteKingSquare = "a1"_sq;
    Square blackKingSquare = "f6"_sq;
    Board base;
    base[whiteKingSquare] = Piece::WHITE_KING;
    base[blackKingSquare] = Piece::BLACK_KING;

    // Test that a king is in check
    {
        Board board = base;
        board["b1"_sq] = Piece::BLACK_ROOK;
        assert(isAttacked(board, whiteKingSquare));
        assert(!isAttacked(board, blackKingSquare));
    }

    // Test that a king is not in check
    {
        Board board = base;
        board["b1"_sq] = Piece::BLACK_ROOK;
    }

    // Test that a king is in check after a move
    {
        Board board = base;
        board["b2"_sq] = Piece::BLACK_ROOK;
        applyMove(board, Move("a1"_sq, "a2"_sq, Move::QUIET));
        assert(isAttacked(board, "a2"_sq));
    }

    // Test that a king is not in check after a move
    {
        Board board;
        board["a1"_sq] = Piece::WHITE_KING;
        board["b1"_sq] = Piece::BLACK_ROOK;
        applyMove(board, Move("a1"_sq, "a2"_sq, Move::QUIET));
        assert(!isAttacked(board, blackKingSquare));
    }

    std::cout << "All isAttacked tests passed!" << std::endl;
}

int main() {
    testSquare();
    testSquareSet();
    testPiece();
    testPieceType();
    testColor();
    testPromotionKind();
    testPossibleMoves();
    testPossibleCaptures();
    testOccupancy();
    testAddAvailableMoves();
    testApplyMove();
    testIsAttacked();
    std::cout << "All move tests passed!" << std::endl;
    return 0;
}

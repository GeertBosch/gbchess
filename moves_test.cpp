#include <algorithm>
#include <cassert>
#include <iostream>

#include "common.h"
#include "fen.h"
#include "moves.h"
#include "options.h"

std::string toString(SquareSet squares) {
    std::string str;
    for (Square sq : squares) {
        str += static_cast<std::string>(sq);
        str += " ";
    }
    if (!squares.empty()) str.pop_back();  // Remove the last space unless the string is empty
    return str;
}

// Output stream operator for std::vector<std::string>
std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    os << "[";
    for (const auto& move : moves) {
        if (move) os << std::string(move) << ", ";
    }
    os << "]";
    return os;
}

void printBoard(std::ostream& os, const Board& board) {
    for (int rank = 7; rank >= 0; --rank) {
        os << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            auto piece = board[Square(file, rank)];
            os << ' ' << to_char(piece);
        }
        os << std::endl;
    }
    os << "   ";
    for (char file = 'a'; file <= 'h'; ++file) {
        os << ' ' << file;
    }
    os << std::endl;
}

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

void testColor() {
    assert(color(Piece::P) == Color::WHITE);
    assert(color(Piece::N) == Color::WHITE);
    assert(color(Piece::B) == Color::WHITE);
    assert(color(Piece::R) == Color::WHITE);
    assert(color(Piece::Q) == Color::WHITE);
    assert(color(Piece::K) == Color::WHITE);
    assert(color(Piece::p) == Color::BLACK);
    assert(color(Piece::n) == Color::BLACK);
    assert(color(Piece::b) == Color::BLACK);
    assert(color(Piece::r) == Color::BLACK);
    assert(color(Piece::q) == Color::BLACK);
    assert(color(Piece::k) == Color::BLACK);

    assert(addColor(PieceType::PAWN, Color::WHITE) == Piece::P);
    assert(addColor(PieceType::KNIGHT, Color::WHITE) == Piece::N);
    assert(addColor(PieceType::BISHOP, Color::WHITE) == Piece::B);
    assert(addColor(PieceType::ROOK, Color::WHITE) == Piece::R);
    assert(addColor(PieceType::QUEEN, Color::WHITE) == Piece::Q);
    assert(addColor(PieceType::KING, Color::WHITE) == Piece::K);
    assert(addColor(PieceType::PAWN, Color::BLACK) == Piece::p);
    assert(addColor(PieceType::KNIGHT, Color::BLACK) == Piece::n);
    assert(addColor(PieceType::BISHOP, Color::BLACK) == Piece::b);
    assert(addColor(PieceType::ROOK, Color::BLACK) == Piece::r);
    assert(addColor(PieceType::QUEEN, Color::BLACK) == Piece::q);
    assert(addColor(PieceType::KING, Color::BLACK) == Piece::k);

    std::cout << "All Color tests passed!" << std::endl;
}

void testPossibleMoves() {
    // Test rook moves
    {
        auto moves = possibleMoves(Piece::R, "a1"_sq);
        assert(moves.size() == 14);
        assert(moves.contains("h1"_sq));
        assert(moves.contains("a8"_sq));
    }

    // Test bishop moves
    {
        auto moves = possibleMoves(Piece::b, "a1"_sq);
        assert(moves.size() == 7);
        assert(moves.contains("h8"_sq));
    }

    // Test knight moves
    {
        auto moves = possibleMoves(Piece::N, "a1"_sq);
        assert(moves.size() == 2);
        assert(moves.contains("c2"_sq));
        assert(moves.contains("b3"_sq));
    }

    // Test king moves
    {
        auto moves = possibleMoves(Piece::k, "a1"_sq);
        assert(moves.size() == 3);
    }

    // Test queen moves
    {
        auto moves = possibleMoves(Piece::Q, "a1"_sq);
        assert(moves.size() == 21);
    }

    // Test white pawn moves
    {
        auto moves = possibleMoves(Piece::P, "a2"_sq);
        assert(moves.size() == 2);
        assert(moves.contains("a3"_sq));
        assert(moves.contains("a4"_sq));
    }

    // Test black pawn moves
    {
        auto moves = possibleMoves(Piece::p, "a7"_sq);
        assert(moves.size() == 2);
        assert(moves.contains("a6"_sq));
        assert(moves.contains("a5"_sq));
    }

    std::cout << "All possibleMoves tests passed!" << std::endl;
}

void testPossibleCaptures() {
    // Knight tests
    auto moves = possibleCaptures(Piece::N, {4, 4});
    assert(moves.contains({3, 2}));
    assert(moves.contains({5, 2}));
    assert(moves.contains({3, 6}));
    assert(moves.contains({5, 6}));
    assert(moves.contains({2, 3}));
    assert(moves.contains({6, 3}));
    assert(moves.contains({2, 5}));
    assert(moves.contains({6, 5}));

    // Edge cases for knight
    moves = possibleCaptures(Piece::n, {7, 7});
    assert(moves.contains({6, 5}));
    assert(moves.contains({5, 6}));

    // Bishop tests
    moves = possibleCaptures(Piece::B, {4, 4});
    assert(moves.contains({3, 3}));
    assert(moves.contains({2, 2}));
    // ... Add more assertions for all possible squares

    // Rook tests
    moves = possibleCaptures(Piece::r, {4, 4});
    assert(moves.contains({0, 4}));
    assert(moves.contains({1, 4}));
    assert(moves.contains({2, 4}));
    assert(moves.contains({3, 4}));
    // ... Add more assertions for all possible squares

    // Queen tests (combination of Rook and Bishop)
    moves = possibleCaptures(Piece::Q, {4, 4});
    assert(moves.contains({3, 3}));
    assert(moves.contains({0, 4}));
    // ... Add more assertions for all possible squares

    // King tests
    moves = possibleCaptures(Piece::k, {4, 4});
    assert(moves.contains({4, 3}));
    assert(moves.contains({3, 4}));
    // ... Add more assertions for all possible squares

    // Pawn tests (White Pawn)
    moves = possibleCaptures(Piece::P, {4, 4});
    assert(moves.contains({3, 5}));
    assert(moves.contains({5, 5}));

    // Pawn tests (Black Pawn)
    moves = possibleCaptures(Piece::p, {4, 4});
    assert(moves.contains({3, 3}));
    assert(moves.contains({5, 3}));

    std::cout << "All possibleCaptures tests passed!" << std::endl;
}
void testMove() {
    // Test constructor
    Move move("a2"_sq, "a4"_sq, Move::QUIET);
    assert(move.from() == "a2"_sq);
    assert(move.to() == "a4"_sq);
    assert(move.kind() == Move::QUIET);

    // Test operator==
    Move capture = Move("a2"_sq, "a4"_sq, Move::CAPTURE);
    assert(move == Move("a2"_sq, "a4"_sq, Move::QUIET));
    assert(!(move == capture));

    // Test conversion to std::string
    Move promotion = Move("a7"_sq, "a8"_sq, MoveKind::QUEEN_PROMOTION);
    assert(static_cast<std::string>(move) == "a2a4");
    assert(static_cast<std::string>(Move()) == "0000");

    std::cout << "All Move tests passed!" << std::endl;
}
void testSquare() {
    // Test constructor with rank and file
    Square square1(3, 2);
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
        board["a3"_sq] = Piece::P;
        board["b4"_sq] = Piece::P;
        board["a1"_sq] = Piece::R;
        board["d2"_sq] = Piece::N;
        board["c2"_sq] = Piece::B;
        board["d1"_sq] = Piece::Q;
        board["e1"_sq] = Piece::K;
        board["f2"_sq] = Piece::B;
        board["g2"_sq] = Piece::N;
        board["h5"_sq] = Piece::r;
        board["d7"_sq] = Piece::p;
        board["e5"_sq] = Piece::p;
        board["f7"_sq] = Piece::p;
        board["g7"_sq] = Piece::p;
        board["h7"_sq] = Piece::p;
        board["a8"_sq] = Piece::r;
        board["b8"_sq] = Piece::n;
        board["c8"_sq] = Piece::b;
        board["d8"_sq] = Piece::q;
        board["e8"_sq] = Piece::k;
        auto squares = SquareSet::occupancy(board);
        assert(squares.size() == 20);
        squares = SquareSet::occupancy(board, Color::WHITE);
        assert(squares.size() == 9);
        squares = SquareSet::occupancy(board, Color::BLACK);
        assert(squares.size() == 11);
    }
    std::cout << "All occupancy tests passed!" << std::endl;
}

void testPromotionKind() {
    {
        Square from = "a7"_sq;
        Square to = "a8"_sq;
        Move move(from, to, MoveKind::QUEEN_PROMOTION);
        assert(move.isPromotion());
        assert(promotionType((move.kind())) == PieceType::QUEEN);
    }
}

void testAddAvailableMoves() {
    auto find = [](MoveVector& moves, Move move) -> bool {
        return std::find(moves.begin(), moves.end(), move) != moves.end();
    };
    // Test pawn moves
    {
        Board board;
        board["a2"_sq] = Piece::P;
        board["a4"_sq] = Piece::P;  // Block the pawn, so there's no two-square move
        MoveVector moves;
        addAvailableMoves(moves, board, Color::WHITE);
        assert(moves.size() == 2);
        assert(find(moves, Move("a2"_sq, "a3"_sq, Move::QUIET)));
        assert(find(moves, Move("a4"_sq, "a5"_sq, Move::QUIET)));
    }

    std::cout << "All addAvailableMoves tests passed!" << std::endl;
}

void testAddAvailableCaptures() {
    auto find = [](MoveVector& moves, Move move) -> bool {
        return std::find(moves.begin(), moves.end(), move) != moves.end();
    };

    // Test case with a regular captures and one promoting a pawn while capturing
    {
        Board board = fen::parsePiecePlacement("r3kbnr/pP1qpppp/3p4/4N3/4P3/8/PPP2PPP/RNB1K2R");
        MoveVector captures;
        addAvailableCaptures(captures, board, Color::WHITE);
        assert(captures.size() == 6);
        assert(find(captures, Move("e5"_sq, "d7"_sq, Move::CAPTURE)));
        assert(find(captures, Move("e5"_sq, "f7"_sq, Move::CAPTURE)));
        assert(find(captures, Move("b7"_sq, "a8"_sq, MoveKind::QUEEN_PROMOTION_CAPTURE)));
        assert(find(captures, Move("b7"_sq, "a8"_sq, MoveKind::ROOK_PROMOTION_CAPTURE)));
        assert(find(captures, Move("b7"_sq, "a8"_sq, MoveKind::BISHOP_PROMOTION_CAPTURE)));
        assert(find(captures, Move("b7"_sq, "a8"_sq, MoveKind::KNIGHT_PROMOTION_CAPTURE)));
    }
    std::cout << "All addAvailableCaptures tests passed!" << std::endl;
}

void testAddAvailableEnPassant() {
    {
        Board board = fen::parsePiecePlacement("rnbqkbnr/1ppppppp/8/8/pP6/P1P5/3PPPPP/RNBQKBNR");
        MoveVector moves;
        addAvailableEnPassant(moves, board, Color::BLACK, "b3"_sq);
        assert(moves.size() == 1);
        assert(moves[0] == Move("a4"_sq, "b3"_sq, MoveKind::EN_PASSANT));
    }
    std::cout << "All addAvailableEnPassant tests passed!" << std::endl;
}

MoveVector allLegalCastling(std::string piecePlacement,
                            Color activeColor,
                            CastlingMask castlingAvailability) {
    Board board = fen ::parsePiecePlacement(piecePlacement);
    Turn turn;
    MoveVector moves;
    turn.activeColor = activeColor;
    turn.castlingAvailability = castlingAvailability;
    forAllLegalMovesAndCaptures(turn, board, [&moves](Board&, MoveWithPieces mwp) {
        if (isCastles(mwp.move.kind())) moves.emplace_back(mwp.move);
    });
    return moves;
}

void testAllLegalCastling() {
    {
        auto moves = allLegalCastling("r3k2r/8/8/8/8/8/8/R3K2R", Color::WHITE, CastlingMask::KQkq);
        assert(moves.size() == 2);
        assert(moves[0] == Move("e1"_sq, "g1"_sq, MoveKind::KING_CASTLE));
        assert(moves[1] == Move("e1"_sq, "c1"_sq, MoveKind::QUEEN_CASTLE));
    }
    {
        auto moves = allLegalCastling(
            "rn1qkbnr/2pppppp/bp6/p7/4P3/5N2/PPPP1PPP/RNBQK2R", Color::WHITE, CastlingMask::KQkq);
        assert(moves.size() == 0);  // There is one castling move (e1g1), but it's illegal
    }
    {
        auto moves = allLegalCastling("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/1R2K2R",
                                      Color::BLACK,
                                      CastlingMask::KQq);
        assert(moves.size() == 1);
        assert(moves[0] == Move("e8"_sq, "c8"_sq, MoveKind::QUEEN_CASTLE));
    }
    std::cout << "All legal castling tests passed!" << std::endl;
}

void testMakeAndUnmakeMove(Board& board, Move move) {
    auto originalBoard = board;
    auto activeColor = color(board[move.from()]);
    auto occupancy = Occupancy(board, activeColor);
    auto delta = occupancyDelta(move);
    auto undo = makeMove(board, move);
    auto expected = Occupancy(board, !activeColor);
    auto actual = !(occupancy ^ delta);
    if (actual != expected) {
        std::cout << "Move: " << static_cast<std::string>(move) << ", kind " << (int)move.kind()
                  << "\n";
        std::cout << "Occupancy: {" << toString(occupancy.ours()) << ", "
                  << toString(occupancy.theirs()) << "}\n";
        std::cout << "Delta: { " << toString(delta.ours()) << ", " << toString(delta.theirs())
                  << "}\n";
        std::cout << "Expected occupancy: { " << toString(expected.ours()) << ", "
                  << toString(expected.theirs()) << "}\n";
        std::cout << "Actual occupancy: { " << toString(actual.ours()) << ", "
                  << toString(actual.theirs()) << "}\n";
        std::cout << "Original board:\n";
        printBoard(std::cout, originalBoard);
        std::cout << "Board after makeMove:\n";
        printBoard(std::cout, board);
    }
    assert(actual == expected);
    unmakeMove(board, undo);
    if (board != originalBoard) {
        std::cout << "Original board:" << std::endl;
        std::cout << fen::to_string(originalBoard) << std::endl;
        printBoard(std::cout, originalBoard);
        std::cout << "Move: " << static_cast<std::string>(move) << std::endl;
        std::cout << "Kind: " << static_cast<int>(move.kind()) << std::endl;
        std::cout << "Captured: " << to_char(undo.captured) << std::endl;
        {
            auto newBoard = originalBoard;
            makeMove(newBoard, move);
            std::cout << "Board after makeMove:" << std::endl;
            std::cout << fen::to_string(newBoard) << std::endl;
            printBoard(std::cout, newBoard);
        }
        std::cout << "Board after makeMove and unmakeMove:" << std::endl;
        std::cout << fen::to_string(board) << std::endl;
        printBoard(std::cout, board);
    }
    assert(board == originalBoard);
    makeMove(board, move);
}

void testMakeAndUnmakeMove() {
    // Test pawn move
    {
        Board board;
        board["a2"_sq] = Piece::P;
        testMakeAndUnmakeMove(board, Move("a2"_sq, "a3"_sq, Move::QUIET));
        assert(board["a3"_sq] == Piece::P);
        assert(board["a2"_sq] == Piece::_);
    }

    // Test pawn capture
    {
        Board board;
        board["a2"_sq] = Piece::P;
        board["b3"_sq] = Piece::r;  // White pawn captures black rook
        testMakeAndUnmakeMove(board, Move("a2"_sq, "b3"_sq, Move::CAPTURE));
        assert(board["b3"_sq] == Piece::P);
        assert(board["a2"_sq] == Piece::_);
    }

    // Test pawn promotion move
    {
        // White pawn promotion
        Board board;
        board["a7"_sq] = Piece::P;
        auto move = Move("a7"_sq, "a8"_sq, MoveKind::QUEEN_PROMOTION);
        testMakeAndUnmakeMove(board, move);
        assert(board["a8"_sq] == Piece::Q);
        assert(board["a7"_sq] == Piece::_);

        // Black pawn promotion
        board["a2"_sq] = Piece::p;
        testMakeAndUnmakeMove(board, Move("a2"_sq, "a1"_sq, MoveKind::ROOK_PROMOTION));
        assert(board["a1"_sq] == Piece::r);
        assert(board["a2"_sq] == Piece::_);
    }

    // Test pawn promotion capture
    {
        // White pawn promotion
        Board board;
        board["a7"_sq] = Piece::P;
        board["b8"_sq] = Piece::r;  // White pawn captures black rook
        testMakeAndUnmakeMove(board, Move("a7"_sq, "b8"_sq, MoveKind::BISHOP_PROMOTION_CAPTURE));
        assert(board["b8"_sq] == Piece::B);
        assert(board["a7"_sq] == Piece::_);

        // Black pawn promotion
        board["a2"_sq] = Piece::p;
        board["b1"_sq] = Piece::R;  // Black pawn captures white rook
        testMakeAndUnmakeMove(board, Move("a2"_sq, "b1"_sq, MoveKind::KNIGHT_PROMOTION_CAPTURE));
        assert(board["b1"_sq] == Piece::n);
        assert(board["a2"_sq] == Piece::_);
    }

    // Test rook move
    {
        Board board;
        board["a8"_sq] = Piece::R;
        testMakeAndUnmakeMove(board, Move("a8"_sq, "h8"_sq, Move::QUIET));
        assert(board["h8"_sq] == Piece::R);
        assert(board["a8"_sq] == Piece::_);
    }

    // Test rook capture
    {
        Board board;
        board["a8"_sq] = Piece::r;
        board["a1"_sq] = Piece::R;  // Black rook captures white rook
        testMakeAndUnmakeMove(board, Move("a8"_sq, "a1"_sq, Move::CAPTURE));
        assert(board["a1"_sq] == Piece::r);
        assert(board["a8"_sq] == Piece::_);
    }

    // Test knight move
    {
        Board board;
        board["b1"_sq] = Piece::N;
        testMakeAndUnmakeMove(board, Move("b1"_sq, "c3"_sq, Move::QUIET));
        assert(board["c3"_sq] == Piece::N);
        assert(board["b1"_sq] == Piece::_);
    }

    // Test knight capture
    {
        Board board;
        board["b1"_sq] = Piece::N;
        board["c3"_sq] = Piece::r;  // White knight captures black rook
        testMakeAndUnmakeMove(board, Move("b1"_sq, "c3"_sq, Move::CAPTURE));
        assert(board["c3"_sq] == Piece::N);
        assert(board["b1"_sq] == Piece::_);
    }

    // Test en passant capture
    {
        Board board;
        board["a5"_sq] = Piece::P;
        board["b5"_sq] = Piece::p;  // Black pawn moved two squares
        board["a6"_sq] = Piece::p;
        Move move("a5"_sq, "b6"_sq, MoveKind::EN_PASSANT);
        testMakeAndUnmakeMove(board, move);
        assert(board["b6"_sq] == Piece::P);
        assert(board["b5"_sq] == Piece::_);
        assert(board["a5"_sq] == Piece::_);
    }

    // Test king castling move
    {
        Board board;
        board["e1"_sq] = Piece::K;
        board["h1"_sq] = Piece::R;
        testMakeAndUnmakeMove(board, Move("e1"_sq, "g1"_sq, MoveKind::KING_CASTLE));
        assert(board["g1"_sq] == Piece::K);
        assert(board["f1"_sq] == Piece::R);
        assert(board["e1"_sq] == Piece::_);
        assert(board["h1"_sq] == Piece::_);
    }
    // Test queen castling move
    {
        Board board;
        board["e1"_sq] = Piece::K;
        board["a1"_sq] = Piece::R;
        testMakeAndUnmakeMove(board, Move("e1"_sq, "c1"_sq, MoveKind::QUEEN_CASTLE));
        assert(board["c1"_sq] == Piece::K);
        assert(board["d1"_sq] == Piece::R);
        assert(board["e1"_sq] == Piece::_);
        assert(board["a1"_sq] == Piece::_);
    }
    std::cout << "All makeMove and unmakeMove tests passed!" << std::endl;
}

void testApplyMove() {
    // Now test the Position applyMove function for a pawn move
    {
        Position position;
        position.board["a2"_sq] = Piece::P;
        position.turn.activeColor = Color::WHITE;
        position.turn.halfmoveClock = 1;
        position = applyMove(position, Move("a2"_sq, "a3"_sq, Move::QUIET));
        assert(position.board["a3"_sq] == Piece::P);
        assert(position.board["a2"_sq] == Piece::_);
        assert(position.turn.activeColor == Color::BLACK);
        assert(position.turn.halfmoveClock == 0);
    }

    // Test pawn capture
    {
        Position position;
        position.board["a2"_sq] = Piece::P;
        position.board["b3"_sq] = Piece::r;  // White pawn captures black rook
        position.turn.activeColor = Color::WHITE;
        position.turn.halfmoveClock = 1;
        position = applyMove(position, Move("a2"_sq, "b3"_sq, Move::CAPTURE));
        assert(position.board["b3"_sq] == Piece::P);
        assert(position.board["a2"_sq] == Piece::_);
        assert(position.turn.activeColor == Color::BLACK);
        assert(position.turn.halfmoveClock == 0);
    }

    // Test that the fullmove number is updated correctly on a black move
    {
        Position position;
        position.board["a2"_sq] = Piece::p;
        position.turn.activeColor = Color::BLACK;
        position.turn.halfmoveClock = 1;
        position.turn.fullmoveNumber = 1;
        position = applyMove(position, Move("a2"_sq, "a3"_sq, Move::QUIET));
        assert(position.board["a3"_sq] == Piece::p);
        assert(position.board["a2"_sq] == Piece::_);
        assert(position.turn.activeColor == Color::WHITE);
        assert(position.turn.halfmoveClock == 0);
        assert(position.turn.fullmoveNumber == 2);
    }

    // Test that capture with a non-pawn piece also resets the halfmoveClock
    {
        Position position;
        position.board["a2"_sq] = Piece::P;
        position.board["b3"_sq] = Piece::r;  // White pawn captures black rook
        position.turn.activeColor = Color::WHITE;
        position.turn.fullmoveNumber = 1;
        position.turn.halfmoveClock = 1;
        position = applyMove(position, Move("a2"_sq, "b3"_sq, Move::CAPTURE));
        assert(position.board["b3"_sq] == Piece::P);
        assert(position.board["a2"_sq] == Piece::_);
        assert(position.turn.activeColor == Color::BLACK);
        assert(position.turn.halfmoveClock == 0);   // reset due to capture
        assert(position.turn.fullmoveNumber == 1);  // not updated on white move
    }

    // Test that a move with a non-pawn piece does not reset the halfmoveClock
    {
        Position position;
        position.board["b1"_sq] = Piece::N;
        position.turn.activeColor = Color::WHITE;
        position.turn.halfmoveClock = 1;
        position = applyMove(position, Move("b1"_sq, "c3"_sq, Move::QUIET));
        assert(position.board["c3"_sq] == Piece::N);
        assert(position.board["b1"_sq] == Piece::_);
        assert(position.turn.activeColor == Color::BLACK);
        assert(position.turn.halfmoveClock == 2);
    }

    // Test en passant capture
    {
        Position position;
        position.board["a5"_sq] = Piece::P;
        position.board["b5"_sq] = Piece::p;
        position.turn.activeColor = Color::WHITE;
        position.turn.enPassantTarget = "b6"_sq;
        position.turn.halfmoveClock = 1;
        position = applyMove(position, Move("a5"_sq, "b6"_sq, MoveKind::EN_PASSANT));
        assert(position.board["b6"_sq] == Piece::P);
        assert(position.board["b5"_sq] == Piece::_);
        assert(position.turn.activeColor == Color::BLACK);
        assert(position.turn.halfmoveClock == 0);
    }

    // Test that capture of rook a by a rook removes castling rights on both sides
    {
        Position position;
        position.board["a1"_sq] = Piece::R;
        position.board["e1"_sq] = Piece::K;
        position.board["a8"_sq] = Piece::r;
        position.board["e8"_sq] = Piece::k;
        position.turn.activeColor = Color::WHITE;
        position.turn.castlingAvailability = CastlingMask::q | CastlingMask::Q;
        position = applyMove(position, Move("a1"_sq, "a8"_sq, Move::CAPTURE));
        assert(position.turn.castlingAvailability == CastlingMask::_);
    }

    std::cout << "All applyMove tests passed!" << std::endl;
}

void testIsAttacked() {
    Square whiteKingSquare = "a1"_sq;
    Square blackKingSquare = "f6"_sq;
    Board base;
    base[whiteKingSquare] = Piece::K;
    base[blackKingSquare] = Piece::k;

    // Test that a king is in check
    {
        Board board = base;
        board["b1"_sq] = Piece::r;
        auto opponentSquares = SquareSet::occupancy(board, Color::BLACK);
        auto occupancy = Occupancy(board, Color::WHITE);
        assert(isAttacked(board, whiteKingSquare, occupancy));
        assert(!isAttacked(board, blackKingSquare, occupancy));
    }

    // Test that a king is not in check
    {
        Board board = base;
        board["b1"_sq] = Piece::r;
    }

    // Test that a king is in check after a move
    {
        Board board = base;
        board["b2"_sq] = Piece::r;
        testMakeAndUnmakeMove(board, Move("a1"_sq, "a2"_sq, Move::QUIET));
        auto occupancy = Occupancy(board, Color::WHITE);
        assert(isAttacked(board, "a2"_sq, occupancy));
    }

    // Test that a king is not in check after a move
    {
        Board board;
        board["a1"_sq] = Piece::K;
        board["b1"_sq] = Piece::r;
        testMakeAndUnmakeMove(board, Move("a1"_sq, "a2"_sq, Move::QUIET));
        auto occupancy = Occupancy(board, Color::BLACK);

        assert(!isAttacked(board, blackKingSquare, occupancy));
    }

    // Test that this method also works for an empty square
    {
        Board board;
        board["e1"_sq] = Piece::K;
        board["f2"_sq] = Piece::r;
        auto occupancy = Occupancy(board, Color::WHITE);

        assert(isAttacked(board, "f1"_sq, occupancy));
    }

    std::cout << "All isAttacked tests passed!" << std::endl;
}

void testAllLegalMoves() {
    {
        auto position =
            fen::parsePosition("rnbqkbnr/pppppp1p/8/6p1/7P/8/PPPPPPP1/RNBQKBNR w KQkq - 0 2");
        auto legalMoves = allLegalMovesAndCaptures(position.turn, position.board);
        assert(legalMoves.size() == 22);
    }
    {
        // Can't castle the king through check
        auto position =
            fen::parsePosition("rn1qkbnr/2pppppp/bp6/p7/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4");
        auto legalMoves = allLegalMovesAndCaptures(position.turn, position.board);
        assert(legalMoves.size() == 23);
    }
    {
        // Can't castle a king that is in check
        auto position =
            fen::parsePosition("r1bqkbnr/pppppppp/8/1B6/4P3/8/PPnPNPPP/RNBQK2R w KQkq - 0 4");
        auto legalMoves = allLegalMovesAndCaptures(position.turn, position.board);
        assert(std::count_if(legalMoves.begin(), legalMoves.end(), [](auto item) {
                   return item == Move{"e1"_sq, "g1"_sq, MoveKind::KING_CASTLE};
               }) == 0);
        assert(legalMoves.size() == 2);
    }
    {
        // This king can castle queen side
        auto position = fen::parsePosition(
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/1R2K2R b Kkq - 1 1");
        auto legalMoves = allLegalMovesAndCaptures(position.turn, position.board);
        assert(std::count_if(legalMoves.begin(), legalMoves.end(), [](auto item) {
                   return item == Move{"e8"_sq, "c8"_sq, MoveKind::QUEEN_CASTLE};
               }) == 1);
    }


    std::cout << "All allLegalMovesAndCaptures tests passed!" << std::endl;
}

/**
 * Test that moves remove the correct castling rights
 */
void testCastlingMask() {
    {
        auto mask = castlingMask("a1"_sq, "a8"_sq);
        assert(mask == (CastlingMask::Q | CastlingMask::q));
    }
}

void testAllLegalQuiescentMoves() {
    {
        std::string fen = "8/1N3k2/6p1/8/2P3P1/p7/1R2K3/8 b - - 0 58";
        auto position = fen::parsePosition(fen);
        auto moves =
            allLegalQuiescentMoves(position.turn, position.board, options::promotionMinDepthLeft);
        assert(moves.size() == 2);
    }
    {
        // This position is not quiet for white, as black may promote a pawn
        std::string fen = "8/1N3k2/6p1/8/2P3P1/8/1p2K3/8 w - - 0 59";
        auto position = fen::parsePosition(fen);
        auto moves = allLegalQuiescentMoves(
            position.turn, position.board, options::promotionMinDepthLeft + 1);
        assert(moves.size() == 14);
    }
    std::cout << "All allLegalQuiescentMoves tests passed!\n";
}

<<<<<<< HEAD
int main() {
    testSquare();
    testMove();
    testSquareSet();
    testPiece();
    testPieceType();
    testColor();
    testPromotionKind();
    testPossibleMoves();
    testPossibleCaptures();
    testOccupancy();
    testAddAvailableMoves();
    testAddAvailableCaptures();
    testAddAvailableEnPassant();
    testAllLegalCastling();
    testMakeAndUnmakeMove();
    testCastlingMask();
    testApplyMove();
    testIsAttacked();
    testAllLegalMoves();
    testAllLegalQuiescentMoves();
    std::cout << "All move tests passed!" << std::endl;
    return 0;
}

#include <algorithm>
#include <cassert>
#include <iostream>

#include "common.h"
#include "fen.h"
#include "moves.h"
#include "options.h"

using namespace for_test;

std::string toString(SquareSet squares) {
    std::string str;
    for (Square sq : squares) {
        str += to_string(sq);
        str += " ";
    }
    if (!squares.empty()) str.pop_back();  // Remove the last space unless the string is empty
    return str;
}

// Output stream operator for std::vector<std::string>
std::ostream& operator<<(std::ostream& os, const MoveVector& moves) {
    os << "[";
    for (const auto& move : moves) {
        if (move) os << to_string(move) << ", ";
    }
    os << "]";
    return os;
}
bool lesssq(Square left, Square right) {
    return file(left) < file(right) || (file(left) == file(right) && rank(left) < rank(right));
}
bool less(Move left, Move right) {
    return lesssq(left.from, right.from) || (left.from == right.from && lesssq(left.to, right.to));
}

MoveVector sort(MoveVector moves) {
    std::sort(moves.begin(), moves.end(), less);
    return moves;
}

void printBoard(std::ostream& os, const Board& board) {
    for (int rank = 7; rank >= 0; --rank) {
        os << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            auto piece = board[makeSquare(file, rank)];
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

// Print the SquareSet in a grid format to the output stream with . for empty and X for set squares
void printSquareSet(std::ostream& os, const SquareSet& squares) {
    for (int rank = 7; rank >= 0; --rank) {
        os << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            os << (squares.contains(makeSquare(file, rank)) ? " X" : " .");
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
        board[a3] = Piece::P;
        board[b4] = Piece::P;
        board[a1] = Piece::R;
        board[d2] = Piece::N;
        board[c2] = Piece::B;
        board[d1] = Piece::Q;
        board[e1] = Piece::K;
        board[f2] = Piece::B;
        board[g2] = Piece::N;
        board[h5] = Piece::r;
        board[d7] = Piece::p;
        board[e5] = Piece::p;
        board[f7] = Piece::p;
        board[g7] = Piece::p;
        board[h7] = Piece::p;
        board[a8] = Piece::r;
        board[b8] = Piece::n;
        board[c8] = Piece::b;
        board[d8] = Piece::q;
        board[e8] = Piece::k;
        auto squares = SquareSet::occupancy(board);
        assert(squares.size() == 20);
        squares = SquareSet::occupancy(board, Color::w);
        assert(squares.size() == 9);
        squares = SquareSet::occupancy(board, Color::b);
        assert(squares.size() == 11);
    }
    std::cout << "All occupancy tests passed!" << std::endl;
}

bool has(const MoveVector& moves, Move move) {
    return std::find(moves.begin(), moves.end(), move) != moves.end();
};

void testLegalPawnMoves() {
    // Test pawn moves
    Board board;
    board[a2] = Piece::P;
    board[a4] = Piece::P;  // Block the pawn, so there's no two-square move
    Turn turn(Color::w, CastlingMask::_, noEnPassantTarget);
    MoveVector moves = allLegalMovesAndCaptures(turn, board);
    assert(moves.size() == 2);
    assert(has(moves, Move(a2, a3, MoveKind::Quiet_Move)));
    assert(has(moves, Move(a4, a5, MoveKind::Quiet_Move)));
}

MoveVector allLegalCaptures(Turn turn, Board board) {
    MoveVector captures;
    for (auto move : allLegalMovesAndCaptures(turn, board))
        if (isCapture(move.kind)) captures.emplace_back(move);
    return captures;
}

void testLegalCaptures() {
    // Test case with a regular captures and one promoting a pawn while capturing
    Board board = fen::parsePiecePlacement("r3kbnr/pP1qpppp/3p4/4N3/4P3/8/PPP2PPP/RNB1K2R");
    Turn turn(Color::w);
    MoveVector captures = allLegalCaptures(turn, board);
    // We expect 6 captures: 2 from the knight, and 4 from the pawn promotion
    assert(captures.size() == 6);
    assert(has(captures, Move(e5, d7, MoveKind::Capture)));
    assert(has(captures, Move(e5, f7, MoveKind::Capture)));
    assert(has(captures, Move(b7, a8, MoveKind::Queen_Promotion_Capture)));
    assert(has(captures, Move(b7, a8, MoveKind::Rook_Promotion_Capture)));
    assert(has(captures, Move(b7, a8, MoveKind::Bishop_Promotion_Capture)));
    assert(has(captures, Move(b7, a8, MoveKind::Knight_Promotion_Capture)));
}

void testLegalEnPassant() {
    Board board = fen::parsePiecePlacement("rnbqkbnr/1ppppppp/8/8/pP6/P1P5/3PPPPP/RNBQKBNR");
    Turn turn(Color::b, CastlingMask::_, b3);
    MoveVector moves;

    for (auto move : allLegalMovesAndCaptures(turn, board))
        if (move.kind == MoveKind::En_Passant) moves.push_back(move);
    assert(moves.size() == 1);
    assert(moves[0] == Move(a4, b3, MoveKind::En_Passant));
}

MoveVector allLegalCastling(std::string fen) {
    auto [board, turn] = fen::parsePosition(fen);
    MoveVector moves;
    SearchState state(board, turn);
    forAllLegalMovesAndCaptures(board, state, [&moves](Board&, MoveWithPieces mwp) {
        if (isCastles(mwp.move.kind)) moves.emplace_back(mwp.move);
    });
    return moves;
}

void testAllLegalCastling() {
    {
        auto moves = allLegalCastling("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
        assert(moves.size() == 2);
        assert(moves[0] == Move(e1, g1, MoveKind::O_O));
        assert(moves[1] == Move(e1, c1, MoveKind::O_O_O));
    }
    {
        auto moves =
            allLegalCastling("rn1qkbnr/2pppppp/bp6/p7/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
        assert(moves.size() == 0);  // There is one castling move (e1g1), but it's illegal
    }
    {
        auto moves = allLegalCastling(
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/1R2K2R b KQq - 0 1");
        assert(moves.size() == 1);
        assert(moves[0] == Move(e8, c8, MoveKind::O_O_O));
    }
    std::cout << "All legal castling tests passed!" << std::endl;
}

void testPinnedPieces() {
    {
        Board board = fen::parsePiecePlacement("rnb1kbnr/pp1ppppp/2p5/q7/8/PP6/2PPPPPP/RNBQKBNR");
        Turn turn = Color::w;
        SearchState state(board, turn);
        assert(state.pinned == d2);
    }
    std::cout << "All pinned pieces tests passed!" << std::endl;
}

void testDoesNotCheck() {
    {
        // We may indicate en passent capture is available, even if it results in check.
        // So test that we reject the move in this case.
        Board board = fen::parsePiecePlacement("8/8/3p4/KPp4r/5R2/4P1k1/6P1/8");
        Turn turn = {Color::w, CastlingMask::_, c6};
        SearchState state(board, turn);
        assert(!doesNotCheck(board, state, Move(b5, c6, MoveKind::En_Passant)));
    }
    std::cout << "All doesNotCheck tests passed!" << std::endl;
}

void testMakeAndUnmakeMove(Board& board, Move move) {
    auto originalBoard = board;
    auto activeColor = color(board[move.from]);
    auto occupancy = Occupancy(board, activeColor);
    auto delta = occupancyDelta(move);
    auto undo = makeMove(board, move);
    auto expected = Occupancy(board, !activeColor);
    auto actual = !(occupancy ^ delta);
    if (actual != expected) {
        std::cout << "Move: " << to_string(move) << ", kind " << (int)move.kind << "\n";
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
        std::cout << "Move: " << to_string(move) << std::endl;
        std::cout << "Kind: " << static_cast<int>(move.kind) << std::endl;
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
        board[a2] = Piece::P;
        testMakeAndUnmakeMove(board, Move(a2, a3, MoveKind::Quiet_Move));
        assert(board[a3] == Piece::P);
        assert(board[a2] == Piece::_);
    }

    // Test pawn capture
    {
        Board board;
        board[a2] = Piece::P;
        board[b3] = Piece::r;  // White pawn captures black rook
        testMakeAndUnmakeMove(board, Move(a2, b3, MoveKind::Capture));
        assert(board[b3] == Piece::P);
        assert(board[a2] == Piece::_);
    }

    // Test pawn promotion move
    {
        // White pawn promotion
        Board board;
        board[a7] = Piece::P;
        auto move = Move(a7, a8, MoveKind::Queen_Promotion);
        testMakeAndUnmakeMove(board, move);
        assert(board[a8] == Piece::Q);
        assert(board[a7] == Piece::_);

        // Black pawn promotion
        board[a2] = Piece::p;
        testMakeAndUnmakeMove(board, Move(a2, a1, MoveKind::Rook_Promotion));
        assert(board[a1] == Piece::r);
        assert(board[a2] == Piece::_);
    }

    // Test pawn promotion capture
    {
        // White pawn promotion
        Board board;
        board[a7] = Piece::P;
        board[b8] = Piece::r;  // White pawn captures black rook
        testMakeAndUnmakeMove(board, Move(a7, b8, MoveKind::Bishop_Promotion_Capture));
        assert(board[b8] == Piece::B);
        assert(board[a7] == Piece::_);

        // Black pawn promotion
        board[a2] = Piece::p;
        board[b1] = Piece::R;  // Black pawn captures white rook
        testMakeAndUnmakeMove(board, Move(a2, b1, MoveKind::Knight_Promotion_Capture));
        assert(board[b1] == Piece::n);
        assert(board[a2] == Piece::_);
    }

    // Test rook move
    {
        Board board;
        board[a8] = Piece::R;
        testMakeAndUnmakeMove(board, Move(a8, h8, MoveKind::Quiet_Move));
        assert(board[h8] == Piece::R);
        assert(board[a8] == Piece::_);
    }

    // Test rook capture
    {
        Board board;
        board[a8] = Piece::r;
        board[a1] = Piece::R;  // Black rook captures white rook
        testMakeAndUnmakeMove(board, Move(a8, a1, MoveKind::Capture));
        assert(board[a1] == Piece::r);
        assert(board[a8] == Piece::_);
    }

    // Test knight move
    {
        Board board;
        board[b1] = Piece::N;
        testMakeAndUnmakeMove(board, Move(b1, c3, MoveKind::Quiet_Move));
        assert(board[c3] == Piece::N);
        assert(board[b1] == Piece::_);
    }

    // Test knight capture
    {
        Board board;
        board[b1] = Piece::N;
        board[c3] = Piece::r;  // White knight captures black rook
        testMakeAndUnmakeMove(board, Move(b1, c3, MoveKind::Capture));
        assert(board[c3] == Piece::N);
        assert(board[b1] == Piece::_);
    }

    // Test en passant capture
    {
        Board board;
        board[a5] = Piece::P;
        board[b5] = Piece::p;  // Black pawn moved two squares
        board[a6] = Piece::p;
        Move move(a5, b6, MoveKind::En_Passant);
        testMakeAndUnmakeMove(board, move);
        assert(board[b6] == Piece::P);
        assert(board[b5] == Piece::_);
        assert(board[a5] == Piece::_);
    }

    // Test king castling move
    {
        Board board;
        board[e1] = Piece::K;
        board[h1] = Piece::R;
        testMakeAndUnmakeMove(board, Move(e1, g1, MoveKind::O_O));
        assert(board[g1] == Piece::K);
        assert(board[f1] == Piece::R);
        assert(board[e1] == Piece::_);
        assert(board[h1] == Piece::_);
    }
    // Test queen castling move
    {
        Board board;
        board[e1] = Piece::K;
        board[a1] = Piece::R;
        testMakeAndUnmakeMove(board, Move(e1, c1, MoveKind::O_O_O));
        assert(board[c1] == Piece::K);
        assert(board[d1] == Piece::R);
        assert(board[e1] == Piece::_);
        assert(board[a1] == Piece::_);
    }
    std::cout << "All makeMove and unmakeMove tests passed!" << std::endl;
}

void testApplyMove() {
    // Now test the Position applyMove function for a pawn move
    {
        Position position;
        position.board[a2] = Piece::P;

        position = applyMove(position, Move(a2, a3, MoveKind::Quiet_Move));
        assert(position.board[a3] == Piece::P);
        assert(position.board[a2] == Piece::_);
        assert(position.turn.activeColor() == Color::b);
        assert(position.turn.halfmove() == 0);
    }

    // Test pawn capture
    {
        Position position;
        position.board[a2] = Piece::P;
        position.board[b3] = Piece::r;  // White pawn captures black rook
        position.turn = {Color::w, CastlingMask::_, noEnPassantTarget, 1, 1};
        position = applyMove(position, Move(a2, b3, MoveKind::Capture));
        assert(position.board[b3] == Piece::P);
        assert(position.board[a2] == Piece::_);
        assert(position.turn.activeColor() == Color::b);
        assert(position.turn.halfmove() == 0);
    }

    // Test that the fullmove number is updated correctly on a black move
    {
        Position position;
        position.board[a2] = Piece::p;
        position.turn = Turn(Color::b, CastlingMask::_, noEnPassantTarget, 1, 1);
        position = applyMove(position, Move(a2, a3, MoveKind::Quiet_Move));
        assert(position.board[a3] == Piece::p);
        assert(position.board[a2] == Piece::_);
        assert(position.turn.activeColor() == Color::w);
        assert(position.turn.halfmove() == 0);
        assert(position.turn.fullmove() == 2);
    }

    // Test that capture with a non-pawn piece also resets the halfmoveClock
    {
        Position position;
        position.board[a2] = Piece::P;
        position.board[b3] = Piece::r;  // White pawn captures black rook
        position.turn = Turn(Color::w, CastlingMask::_, noEnPassantTarget, 1, 1);
        position = applyMove(position, Move(a2, b3, MoveKind::Capture));
        assert(position.board[b3] == Piece::P);
        assert(position.board[a2] == Piece::_);
        assert(position.turn.activeColor() == Color::b);
        assert(position.turn.halfmove() == 0);  // reset due to capture
        assert(position.turn.fullmove() == 1);  // not updated on white move
    }

    // Test that a move with a non-pawn piece does not reset the halfmoveClock
    {
        Position position;
        position.board[b1] = Piece::N;
        position.turn = {Color::w, CastlingMask::_, noEnPassantTarget, 1, 1};

        position = applyMove(position, Move(b1, c3, MoveKind::Quiet_Move));
        assert(position.board[c3] == Piece::N);
        assert(position.board[b1] == Piece::_);
        assert(position.turn.activeColor() == Color::b);
        assert(position.turn.halfmove() == 2);
    }

    // Test en passant capture
    {
        Position position;
        position.board[a5] = Piece::P;
        position.board[b5] = Piece::p;
        position.turn = {Color::w, CastlingMask::_, b6, 1, 1};

        position = applyMove(position, Move(a5, b6, MoveKind::En_Passant));
        assert(position.board[b6] == Piece::P);
        assert(position.board[b5] == Piece::_);
        assert(position.turn.activeColor() == Color::b);
        assert(position.turn.halfmove() == 0);
    }

    // Test that capture of rook a by a rook removes castling rights on both sides
    {
        Position position;
        position.board[a1] = Piece::R;
        position.board[e1] = Piece::K;
        position.board[a8] = Piece::r;
        position.board[e8] = Piece::k;
        position.turn.setCastling(CastlingMask::q | CastlingMask::Q);
        position = applyMove(position, Move(a1, a8, MoveKind::Capture));
        assert(position.turn.castling() == CastlingMask::_);
    }

    std::cout << "All applyMove tests passed!" << std::endl;
}

void testIsAttacked() {
    Square whiteKingSquare = a1;
    Square blackKingSquare = f6;
    Board base;
    base[whiteKingSquare] = Piece::K;
    base[blackKingSquare] = Piece::k;

    // Test that a king is in check
    {
        Board board = base;
        board[b1] = Piece::r;
        auto occupancy = Occupancy(board, Color::w);
        assert(isAttacked(board, whiteKingSquare, occupancy));
        assert(!isAttacked(board, blackKingSquare, occupancy));
    }

    // Test that a king is not in check
    {
        Board board = base;
        board[b1] = Piece::r;
    }

    // Test that a king is in check after a move
    {
        Board board = base;
        board[b2] = Piece::r;
        testMakeAndUnmakeMove(board, Move(a1, a2, MoveKind::Quiet_Move));
        auto occupancy = Occupancy(board, Color::w);
        assert(isAttacked(board, a2, occupancy));
    }

    // Test that a king is not in check after a move
    {
        Board board;
        board[a1] = Piece::K;
        board[b1] = Piece::r;
        testMakeAndUnmakeMove(board, Move(a1, a2, MoveKind::Quiet_Move));
        auto occupancy = Occupancy(board, Color::b);

        assert(!isAttacked(board, blackKingSquare, occupancy));
    }

    // Test that this method also works for an empty square
    {
        Board board;
        board[e1] = Piece::K;
        board[f2] = Piece::r;
        auto occupancy = Occupancy(board, Color::w);

        assert(isAttacked(board, f1, occupancy));
    }

    std::cout << "All isAttacked tests passed!" << std::endl;
}

MoveVector allLegalMoves(Turn turn, Board board) {
    MoveVector moves;
    for (auto move : allLegalMovesAndCaptures(turn, board))
        if (!isCapture(move.kind)) moves.emplace_back(move);
    return moves;
}


void showCapturesAndMoves(Position position) {
    std::cout << "FEN: " << fen::to_string(position) << "\n";
    auto captures = allLegalCaptures(position.turn, position.board);
    auto moves = allLegalMoves(position.turn, position.board);
    std::cout << "Legal captures: " << captures.size() << "\n";
    for (auto move : captures) std::cout << to_string(move) << " ";
    std::cout << "\n";
    std::cout << "Legal moves: " << moves.size() << "\n";
    for (auto move : moves) std::cout << to_string(move) << " ";
    std::cout << "\n";
}

void checkMovesAndCaptures(std::string fen, size_t expectedMoves, size_t expectedCaptures) {
    auto position = fen::parsePosition(fen);
    auto legalMoves = allLegalMoves(position.turn, position.board);
    auto legalCaptures = allLegalCaptures(position.turn, position.board);
    if (legalMoves.size() != expectedMoves || legalCaptures.size() != expectedCaptures) {
        std::cout << "Expected moves: " << expectedMoves << ", got: " << legalMoves.size() << "\n";
        std::cout << "Expected captures: " << expectedCaptures << ", got: " << legalCaptures.size()
                  << "\n";
        showCapturesAndMoves(position);
    }
    assert(legalMoves.size() == expectedMoves);
    assert(legalCaptures.size() == expectedCaptures);
}

void testAllLegalMovesAndCaptures() {
    checkMovesAndCaptures("rnbqkbnr/pppppp1p/8/6p1/7P/8/PPPPPPP1/RNBQKBNR w KQkq - 0 2", 21, 1);
    // Can't castle the king through check
    checkMovesAndCaptures("rn1qkbnr/2pppppp/bp6/p7/4P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 4", 23, 0);
    // Can't castle the king that is in check
    checkMovesAndCaptures("r1bqkbnr/pppppppp/8/1B6/4P3/8/PPnPNPPP/RNBQK2R w KQkq - 0 4", 1, 1);
    {
        // The black king can castle queen side, but not king side
        auto fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/1R2K2R b Kkq - 1 1";
        auto position = fen::parsePosition(fen);
        auto castles = fen::parseUCIMove(position.board, "e8c8");
        assert(castles.kind == MoveKind::O_O_O);
        auto legalMoves = allLegalMovesAndCaptures(position.turn, position.board);
        assert(std::count(legalMoves.begin(), legalMoves.end(), castles) == 1);
        auto castled = applyMove(position, castles);

        // Check that pieces moved correctly
        assert(castled.board[c8] == Piece::k);
        assert(castled.board[d8] == Piece::r);
        assert(castled.board[e8] == Piece::_);

        // Black's castling rights are cancelled.
        assert(castled.turn.castling() == CastlingMask::K);

        checkMovesAndCaptures(fen, 36, 7);
    }

    std::cout << "All allLegalMovesAndCaptures tests passed!" << std::endl;
}

/**
 * Test that moves remove the correct castling rights
 */
void testCastlingMask() {
    {
        auto mask = castlingMask(a1, a8);
        assert(mask == (CastlingMask::Q | CastlingMask::q));
    }
}

void testAllLegalQuiescentMoves() {
    {
        std::string fen = "8/1N3k2/6p1/8/2P3P1/p7/1R2K3/8 b - - 0 58";
        auto position = fen::parsePosition(fen);
        auto moves =
            allLegalQuiescentMoves(position.turn, position.board, options::promotionMinDepthLeft);
        if (moves.size() != 2) {
            std::cout << "FEN: " << fen::to_string(position) << "\n";
            std::cout << "Moves:\n";
            for (auto move : moves) std::cout << to_string(move) << " ";
            std::cout << "\n";
        }
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

int main(int argc, char* argv[]) {
    if (argc == 2) {
        std::string fen = argv[1];
        auto position = fen::parsePosition(fen);
        showCapturesAndMoves(position);
        return 0;
    }
    testSquare();
    testMove();
    testSquareSet();
    testPiece();
    testPieceType();
    testColor();
    testPossibleMoves();
    testPossibleCaptures();
    testOccupancy();
    testDoesNotCheck();
    testPinnedPieces();
    testLegalPawnMoves();
    testLegalCaptures();
    testLegalEnPassant();
    testAllLegalCastling();
    testMakeAndUnmakeMove();
    testCastlingMask();
    testApplyMove();
    testIsAttacked();
    testAllLegalMovesAndCaptures();
    testAllLegalQuiescentMoves();
    std::cout << "All move tests passed!" << std::endl;
    return 0;
}

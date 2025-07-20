#include <cassert>
#include <iostream>

#include "common.h"
#include "fen.h"
#include "moves.h"
#include "moves_table.h"

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

/**
 * Test that moves remove the correct castling rights
 */
void testCastlingMask() {
    {
        auto mask = castlingMask(a1, a8);
        assert(mask == (CastlingMask::Q | CastlingMask::q));
    }
}

int main(int argc, [[maybe_unused]] char* argv[]) {
    assert(argc == 1);
    testMakeAndUnmakeMove();
    testCastlingMask();
    testApplyMove();
    testIsAttacked();
    std::cout << "All move tests passed!" << std::endl;
    return 0;
}

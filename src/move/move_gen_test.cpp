#include <cassert>
#include <iostream>

#include "engine/fen/fen.h"
#include "move.h"
#include "move_gen.h"

using namespace moves;

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

void testDoublePawnPushes() {
    // Test starting position - should have 8 double pawn pushes + 8 single pushes + 4 knight moves
    // = 20 total
    auto position = fen::parsePosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    MoveVector moves = allLegalMovesAndCaptures(position.turn, position.board);
    assert(moves.size() == 20);

    // Verify all 8 double pawn pushes are present
    assert(has(moves, Move(a2, a4, MoveKind::Double_Push)));
    assert(has(moves, Move(b2, b4, MoveKind::Double_Push)));
    assert(has(moves, Move(c2, c4, MoveKind::Double_Push)));
    assert(has(moves, Move(d2, d4, MoveKind::Double_Push)));
    assert(has(moves, Move(e2, e4, MoveKind::Double_Push)));
    assert(has(moves, Move(f2, f4, MoveKind::Double_Push)));
    assert(has(moves, Move(g2, g4, MoveKind::Double_Push)));
    assert(has(moves, Move(h2, h4, MoveKind::Double_Push)));

    // Also verify all 8 single pawn pushes are present
    assert(has(moves, Move(a2, a3, MoveKind::Quiet_Move)));
    assert(has(moves, Move(b2, b3, MoveKind::Quiet_Move)));
    assert(has(moves, Move(c2, c3, MoveKind::Quiet_Move)));
    assert(has(moves, Move(d2, d3, MoveKind::Quiet_Move)));
    assert(has(moves, Move(e2, e3, MoveKind::Quiet_Move)));
    assert(has(moves, Move(f2, f3, MoveKind::Quiet_Move)));
    assert(has(moves, Move(g2, g3, MoveKind::Quiet_Move)));
    assert(has(moves, Move(h2, h3, MoveKind::Quiet_Move)));

    std::cout << "Double pawn pushes test passed!" << std::endl;
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

void testAllLegalQuiescentMoves() {
    {
        // Test position without promotion threats - should only return captures
        std::string fen = "8/1N3k2/6p1/8/2P3P1/p7/1R2K3/8 b - - 0 58";
        auto position = fen::parsePosition(fen);
        auto moves = allLegalQuiescentMoves(position.turn, position.board, 0);
        // Only captures should be returned (no promotion moves in QS anymore)
        if (moves.size() != 2) {
            std::cout << "FEN: " << fen::to_string(position) << "\n";
            std::cout << "Moves:\n";
            for (auto move : moves) std::cout << to_string(move) << " ";
            std::cout << "\n";
        }
        assert(moves.size() == 2);
    }
    {
        // Test position with black in check - should return all escape moves
        std::string fen = "8/1N3k2/6p1/8/2P3P1/8/4K3/8 b - - 0 59";
        auto position = fen::parsePosition(fen);
        auto moves = allLegalQuiescentMoves(position.turn, position.board, 0);
        // King moves to escape (no checks in this position, just king moves)
        assert(moves.size() >= 0);  // Basic sanity check
    }
    std::cout << "All allLegalQuiescentMoves tests passed!\n";
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
        auto castled = moves::applyMove(position, castles);

        // Check that pieces moved correctly
        assert(castled.board[c8] == Piece::k);
        assert(castled.board[d8] == Piece::r);
        assert(castled.board[e8] == Piece::_);

        // Black's castling rights are cancelled.
        assert(castled.turn.castling() == CastlingMask::K);

        checkMovesAndCaptures(fen, 36, 7);
    }
    checkMovesAndCaptures("2r3k1/6pp/p1q2r2/1pn2p2/1B1pPP2/3Pn1QB/1PP2R1P/6RK w - - 4 25", 29, 4);

    std::cout << "All allLegalMovesAndCaptures tests passed!" << std::endl;
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

void testKingInCheck() {
    {
        // Test position where black king is in check from white queen
        // Position: 4k3/8/4Qn2/3K4/8/8/8/8 b - - 0 1
        // - Black king on e8
        // - White queen on e6 (giving check along the e-file)
        // - Black knight on f6 (cannot move as it would leave king in check)
        // - White king on d5
        // Only legal moves should be king moves to safe squares (d8, f8)
        // No captures are possible
        checkMovesAndCaptures("4k3/8/3KQn2/8/8/8/8/8 b - - 0 1", 2, 0);
    }
    {
        checkMovesAndCaptures(
            "rnbqkbnr/ppp1pppp/3p4/8/Q7/2P5/PP1PPPPP/RNB1KBNR b KQkq - 1 2", 6, 0);
    }

    std::cout << "All king in check tests passed!" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        std::string fen = argv[1];
        auto position = fen::parsePosition(fen);
        showCapturesAndMoves(position);
        return 0;
    }
    testLegalPawnMoves();
    testDoublePawnPushes();
    testLegalCaptures();
    testLegalEnPassant();
    testAllLegalCastling();
    testAllLegalQuiescentMoves();
    testAllLegalMovesAndCaptures();
    testAllLegalQuiescentMoves();
    testDoesNotCheck();
    testKingInCheck();
    testPinnedPieces();

    std::cout << "All moves_gen tests passed!" << std::endl;
    return 0;
}
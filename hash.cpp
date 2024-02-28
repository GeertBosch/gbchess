#include "hash.h"
#include "moves.h"
#include <random>

std::array<uint64_t, kNumHashVectors> hashVectors = []() {
    std::array<uint64_t, kNumHashVectors> vectors;
    std::ranlux48 gen(0xbad5eed5'bad5eed5);
    for (auto& v : vectors) v = gen();
    return vectors;
}();

Hash::Hash(Position position) {
    int location = 0;
    for (auto square : SquareSet::occupancy(position.board))
        toggle(position.board[square], square.index());
    if (position.activeColor() == Color::BLACK) toggle(BLACK_TO_MOVE);
    if (position.turn.castlingAvailability != CastlingMask::NONE)
        toggle(ExtraVectors(CASTLING_1 - 1 + uint8_t(position.turn.castlingAvailability)));
    if (position.turn.enPassantTarget.index())
        toggle(ExtraVectors(position.turn.enPassantTarget.file() + EN_PASSANT_A));
}

void Hash::applyMove(const Board& board, Move mv) {
    auto piece = board[mv.from()];
    auto target = board[mv.to()];
    move(piece, mv.from().index(), mv.to().index());
    switch (mv.kind()) {
    case MoveKind::QUIET_MOVE: break;
    case MoveKind::DOUBLE_PAWN_PUSH: toggle(ExtraVectors(mv.to().file() + EN_PASSANT_A)); break;
    case MoveKind::KING_CASTLE:  // Assume the move has the king move, so adjust the rook here.
        move(addColor(PieceType::ROOK, color(piece)),
             (color(piece) == Color::WHITE ? Position::whiteKingSideRook
                                           : Position::blackKingSideRook)
                 .index(),
             (color(piece) == Color::WHITE ? Position::whiteRookCastledKingSide
                                           : Position::blackRookCastledKingSide)
                 .index());
        break;
    case MoveKind::QUEEN_CASTLE:  // Assume the move has the king move, so adjust the rook here.
        move(addColor(PieceType::ROOK, color(piece)),
             (color(piece) == Color::WHITE ? Position::whiteQueenSideRook
                                           : Position::blackQueenSideRook)
                 .index(),
             (color(piece) == Color::WHITE ? Position::whiteRookCastledQueenSide
                                           : Position::blackRookCastledQueenSide)
                 .index());
        break;

    case MoveKind::CAPTURE: toggle(target, mv.to().index()); break;
    case MoveKind::EN_PASSANT:
        // Depending of the color of our piece, the captured pawn is either above or below the
        // destination square.
        toggle(target, mv.to().index() + (color(piece) == Color::WHITE ? -kNumFiles : kNumFiles));
        break;
    case MoveKind::KNIGHT_PROMOTION:
    case MoveKind::BISHOP_PROMOTION:
    case MoveKind::ROOK_PROMOTION:
    case MoveKind::QUEEN_PROMOTION:
        toggle(piece, mv.to().index());  // Remove the pawn, add the promoted piece
        toggle(addColor(promotionType(mv.kind()), color(piece)), mv.to().index());
        break;
    case MoveKind::KNIGHT_PROMOTION_CAPTURE:
    case MoveKind::BISHOP_PROMOTION_CAPTURE:
    case MoveKind::ROOK_PROMOTION_CAPTURE:
    case MoveKind::QUEEN_PROMOTION_CAPTURE:
        toggle(target, mv.to().index());  // Remove the captured piece
        toggle(piece, mv.to().index());   // Remove the pawn, add the promoted piece
        toggle(addColor(promotionType(mv.kind()), color(piece)), mv.to().index());
        break;
    }
}

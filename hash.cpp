#include <array>
#include <random>

#include "common.h"
#include "hash.h"
#include "moves.h"

std::array<uint64_t, kNumHashVectors> hashVectors = []() {
    std::array<uint64_t, kNumHashVectors> vectors;
    std::mt19937_64 gen(0xbad5eed5bad5eed5ULL);  // 64-bit seed
    for (auto& v : vectors) v = gen();
    return vectors;
}();

Hash::Hash(Position position) {
    int location = 0;
    for (auto square : SquareSet::occupancy(position.board))
        toggle(position.board[square], square.index());
    if (position.active() == Color::BLACK) toggle(BLACK_TO_MOVE);
    if (position.turn.castling() != CastlingMask::_) toggle(position.turn.castling());
    if (position.turn.enPassant().index())
        toggle(ExtraVectors(position.turn.enPassant().file() + EN_PASSANT_A));
}

namespace {
static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::WHITE),
                                                 CastlingInfo(Color::BLACK)};
}  // namespace

void Hash::applyMove(const Position& position, Move mv) {
    const Board& board = position.board;
    auto piece = board[mv.from()];
    auto target = board[mv.to()];

    // Always assume we move a piece and switch the player to move.
    move(piece, mv.from().index(), mv.to().index());
    toggle(BLACK_TO_MOVE);

    // Cancel en passant target if it was set.
    if (position.turn.enPassant() != noEnPassantTarget)
        toggle(ExtraVectors(position.turn.enPassant().file() + EN_PASSANT_A));

    // Cancel any castling rights according to the move squares.
    toggle(position.turn.castling() & castlingMask(mv.from(), mv.to()));

    switch (mv.kind()) {
    case MoveKind::QUIET_MOVE: break;
    case MoveKind::DOUBLE_PAWN_PUSH: toggle(ExtraVectors(mv.to().file() + EN_PASSANT_A)); break;
    case MoveKind::KING_CASTLE:  // Assume the move has the king move, so adjust the rook here.
    {
        auto& info = castlingInfo[int(color(piece))];
        move(info.rook, info.kingSide[1][0].index(), info.kingSide[1][1].index());
    } break;
    case MoveKind::QUEEN_CASTLE:  // Assume the move has the king move, so adjust the rook here.
    {
        auto& info = castlingInfo[int(color(piece))];
        move(info.rook, info.queenSide[1][0].index(), info.queenSide[1][1].index());
    } break;

    case MoveKind::CAPTURE: toggle(target, mv.to().index()); break;
    case MoveKind::EN_PASSANT:
        // Depending of the color of our piece, the captured pawn is either above or below the
        // destination square.
        {
            auto targetSquare = Square{mv.to().file(), mv.from().rank()};
            target = board[targetSquare];
            toggle(target, targetSquare.index());
        }
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

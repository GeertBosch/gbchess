#include <array>

#include "common.h"
#include "hash.h"
#include "moves.h"
#include "random.h"

std::array<uint64_t, kNumHashVectors> hashVectors = []() {
    std::array<uint64_t, kNumHashVectors> vectors;
    xorshift gen;
    for (auto& v : vectors) v = gen();
    return vectors;
}();

Hash::Hash(const Position& position) {
    for (auto square : occupancy(position.board)) toggle(position.board[square], square);
    if (position.active() == Color::b) toggle(BLACK_TO_MOVE);
    if (position.turn.castling() != CastlingMask::_) toggle(position.turn.castling());
    if (position.turn.enPassant())
        toggle(ExtraVectors(file(position.turn.enPassant()) + EN_PASSANT_A));
}

namespace {
static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::w), CastlingInfo(Color::b)};
}  // namespace

void Hash::applyMove(const Position& position, Move mv) {
    const Board& board = position.board;
    auto piece = board[mv.from];
    auto target = board[mv.to];

    // Always assume we move a piece and switch the player to move.
    move(piece, mv.from, mv.to);
    toggle(BLACK_TO_MOVE);

    // Cancel en passant target if it was set.
    if (position.turn.enPassant() != noEnPassantTarget)
        toggle(ExtraVectors(file(position.turn.enPassant()) + EN_PASSANT_A));

    // Cancel any castling rights according to the move squares.
    toggle(position.turn.castling() & castlingMask(mv.from, mv.to));

    switch (mv.kind) {
    case MoveKind::Quiet_Move: break;
    case MoveKind::Double_Push: toggle(ExtraVectors(file(mv.to) + EN_PASSANT_A)); break;
    case MoveKind::O_O:  // Assume the move has the king move, so adjust the rook here.
    {
        auto& info = castlingInfo[int(color(piece))];
        move(info.rook, info.kingSide[1].from, info.kingSide[1].to);
    } break;
    case MoveKind::O_O_O:  // Assume the move has the king move, so adjust the rook here.
    {
        auto& info = castlingInfo[int(color(piece))];
        move(info.rook, info.queenSide[1].from, info.queenSide[1].to);
    } break;

    case MoveKind::Capture: toggle(target, mv.to); break;
    case MoveKind::En_Passant:
        // Depending of the color of our piece, the captured pawn is either above or below the
        // destination square.
        {
            auto targetSquare = makeSquare(file(mv.to), rank(mv.from));
            target = board[targetSquare];
            toggle(target, targetSquare);
        }
        break;
    case MoveKind::Knight_Promotion:
    case MoveKind::Bishop_Promotion:
    case MoveKind::Rook_Promotion:
    case MoveKind::Queen_Promotion:
        toggle(piece, mv.to);  // Remove the pawn, add the promoted piece
        toggle(addColor(promotionType(mv.kind), color(piece)), mv.to);
        break;
    case MoveKind::Knight_Promotion_Capture:
    case MoveKind::Bishop_Promotion_Capture:
    case MoveKind::Rook_Promotion_Capture:
    case MoveKind::Queen_Promotion_Capture:
        toggle(target, mv.to);  // Remove the captured piece
        toggle(piece, mv.to);   // Remove the pawn, add the promoted piece
        toggle(addColor(promotionType(mv.kind), color(piece)), mv.to);
        break;
    }
}

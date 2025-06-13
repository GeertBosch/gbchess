#include <array>
#include <random>

#include "common.h"
#include "hash.h"
#include "moves.h"

std::array<HashValue, kNumHashVectors> hashVectors = []() {
    std::array<HashValue, kNumHashVectors> vectors;
    std::mt19937_64 gen(0xbad5eed5bad5eed5ULL);  // 64-bit seed
    for (auto& v : vectors) v = gen();

    if constexpr (sizeof(HashValue) > 64)
        for (auto& v : vectors) v = (v << 64) + gen();
    return vectors;
}();

Hash::Hash(const Position& position) {
    for (auto square : SquareSet::occupancy(position.board)) toggle(position.board[square], square);
    if (position.active() == Color::b) toggle(BLACK_TO_MOVE);
    if (position.turn.castling() != CastlingMask::_) toggle(position.turn.castling());
    if (position.turn.enPassant())
        toggle(ExtraVectors(file(position.turn.enPassant()) + EN_PASSANT_A));
}

namespace {
static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::w), CastlingInfo(Color::b)};
}  // namespace

void Hash::applyMove(const Position& position, Move mv) {
    auto piece = position.board[mv.from];
    auto target =
        position.board[mv.kind == MoveKind::En_Passant ? makeSquare(file(mv.to), rank(mv.from))
                                                       : mv.to];
    applyMove(position.turn, {mv, piece, target});
}

void Hash::applyMove(Turn turn, MoveWithPieces mwp) {
    auto [mv, piece, target] = mwp;

    // Always assume we move a piece and switch the player to move.
    move(piece, mv.from, mv.to);
    toggle(BLACK_TO_MOVE);

    // Cancel en passant target if it was set.
    if (turn.enPassant() != noEnPassantTarget)
        toggle(ExtraVectors(file(turn.enPassant()) + EN_PASSANT_A));

    // Cancel any castling rights according to the move squares.
    toggle(turn.castling() & castlingMask(mv.from, mv.to));

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

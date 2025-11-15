#include <array>

#include "castling_info.h"
#include "common.h"
#include "hash.h"
#include "move/move.h"
#include "util/random.h"

std::array<HashValue, kNumHashVectors> hashVectors = []() {
    std::array<HashValue, kNumHashVectors> vectors;
    xorshift gen;
    for (auto& v : vectors) v = gen();

    if constexpr (sizeof(HashValue) > sizeof(uint64_t))
        for (auto& v : vectors) v = (v << 64) + gen();
    return vectors;
}();

Hash::Hash(const Position& position) {
    for (auto square : occupancy(position.board)) toggle(position.board[square], square);
    if (position.active() == Color::b) toggle(BLACK_TO_MOVE);
    if (position.turn.castling() != CastlingMask::_) toggle(position.turn.castling());
    if (position.turn.enPassant())
        toggle(ExtraVectors(file(position.turn.enPassant()) + EN_PASSANT_A));
}

void Hash::applyMove(const Position& position, Move mv) {
    auto piece = position.board[mv.from];
    auto target =
        position.board[mv.kind == MoveKind::En_Passant ? makeSquare(file(mv.to), rank(mv.from))
                                                       : mv.to];
    *this = ::applyMove(*this, position.turn, {mv, piece, target});
}

Hash applyMove(Hash hash, Turn turn, MoveWithPieces mwp) {
    auto [mv, piece, target] = mwp;

    // Always assume we move a piece and switch the player to move.
    hash.move(piece, mv.from, mv.to);
    hash.toggle(Hash::BLACK_TO_MOVE);

    // Cancel en passant target if it was set.
    if (turn.enPassant() != noEnPassantTarget)
        hash.toggle(Hash::ExtraVectors(file(turn.enPassant()) + Hash::EN_PASSANT_A));

    // Cancel any castling rights according to the move squares.
    hash.toggle(turn.castling() & moves::castlingMask(mv.from, mv.to));

    switch (mv.kind) {
    case MoveKind::Quiet_Move: break;
    case MoveKind::Double_Push:
        hash.toggle(Hash::ExtraVectors(file(mv.to) + Hash::EN_PASSANT_A));
        break;
    case MoveKind::O_O:  // Assume the move has the king move, so adjust the rook here.
    {
        auto& info = castlingInfo[int(color(piece))];
        hash.move(info.rook, info.kingSide[1].from, info.kingSide[1].to);
    } break;
    case MoveKind::O_O_O:  // Assume the move has the king move, so adjust the rook here.
    {
        auto& info = castlingInfo[int(color(piece))];
        hash.move(info.rook, info.queenSide[1].from, info.queenSide[1].to);
    } break;

    case MoveKind::Capture: hash.toggle(target, mv.to); break;
    case MoveKind::En_Passant:
        // Depending of the color of our piece, the captured pawn is either above or below the
        // destination square.
        {
            auto targetSquare = makeSquare(file(mv.to), rank(mv.from));
            hash.toggle(target, targetSquare);
        }
        break;
    case MoveKind::Knight_Promotion:
    case MoveKind::Bishop_Promotion:
    case MoveKind::Rook_Promotion:
    case MoveKind::Queen_Promotion:
        hash.toggle(piece, mv.to);  // Remove the pawn, add the promoted piece
        hash.toggle(addColor(promotionType(mv.kind), color(piece)), mv.to);
        break;
    case MoveKind::Knight_Promotion_Capture:
    case MoveKind::Bishop_Promotion_Capture:
    case MoveKind::Rook_Promotion_Capture:
    case MoveKind::Queen_Promotion_Capture:
        hash.toggle(target, mv.to);  // Remove the captured piece
        hash.toggle(piece, mv.to);   // Remove the pawn, add the promoted piece
        hash.toggle(addColor(promotionType(mv.kind), color(piece)), mv.to);
        break;
    }
    return hash;
}

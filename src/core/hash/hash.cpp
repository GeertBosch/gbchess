#include <array>

#include "core/castling_info.h"
#include "core/core.h"
#include "core/random.h"

#include "hash.h"

std::array<HashValue, kNumHashVectors> hashVectors = []() {
    std::array<HashValue, kNumHashVectors> vectors;
    xorshift gen;
    for (auto& v : vectors) v = gen();

    if constexpr (sizeof(HashValue) > sizeof(uint64_t))
        for (auto& v : vectors) v = (v << 64) + gen();
    return vectors;
}();

Hash::Hash(const Board& board, Turn turn) {
    for (auto square : squares)
        if (board[square] != Piece::_) toggle(board[square], square);
    if (turn.activeColor() == Color::b) toggle(BLACK_TO_MOVE);
    if (turn.castling() != CastlingMask::_) toggle(turn.castling());
    if (turn.enPassant()) toggle(ExtraVectors(file(turn.enPassant()) + EN_PASSANT_A));
}

void Hash::applyMove(Turn turn, MoveWithPieces mwp, CastlingMask mask) {
    *this = ::applyMove(*this, turn, mwp, mask);
}

Hash applyMove(Hash hash, Turn turn, MoveWithPieces mwp, CastlingMask mask) {
    auto [mv, piece, target] = mwp;

    // Always assume we move a piece and switch the player to move.
    hash.move(piece, mv.from, mv.to);
    hash.toggle(Hash::BLACK_TO_MOVE);

    // Cancel en passant target if it was set.
    if (turn.enPassant() != noEnPassantTarget)
        hash.toggle(Hash::ExtraVectors(file(turn.enPassant()) + Hash::EN_PASSANT_A));

    // Cancel any castling rights according to the move squares.
    hash.toggle(turn.castling() & mask);

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

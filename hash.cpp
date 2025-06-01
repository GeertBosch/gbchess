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
    for (auto square : SquareSet::occupancy(position.board))
        toggle(position.board[square], index(square));
    if (position.active() == Color::BLACK) toggle(BLACK_TO_MOVE);
    if (position.turn.castling() != CastlingMask::_) toggle(position.turn.castling());
    if (index(position.turn.enPassant()))
        toggle(ExtraVectors(file(position.turn.enPassant()) + EN_PASSANT_A));
}

namespace {
static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::WHITE),
                                                 CastlingInfo(Color::BLACK)};
}  // namespace

void Hash::applyMove(const Position& position, Move mv) {
    const Board& board = position.board;
    auto piece = board[mv.from];
    auto target = board[mv.to];

    // Always assume we move a piece and switch the player to move.
    move(piece, index(mv.from), index(mv.to));
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
        move(info.rook, index(info.kingSide[1][0]), index(info.kingSide[1][1]));
    } break;
    case MoveKind::O_O_O:  // Assume the move has the king move, so adjust the rook here.
    {
        auto& info = castlingInfo[int(color(piece))];
        move(info.rook, index(info.queenSide[1][0]), index(info.queenSide[1][1]));
    } break;

    case MoveKind::Capture: toggle(target, index(mv.to)); break;
    case MoveKind::En_Passant:
        // Depending of the color of our piece, the captured pawn is either above or below the
        // destination square.
        {
            auto targetSquare = makeSquare(file(mv.to), rank(mv.from));
            target = board[targetSquare];
            toggle(target, index(targetSquare));
        }
        break;
    case MoveKind::Knight_Promotion:
    case MoveKind::Bishop_Promotion:
    case MoveKind::Rook_Promotion:
    case MoveKind::Queen_Promotion:
        toggle(piece, index(mv.to));  // Remove the pawn, add the promoted piece
        toggle(addColor(promotionType(mv.kind), color(piece)), index(mv.to));
        break;
    case MoveKind::Knight_Promotion_Capture:
    case MoveKind::Bishop_Promotion_Capture:
    case MoveKind::Rook_Promotion_Capture:
    case MoveKind::Queen_Promotion_Capture:
        toggle(target, index(mv.to));  // Remove the captured piece
        toggle(piece, index(mv.to));   // Remove the pawn, add the promoted piece
        toggle(addColor(promotionType(mv.kind), color(piece)), index(mv.to));
        break;
    }
}

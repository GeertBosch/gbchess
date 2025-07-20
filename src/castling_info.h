#pragma once
#include <array>

#include "common.h"
using CastlingMove = std::array<FromTo, 2>;  // King and Rook moves for castling
struct CastlingInfo {
    const Piece rook;
    const Piece king;
    const CastlingMask kingSideMask;
    const CastlingMask queenSideMask;

    const CastlingMove kingSide;
    const CastlingMove queenSide;

    constexpr CastlingInfo(Color color)
        : rook(addColor(PieceType::ROOK, color)),
          king(addColor(PieceType::KING, color)),
          kingSideMask(color == Color::w ? CastlingMask::K : CastlingMask::k),
          queenSideMask(color == Color::w ? CastlingMask::Q : CastlingMask::q),

          kingSide(color == Color::w ? CastlingMove{FromTo{e1, g1}, FromTo{h1, f1}}
                                     : CastlingMove{FromTo{e8, g8}, FromTo{h8, f8}}),
          queenSide(color == Color::w ? CastlingMove{FromTo{e1, c1}, FromTo{a1, d1}}
                                      : CastlingMove{FromTo{e8, c8}, FromTo{a8, d8}}) {};
};
static constexpr CastlingInfo castlingInfo[2] = {CastlingInfo(Color::w), CastlingInfo(Color::b)};

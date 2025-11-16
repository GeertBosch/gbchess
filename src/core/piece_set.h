#pragma once
#include <cstdint>

#include "common.h"

struct PieceSet {
    uint16_t pieces = 0;
    PieceSet() = default;
    constexpr PieceSet(Piece piece) : pieces(1 << index(piece)) {}
    constexpr PieceSet(PieceType pieceType)
        : pieces((1 << index(addColor(pieceType, Color::w))) |
                 (1 << index(addColor(pieceType, Color::b)))) {}
    constexpr PieceSet(std::initializer_list<Piece> piecesList) {
        for (auto piece : piecesList) pieces |= 1 << index(piece);
    }
    constexpr PieceSet(std::initializer_list<PieceType> types) {
        for (auto pieceType : types)
            pieces |= (1 << index(addColor(pieceType, Color::w))) |
                (1 << index(addColor(pieceType, Color::b)));
    }
    constexpr PieceSet(uint16_t pieces) : pieces(pieces) {}
    constexpr PieceSet operator|(PieceSet other) const { return PieceSet(pieces | other.pieces); }
    bool contains(Piece piece) const { return pieces & (1 << index(piece)); }
};

static constexpr PieceSet sliders = {Piece::B, Piece::b, Piece::R, Piece::r, Piece::Q, Piece::q};

#pragma once
#include <cstdint>

#include "core.h"

struct PieceSet {
    uint16_t pieces = 0;
    constexpr PieceSet() = default;
    constexpr PieceSet(std::initializer_list<Piece> piecesList) {
        for (auto piece : piecesList) pieces |= 1 << index(piece);
    }
    constexpr PieceSet(std::initializer_list<PieceType> types) {
        for (auto pieceType : types)
            pieces |= (1 << index(addColor(pieceType, Color::w))) |
                (1 << index(addColor(pieceType, Color::b)));
    }
    constexpr bool contains(Piece piece) const { return pieces & (1 << index(piece)); }
};

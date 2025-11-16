#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <type_traits>

#include "core/core.h"
#include "core/options.h"

// Implement a hashing method for chess positions using Zobrist hashing
// https://en.wikipedia.org/wiki/Zobrist_hashing This relies just on the number of locations
// ("squares") and number of pieces. The hash allows for efficient incremental updating of the hash
// value when a move is made.

// 1 for black to move, 1 for each castling right, 8 for en passant file
static constexpr int kNumExtraVectors = 24;
static constexpr int kNumBoardVectors = kNumPieces * kNumSquares;
static constexpr int kNumHashVectors = kNumBoardVectors + kNumExtraVectors;

using HashValue = std::conditional_t<options::hash128, __uint128_t, uint64_t>;

// A random 64- or 128-bit integer for each piece on each square, as well as the extra vectors.
extern std::array<HashValue, kNumHashVectors> hashVectors;

constexpr PieceType promotionType(MoveKind kind) {
    return PieceType((index(kind) & 3) + 1);
}

// A Hash is a 64- or 128-bit integer that represents a position. It is the XOR of the hash vectors
// for each piece on each square, as well as the applicable extra vectors.
class Hash {

public:
    enum ExtraVectors {
        BLACK_TO_MOVE = kNumBoardVectors + 0,
        CASTLING_0 = kNumBoardVectors + 1,
        CASTLING_1 = kNumBoardVectors + 2,
        CASTLING_2 = kNumBoardVectors + 3,
        CASTLING_3 = kNumBoardVectors + 4,
        EN_PASSANT_A = kNumBoardVectors + 16,
        EN_PASSANT_B = kNumBoardVectors + 17,
        EN_PASSANT_C = kNumBoardVectors + 18,
        EN_PASSANT_D = kNumBoardVectors + 19,
        EN_PASSANT_E = kNumBoardVectors + 20,
        EN_PASSANT_F = kNumBoardVectors + 21,
        EN_PASSANT_G = kNumBoardVectors + 22,
        EN_PASSANT_H = kNumBoardVectors + 23,
    };

    Hash() = default;
    Hash(const Position& position);

    HashValue operator()() const { return hash; }
    bool operator==(const Hash& other) const { return hash == other.hash; }

    void move(Piece piece, int from, int to) {
        toggle(piece, from);
        toggle(piece, to);
    }
    void capture(Piece piece, Piece target, int from, int to) {
        toggle(piece, from);
        toggle(target, to);
        toggle(piece, to);
    }

    // Assumes that passed in position is the same as the one used to construct this hash.
    // Cancels out castling rights and en passant targets.
    void applyMove(const Position& position, Move mv);

    // Use toggle to add/remove a piece or non piece/location vector.
    void toggle(Piece piece, int location) {
        dassert(piece != Piece::_);
        toggle(index(piece) * kNumSquares + location);
    }
    void toggle(int vector) { hash ^= hashVectors[vector]; }
    void toggle(ExtraVectors extra) { toggle(int(extra)); }
    void toggle(CastlingMask mask) {
        for (int i = 0; i < 4; ++i)
            if ((mask & CastlingMask(1 << i)) != CastlingMask::_)
                toggle(ExtraVectors(CASTLING_0 + i));
    }

    /** Create a hash for a null move (flip side to move and clear en passant) */
    Hash makeNullMove(const Turn& turn) const {
        Hash result = *this;
        result.toggle(BLACK_TO_MOVE);
        // Clear en passant hash if it was set (matching Turn::makeNullMove behavior)
        if (turn.enPassant() != noEnPassantTarget)
            result.toggle(ExtraVectors(file(turn.enPassant()) + EN_PASSANT_A));
        return result;
    }

private:
    HashValue hash = 0;
};

// Updates the given hash at the given turn, based on the passed move.
Hash applyMove(Hash hash, Turn turn, MoveWithPieces mwp);

namespace std {
template <>
struct hash<Hash> {
    std::size_t operator()(const Hash& h) const { return static_cast<std::size_t>(h()); }
};
template <>
struct hash<Position> {
    std::size_t operator()(const Position& p) const { return static_cast<std::size_t>(Hash(p)()); }
};
}  // namespace std

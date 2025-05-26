#include <array>
#include <cstdint>
#include <functional>

#include "common.h"

// Implement a hashing method for chess positions using Zobrist hashing
// https://en.wikipedia.org/wiki/Zobrist_hashing This relies just on the number of locations
// ("squares") and number of pieces, where we assume piece 0 to be "no piece". The hash allows for
// efficient incremental updating of the hash value when a move is made.

// 1 for black to move, 1 for each castling right, 8 for en passant file
static constexpr int kNumExtraVectors = 24;
static constexpr int kNumBoardVectors = kNumPieces * kNumSquares;
static constexpr int kNumHashVectors = kNumBoardVectors + kNumExtraVectors;

// A random 64-bit integer for each piece on each square, as well as the extra vectors. The first
// piece is None, but it is not omitted here, as it allows removing a hard-to-predict branch in the
// hash function.
extern std::array<uint64_t, kNumHashVectors> hashVectors;

// A Hash is a 64-bit integer that represents a position. It is the XOR of the hash vectors for
// each piece on each square, as well as the applicable extra vectors.
class Hash {
    uint64_t hash = 0;

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
    Hash(Position position);

    size_t operator()() const { return hash; }
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
    void toggle(ExtraVectors extra) { toggle(kNumBoardVectors + int(extra)); }
    void toggle(CastlingMask mask) {
        for (int i = 0; i < 4; ++i)
            if ((mask & CastlingMask(1 << i)) != CastlingMask::_)
                toggle(ExtraVectors(CASTLING_0 + i));
    }
};

namespace std {
template <>
struct hash<Hash> {
    std::size_t operator()(const Hash& h) const { return std::hash<uint64_t>()(h()); }
};
}  // namespace std

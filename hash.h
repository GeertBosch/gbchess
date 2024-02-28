#include <array>
#include <cstdint>

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
        CASTLING_1 = kNumBoardVectors + 1,
        CASTLING_15 = kNumBoardVectors + 15,
        EN_PASSANT_A = kNumBoardVectors + 16,
        EN_PASSANT_H = kNumBoardVectors + 23,
    };

    Hash() = default;
    Hash(Position position);

    uint64_t operator()() { return hash; }

    void move(Piece piece, int from, int to) {
        toggle(piece, from);
        toggle(piece, to);
    }
    void capture(Piece piece, Piece target, int from, int to) {
        toggle(piece, from);
        toggle(target, to);
        toggle(piece, to);
    }

    // Does not cancel out castling rights or en passant targets.
    // Assumes that passed in board is the same as the board used to construct this hash.
    void applyMove(const Board& board, Move mv);

    // Use toggle to add/remove a piece or non piece/location vector.
    void toggle(Piece piece, int location) { toggle(index(piece) * kNumSquares + location); }
    void toggle(int vector) { hash ^= hashVectors[vector]; }
    void toggle(ExtraVectors extra) { toggle(kNumBoardVectors + int(extra)); }
};

#include "square_set.h"
#include "sse2.h"

/**
 * Returns the bits corresponding to the bytes in the input that contain the nibble.
 * Note: nibble is assumed to be at most 4 bits.
 * The implementation really finds everything unequal and then inverts the result.
 */
uint64_t equalSet(uint64_t input, uint8_t nibble) {
    input ^= 0x01010101'01010101ull * nibble;  // 0 nibbles in the input indicate the piece
    input += 0x7f3f1f0f'7f3f1f0full;           // Cause overflow in the right bits
    input &= 0x80402010'80402010ull;           // These are them, the 8 occupancy bits
    input >>= 4;                               // The low nibbles is where the action is
    input += input >> 16;
    input += input >> 8;
    input += (input >> 28);  // Merge in the high nibbel from the high word
    input &= 0xffull;
    input ^= 0xffull;  // Invert to get the bits that are equal
    return input;
}

uint64_t equalSet(__m128i input, uint8_t nibble) {
    __m128i cmp = {0x01010101'01010101ll * nibble, 0x01010101'01010101ll * nibble};
    __m128i res = _mm_cmpeq_epi8(input, cmp);
    return static_cast<uint16_t>(_mm_movemask_epi8(res));  // avoid sign extending
}

template <typename T>
uint64_t equalSetT(std::array<Piece, 64> squares, Piece piece, bool invert) {
    static_assert(kNumPieces <= 16, "Piece must fit in 4 bits");

    uint64_t set = 0;
    for (size_t j = 0; j < sizeof(squares); j += sizeof(T)) {
        T input;
        memcpy(&input, &squares[j], sizeof(T));
        set |= equalSet(input, static_cast<uint8_t>(piece)) << j;
    }

    return invert ? ~set : set;
}

uint64_t equalSet(std::array<Piece, 64> squares, Piece piece, bool invert) {
    uint64_t res;
    if constexpr (haveSSE2)
        res = equalSetT<__m128i>(squares, piece, invert);
    else
        res = equalSetT<uint64_t>(squares, piece, invert);

    if constexpr (debug && haveSSE2) {
        auto ref = equalSetT<uint64_t>(squares, piece, invert);
        assert(res == ref);
    }
    return res;
}

/**
 * Returns the bits corresponding to the bytes in the input that are less than the nibble.
 * Note: nibble is assumed to be at most 4 bits and not zero.
 * The implementation really finds everything greater than or equal and then inverts the result.
 */
uint64_t lessSet(uint64_t input, uint8_t nibble) {
    nibble = 0x10 - nibble;  // The amount to add to overflow into the high nibble for >=
    input += 0x01010101'01010101ull * nibble;  // broadcast to all bytes
    input += 0x70301000'70301000ull;           // Cause overflow in the right bits
    input &= 0x80402010'80402010ull;           // These are them, the 8 occupancy bits
    input >>= 4;                               // The low nibbles is where the action is
    input += input >> 16;
    input += input >> 8;
    input += (input >> 28);  // Merge in the high nibbel from the high word
    input &= 0xffull;
    input ^= 0xffull;  // Invert to get the bits that are less
    return input;
}

uint64_t lessSet(__m128i input, uint8_t nibble) {
    __m128i cmp = {0x01010101'01010101ll * nibble, 0x01010101'01010101ll * nibble};
    __m128i res = _mm_cmplt_epi8(input, cmp);
    return static_cast<uint16_t>(_mm_movemask_epi8(res));  // avoid sign extending
}

/**
 *  Returns the bits corresponding to the bytes in the input that are less than the piece.
 */
template <typename T>
uint64_t lessSetT(std::array<Piece, 64> squares, Piece piece, bool invert) {
    static_assert(kNumPieces <= 16, "Piece must fit in 4 bits");
    if (!int(piece)) return invert ? 0xffffffffffffffffull : 0;  // Special case for empty squares

    uint64_t set = 0;
    for (size_t j = 0; j < sizeof(squares); j += sizeof(T)) {
        T input;
        memcpy(&input, &squares[j], sizeof(T));
        set |= lessSet(input, static_cast<uint8_t>(piece)) << j;
    }

    return invert ? ~set : set;
}
uint64_t lessSet(std::array<Piece, 64> squares, Piece piece, bool invert) {
    uint64_t res;
    if constexpr (haveSSE2)
        res = lessSetT<__m128i>(squares, piece, invert);
    else
        res = lessSetT<uint64_t>(squares, piece, invert);

    if constexpr (debug && haveSSE2) {
        auto ref = lessSetT<uint64_t>(squares, piece, invert);
        assert(res == ref);
    }

    return res;
}

SquareSet occupancy(const Board& board) {
    return equalSet(board.squares(), Piece::_, true);
}

SquareSet occupancy(const Board& board, Color color) {
    bool black = color == Color::b;
    return lessSet(board.squares(), black ? Piece::p : Piece::_, black);
}

SquareSet find(const Board& board, Piece piece) {
    return equalSet(board.squares(), piece, false);
}

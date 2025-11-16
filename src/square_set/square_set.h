#pragma once

#include <climits>

#include "core/common.h"

/**
 * Represents a set of squares on a chess board. This class is like std::set<Square>, but
 * uses a bitset represented by a uint64_t to store the squares, which is more efficient.
 */
class SquareSet {
    using T = uint64_t;
    T _squares = 0;
    static_assert(kNumSquares <= sizeof(T) * CHAR_BIT);

public:
    SquareSet(uint64_t squares) : _squares(squares) {}
    SquareSet(Square square) : _squares(1ull << square) {}
    class iterator;

    SquareSet() = default;

    static SquareSet rank(int rank) {
        int shift = rank * kNumFiles;
        uint64_t ret = (1ull << kNumFiles) - 1;
        ret <<= shift;
        return SquareSet(ret);
    }
    static SquareSet file(int file) { return SquareSet(0x0101010101010101ull << file); }

    static SquareSet valid(int rank, int file) {
        return rank >= 0 && rank < kNumRanks && file >= 0 && file < kNumFiles
            ? SquareSet(makeSquare(file, rank))
            : SquareSet();
    }
    static constexpr SquareSet makePath(Square from, Square to) {
        int rankDiff = ::rank(to) - ::rank(from);
        int fileDiff = ::file(to) - ::file(from);

        // If this is not a horizontal, vertical, or diagonal move, return an empty path
        if (rankDiff && fileDiff && abs(rankDiff) != abs(fileDiff)) return {};

        int stepRank = (rankDiff > 0) - (rankDiff < 0);  // 1 if up, -1 if down, 0 if same
        int stepFile = (fileDiff > 0) - (fileDiff < 0);  // 1 if right, -1 if left, 0 if same

        from = step(from, stepFile, stepRank);  // Exclude the starting square from the path
        SquareSet path;
        for (Square sq = from; sq != to; sq = step(sq, stepFile, stepRank)) path.insert(sq);

        return path;
    }

    static SquareSet all() { return SquareSet(0xffff'ffff'ffff'ffffull); }

    void erase(Square square) { _squares &= ~(1ull << square); }
    void insert(Square square) { _squares |= (1ull << square); }
    void insert(SquareSet other) { _squares |= other._squares; }

    void insert(iterator begin, iterator end) {
        for (auto it = begin; it != end; ++it) insert(*it);
    }

    explicit operator bool() const { return _squares; }
    T bits() const { return _squares; }
    bool empty() const { return !_squares; }
    size_t size() const { return std::distance(begin(), end()); }
    bool contains(Square square) const { return bool(*this & SquareSet(square)); }

    SquareSet operator&(SquareSet other) const { return _squares & other._squares; }
    SquareSet operator|(SquareSet other) const { return _squares | other._squares; }
    SquareSet operator^(SquareSet other) const { return _squares ^ other._squares; }
    SquareSet operator~(void) const { return ~_squares; }
    SquareSet operator>>(int shift) const { return _squares >> shift; }
    SquareSet operator<<(int shift) const { return _squares << shift; }
    SquareSet operator-(SquareSet other) const { return _squares & ~other._squares; }

    SquareSet operator|=(SquareSet other) { return _squares |= other._squares; }
    SquareSet operator&=(SquareSet other) { return _squares &= other._squares; }
    SquareSet operator^=(SquareSet other) { return _squares ^= other._squares; }
    SquareSet operator-=(SquareSet other) { return _squares &= ~other._squares; }
    SquareSet operator>>=(int shift) { return _squares >>= shift; }
    SquareSet operator<<=(int shift) { return _squares <<= shift; }

    bool operator==(SquareSet other) const { return _squares == other._squares; }
    bool operator!=(SquareSet other) const { return _squares != other._squares; }

    class iterator {
        friend class SquareSet;
        SquareSet::T _squares;

    public:
        iterator(SquareSet squares) : _squares(squares._squares) {}
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Square;
        using pointer = Square*;
        using reference = Square&;

        iterator operator++() {
            _squares &= _squares - 1;  // Clear the least significant bit
            return *this;
        }
        // Count trailing zeros. TODO: C++20 use std::countr_zero
        value_type operator*() { return Square(__builtin_ctzll(_squares)); }
        bool operator==(const iterator& other) { return _squares == other._squares; }
        bool operator!=(const iterator& other) { return !(_squares == other._squares); }
    };

    iterator begin() const { return {*this}; }
    iterator end() const { return SquareSet(); }
};

/**
 * Returns the set of non-empty fields on the board, possibly limited to one color.
 */
SquareSet occupancy(const Board& board);
SquareSet occupancy(const Board& board, Color color);

SquareSet find(const Board& board, Piece piece);

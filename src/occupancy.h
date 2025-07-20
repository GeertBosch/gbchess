#pragma once

#include "square_set.h"

class Occupancy {
    SquareSet _theirs;
    SquareSet _ours;

    // Private constructor to prevent creation of invalid occupancies
    constexpr Occupancy(SquareSet theirs, SquareSet ours) : _theirs(theirs), _ours(ours) {}

public:
    constexpr Occupancy() = default;
    Occupancy(const Board& board, Color activeColor)
        : Occupancy(occupancy(board, !activeColor), occupancy(board, activeColor)) {}

    // Deltas may have corresponding elements set in both theirs and ourss
    static Occupancy delta(SquareSet theirs, SquareSet ours) { return Occupancy(theirs, ours); }

    SquareSet theirs() const { return _theirs; }
    SquareSet ours() const { return _ours; }

    SquareSet operator()(void) const { return SquareSet{_theirs | _ours}; }
    Occupancy operator!() const { return {_ours, _theirs}; }
    Occupancy operator^(Occupancy other) const {
        return {_theirs ^ other._theirs, _ours ^ other._ours};
    }
    Occupancy operator^=(Occupancy other) {
        _theirs ^= other._theirs;
        _ours ^= other._ours;
        return *this;
    }
    bool operator==(Occupancy other) const {
        return _theirs == other._theirs && _ours == other._ours;
    }
    bool operator!=(Occupancy other) const { return !(*this == other); }
};

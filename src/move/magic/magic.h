#pragma once

#include "core/core.h"
#include "core/square_set/square_set.h"
#include <array>

/**
 * Parallel deposit function that deposits bits from `value` into `result` according to the `mask`.
 * The mask specifies where to deposit bits from the value.
 */
uint64_t parallelDeposit(uint64_t value, uint64_t mask);

/** Table for fast magic bitboard sliding piece attack lookups. */
class MagicEntry {
public:
    MagicEntry(bool bishop, Square square);

    SquareSet targets(SquareSet occupancy) const {
        auto blockers = occupancy & mask;
        return table[(blockers.bits() * magic) >> shift];
    }

private:
    uint64_t magic;
    uint64_t mask;
    int shift;
    std::vector<SquareSet> table;
};

extern std::array<MagicEntry, kNumSquares> rookEntries;
extern std::array<MagicEntry, kNumSquares> bishopEntries;

/** Returns squares attacked by a rook on `square` given board `occupancy`. */
inline SquareSet rookTargets(Square square, SquareSet occupancy) {
    return rookEntries[square].targets(occupancy);
}

/** Returns squares attacked by a bishop on `square` given board `occupancy`. */
inline SquareSet bishopTargets(Square square, SquareSet occupancy) {
    return bishopEntries[square].targets(occupancy);
}

/**
 * Returns the set of squares where a piece can restrict the movement of the sliding piece. Note
 * that this will not include the final square in each direction of motion, as that piece may be
 * captured by the sliding piece, if it's an enemy piece, and does not restrict its motion.
 */
SquareSet computeSliderBlockers(Square square, bool bishop);

SquareSet computeSliderTargets(Square square, bool bishop, SquareSet blockers);

#pragma once

#include "core/core.h"
#include "core/square_set/square_set.h"

/**
 * Parallel deposit function that deposits bits from `value` into `result` according to the `mask`.
 * The mask specifies where to deposit bits from the value.
 */
uint64_t parallelDeposit(uint64_t value, uint64_t mask);

/**
 * Returns a set of pseudo-legal target squares for the given piece on the given square, assuming
 * the provided occupancy.
 */
SquareSet targets(Square square, bool bishop, SquareSet occupancy);

/**
 * Returns the set of squares where a piece can restrict the movement of the sliding piece. Note
 * that this will not include the final square in each direction of motion, as that piece may be
 * captured by the sliding piece, if it's an enemy piece, and does not restrict its motion.
 */
SquareSet computeSliderBlockers(Square square, bool bishop);

SquareSet computeSliderTargets(Square square, bool bishop, SquareSet blockers);

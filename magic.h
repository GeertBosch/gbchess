#pragma once

#include "common.h"

/**
 *   Counts the number of set bits (population count) in a 64-bit integer.
 */
constexpr int pop_count(uint64_t b) {
    return __builtin_popcountll(b);
}

/**
 * Parallel deposit function that deposits bits from `value` into `result` according to the `mask`.
 * The mask specifies where to deposit bits from the value.
 */
uint64_t parallelDeposit(uint64_t value, uint64_t mask);

/**
 * Returns a set of pseudo-legal target squares for the given piece on the given square, assuming
 * the provided occupancy.
 */
uint64_t targets(uint8_t square, bool bishop, uint64_t occupancy);

/**
 * Returns the set of squares where an enemy piece can restrict the movement of the sliding piece.
 * Note that this will not include the final square in each direction of motion, as that piece
 * may be captured by the sliding piece and does not restrict its motion.
 */
uint64_t computeSliderBlockers(uint8_t square, bool bishop);

uint64_t computeSliderTargets(uint8_t square, bool bishop, uint64_t blockers);

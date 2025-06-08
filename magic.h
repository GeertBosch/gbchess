#include "common.h"

/**
 * Returns a bitboard of pseudo-legal target squares for the given piece on the given square,
 * assuming the provided occupancy.
 */
uint64_t targets(uint8_t square, bool bishop, uint64_t occupancy);

/**
 *   Counts the number of set bits (population count) in a 64-bit integer.
 */
constexpr int pop_count(uint64_t b) {
    return __builtin_popcountll(b);
}

/**
 *   Converts an index to a set of blockers by mapping the bits of the index to the positions of set
 *   bits in the mask m.
 */
inline uint64_t indexToBlockers(int index, uint64_t mask) {
    uint64_t result = 0ull;
    int bits = pop_count(mask);
    for (int i = 0; i < bits; i++) {
        auto bit = mask & -mask;  // Isolate the least significant set bit in mask
        mask -= bit;              // Remove it from mask
        if (index & (1 << i)) result |= bit;
    }
    return result;
}
/**
 *   Computes the magic index for a given bitboard, magic number, and number of bits.
 */
inline size_t magicTableIndex(uint64_t blocked, uint64_t magic, int bits) {
    return (blocked * magic) >> (64 - bits);
}

/**
 * Returns the set of squares where an enemy piece can restrict the movement of the sliding piece.
 * Note that this will not include the final square in each direction of motion, as that piece
 * may be captured by the sliding piece and does not restrict its motion.
 */
uint64_t computeSliderBlockers(uint8_t square, bool bishop);

uint64_t computeSliderTargets(uint8_t square, bool bishop, uint64_t blockers);

struct Magic {
    uint64_t magic;               // Magic number for the piece type
    uint64_t mask;                // Bitmask for the piece type
    std::vector<uint64_t> table;  // Precomputed attack tables for the piece type
    int bits;                     // Number of bits used for indexing into the table
    Magic(uint8_t square, bool bishop, uint64_t magic)
        : magic(magic), mask(computeSliderBlockers(square, bishop)), bits(pop_count(mask)) {
        table.resize(1ull << bits, 0ull);
        uint64_t a[1 << 12];
        for (int i = 0; i < (1 << pop_count(mask)); i++)
            a[i] = computeSliderTargets(square, bishop, indexToBlockers(i, mask));
        for (int i = 0; i < (1 << bits); i++) {
            auto& entry = table[magicTableIndex(indexToBlockers(i, mask), magic, bits)];
            if (!entry)
                entry = a[i];
            else if (entry != a[i])
                throw "collision found, incorrect magic";
        }
    }
    // Computes the attack bitboard for this square and piece type, given the board occupancy.
    uint64_t targets(uint64_t occupancy) const {
        uint64_t blockers = occupancy & mask;
        size_t index = magicTableIndex(blockers, magic, bits);
        return table[index];
    }
};

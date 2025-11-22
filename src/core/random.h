#pragma once
#include <cstdint>
/**
 * The xorshift PRNG provides a balance between quality and performance. Poor quality would lead to
 * longer search times for magic generation, or in the worst case an unsuccessful search.
 * See Marsaglia, George. (2003). Xorshift RNGs. Journal of Statistical Software.
 * 08. 10.18637/jss.v008.i14. https://www.researchgate.net/publication/5142825_Xorshift_RNGs
 */
struct xorshift {
    xorshift(uint64_t seed = 0xc1f651c67c62c6e0ull) : state(seed) {}
    uint64_t operator()() {
        auto result = state * 0xd989bcacc137dcd5ull;
        state ^= state >> 11;
        state ^= state << 31;
        state ^= state >> 18;
        return result;
    }
    uint64_t state;
};

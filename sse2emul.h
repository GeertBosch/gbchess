/**
 *  Implement our own SSE2 functions for non-x86 systems
 *
 *  The compiler should compile most to native SIMD instructions, where available.
 *  Results on my Apple Silicon M1 exceed current on my x86 using native SSE2.
 */

#include <cstring>

#pragma once

typedef __attribute__((__vector_size__(2 * sizeof(long long)))) long long __m128i;

inline __m128i _mm_cmpeq_epi8(__m128i x, __m128i y) {
    __m128i z;
    char xm[16];
    char ym[16];
    char zm[16];

    memcpy(xm, &x, sizeof(x));
    memcpy(ym, &y, sizeof(y));

    for (size_t i = 0; i < sizeof(xm); ++i) zm[i] = xm[i] == ym[i] ? 0xff : 0;

    memcpy(&z, zm, sizeof(z));
    return z;
}
inline __m128i _mm_cmplt_epi8(__m128i x, __m128i y) {
    __m128i z;
    char xm[16];
    char ym[16];
    char zm[16];

    memcpy(xm, &x, sizeof(x));
    memcpy(ym, &y, sizeof(y));

    for (size_t i = 0; i < sizeof(xm); ++i) zm[i] = xm[i] < ym[i] ? 0xff : 0;

    memcpy(&z, zm, sizeof(z));
    return z;
}

inline __m128i _mm_srli_epi64(__m128i x, int count) {
    auto srl = [](long long x, int count) -> auto {
        return static_cast<long long>(static_cast<unsigned long long>(x) >> count);
    };
    return __m128i{srl(x[0], count), srl(x[1], count)};
}

inline int _mm_movemask_epi8(__m128i x) {
    auto c1 = static_cast<long long>(0x8040201008040201ull);
    x = ~_mm_cmpeq_epi8(x, __m128i{0, 0});
    x &= __m128i{c1, c1};
    x |= _mm_srli_epi64(x, 8);
    x |= _mm_srli_epi64(x, 16);
    x |= _mm_srli_epi64(x, 32);
    return static_cast<unsigned char>(x[0]) + (static_cast<unsigned char>(x[1]) << 8);
}

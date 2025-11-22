#pragma once
// Use native SSE2, or emulation if SSE2 is not available or desired for testing purposes
#if defined(__SSE2__) && !defined(SSE2EMUL)
#include <emmintrin.h>
#else
#include "core/sse2emul.h"
#endif

// Unconditionally use actual SSE2 or emulated SSE2: it turns out that the emulated CPU has more
// benefits for ARM (on at least Apple Silicon M1) than real SSE2 has for x86.
constexpr bool haveSSE2 = true;

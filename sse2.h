
// Use native SSE2 or emulation of SSE2 is not available or desired for testing purposes
#if defined(__SSE2__) || defined(DEBUG) || defined(NOSSE2)

#include <emmintrin.h>
constexpr bool haveSSE2 = true;
#else
#include "sse2emul.h"
constexpr bool haveSSE2 = false;
#endif

// Use native SSE2, or emulation if SSE2 is not available or desired for testing purposes
#if defined(__SSE2__) && !defined(NOSSE2) && !defined(SSE2EMUL)

#include <emmintrin.h>
constexpr bool haveSSE2 = true;

#else

#include "sse2emul.h"
#if SSE2EMUL
constexpr bool haveSSE2 = true;
#else
constexpr bool haveSSE2 = false;
#endif

#endif

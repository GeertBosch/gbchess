
// Use native SSE2, or emulation if SSE2 is not available or desired for testing purposes
#if defined(__SSE2__) && !defined(SSE2EMUL)
#include <emmintrin.h>
#else
#include "sse2emul.h"
#endif

#pragma once

#ifdef NDEBUG
#undef NDEBUG
#define dbg_assert(...)
#else
#define DEBUG_IMAGE_VERSION
#define dbg_assert assert
#endif

#define RESTRICT __restrict__

#define CEIL_DIV(a, b) (((a) + (b) -1) / (b))

#define UNUSED(x) ((void) (x))

#define INDEX_2D(x, y, stride) ((y) * (stride) + (x))

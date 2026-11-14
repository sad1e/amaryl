#pragma once

#include "amaryl/core/types.h"

// clang-format off

#if defined(__clang__)
  #define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
  #define FORCE_INLINE __forceinline
#else
  #define FORCE_INLINE inline
#endif

#define AMARYL_MATH_OVERRIDE_NEW_DELETE

inline uint32 rotl32(uint32 x, int8 r) { return (x << r) | (x >> (32 - r)); }
inline uint64 rotl64(uint64 x, int8 r) { return (x << r) | (x >> (64 - r)); }

#define ROTL32(x, y)    rotl32(x, y)
#define ROTL64(x, y)    rotl64(x, y)

#define BIG_CONSTANT(x) (x## LLU)

// clang-format on

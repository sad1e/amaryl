#pragma once

#include "amaryl/core/macros.h"

namespace amaryl {

//> s align up with mask v to keep granularity.
template <typename T>
constexpr inline T Align(T s, T a) {
  return (s + a - 1) & ~(a - 1);
}

template <typename T>
inline T AlignUpWithMask(T v, size_t mask) {
  return (T)(((size_t)v + mask) & ~mask);
}

template <typename T>
inline T AlignDownWithMask(T v, size_t mask) {
  return (T)((size_t)v & ~mask);
}

template <typename T>
inline T AlignUp(T v, size_t alignment) {
  return AlignUpWithMask(v, alignment - 1);
}

template <typename T>
inline T AlignDown(T v, size_t alignment) {
  return AlignDownWithMask(v, alignment - 1);
}

template <typename T>
inline bool IsAligned(T v, size_t alignment) {
  return ((size_t)v & (alignment - 1)) == 0;
}

template <typename T>
inline bool IsPowerOfTwo(T v) {
  return (v & (v - 1)) == 0;
}

}  // namespace amaryl

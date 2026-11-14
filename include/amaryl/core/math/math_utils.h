#pragma once

#include <algorithm>
#include <bit>
#include <cmath>
#include <type_traits>

#include "amaryl/core/math/math_types.h"

namespace amaryl {

template <typename T>
constexpr T Abs(T v) {
  return std::abs(v);
}

template <typename T>
constexpr T Min(T a, T b) {
  return (std::min)(a, b);
}

template <typename T>
constexpr T Max(T a, T b) {
  return (std::max)(a, b);
}

template <typename T>
constexpr T Clamp(T v, T min, T max) {
  return Min(max, Max(min, v));
}

template <typename T>
constexpr T Square(T v) {
  return v * v;
}

template <typename T>
constexpr T Cube(T v) {
  return v * v * v;
}

/// Compute number of trailing zero bits (how many low bits are zero)
inline uint32 CountTrailingZeros(uint32 x) { return std::countr_zero(x); }

/// Compute the number of leading zero bits (how many high bits are zero)
inline uint32 CountLeadingZeros(uint32 x) { return std::countl_zero(x); }

/// Count the number of 1 bits in a value
inline uint32 CountBits(uint32 x) { return std::popcount(x); }

/// Get the next higher power of 2 of a value, or the value itself if the value
/// is already a power of 2
inline uint32 GetNextPowerOfTwo(uint32 x) { return std::bit_ceil(x); }

template <class To, class From>
FORCE_INLINE To BitCast(const From& x) {
  return std::bit_cast<To>(x);
}

}  // namespace amaryl

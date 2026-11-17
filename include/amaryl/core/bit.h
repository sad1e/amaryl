#pragma once

#include <bit>

#include "amaryl/core/macros.h"

namespace amaryl {

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

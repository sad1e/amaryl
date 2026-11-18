#pragma once

#include <bit>

#include "amaryl/core/macros.h"

namespace amaryl {

/// Compute number of trailing zero bits (how many low bits are zero)
inline uint32 count_trail_zeros(uint32 x) { return std::countr_zero(x); }

/// Compute the number of leading zero bits (how many high bits are zero)
inline uint32 count_lead_zeros(uint32 x) { return std::countl_zero(x); }

/// Count the number of 1 bits in a value
inline uint32 count_bits(uint32 x) { return std::popcount(x); }

/// Get the next higher power of 2 of a value, or the value itself if the value
/// is already a power of 2
inline uint32 get_next_power_of_2(uint32 x) { return std::bit_ceil(x); }

template <class To, class From>
FORCE_INLINE To bit_cast(const From& x) {
  return std::bit_cast<To>(x);
}

}  // namespace amaryl

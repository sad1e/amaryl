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

}  // namespace amaryl

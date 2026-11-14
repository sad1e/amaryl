#pragma once

#include <initializer_list>
#include <type_traits>

#include "amaryl/core/hash_combine.h"
#include "amaryl/core/macros.h"

namespace amaryl {

template <typename T, std::size_t N>
struct VecN {
  AMARYL_MATH_OVERRIDE_NEW_DELETE;

  static_assert(std::is_arithmetic_v<T>, "Vec<T, N>: T must be arithmetic");

  VecN() = default;
  VecN(const VecN& InRHS) = default;
  VecN& operator=(const VecN& InRHS) = default;
  explicit VecN(T v) {
    for (std::size_t i = 0; i < N; ++i) {
      Data[i] = v;
    }
  }

  template <typename... Args, typename = std::enable_if_t<sizeof...(Args) == N>>
  explicit constexpr VecN(Args... args) : Data{static_cast<T>(args)...} {}

  //> Don't use initializer_list, I don't like to check it at runtime.
  constexpr VecN(std::initializer_list<T> InData) {
    std::size_t i = 0;
    for (auto Value : InData) {
      if (i < N) Data[i++] = Value;
    }
  }

  // clang-format off
  constexpr T& X() requires (N >= 1) { return Data[0]; }
  constexpr T& Y() requires (N >= 2) { return Data[1]; }
  constexpr T& Z() requires (N >= 3) { return Data[2]; }
  constexpr T& W() requires (N >= 4) { return Data[3]; }

  constexpr const T& X() const requires (N >= 1) { return Data[0]; }
  constexpr const T& Y() const requires (N >= 2) { return Data[1]; }
  constexpr const T& Z() const requires (N >= 3) { return Data[2]; }
  constexpr const T& W() const requires (N >= 4) { return Data[3]; }

  constexpr T& operator[](std::size_t i) { return Data[i]; }
  constexpr const T& operator[](std::size_t i) const { return Data[i]; }
  // clang-format on

  constexpr bool operator==(const VecN& InRHS) const {
    for (std::size_t i = 0; i < N; i++) {
      if (Data[i] != InRHS.Data[i]) return false;
    }
    return true;
  }

  constexpr bool operator!=(const VecN& InRHS) const { return !(*this == InRHS); }

  static VecN sZero() { return VecN(T(0)); }

  T Data[N];
};

using int2 = VecN<int, 2>;
using int3 = VecN<int, 3>;
using int4 = VecN<int, 4>;
using uint2 = VecN<uint32, 2>;
using uint3 = VecN<uint32, 3>;
using uint4 = VecN<uint32, 4>;
using float2 = VecN<float, 2>;
using float3 = VecN<float, 3>;
using float4 = VecN<float, 4>;

}  // namespace amaryl

AMARYL_MAKE_HASHABLE(amaryl::float3, t.X(), t.Y(), t.Z())

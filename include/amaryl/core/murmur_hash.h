#pragma once

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "amaryl/core/config.h"
#include "amaryl/core/macros.h"
#include "amaryl/core/types.h"

namespace amaryl {

// Hash function for std::optional - hashes the value if present, or a sentinel
// if not.
template <typename H, typename T>
H HashValue(H h, const std::optional<T>& value) {
  if (value.has_value()) {
    return H::Combine(H::Combine(std::move(h), true), *value);
  }
  return H::Combine(std::move(h), false, 0);
}

// Hash function for std::pair - combines hashes of both elements.
template <typename H, typename T1, typename T2>
H HashValue(H h, const std::pair<T1, T2>& value) {
  return H::Combine(std::move(h), value.first, value.second);
}

// Hash function for std::tuple - combines hashes of all elements.
template <typename H, typename... Ts>
H HashValue(H h, const std::tuple<Ts...>& value) {
  return std::apply(
      [&h](const auto&... elements) { return H::Combine(std::move(h), elements...); },
      value);
}

// Hash function for pointers - hashes the pointer value as an integer.
template <typename H, typename T>
H HashValue(H h, const std::unique_ptr<T>& ptr) {
  return H::Combine(std::move(h), ptr.get());
}

template <typename H, typename T>
H HashValue(H h, const std::shared_ptr<T>& ptr) {
  return H::Combine(std::move(h), ptr.get());
}

// This is for using already-hashed key into std::unordered_map and avoid the
// cost of re-hashing. Example:
// unordered_map<uint64_t, Value, AlreadyHashed> my_map.
template <typename T>
struct AlreadyHashed {
  size_t operator()(const T& x) const { return static_cast<size_t>(x); }
};

namespace hash_internal {

// clang-format off
FORCE_INLINE uint32 fmix32(uint32 h) {
  h ^= h >> 16; h *= 0x85ebca6b;
  h ^= h >> 13; h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

FORCE_INLINE uint64 fmix64(uint64 k) {
  k ^= k >> 33; k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33; k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;
  return k;
}
// clang-format on

template <typename TFloat, typename TInt>
TInt NormalizeFloatToInt(TFloat value) {
  static_assert(std::is_floating_point_v<TFloat>);
  static_assert(std::is_integral_v<TInt>);

  if (value == 0.0) {
    value = 0.0;
  } else if (std::isnan(value)) {
    value = std::numeric_limits<TFloat>::quiet_NaN();
  }

  TInt res;
  static_assert(sizeof(TFloat) == sizeof(TInt));
  memcpy(&res, &value, sizeof(TFloat));
  return res;
}

CORE_API uint64 MurmurHash3(const void* data, uint64 len);

template <typename T>
auto MurmurHashBuiltinValue(const T& value) {
  if constexpr (std::is_enum_v<T>) {
    return hash_internal::fmix64(
        static_cast<uint64>(static_cast<std::underlying_type_t<T>>(value)));
  } else if constexpr (std::is_integral_v<T>) {
    return hash_internal::fmix64(static_cast<uint64>(value));
  } else if constexpr (std::is_same_v<T, double>) {
    return hash_internal::fmix64(
        hash_internal::NormalizeFloatToInt<double, uint64>(value));
  } else if constexpr (std::is_same_v<T, float>) {
    return hash_internal::fmix64(
        hash_internal::NormalizeFloatToInt<float, uint32>(value));
  } else if constexpr (std::is_same_v<T, std::string> ||
                       std::is_same_v<T, std::string_view>) {
    return hash_internal::MurmurHash3(value.data(), value.size());
  } else if constexpr (std::is_same_v<T, const char*>) {
    std::string_view view(value);
    return hash_internal::MurmurHash3(view.data(), view.size());
  } else if constexpr (std::is_pointer_v<T>) {
    return hash_internal::fmix64(static_cast<uint64>(reinterpret_cast<uintptr_t>(value)));
  } else {
    //> This type is help for check.
    struct InvalidBulitin {};
    return InvalidBulitin{};
  }
}

template <typename T>
constexpr bool HasMurmurHashBuiltinValue() {
  return std::is_same_v<decltype(MurmurHashBuiltinValue(std::declval<T>())), uint64>;
}

template <typename T, typename U>
constexpr bool IsConvertibleIntegral() {
  return std::is_integral_v<T> && std::is_integral_v<U> && std::is_convertible_v<U, T>;
}

template <typename T>
constexpr bool IsStringLike() {
  return std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
         std::is_same_v<T, const char*>;
}

template <typename T, typename U>
constexpr bool AllowHeterogeneousLookup() {
  return IsConvertibleIntegral<T, U>() || (IsStringLike<T>() && IsStringLike<U>());
}

}  // namespace hash_internal

/**
 *
 */
struct MurmurHashCombiner {
  MurmurHashCombiner() = default;

  template <typename... Args>
  static MurmurHashCombiner Combine(MurmurHashCombiner h, const Args&... args) {
    h.Combine(args...);
    return h;
  }

  template <typename... Args>
  void Combine(const Args&... args) {
    (CombineOne(args), ...);
  }

  CORE_API uint64 Digest() const { return hash_; }

 private:
  template <typename T>
  void CombineOne(const T& value) {
    if constexpr (hash_internal::HasMurmurHashBuiltinValue<T>()) {
      Update(hash_internal::MurmurHashBuiltinValue(value));
    } else {
      hash_ = HashValue(std::move(*this), value).Digest();
    }
  }

  void Update(uint64 piece_hash) {
    hash_ ^= piece_hash + 0x9e3779b9 + (hash_ << 6) + (hash_ >> 2);
  }

  static constexpr uint64 kSeed = 0xe17a1465u;
  uint64 hash_;
};

// Simple wrapper function around MurmurHashCombiner to improve clarity in
// callsites to not have to instantiate the class, call Combine() then digest().
template <typename... Args>
uint64 MurmurHashCombine(const Args&... args) {
  return MurmurHashCombiner::Combine(MurmurHashCombiner(), args...).Digest();
}

// Simple wrapper function to compute a hash value for a single value.
// This is the primitive hash operation that MurmurHash<T> delegates to.
//
// For built-in types (integers, floats, strings), this uses a fast path that
// avoids the overhead of the MurmurHashCombiner. For custom types, it delegates
// to MurmurHashCombiner which will use ADL to find the PerfettoHashValue.
template <typename T>
uint64 MurmurHashValue(const T& value) {
  if constexpr (hash_internal::HasMurmurHashBuiltinValue<T>()) {
    return hash_internal::MurmurHashBuiltinValue(value);
  } else {
    return MurmurHashCombine(value);
  }
}

template <typename T>
struct MurmurHash {
  using is_transparent = void;

  uint64 operator()(const T& value) { return MurmurHashValue(value); }

  template <typename U>
  auto operator()(const U& value) const
      -> std::enable_if_t<hash_internal::AllowHeterogeneousLookup<T, U>(), uint64> {
    return MurmurHashValue(value);
  }
};

}  // namespace amaryl

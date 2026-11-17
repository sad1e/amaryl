#pragma once

#include "amaryl/core/types.h"

// clang-format off

#if defined(__clang__)
  #define FORCE_INLINE  inline __attribute__((always_inline))
#elif defined(_MSC_VER)
  #define FORCE_INLINE  __forceinline
#else
  #define FORCE_INLINE  inline
#endif

#define AMARYL_MATH_OVERRIDE_NEW_DELETE

inline uint32 rotl32(uint32 x, int8 r) { return (x << r) | (x >> (32 - r)); }
inline uint64 rotl64(uint64 x, int8 r) { return (x << r) | (x >> (64 - r)); }

#define ROTL32(x, y)    rotl32(x, y)
#define ROTL64(x, y)    rotl64(x, y)

#define UNREF_PARAM(x)  ((void)(x))

#define BIG_CONSTANT(x) (x## LLU)

#ifndef MAX
  #define MAX(a, b)     (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
  #define MIN(a, b)     (((a) < (b)) ? (a) : (b))
#endif

// #define ARRAY_COUNT(arr) \
//   (sizeof(arr) / sizeof(arr[0]) * (sizeof(arr) != PTR_SIZE || sizeof(arr[0]) <= PTR_SIZE))

// std::size cannot accept pointer type
#define ARRAY_COUNT(x) std::size(x)

#define BIT(x)          (1 << x)
#define KILOBYTES       (1024)
#define MEGABYTES       (1024 * KILOBYTES)
#define GIGABYTES       (1024 * MEGABYTES)

#define NO_COPYABLE(T)  \
  T(const T&) = delete; \
  T& operator=(const T&) = delete

#define NO_MOVEABLE(T) \
  T(T&&) = delete;     \
  T& operator=(T&&) = delete

#define NO_COPYABLE_AND_MOVEABLE(T) \
  NO_COPYABLE(T);                   \
  NO_MOVEABLE(T)

#define MAKE_ENUM_FLAGS(Enum)                                            \
  inline Enum& operator|=(Enum& Lhs, Enum Rhs) {                         \
    return Lhs = (Enum)(static_cast<std::underlying_type_t<Enum>>(Lhs) | \
                        static_cast<std::underlying_type_t<Enum>>(Rhs)); \
  }                                                                      \
  inline Enum& operator&=(Enum& Lhs, Enum Rhs) {                         \
    return Lhs = (Enum)(static_cast<std::underlying_type_t<Enum>>(Lhs) & \
                        static_cast<std::underlying_type_t<Enum>>(Rhs)); \
  }                                                                      \
  inline Enum& operator^=(Enum& Lhs, Enum Rhs) {                         \
    return Lhs = (Enum)(static_cast<std::underlying_type_t<Enum>>(Lhs) ^ \
                        static_cast<std::underlying_type_t<Enum>>(Rhs)); \
  }                                                                      \
  inline constexpr Enum operator|(Enum Lhs, Enum Rhs) {                  \
    return (Enum)(static_cast<std::underlying_type_t<Enum>>(Lhs) |       \
                  static_cast<std::underlying_type_t<Enum>>(Rhs));       \
  }                                                                      \
  inline constexpr Enum operator&(Enum Lhs, Enum Rhs) {                  \
    return (Enum)(static_cast<std::underlying_type_t<Enum>>(Lhs) &       \
                  static_cast<std::underlying_type_t<Enum>>(Rhs));       \
  }                                                                      \
  inline constexpr Enum operator^(Enum Lhs, Enum Rhs) {                  \
    return (Enum)(static_cast<std::underlying_type_t<Enum>>(Lhs) ^       \
                  static_cast<std::underlying_type_t<Enum>>(Rhs));       \
  }                                                                      \
  inline constexpr bool operator!(Enum E) {                              \
    return !(static_cast<std::underlying_type_t<Enum>>(E));              \
  }                                                                      \
  inline constexpr Enum operator~(Enum E) {                              \
    return (Enum) ~(static_cast<std::underlying_type_t<Enum>>(E));       \
  }

template <typename FlagsType, typename Enum>
constexpr bool
EnumHasAllFlags(FlagsType Flags, Enum Contains) {
  static_assert(std::is_enum_v<Enum>);
  using TUnderlying = std::underlying_type_t<Enum>;
  const TUnderlying F = static_cast<TUnderlying>(Flags);
  const TUnderlying C = static_cast<TUnderlying>(Contains);
  return (F & C) == C;
}

template <typename FlagsType, typename Enum>
constexpr bool
EnumHasAnyFlags(FlagsType Flags, Enum Contains) {
  static_assert(std::is_enum_v<Enum>);
  using TUnderlying = std::underlying_type_t<Enum>;
  const TUnderlying F = static_cast<TUnderlying>(Flags);
  const TUnderlying C = static_cast<TUnderlying>(Contains);
  return (F & C) != 0;
}

template <typename Enum>
void
EnumAddFlags(Enum& Flags, Enum FlagsToAdd) {
  Flags |= FlagsToAdd;
}

template <typename Enum>
void
EnumRemoveFlags(Enum& Flags, Enum FlagsToRemove) {
  Flags &= ~FlagsToRemove;
}

//> Check sdl error.
#define CHECK_SDL(expr)                                                        \
  do {                                                                         \
    if (!expr) {                                                               \
      fprintf(                                                                 \
        stderr, "Runtime SDL error: %s at %s:%d.\n", SDL_GetError(), __FILE__, \
        __LINE__);                                                             \
    }                                                                          \
  } while (0)


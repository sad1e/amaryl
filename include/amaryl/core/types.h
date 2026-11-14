#pragma once

#include <wchar.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "amaryl/core/config.h"

// clang-format off
#if INTPTR_MAX == 0x7FFFFFFFFFFFFFFFLL
  #define PTR_SIZE 8
#elif INTPTR_MAX == 0x7FFFFFFF
  #define PTR_SIZE 4
#else
  #error unsupported platform
#endif

#ifndef VOIDPTR
  #define VOIDPTR void *
#endif
// clang-format on

// clang-format off

using f32 = float;
using f64 = double;
using byte = unsigned char;

using uint8  = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using int8   = std::int8_t;
using int16  = std::int16_t;
using int32  = std::int32_t;
using int64  = std::int64_t;

static_assert(sizeof(ptrdiff_t) == sizeof(size_t), "ptrdiff_t and size_t has different size, weird.");

constexpr static uint16_t kInvalidHandle = UINT16_MAX;

// clang-format on

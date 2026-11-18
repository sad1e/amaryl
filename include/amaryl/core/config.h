#pragma once

#include "amaryl/core/core_export.h"

// clang-format off
// Compile-target features only; these macros do not probe the running CPU.

#if defined(_WIN32)
  #define PLATFORM_WINDOWS 1
#else
  #define PLATFORM_WINDOWS 0
#endif

#if defined(__ANDROID__)
  #define PLATFORM_ANDROID 1
#else
  #define PLATFORM_ANDROID 0
#endif

#if defined(__APPLE__)
  #include <TargetConditionals.h>
  #if TARGET_OS_IOS
    #define PLATFORM_IOS 1
  #else
    #define PLATFORM_IOS 0
  #endif
#endif

#if defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
  #define AMARYL_ARCH_ARM64 1
#else
  #define AMARYL_ARCH_ARM64 0
#endif

// ARM64EC uses x64-compatible predefines but is not an x64 instruction target.
#if !defined(_M_ARM64EC) && (defined(__x86_64__) || defined(_M_X64))
  #define AMARYL_ARCH_X86 1
#else
  #define AMARYL_ARCH_X86 0
#endif

#if !AMARYL_ARCH_ARM64 && !AMARYL_ARCH_X86
#error "Only 64-bit ARM64 and x64 compilation targets are supported"
#endif


#define DEFAULT_ALIGNMENT 16
#define VECTOR_ALIGNMENT  16
#define DVECTOR_ALIGNMENT 32

#define AMARYL_SSE_NONE 0
#define AMARYL_SSE      10
#define AMARYL_SSE2     20
#define AMARYL_SSE3     30
#define AMARYL_SSSE3    31 // Supplemental SSE3
#define AMARYL_SSE4_1   41
#define AMARYL_SSE4_2   42

#if AMARYL_ARCH_X86 && defined(__SSE4_2__)
  #define AMARYL_SSE_LEVEL AMARYL_SSE4_2
#elif AMARYL_ARCH_X86 && defined(__SSE4_1__)
  #define AMARYL_SSE_LEVEL AMARYL_SSE4_1
#elif AMARYL_ARCH_X86 && defined(__SSSE3__)
  #define AMARYL_SSE_LEVEL AMARYL_SSSE3
#elif AMARYL_ARCH_X86 && defined(__SSE3__)
  #define AMARYL_SSE_LEVEL AMARYL_SSE3
#elif AMARYL_ARCH_X86 && (defined(__SSE2__) || defined(_M_X64))
  #define AMARYL_SSE_LEVEL AMARYL_SSE2
#elif AMARYL_ARCH_X86 && defined(__SSE__)
  #define AMARYL_SSE_LEVEL AMARYL_SSE
#else
  #define AMARYL_SSE_LEVEL AMARYL_SSE_NONE
#endif

#if AMARYL_SSE_LEVEL >= AMARYL_SSE
  #define AMARYL_USE_SSE    1
#else
  #define AMARYL_USE_SSE    0
#endif

#if AMARYL_SSE_LEVEL >= AMARYL_SSE4_1
  #define AMARYL_USE_SSE4_1 1
#else
  #define AMARYL_USE_SSE4_1 0
#endif

#if AMARYL_SSE_LEVEL >= AMARYL_SSE4_2
  #define AMARYL_USE_SSE4_2 1
#else
  #define AMARYL_USE_SSE4_2 0
#endif

#if AMARYL_ARCH_ARM64 && (defined(__ARM_NEON) || defined(__ARM_NEON__) || \
  defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC))
  #define AMARYL_SIMD_NEON 1
#else
  #define AMARYL_SIMD_NEON 0
#endif

#if AMARYL_ARCH_X86 && defined(__AVX__)
  #define AMARYL_SIMD_AVX 1
#else
  #define AMARYL_SIMD_AVX 0
#endif

#if AMARYL_ARCH_X86 && defined(__AVX2__)
  #define AMARYL_SIMD_AVX2 1
#else
  #define AMARYL_SIMD_AVX2 0
#endif

#if defined(DEBUG) || defined(_DEBUG)
  #define USE_DEBUG  1
#endif

#ifdef USE_DEBUG
  #define ENABLE_MEMORY_TRACKING
  #define WITH_TRACY
#endif

// clang-format on

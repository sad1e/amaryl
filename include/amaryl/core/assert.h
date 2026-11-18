#pragma once

#include <cstdarg>
#include <cstdio>

#include "amaryl/core/macros.h"
#include "amaryl/core/platform.h"

#define OUTPUT_DEBUG_STRING 1

// clang-format off
#ifdef USE_DEBUG
  #define DEBUG_OP(x) x
#else
  #define DEBUG_OP(x) ((void)0)
#endif
// clang-format on

namespace amaryl {

inline void Print(const char* msg) {
#if OUTPUT_DEBUG_STRING
  OutputDebugStringA(msg);
#endif
  printf("%s", msg);
}

inline void Print(const wchar_t* msg) {
#if OUTPUT_DEBUG_STRING
  OutputDebugStringW(msg);
#endif
  wprintf(L"%ws", msg);
}

inline void Printf(const char* format, ...) {
  char buffer[256];
  va_list ap;
  va_start(ap, format);
  vsprintf_s(buffer, 256, format, ap);
  va_end(ap);
  Print(buffer);
}

inline void Printf(const wchar_t* format, ...) {
  wchar_t buffer[256];
  va_list ap;
  va_start(ap, format);
  vswprintf(buffer, 256, format, ap);
  va_end(ap);
  Print(buffer);
}

#ifndef RELEASE
inline void PrintSubMessage(const char* format, ...) {
  Print("--> ");
  char buffer[256];
  va_list ap;
  va_start(ap, format);
  vsprintf_s(buffer, 256, format, ap);
  va_end(ap);
  Print(buffer);
  Print("\n");
}
inline void PrintSubMessage(const wchar_t* format, ...) {
  Print("--> ");
  wchar_t buffer[256];
  va_list ap;
  va_start(ap, format);
  vswprintf(buffer, 256, format, ap);
  va_end(ap);
  Print(buffer);
  Print("\n");
}
inline void PrintSubMessage(void) {}
#endif

CORE_API void OutputDebugStringFormat(const wchar_t* format, ...);

CORE_API void PrintStack();

}  // namespace amaryl

#ifdef USE_DEBUG
#define OUTPUT_DEBUG_STRING_FMT(format, ...)                              \
  amaryl::OutputDebugStringFormat(TEXT("FUNC: %s()  MSG: ") TEXT(format), \
                                  TEXT(__FUNCTION__), ##__VA_ARGS__)
#else
#define OUTPUT_DEBUG_STRING_FMT(format, ...)
#endif

#ifdef ERROR
#undef ERROR
#endif

#ifdef ASSERT
#undef ASSERT
#endif

#ifdef HALT
#undef HALT
#endif

#define PLATFORM_BREAK() (__nop(), __debugbreak())

#ifdef RELEASE

#define ASSERT(isTrue, ...) (void)(isTrue)
#define WARN_ONCE_IF(isTrue, ...) (void)(isTrue)
#define WARN_ONCE_IF_NOT(isTrue, ...) (void)(isTrue)
#define DEBUGPRINT(msg, ...) \
  do {                       \
  } while (0)
#define ASSERT_SUCCEEDED(hr, ...) (void)(hr)

#else  // !RELEASE

#define STRINGIFY(x) #x
#define STRINGIFY_BUILTIN(x) STRINGIFY(x)
#define ASSERT(isFalse, ...)                                  \
  if (!(bool)(isFalse)) {                                     \
    amaryl::Print("\nAssertion failed in " STRINGIFY_BUILTIN( \
        __FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n");    \
    amaryl::PrintSubMessage("\'" #isFalse "\' is false");     \
    amaryl::PrintSubMessage(__VA_ARGS__);                     \
    amaryl::Print("\n");                                      \
    amaryl::PrintStack();                                     \
    PLATFORM_BREAK();                                         \
  }

#define ASSERT_SUCCEEDED(hr, ...)                           \
  if (FAILED(hr)) {                                         \
    amaryl::Print("\nHRESULT failed in " STRINGIFY_BUILTIN( \
        __FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n");  \
    amaryl::PrintSubMessage("hr = 0x%08X", hr);             \
    amaryl::PrintSubMessage(__VA_ARGS__);                   \
    amaryl::Print("\n");                                    \
    PLATFORM_BREAK();                                       \
  }

#define WARN_ONCE_IF(isTrue, ...)                             \
  {                                                           \
    static bool s_TriggeredWarning = false;                   \
    if ((bool)(isTrue) && !s_TriggeredWarning) {              \
      s_TriggeredWarning = true;                              \
      amaryl::Print("\nWarning issued in " STRINGIFY_BUILTIN( \
          __FILE__) " @ " STRINGIFY_BUILTIN(__LINE__) "\n");  \
      amaryl::PrintSubMessage("\'" #isTrue "\' is true");     \
      amaryl::PrintSubMessage(__VA_ARGS__);                   \
      amaryl::Print("\n");                                    \
    }                                                         \
  }

#define WARN_ONCE_IF_NOT(isTrue, ...) WARN_ONCE_IF(!(isTrue), __VA_ARGS__)

#define DEBUGPRINT(msg, ...) amaryl::Printf(msg "\n", ##__VA_ARGS__);

#endif

#define BreakIfFailed(hr) \
  if (FAILED(hr)) {       \
    PLATFORM_BREAK();     \
  }

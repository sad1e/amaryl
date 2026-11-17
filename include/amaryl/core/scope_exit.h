#pragma once

#include "amaryl/core/macros.h"

// clang-format off
#ifndef _AMARYL_ANONYMOUS_VARIABLE
  #define _AMARYL_CONCATENATE_IMPL(s1, s2) s1## s2
  #define _AMARYL_CONCATENATE(s1, s2) _AMARYL_CONCATENATE_IMPL(s1, s2)

  #ifdef __COUNTER__
    #define _AMARYL_ANONYMOUS_VARIABLE(str) _AMARYL_CONCATENATE(str, __COUNTER__)
  #else
    #define _AMARYL_ANONYMOUS_VARIABLE(str) _AMARYL_CONCATENATE(str, __LINE__)
  #endif
#endif
// clang-format on

namespace amaryl {

enum class ScopeGuardOnExit {};

template <typename T>
struct ScopeGuard {
  explicit ScopeGuard(T&& fn) : fn_(std::move(fn)) {}
  ~ScopeGuard() { fn_(); }

 private:
  T fn_;
};

template <typename T>
ScopeGuard<T> operator+(ScopeGuardOnExit, T&& fn) {
  return ScopeGuard<T>(std::forward<T>(fn));
}

}  // namespace amaryl

#define SCOPE_EXIT \
  auto _AMARYL_ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) = ScopeGuardOnExit() + [&]() noexcept

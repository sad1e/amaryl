#pragma once

#include "amaryl/core/macros.h"

namespace amaryl {

template <typename T, uint32 kId = 0>
struct TSingleton {
  typedef T ValueType;
  typedef TSingleton<T, kId> ThisType;

  static T& GetInstance() {
    static T Instance;
    return Instance;
  }

  TSingleton() = default;
  TSingleton(TSingleton const&) = delete;
  TSingleton& operator=(TSingleton const&) = delete;
};

}  // namespace amaryl

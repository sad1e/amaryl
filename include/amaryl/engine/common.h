#pragma once

// clang-format off
#include "amaryl/engine/engine_export.h"
// clang-format on

#include "amaryl/core/amaryl.h"
#include "amaryl/core/handle_pool.h"
#include "amaryl/nrhi/nrhi.h"

namespace amaryl {

constexpr uint32 kMaxAssetDebugName = 256;
constexpr uint32 kMaxDebugName = 256;

inline std::unique_ptr<IContext> kRHI;

}  // namespace amaryl

#pragma once

#include "amaryl/core/uuid.h"
#include "amaryl/engine/asset/asset_importer.h"
#include "amaryl/engine/common.h"

namespace amaryl {

/**
 * Asset is the resource in project, it include runtime resource and editor resource. The
 * asset will strip all editor resource and be packed into pak files for running.
 */
struct Asset {
 private:
  // The unique id of asset.
  Uuid uuid;

#if AMARYL_EDITOR
  char debug_name_[kMaxAssetDebugName]{};
#endif
};

struct AssetManager {};

}  // namespace amaryl

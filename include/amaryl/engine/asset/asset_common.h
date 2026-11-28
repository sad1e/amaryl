#pragma once

#include "amaryl/core/math/math.h"
#include "amaryl/core/span.h"
#include "amaryl/core/uuid.h"

namespace amaryl {

enum class EAssetType : uint16 {
  Mesh,
  Texture,
  Material,
  Shader,
};

/**
 * Asset is the resource in project, it include runtime resource and editor resource. The
 * asset will strip all editor resource and be packed into pak files for running.
 */
struct Asset {
 private:
  // The unique id of asset.
  Uuid uuid;

  std::string displayName;

  // The path of asset
  std::string path;
};

}  // namespace amaryl

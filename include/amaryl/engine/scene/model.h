#pragma once

#include <vector>

#include "amaryl/engine/asset/scene_asset.h"

namespace amaryl {

struct MeshRegistry;
struct MaterialRegistry;

// GPU resources of a model asset, shared by every entity using them.
struct Model {
  // Per ModelAsset mesh, empty when the mesh failed to import.
  std::vector<MeshHandle> meshes;
  std::vector<MaterialHandle> materials;
};

// Creates the materials and meshes of `asset`, the meshes use the asset materials by
// default.
ENGINE_API Model registerModel(const ModelAsset& asset, MeshRegistry& meshes,
                               MaterialRegistry& materials);
// Destroys the meshes and materials of `model`. Every render proxy using them must be
// destroyed first, the ones destroyed with their transform are released by
// `RenderScene::sync()`.
ENGINE_API void releaseModel(Model& model, MeshRegistry& meshes,
                             MaterialRegistry& materials);

}  // namespace amaryl

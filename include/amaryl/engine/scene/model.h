#pragma once

#include <vector>

#include "amaryl/engine/asset/asset_importer.h"
#include "amaryl/engine/scene/entity.h"

namespace amaryl {

struct MeshRegistry;
struct MaterialRegistry;

// GPU scene resources of an imported glTF file, shared by every instance.
struct Model {
  // Per glTF mesh, empty when the mesh failed to import.
  std::vector<MeshHandle> meshes;
  std::vector<MaterialHandle> materials;
  // Copy of GltfImportResult::nodes, `mesh` indexes `meshes`.
  std::vector<GltfNode> nodes;
};

// Creates the materials and meshes of `imported`, the meshes use the glTF materials by
// default.
ENGINE_API Model registerModel(const GltfImportResult& imported, MeshRegistry& meshes,
                               MaterialRegistry& materials);
// Destroys the meshes and materials of `model`. Every render proxy using them must be
// destroyed first, the ones destroyed with their transform are released by
// `RenderScene::sync()`.
ENGINE_API void releaseModel(Model& model, MeshRegistry& meshes,
                             MaterialRegistry& materials);
// Creates a root entity from `root` and one child entity per node, every node uses the
// mobility of `root`. Destroying the returned entity destroys the whole instance. Returns
// an empty handle on failure.
ENGINE_API EntityHandle instantiateModel(const Model& model, EntityManager& entities,
                                         const TransformDesc& root = {});

}  // namespace amaryl

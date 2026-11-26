#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "amaryl/engine/asset/asset_importer.h"
#include "amaryl/engine/scene/model.h"
#include "amaryl/engine/scene/scene_loader.h"

namespace amaryl {

struct World;
struct TextureRegistry;

// A glTF file registered in a World.
struct LoadedModel {
  std::string path;
  Model model;
  // Node hierarchy instantiated by ModelLibrary::instantiate().
  SceneAsset scene;
};

// Imports glTF files once and instantiates their node hierarchy into a World any number
// of times. The images are loaded by `textures`, which must be the texture references of
// the world materials (`MaterialRegistry::setTextureReferences()`): the materials keep
// their textures alive, the library holds no texture reference.
struct ENGINE_API ModelLibrary final {
  // `world` and `textures` must outlive the library. Without `textures` the materials
  // have no texture.
  ModelLibrary(World& world, TextureRegistry* textures);
  // Releases the unused models, the models still used stay registered in the world.
  ~ModelLibrary();

  NO_COPYABLE(ModelLibrary);

  // Imports `path` (virtual or native UTF-8 path) unless it is already loaded. Returns
  // null when the file cannot be imported.
  const LoadedModel* load(const std::string& path);
  // Null when `path` is not loaded.
  const LoadedModel* find(const std::string& path) const;
  // Loads `path` and adds its nodes to the world, see addSceneNodes().
  bool instantiate(const std::string& path, const SceneNodesDesc& desc,
                   std::vector<EntityHandle>& out);

  // Releases the models whose meshes and materials are used by no render proxy, call
  // after World::sync() so the proxies of destroyed transforms are released. Returns the
  // number of released models.
  uint32 releaseUnused();
  uint32 getCount() const { return static_cast<uint32>(models_.size()); }

  // Mesh build options of the next imports.
  MeshBuildOptions meshBuild{};

 private:
  bool isUnused(const Model& model) const;

  World& world_;
  TextureRegistry* textures_{nullptr};
  std::unordered_map<std::string, std::unique_ptr<LoadedModel>> models_;
};

}  // namespace amaryl

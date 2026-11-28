#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "amaryl/editor/common.h"
#include "amaryl/engine/scene/model_library.h"
#include "amaryl/engine/scene/world.h"

namespace amaryl {

// Editing operations of the editor on a World. The editor only changes CPU data (scene
// graph, registries, render scene), the engine uploads the changes of every frame through
// the registries' dirty tracking and GpuSceneUploads.
//
// Materials are copy-on-write: editing the material of an entity clones it into an
// override owned by the editor and assigned to that entity only, the shared material of
// the model is left untouched.
struct EDITOR_API EditorScene final {
  // `world` and `models` must outlive the editor scene.
  EditorScene(World& world, ModelLibrary& models);
  // Destroys the unused overrides, the ones still used stay in the world.
  ~EditorScene();

  NO_COPYABLE(EditorScene);

  // Imports the glTF file at `path` under a new root entity (a child of `parent` when
  // set) and returns the root, empty when the file cannot be imported. The root is named
  // after the file, the node entities after the glTF nodes.
  EntityHandle importModel(const std::string& path, EntityHandle parent = {});
  // Empty when the entity has no name.
  std::string_view getName(EntityHandle entity) const;
  // An empty name removes the name.
  void setName(EntityHandle entity, std::string name);
  // Destroys the entity with its descendants, then the overrides and models nobody uses.
  void destroyEntity(EntityHandle entity);
  void setLocalTRS(
    EntityHandle entity, Vec3Arg position, QuatArg rotation, Vec3Arg scale);

  // An empty handle clears the selection.
  void select(EntityHandle entity);
  // Empty when nothing is selected or the selected entity was destroyed.
  EntityHandle getSelection() const;
  // Selects the entity owning the render proxy of a GpuInstanceId (GPU picking), clears
  // the selection when the instance is not alive. Returns the new selection.
  EntityHandle selectByInstanceId(uint32 instance_id);

  // Material of `section` of the entity's render proxy, empty without render proxy.
  MaterialHandle getMaterial(EntityHandle entity, uint32 section) const;
  // Writes `asset` to the material of `section` for this entity only: an override owned
  // by the entity is updated in place, a shared material is replaced by a new override.
  // Returns the written material, empty without render proxy.
  MaterialHandle editMaterial(
    EntityHandle entity, uint32 section, const MaterialAsset& asset);
  // Restores the mesh's default material and destroys the override.
  void resetMaterial(EntityHandle entity, uint32 section);
  bool isOverride(MaterialHandle material) const;
  uint32 getOverrideCount() const { return static_cast<uint32>(overrides_.size()); }

  // Destroys the overrides, the models no render proxy uses anymore and the names of the
  // destroyed entities, call after World::sync() when transforms were destroyed outside
  // of destroyEntity().
  void collectGarbage();

 private:
  static uint64 NameKey(EntityHandle entity);
  RenderProxyHandle getProxy(EntityHandle entity) const;
  void destroyOverrideIfUnused(MaterialHandle material);

  World& world_;
  ModelLibrary& models_;
  EntityHandle selection_{};
  std::vector<MaterialHandle> overrides_;
  std::unordered_map<uint64, std::string> names_;
};

}  // namespace amaryl

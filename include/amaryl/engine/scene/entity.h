#pragma once

#include <vector>

#include "amaryl/engine/render/render_scene.h"
#include "amaryl/engine/scene/common.h"

namespace amaryl {

struct SceneGraph;

struct EntityDesc {
  // `transform.parent` is the transform of the parent entity, or empty for a root.
  TransformDesc transform{};
  // Optional, the entity has no render proxy without mesh.
  MeshHandle mesh{};
  // See RenderProxyDesc.
  Span<const MaterialHandle> materials{};
  bool castShadow = true;
};

// An entity owns a SceneGraph transform and the components attached to it, only a render
// proxy for now. Entities must be destroyed through the manager: destroying their
// transform directly kills them (`isAlive()` returns false) but their records are only
// released by `destroy()`.
struct ENGINE_API EntityManager final {
  static constexpr uint32 kInvalidIndex = 0xffffffffu;

  // The graph and the render scene must outlive the manager.
  EntityManager(SceneGraph& graph, RenderScene& render_scene);
  ~EntityManager();

  NO_COPYABLE(EntityManager);

  // Returns an empty handle when the parent transform, the mesh or a material is invalid.
  EntityHandle create(const EntityDesc& desc);
  // Immediately destroys the entity, the entities of its descendant transforms and their
  // components, then the transform hierarchy. Does nothing for released handles.
  void destroy(EntityHandle handle);
  bool isAlive(EntityHandle handle) const;
  uint32 getCount() const { return pool_.getNumObjects(); }

  TransformHandle getTransform(EntityHandle handle) const;
  // Empty when the entity has no mesh.
  RenderProxyHandle getRenderProxy(EntityHandle handle) const;
  // Empty when no entity owns `transform`.
  EntityHandle findByTransform(TransformHandle transform) const;

 private:
  struct Record {
    TransformHandle transform{};
    RenderProxyHandle proxy{};
  };

  bool isRecordAlive(EntityHandle handle) const;
  const Record& getRecord(EntityHandle handle) const;
  void release(EntityHandle handle);

  SceneGraph& graph_;
  RenderScene& render_scene_;
  Pool<EntityTag, Record> pool_;
  // Transform handle index -> entity slot.
  std::vector<uint32> by_transform_;
  std::vector<TransformHandle> walk_stack_;
};

}  // namespace amaryl

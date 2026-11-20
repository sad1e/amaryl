#pragma once

#include <memory>
#include <vector>

#include "amaryl/engine/render/render_scene.h"
#include "amaryl/engine/scene/camera.h"
#include "amaryl/engine/scene/common.h"
#include "amaryl/engine/scene/light.h"

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

// An entity owns a SceneGraph transform and the components attached to it: a render
// proxy, a camera and a light. Entities must be destroyed through the manager: destroying
// their transform directly kills them (`isAlive()` returns false) but their records are
// only released by `destroy()`.
struct ENGINE_API EntityManager final {
  static constexpr uint32 kInvalidIndex = 0xffffffffu;

  // The graph and the render scene must outlive the manager.
  EntityManager(SceneGraph& graph, RenderScene& render_scene);
  ~EntityManager();

  NO_COPYABLE(EntityManager);

  // Returns an empty handle when the parent transform, the mesh or a material is invalid,
  // or when a static entity has a movable parent.
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

  // Attaches `camera` to the entity, replacing and destroying the previous one, null
  // detaches it. Returns the attached camera, its world transform is set immediately.
  Camera* setCamera(EntityHandle handle, std::unique_ptr<Camera> camera);
  // Null when the entity has no camera.
  Camera* getCamera(EntityHandle handle) const;
  // Same as setCamera() for the light.
  Light* setLight(EntityHandle handle, std::unique_ptr<Light> light);
  Light* getLight(EntityHandle handle) const;
  // Copies the world transforms of the graph into the cameras and lights, call after
  // SceneGraph::update().
  void updateComponents();

 private:
  struct Record {
    TransformHandle transform{};
    RenderProxyHandle proxy{};
  };

  // Per entity slot, the polymorphic components live outside the pool.
  struct Components {
    std::unique_ptr<Camera> camera;
    std::unique_ptr<Light> light;
  };

  bool isRecordAlive(EntityHandle handle) const;
  const Record& getRecord(EntityHandle handle) const;
  Components& getComponents(EntityHandle handle);
  void release(EntityHandle handle);

  SceneGraph& graph_;
  RenderScene& render_scene_;
  Pool<EntityTag, Record> pool_;
  std::vector<Components> components_;
  // Transform handle index -> entity slot.
  std::vector<uint32> by_transform_;
  std::vector<TransformHandle> walk_stack_;
};

}  // namespace amaryl

#pragma once

#include <vector>

#include "amaryl/engine/render/gpu_scene.h"
#include "amaryl/engine/render/range_allocator.h"
#include "amaryl/engine/scene/common.h"

namespace amaryl {

struct SceneGraph;
struct MeshRegistry;
struct MaterialRegistry;

struct RenderProxyDesc {
  TransformHandle transform{};
  MeshHandle mesh{};
  // `materials[i]` is used by section i, missing or empty entries use the mesh's default
  // material.
  Span<const MaterialHandle> materials{};
  bool castShadow = true;
};

// Static mesh instances of the GPU scene, each one follows a SceneGraph transform. The
// GpuInstanceId of a proxy is its handle index. Proxies of static transforms upload their
// transform once at creation.
//
// Per frame:
//   graph.update(); scene.sync();
//   uploads.clear(); meshes.flushUploads(uploads); materials.flushUploads(uploads);
//   scene.flushUploads(uploads);
struct ENGINE_API RenderScene final {
  static constexpr uint32 kInvalidIndex = 0xffffffffu;

  // The graph and the registries must outlive the scene, destroying the scene releases
  // the meshes and materials of its proxies.
  RenderScene(SceneGraph& graph, MeshRegistry& meshes, MaterialRegistry& materials);
  ~RenderScene();

  NO_COPYABLE(RenderScene);

  // Returns an empty handle when the transform, the mesh or a material is not alive.
  RenderProxyHandle create(const RenderProxyDesc& desc);
  // Does nothing for dead handles, proxies are also destroyed with their transform.
  void destroy(RenderProxyHandle handle);
  bool isAlive(RenderProxyHandle handle) const;
  uint32 getCount() const { return pool_.getNumObjects(); }

  TransformHandle getTransform(RenderProxyHandle handle) const;
  MeshHandle getMesh(RenderProxyHandle handle) const;
  MaterialHandle getMaterial(RenderProxyHandle handle, uint32 section) const;
  // An empty `material` restores the mesh's default material.
  void setMaterial(RenderProxyHandle handle, uint32 section, MaterialHandle material);
  void setCastShadow(RenderProxyHandle handle, bool cast_shadow);
  static uint32 getInstanceId(RenderProxyHandle handle) { return handle.index(); }
  // Live proxy of a GpuInstanceId (e.g. read back by GPU picking), empty otherwise.
  RenderProxyHandle getProxyByInstanceId(uint32 instance_id) const;

  // Destroys the proxies of destroyed transforms and queues the transforms that changed
  // since the last sync. Call after `SceneGraph::update()`.
  void sync();
  // Writes the instances, material slots and transform updates of this frame.
  void flushUploads(GpuSceneUploads& uploads);

 private:
  struct Proxy {
    TransformHandle transform{};
    MeshHandle mesh{};
    // One per mesh section, never empty.
    std::vector<MaterialHandle> materials;
    uint32 materialSlotOffset{0};
    // Doubly linked list of the proxies whose transform has the same handle index.
    uint32 prevOnTransform{kInvalidIndex};
    uint32 nextOnTransform{kInvalidIndex};
    bool castShadow{true};
    bool isStatic{false};
  };

  Proxy& getProxy(RenderProxyHandle handle);
  const Proxy& getProxy(RenderProxyHandle handle) const;
  Proxy& getSlot(uint32 slot);
  const Proxy& getSlot(uint32 slot) const;
  bool isSlotAlive(uint32 slot) const;
  void destroyAttached(TransformHandle transform);
  void markDirty(uint32 slot);
  void queueTransform(uint32 slot, const Mat44& world, bool teleport);
  void writeInstance(uint32 slot, GpuSceneUploads& uploads) const;

  SceneGraph& graph_;
  MeshRegistry& meshes_;
  MaterialRegistry& materials_;

  Pool<RenderProxyTag, Proxy> pool_;
  RangeAllocator material_slots_;
  // Transform handle index -> first proxy slot.
  std::vector<uint32> transform_heads_;

  // Per slot: 1 when the instance record must be rewritten.
  std::vector<uint8> slot_dirty_;
  std::vector<uint32> dirty_slots_;

  // Per slot: position in `pending_transforms_`, at most one update per instance and
  // frame.
  std::vector<uint32> slot_transform_entries_;
  std::vector<GpuTransformUpdate> pending_transforms_;
  // Instances moved by the previous flush, they settle when they do not move again.
  std::vector<uint32> moved_slots_;

  uint32 synced_version_{0};
};

}  // namespace amaryl

#pragma once

#include <memory>
#include <vector>

#include "amaryl/engine/asset/mesh_asset.h"
#include "amaryl/engine/render/gpu_scene.h"
#include "amaryl/engine/render/range_allocator.h"
#include "amaryl/engine/scene/common.h"

namespace amaryl {

struct MaterialRegistry;

// CPU owner of the GPU mesh table and the geometry buffers, the GPU index of a mesh is
// its handle index. Geometry is immutable and uploaded by the first `flushUploads()`
// after `create()`. Meshes are reference counted by the render proxies using them and
// must be unused when destroyed.
struct ENGINE_API MeshRegistry final {
  // `materials` must outlive the registry.
  explicit MeshRegistry(MaterialRegistry& materials);
  ~MeshRegistry();

  NO_COPYABLE(MeshRegistry);

  // `default_materials[i]` is used by section i, missing or empty entries use the default
  // material. Returns an empty handle when the asset is malformed.
  MeshHandle create(std::shared_ptr<const MeshAsset> asset,
                    Span<const MaterialHandle> default_materials = {});
  void destroy(MeshHandle handle);
  bool isAlive(MeshHandle handle) const;
  uint32 getCount() const { return pool_.getNumObjects(); }

  const MeshAsset& getAsset(MeshHandle handle) const;
  uint32 getSectionCount(MeshHandle handle) const;
  MaterialHandle getDefaultMaterial(MeshHandle handle, uint32 section) const;
  static uint32 getGpuIndex(MeshHandle handle) { return handle.index(); }

  void addRef(MeshHandle handle);
  void release(MeshHandle handle);
  uint32 getRefCount(MeshHandle handle) const;

  // Writes the meshes created since the last flush.
  void flushUploads(GpuSceneUploads& uploads);

 private:
  struct Record {
    std::shared_ptr<const MeshAsset> asset;
    std::vector<MaterialHandle> defaultMaterials;
    uint32 baseVertex{0};
    uint32 indexOffset{0};
    uint32 sectionOffset{0};
    uint32 meshletOffset{0};
    uint32 meshletVertexOffset{0};
    // In dwords.
    uint32 meshletTriangleOffset{0};
    uint32 refCount{0};
    bool pendingUpload{false};
  };

  Record& getRecord(MeshHandle handle);
  const Record& getRecord(MeshHandle handle) const;
  void upload(uint32 slot, const Record& record, GpuSceneUploads& uploads) const;

  MaterialRegistry& materials_;
  Pool<MeshTag, Record> pool_;
  RangeAllocator vertices_;
  RangeAllocator indices_;
  RangeAllocator sections_;
  RangeAllocator meshlets_;
  RangeAllocator meshlet_vertices_;
  RangeAllocator meshlet_triangles_;
  // May contain destroyed or duplicated slots, filtered by `Record::pendingUpload`.
  std::vector<uint32> pending_slots_;
};

}  // namespace amaryl

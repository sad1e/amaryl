#pragma once

#include <vector>

#include "amaryl/engine/asset/material_asset.h"
#include "amaryl/engine/render/gpu_scene.h"
#include "amaryl/engine/scene/common.h"

namespace amaryl {

// CPU owner of the GPU material table, the GPU index of a material is its handle index.
// Materials are reference counted by the meshes and render proxies using them and must be
// unused when destroyed. To change a shared material for one user only, `clone()` it and
// assign the copy (copy-on-write).
struct ENGINE_API MaterialRegistry final {
  // Creates the default material.
  MaterialRegistry();
  ~MaterialRegistry();

  NO_COPYABLE(MaterialRegistry);

  MaterialHandle create(const MaterialAsset& asset);
  MaterialHandle clone(MaterialHandle source);
  void update(MaterialHandle handle, const MaterialAsset& asset);
  void destroy(MaterialHandle handle);
  bool isAlive(MaterialHandle handle) const;
  uint32 getCount() const { return pool_.getNumObjects(); }

  const MaterialAsset& get(MaterialHandle handle) const;
  // Used by sections without material, lives as long as the registry.
  MaterialHandle getDefault() const { return default_; }
  static uint32 getGpuIndex(MaterialHandle handle) { return handle.index(); }

  void addRef(MaterialHandle handle);
  void release(MaterialHandle handle);
  uint32 getRefCount(MaterialHandle handle) const;

  // Writes the materials created or updated since the last flush.
  void flushUploads(GpuSceneUploads& uploads);

 private:
  struct Record {
    MaterialAsset asset;
    uint32 refCount{0};
    bool dirty{false};
  };

  Record& getRecord(MaterialHandle handle);
  const Record& getRecord(MaterialHandle handle) const;
  void markDirty(MaterialHandle handle);

  Pool<MaterialTag, Record> pool_;
  // May contain destroyed or duplicated slots, filtered by `Record::dirty`.
  std::vector<uint32> dirty_slots_;
  MaterialHandle default_;
};

}  // namespace amaryl

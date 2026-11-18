#include "amaryl/engine/render/material_registry.h"

namespace amaryl {

MaterialRegistry::MaterialRegistry() {
  MaterialAsset asset;
  asset.name = "default";
  default_ = create(asset);
}

MaterialRegistry::~MaterialRegistry() = default;

MaterialHandle MaterialRegistry::create(const MaterialAsset& asset) {
  const MaterialHandle handle = pool_.create(Record{asset});
  markDirty(handle);
  return handle;
}

MaterialHandle MaterialRegistry::clone(MaterialHandle source) {
  // Copied first, `create()` may grow the pool and move the source record.
  const MaterialAsset asset = get(source);
  return create(asset);
}

void MaterialRegistry::update(MaterialHandle handle, const MaterialAsset& asset) {
  getRecord(handle).asset = asset;
  markDirty(handle);
}

void MaterialRegistry::destroy(MaterialHandle handle) {
  ASSERT(handle != default_, "The default material cannot be destroyed");
  ASSERT(getRecord(handle).refCount == 0, "Destroying a material in use");
  pool_.destroy(handle);
}

bool MaterialRegistry::isAlive(MaterialHandle handle) const {
  return handle.index() < pool_.getSizeOfObjects() &&
         pool_.getHandle(handle.index()) == handle;
}

const MaterialAsset& MaterialRegistry::get(MaterialHandle handle) const {
  return getRecord(handle).asset;
}

void MaterialRegistry::addRef(MaterialHandle handle) { ++getRecord(handle).refCount; }

void MaterialRegistry::release(MaterialHandle handle) {
  Record& record = getRecord(handle);
  ASSERT(record.refCount > 0, "Material reference count underflow");
  --record.refCount;
}

uint32 MaterialRegistry::getRefCount(MaterialHandle handle) const {
  return getRecord(handle).refCount;
}

void MaterialRegistry::flushUploads(GpuSceneUploads& uploads) {
  GpuScatterQueue& queue = uploads[EGpuSceneBuffer::Materials];
  for (const uint32 slot : dirty_slots_) {
    Record& record = *pool_.get(pool_.getHandle(slot));
    if (record.dirty) {
      record.dirty = false;
      queue.writeElement(slot, toGpuMaterial(record.asset));
    }
  }
  dirty_slots_.clear();
  uploads.capacity(EGpuSceneBuffer::Materials) = pool_.getSizeOfObjects();
}

MaterialRegistry::Record& MaterialRegistry::getRecord(MaterialHandle handle) {
  ASSERT(isAlive(handle), "Invalid material handle");
  return *pool_.get(handle);
}

const MaterialRegistry::Record& MaterialRegistry::getRecord(MaterialHandle handle) const {
  ASSERT(isAlive(handle), "Invalid material handle");
  return *pool_.get(handle);
}

void MaterialRegistry::markDirty(MaterialHandle handle) {
  Record& record = getRecord(handle);
  if (!record.dirty) {
    record.dirty = true;
    dirty_slots_.push_back(handle.index());
  }
}

}  // namespace amaryl

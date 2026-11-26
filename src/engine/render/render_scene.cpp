#include "amaryl/engine/render/render_scene.h"

#include "amaryl/core/logger.h"
#include "amaryl/engine/render/material_registry.h"
#include "amaryl/engine/render/mesh_registry.h"
#include "amaryl/engine/scene/scene_graph.h"

namespace amaryl {

RenderScene::RenderScene(SceneGraph& graph, MeshRegistry& meshes,
                         MaterialRegistry& materials)
    : graph_(graph), meshes_(meshes), materials_(materials) {}

RenderScene::~RenderScene() {
  for (uint32 slot = 0; slot < pool_.getSizeOfObjects(); ++slot) {
    if (isSlotAlive(slot)) {
      const Proxy& proxy = getSlot(slot);
      for (const MaterialHandle material : proxy.materials) {
        materials_.release(material);
      }
      meshes_.release(proxy.mesh);
    }
  }
}

RenderProxyHandle RenderScene::create(const RenderProxyDesc& desc) {
  if (!graph_.isAlive(desc.transform) || !meshes_.isAlive(desc.mesh)) {
    ALOGE("RenderScene: invalid transform or mesh");
    return {};
  }

  const uint32 section_count = meshes_.getSectionCount(desc.mesh);
  Proxy proxy;
  proxy.transform = desc.transform;
  proxy.mesh = desc.mesh;
  proxy.castShadow = desc.castShadow;
  proxy.isStatic = graph_.getMobility(desc.transform) == ETransformMobility::Static;
  proxy.materials.resize(section_count);
  for (uint32 i = 0; i < section_count; ++i) {
    const MaterialHandle material =
        i < desc.materials.size() ? desc.materials[i] : MaterialHandle{};
    if (material && !materials_.isAlive(material)) {
      ALOGE("RenderScene: invalid material for section %u", i);
      return {};
    }
    proxy.materials[i] = material ? material : meshes_.getDefaultMaterial(desc.mesh, i);
  }

  meshes_.addRef(proxy.mesh);
  for (const MaterialHandle material : proxy.materials) {
    materials_.addRef(material);
  }
  proxy.materialSlotOffset = material_slots_.allocate(section_count);

  const uint32 transform_index = desc.transform.index();
  if (transform_index >= transform_heads_.size()) {
    transform_heads_.resize(transform_index + 1, kInvalidIndex);
  }
  proxy.nextOnTransform = transform_heads_[transform_index];

  const RenderProxyHandle handle = pool_.create(std::move(proxy));
  const uint32 slot = handle.index();
  if (slot >= slot_dirty_.size()) {
    slot_dirty_.resize(slot + 1, 0);
    slot_transform_entries_.resize(slot + 1, kInvalidIndex);
  }
  const uint32 next = transform_heads_[transform_index];
  if (next != kInvalidIndex) {
    getSlot(next).prevOnTransform = slot;
  }
  transform_heads_[transform_index] = slot;

  markDirty(slot);
  queueTransform(slot,
                 graph_.isWorldStale(desc.transform)
                     ? graph_.computeWorldImmediate(desc.transform)
                     : graph_.getWorld(desc.transform),
                 true);
  return handle;
}

void RenderScene::destroy(RenderProxyHandle handle) {
  if (!isAlive(handle)) {
    return;
  }

  const Proxy& proxy = getProxy(handle);
  if (proxy.prevOnTransform != kInvalidIndex) {
    getSlot(proxy.prevOnTransform).nextOnTransform = proxy.nextOnTransform;
  } else {
    transform_heads_[proxy.transform.index()] = proxy.nextOnTransform;
  }
  if (proxy.nextOnTransform != kInvalidIndex) {
    getSlot(proxy.nextOnTransform).prevOnTransform = proxy.prevOnTransform;
  }

  material_slots_.free(proxy.materialSlotOffset,
                       static_cast<uint32>(proxy.materials.size()));
  for (const MaterialHandle material : proxy.materials) {
    materials_.release(material);
  }
  meshes_.release(proxy.mesh);
  pool_.destroy(handle);
  // Rewrites the instance as invalid, unless the slot is reused first.
  markDirty(handle.index());
}

bool RenderScene::isAlive(RenderProxyHandle handle) const {
  return handle.index() < pool_.getSizeOfObjects() &&
         pool_.getHandle(handle.index()) == handle;
}

RenderProxyHandle RenderScene::getProxyByInstanceId(uint32 instance_id) const {
  return isSlotAlive(instance_id) ? pool_.getHandle(instance_id) : RenderProxyHandle{};
}

TransformHandle RenderScene::getTransform(RenderProxyHandle handle) const {
  return getProxy(handle).transform;
}

MeshHandle RenderScene::getMesh(RenderProxyHandle handle) const {
  return getProxy(handle).mesh;
}

MaterialHandle RenderScene::getMaterial(RenderProxyHandle handle, uint32 section) const {
  const Proxy& proxy = getProxy(handle);
  ASSERT(section < proxy.materials.size(), "Invalid mesh section");
  return proxy.materials[section];
}

void RenderScene::setMaterial(RenderProxyHandle handle, uint32 section,
                              MaterialHandle material) {
  Proxy& proxy = getProxy(handle);
  ASSERT(section < proxy.materials.size(), "Invalid mesh section");
  const MaterialHandle resolved =
      material ? material : meshes_.getDefaultMaterial(proxy.mesh, section);
  if (resolved == proxy.materials[section]) {
    return;
  }

  materials_.addRef(resolved);
  materials_.release(proxy.materials[section]);
  proxy.materials[section] = resolved;
  markDirty(handle.index());
}

void RenderScene::setCastShadow(RenderProxyHandle handle, bool cast_shadow) {
  Proxy& proxy = getProxy(handle);
  if (proxy.castShadow != cast_shadow) {
    proxy.castShadow = cast_shadow;
    markDirty(handle.index());
  }
}

void RenderScene::sync() {
  const bool complete = graph_.forEachDestroyedSince(
      synced_version_, [this](TransformHandle transform) { destroyAttached(transform); });
  if (!complete) {
    for (uint32 slot = 0; slot < pool_.getSizeOfObjects(); ++slot) {
      if (isSlotAlive(slot) && !graph_.isAlive(getSlot(slot).transform)) {
        destroy(pool_.getHandle(slot));
      }
    }
  }

  graph_.forEachChangedSince(
      synced_version_,
      [this](TransformHandle transform, const Mat44& world, bool teleported) {
        if (transform.index() >= transform_heads_.size()) {
          return;
        }
        for (uint32 slot = transform_heads_[transform.index()]; slot != kInvalidIndex;) {
          const Proxy& proxy = getSlot(slot);
          if (proxy.transform == transform && !proxy.isStatic) {
            queueTransform(slot, world, teleported);
          }
          slot = proxy.nextOnTransform;
        }
      });
  synced_version_ = graph_.getWorldVersion();
}

void RenderScene::flushUploads(GpuSceneUploads& uploads) {
  for (const uint32 slot : dirty_slots_) {
    slot_dirty_[slot] = 0;
    writeInstance(slot, uploads);
  }
  dirty_slots_.clear();

  // Instances that moved in the previous frame but not in this one: prev = curr.
  for (const uint32 slot : moved_slots_) {
    if (slot_transform_entries_[slot] == kInvalidIndex && isSlotAlive(slot)) {
      uploads.transforms.push_back({slot, GPU_TRANSFORM_UPDATE_SETTLE, 0, 0, {}});
    }
  }
  moved_slots_.clear();

  for (const GpuTransformUpdate& update : pending_transforms_) {
    slot_transform_entries_[update.instanceId] = kInvalidIndex;
    if (isSlotAlive(update.instanceId)) {
      uploads.transforms.push_back(update);
      if ((update.flags & GPU_TRANSFORM_UPDATE_TELEPORT) == 0) {
        moved_slots_.push_back(update.instanceId);
      }
    }
  }
  pending_transforms_.clear();

  uploads.capacity(EGpuSceneBuffer::Instances) = pool_.getSizeOfObjects();
  uploads.capacity(EGpuSceneBuffer::MaterialSlots) = material_slots_.getEnd();
}

RenderScene::Proxy& RenderScene::getProxy(RenderProxyHandle handle) {
  ASSERT(isAlive(handle), "Invalid render proxy handle");
  return *pool_.get(handle);
}

const RenderScene::Proxy& RenderScene::getProxy(RenderProxyHandle handle) const {
  ASSERT(isAlive(handle), "Invalid render proxy handle");
  return *pool_.get(handle);
}

RenderScene::Proxy& RenderScene::getSlot(uint32 slot) {
  return *pool_.get(pool_.getHandle(slot));
}

const RenderScene::Proxy& RenderScene::getSlot(uint32 slot) const {
  return *pool_.get(pool_.getHandle(slot));
}

bool RenderScene::isSlotAlive(uint32 slot) const {
  // Destroyed slots are reset to Proxy{}, live proxies always have a mesh.
  return slot < pool_.getSizeOfObjects() && getSlot(slot).mesh.valid();
}

void RenderScene::destroyAttached(TransformHandle transform) {
  if (transform.index() >= transform_heads_.size()) {
    return;
  }
  for (uint32 slot = transform_heads_[transform.index()]; slot != kInvalidIndex;) {
    const Proxy& proxy = getSlot(slot);
    const uint32 next = proxy.nextOnTransform;
    if (proxy.transform == transform) {
      destroy(pool_.getHandle(slot));
    }
    slot = next;
  }
}

void RenderScene::markDirty(uint32 slot) {
  if (slot_dirty_[slot] == 0) {
    slot_dirty_[slot] = 1;
    dirty_slots_.push_back(slot);
  }
}

void RenderScene::queueTransform(uint32 slot, const Mat44& world, bool teleport) {
  uint32& entry = slot_transform_entries_[slot];
  if (entry == kInvalidIndex) {
    entry = static_cast<uint32>(pending_transforms_.size());
    pending_transforms_.push_back({slot, 0, 0, 0, {}});
  }

  GpuTransformUpdate& update = pending_transforms_[entry];
  update.flags |= teleport ? GPU_TRANSFORM_UPDATE_TELEPORT : 0u;
  for (uint32 row = 0; row < 3; ++row) {
    update.world.rows[row] =
        float4{world(row, 0), world(row, 1), world(row, 2), world(row, 3)};
  }
}

void RenderScene::writeInstance(uint32 slot, GpuSceneUploads& uploads) const {
  GpuInstance instance{};
  instance.meshIndex = GPU_SCENE_INVALID_INDEX;
  instance.materialSlotOffset = GPU_SCENE_INVALID_INDEX;
  instance.skinningOffset = GPU_SCENE_INVALID_INDEX;
  if (isSlotAlive(slot)) {
    const Proxy& proxy = getSlot(slot);
    instance.localBoundingSphere = meshes_.getAsset(proxy.mesh).boundingSphere;
    instance.meshIndex = MeshRegistry::getGpuIndex(proxy.mesh);
    instance.materialSlotOffset = proxy.materialSlotOffset;
    instance.flags = GPU_INSTANCE_FLAG_VALID |
                     (proxy.castShadow ? GPU_INSTANCE_FLAG_CAST_SHADOW : 0u) |
                     (proxy.isStatic ? GPU_INSTANCE_FLAG_STATIC : 0u);

    GpuScatterQueue& material_slots = uploads[EGpuSceneBuffer::MaterialSlots];
    for (uint32 i = 0; i < proxy.materials.size(); ++i) {
      material_slots.writeElement(proxy.materialSlotOffset + i,
                                  MaterialRegistry::getGpuIndex(proxy.materials[i]));
    }
  }
  uploads[EGpuSceneBuffer::Instances].writeElement(slot, instance);
}

}  // namespace amaryl

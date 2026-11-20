#include "amaryl/engine/scene/entity.h"

#include "amaryl/core/logger.h"
#include "amaryl/engine/scene/scene_graph.h"

namespace amaryl {

EntityManager::EntityManager(SceneGraph& graph, RenderScene& render_scene)
    : graph_(graph), render_scene_(render_scene) {}

EntityManager::~EntityManager() = default;

EntityHandle EntityManager::create(const EntityDesc& desc) {
  if (desc.transform.parent && !graph_.isAlive(desc.transform.parent)) {
    ALOGE("EntityManager: invalid parent transform");
    return {};
  }
  if (desc.transform.parent && desc.transform.mobility == ETransformMobility::Static &&
      graph_.getMobility(desc.transform.parent) == ETransformMobility::Movable) {
    ALOGE("EntityManager: a static entity cannot have a movable parent");
    return {};
  }

  const TransformHandle transform = graph_.create(desc.transform);
  RenderProxyHandle proxy{};
  if (desc.mesh) {
    proxy = render_scene_.create({transform, desc.mesh, desc.materials, desc.castShadow});
    if (!proxy) {
      graph_.destroy(transform);
      return {};
    }
  }

  const EntityHandle handle = pool_.create(Record{transform, proxy});
  if (transform.index() >= by_transform_.size()) {
    by_transform_.resize(transform.index() + 1, kInvalidIndex);
  }
  by_transform_[transform.index()] = handle.index();
  if (handle.index() >= components_.size()) {
    components_.resize(handle.index() + 1);
  }
  return handle;
}

void EntityManager::destroy(EntityHandle handle) {
  if (!isRecordAlive(handle)) {
    return;
  }

  const TransformHandle transform = getRecord(handle).transform;
  if (graph_.isAlive(transform)) {
    walk_stack_.clear();
    walk_stack_.push_back(transform);
    while (!walk_stack_.empty()) {
      const TransformHandle node = walk_stack_.back();
      walk_stack_.pop_back();
      for (TransformHandle child = graph_.getFirstChild(node); child;
           child = graph_.getNextSibling(child)) {
        walk_stack_.push_back(child);
        const EntityHandle entity = findByTransform(child);
        if (entity) {
          release(entity);
        }
      }
    }
    graph_.destroy(transform, ETransformDestroyMode::Recursive);
  }
  release(handle);
}

bool EntityManager::isAlive(EntityHandle handle) const {
  return isRecordAlive(handle) && graph_.isAlive(pool_.get(handle)->transform);
}

TransformHandle EntityManager::getTransform(EntityHandle handle) const {
  return getRecord(handle).transform;
}

RenderProxyHandle EntityManager::getRenderProxy(EntityHandle handle) const {
  return getRecord(handle).proxy;
}

EntityHandle EntityManager::findByTransform(TransformHandle transform) const {
  if (transform.index() >= by_transform_.size()) {
    return {};
  }
  const uint32 slot = by_transform_[transform.index()];
  if (slot == kInvalidIndex) {
    return {};
  }
  // The slot stays alive while referenced, but the transform may have been destroyed
  // directly and its index reused.
  const EntityHandle handle = pool_.getHandle(slot);
  return pool_.get(handle)->transform == transform ? handle : EntityHandle{};
}

Camera* EntityManager::setCamera(EntityHandle handle, std::unique_ptr<Camera> camera) {
  const TransformHandle transform = getRecord(handle).transform;
  std::unique_ptr<Camera>& slot = getComponents(handle).camera;
  slot = std::move(camera);
  if (slot && graph_.isAlive(transform)) {
    slot->setWorldTransform(graph_.computeWorldImmediate(transform));
  }
  return slot.get();
}

Camera* EntityManager::getCamera(EntityHandle handle) const {
  ASSERT(isRecordAlive(handle), "Invalid entity handle");
  return components_[handle.index()].camera.get();
}

Light* EntityManager::setLight(EntityHandle handle, std::unique_ptr<Light> light) {
  const TransformHandle transform = getRecord(handle).transform;
  std::unique_ptr<Light>& slot = getComponents(handle).light;
  slot = std::move(light);
  if (slot && graph_.isAlive(transform)) {
    slot->setWorldTransform(graph_.computeWorldImmediate(transform));
  }
  return slot.get();
}

Light* EntityManager::getLight(EntityHandle handle) const {
  ASSERT(isRecordAlive(handle), "Invalid entity handle");
  return components_[handle.index()].light.get();
}

void EntityManager::updateComponents() {
  for (uint32 slot = 0; slot < components_.size(); ++slot) {
    Components& components = components_[slot];
    if (!components.camera && !components.light) {
      continue;
    }
    // Components are reset on release, their slot is alive.
    const TransformHandle transform = pool_.get(pool_.getHandle(slot))->transform;
    if (!graph_.isAlive(transform)) {
      continue;
    }
    const Mat44& world = graph_.getWorld(transform);
    if (components.camera) {
      components.camera->setWorldTransform(world);
    }
    if (components.light) {
      components.light->setWorldTransform(world);
    }
  }
}

bool EntityManager::isRecordAlive(EntityHandle handle) const {
  return handle.index() < pool_.getSizeOfObjects() &&
         pool_.getHandle(handle.index()) == handle;
}

const EntityManager::Record& EntityManager::getRecord(EntityHandle handle) const {
  ASSERT(isRecordAlive(handle), "Invalid entity handle");
  return *pool_.get(handle);
}

EntityManager::Components& EntityManager::getComponents(EntityHandle handle) {
  ASSERT(isRecordAlive(handle), "Invalid entity handle");
  return components_[handle.index()];
}

void EntityManager::release(EntityHandle handle) {
  const Record record = getRecord(handle);
  if (record.proxy) {
    render_scene_.destroy(record.proxy);
  }
  components_[handle.index()] = {};
  uint32& slot = by_transform_[record.transform.index()];
  if (slot == handle.index()) {
    slot = kInvalidIndex;
  }
  pool_.destroy(handle);
}

}  // namespace amaryl

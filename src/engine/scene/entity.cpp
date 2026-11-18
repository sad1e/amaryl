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

bool EntityManager::isRecordAlive(EntityHandle handle) const {
  return handle.index() < pool_.getSizeOfObjects() &&
         pool_.getHandle(handle.index()) == handle;
}

const EntityManager::Record& EntityManager::getRecord(EntityHandle handle) const {
  ASSERT(isRecordAlive(handle), "Invalid entity handle");
  return *pool_.get(handle);
}

void EntityManager::release(EntityHandle handle) {
  const Record record = getRecord(handle);
  if (record.proxy) {
    render_scene_.destroy(record.proxy);
  }
  uint32& slot = by_transform_[record.transform.index()];
  if (slot == handle.index()) {
    slot = kInvalidIndex;
  }
  pool_.destroy(handle);
}

}  // namespace amaryl

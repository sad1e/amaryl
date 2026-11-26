#include "amaryl/editor/scene/editor_scene.h"

#include <algorithm>

namespace amaryl {

EditorScene::EditorScene(World& world, ModelLibrary& models)
    : world_(world), models_(models) {}

EditorScene::~EditorScene() { collectGarbage(); }

EntityHandle EditorScene::importModel(const std::string& path, EntityHandle parent) {
  EntityDesc root_desc;
  if (parent) {
    if (!world_.entities.isAlive(parent)) {
      return {};
    }
    root_desc.transform.parent = world_.entities.getTransform(parent);
  }
  const EntityHandle root = world_.entities.create(root_desc);
  if (!root) {
    return {};
  }

  std::vector<EntityHandle> nodes;
  if (!models_.instantiate(path, {.parent = world_.entities.getTransform(root)}, nodes)) {
    world_.entities.destroy(root);
    // The model may have been imported before the nodes failed.
    models_.releaseUnused();
    return {};
  }
  return root;
}

void EditorScene::destroyEntity(EntityHandle entity) {
  world_.entities.destroy(entity);
  collectGarbage();
}

void EditorScene::setLocalTRS(
  EntityHandle entity, Vec3Arg position, QuatArg rotation, Vec3Arg scale) {
  if (world_.entities.isAlive(entity)) {
    world_.graph.setLocalTRS(
      world_.entities.getTransform(entity), position, rotation, scale);
  }
}

void EditorScene::select(EntityHandle entity) { selection_ = entity; }

EntityHandle EditorScene::getSelection() const {
  return world_.entities.isAlive(selection_) ? selection_ : EntityHandle{};
}

EntityHandle EditorScene::selectByInstanceId(uint32 instance_id) {
  const RenderProxyHandle proxy = world_.scene.getProxyByInstanceId(instance_id);
  selection_ = proxy ? world_.entities.findByTransform(world_.scene.getTransform(proxy))
                     : EntityHandle{};
  return selection_;
}

MaterialHandle EditorScene::getMaterial(EntityHandle entity, uint32 section) const {
  const RenderProxyHandle proxy = getProxy(entity);
  return proxy ? world_.scene.getMaterial(proxy, section) : MaterialHandle{};
}

MaterialHandle EditorScene::editMaterial(
  EntityHandle entity, uint32 section, const MaterialAsset& asset) {
  const RenderProxyHandle proxy = getProxy(entity);
  if (!proxy) {
    return {};
  }

  const MaterialHandle current = world_.scene.getMaterial(proxy, section);
  if (isOverride(current) && world_.materials.getRefCount(current) == 1) {
    world_.materials.update(current, asset);
    return current;
  }

  const MaterialHandle material = world_.materials.create(asset);
  overrides_.push_back(material);
  world_.scene.setMaterial(proxy, section, material);
  destroyOverrideIfUnused(current);
  return material;
}

void EditorScene::resetMaterial(EntityHandle entity, uint32 section) {
  const RenderProxyHandle proxy = getProxy(entity);
  if (!proxy) {
    return;
  }
  const MaterialHandle current = world_.scene.getMaterial(proxy, section);
  world_.scene.setMaterial(proxy, section, {});
  destroyOverrideIfUnused(current);
}

bool EditorScene::isOverride(MaterialHandle material) const {
  return material &&
         std::find(overrides_.begin(), overrides_.end(), material) != overrides_.end();
}

void EditorScene::collectGarbage() {
  std::erase_if(overrides_, [this](MaterialHandle material) {
    if (world_.materials.getRefCount(material) != 0) {
      return false;
    }
    world_.materials.destroy(material);
    return true;
  });
  models_.releaseUnused();
}

RenderProxyHandle EditorScene::getProxy(EntityHandle entity) const {
  return world_.entities.isAlive(entity) ? world_.entities.getRenderProxy(entity)
                                         : RenderProxyHandle{};
}

void EditorScene::destroyOverrideIfUnused(MaterialHandle material) {
  const auto it = std::find(overrides_.begin(), overrides_.end(), material);
  if (it != overrides_.end() && world_.materials.getRefCount(material) == 0) {
    world_.materials.destroy(material);
    overrides_.erase(it);
  }
}

}  // namespace amaryl

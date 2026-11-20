#include "amaryl/engine/scene/scene_loader.h"

#include <cmath>
#include <memory>

#include "amaryl/core/logger.h"

namespace amaryl {
namespace {

bool isCameraValid(const CameraAsset& asset) {
  if (asset.projection == ECameraProjection::Orthographic) {
    return asset.orthoHalfHeight > 0.0f && asset.nearZ >= 0.0f &&
           std::isfinite(asset.farZ) && asset.farZ > asset.nearZ;
  }
  return asset.fovY > 0.0f && asset.fovY < PI && asset.nearZ > 0.0f &&
         asset.farZ > asset.nearZ;
}

std::unique_ptr<Camera> createCamera(const CameraAsset& asset) {
  if (!isCameraValid(asset)) {
    ALOGW("Scene: camera '%s' has invalid parameters", asset.name.c_str());
    return nullptr;
  }
  auto camera = std::make_unique<Camera>();
  const float aspect = asset.aspect > 0.0f ? asset.aspect : camera->getAspect();
  if (asset.projection == ECameraProjection::Orthographic) {
    camera->setOrthographic(asset.orthoHalfHeight, aspect, asset.nearZ, asset.farZ);
  } else {
    camera->setPerspective(asset.fovY, aspect, asset.nearZ, asset.farZ);
  }
  return camera;
}

std::unique_ptr<Light> createLight(const LightAsset& asset) {
  if (asset.type != ELightType::Directional) {
    ALOGW("Scene: light '%s' has an unsupported type", asset.name.c_str());
    return nullptr;
  }
  auto light = std::make_unique<DirectionalLight>();
  light->setColor(asset.color);
  light->setIntensity(asset.intensity);
  return light;
}

bool isSceneValid(const SceneAsset& scene, const Model& model) {
  for (size_t i = 0; i < scene.nodes.size(); ++i) {
    const SceneNodeAsset& node = scene.nodes[i];
    if (node.parent >= static_cast<int32>(i) ||
        node.mesh >= static_cast<int32>(model.meshes.size()) ||
        node.camera >= static_cast<int32>(scene.cameras.size()) ||
        node.light >= static_cast<int32>(scene.lights.size())) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool addSceneNodes(const SceneAsset& scene, const Model& model, EntityManager& entities,
                   const SceneNodesDesc& desc, std::vector<EntityHandle>& out) {
  out.clear();
  if (!isSceneValid(scene, model)) {
    ALOGE("Scene: invalid scene asset");
    return false;
  }

  // Parents come first, their entities exist when the children are created.
  out.reserve(scene.nodes.size());
  for (const SceneNodeAsset& node : scene.nodes) {
    EntityDesc entity_desc;
    entity_desc.transform.parent =
        node.parent >= 0 ? entities.getTransform(out[node.parent]) : desc.parent;
    entity_desc.transform.position = node.position;
    entity_desc.transform.rotation = node.rotation;
    entity_desc.transform.scale = node.scale;
    entity_desc.transform.mobility = desc.mobility;
    if (node.mesh >= 0) {
      entity_desc.mesh = model.meshes[node.mesh];
    }

    const EntityHandle entity = entities.create(entity_desc);
    if (!entity) {
      // Destroying the added roots destroys every added node.
      for (size_t i = 0; i < out.size(); ++i) {
        if (scene.nodes[i].parent < 0) {
          entities.destroy(out[i]);
        }
      }
      out.clear();
      return false;
    }
    out.push_back(entity);

    if (node.camera >= 0) {
      entities.setCamera(entity, createCamera(scene.cameras[node.camera]));
    }
    if (node.light >= 0) {
      entities.setLight(entity, createLight(scene.lights[node.light]));
    }
  }
  return true;
}

}  // namespace amaryl

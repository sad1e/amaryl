#pragma once

#include <vector>

#include "amaryl/engine/asset/scene_asset.h"
#include "amaryl/engine/scene/entity.h"
#include "amaryl/engine/scene/model.h"

namespace amaryl {

struct SceneNodesDesc {
  // Optional, the scene roots become its children. Graph roots when empty.
  TransformHandle parent{};
  // Mobility of every added node, static nodes need a static parent.
  ETransformMobility mobility = ETransformMobility::Movable;
};

// Adds the nodes of `scene` to the scene graph of `entities`, one entity per node and
// without wrapper root, the existing hierarchy is left untouched. Mesh nodes get a render
// proxy using `model` (registered from the matching ModelAsset), camera nodes a Camera
// and directional light nodes a DirectionalLight. Unsupported lights and invalid cameras
// only keep their transform. On success `out[i]` is the entity of `scene.nodes[i]`, on
// failure every added entity is destroyed, `out` is cleared and false is returned.
ENGINE_API bool addSceneNodes(const SceneAsset& scene, const Model& model,
                              EntityManager& entities, const SceneNodesDesc& desc,
                              std::vector<EntityHandle>& out);

}  // namespace amaryl

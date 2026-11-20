#pragma once

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "amaryl/engine/asset/material_asset.h"
#include "amaryl/engine/asset/mesh_asset.h"
#include "amaryl/engine/scene/common.h"

namespace amaryl {

struct ModelMesh {
  // Null when the mesh failed to import.
  std::shared_ptr<const MeshAsset> asset;
  // Per section, index in `ModelAsset::materials` or -1 for the default material.
  std::vector<int32> sectionMaterials;
};

// Meshes and materials of an imported file, without scene structure.
struct ModelAsset {
  std::vector<ModelMesh> meshes;
  std::vector<MaterialAsset> materials;
};

struct CameraAsset {
  std::string name;
  ECameraProjection projection = ECameraProjection::Perspective;
  // Perspective, vertical field of view in radians.
  float fovY = 60.0f * DEG_TO_RAD;
  // Orthographic, half of the view height.
  float orthoHalfHeight = 1.0f;
  // Width / height, 0 when it follows the viewport.
  float aspect = 0.0f;
  float nearZ = 0.1f;
  // Infinite is only supported by perspective cameras.
  float farZ = std::numeric_limits<float>::infinity();
};

struct LightAsset {
  std::string name;
  ELightType type = ELightType::Directional;
  // Linear RGB.
  Vec3 color = Vec3::sOne();
  // Directional: lux, point and spot: candela.
  float intensity = 1.0f;
  // Point and spot, infinite when unbounded.
  float range = std::numeric_limits<float>::infinity();
  // Spot, radians.
  float innerConeAngle = 0.0f;
  float outerConeAngle = 0.25f * PI;
};

struct SceneNodeAsset {
  std::string name;
  // Index in `SceneAsset::nodes`, lower than the index of the node, -1 for roots.
  int32 parent = -1;
  Vec3 position = Vec3::sZero();
  Quat rotation = Quat::sIdentity();
  Vec3 scale = Vec3::sOne();
  // Index in `ModelAsset::meshes`, -1 without mesh or when the mesh failed.
  int32 mesh = -1;
  // Index in `SceneAsset::cameras`, -1 without camera.
  int32 camera = -1;
  // Index in `SceneAsset::lights`, -1 without light.
  int32 light = -1;
};

// Node hierarchy of an imported file. Cameras and lights look down their node's -Z axis.
struct SceneAsset {
  // Parents before children.
  std::vector<SceneNodeAsset> nodes;
  std::vector<CameraAsset> cameras;
  std::vector<LightAsset> lights;
};

}  // namespace amaryl

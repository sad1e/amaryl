#pragma once

#include "amaryl/core/math/math.h"
#include "amaryl/engine/common.h"

namespace amaryl {

DECLARE_AMARYL_HANDLE(Entity);
DECLARE_AMARYL_HANDLE(Transform);
DECLARE_AMARYL_HANDLE(Mesh);
DECLARE_AMARYL_HANDLE(Material);
DECLARE_AMARYL_HANDLE(Skeleton);
DECLARE_AMARYL_HANDLE(PhysicalBody);
DECLARE_AMARYL_HANDLE(RenderProxy);
DECLARE_AMARYL_HANDLE(AnimationInstance);
DECLARE_AMARYL_HANDLE(RenderScene);
DECLARE_AMARYL_HANDLE(PhysicalWorld);

enum class ETransformMobility : uint8 {
  // Written once at creation, later writes assert. Must not have a movable parent.
  Static,
  Movable,
};

enum class EAttachRule : uint8 {
  KeepWorld,
  KeepLocal,
};

enum class ETransformDestroyMode : uint8 {
  Recursive,
  // Children are attached to the destroyed node's parent and keep their world transform.
  ReparentChildren,
};

enum class ECameraProjection : uint8 {
  Perspective,
  Orthographic,
};

enum class ELightType : uint8 {
  Directional,
  Point,
  Spot,
};

struct TransformDesc {
  TransformHandle parent{};
  Vec3 position = Vec3::sZero();
  Quat rotation = Quat::sIdentity();
  Vec3 scale = Vec3::sOne();
  ETransformMobility mobility = ETransformMobility::Movable;
};

}  // namespace amaryl

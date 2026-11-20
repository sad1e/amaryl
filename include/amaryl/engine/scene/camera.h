#pragma once

#include <limits>

#include "amaryl/engine/scene/common.h"

namespace amaryl {

// Right handed, looks down the -Z axis of its world transform with +Y up. The projection
// maps the view depth to [0, 1] from near to far (Vulkan clip space, +Y up).
struct ENGINE_API Camera {
  static constexpr float kInfiniteFar = std::numeric_limits<float>::infinity();

  Camera() = default;
  virtual ~Camera();

  // `fovY` is the vertical field of view in radians, `nearZ` must be positive and `farZ`
  // may be kInfiniteFar.
  void setPerspective(float fovY, float aspect, float nearZ, float farZ = kInfiniteFar);
  // `halfHeight` is half of the view height in world units, `farZ` must be finite.
  void setOrthographic(float halfHeight, float aspect, float nearZ, float farZ);
  // Width / height, usually updated when the viewport is resized.
  void setAspect(float aspect);
  void setFovY(float fovY);
  void setOrthoHalfHeight(float halfHeight);
  void setClipPlanes(float nearZ, float farZ);

  ECameraProjection getProjectionType() const { return projection_; }
  float getFovY() const { return fov_y_; }
  float getOrthoHalfHeight() const { return ortho_half_height_; }
  float getAspect() const { return aspect_; }
  float getNear() const { return near_; }
  float getFar() const { return far_; }

  // The scale of `world` is removed.
  void setWorldTransform(Mat44Arg world);
  // Rotation and translation only.
  const Mat44& getWorldTransform() const { return world_; }
  Vec3 getPosition() const { return world_.GetTranslation(); }
  Vec3 getForward() const { return -world_.GetAxisZ(); }
  Vec3 getUp() const { return world_.GetAxisY(); }
  Vec3 getRight() const { return world_.GetAxisX(); }

  // World to view.
  const Mat44& getView() const { return view_; }
  // View to clip, may be overridden for jittered or oblique projections.
  virtual Mat44 getProjection() const;
  Mat44 getViewProjection() const { return getProjection() * view_; }

 private:
  ECameraProjection projection_ = ECameraProjection::Perspective;
  float fov_y_ = 60.0f * DEG_TO_RAD;
  float ortho_half_height_ = 1.0f;
  float aspect_ = 16.0f / 9.0f;
  float near_ = 0.1f;
  float far_ = kInfiniteFar;
  Mat44 world_ = Mat44::sIdentity();
  Mat44 view_ = Mat44::sIdentity();
};

}  // namespace amaryl

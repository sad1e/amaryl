#include "amaryl/engine/scene/camera.h"

#include <cmath>

#include "amaryl/core/assert.h"

namespace amaryl {

Camera::~Camera() = default;

void Camera::setPerspective(float fovY, float aspect, float nearZ, float farZ) {
  ASSERT(nearZ > 0.0f, "Camera: perspective near must be positive");
  projection_ = ECameraProjection::Perspective;
  setFovY(fovY);
  setAspect(aspect);
  setClipPlanes(nearZ, farZ);
}

void Camera::setOrthographic(float halfHeight, float aspect, float nearZ, float farZ) {
  projection_ = ECameraProjection::Orthographic;
  setOrthoHalfHeight(halfHeight);
  setAspect(aspect);
  setClipPlanes(nearZ, farZ);
}

void Camera::setAspect(float aspect) {
  ASSERT(aspect > 0.0f, "Camera: invalid aspect");
  aspect_ = aspect;
}

void Camera::setFovY(float fovY) {
  ASSERT(fovY > 0.0f && fovY < PI, "Camera: invalid field of view");
  fov_y_ = fovY;
}

void Camera::setOrthoHalfHeight(float halfHeight) {
  ASSERT(halfHeight > 0.0f, "Camera: invalid orthographic height");
  ortho_half_height_ = halfHeight;
}

void Camera::setClipPlanes(float nearZ, float farZ) {
  ASSERT(nearZ >= 0.0f && farZ > nearZ, "Camera: invalid clip planes");
  near_ = nearZ;
  far_ = farZ;
}

void Camera::setWorldTransform(Mat44Arg world) {
  Vec3 scale;
  world_ = world.Decompose(scale);
  view_ = world_.InversedRotationTranslation();
}

Mat44 Camera::getProjection() const {
  if (projection_ == ECameraProjection::Orthographic) {
    ASSERT(std::isfinite(far_), "Camera: orthographic projection needs a finite far");
    const float inv_depth = 1.0f / (far_ - near_);
    return Mat44(Vec4(1.0f / (ortho_half_height_ * aspect_), 0.0f, 0.0f, 0.0f),
                 Vec4(0.0f, 1.0f / ortho_half_height_, 0.0f, 0.0f),
                 Vec4(0.0f, 0.0f, -inv_depth, 0.0f),
                 Vec4(0.0f, 0.0f, -near_ * inv_depth, 1.0f));
  }

  ASSERT(near_ > 0.0f, "Camera: perspective near must be positive");
  if (std::isfinite(far_)) {
    return Mat44::sPerspective(fov_y_, aspect_, near_, far_);
  }
  // Limit of Mat44::sPerspective when far goes to infinity.
  const float height = 1.0f / Tan(0.5f * fov_y_);
  return Mat44(Vec4(height / aspect_, 0.0f, 0.0f, 0.0f), Vec4(0.0f, height, 0.0f, 0.0f),
               Vec4(0.0f, 0.0f, -1.0f, -1.0f), Vec4(0.0f, 0.0f, -near_, 0.0f));
}

}  // namespace amaryl

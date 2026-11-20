#pragma once

#include "amaryl/engine/scene/common.h"

namespace amaryl {

// Base of the light components, lights shine down the -Z axis of their world transform.
struct ENGINE_API Light {
  virtual ~Light();

  ELightType getType() const { return type_; }

  // Linear RGB.
  void setColor(Vec3Arg color) { color_ = color; }
  Vec3 getColor() const { return color_; }
  // Directional: lux, point and spot: candela.
  void setIntensity(float intensity) { intensity_ = intensity; }
  float getIntensity() const { return intensity_; }
  void setCastShadow(bool castShadow) { cast_shadow_ = castShadow; }
  bool getCastShadow() const { return cast_shadow_; }

  // The scale of `world` is removed.
  void setWorldTransform(Mat44Arg world);
  // Rotation and translation only.
  const Mat44& getWorldTransform() const { return world_; }
  Vec3 getPosition() const { return world_.GetTranslation(); }
  // Direction the light travels.
  Vec3 getDirection() const { return -world_.GetAxisZ(); }

 protected:
  explicit Light(ELightType type) : type_(type) {}

 private:
  ELightType type_;
  Vec3 color_ = Vec3::sOne();
  float intensity_ = 1.0f;
  bool cast_shadow_ = true;
  Mat44 world_ = Mat44::sIdentity();
};

// Infinitely far light with parallel rays, only the rotation of the transform is used.
struct ENGINE_API DirectionalLight final : Light {
  DirectionalLight() : Light(ELightType::Directional) {}
  ~DirectionalLight() override;
};

}  // namespace amaryl

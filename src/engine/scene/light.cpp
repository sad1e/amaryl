#include "amaryl/engine/scene/light.h"

namespace amaryl {

Light::~Light() = default;

void Light::setWorldTransform(Mat44Arg world) {
  Vec3 scale;
  world_ = world.Decompose(scale);
}

DirectionalLight::~DirectionalLight() = default;

}  // namespace amaryl

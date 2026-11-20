#include "amaryl/engine/scene/camera.h"

#include <cmath>
#include <memory>

#include "amaryl/engine/scene/light.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

// Clip space divided by w.
Vec3 project(Mat44Arg projection, Vec3Arg point) {
  const Vec4 clip = projection * Vec4(point, 1.0f);
  return Vec3(clip) / clip.GetW();
}

bool isNear(Vec3Arg a, Vec3Arg b) { return a.IsClose(b, 1e-8f); }

struct JitteredCamera final : Camera {
  Vec3 jitter = Vec3::sZero();

  Mat44 getProjection() const override {
    return Mat44::sTranslation(jitter) * Camera::getProjection();
  }
};

}  // namespace

int main() {
  "perspective camera maps the frustum to vulkan clip space"_test = [] {
    Camera camera;
    expect(camera.getProjectionType() == ECameraProjection::Perspective);
    const float fov = 0.5f * PI;
    camera.setPerspective(fov, 2.0f, 0.5f, 100.0f);
    expect(camera.getFovY() == fov && camera.getAspect() == 2.0f);
    expect(camera.getNear() == 0.5f && camera.getFar() == 100.0f);

    const Mat44 projection = camera.getProjection();
    expect(isNear(project(projection, Vec3(0, 0, -0.5f)), Vec3(0, 0, 0)));
    expect(isNear(project(projection, Vec3(0, 0, -100)), Vec3(0, 0, 1)));
    // tan(fov / 2) = 1, the right edge is at aspect * depth.
    const Vec3 corner = project(projection, Vec3(20, 10, -10));
    expect(isNear(Vec3(corner.GetX(), corner.GetY(), 0), Vec3(1, 1, 0)));
    expect(corner.GetZ() > 0.0f && corner.GetZ() < 1.0f);
    expect((projection * Vec4(0, 0, -10, 1)).GetW() == 10.0f);

    // Infinite far plane.
    camera.setClipPlanes(0.5f, Camera::kInfiniteFar);
    const Mat44 infinite = camera.getProjection();
    expect(isNear(project(infinite, Vec3(0, 0, -0.5f)), Vec3(0, 0, 0)));
    expect(isNear(project(infinite, Vec3(-20, -10, -10)), Vec3(-1, -1, 0.95f)));
    expect(project(infinite, Vec3(0, 0, -1e6f)).GetZ() < 1.0f);
    expect(std::abs(project(infinite, Vec3(0, 0, -1e6f)).GetZ() - 1.0f) < 1e-5f);
  };

  "orthographic camera maps the box to vulkan clip space"_test = [] {
    Camera camera;
    camera.setOrthographic(2.0f, 1.5f, 0.0f, 50.0f);
    expect(camera.getProjectionType() == ECameraProjection::Orthographic);
    expect(camera.getOrthoHalfHeight() == 2.0f);

    const Mat44 projection = camera.getProjection();
    expect(isNear(project(projection, Vec3(3, 2, 0)), Vec3(1, 1, 0)));
    expect(isNear(project(projection, Vec3(-3, -2, -50)), Vec3(-1, -1, 1)));
    expect(isNear(project(projection, Vec3(0, 0, -25)), Vec3(0, 0, 0.5f)));
    expect((projection * Vec4(1, 2, -3, 1)).GetW() == 1.0f);

    camera.setOrthoHalfHeight(4.0f);
    camera.setAspect(1.0f);
    expect(isNear(project(camera.getProjection(), Vec3(4, -4, 0)), Vec3(1, -1, 0)));
  };

  "camera view follows the world transform without scale"_test = [] {
    Camera camera;
    expect(camera.getView() == Mat44::sIdentity());
    expect(isNear(camera.getForward(), Vec3(0, 0, -1)));

    // Looks down -X from (0, 0, 5).
    const Quat rotation = Quat::sRotation(Vec3::sAxisY(), 0.5f * PI);
    camera.setWorldTransform(Mat44::sRotationTranslation(rotation, Vec3(0, 0, 5)) *
                             Mat44::sScale(3.0f));
    expect(isNear(camera.getPosition(), Vec3(0, 0, 5)));
    expect(isNear(camera.getForward(), Vec3(-1, 0, 0)));
    expect(isNear(camera.getUp(), Vec3(0, 1, 0)));
    expect(isNear(camera.getRight(), Vec3(0, 0, -1)));
    expect(isNear(camera.getView() * Vec3(-3, 0, 5), Vec3(0, 0, -3)));
    expect(isNear(camera.getView() * Vec3(-3, 1, 4), Vec3(1, 1, -3)));

    camera.setPerspective(0.5f * PI, 1.0f, 1.0f, 10.0f);
    const Mat44 expected = camera.getProjection() * camera.getView();
    expect(camera.getViewProjection().IsClose(expected));
    // Same as looking at the target.
    const Mat44 look_at = Mat44::sLookAt(Vec3(0, 0, 5), Vec3(-1, 0, 5), Vec3::sAxisY());
    expect(camera.getView().IsClose(look_at, 1e-10f));
  };

  "cameras can be extended"_test = [] {
    JitteredCamera jittered;
    jittered.jitter = Vec3(0.1f, -0.2f, 0.0f);
    const Camera& camera = jittered;
    const Vec3 center = project(camera.getProjection(), Vec3(0, 0, -1));
    expect(isNear(Vec3(center.GetX(), center.GetY(), 0), Vec3(0.1f, -0.2f, 0)));
    expect(camera.getViewProjection().IsClose(camera.getProjection()));
  };

  "directional light shines down its -Z axis"_test = [] {
    const std::unique_ptr<Light> light = std::make_unique<DirectionalLight>();
    expect(light->getType() == ELightType::Directional);
    expect(light->getColor() == Vec3::sOne() && light->getIntensity() == 1.0f);
    expect(light->getCastShadow());
    expect(isNear(light->getDirection(), Vec3(0, 0, -1)));

    light->setColor(Vec3(1, 0.5f, 0));
    light->setIntensity(10.0f);
    light->setCastShadow(false);
    // Pointing down.
    light->setWorldTransform(
        Mat44::sRotationTranslation(Quat::sRotation(Vec3::sAxisX(), -0.5f * PI),
                                    Vec3(1, 2, 3)) *
        Mat44::sScale(Vec3(2, 3, 4)));
    expect(isNear(light->getDirection(), Vec3(0, -1, 0)));
    expect(isNear(light->getPosition(), Vec3(1, 2, 3)));
    expect(light->getColor() == Vec3(1, 0.5f, 0) && light->getIntensity() == 10.0f);
    expect(!light->getCastShadow());
  };

  return 0;
}

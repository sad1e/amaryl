#include "amaryl/core/math/rotator.h"

#include <cmath>

#include "amaryl/core/math/quat.h"

namespace amaryl {

Rotator::Rotator(Vec4 inRHS) : value_(inRHS) {}

Rotator::Rotator(float inPitch, float inRoll, float inYaw)
    : value_(inPitch, inRoll, inYaw, 0.0) {}

Rotator::Rotator(const Quat& inQ) {
  float x = inQ.GetX(), y = inQ.GetY(), z = inQ.GetZ(), w = inQ.GetW();
  // Intrinsic XYZ: sin(Roll) = R[0][2] = 2*(x*z + y*w)
  float sinRoll = 2.0f * (x * z + y * w);
  float pitch, roll, yaw;
  constexpr float SINGULARITY_THRESHOLD = 0.9999999f;
  if (sinRoll > SINGULARITY_THRESHOLD) {
    roll = 90.0f;
    pitch = ATan2(2.0f * (x * y + z * w), 1.0f - 2.0f * (y * y + z * z)) * RAD_TO_DEG;
    yaw = 0.0f;
  } else if (sinRoll < -SINGULARITY_THRESHOLD) {
    roll = -90.0f;
    pitch = ATan2(2.0f * (x * y + z * w), 1.0f - 2.0f * (y * y + z * z)) * RAD_TO_DEG;
    yaw = 0.0f;
  } else {
    roll = ASin(sinRoll) * RAD_TO_DEG;
    pitch = ATan2(2.0f * (w * x - y * z), 1.0f - 2.0f * (x * x + y * y)) * RAD_TO_DEG;
    yaw = ATan2(2.0f * (w * z - x * y), 1.0f - 2.0f * (y * y + z * z)) * RAD_TO_DEG;
  }
  value_ = Vec4(pitch, roll, yaw, 0.0f);
}

Rotator Rotator::MakeFromEuler(const Vec3& inVec3) {
  return Rotator(inVec3.GetX(), inVec3.GetY(), inVec3.GetZ());
}

Quat Rotator::GetQuat() const {
  // TODO: the fmod can be vertorized.
  const float PitchNoWinding = std::fmod(value_[0], 360.0f);
  const float RollNoWinding = std::fmod(value_[1], 360.0f);
  const float YawNoWinding = std::fmod(value_[2], 360.0f);

  Vec4 RotatorNoWinding(PitchNoWinding, RollNoWinding, YawNoWinding, 0.0);
  Vec4 HalfAngles = RotatorNoWinding * DEG_TO_RAD_HALF;

  Vec4 SR, CR;
  HalfAngles.SinCos(SR, CR);

  Quat Q;
  // xyz (intrinsic X-Y-Z)
  Q.SetX(CR.GetY() * CR.GetZ() * SR.GetX() + SR.GetY() * SR.GetZ() * CR.GetX());
  Q.SetY(SR.GetY() * CR.GetZ() * CR.GetX() - CR.GetY() * SR.GetZ() * SR.GetX());
  Q.SetZ(CR.GetY() * SR.GetZ() * CR.GetX() + SR.GetY() * CR.GetZ() * SR.GetX());
  Q.SetW(CR.GetY() * CR.GetZ() * CR.GetX() - SR.GetY() * SR.GetZ() * SR.GetX());

  return Q;
}

Vec3 Rotator::GetEulers() const {
  return Vec3(value_.GetX(), value_.GetY(), value_.GetZ());
}

}  // namespace amaryl

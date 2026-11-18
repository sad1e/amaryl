#pragma once

#include "amaryl/core/amaryl.h"
#include "amaryl/core/math/math_types.h"
#include "amaryl/core/math/vec3.h"

namespace amaryl {

/**
 * Using same coordinate system as blender.
 *   X: Right
 *   Y: Forward
 *   Z: Up
 * Rotator:
 *   X: Pitch, Rotation around the right axis (around X axis), Looking up and
 * down (0=Straight Ahead, +Up, -Down) Y: Roll,  Rotation around the forward
 * axis (around Y axis), Tilting your head (0=Straight, +Clockwise, -CCW) Z:
 * Yaw, 	 Rotation around the up axis (around Z axis), Running in circles
 * (0=East, +North, -South)
 */
class [[nodiscard]] alignas(VECTOR_ALIGNMENT) Rotator {
 public:
  AMARYL_MATH_OVERRIDE_NEW_DELETE

  using Type = Vec4::Type;

  Rotator() = default;
  Rotator(const Rotator&) = default;
  Rotator& operator=(const Rotator& inRHS) = default;
  CORE_API explicit Rotator(Vec4 inRHS);
  FORCE_INLINE Rotator(Type inRHS) : value_(inRHS) {}
  CORE_API explicit Rotator(const Quat& inQ);

  CORE_API Rotator(float inPitch, float inRoll, float inYaw);

  CORE_API static Rotator MakeFromEuler(const Vec3& inVec3);

  FORCE_INLINE void SetPitch(float inPitch) { value_.SetX(inPitch); }
  FORCE_INLINE void SetYaw(float inYaw) { value_.SetZ(inYaw); }
  FORCE_INLINE void SetRoll(float inRoll) { value_.SetY(inRoll); }

  FORCE_INLINE Rotator operator+(const Rotator& inR) const {
    return Rotator(value_ + inR.value_);
  }
  FORCE_INLINE Rotator& operator+=(const Rotator& inR) {
    value_ += inR.value_;
    return *this;
  }
  FORCE_INLINE Rotator operator-(const Rotator& inR) const {
    return Rotator(value_ - inR.value_);
  }
  FORCE_INLINE Rotator& operator-=(const Rotator& inR) {
    value_ -= inR.value_;
    return *this;
  }
  FORCE_INLINE Rotator operator*(float inScale) const {
    return Rotator(value_ * inScale);
  }
  FORCE_INLINE Rotator& operator*=(float inScale) {
    value_ *= inScale;
    return *this;
  }

  FORCE_INLINE bool operator==(const Rotator& inRHS) const {
    return value_ == inRHS.value_;
  }
  FORCE_INLINE bool operator!=(const Rotator& inRHS) const {
    return !(value_ == inRHS.value_);
  }

  FORCE_INLINE float GetPitch() const { return value_.GetX(); }
  FORCE_INLINE float GetRoll() const { return value_.GetY(); }
  FORCE_INLINE float GetYaw() const { return value_.GetZ(); }

  /// Covnert Rotator(Pitch/Roll/Yaw system) to quaternion.
  CORE_API Quat GetQuat() const;

  CORE_API Vec3 GetEulers() const;

 private:
  Vec4 value_;
};

}  // namespace amaryl

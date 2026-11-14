#include "amaryl/core/math/quat.h"

namespace amaryl {

Quat Quat::sFromTo(Vec3Arg inFrom, Vec3Arg inTo) {
  /*
      Uses (inFrom = v1, inTo = v2):

      angle = arcos(v1 . v2 / |v1||v2|)
      axis = normalize(v1 x v2)

      Quaternion is then:

      s = sin(angle / 2)
      x = axis.x * s
      y = axis.y * s
      z = axis.z * s
      w = cos(angle / 2)

      Using identities:

      sin(2 * a) = 2 * sin(a) * cos(a)
      cos(2 * a) = cos(a)^2 - sin(a)^2
      sin(a)^2 + cos(a)^2 = 1

      This reduces to:

      x = (v1 x v2).x
      y = (v1 x v2).y
      z = (v1 x v2).z
      w = |v1||v2| + v1 . v2

   which then needs to be normalized because the whole equation was multiplied by 2
   cos(angle / 2)
    */

  float len_v1_v2 = sqrt(inFrom.LengthSq() * inTo.LengthSq());
  float w = len_v1_v2 + inFrom.Dot(inTo);

  if (w == 0.0f) {
    if (len_v1_v2 == 0.0f) {
      // If either of the vectors has zero length, there is no rotation and we
      // return identity
      return Quat::sIdentity();
    } else {
      // If vectors are perpendicular, take one of the many 180 degree rotations
      // that exist
      return Quat(Vec4(inFrom.GetNormalizedPerpendicular(), 0));
    }
  }

  Vec3 v = inFrom.Cross(inTo);
  return Quat(Vec4(v, w)).Normalized();
}

Quat Quat::sEulerAngles(Vec3Arg inAngles) {
  Vec4 half(0.5f * inAngles);
  Vec4 s, c;
  half.SinCos(s, c);
  float cx = c.GetX(), sx = s.GetX();
  float cy = c.GetY(), sy = s.GetY();
  float cz = c.GetZ(), sz = s.GetZ();
  // Q = Qx * Qy * Qz (intrinsic XYZ)
  return Quat(sx * cy * cz + cx * sy * sz, cx * sy * cz - sx * cy * sz,
              cx * cy * sz + sx * sy * cz, cx * cy * cz - sx * sy * sz);
}

Vec3 Quat::GetEulerAngles() const {
  // Intrinsic XYZ: sin(Y) = R[0][2] = 2*(xz + yw)
  float sinY = 2.0f * (GetX() * GetZ() + GetY() * GetW());
  sinY = sinY > 1.0f ? 1.0f : (sinY < -1.0f ? -1.0f : sinY);
  float ex = ATan2(2.0f * (GetW() * GetX() - GetY() * GetZ()),
                   1.0f - 2.0f * (GetX() * GetX() + GetY() * GetY()));
  float ey = ASin(sinY);
  float ez = ATan2(2.0f * (GetW() * GetZ() - GetX() * GetY()),
                   1.0f - 2.0f * (GetY() * GetY() + GetZ() * GetZ()));
  return Vec3(ex, ey, ez);
}

}  // namespace amaryl

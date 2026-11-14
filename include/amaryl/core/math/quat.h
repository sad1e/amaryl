#pragma once

#include "amaryl/core/math/vec3.h"
#include "amaryl/core/math/vec4.h"

namespace amaryl {

/// Quaternion class, quaternions are 4 dimensional vectors which can describe
/// rotations in 3 dimensional space if their length is 1.
///
/// They are written as:
///
/// \f$q = w + x \: i + y \: j + z \: k\f$
///
/// or in vector notation:
///
/// \f$q = [w, v] = [w, x, y, z]\f$
///
/// Where:
///
/// w = the real part
/// v = the imaginary part, (x, y, z)
///
/// Note that we store the quaternion in a Vec4 as [x, y, z, w] because that
/// makes it easy to extract the rotation axis of the quaternion:
///
/// q = [cos(angle / 2), sin(angle / 2) * rotation_axis]
class [[nodiscard]] alignas(VECTOR_ALIGNMENT) Quat {
 public:
  AMARYL_MATH_OVERRIDE_NEW_DELETE

  ///@name Constructors
  ///@{
  inline Quat() = default;  ///< Intentionally not initialized for performance reasons
  Quat(const Quat& inRHS) = default;
  Quat& operator=(const Quat& inRHS) = default;
  inline Quat(float inX, float inY, float inZ, float inW) : mValue(inX, inY, inZ, inW) {}
  inline explicit Quat(Vec4Arg inV) : mValue(inV) {}
  ///@}

  /// Default quaternion.
  static FORCE_INLINE Quat sDefault() { return Quat(0.0f, 0.0f, 0.0f, 1.0f); }

  ///@name Tests
  ///@{

  /// Check if two quaternions are exactly equal
  inline bool operator==(QuatArg inRHS) const { return mValue == inRHS.mValue; }

  /// Check if two quaternions are different
  inline bool operator!=(QuatArg inRHS) const { return mValue != inRHS.mValue; }

  /// If this quaternion is close to inRHS. Note that q and -q represent the
  /// same rotation, this is not checked here.
  inline bool IsClose(QuatArg inRHS, float inMaxDistSq = 1.0e-12f) const {
    return mValue.IsClose(inRHS.mValue, inMaxDistSq);
  }

  /// If the length of this quaternion is 1 +/- inTolerance
  inline bool IsNormalized(float inTolerance = 1.0e-5f) const {
    return mValue.IsNormalized(inTolerance);
  }

  /// If any component of this quaternion is a NaN (not a number)
  inline bool IsNaN() const { return mValue.IsNaN(); }

  ///@}
  ///@name Get components
  ///@{

  /// Get X component (imaginary part i)
  FORCE_INLINE float GetX() const { return mValue.GetX(); }

  /// Get Y component (imaginary part j)
  FORCE_INLINE float GetY() const { return mValue.GetY(); }

  /// Get Z component (imaginary part k)
  FORCE_INLINE float GetZ() const { return mValue.GetZ(); }

  /// Get W component (real part)
  FORCE_INLINE float GetW() const { return mValue.GetW(); }

  /// Get the imaginary part of the quaternion
  FORCE_INLINE Vec3 GetXYZ() const { return Vec3(mValue); }

  /// Get the quaternion as a Vec4
  FORCE_INLINE Vec4 GetXYZW() const { return mValue; }

  /// Set individual components
  FORCE_INLINE void SetX(float inX) { mValue.SetX(inX); }
  FORCE_INLINE void SetY(float inY) { mValue.SetY(inY); }
  FORCE_INLINE void SetZ(float inZ) { mValue.SetZ(inZ); }
  FORCE_INLINE void SetW(float inW) { mValue.SetW(inW); }

  /// Set all components
  FORCE_INLINE void Set(float inX, float inY, float inZ, float inW) {
    mValue.Set(inX, inY, inZ, inW);
  }

  ///@}
  ///@name Default quaternions
  ///@{

  /// @return [0, 0, 0, 0]
  FORCE_INLINE static Quat sZero() { return Quat(Vec4::sZero()); }

  /// @return [1, 0, 0, 0] (or in storage format Quat(0, 0, 0, 1))
  FORCE_INLINE static Quat sIdentity() { return Quat(0, 0, 0, 1); }

  ///@}

  /// Rotation from axis and angle
  FORCE_INLINE static Quat sRotation(Vec3Arg inAxis, float inAngle);

  /// Get axis and angle that represents this quaternion, outAngle will always
  /// be in the range \f$[0, \pi]\f$
  FORCE_INLINE void GetAxisAngle(Vec3& outAxis, float& outAngle) const;

  /// Create quaternion that rotates a vector from the direction of inFrom to
  /// the direction of inTo along the shortest path
  /// @see
  /// https://www.euclideanspace.com/maths/algebra/vectors/angleBetween/index.htm
  static Quat sFromTo(Vec3Arg inFrom, Vec3Arg inTo);

  /// Random unit quaternion
  template <class Random>
  inline static Quat sRandom(Random& inRandom);

  /// Conversion from Euler angles. Rotation order is intrinsic X-Y-Z
  /// (RotX * RotY * RotZ). Angles in radians.
  static Quat sEulerAngles(Vec3Arg inAngles);

  /// Conversion to Euler angles. Rotation order is intrinsic X-Y-Z
  /// (RotX * RotY * RotZ). Angles in radians.
  Vec3 GetEulerAngles() const;

  ///@name Length / normalization operations
  ///@{

  /// Squared length of quaternion.
  /// @return Squared length of quaternion (\f$|v|^2\f$)
  FORCE_INLINE float LengthSq() const { return mValue.LengthSq(); }

  /// Length of quaternion.
  /// @return Length of quaternion (\f$|v|\f$)
  FORCE_INLINE float Length() const { return mValue.Length(); }

  /// Normalize the quaternion (make it length 1)
  FORCE_INLINE Quat Normalized() const { return Quat(mValue.Normalized()); }

  ///@}
  ///@name Additions / multiplications
  ///@{

  FORCE_INLINE void operator+=(QuatArg inRHS) { mValue += inRHS.mValue; }
  FORCE_INLINE void operator-=(QuatArg inRHS) { mValue -= inRHS.mValue; }
  FORCE_INLINE void operator*=(float inValue) { mValue *= inValue; }
  FORCE_INLINE void operator/=(float inValue) { mValue /= inValue; }
  FORCE_INLINE Quat operator-() const { return Quat(-mValue); }
  FORCE_INLINE Quat operator+(QuatArg inRHS) const { return Quat(mValue + inRHS.mValue); }
  FORCE_INLINE Quat operator-(QuatArg inRHS) const { return Quat(mValue - inRHS.mValue); }
  FORCE_INLINE Quat operator*(QuatArg inRHS) const;
  FORCE_INLINE Quat operator*(float inValue) const { return Quat(mValue * inValue); }
  inline friend Quat operator*(float inValue, QuatArg inRHS) {
    return Quat(inRHS.mValue * inValue);
  }
  FORCE_INLINE Quat operator/(float inValue) const { return Quat(mValue / inValue); }

  ///@}

  /// Rotate a vector by this quaternion
  FORCE_INLINE Vec3 operator*(Vec3Arg inValue) const;

  /// Rotate a vector by the inverse of this quaternion
  FORCE_INLINE Vec3 InverseRotate(Vec3Arg inValue) const;

  /// Rotate a the vector (1, 0, 0) with this quaternion
  FORCE_INLINE Vec3 RotateAxisX() const;

  /// Rotate a the vector (0, 1, 0) with this quaternion
  FORCE_INLINE Vec3 RotateAxisY() const;

  /// Rotate a the vector (0, 0, 1) with this quaternion
  FORCE_INLINE Vec3 RotateAxisZ() const;

  /// Dot product
  FORCE_INLINE float Dot(QuatArg inRHS) const { return mValue.Dot(inRHS.mValue); }

  /// The conjugate [w, -x, -y, -z] is the same as the inverse for unit
  /// quaternions
  FORCE_INLINE Quat Conjugated() const;

  /// Get inverse quaternion
  FORCE_INLINE Quat Inversed() const { return Conjugated() / Length(); }

  /// Ensures that the W component is positive by negating the entire quaternion
  /// if it is not. This is useful when you want to store a quaternion as a 3
  /// vector by discarding W and reconstructing it as sqrt(1 - x^2 - y^2 - z^2).
  FORCE_INLINE Quat EnsureWPositive() const;

  /// Get a quaternion that is perpendicular to this quaternion
  FORCE_INLINE Quat GetPerpendicular() const {
    return Quat(Vec4(1, -1, 1, -1) *
                mValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>());
  }

  /// Get rotation angle around inAxis (uses Swing Twist Decomposition to get
  /// the twist quaternion and uses q(axis, angle) = [cos(angle / 2), axis *
  /// sin(angle / 2)])
  FORCE_INLINE float GetRotationAngle(Vec3Arg inAxis) const;

  /// Swing Twist Decomposition: any quaternion can be split up as:
  ///
  /// \f[q = q_{swing} \: q_{twist}\f]
  ///
  /// where \f$q_{twist}\f$ rotates only around axis v.
  ///
  /// \f$q_{twist}\f$ is:
  ///
  /// \f[q_{twist} = \frac{[q_w, q_{ijk} \cdot v \: v]}{\left|[q_w, q_{ijk}
  /// \cdot v \: v]\right|}\f]
  ///
  /// where q_w is the real part of the quaternion and q_i the imaginary part (a
  /// 3 vector).
  ///
  /// The swing can then be calculated as:
  ///
  /// \f[q_{swing} = q \: q_{twist}^* \f]
  ///
  /// Where \f$q_{twist}^*\f$ = complex conjugate of \f$q_{twist}\f$
  FORCE_INLINE Quat GetTwist(Vec3Arg inAxis) const;

  /// Decomposes quaternion into swing and twist component:
  ///
  /// \f$q = q_{swing} \: q_{twist}\f$
  ///
  /// where \f$q_{swing} \: \hat{x} = q_{twist} \: \hat{y} = q_{twist} \:
  /// \hat{z} = 0\f$
  ///
  /// In other words:
  ///
  /// - \f$q_{twist}\f$ only rotates around the X-axis.
  /// - \f$q_{swing}\f$ only rotates around the Y and Z-axis.
  ///
  /// @see Gino van den Bergen - Rotational Joint Limits in Quaternion Space -
  /// GDC 2016
  FORCE_INLINE void GetSwingTwist(Quat& outSwing, Quat& outTwist) const;

  /// Linear interpolation between two quaternions (for small steps).
  /// @param inFraction is in the range [0, 1]
  /// @param inDestination The destination quaternion
  /// @return (1 - inFraction) * this + fraction * inDestination
  FORCE_INLINE Quat LERP(QuatArg inDestination, float inFraction) const;

  /// Spherical linear interpolation between two quaternions.
  /// @param inFraction is in the range [0, 1]
  /// @param inDestination The destination quaternion
  /// @return When fraction is zero this quaternion is returned, when fraction
  /// is 1 inDestination is returned. When fraction is between 0 and 1 an
  /// interpolation along the shortest path is returned.
  FORCE_INLINE Quat SLERP(QuatArg inDestination, float inFraction) const;

  /// Load 3 floats from memory (X, Y and Z component and then calculates W)
  /// reads 32 bits extra which it doesn't use
  static FORCE_INLINE Quat sLoadFloat3Unsafe(const float3& inV);

  /// Store 3 as floats to memory (X, Y and Z component)
  FORCE_INLINE void StoreFloat3(float3* outV) const;

  /// 4 vector that stores [x, y, z, w] parts of the quaternion
  Vec4 mValue;
};

static_assert(std::is_trivial<Quat>(), "Is supposed to be a trivial type!");

}  // namespace amaryl

namespace amaryl {

Quat Quat::operator*(QuatArg inRHS) const {
#if AMARYL_USE_SSE4_1
  // Taken from:
  // http://momchil-velikov.blogspot.nl/2013/10/fast-sse-quternion-multiplication.html
  __m128 abcd = mValue.mValue;
  __m128 xyzw = inRHS.mValue.mValue;

  __m128 t0 = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(3, 3, 3, 3));
  __m128 t1 = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(2, 3, 0, 1));

  __m128 t3 = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(0, 0, 0, 0));
  __m128 t4 = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(1, 0, 3, 2));

  __m128 t5 = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(1, 1, 1, 1));
  __m128 t6 = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(2, 0, 3, 1));

  // [d,d,d,d] * [z,w,x,y] = [dz,dw,dx,dy]
  __m128 m0 = _mm_mul_ps(t0, t1);

  // [a,a,a,a] * [y,x,w,z] = [ay,ax,aw,az]
  __m128 m1 = _mm_mul_ps(t3, t4);

  // [b,b,b,b] * [z,x,w,y] = [bz,bx,bw,by]
  __m128 m2 = _mm_mul_ps(t5, t6);

  // [c,c,c,c] * [w,z,x,y] = [cw,cz,cx,cy]
  __m128 t7 = _mm_shuffle_ps(abcd, abcd, _MM_SHUFFLE(2, 2, 2, 2));
  __m128 t8 = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(3, 2, 0, 1));
  __m128 m3 = _mm_mul_ps(t7, t8);

  // [dz,dw,dx,dy] + -[ay,ax,aw,az] = [dz+ay,dw-ax,dx+aw,dy-az]
  __m128 e = _mm_addsub_ps(m0, m1);

  // [dx+aw,dz+ay,dy-az,dw-ax]
  e = _mm_shuffle_ps(e, e, _MM_SHUFFLE(1, 3, 0, 2));

  // [dx+aw,dz+ay,dy-az,dw-ax] + -[bz,bx,bw,by] =
  // [dx+aw+bz,dz+ay-bx,dy-az+bw,dw-ax-by]
  e = _mm_addsub_ps(e, m2);

  // [dz+ay-bx,dw-ax-by,dy-az+bw,dx+aw+bz]
  e = _mm_shuffle_ps(e, e, _MM_SHUFFLE(2, 0, 1, 3));

  // [dz+ay-bx,dw-ax-by,dy-az+bw,dx+aw+bz] + -[cw,cz,cx,cy] =
  // [dz+ay-bx+cw,dw-ax-by-cz,dy-az+bw+cx,dx+aw+bz-cy]
  e = _mm_addsub_ps(e, m3);

  // [dw-ax-by-cz,dz+ay-bx+cw,dy-az+bw+cx,dx+aw+bz-cy]
  return Quat(Vec4(_mm_shuffle_ps(e, e, _MM_SHUFFLE(2, 3, 1, 0))));
#else
  float lx = mValue.GetX();
  float ly = mValue.GetY();
  float lz = mValue.GetZ();
  float lw = mValue.GetW();

  float rx = inRHS.mValue.GetX();
  float ry = inRHS.mValue.GetY();
  float rz = inRHS.mValue.GetZ();
  float rw = inRHS.mValue.GetW();

  float x = lw * rx + lx * rw + ly * rz - lz * ry;
  float y = lw * ry - lx * rz + ly * rw + lz * rx;
  float z = lw * rz + lx * ry - ly * rx + lz * rw;
  float w = lw * rw - lx * rx - ly * ry - lz * rz;

  return Quat(x, y, z, w);
#endif
}

Quat Quat::sRotation(Vec3Arg inAxis, float inAngle) {
  // returns [inAxis * sin(0.5f * inAngle), cos(0.5f * inAngle)]
  ASSERT(inAxis.IsNormalized());
  Vec4 s, c;
  Vec4::sReplicate(0.5f * inAngle).SinCos(s, c);
  return Quat(Vec4::sSelect(Vec4(inAxis) * s, c, UVec4(0, 0, 0, 0xffffffffU)));
}

void Quat::GetAxisAngle(Vec3& outAxis, float& outAngle) const {
  ASSERT(IsNormalized());
  Quat w_pos = EnsureWPositive();
  float abs_w = w_pos.GetW();
  if (abs_w >= 1.0f) {
    outAxis = Vec3::sZero();
    outAngle = 0.0f;
  } else {
    outAngle = 2.0f * ACos(abs_w);
    outAxis = w_pos.GetXYZ().NormalizedOr(Vec3::sZero());
  }
}

template <class Random>
Quat Quat::sRandom(Random& inRandom) {
  std::uniform_real_distribution<float> zero_to_one(0.0f, 1.0f);
  float x0 = zero_to_one(inRandom);
  float r1 = sqrt(1.0f - x0), r2 = sqrt(x0);
  std::uniform_real_distribution<float> zero_to_two_pi(0.0f, 2.0f * PI);
  Vec4 s, c;
  Vec4(zero_to_two_pi(inRandom), zero_to_two_pi(inRandom), 0, 0).SinCos(s, c);
  return Quat(s.GetX() * r1, c.GetX() * r1, s.GetY() * r2, c.GetY() * r2);
}

Quat Quat::GetTwist(Vec3Arg inAxis) const {
  Quat twist(Vec4(GetXYZ().Dot(inAxis) * inAxis, GetW()));
  float twist_len = twist.LengthSq();
  if (twist_len != 0.0f)
    return twist / sqrt(twist_len);
  else
    return Quat::sIdentity();
}

void Quat::GetSwingTwist(Quat& outSwing, Quat& outTwist) const {
  float x = GetX(), y = GetY(), z = GetZ(), w = GetW();
  float s = sqrt(Square(w) + Square(x));
  if (s != 0.0f) {
    outTwist = Quat(x / s, 0, 0, w / s);
    outSwing = Quat(0, (w * y - x * z) / s, (w * z + x * y) / s, s);
  } else {
    // If both x and w are zero, this must be a 180 degree rotation around
    // either y or z
    outTwist = Quat::sIdentity();
    outSwing = *this;
  }
}

Quat Quat::LERP(QuatArg inDestination, float inFraction) const {
  float scale0 = 1.0f - inFraction;
  return Quat(Vec4::sReplicate(scale0) * mValue +
              Vec4::sReplicate(inFraction) * inDestination.mValue);
}

Quat Quat::SLERP(QuatArg inDestination, float inFraction) const {
  // Difference at which to LERP instead of SLERP
  const float delta = 0.0001f;

  // Calc cosine
  float sign_scale1 = 1.0f;
  float cos_omega = Dot(inDestination);

  // Adjust signs (if necessary)
  if (cos_omega < 0.0f) {
    cos_omega = -cos_omega;
    sign_scale1 = -1.0f;
  }

  // Calculate coefficients
  float scale0, scale1;
  if (1.0f - cos_omega > delta) {
    // Standard case (slerp)
    float omega = ACos(cos_omega);
    float sin_omega = Sin(omega);
    scale0 = Sin((1.0f - inFraction) * omega) / sin_omega;
    scale1 = sign_scale1 * Sin(inFraction * omega) / sin_omega;
  } else {
    // Quaternions are very close so we can do a linear interpolation
    scale0 = 1.0f - inFraction;
    scale1 = sign_scale1 * inFraction;
  }

  // Interpolate between the two quaternions
  return Quat(Vec4::sReplicate(scale0) * mValue +
              Vec4::sReplicate(scale1) * inDestination.mValue)
      .Normalized();
}

Vec3 Quat::operator*(Vec3Arg inValue) const {
  // Rotating a vector by a quaternion is done by: p' = q * p * q^-1 (q^-1 =
  // conjugated(q) for a unit quaternion)
  ASSERT(IsNormalized());
  return Vec3((*this * Quat(Vec4(inValue, 0)) * Conjugated()).mValue);
}

Vec3 Quat::InverseRotate(Vec3Arg inValue) const {
  ASSERT(IsNormalized());
  return Vec3((Conjugated() * Quat(Vec4(inValue, 0)) * *this).mValue);
}

Vec3 Quat::RotateAxisX() const {
  // This is *this * Vec3::sAxisX() written out:
  ASSERT(IsNormalized());
  float x = GetX(), y = GetY(), z = GetZ(), w = GetW();
  float tx = 2.0f * x, tw = 2.0f * w;
  return Vec3(tx * x + tw * w - 1.0f, tx * y + z * tw, tx * z - y * tw);
}

Vec3 Quat::RotateAxisY() const {
  // This is *this * Vec3::sAxisY() written out:
  ASSERT(IsNormalized());
  float x = GetX(), y = GetY(), z = GetZ(), w = GetW();
  float ty = 2.0f * y, tw = 2.0f * w;
  return Vec3(x * ty - z * tw, tw * w + ty * y - 1.0f, x * tw + ty * z);
}

Vec3 Quat::RotateAxisZ() const {
  // This is *this * Vec3::sAxisZ() written out:
  ASSERT(IsNormalized());
  float x = GetX(), y = GetY(), z = GetZ(), w = GetW();
  float tz = 2.0f * z, tw = 2.0f * w;
  return Vec3(x * tz + y * tw, y * tz - x * tw, tw * w + tz * z - 1.0f);
}

void Quat::StoreFloat3(float3* outV) const {
  ASSERT(IsNormalized());
  EnsureWPositive().GetXYZ().StoreFloat3(outV);
}

Quat Quat::sLoadFloat3Unsafe(const float3& inV) {
  Vec3 v = Vec3::sLoadFloat3Unsafe(inV);
  float w = sqrt(amaryl::Max(
      1.0f - v.LengthSq(),
      0.0f));  // It is possible that the length of v is a fraction above 1, and
               // we don't want to introduce NaN's in that case so we clamp to 0
  return Quat(Vec4(v, w));
}

Quat Quat::Conjugated() const { return Quat(mValue.FlipSign<-1, -1, -1, 1>()); }

Quat Quat::EnsureWPositive() const {
  return Quat(Vec4::sXor(
      mValue,
      Vec4::sAnd(mValue.SplatW(), UVec4::sReplicate(0x80000000).ReinterpretAsFloat())));
}

float Quat::GetRotationAngle(Vec3Arg inAxis) const {
  return GetW() == 0.0f ? PI : 2.0f * ATan(GetXYZ().Dot(inAxis) / GetW());
}

}  // namespace amaryl

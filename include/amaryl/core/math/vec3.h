#pragma once

#include <vector>

#include "amaryl/core/amaryl.h"
#include "amaryl/core/math/math_constants.h"
#include "amaryl/core/math/math_types.h"
#include "amaryl/core/math/vector_aliases.h"

namespace amaryl {

/// 3 component vector (stored as 4 vectors).
/// Note that we keep the 4th component the same as the 3rd component to avoid
/// divisions by zero.
class [[nodiscard]] alignas(VECTOR_ALIGNMENT) Vec3 {
 public:
  AMARYL_MATH_OVERRIDE_NEW_DELETE

  // Underlying vector type
#if AMARYL_USE_SSE
  using Type = __m128;
#elif AMARYL_SIMD_NEON
  using Type = float32x4_t;
#else
  using Type = struct {
    float mData[4];
  };
#endif

  // Argument type
  using ArgType = Vec3Arg;

  /// Constructor
  Vec3() = default;  ///< Intentionally not initialized for performance reasons
  Vec3(const Vec3& inRHS) = default;
  Vec3& operator=(const Vec3& inRHS) = default;
  explicit FORCE_INLINE Vec3(Vec4Arg inRHS);
  FORCE_INLINE Vec3(Type inRHS) : mValue(inRHS) { CheckW(); }

  /// Load 3 floats from memory
  explicit FORCE_INLINE Vec3(const float3& inV);

  /// Create a vector from 3 components
  FORCE_INLINE Vec3(float inX, float inY, float inZ);

  /// Vector with all zeros
  static FORCE_INLINE Vec3 sZero();
  static FORCE_INLINE Vec3 sOne();

  /// Vector with all NaN's
  static FORCE_INLINE Vec3 sNaN();

  /// Vectors with the principal axis
  static FORCE_INLINE Vec3 sAxisX() { return Vec3(1, 0, 0); }
  static FORCE_INLINE Vec3 sAxisY() { return Vec3(0, 1, 0); }
  static FORCE_INLINE Vec3 sAxisZ() { return Vec3(0, 0, 1); }

  static FORCE_INLINE Vec3 sForward() { return Vec3(0.0, 1.0, 0.0); }
  static FORCE_INLINE Vec3 sRight() { return Vec3(1.0, 0.0, 0.0); }
  static FORCE_INLINE Vec3 sUp() { return Vec3(0.0, 0.0, 1.0); }

  /// Replicate inV across all components
  static FORCE_INLINE Vec3 sReplicate(float inV);

  /// Load 3 floats from memory (reads 32 bits extra which it doesn't use)
  static FORCE_INLINE Vec3 sLoadFloat3Unsafe(float3 inV);

  /// Return the minimum value of each of the components
  static FORCE_INLINE Vec3 sMin(Vec3Arg inV1, Vec3Arg inV2);

  /// Return the maximum of each of the components
  static FORCE_INLINE Vec3 sMax(Vec3Arg inV1, Vec3Arg inV2);

  /// Clamp a vector between min and max (component wise)
  static FORCE_INLINE Vec3 sClamp(Vec3Arg inV, Vec3Arg inMin, Vec3Arg inMax);

  /// Equals (component wise)
  static FORCE_INLINE UVec4 sEquals(Vec3Arg inV1, Vec3Arg inV2);

  /// Less than (component wise)
  static FORCE_INLINE UVec4 sLess(Vec3Arg inV1, Vec3Arg inV2);

  /// Less than or equal (component wise)
  static FORCE_INLINE UVec4 sLessOrEqual(Vec3Arg inV1, Vec3Arg inV2);

  /// Greater than (component wise)
  static FORCE_INLINE UVec4 sGreater(Vec3Arg inV1, Vec3Arg inV2);

  /// Greater than or equal (component wise)
  static FORCE_INLINE UVec4 sGreaterOrEqual(Vec3Arg inV1, Vec3Arg inV2);

  /// Calculates inMul1 * inMul2 + inAdd
  static FORCE_INLINE Vec3 sFusedMultiplyAdd(Vec3Arg inMul1, Vec3Arg inMul2,
                                             Vec3Arg inAdd);

  /// Component wise select, returns inV1 when highest bit of inControl = 0 and
  /// inV2 when highest bit of inControl = 1
  static FORCE_INLINE Vec3 sSelect(Vec3Arg inV1, Vec3Arg inV2, UVec4Arg inControl);

  /// Logical or (component wise)
  static FORCE_INLINE Vec3 sOr(Vec3Arg inV1, Vec3Arg inV2);

  /// Logical xor (component wise)
  static FORCE_INLINE Vec3 sXor(Vec3Arg inV1, Vec3Arg inV2);

  /// Logical and (component wise)
  static FORCE_INLINE Vec3 sAnd(Vec3Arg inV1, Vec3Arg inV2);

  /// Get unit vector given spherical coordinates
  /// inTheta \f$\in [0, \pi]\f$ is angle between vector and z-axis
  /// inPhi \f$\in [0, 2 \pi]\f$ is the angle in the xy-plane starting from the
  /// x axis and rotating counter clockwise around the z-axis
  static FORCE_INLINE Vec3 sUnitSpherical(float inTheta, float inPhi);

  /// A set of vectors uniformly spanning the surface of a unit sphere, usable
  /// for debug purposes
  CORE_API static const std::vector<Vec3> sUnitSphere;

  /// Get random unit vector
  template <class Random>
  static inline Vec3 sRandom(Random& inRandom);

  /// Get individual components
#if AMARYL_USE_SSE
  FORCE_INLINE float GetX() const { return _mm_cvtss_f32(mValue); }
  FORCE_INLINE float GetY() const { return mF32[1]; }
  FORCE_INLINE float GetZ() const { return mF32[2]; }
#elif AMARYL_SIMD_NEON
  FORCE_INLINE float GetX() const { return vgetq_lane_f32(mValue, 0); }
  FORCE_INLINE float GetY() const { return vgetq_lane_f32(mValue, 1); }
  FORCE_INLINE float GetZ() const { return vgetq_lane_f32(mValue, 2); }
#else
  FORCE_INLINE float GetX() const { return mF32[0]; }
  FORCE_INLINE float GetY() const { return mF32[1]; }
  FORCE_INLINE float GetZ() const { return mF32[2]; }
#endif

  /// Set individual components
  FORCE_INLINE void SetX(float inX) { mF32[0] = inX; }
  FORCE_INLINE void SetY(float inY) { mF32[1] = inY; }
  FORCE_INLINE void SetZ(float inZ) {
    mF32[2] = mF32[3] = inZ;
  }  // Assure Z and W are the same

  /// Set all components
  FORCE_INLINE void Set(float inX, float inY, float inZ) { *this = Vec3(inX, inY, inZ); }

  /// Get float component by index
  FORCE_INLINE float operator[](uint32 inCoordinate) const {
    ASSERT(inCoordinate < 3);
    return mF32[inCoordinate];
  }

  /// Set float component by index
  FORCE_INLINE void SetComponent(uint32 inCoordinate, float inValue) {
    ASSERT(inCoordinate < 3);
    mF32[inCoordinate] = inValue;
    mValue = sFixW(mValue);
  }  // Assure Z and W are the same

  /// Comparison
  FORCE_INLINE bool operator==(Vec3Arg inV2) const;
  FORCE_INLINE bool operator!=(Vec3Arg inV2) const { return !(*this == inV2); }

  /// Test if two vectors are close
  FORCE_INLINE bool IsClose(Vec3Arg inV2, float inMaxDistSq = 1.0e-12f) const;

  /// Test if vector is near zero
  FORCE_INLINE bool IsNearZero(float inMaxDistSq = 1.0e-12f) const;

  /// Test if vector is normalized
  FORCE_INLINE bool IsNormalized(float inTolerance = 1.0e-6f) const;

  /// Test if vector contains NaN elements
  FORCE_INLINE bool IsNaN() const;

  /// Multiply two float vectors (component wise)
  FORCE_INLINE Vec3 operator*(Vec3Arg inV2) const;

  /// Multiply vector with float
  FORCE_INLINE Vec3 operator*(float inV2) const;

  /// Multiply vector with float
  friend FORCE_INLINE Vec3 operator*(float inV1, Vec3Arg inV2);

  /// Divide vector by float
  FORCE_INLINE Vec3 operator/(float inV2) const;

  /// Multiply vector with float
  FORCE_INLINE Vec3& operator*=(float inV2);

  /// Multiply vector with vector
  FORCE_INLINE Vec3& operator*=(Vec3Arg inV2);

  /// Divide vector by float
  FORCE_INLINE Vec3& operator/=(float inV2);

  /// Add two float vectors (component wise)
  FORCE_INLINE Vec3 operator+(Vec3Arg inV2) const;

  /// Add two float vectors (component wise)
  FORCE_INLINE Vec3& operator+=(Vec3Arg inV2);

  /// Negate
  FORCE_INLINE Vec3 operator-() const;

  /// Subtract two float vectors (component wise)
  FORCE_INLINE Vec3 operator-(Vec3Arg inV2) const;

  /// Add two float vectors (component wise)
  FORCE_INLINE Vec3& operator-=(Vec3Arg inV2);

  /// Divide (component wise)
  FORCE_INLINE Vec3 operator/(Vec3Arg inV2) const;

  /// Swizzle the elements in inV
  template <uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ>
  FORCE_INLINE Vec3 Swizzle() const;

  /// Replicate the X component to all components
  FORCE_INLINE Vec4 SplatX() const;

  /// Replicate the Y component to all components
  FORCE_INLINE Vec4 SplatY() const;

  /// Replicate the Z component to all components
  FORCE_INLINE Vec4 SplatZ() const;

  /// Get index of component with lowest value
  FORCE_INLINE int GetLowestComponentIndex() const;

  /// Get index of component with highest value
  FORCE_INLINE int GetHighestComponentIndex() const;

  /// Return the absolute value of each of the components
  FORCE_INLINE Vec3 Abs() const;

  /// Reciprocal vector (1 / value) for each of the components
  FORCE_INLINE Vec3 Reciprocal() const;

  /// Cross product
  FORCE_INLINE Vec3 Cross(Vec3Arg inV2) const;

  /// Dot product, returns the dot product in X, Y and Z components
  FORCE_INLINE Vec3 DotV(Vec3Arg inV2) const;

  /// Dot product, returns the dot product in X, Y, Z and W components
  FORCE_INLINE Vec4 DotV4(Vec3Arg inV2) const;

  /// Dot product
  FORCE_INLINE float Dot(Vec3Arg inV2) const;

  /// Squared length of vector
  FORCE_INLINE float LengthSq() const;

  /// Length of vector
  FORCE_INLINE float Length() const;

  /// Normalize vector
  FORCE_INLINE Vec3 Normalized() const;

  /// Normalize vector or return inZeroValue if the length of the vector is zero
  FORCE_INLINE Vec3 NormalizedOr(Vec3Arg inZeroValue) const;

  /// Store 3 floats to memory
  FORCE_INLINE void StoreFloat3(float3* outV) const;

  /// Convert each component from a float to an int
  FORCE_INLINE UVec4 ToInt() const;

  /// Reinterpret Vec3 as a UVec4 (doesn't change the bits)
  FORCE_INLINE UVec4 ReinterpretAsInt() const;

  /// Get the minimum of X, Y and Z
  FORCE_INLINE float ReduceMin() const;

  /// Get the maximum of X, Y and Z
  FORCE_INLINE float ReduceMax() const;

  /// Component wise square root
  FORCE_INLINE Vec3 Sqrt() const;

  /// Get normalized vector that is perpendicular to this vector
  FORCE_INLINE Vec3 GetNormalizedPerpendicular() const;

  /// Get vector that contains the sign of each element (returns 1.0f if
  /// positive, -1.0f if negative)
  FORCE_INLINE Vec3 GetSign() const;

  /// Flips the signs of the components, e.g. FlipSign<-1, 1, -1>() will flip
  /// the signs of the X and Z components
  template <int X, int Y, int Z>
  FORCE_INLINE Vec3 FlipSign() const;

  /// Internal helper function that checks that W is equal to Z, so e.g.
  /// dividing by it should not generate div by 0
  FORCE_INLINE void CheckW() const;

  /// Internal helper function that ensures that the Z component is replicated
  /// to the W component to prevent divisions by zero
  static FORCE_INLINE Type sFixW(Type inValue);

  union {
    Type mValue;
    float mF32[4];
  };
};

static_assert(std::is_trivial<Vec3>(), "Is supposed to be a trivial type!");

}  // namespace amaryl

#include <random>

#include "amaryl/core/math/uvec4.h"
#include "amaryl/core/math/vec4.h"

AMARYL_MAKE_HASHABLE(amaryl::Vec3, t.GetX(), t.GetY(), t.GetZ())

namespace amaryl {

void Vec3::CheckW() const {
  // Avoid asserts when both components are NaN
  ASSERT(reinterpret_cast<const uint32*>(mF32)[2] ==
         reinterpret_cast<const uint32*>(mF32)[3]);
}

Vec3::Type Vec3::sFixW(Type inValue) {
#if AMARYL_USE_SSE
  return _mm_shuffle_ps(inValue, inValue, _MM_SHUFFLE(2, 2, 1, 0));
#elif AMARYL_SIMD_NEON
  return ARM_NEON_SHUFFLE_F32x4(inValue, inValue, 0, 1, 2, 2);
#else
  Type value;
  value.mData[0] = inValue.mData[0];
  value.mData[1] = inValue.mData[1];
  value.mData[2] = inValue.mData[2];
  value.mData[3] = inValue.mData[2];
  return value;
#endif
}

Vec3::Vec3(Vec4Arg inRHS) : mValue(sFixW(inRHS.mValue)) {}

Vec3::Vec3(const float3& inV) {
#if AMARYL_USE_SSE
  Type x = _mm_load_ss(&inV.X());
  Type y = _mm_load_ss(&inV.Y());
  Type z = _mm_load_ss(&inV.Z());
  Type xy = _mm_unpacklo_ps(x, y);
  mValue = _mm_shuffle_ps(xy, z, _MM_SHUFFLE(0, 0, 1, 0));  // Assure Z and W are the same
#elif AMARYL_SIMD_NEON
  float32x2_t xy = vld1_f32(&inV.X());
  float32x2_t zz = vdup_n_f32(inV.Z());  // Assure Z and W are the same
  mValue = vcombine_f32(xy, zz);
#else
  mF32[0] = inV[0];
  mF32[1] = inV[1];
  mF32[2] = inV[2];
  mF32[3] = inV[2];
#endif
}

Vec3::Vec3(float inX, float inY, float inZ) {
#if AMARYL_USE_SSE
  mValue = _mm_set_ps(inZ, inZ, inY, inX);
#elif AMARYL_SIMD_NEON
  uint32x2_t xy =
      vcreate_f32(static_cast<uint64>(*reinterpret_cast<uint32*>(&inX)) |
                  (static_cast<uint64>(*reinterpret_cast<uint32*>(&inY)) << 32));
  uint32x2_t zz =
      vcreate_f32(static_cast<uint64>(*reinterpret_cast<uint32*>(&inZ)) |
                  (static_cast<uint64>(*reinterpret_cast<uint32*>(&inZ)) << 32));
  mValue = vcombine_f32(xy, zz);
#else
  mF32[0] = inX;
  mF32[1] = inY;
  mF32[2] = inZ;
  mF32[3] = inZ;
#endif
}

template <uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ>
Vec3 Vec3::Swizzle() const {
  static_assert(SwizzleX <= 3, "SwizzleX template parameter out of range");
  static_assert(SwizzleY <= 3, "SwizzleY template parameter out of range");
  static_assert(SwizzleZ <= 3, "SwizzleZ template parameter out of range");

#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue,
                        _MM_SHUFFLE(SwizzleZ, SwizzleZ, SwizzleY,
                                    SwizzleX));  // Assure Z and W are the same
#elif AMARYL_SIMD_NEON
  return ARM_NEON_SHUFFLE_F32x4(mValue, mValue, SwizzleX, SwizzleY, SwizzleZ, SwizzleZ);
#else
  return Vec3(mF32[SwizzleX], mF32[SwizzleY], mF32[SwizzleZ]);
#endif
}

Vec3 Vec3::sZero() {
#if AMARYL_USE_SSE
  return _mm_setzero_ps();
#elif AMARYL_SIMD_NEON
  return vdupq_n_f32(0);
#else
  return Vec3(0, 0, 0);
#endif
}

Vec3 Vec3::sOne() { return Vec3::sReplicate(1.0f); }

Vec3 Vec3::sReplicate(float inV) {
#if AMARYL_USE_SSE
  return _mm_set1_ps(inV);
#elif AMARYL_SIMD_NEON
  return vdupq_n_f32(inV);
#else
  return Vec3(inV, inV, inV);
#endif
}

Vec3 Vec3::sNaN() { return sReplicate(std::numeric_limits<float>::quiet_NaN()); }

Vec3 Vec3::sLoadFloat3Unsafe(float3 inV) {
#if AMARYL_USE_SSE
  Type v = _mm_loadu_ps(&inV.X());
#elif AMARYL_SIMD_NEON
  Type v = vld1q_f32(&inV.X());
#else
  Type v = {inV.X(), inV.Y(), inV.Z()};
#endif
  return sFixW(v);
}

Vec3 Vec3::sMin(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_min_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vminq_f32(inV1.mValue, inV2.mValue);
#else
  return Vec3(amaryl::Min(inV1.mF32[0], inV2.mF32[0]),
              amaryl::Min(inV1.mF32[1], inV2.mF32[1]),
              amaryl::Min(inV1.mF32[2], inV2.mF32[2]));
#endif
}

Vec3 Vec3::sMax(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_max_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vmaxq_f32(inV1.mValue, inV2.mValue);
#else
  return Vec3(amaryl::Max(inV1.mF32[0], inV2.mF32[0]),
              amaryl::Max(inV1.mF32[1], inV2.mF32[1]),
              amaryl::Max(inV1.mF32[2], inV2.mF32[2]));
#endif
}

Vec3 Vec3::sClamp(Vec3Arg inV, Vec3Arg inMin, Vec3Arg inMax) {
  return sMax(sMin(inV, inMax), inMin);
}

UVec4 Vec3::sEquals(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmpeq_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vceqq_f32(inV1.mValue, inV2.mValue);
#else
  uint32 z = inV1.mF32[2] == inV2.mF32[2] ? 0xffffffffu : 0;
  return UVec4(inV1.mF32[0] == inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] == inV2.mF32[1] ? 0xffffffffu : 0, z, z);
#endif
}

UVec4 Vec3::sLess(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmplt_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vcltq_f32(inV1.mValue, inV2.mValue);
#else
  uint32 z = inV1.mF32[2] < inV2.mF32[2] ? 0xffffffffu : 0;
  return UVec4(inV1.mF32[0] < inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] < inV2.mF32[1] ? 0xffffffffu : 0, z, z);
#endif
}

UVec4 Vec3::sLessOrEqual(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmple_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vcleq_f32(inV1.mValue, inV2.mValue);
#else
  uint32 z = inV1.mF32[2] <= inV2.mF32[2] ? 0xffffffffu : 0;
  return UVec4(inV1.mF32[0] <= inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] <= inV2.mF32[1] ? 0xffffffffu : 0, z, z);
#endif
}

UVec4 Vec3::sGreater(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmpgt_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vcgtq_f32(inV1.mValue, inV2.mValue);
#else
  uint32 z = inV1.mF32[2] > inV2.mF32[2] ? 0xffffffffu : 0;
  return UVec4(inV1.mF32[0] > inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] > inV2.mF32[1] ? 0xffffffffu : 0, z, z);
#endif
}

UVec4 Vec3::sGreaterOrEqual(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmpge_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vcgeq_f32(inV1.mValue, inV2.mValue);
#else
  uint32 z = inV1.mF32[2] >= inV2.mF32[2] ? 0xffffffffu : 0;
  return UVec4(inV1.mF32[0] >= inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] >= inV2.mF32[1] ? 0xffffffffu : 0, z, z);
#endif
}

Vec3 Vec3::sFusedMultiplyAdd(Vec3Arg inMul1, Vec3Arg inMul2, Vec3Arg inAdd) {
#if AMARYL_USE_SSE
  return _mm_add_ps(_mm_mul_ps(inMul1.mValue, inMul2.mValue), inAdd.mValue);
#elif AMARYL_SIMD_NEON
  return vmlaq_f32(inAdd.mValue, inMul1.mValue, inMul2.mValue);
#else
  return Vec3(inMul1.mF32[0] * inMul2.mF32[0] + inAdd.mF32[0],
              inMul1.mF32[1] * inMul2.mF32[1] + inAdd.mF32[1],
              inMul1.mF32[2] * inMul2.mF32[2] + inAdd.mF32[2]);
#endif
}

Vec3 Vec3::sSelect(Vec3Arg inV1, Vec3Arg inV2, UVec4Arg inControl) {
#if AMARYL_USE_SSE4_1
  Type v = _mm_blendv_ps(inV1.mValue, inV2.mValue, _mm_castsi128_ps(inControl.mValue));
  return sFixW(v);
#elif AMARYL_SIMD_NEON
  Type v = vbslq_f32(vshrq_n_s32(inControl.mValue, 31), inV2.mValue, inV1.mValue);
  return sFixW(v);
#else
  Vec3 result;
  for (int i = 0; i < 3; i++) {
    result.mF32[i] = inControl.mU32[i] ? inV2.mF32[i] : inV1.mF32[i];
  }
  result.mF32[3] = result.mF32[2];
  return result;
#endif
}

Vec3 Vec3::sOr(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_or_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vorrq_s32(inV1.mValue, inV2.mValue);
#else
  return Vec3(
      UVec4::sOr(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat());
#endif
}

Vec3 Vec3::sXor(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_xor_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return veorq_s32(inV1.mValue, inV2.mValue);
#else
  return Vec3(
      UVec4::sXor(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat());
#endif
}

Vec3 Vec3::sAnd(Vec3Arg inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_and_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vandq_s32(inV1.mValue, inV2.mValue);
#else
  return Vec3(
      UVec4::sAnd(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt()).ReinterpretAsFloat());
#endif
}

Vec3 Vec3::sUnitSpherical(float inTheta, float inPhi) {
  Vec4 s, c;
  Vec4(inTheta, inPhi, 0, 0).SinCos(s, c);
  return Vec3(s.GetX() * c.GetY(), s.GetX() * s.GetY(), c.GetX());
}

template <class Random>
Vec3 Vec3::sRandom(Random& inRandom) {
  std::uniform_real_distribution<float> zero_to_one(0.0f, 1.0f);
  float theta = PI * zero_to_one(inRandom);
  float phi = 2.0f * PI * zero_to_one(inRandom);
  return sUnitSpherical(theta, phi);
}

bool Vec3::operator==(Vec3Arg inV2) const {
  return sEquals(*this, inV2).TestAllXYZTrue();
}

bool Vec3::IsClose(Vec3Arg inV2, float inMaxDistSq) const {
  return (inV2 - *this).LengthSq() <= inMaxDistSq;
}

bool Vec3::IsNearZero(float inMaxDistSq) const { return LengthSq() <= inMaxDistSq; }

Vec3 Vec3::operator*(Vec3Arg inV2) const {
#if AMARYL_USE_SSE
  return _mm_mul_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vmulq_f32(mValue, inV2.mValue);
#else
  return Vec3(mF32[0] * inV2.mF32[0], mF32[1] * inV2.mF32[1], mF32[2] * inV2.mF32[2]);
#endif
}

Vec3 Vec3::operator*(float inV2) const {
#if AMARYL_USE_SSE
  return _mm_mul_ps(mValue, _mm_set1_ps(inV2));
#elif AMARYL_SIMD_NEON
  return vmulq_n_f32(mValue, inV2);
#else
  return Vec3(mF32[0] * inV2, mF32[1] * inV2, mF32[2] * inV2);
#endif
}

Vec3 operator*(float inV1, Vec3Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_mul_ps(_mm_set1_ps(inV1), inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vmulq_n_f32(inV2.mValue, inV1);
#else
  return Vec3(inV1 * inV2.mF32[0], inV1 * inV2.mF32[1], inV1 * inV2.mF32[2]);
#endif
}

Vec3 Vec3::operator/(float inV2) const {
#if AMARYL_USE_SSE
  return _mm_div_ps(mValue, _mm_set1_ps(inV2));
#elif AMARYL_SIMD_NEON
  return vdivq_f32(mValue, vdupq_n_f32(inV2));
#else
  return Vec3(mF32[0] / inV2, mF32[1] / inV2, mF32[2] / inV2);
#endif
}

Vec3& Vec3::operator*=(float inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_mul_ps(mValue, _mm_set1_ps(inV2));
#elif AMARYL_SIMD_NEON
  mValue = vmulq_n_f32(mValue, inV2);
#else
  for (int i = 0; i < 3; ++i) {
    mF32[i] *= inV2;
  }
  mF32[3] = mF32[2];
#endif
  return *this;
}

Vec3& Vec3::operator*=(Vec3Arg inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_mul_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  mValue = vmulq_f32(mValue, inV2.mValue);
#else
  for (int i = 0; i < 3; ++i) {
    mF32[i] *= inV2.mF32[i];
  }
  mF32[3] = mF32[2];
#endif
  return *this;
}

Vec3& Vec3::operator/=(float inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_div_ps(mValue, _mm_set1_ps(inV2));
#elif AMARYL_SIMD_NEON
  mValue = vdivq_f32(mValue, vdupq_n_f32(inV2));
#else
  for (int i = 0; i < 3; ++i) {
    mF32[i] /= inV2;
  }
  mF32[3] = mF32[2];
#endif
  return *this;
}

Vec3 Vec3::operator+(Vec3Arg inV2) const {
#if AMARYL_USE_SSE
  return _mm_add_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vaddq_f32(mValue, inV2.mValue);
#else
  return Vec3(mF32[0] + inV2.mF32[0], mF32[1] + inV2.mF32[1], mF32[2] + inV2.mF32[2]);
#endif
}

Vec3& Vec3::operator+=(Vec3Arg inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_add_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  mValue = vaddq_f32(mValue, inV2.mValue);
#else
  for (int i = 0; i < 3; ++i) {
    mF32[i] += inV2.mF32[i];
  }
  mF32[3] = mF32[2];
#endif
  return *this;
}

Vec3 Vec3::operator-() const {
#if AMARYL_USE_SSE
  return _mm_sub_ps(_mm_setzero_ps(), mValue);
#elif AMARYL_SIMD_NEON
  return vnegq_f32(mValue);
#else
  return Vec3(-mF32[0], -mF32[1], -mF32[2]);
#endif
}

Vec3 Vec3::operator-(Vec3Arg inV2) const {
#if AMARYL_USE_SSE
  return _mm_sub_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vsubq_f32(mValue, inV2.mValue);
#else
  return Vec3(mF32[0] - inV2.mF32[0], mF32[1] - inV2.mF32[1], mF32[2] - inV2.mF32[2]);
#endif
}

Vec3& Vec3::operator-=(Vec3Arg inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_sub_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  mValue = vsubq_f32(mValue, inV2.mValue);
#else
  for (int i = 0; i < 3; ++i) {
    mF32[i] -= inV2.mF32[i];
  }

  mF32[3] = mF32[2];
#endif
  return *this;
}

Vec3 Vec3::operator/(Vec3Arg inV2) const {
  inV2.CheckW();  // Check W equals Z to avoid div by zero
#if AMARYL_USE_SSE
  return _mm_div_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vdivq_f32(mValue, inV2.mValue);
#else
  return Vec3(mF32[0] / inV2.mF32[0], mF32[1] / inV2.mF32[1], mF32[2] / inV2.mF32[2]);
#endif
}

Vec4 Vec3::SplatX() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(0, 0, 0, 0));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_f32(mValue, 0);
#else
  return Vec4(mF32[0], mF32[0], mF32[0], mF32[0]);
#endif
}

Vec4 Vec3::SplatY() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(1, 1, 1, 1));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_f32(mValue, 1);
#else
  return Vec4(mF32[1], mF32[1], mF32[1], mF32[1]);
#endif
}

Vec4 Vec3::SplatZ() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(2, 2, 2, 2));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_f32(mValue, 2);
#else
  return Vec4(mF32[2], mF32[2], mF32[2], mF32[2]);
#endif
}

int Vec3::GetLowestComponentIndex() const {
  return GetX() < GetY() ? (GetZ() < GetX() ? 2 : 0) : (GetZ() < GetY() ? 2 : 1);
}

int Vec3::GetHighestComponentIndex() const {
  return GetX() > GetY() ? (GetZ() > GetX() ? 2 : 0) : (GetZ() > GetY() ? 2 : 1);
}

Vec3 Vec3::Abs() const {
#if AMARYL_SIMD_AVX2
  return _mm_range_ps(mValue, mValue, 0b1000);
#elif AMARYL_USE_SSE
  return _mm_max_ps(_mm_sub_ps(_mm_setzero_ps(), mValue), mValue);
#elif AMARYL_SIMD_NEON
  return vabsq_f32(mValue);
#else
  return Vec3(abs(mF32[0]), abs(mF32[1]), abs(mF32[2]));
#endif
}

Vec3 Vec3::Reciprocal() const { return sReplicate(1.0f) / mValue; }

Vec3 Vec3::Cross(Vec3Arg inV2) const {
#if AMARYL_USE_SSE
  Type t1 = _mm_shuffle_ps(inV2.mValue, inV2.mValue,
                           _MM_SHUFFLE(0, 0, 2, 1));  // Assure Z and W are the same
  t1 = _mm_mul_ps(t1, mValue);
  Type t2 = _mm_shuffle_ps(mValue, mValue,
                           _MM_SHUFFLE(0, 0, 2, 1));  // Assure Z and W are the same
  t2 = _mm_mul_ps(t2, inV2.mValue);
  Type t3 = _mm_sub_ps(t1, t2);
  return _mm_shuffle_ps(t3, t3, _MM_SHUFFLE(0, 0, 2, 1));  // Assure Z and W are the same
#elif AMARYL_SIMD_NEON
  Type t1 = ARM_NEON_SHUFFLE_F32x4(inV2.mValue, inV2.mValue, 1, 2, 0,
                                   0);  // Assure Z and W are the same
  t1 = vmulq_f32(t1, mValue);
  Type t2 = ARM_NEON_SHUFFLE_F32x4(mValue, mValue, 1, 2, 0,
                                   0);  // Assure Z and W are the same
  t2 = vmulq_f32(t2, inV2.mValue);
  Type t3 = vsubq_f32(t1, t2);
  return ARM_NEON_SHUFFLE_F32x4(t3, t3, 1, 2, 0,
                                0);  // Assure Z and W are the same
#else
  return Vec3(mF32[1] * inV2.mF32[2] - mF32[2] * inV2.mF32[1],
              mF32[2] * inV2.mF32[0] - mF32[0] * inV2.mF32[2],
              mF32[0] * inV2.mF32[1] - mF32[1] * inV2.mF32[0]);
#endif
}

Vec3 Vec3::DotV(Vec3Arg inV2) const {
#if AMARYL_USE_SSE4_1
  return _mm_dp_ps(mValue, inV2.mValue, 0x7f);
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
  mul = vsetq_lane_f32(0, mul, 3);
  return vdupq_n_f32(vaddvq_f32(mul));
#else
  float dot = 0.0f;
  for (int i = 0; i < 3; i++) dot += mF32[i] * inV2.mF32[i];
  return Vec3::sReplicate(dot);
#endif
}

Vec4 Vec3::DotV4(Vec3Arg inV2) const {
#if AMARYL_USE_SSE4_1
  return _mm_dp_ps(mValue, inV2.mValue, 0x7f);
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
  mul = vsetq_lane_f32(0, mul, 3);
  return vdupq_n_f32(vaddvq_f32(mul));
#else
  float dot = 0.0f;
  for (int i = 0; i < 3; i++) dot += mF32[i] * inV2.mF32[i];
  return Vec4::sReplicate(dot);
#endif
}

float Vec3::Dot(Vec3Arg inV2) const {
#if AMARYL_USE_SSE4_1
  return _mm_cvtss_f32(_mm_dp_ps(mValue, inV2.mValue, 0x7f));
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
  mul = vsetq_lane_f32(0, mul, 3);
  return vaddvq_f32(mul);
#else
  float dot = 0.0f;
  for (int i = 0; i < 3; i++) dot += mF32[i] * inV2.mF32[i];
  return dot;
#endif
}

float Vec3::LengthSq() const {
#if AMARYL_USE_SSE4_1
  return _mm_cvtss_f32(_mm_dp_ps(mValue, mValue, 0x7f));
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, mValue);
  mul = vsetq_lane_f32(0, mul, 3);
  return vaddvq_f32(mul);
#else
  float len_sq = 0.0f;
  for (int i = 0; i < 3; i++) len_sq += mF32[i] * mF32[i];
  return len_sq;
#endif
}

float Vec3::Length() const {
#if AMARYL_USE_SSE4_1
  return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(mValue, mValue, 0x7f)));
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, mValue);
  mul = vsetq_lane_f32(0, mul, 3);
  float32x2_t sum = vdup_n_f32(vaddvq_f32(mul));
  return vget_lane_f32(vsqrt_f32(sum), 0);
#else
  return sqrt(LengthSq());
#endif
}

Vec3 Vec3::Sqrt() const {
#if AMARYL_USE_SSE
  return _mm_sqrt_ps(mValue);
#elif AMARYL_SIMD_NEON
  return vsqrtq_f32(mValue);
#else
  return Vec3(sqrt(mF32[0]), sqrt(mF32[1]), sqrt(mF32[2]));
#endif
}

Vec3 Vec3::Normalized() const {
#if AMARYL_USE_SSE4_1
  return _mm_div_ps(mValue, _mm_sqrt_ps(_mm_dp_ps(mValue, mValue, 0x7f)));
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, mValue);
  mul = vsetq_lane_f32(0, mul, 3);
  float32x4_t sum = vdupq_n_f32(vaddvq_f32(mul));
  return vdivq_f32(mValue, vsqrtq_f32(sum));
#else
  return *this / Length();
#endif
}

Vec3 Vec3::NormalizedOr(Vec3Arg inZeroValue) const {
#if AMARYL_USE_SSE4_1
  Type len_sq = _mm_dp_ps(mValue, mValue, 0x7f);
  Type is_zero = _mm_cmpeq_ps(len_sq, _mm_setzero_ps());
  if (_mm_movemask_ps(is_zero) == 0xf) {
    return inZeroValue;
  } else {
    return _mm_div_ps(mValue, _mm_sqrt_ps(len_sq));
  }
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, mValue);
  mul = vsetq_lane_f32(0, mul, 3);
  float32x4_t sum = vdupq_n_f32(vaddvq_f32(mul));
  float32x4_t len = vsqrtq_f32(sum);
  float32x4_t is_zero = vceqq_f32(len, vdupq_n_f32(0));
  return vbslq_f32(is_zero, inZeroValue.mValue, vdivq_f32(mValue, len));
#else
  float len_sq = LengthSq();
  if (len_sq == 0.0f)
    return inZeroValue;
  else
    return *this / sqrt(len_sq);
#endif
}

bool Vec3::IsNormalized(float inTolerance) const {
  return abs(LengthSq() - 1.0f) <= inTolerance;
}

bool Vec3::IsNaN() const {
#if AMARYL_SIMD_AVX2
  return (_mm_fpclass_ps_mask(mValue, 0b10000001) & 0x7) != 0;
#elif AMARYL_USE_SSE
  return (_mm_movemask_ps(_mm_cmpunord_ps(mValue, mValue)) & 0x7) != 0;
#elif AMARYL_SIMD_NEON
  u32x4_t mask = ARM_NEON_UINT32x4(1, 1, 1, 0);
  u32x4_t is_equal =
      vceqq_f32(mValue, mValue);  // If a number is not equal to itself it's a NaN
  return vaddvq_u32(vandq_u32(is_equal, mask)) != 3;
#else
  return isnan(mF32[0]) || isnan(mF32[1]) || isnan(mF32[2]);
#endif
}

void Vec3::StoreFloat3(float3* outV) const {
#if AMARYL_USE_SSE
  _mm_store_ss(&outV->X(), mValue);
  Vec3 t = Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_UNUSED>();
  _mm_store_ss(&outV->Y(), t.mValue);
  t = t.Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_UNUSED>();
  _mm_store_ss(&outV->Z(), t.mValue);
#elif AMARYL_SIMD_NEON
  float32x2_t xy = vget_low_f32(mValue);
  vst1_f32(&outV->X(), xy);
  vst1q_lane_f32(&outV->Z(), mValue, 2);
#else
  outV->x = mF32[0];
  outV->y = mF32[1];
  outV->z = mF32[2];
#endif
}

UVec4 Vec3::ToInt() const {
#if AMARYL_USE_SSE
  return _mm_cvttps_epi32(mValue);
#elif AMARYL_SIMD_NEON
  return vcvtq_u32_f32(mValue);
#else
  return UVec4(uint32(mF32[0]), uint32(mF32[1]), uint32(mF32[2]), uint32(mF32[3]));
#endif
}

UVec4 Vec3::ReinterpretAsInt() const {
#if AMARYL_USE_SSE
  return UVec4(_mm_castps_si128(mValue));
#elif AMARYL_SIMD_NEON
  return vreinterpretq_u32_f32(mValue);
#else
  return *reinterpret_cast<const UVec4*>(this);
#endif
}

float Vec3::ReduceMin() const {
  Vec3 v = sMin(mValue, Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_Z>());
  v = sMin(v, v.Swizzle<SWIZZLE_Z, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());
  return v.GetX();
}

float Vec3::ReduceMax() const {
  Vec3 v = sMax(mValue, Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_Z>());
  v = sMax(v, v.Swizzle<SWIZZLE_Z, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());
  return v.GetX();
}

Vec3 Vec3::GetNormalizedPerpendicular() const {
  if (abs(mF32[0]) > abs(mF32[1])) {
    float len = sqrt(mF32[0] * mF32[0] + mF32[2] * mF32[2]);
    return Vec3(mF32[2], 0.0f, -mF32[0]) / len;
  } else {
    float len = sqrt(mF32[1] * mF32[1] + mF32[2] * mF32[2]);
    return Vec3(0.0f, mF32[2], -mF32[1]) / len;
  }
}

Vec3 Vec3::GetSign() const {
#if AMARYL_SIMD_AVX2
  return _mm_fixupimm_ps(mValue, mValue, _mm_set1_epi32(0xA9A90A00), 0);
#elif AMARYL_USE_SSE
  Type minus_one = _mm_set1_ps(-1.0f);
  Type one = _mm_set1_ps(1.0f);
  return _mm_or_ps(_mm_and_ps(mValue, minus_one), one);
#elif AMARYL_SIMD_NEON
  Type minus_one = vdupq_n_f32(-1.0f);
  Type one = vdupq_n_f32(1.0f);
  return vorrq_s32(vandq_s32(mValue, minus_one), one);
#else
  return Vec3(signbit(mF32[0]) ? -1.0f : 1.0f, signbit(mF32[1]) ? -1.0f : 1.0f,
              signbit(mF32[2]) ? -1.0f : 1.0f);
#endif
}

template <int X, int Y, int Z>
Vec3 Vec3::FlipSign() const {
  static_assert(X == 1 || X == -1, "X must be 1 or -1");
  static_assert(Y == 1 || Y == -1, "Y must be 1 or -1");
  static_assert(Z == 1 || Z == -1, "Z must be 1 or -1");
  return Vec3::sXor(
      *this, Vec3(X > 0 ? 0.0f : -0.0f, Y > 0 ? 0.0f : -0.0f, Z > 0 ? 0.0f : -0.0f));
}

}  // namespace amaryl

#pragma once

#include "amaryl/core/amaryl.h"
#include "amaryl/core/math/math_constants.h"
#include "amaryl/core/math/math_types.h"
#include "amaryl/core/math/vector_aliases.h"

namespace amaryl {

class [[nodiscard]] alignas(VECTOR_ALIGNMENT) Vec4 {
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

  /// Constructor
  Vec4() = default;  ///< Intentionally not initialized for performance reasons
  Vec4(const Vec4& inRHS) = default;
  Vec4& operator=(const Vec4& inRHS) = default;
  explicit FORCE_INLINE Vec4(Vec3Arg inRHS);  ///< WARNING: W component undefined!
  FORCE_INLINE Vec4(Vec3Arg inRHS, float inW);
  FORCE_INLINE Vec4(Type inRHS) : mValue(inRHS) {}

  /// Create a vector from 4 components
  FORCE_INLINE Vec4(float inX, float inY, float inZ, float inW);

  /// Vector with all zeros
  static FORCE_INLINE Vec4 sZero();

  /// Vector with all NaN's
  static FORCE_INLINE Vec4 sNaN();

  /// Replicate inV across all components
  static FORCE_INLINE Vec4 sReplicate(float inV);

  /// Load 4 floats from memory
  static FORCE_INLINE Vec4 sLoadFloat4(const float4* inV);

  /// Load 4 floats from memory, 16 bytes aligned
  static FORCE_INLINE Vec4 sLoadFloat4Aligned(const float4* inV);

  /// Gather 4 floats from memory at inBase + inOffsets[i] * Scale
  template <const int Scale>
  static FORCE_INLINE Vec4 sGatherFloat4(const float* inBase, UVec4Arg inOffsets);

  /// Return the minimum value of each of the components
  static FORCE_INLINE Vec4 sMin(Vec4Arg inV1, Vec4Arg inV2);

  /// Return the maximum of each of the components
  static FORCE_INLINE Vec4 sMax(Vec4Arg inV1, Vec4Arg inV2);

  /// Equals (component wise)
  static FORCE_INLINE UVec4 sEquals(Vec4Arg inV1, Vec4Arg inV2);

  /// Less than (component wise)
  static FORCE_INLINE UVec4 sLess(Vec4Arg inV1, Vec4Arg inV2);

  /// Less than or equal (component wise)
  static FORCE_INLINE UVec4 sLessOrEqual(Vec4Arg inV1, Vec4Arg inV2);

  /// Greater than (component wise)
  static FORCE_INLINE UVec4 sGreater(Vec4Arg inV1, Vec4Arg inV2);

  /// Greater than or equal (component wise)
  static FORCE_INLINE UVec4 sGreaterOrEqual(Vec4Arg inV1, Vec4Arg inV2);

  /// Calculates inMul1 * inMul2 + inAdd
  static FORCE_INLINE Vec4 sFusedMultiplyAdd(Vec4Arg inMul1, Vec4Arg inMul2,
                                             Vec4Arg inAdd);

  /// Component wise select, returns inV1 when highest bit of inControl = 0 and
  /// inV2 when highest bit of inControl = 1
  static FORCE_INLINE Vec4 sSelect(Vec4Arg inV1, Vec4Arg inV2, UVec4Arg inControl);

  /// Logical or (component wise)
  static FORCE_INLINE Vec4 sOr(Vec4Arg inV1, Vec4Arg inV2);

  /// Logical xor (component wise)
  static FORCE_INLINE Vec4 sXor(Vec4Arg inV1, Vec4Arg inV2);

  /// Logical and (component wise)
  static FORCE_INLINE Vec4 sAnd(Vec4Arg inV1, Vec4Arg inV2);

  /// Sort the four elements of ioValue and sort ioIndex at the same time.
  /// Based on a sorting network: http://en.wikipedia.org/wiki/Sorting_network
  static FORCE_INLINE void sSort4(Vec4& ioValue, UVec4& ioIndex);

  /// Reverse sort the four elements of ioValue (highest first) and sort ioIndex
  /// at the same time. Based on a sorting network:
  /// http://en.wikipedia.org/wiki/Sorting_network
  static FORCE_INLINE void sSort4Reverse(Vec4& ioValue, UVec4& ioIndex);

  /// Get individual components
#if AMARYL_USE_SSE
  FORCE_INLINE float GetX() const { return _mm_cvtss_f32(mValue); }
  FORCE_INLINE float GetY() const { return mF32[1]; }
  FORCE_INLINE float GetZ() const { return mF32[2]; }
  FORCE_INLINE float GetW() const { return mF32[3]; }
#elif AMARYL_SIMD_NEON
  FORCE_INLINE float GetX() const { return vgetq_lane_f32(mValue, 0); }
  FORCE_INLINE float GetY() const { return vgetq_lane_f32(mValue, 1); }
  FORCE_INLINE float GetZ() const { return vgetq_lane_f32(mValue, 2); }
  FORCE_INLINE float GetW() const { return vgetq_lane_f32(mValue, 3); }
#else
  FORCE_INLINE float GetX() const { return mF32[0]; }
  FORCE_INLINE float GetY() const { return mF32[1]; }
  FORCE_INLINE float GetZ() const { return mF32[2]; }
  FORCE_INLINE float GetW() const { return mF32[3]; }
#endif

  /// Set individual components
  FORCE_INLINE void SetX(float inX) { mF32[0] = inX; }
  FORCE_INLINE void SetY(float inY) { mF32[1] = inY; }
  FORCE_INLINE void SetZ(float inZ) { mF32[2] = inZ; }
  FORCE_INLINE void SetW(float inW) { mF32[3] = inW; }

  /// Set all components
  FORCE_INLINE void Set(float inX, float inY, float inZ, float inW) {
    *this = Vec4(inX, inY, inZ, inW);
  }

  /// Get float component by index
  FORCE_INLINE float operator[](uint32 inCoordinate) const {
    ASSERT(inCoordinate < 4);
    return mF32[inCoordinate];
  }
  FORCE_INLINE float& operator[](uint32 inCoordinate) {
    ASSERT(inCoordinate < 4);
    return mF32[inCoordinate];
  }

  /// Comparison
  FORCE_INLINE bool operator==(Vec4Arg inV2) const;
  FORCE_INLINE bool operator!=(Vec4Arg inV2) const { return !(*this == inV2); }

  /// Test if two vectors are close
  FORCE_INLINE bool IsClose(Vec4Arg inV2, float inMaxDistSq = 1.0e-12f) const;

  /// Test if vector is normalized
  FORCE_INLINE bool IsNormalized(float inTolerance = 1.0e-6f) const;

  /// Test if vector contains NaN elements
  FORCE_INLINE bool IsNaN() const;

  /// Multiply two float vectors (component wise)
  FORCE_INLINE Vec4 operator*(Vec4Arg inV2) const;

  /// Multiply vector with float
  FORCE_INLINE Vec4 operator*(float inV2) const;

  /// Multiply vector with float
  friend FORCE_INLINE Vec4 operator*(float inV1, Vec4Arg inV2);

  /// Divide vector by float
  FORCE_INLINE Vec4 operator/(float inV2) const;

  /// Multiply vector with float
  FORCE_INLINE Vec4& operator*=(float inV2);

  /// Multiply vector with vector
  FORCE_INLINE Vec4& operator*=(Vec4Arg inV2);

  /// Divide vector by float
  FORCE_INLINE Vec4& operator/=(float inV2);

  /// Add two float vectors (component wise)
  FORCE_INLINE Vec4 operator+(Vec4Arg inV2) const;

  /// Add two float vectors (component wise)
  FORCE_INLINE Vec4& operator+=(Vec4Arg inV2);

  /// Negate
  FORCE_INLINE Vec4 operator-() const;

  /// Subtract two float vectors (component wise)
  FORCE_INLINE Vec4 operator-(Vec4Arg inV2) const;

  /// Add two float vectors (component wise)
  FORCE_INLINE Vec4& operator-=(Vec4Arg inV2);

  /// Divide (component wise)
  FORCE_INLINE Vec4 operator/(Vec4Arg inV2) const;

  /// Swizzle the elements in inV
  template <uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ, uint32 SwizzleW>
  FORCE_INLINE Vec4 Swizzle() const;

  /// Replicate the X component to all components
  FORCE_INLINE Vec4 SplatX() const;

  /// Replicate the Y component to all components
  FORCE_INLINE Vec4 SplatY() const;

  /// Replicate the Z component to all components
  FORCE_INLINE Vec4 SplatZ() const;

  /// Replicate the W component to all components
  FORCE_INLINE Vec4 SplatW() const;

  /// Return the absolute value of each of the components
  FORCE_INLINE Vec4 Abs() const;

  /// Reciprocal vector (1 / value) for each of the components
  FORCE_INLINE Vec4 Reciprocal() const;

  /// Dot product, returns the dot product in X, Y and Z components
  FORCE_INLINE Vec4 DotV(Vec4Arg inV2) const;

  /// Dot product
  FORCE_INLINE float Dot(Vec4Arg inV2) const;

  /// Squared length of vector
  FORCE_INLINE float LengthSq() const;

  /// Length of vector
  FORCE_INLINE float Length() const;

  /// Normalize vector
  FORCE_INLINE Vec4 Normalized() const;

  /// Store 4 floats to memory
  FORCE_INLINE void StoreFloat4(float4* outV) const;

  /// Convert each component from a float to an int
  FORCE_INLINE UVec4 ToInt() const;

  /// Reinterpret Vec4 as a UVec4 (doesn't change the bits)
  FORCE_INLINE UVec4 ReinterpretAsInt() const;

  /// Store if X is negative in bit 0, Y in bit 1, Z in bit 2 and W in bit 3
  FORCE_INLINE int GetSignBits() const;

  /// Get the minimum of X, Y, Z and W
  FORCE_INLINE float ReduceMin() const;

  /// Get the maximum of X, Y, Z and W
  FORCE_INLINE float ReduceMax() const;

  /// Component wise square root
  FORCE_INLINE Vec4 Sqrt() const;

  /// Get Truncate result
  FORCE_INLINE Vec4 Trunc() const;

  /// Get vector that contains the sign of each element (returns 1.0f if
  /// positive, -1.0f if negative)
  FORCE_INLINE Vec4 GetSign() const;

  /// Flips the signs of the components, e.g. FlipSign<-1, 1, -1, 1>() will flip
  /// the signs of the X and Z components
  template <int X, int Y, int Z, int W>
  FORCE_INLINE Vec4 FlipSign() const;

  /// Calcluate the sine and cosine for each element of this vector (input in
  /// radians)
  inline void SinCos(Vec4& outSin, Vec4& outCos) const;

  /// Calcluate the tangent for each element of this vector (input in radians)
  inline Vec4 Tan() const;

  /// Calculate the arc sine for each element of this vector (returns value in
  /// the range [-PI / 2, PI / 2]) Note that all input values will be clamped to
  /// the range [-1, 1] and this function will not return NaNs like std::asin
  inline Vec4 ASin() const;

  /// Calculate the arc cosine for each element of this vector (returns value in
  /// the range [0, PI]) Note that all input values will be clamped to the range
  /// [-1, 1] and this function will not return NaNs like std::acos
  inline Vec4 ACos() const;

  /// Calculate the arc tangent for each element of this vector (returns value
  /// in the range [-PI / 2, PI / 2])
  inline Vec4 ATan() const;

  /// Calculate the arc tangent of y / x using the signs of the arguments to
  /// determine the correct quadrant (returns value in the range [-PI, PI])
  inline static Vec4 sATan2(Vec4Arg inY, Vec4Arg inX);

  union {
    Type mValue;
    float mF32[4];
  };
};

static_assert(std::is_trivial<Vec4>(), "Is supposed to be a trivial type!");

}  // namespace amaryl

#include "amaryl/core/math/trigonometry.h"
#include "amaryl/core/math/uvec4.h"
#include "amaryl/core/math/vec3.h"

namespace amaryl {

Vec4::Vec4(Vec3Arg inRHS) : mValue(inRHS.mValue) {}

Vec4::Vec4(Vec3Arg inRHS, float inW) {
#if AMARYL_USE_SSE4_1
  mValue = _mm_blend_ps(inRHS.mValue, _mm_set1_ps(inW), 8);
#elif AMARYL_SIMD_NEON
  mValue = vsetq_lane_f32(inW, inRHS.mValue, 3);
#else
  for (int i = 0; i < 3; i++) mF32[i] = inRHS.mF32[i];
  mF32[3] = inW;
#endif
}

Vec4::Vec4(float inX, float inY, float inZ, float inW) {
#if AMARYL_USE_SSE
  mValue = _mm_set_ps(inW, inZ, inY, inX);
#elif AMARYL_SIMD_NEON
  uint32x2_t xy =
      vcreate_f32(static_cast<uint64>(*reinterpret_cast<uint32*>(&inX)) |
                  (static_cast<uint64>(*reinterpret_cast<uint32*>(&inY)) << 32));
  uint32x2_t zw =
      vcreate_f32(static_cast<uint64>(*reinterpret_cast<uint32*>(&inZ)) |
                  (static_cast<uint64>(*reinterpret_cast<uint32*>(&inW)) << 32));
  mValue = vcombine_f32(xy, zw);
#else
  mF32[0] = inX;
  mF32[1] = inY;
  mF32[2] = inZ;
  mF32[3] = inW;
#endif
}

template <uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ, uint32 SwizzleW>
Vec4 Vec4::Swizzle() const {
  static_assert(SwizzleX <= 3, "SwizzleX template parameter out of range");
  static_assert(SwizzleY <= 3, "SwizzleY template parameter out of range");
  static_assert(SwizzleZ <= 3, "SwizzleZ template parameter out of range");
  static_assert(SwizzleW <= 3, "SwizzleW template parameter out of range");

#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue,
                        _MM_SHUFFLE(SwizzleW, SwizzleZ, SwizzleY, SwizzleX));
#elif AMARYL_SIMD_NEON
  return ARM_NEON_SHUFFLE_F32x4(mValue, mValue, SwizzleX, SwizzleY, SwizzleZ, SwizzleW);
#else
  return Vec4(mF32[SwizzleX], mF32[SwizzleY], mF32[SwizzleZ], mF32[SwizzleW]);
#endif
}

Vec4 Vec4::sZero() {
#if AMARYL_USE_SSE
  return _mm_setzero_ps();
#elif AMARYL_SIMD_NEON
  return vdupq_n_f32(0);
#else
  return Vec4(0, 0, 0, 0);
#endif
}

Vec4 Vec4::sReplicate(float inV) {
#if AMARYL_USE_SSE
  return _mm_set1_ps(inV);
#elif AMARYL_SIMD_NEON
  return vdupq_n_f32(inV);
#else
  return Vec4(inV, inV, inV, inV);
#endif
}

Vec4 Vec4::sNaN() { return sReplicate(std::numeric_limits<float>::quiet_NaN()); }

Vec4 Vec4::sLoadFloat4(const float4* inV) {
#if AMARYL_USE_SSE
  return _mm_loadu_ps(&inV->X());
#elif AMARYL_SIMD_NEON
  return vld1q_f32(&inV->X());
#else
  return Vec4(inV->X(), inV->Y(), inV->Z(), inV->W());
#endif
}

Vec4 Vec4::sLoadFloat4Aligned(const float4* inV) {
#if AMARYL_USE_SSE
  return _mm_load_ps(&inV->X());
#elif AMARYL_SIMD_NEON
  return vld1q_f32(&inV->X());
#else
  return Vec4(inV->X(), inV->Y(), inV->Z(), inV->W());
#endif
}

template <const int Scale>
Vec4 Vec4::sGatherFloat4(const float* inBase, UVec4Arg inOffsets) {
#if AMARYL_USE_SSE
#if AMARYL_SIMD_AVX2
  return _mm_i32gather_ps(inBase, inOffsets.mValue, Scale);
#else
  const uint8* base = reinterpret_cast<const uint8*>(inBase);
  Type x = _mm_load_ss(reinterpret_cast<const float*>(base + inOffsets.GetX() * Scale));
  Type y = _mm_load_ss(reinterpret_cast<const float*>(base + inOffsets.GetY() * Scale));
  Type xy = _mm_unpacklo_ps(x, y);
  Type z = _mm_load_ss(reinterpret_cast<const float*>(base + inOffsets.GetZ() * Scale));
  Type w = _mm_load_ss(reinterpret_cast<const float*>(base + inOffsets.GetW() * Scale));
  Type zw = _mm_unpacklo_ps(z, w);
  return _mm_movelh_ps(xy, zw);
#endif
#else
  const uint8* base = reinterpret_cast<const uint8*>(inBase);
  float x = *reinterpret_cast<const float*>(base + inOffsets.GetX() * Scale);
  float y = *reinterpret_cast<const float*>(base + inOffsets.GetY() * Scale);
  float z = *reinterpret_cast<const float*>(base + inOffsets.GetZ() * Scale);
  float w = *reinterpret_cast<const float*>(base + inOffsets.GetW() * Scale);
  return Vec4(x, y, z, w);
#endif
}

Vec4 Vec4::sMin(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_min_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vminq_f32(inV1.mValue, inV2.mValue);
#else
  return Vec4(
      amaryl::Min(inV1.mF32[0], inV2.mF32[0]), amaryl::Min(inV1.mF32[1], inV2.mF32[1]),
      amaryl::Min(inV1.mF32[2], inV2.mF32[2]), amaryl::Min(inV1.mF32[3], inV2.mF32[3]));
#endif
}

Vec4 Vec4::sMax(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_max_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vmaxq_f32(inV1.mValue, inV2.mValue);
#else
  return Vec4(
      amaryl::Max(inV1.mF32[0], inV2.mF32[0]), amaryl::Max(inV1.mF32[1], inV2.mF32[1]),
      amaryl::Max(inV1.mF32[2], inV2.mF32[2]), amaryl::Max(inV1.mF32[3], inV2.mF32[3]));
#endif
}

UVec4 Vec4::sEquals(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmpeq_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vceqq_f32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mF32[0] == inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] == inV2.mF32[1] ? 0xffffffffu : 0,
               inV1.mF32[2] == inV2.mF32[2] ? 0xffffffffu : 0,
               inV1.mF32[3] == inV2.mF32[3] ? 0xffffffffu : 0);
#endif
}

UVec4 Vec4::sLess(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmplt_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vcltq_f32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mF32[0] < inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] < inV2.mF32[1] ? 0xffffffffu : 0,
               inV1.mF32[2] < inV2.mF32[2] ? 0xffffffffu : 0,
               inV1.mF32[3] < inV2.mF32[3] ? 0xffffffffu : 0);
#endif
}

UVec4 Vec4::sLessOrEqual(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmple_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vcleq_f32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mF32[0] <= inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] <= inV2.mF32[1] ? 0xffffffffu : 0,
               inV1.mF32[2] <= inV2.mF32[2] ? 0xffffffffu : 0,
               inV1.mF32[3] <= inV2.mF32[3] ? 0xffffffffu : 0);
#endif
}

UVec4 Vec4::sGreater(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmpgt_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vcgtq_f32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mF32[0] > inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] > inV2.mF32[1] ? 0xffffffffu : 0,
               inV1.mF32[2] > inV2.mF32[2] ? 0xffffffffu : 0,
               inV1.mF32[3] > inV2.mF32[3] ? 0xffffffffu : 0);
#endif
}

UVec4 Vec4::sGreaterOrEqual(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_cmpge_ps(inV1.mValue, inV2.mValue));
#elif AMARYL_SIMD_NEON
  return vcgeq_f32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mF32[0] >= inV2.mF32[0] ? 0xffffffffu : 0,
               inV1.mF32[1] >= inV2.mF32[1] ? 0xffffffffu : 0,
               inV1.mF32[2] >= inV2.mF32[2] ? 0xffffffffu : 0,
               inV1.mF32[3] >= inV2.mF32[3] ? 0xffffffffu : 0);
#endif
}

Vec4 Vec4::sFusedMultiplyAdd(Vec4Arg inMul1, Vec4Arg inMul2, Vec4Arg inAdd) {
#if AMARYL_USE_SSE
  return _mm_add_ps(_mm_mul_ps(inMul1.mValue, inMul2.mValue), inAdd.mValue);
#elif AMARYL_SIMD_NEON
  return vmlaq_f32(inAdd.mValue, inMul1.mValue, inMul2.mValue);
#else
  return Vec4(inMul1.mF32[0] * inMul2.mF32[0] + inAdd.mF32[0],
              inMul1.mF32[1] * inMul2.mF32[1] + inAdd.mF32[1],
              inMul1.mF32[2] * inMul2.mF32[2] + inAdd.mF32[2],
              inMul1.mF32[3] * inMul2.mF32[3] + inAdd.mF32[3]);
#endif
}

Vec4 Vec4::sSelect(Vec4Arg inV1, Vec4Arg inV2, UVec4Arg inControl) {
#if AMARYL_USE_SSE4_1
  return _mm_blendv_ps(inV1.mValue, inV2.mValue, _mm_castsi128_ps(inControl.mValue));
#elif AMARYL_SIMD_NEON
  return vbslq_f32(vshrq_n_s32(inControl.mValue, 31), inV2.mValue, inV1.mValue);
#else
  Vec4 result;
  for (int i = 0; i < 4; i++)
    result.mF32[i] = inControl.mU32[i] ? inV2.mF32[i] : inV1.mF32[i];
  return result;
#endif
}

Vec4 Vec4::sOr(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_or_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vorrq_s32(inV1.mValue, inV2.mValue);
#else
  return UVec4::sOr(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt())
      .ReinterpretAsFloat();
#endif
}

Vec4 Vec4::sXor(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_xor_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return veorq_s32(inV1.mValue, inV2.mValue);
#else
  return UVec4::sXor(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt())
      .ReinterpretAsFloat();
#endif
}

Vec4 Vec4::sAnd(Vec4Arg inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_and_ps(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vandq_s32(inV1.mValue, inV2.mValue);
#else
  return UVec4::sAnd(inV1.ReinterpretAsInt(), inV2.ReinterpretAsInt())
      .ReinterpretAsFloat();
#endif
}

void Vec4::sSort4(Vec4& ioValue, UVec4& ioIndex) {
  // Pass 1, test 1st vs 3rd, 2nd vs 4th
  Vec4 v1 = ioValue.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>();
  UVec4 i1 = ioIndex.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>();
  UVec4 c1 = sLess(ioValue, v1).Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W>();
  ioValue = sSelect(ioValue, v1, c1);
  ioIndex = UVec4::sSelect(ioIndex, i1, c1);

  // Pass 2, test 1st vs 2nd, 3rd vs 4th
  Vec4 v2 = ioValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>();
  UVec4 i2 = ioIndex.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>();
  UVec4 c2 = sLess(ioValue, v2).Swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>();
  ioValue = sSelect(ioValue, v2, c2);
  ioIndex = UVec4::sSelect(ioIndex, i2, c2);

  // Pass 3, test 2nd vs 3rd component
  Vec4 v3 = ioValue.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>();
  UVec4 i3 = ioIndex.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>();
  UVec4 c3 = sLess(ioValue, v3).Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W>();
  ioValue = sSelect(ioValue, v3, c3);
  ioIndex = UVec4::sSelect(ioIndex, i3, c3);
}

void Vec4::sSort4Reverse(Vec4& ioValue, UVec4& ioIndex) {
  // Pass 1, test 1st vs 3rd, 2nd vs 4th
  Vec4 v1 = ioValue.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>();
  UVec4 i1 = ioIndex.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>();
  UVec4 c1 = sGreater(ioValue, v1).Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W>();
  ioValue = sSelect(ioValue, v1, c1);
  ioIndex = UVec4::sSelect(ioIndex, i1, c1);

  // Pass 2, test 1st vs 2nd, 3rd vs 4th
  Vec4 v2 = ioValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>();
  UVec4 i2 = ioIndex.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>();
  UVec4 c2 = sGreater(ioValue, v2).Swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>();
  ioValue = sSelect(ioValue, v2, c2);
  ioIndex = UVec4::sSelect(ioIndex, i2, c2);

  // Pass 3, test 2nd vs 3rd component
  Vec4 v3 = ioValue.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>();
  UVec4 i3 = ioIndex.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>();
  UVec4 c3 = sGreater(ioValue, v3).Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W>();
  ioValue = sSelect(ioValue, v3, c3);
  ioIndex = UVec4::sSelect(ioIndex, i3, c3);
}

bool Vec4::operator==(Vec4Arg inV2) const { return sEquals(*this, inV2).TestAllTrue(); }

bool Vec4::IsClose(Vec4Arg inV2, float inMaxDistSq) const {
  return (inV2 - *this).LengthSq() <= inMaxDistSq;
}

bool Vec4::IsNormalized(float inTolerance) const {
  return abs(LengthSq() - 1.0f) <= inTolerance;
}

bool Vec4::IsNaN() const {
#if AMARYL_SIMD_AVX2
  return _mm_fpclass_ps_mask(mValue, 0b10000001) != 0;
#elif AMARYL_USE_SSE
  return _mm_movemask_ps(_mm_cmpunord_ps(mValue, mValue)) != 0;
#elif AMARYL_SIMD_NEON
  uint32x4_t is_equal =
      vceqq_f32(mValue, mValue);  // If a number is not equal to itself it's a NaN
  return vaddvq_u32(vshrq_n_u32(is_equal, 31)) != 4;
#else
  return isnan(mF32[0]) || isnan(mF32[1]) || isnan(mF32[2]) || isnan(mF32[3]);
#endif
}

Vec4 Vec4::operator*(Vec4Arg inV2) const {
#if AMARYL_USE_SSE
  return _mm_mul_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vmulq_f32(mValue, inV2.mValue);
#else
  return Vec4(mF32[0] * inV2.mF32[0], mF32[1] * inV2.mF32[1], mF32[2] * inV2.mF32[2],
              mF32[3] * inV2.mF32[3]);
#endif
}

Vec4 Vec4::operator*(float inV2) const {
#if AMARYL_USE_SSE
  return _mm_mul_ps(mValue, _mm_set1_ps(inV2));
#elif AMARYL_SIMD_NEON
  return vmulq_n_f32(mValue, inV2);
#else
  return Vec4(mF32[0] * inV2, mF32[1] * inV2, mF32[2] * inV2, mF32[3] * inV2);
#endif
}

/// Multiply vector with float
Vec4 operator*(float inV1, Vec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_mul_ps(_mm_set1_ps(inV1), inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vmulq_n_f32(inV2.mValue, inV1);
#else
  return Vec4(inV1 * inV2.mF32[0], inV1 * inV2.mF32[1], inV1 * inV2.mF32[2],
              inV1 * inV2.mF32[3]);
#endif
}

Vec4 Vec4::operator/(float inV2) const {
#if AMARYL_USE_SSE
  return _mm_div_ps(mValue, _mm_set1_ps(inV2));
#elif AMARYL_SIMD_NEON
  return vdivq_f32(mValue, vdupq_n_f32(inV2));
#else
  return Vec4(mF32[0] / inV2, mF32[1] / inV2, mF32[2] / inV2, mF32[3] / inV2);
#endif
}

Vec4& Vec4::operator*=(float inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_mul_ps(mValue, _mm_set1_ps(inV2));
#elif AMARYL_SIMD_NEON
  mValue = vmulq_n_f32(mValue, inV2);
#else
  for (int i = 0; i < 4; ++i) mF32[i] *= inV2;
#endif
  return *this;
}

Vec4& Vec4::operator*=(Vec4Arg inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_mul_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  mValue = vmulq_f32(mValue, inV2.mValue);
#else
  for (int i = 0; i < 4; ++i) mF32[i] *= inV2.mF32[i];
#endif
  return *this;
}

Vec4& Vec4::operator/=(float inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_div_ps(mValue, _mm_set1_ps(inV2));
#elif AMARYL_SIMD_NEON
  mValue = vdivq_f32(mValue, vdupq_n_f32(inV2));
#else
  for (int i = 0; i < 4; ++i) mF32[i] /= inV2;
#endif
  return *this;
}

Vec4 Vec4::operator+(Vec4Arg inV2) const {
#if AMARYL_USE_SSE
  return _mm_add_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vaddq_f32(mValue, inV2.mValue);
#else
  return Vec4(mF32[0] + inV2.mF32[0], mF32[1] + inV2.mF32[1], mF32[2] + inV2.mF32[2],
              mF32[3] + inV2.mF32[3]);
#endif
}

Vec4& Vec4::operator+=(Vec4Arg inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_add_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  mValue = vaddq_f32(mValue, inV2.mValue);
#else
  for (int i = 0; i < 4; ++i) mF32[i] += inV2.mF32[i];
#endif
  return *this;
}

Vec4 Vec4::operator-() const {
#if AMARYL_USE_SSE
  return _mm_sub_ps(_mm_setzero_ps(), mValue);
#elif AMARYL_SIMD_NEON
  return vnegq_f32(mValue);
#else
  return Vec4(-mF32[0], -mF32[1], -mF32[2], -mF32[3]);
#endif
}

Vec4 Vec4::operator-(Vec4Arg inV2) const {
#if AMARYL_USE_SSE
  return _mm_sub_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vsubq_f32(mValue, inV2.mValue);
#else
  return Vec4(mF32[0] - inV2.mF32[0], mF32[1] - inV2.mF32[1], mF32[2] - inV2.mF32[2],
              mF32[3] - inV2.mF32[3]);
#endif
}

Vec4& Vec4::operator-=(Vec4Arg inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_sub_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  mValue = vsubq_f32(mValue, inV2.mValue);
#else
  for (int i = 0; i < 4; ++i) mF32[i] -= inV2.mF32[i];
#endif
  return *this;
}

Vec4 Vec4::operator/(Vec4Arg inV2) const {
#if AMARYL_USE_SSE
  return _mm_div_ps(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vdivq_f32(mValue, inV2.mValue);
#else
  return Vec4(mF32[0] / inV2.mF32[0], mF32[1] / inV2.mF32[1], mF32[2] / inV2.mF32[2],
              mF32[3] / inV2.mF32[3]);
#endif
}

Vec4 Vec4::SplatX() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(0, 0, 0, 0));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_f32(mValue, 0);
#else
  return Vec4(mF32[0], mF32[0], mF32[0], mF32[0]);
#endif
}

Vec4 Vec4::SplatY() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(1, 1, 1, 1));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_f32(mValue, 1);
#else
  return Vec4(mF32[1], mF32[1], mF32[1], mF32[1]);
#endif
}

Vec4 Vec4::SplatZ() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(2, 2, 2, 2));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_f32(mValue, 2);
#else
  return Vec4(mF32[2], mF32[2], mF32[2], mF32[2]);
#endif
}

Vec4 Vec4::SplatW() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_ps(mValue, mValue, _MM_SHUFFLE(3, 3, 3, 3));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_f32(mValue, 3);
#else
  return Vec4(mF32[3], mF32[3], mF32[3], mF32[3]);
#endif
}

Vec4 Vec4::Abs() const {
#if AMARYL_SIMD_AVX2
  return _mm_range_ps(mValue, mValue, 0b1000);
#elif AMARYL_USE_SSE
  return _mm_max_ps(_mm_sub_ps(_mm_setzero_ps(), mValue), mValue);
#elif AMARYL_SIMD_NEON
  return vabsq_f32(mValue);
#else
  return Vec4(abs(mF32[0]), abs(mF32[1]), abs(mF32[2]), abs(mF32[3]));
#endif
}

Vec4 Vec4::Reciprocal() const { return sReplicate(1.0f) / mValue; }

Vec4 Vec4::DotV(Vec4Arg inV2) const {
#if AMARYL_USE_SSE4_1
  return _mm_dp_ps(mValue, inV2.mValue, 0xff);
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
  return vdupq_n_f32(vaddvq_f32(mul));
#else
  // Brackets placed so that the order is consistent with the vectorized version
  return Vec4::sReplicate((mF32[0] * inV2.mF32[0] + mF32[1] * inV2.mF32[1]) +
                          (mF32[2] * inV2.mF32[2] + mF32[3] * inV2.mF32[3]));
#endif
}

float Vec4::Dot(Vec4Arg inV2) const {
#if AMARYL_USE_SSE4_1
  return _mm_cvtss_f32(_mm_dp_ps(mValue, inV2.mValue, 0xff));
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, inV2.mValue);
  return vaddvq_f32(mul);
#else
  // Brackets placed so that the order is consistent with the vectorized version
  return (mF32[0] * inV2.mF32[0] + mF32[1] * inV2.mF32[1]) +
         (mF32[2] * inV2.mF32[2] + mF32[3] * inV2.mF32[3]);
#endif
}

float Vec4::LengthSq() const {
#if AMARYL_USE_SSE4_1
  return _mm_cvtss_f32(_mm_dp_ps(mValue, mValue, 0xff));
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, mValue);
  return vaddvq_f32(mul);
#else
  // Brackets placed so that the order is consistent with the vectorized version
  return (mF32[0] * mF32[0] + mF32[1] * mF32[1]) +
         (mF32[2] * mF32[2] + mF32[3] * mF32[3]);
#endif
}

float Vec4::Length() const {
#if AMARYL_USE_SSE4_1
  return _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(mValue, mValue, 0xff)));
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, mValue);
  float32x2_t sum = vdup_n_f32(vaddvq_f32(mul));
  return vget_lane_f32(vsqrt_f32(sum), 0);
#else
  // Brackets placed so that the order is consistent with the vectorized version
  return sqrt((mF32[0] * mF32[0] + mF32[1] * mF32[1]) +
              (mF32[2] * mF32[2] + mF32[3] * mF32[3]));
#endif
}

Vec4 Vec4::Sqrt() const {
#if AMARYL_USE_SSE
  return _mm_sqrt_ps(mValue);
#elif AMARYL_SIMD_NEON
  return vsqrtq_f32(mValue);
#else
  return Vec4(sqrt(mF32[0]), sqrt(mF32[1]), sqrt(mF32[2]), sqrt(mF32[3]));
#endif
}

Vec4 Vec4::Trunc() const {
#if AMARYL_USE_SSE4_1
  return _mm_round_ps(mValue, _MM_FROUND_TRUNC);
#elif AMARYL_SIMD_NEON
  return vrndaq_f32(mValue);
#else
  return Vec4(std::round(mF32[0]), std::round(mF32[1]), std::round(mF32[2]),
              std::round(mF32[3]));
#endif
}

Vec4 Vec4::GetSign() const {
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
  return Vec4(signbit(mF32[0]) ? -1.0f : 1.0f, signbit(mF32[1]) ? -1.0f : 1.0f,
              signbit(mF32[2]) ? -1.0f : 1.0f, signbit(mF32[3]) ? -1.0f : 1.0f);
#endif
}

template <int X, int Y, int Z, int W>
FORCE_INLINE Vec4 Vec4::FlipSign() const {
  static_assert(X == 1 || X == -1, "X must be 1 or -1");
  static_assert(Y == 1 || Y == -1, "Y must be 1 or -1");
  static_assert(Z == 1 || Z == -1, "Z must be 1 or -1");
  static_assert(W == 1 || W == -1, "W must be 1 or -1");
  return Vec4::sXor(*this, Vec4(X > 0 ? 0.0f : -0.0f, Y > 0 ? 0.0f : -0.0f,
                                Z > 0 ? 0.0f : -0.0f, W > 0 ? 0.0f : -0.0f));
}

Vec4 Vec4::Normalized() const {
#if AMARYL_USE_SSE4_1
  return _mm_div_ps(mValue, _mm_sqrt_ps(_mm_dp_ps(mValue, mValue, 0xff)));
#elif AMARYL_SIMD_NEON
  float32x4_t mul = vmulq_f32(mValue, mValue);
  float32x4_t sum = vdupq_n_f32(vaddvq_f32(mul));
  return vdivq_f32(mValue, vsqrtq_f32(sum));
#else
  return *this / Length();
#endif
}

void Vec4::StoreFloat4(float4* outV) const {
#if AMARYL_USE_SSE
  _mm_storeu_ps(&outV->X(), mValue);
#elif AMARYL_SIMD_NEON
  vst1q_f32(&outV->X(), mValue);
#else
  for (int i = 0; i < 4; ++i) (&outV->X())[i] = mF32[i];
#endif
}

UVec4 Vec4::ToInt() const {
#if AMARYL_USE_SSE
  return _mm_cvttps_epi32(mValue);
#elif AMARYL_SIMD_NEON
  return vcvtq_u32_f32(mValue);
#else
  return UVec4(uint32(mF32[0]), uint32(mF32[1]), uint32(mF32[2]), uint32(mF32[3]));
#endif
}

UVec4 Vec4::ReinterpretAsInt() const {
#if AMARYL_USE_SSE
  return UVec4(_mm_castps_si128(mValue));
#elif AMARYL_SIMD_NEON
  return vreinterpretq_u32_f32(mValue);
#else
  return *reinterpret_cast<const UVec4*>(this);
#endif
}

int Vec4::GetSignBits() const {
#if AMARYL_USE_SSE
  return _mm_movemask_ps(mValue);
#elif AMARYL_SIMD_NEON
  int32x4_t shift = ARM_NEON_INT32x4(0, 1, 2, 3);
  return vaddvq_u32(vshlq_u32(vshrq_n_u32(vreinterpretq_u32_f32(mValue), 31), shift));
#else
  return (signbit(mF32[0]) ? 1 : 0) | (signbit(mF32[1]) ? 2 : 0) |
         (signbit(mF32[2]) ? 4 : 0) | (signbit(mF32[3]) ? 8 : 0);
#endif
}

float Vec4::ReduceMin() const {
  Vec4 v = sMin(mValue, Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_W, SWIZZLE_UNUSED>());
  v = sMin(v, v.Swizzle<SWIZZLE_Z, SWIZZLE_UNUSED, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());
  return v.GetX();
}

float Vec4::ReduceMax() const {
  Vec4 v = sMax(mValue, Swizzle<SWIZZLE_Y, SWIZZLE_UNUSED, SWIZZLE_W, SWIZZLE_UNUSED>());
  v = sMax(v, v.Swizzle<SWIZZLE_Z, SWIZZLE_UNUSED, SWIZZLE_UNUSED, SWIZZLE_UNUSED>());
  return v.GetX();
}

void Vec4::SinCos(Vec4& outSin, Vec4& outCos) const {
  // Implementation based on sinf.c from the cephes library, combines sinf and
  // cosf in a single function, changes octants to quadrants and vectorizes it
  // Original implementation by Stephen L. Moshier (See:
  // http://www.moshier.net/)

  // Make argument positive and remember sign for sin only since cos is
  // symmetric around x (highest bit of a float is the sign bit)
  UVec4 sin_sign = UVec4::sAnd(ReinterpretAsInt(), UVec4::sReplicate(0x80000000U));
  Vec4 x = Vec4::sXor(*this, sin_sign.ReinterpretAsFloat());

  // x / (PI / 2) rounded to nearest int gives us the quadrant closest to x
  UVec4 quadrant = (0.6366197723675814f * x + Vec4::sReplicate(0.5f)).ToInt();

  // Make x relative to the closest quadrant.
  // This does x = x - quadrant * PI / 2 using a two step Cody-Waite argument
  // reduction. This improves the accuracy of the result by avoiding loss of
  // significant bits in the subtraction. We start with x = x - quadrant * PI /
  // 2, PI / 2 in hexadecimal notation is 0x3fc90fdb, we remove the lowest 16
  // bits to get 0x3fc90000 (= 1.5703125) this means we can now multiply with a
  // number of up to 2^16 without losing any bits. This leaves us with: x = (x -
  // quadrant * 1.5703125) - quadrant * (PI / 2 - 1.5703125). PI / 2 - 1.5703125
  // in hexadecimal is 0x39fdaa22, stripping the lowest 12 bits we get
  // 0x39fda000 (= 0.0004837512969970703125) This leaves uw with: x = ((x -
  // quadrant * 1.5703125) - quadrant * 0.0004837512969970703125) - quadrant *
  // (PI / 2 - 1.5703125 - 0.0004837512969970703125) See:
  // https://stackoverflow.com/questions/42455143/sine-cosine-modular-extended-precision-arithmetic
  // After this we have x in the range [-PI / 4, PI / 4].
  Vec4 float_quadrant = quadrant.ToFloat();
  x = ((x - float_quadrant * 1.5703125f) - float_quadrant * 0.0004837512969970703125f) -
      float_quadrant * 7.549789948768648e-8f;

  // Calculate x2 = x^2
  Vec4 x2 = x * x;

  // Taylor expansion:
  // Cos(x) = 1 - x^2/2! + x^4/4! - x^6/6! + x^8/8! + ... = (((x2/8!- 1/6!) * x2
  // + 1/4!) * x2 - 1/2!) * x2 + 1
  Vec4 taylor_cos =
      ((2.443315711809948e-5f * x2 - Vec4::sReplicate(1.388731625493765e-3f)) * x2 +
       Vec4::sReplicate(4.166664568298827e-2f)) *
          x2 * x2 -
      0.5f * x2 + Vec4::sReplicate(1.0f);
  // Sin(x) = x - x^3/3! + x^5/5! - x^7/7! + ... = ((-x2/7! + 1/5!) * x2 - 1/3!)
  // * x2 * x + x
  Vec4 taylor_sin = ((-1.9515295891e-4f * x2 + Vec4::sReplicate(8.3321608736e-3f)) * x2 -
                     Vec4::sReplicate(1.6666654611e-1f)) *
                        x2 * x +
                    x;

  // The lowest 2 bits of quadrant indicate the quadrant that we are in.
  // Let x be the original input value and x' our value that has been mapped to
  // the range [-PI / 4, PI / 4]. since cos(x) = sin(x - PI / 2) and since we
  // want to use the Taylor expansion as close as possible to 0, we can
  // alternate between using the Taylor expansion for sin and cos according to
  // the following table:
  //
  // quadrant	 sin(x)		 cos(x)
  // XXX00b	 sin(x')	 cos(x')
  // XXX01b	 cos(x')	-sin(x')
  // XXX10b	-sin(x')	-cos(x')
  // XXX11b	-cos(x')	 sin(x')
  //
  // So: sin_sign = bit2, cos_sign = bit1 ^ bit2, bit1 determines if we use sin
  // or cos Taylor expansion
  UVec4 bit1 = quadrant.LogicalShiftLeft<31>();
  UVec4 bit2 =
      UVec4::sAnd(quadrant.LogicalShiftLeft<30>(), UVec4::sReplicate(0x80000000U));

  // Select which one of the results is sin and which one is cos
  Vec4 s = Vec4::sSelect(taylor_sin, taylor_cos, bit1);
  Vec4 c = Vec4::sSelect(taylor_cos, taylor_sin, bit1);

  // Update the signs
  sin_sign = UVec4::sXor(sin_sign, bit2);
  UVec4 cos_sign = UVec4::sXor(bit1, bit2);

  // Correct the signs
  outSin = Vec4::sXor(s, sin_sign.ReinterpretAsFloat());
  outCos = Vec4::sXor(c, cos_sign.ReinterpretAsFloat());
}

Vec4 Vec4::Tan() const {
  // Implementation based on tanf.c from the cephes library, see Vec4::SinCos
  // for further details Original implementation by Stephen L. Moshier (See:
  // http://www.moshier.net/)

  // Make argument positive
  UVec4 tan_sign = UVec4::sAnd(ReinterpretAsInt(), UVec4::sReplicate(0x80000000U));
  Vec4 x = Vec4::sXor(*this, tan_sign.ReinterpretAsFloat());

  // x / (PI / 2) rounded to nearest int gives us the quadrant closest to x
  UVec4 quadrant = (0.6366197723675814f * x + Vec4::sReplicate(0.5f)).ToInt();

  // Remap x to range [-PI / 4, PI / 4], see Vec4::SinCos
  Vec4 float_quadrant = quadrant.ToFloat();
  x = ((x - float_quadrant * 1.5703125f) - float_quadrant * 0.0004837512969970703125f) -
      float_quadrant * 7.549789948768648e-8f;

  // Calculate x2 = x^2
  Vec4 x2 = x * x;

  // Roughly equivalent to the Taylor expansion:
  // Tan(x) = x + x^3/3 + 2*x^5/15 + 17*x^7/315 + 62*x^9/2835 + ...
  Vec4 tan = (((((9.38540185543e-3f * x2 + Vec4::sReplicate(3.11992232697e-3f)) * x2 +
                 Vec4::sReplicate(2.44301354525e-2f)) *
                    x2 +
                Vec4::sReplicate(5.34112807005e-2f)) *
                   x2 +
               Vec4::sReplicate(1.33387994085e-1f)) *
                  x2 +
              Vec4::sReplicate(3.33331568548e-1f)) *
                 x2 * x +
             x;

  // For the 2nd and 4th quadrant we need to invert the value
  UVec4 bit1 = quadrant.LogicalShiftLeft<31>();
  tan = Vec4::sSelect(tan, Vec4::sReplicate(-1.0f) / tan, bit1);

  // Put the sign back
  return Vec4::sXor(tan, tan_sign.ReinterpretAsFloat());
}

Vec4 Vec4::ASin() const {
  // Implementation based on asinf.c from the cephes library
  // Original implementation by Stephen L. Moshier (See:
  // http://www.moshier.net/)

  // Make argument positive
  UVec4 asin_sign = UVec4::sAnd(ReinterpretAsInt(), UVec4::sReplicate(0x80000000U));
  Vec4 a = Vec4::sXor(*this, asin_sign.ReinterpretAsFloat());

  // ASin is not defined outside the range [-1, 1] but it often happens that a
  // value is slightly above 1 so we just clamp here
  a = Vec4::sMin(a, Vec4::sReplicate(1.0f));

  // When |x| <= 0.5 we use the asin approximation as is
  Vec4 z1 = a * a;
  Vec4 x1 = a;

  // When |x| > 0.5 we use the identity asin(x) = PI / 2 - 2 * asin(sqrt((1 - x)
  // / 2))
  Vec4 z2 = 0.5f * (Vec4::sReplicate(1.0f) - a);
  Vec4 x2 = z2.Sqrt();

  // Select which of the two situations we have
  UVec4 greater = Vec4::sGreater(a, Vec4::sReplicate(0.5f));
  Vec4 z = Vec4::sSelect(z1, z2, greater);
  Vec4 x = Vec4::sSelect(x1, x2, greater);

  // Polynomial approximation of asin
  z = ((((4.2163199048e-2f * z + Vec4::sReplicate(2.4181311049e-2f)) * z +
         Vec4::sReplicate(4.5470025998e-2f)) *
            z +
        Vec4::sReplicate(7.4953002686e-2f)) *
           z +
       Vec4::sReplicate(1.6666752422e-1f)) *
          z * x +
      x;

  // If |x| > 0.5 we need to apply the remainder of the identity above
  z = Vec4::sSelect(z, Vec4::sReplicate(0.5f * PI) - (z + z), greater);

  // Put the sign back
  return Vec4::sXor(z, asin_sign.ReinterpretAsFloat());
}

Vec4 Vec4::ACos() const {
  // Not the most accurate, but simple
  return Vec4::sReplicate(0.5f * PI) - ASin();
}

Vec4 Vec4::ATan() const {
  // Implementation based on atanf.c from the cephes library Original
  // implementation by Stephen L. Moshier (See: http://www.moshier.net/)

  // Make argument positive
  UVec4 atan_sign = UVec4::sAnd(ReinterpretAsInt(), UVec4::sReplicate(0x80000000U));
  Vec4 x = Vec4::sXor(*this, atan_sign.ReinterpretAsFloat());
  Vec4 y = Vec4::sZero();

  // If x > Tan(PI / 8)
  UVec4 greater1 = Vec4::sGreater(x, Vec4::sReplicate(0.4142135623730950f));
  Vec4 x1 = (x - Vec4::sReplicate(1.0f)) / (x + Vec4::sReplicate(1.0f));

  // If x > Tan(3 * PI / 8)
  UVec4 greater2 = Vec4::sGreater(x, Vec4::sReplicate(2.414213562373095f));
  Vec4 x2 = Vec4::sReplicate(-1.0f) / (x);

  // Apply first if
  x = Vec4::sSelect(x, x1, greater1);
  y = Vec4::sSelect(y, Vec4::sReplicate(0.25f * PI), greater1);

  // Apply second if
  x = Vec4::sSelect(x, x2, greater2);
  y = Vec4::sSelect(y, Vec4::sReplicate(0.5f * PI), greater2);

  // Polynomial approximation
  Vec4 z = x * x;
  y += (((8.05374449538e-2f * z - Vec4::sReplicate(1.38776856032e-1f)) * z +
         Vec4::sReplicate(1.99777106478e-1f)) *
            z -
        Vec4::sReplicate(3.33329491539e-1f)) *
           z * x +
       x;

  // Put the sign back
  return Vec4::sXor(y, atan_sign.ReinterpretAsFloat());
}

Vec4 Vec4::sATan2(Vec4Arg inY, Vec4Arg inX) {
  UVec4 sign_mask = UVec4::sReplicate(0x80000000U);

  // Determine absolute value and sign of y
  UVec4 y_sign = UVec4::sAnd(inY.ReinterpretAsInt(), sign_mask);
  Vec4 y_abs = Vec4::sXor(inY, y_sign.ReinterpretAsFloat());

  // Determine absolute value and sign of x
  UVec4 x_sign = UVec4::sAnd(inX.ReinterpretAsInt(), sign_mask);
  Vec4 x_abs = Vec4::sXor(inX, x_sign.ReinterpretAsFloat());

  // Always divide smallest / largest to avoid dividing by zero
  UVec4 x_is_numerator = Vec4::sLess(x_abs, y_abs);
  Vec4 numerator = Vec4::sSelect(y_abs, x_abs, x_is_numerator);
  Vec4 denominator = Vec4::sSelect(x_abs, y_abs, x_is_numerator);
  Vec4 atan = (numerator / denominator).ATan();

  // If we calculated x / y instead of y / x the result is PI / 2 - result (note
  // that this is true because we know the result is positive because the input
  // was positive)
  atan = Vec4::sSelect(atan, Vec4::sReplicate(0.5f * PI) - atan, x_is_numerator);

  // Now we need to map to the correct quadrant
  // x_sign	y_sign	result
  // +1		+1		atan
  // -1		+1		-atan + PI
  // -1		-1		atan - PI
  // +1		-1		-atan
  // This can be written as: x_sign * y_sign * (atan - (x_sign < 0? PI : 0))
  atan -= Vec4::sAnd(x_sign.ArithmeticShiftRight<31>().ReinterpretAsFloat(),
                     Vec4::sReplicate(PI));
  atan = Vec4::sXor(atan, UVec4::sXor(x_sign, y_sign).ReinterpretAsFloat());
  return atan;
}

}  // namespace amaryl

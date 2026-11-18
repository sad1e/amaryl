#pragma once

#include "amaryl/core/math/math_utils.h"
#include "amaryl/core/math/vector_aliases.h"

namespace amaryl {

class [[nodiscard]] alignas(VECTOR_ALIGNMENT) UVec4 {
 public:
  AMARYL_MATH_OVERRIDE_NEW_DELETE

  // Underlying vector type
#if AMARYL_USE_SSE
  using Type = __m128i;
#elif AMARYL_SIMD_NEON
  using Type = u32x4_t;
#else
  using Type = struct {
    uint32 mData[4];
  };
#endif

  /// Constructor
  UVec4() = default;  ///< Intentionally not initialized for performance reasons
  UVec4(const UVec4& inRHS) = default;
  UVec4& operator=(const UVec4& inRHS) = default;
  FORCE_INLINE UVec4(Type inRHS) : mValue(inRHS) {}

  /// Create a vector from 4 integer components
  FORCE_INLINE UVec4(uint32 inX, uint32 inY, uint32 inZ, uint32 inW);

  /// Comparison
  FORCE_INLINE bool operator==(UVec4Arg inV2) const;
  FORCE_INLINE bool operator!=(UVec4Arg inV2) const { return !(*this == inV2); }

  /// Swizzle the elements in inV
  template <uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ, uint32 SwizzleW>
  FORCE_INLINE UVec4 Swizzle() const;

  /// Vector with all zeros
  static FORCE_INLINE UVec4 sZero();

  /// Replicate int inV across all components
  static FORCE_INLINE UVec4 sReplicate(uint32 inV);

  /// Load 1 int from memory and place it in the X component, zeros Y, Z and W
  static FORCE_INLINE UVec4 sLoadInt(const uint32* inV);

  /// Load 4 ints from memory
  static FORCE_INLINE UVec4 sLoadInt4(const uint32* inV);

  /// Load 4 ints from memory, aligned to 16 bytes
  static FORCE_INLINE UVec4 sLoadInt4Aligned(const uint32* inV);

  /// Gather 4 ints from memory at inBase + inOffsets[i] * Scale
  template <const int Scale>
  static FORCE_INLINE UVec4 sGatherInt4(const uint32* inBase, UVec4Arg inOffsets);

  /// Return the minimum value of each of the components
  static FORCE_INLINE UVec4 sMin(UVec4Arg inV1, UVec4Arg inV2);

  /// Return the maximum of each of the components
  static FORCE_INLINE UVec4 sMax(UVec4Arg inV1, UVec4Arg inV2);

  /// Equals (component wise)
  static FORCE_INLINE UVec4 sEquals(UVec4Arg inV1, UVec4Arg inV2);

  /// Component wise select, returns inV1 when highest bit of inControl = 0 and
  /// inV2 when highest bit of inControl = 1
  static FORCE_INLINE UVec4 sSelect(UVec4Arg inV1, UVec4Arg inV2, UVec4Arg inControl);

  /// Logical or (component wise)
  static FORCE_INLINE UVec4 sOr(UVec4Arg inV1, UVec4Arg inV2);

  /// Logical xor (component wise)
  static FORCE_INLINE UVec4 sXor(UVec4Arg inV1, UVec4Arg inV2);

  /// Logical and (component wise)
  static FORCE_INLINE UVec4 sAnd(UVec4Arg inV1, UVec4Arg inV2);

  /// Logical not (component wise)
  static FORCE_INLINE UVec4 sNot(UVec4Arg inV1);

  /// Sorts the elements in inIndex so that the values that correspond to trues
  /// in inValue are the first elements. The remaining elements will be set to
  /// inValue.w. I.e. if inValue = (true, false, true, false) and inIndex = (1,
  /// 2, 3, 4) the function returns (1, 3, 4, 4).
  static FORCE_INLINE UVec4 sSort4True(UVec4Arg inValue, UVec4Arg inIndex);

  /// Get individual components
#if AMARYL_USE_SSE
  FORCE_INLINE uint32 GetX() const { return uint32(_mm_cvtsi128_si32(mValue)); }
  FORCE_INLINE uint32 GetY() const { return mU32[1]; }
  FORCE_INLINE uint32 GetZ() const { return mU32[2]; }
  FORCE_INLINE uint32 GetW() const { return mU32[3]; }
#elif AMARYL_SIMD_NEON
  FORCE_INLINE uint32 GetX() const { return vgetq_lane_u32(mValue, 0); }
  FORCE_INLINE uint32 GetY() const { return vgetq_lane_u32(mValue, 1); }
  FORCE_INLINE uint32 GetZ() const { return vgetq_lane_u32(mValue, 2); }
  FORCE_INLINE uint32 GetW() const { return vgetq_lane_u32(mValue, 3); }
#else
  FORCE_INLINE uint32 GetX() const { return mU32[0]; }
  FORCE_INLINE uint32 GetY() const { return mU32[1]; }
  FORCE_INLINE uint32 GetZ() const { return mU32[2]; }
  FORCE_INLINE uint32 GetW() const { return mU32[3]; }
#endif

  /// Set individual components
  FORCE_INLINE void SetX(uint32 inX) { mU32[0] = inX; }
  FORCE_INLINE void SetY(uint32 inY) { mU32[1] = inY; }
  FORCE_INLINE void SetZ(uint32 inZ) { mU32[2] = inZ; }
  FORCE_INLINE void SetW(uint32 inW) { mU32[3] = inW; }

  /// Get component by index
  FORCE_INLINE uint32 operator[](uint32 inCoordinate) const {
    ASSERT(inCoordinate < 4);
    return mU32[inCoordinate];
  }
  FORCE_INLINE uint32& operator[](uint32 inCoordinate) {
    ASSERT(inCoordinate < 4);
    return mU32[inCoordinate];
  }

  /// Multiplies each of the 4 integer components with an integer (discards any
  /// overflow)
  FORCE_INLINE UVec4 operator*(UVec4Arg inV2) const;

  /// Adds an integer value to all integer components (discards any overflow)
  FORCE_INLINE UVec4 operator+(UVec4Arg inV2);

  /// Add two integer vectors (component wise)
  FORCE_INLINE UVec4& operator+=(UVec4Arg inV2);

  /// Replicate the X component to all components
  FORCE_INLINE UVec4 SplatX() const;

  /// Replicate the Y component to all components
  FORCE_INLINE UVec4 SplatY() const;

  /// Replicate the Z component to all components
  FORCE_INLINE UVec4 SplatZ() const;

  /// Replicate the W component to all components
  FORCE_INLINE UVec4 SplatW() const;

  /// Convert each component from an int to a float
  FORCE_INLINE Vec4 ToFloat() const;

  /// Reinterpret UVec4 as a Vec4 (doesn't change the bits)
  FORCE_INLINE Vec4 ReinterpretAsFloat() const;

  /// Store 4 ints to memory
  FORCE_INLINE void StoreInt4(uint32* outV) const;

  /// Store 4 ints to memory, aligned to 16 bytes
  FORCE_INLINE void StoreInt4Aligned(uint32* outV) const;

  /// Test if any of the components are true (true is when highest bit of
  /// component is set)
  FORCE_INLINE bool TestAnyTrue() const;

  /// Test if any of X, Y or Z components are true (true is when highest bit of
  /// component is set)
  FORCE_INLINE bool TestAnyXYZTrue() const;

  /// Test if all components are true (true is when highest bit of component is
  /// set)
  FORCE_INLINE bool TestAllTrue() const;

  /// Test if X, Y and Z components are true (true is when highest bit of
  /// component is set)
  FORCE_INLINE bool TestAllXYZTrue() const;

  /// Count the number of components that are true (true is when highest bit of
  /// component is set)
  FORCE_INLINE int CountTrues() const;

  /// Store if X is true in bit 0, Y in bit 1, Z in bit 2 and W in bit 3 (true
  /// is when highest bit of component is set)
  FORCE_INLINE int GetTrues() const;

  /// Shift all components by Count bits to the left (filling with zeros from
  /// the left)
  template <const uint32 Count>
  FORCE_INLINE UVec4 LogicalShiftLeft() const;

  /// Shift all components by Count bits to the right (filling with zeros from
  /// the right)
  template <const uint32 Count>
  FORCE_INLINE UVec4 LogicalShiftRight() const;

  /// Shift all components by Count bits to the right (shifting in the value of
  /// the highest bit)
  template <const uint32 Count>
  FORCE_INLINE UVec4 ArithmeticShiftRight() const;

  /// Takes the lower 4 16 bits and expands them to X, Y, Z and W
  FORCE_INLINE UVec4 Expand4Uint16Lo() const;

  /// Takes the upper 4 16 bits and expands them to X, Y, Z and W
  FORCE_INLINE UVec4 Expand4Uint16Hi() const;

  /// Takes byte 0 .. 3 and expands them to X, Y, Z and W
  FORCE_INLINE UVec4 Expand4Byte0() const;

  /// Takes byte 4 .. 7 and expands them to X, Y, Z and W
  FORCE_INLINE UVec4 Expand4Byte4() const;

  /// Takes byte 8 .. 11 and expands them to X, Y, Z and W
  FORCE_INLINE UVec4 Expand4Byte8() const;

  /// Takes byte 12 .. 15 and expands them to X, Y, Z and W
  FORCE_INLINE UVec4 Expand4Byte12() const;

  /// Shift vector components by 4 - Count floats to the left, so if Count = 1
  /// the resulting vector is (W, 0, 0, 0), when Count = 3 the resulting vector
  /// is (Y, Z, W, 0)
  FORCE_INLINE UVec4 ShiftComponents4Minus(int inCount) const;

  union {
    Type mValue;
    uint32 mU32[4];
  };
};

static_assert(std::is_trivial<UVec4>(), "Is supposed to be a trivial type!");

}  // namespace amaryl

#include "amaryl/core/math/vec4.h"

namespace amaryl {

UVec4::UVec4(uint32 inX, uint32 inY, uint32 inZ, uint32 inW) {
#if AMARYL_USE_SSE
  mValue = _mm_set_epi32(int(inW), int(inZ), int(inY), int(inX));
#elif AMARYL_SIMD_NEON
  uint32x2_t xy = vcreate_u32(static_cast<u3264>(inX) | (static_cast<u3264>(inY) << 32));
  uint32x2_t zw = vcreate_u32(static_cast<u3264>(inZ) | (static_cast<u3264>(inW) << 32));
  mValue = vcombine_u32(xy, zw);
#else
  mU32[0] = inX;
  mU32[1] = inY;
  mU32[2] = inZ;
  mU32[3] = inW;
#endif
}

bool UVec4::operator==(UVec4Arg inV2) const { return sEquals(*this, inV2).TestAllTrue(); }

template <uint32 SwizzleX, uint32 SwizzleY, uint32 SwizzleZ, uint32 SwizzleW>
UVec4 UVec4::Swizzle() const {
  static_assert(SwizzleX <= 3, "SwizzleX template parameter out of range");
  static_assert(SwizzleY <= 3, "SwizzleY template parameter out of range");
  static_assert(SwizzleZ <= 3, "SwizzleZ template parameter out of range");
  static_assert(SwizzleW <= 3, "SwizzleW template parameter out of range");

#if AMARYL_USE_SSE
  return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(SwizzleW, SwizzleZ, SwizzleY, SwizzleX));
#elif AMARYL_SIMD_NEON
  return ARM_NEON_SHUFFLE_F32x4(mValue, mValue, SwizzleX, SwizzleY, SwizzleZ, SwizzleW);
#else
  return UVec4(mU32[SwizzleX], mU32[SwizzleY], mU32[SwizzleZ], mU32[SwizzleW]);
#endif
}

UVec4 UVec4::sZero() {
#if AMARYL_USE_SSE
  return _mm_setzero_si128();
#elif AMARYL_SIMD_NEON
  return vdupq_n_u32(0);
#else
  return UVec4(0, 0, 0, 0);
#endif
}

UVec4 UVec4::sReplicate(uint32 inV) {
#if AMARYL_USE_SSE
  return _mm_set1_epi32(int(inV));
#elif AMARYL_SIMD_NEON
  return vdupq_n_u32(inV);
#else
  return UVec4(inV, inV, inV, inV);
#endif
}

UVec4 UVec4::sLoadInt(const uint32* inV) {
#if AMARYL_USE_SSE
  return _mm_castps_si128(_mm_load_ss(reinterpret_cast<const float*>(inV)));
#elif AMARYL_SIMD_NEON
  return vsetq_lane_u32(*inV, vdupq_n_u32(0), 0);
#else
  return UVec4(*inV, 0, 0, 0);
#endif
}

UVec4 UVec4::sLoadInt4(const uint32* inV) {
#if AMARYL_USE_SSE
  return _mm_loadu_si128(reinterpret_cast<const __m128i*>(inV));
#elif AMARYL_SIMD_NEON
  return vld1q_u32(inV);
#else
  return UVec4(inV[0], inV[1], inV[2], inV[3]);
#endif
}

UVec4 UVec4::sLoadInt4Aligned(const uint32* inV) {
#if AMARYL_USE_SSE
  return _mm_load_si128(reinterpret_cast<const __m128i*>(inV));
#elif AMARYL_SIMD_NEON
  return vld1q_u32(inV);  // ARM doesn't make distinction between aligned or not
#else
  return UVec4(inV[0], inV[1], inV[2], inV[3]);
#endif
}

template <const int Scale>
UVec4 UVec4::sGatherInt4(const uint32* inBase, UVec4Arg inOffsets) {
#if AMARYL_SIMD_AVX2
  return _mm_i32gather_epi32(reinterpret_cast<const int*>(inBase), inOffsets.mValue,
                             Scale);
#else
  return Vec4::sGatherFloat4<Scale>(reinterpret_cast<const float*>(inBase), inOffsets)
      .ReinterpretAsInt();
#endif
}

UVec4 UVec4::sMin(UVec4Arg inV1, UVec4Arg inV2) {
#if AMARYL_USE_SSE4_1
  return _mm_min_epu32(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vminq_u32(inV1.mValue, inV2.mValue);
#else
  UVec4 result;
  for (int i = 0; i < 4; i++) result.mU32[i] = amaryl::Min(inV1.mU32[i], inV2.mU32[i]);
  return result;
#endif
}

UVec4 UVec4::sMax(UVec4Arg inV1, UVec4Arg inV2) {
#if AMARYL_USE_SSE4_1
  return _mm_max_epu32(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vmaxq_u32(inV1.mValue, inV2.mValue);
#else
  UVec4 result;
  for (int i = 0; i < 4; i++) result.mU32[i] = amaryl::Max(inV1.mU32[i], inV2.mU32[i]);
  return result;
#endif
}

UVec4 UVec4::sEquals(UVec4Arg inV1, UVec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_cmpeq_epi32(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vceqq_u32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mU32[0] == inV2.mU32[0] ? 0xffffffffu : 0,
               inV1.mU32[1] == inV2.mU32[1] ? 0xffffffffu : 0,
               inV1.mU32[2] == inV2.mU32[2] ? 0xffffffffu : 0,
               inV1.mU32[3] == inV2.mU32[3] ? 0xffffffffu : 0);
#endif
}

UVec4 UVec4::sSelect(UVec4Arg inV1, UVec4Arg inV2, UVec4Arg inControl) {
#if AMARYL_USE_SSE4_1
  return _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(inV1.mValue),
                                        _mm_castsi128_ps(inV2.mValue),
                                        _mm_castsi128_ps(inControl.mValue)));
#elif AMARYL_SIMD_NEON
  return vbslq_u32(vshrq_n_s32(inControl.mValue, 31), inV2.mValue, inV1.mValue);
#else
  UVec4 result;
  for (int i = 0; i < 4; i++)
    result.mU32[i] = inControl.mU32[i] ? inV2.mU32[i] : inV1.mU32[i];
  return result;
#endif
}

UVec4 UVec4::sOr(UVec4Arg inV1, UVec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_or_si128(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vorrq_u32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mU32[0] | inV2.mU32[0], inV1.mU32[1] | inV2.mU32[1],
               inV1.mU32[2] | inV2.mU32[2], inV1.mU32[3] | inV2.mU32[3]);
#endif
}

UVec4 UVec4::sXor(UVec4Arg inV1, UVec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_xor_si128(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return veorq_u32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mU32[0] ^ inV2.mU32[0], inV1.mU32[1] ^ inV2.mU32[1],
               inV1.mU32[2] ^ inV2.mU32[2], inV1.mU32[3] ^ inV2.mU32[3]);
#endif
}

UVec4 UVec4::sAnd(UVec4Arg inV1, UVec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_and_si128(inV1.mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vandq_u32(inV1.mValue, inV2.mValue);
#else
  return UVec4(inV1.mU32[0] & inV2.mU32[0], inV1.mU32[1] & inV2.mU32[1],
               inV1.mU32[2] & inV2.mU32[2], inV1.mU32[3] & inV2.mU32[3]);
#endif
}

UVec4 UVec4::sNot(UVec4Arg inV1) {
#if AMARYL_SIMD_AVX2
  return _mm_ternarylogic_epi32(inV1.mValue, inV1.mValue, inV1.mValue, 0b01010101);
#elif AMARYL_USE_SSE
  return sXor(inV1, sReplicate(0xffffffff));
#elif AMARYL_SIMD_NEON
  return vmvnq_u32(inV1.mValue);
#else
  return UVec4(~inV1.mU32[0], ~inV1.mU32[1], ~inV1.mU32[2], ~inV1.mU32[3]);
#endif
}

UVec4 UVec4::sSort4True(UVec4Arg inValue, UVec4Arg inIndex) {
  // If inValue.z is false then shift W to Z
  UVec4 v = UVec4::sSelect(inIndex.Swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>(),
                           inIndex, inValue.SplatZ());

  // If inValue.y is false then shift Z and further to Y and further
  v = UVec4::sSelect(v.Swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W>(), v,
                     inValue.SplatY());

  // If inValue.x is false then shift X and furhter to Y and furhter
  v = UVec4::sSelect(v.Swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W>(), v,
                     inValue.SplatX());

  return v;
}

UVec4 UVec4::operator*(UVec4Arg inV2) const {
#if AMARYL_USE_SSE4_1
  return _mm_mullo_epi32(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vmulq_u32(mValue, inV2.mValue);
#else
  UVec4 result;
  for (int i = 0; i < 4; i++) result.mU32[i] = mU32[i] * inV2.mU32[i];
  return result;
#endif
}

UVec4 UVec4::operator+(UVec4Arg inV2) {
#if AMARYL_USE_SSE
  return _mm_add_epi32(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  return vaddq_u32(mValue, inV2.mValue);
#else
  return UVec4(mU32[0] + inV2.mU32[0], mU32[1] + inV2.mU32[1], mU32[2] + inV2.mU32[2],
               mU32[3] + inV2.mU32[3]);
#endif
}

UVec4& UVec4::operator+=(UVec4Arg inV2) {
#if AMARYL_USE_SSE
  mValue = _mm_add_epi32(mValue, inV2.mValue);
#elif AMARYL_SIMD_NEON
  mValue = vaddq_u32(mValue, inV2.mValue);
#else
  for (int i = 0; i < 4; ++i) mU32[i] += inV2.mU32[i];
#endif
  return *this;
}

UVec4 UVec4::SplatX() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(0, 0, 0, 0));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_u32(mValue, 0);
#else
  return UVec4(mU32[0], mU32[0], mU32[0], mU32[0]);
#endif
}

UVec4 UVec4::SplatY() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(1, 1, 1, 1));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_u32(mValue, 1);
#else
  return UVec4(mU32[1], mU32[1], mU32[1], mU32[1]);
#endif
}

UVec4 UVec4::SplatZ() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(2, 2, 2, 2));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_u32(mValue, 2);
#else
  return UVec4(mU32[2], mU32[2], mU32[2], mU32[2]);
#endif
}

UVec4 UVec4::SplatW() const {
#if AMARYL_USE_SSE
  return _mm_shuffle_epi32(mValue, _MM_SHUFFLE(3, 3, 3, 3));
#elif AMARYL_SIMD_NEON
  return vdupq_laneq_u32(mValue, 3);
#else
  return UVec4(mU32[3], mU32[3], mU32[3], mU32[3]);
#endif
}

Vec4 UVec4::ToFloat() const {
#if AMARYL_USE_SSE
  return _mm_cvtepi32_ps(mValue);
#elif AMARYL_SIMD_NEON
  return vcvtq_f32_s32(mValue);
#else
  return Vec4((float)mU32[0], (float)mU32[1], (float)mU32[2], (float)mU32[3]);
#endif
}

Vec4 UVec4::ReinterpretAsFloat() const {
#if AMARYL_USE_SSE
  return Vec4(_mm_castsi128_ps(mValue));
#elif AMARYL_SIMD_NEON
  return vreinterpretq_f32_s32(mValue);
#else
  return *reinterpret_cast<const Vec4*>(this);
#endif
}

void UVec4::StoreInt4(uint32* outV) const {
#if AMARYL_USE_SSE
  _mm_storeu_si128(reinterpret_cast<__m128i*>(outV), mValue);
#elif AMARYL_SIMD_NEON
  vst1q_u32(outV, mValue);
#else
  for (int i = 0; i < 4; ++i) outV[i] = mU32[i];
#endif
}

void UVec4::StoreInt4Aligned(uint32* outV) const {
#if AMARYL_USE_SSE
  _mm_store_si128(reinterpret_cast<__m128i*>(outV), mValue);
#elif AMARYL_SIMD_NEON
  vst1q_u32(outV,
            mValue);  // ARM doesn't make distinction between aligned or not
#else
  for (int i = 0; i < 4; ++i) outV[i] = mU32[i];
#endif
}

int UVec4::CountTrues() const {
#if AMARYL_USE_SSE
  return _mm_popcnt_u32(_mm_movemask_ps(_mm_castsi128_ps(mValue)));
#elif AMARYL_SIMD_NEON
  return vaddvq_u32(vshrq_n_u32(mValue, 31));
#else
  return (mU32[0] >> 31) + (mU32[1] >> 31) + (mU32[2] >> 31) + (mU32[3] >> 31);
#endif
}

int UVec4::GetTrues() const {
#if AMARYL_USE_SSE
  return _mm_movemask_ps(_mm_castsi128_ps(mValue));
#elif AMARYL_SIMD_NEON
  int32x4_t shift = ARM_NEON_INT32x4(0, 1, 2, 3);
  return vaddvq_u32(vshlq_u32(vshrq_n_u32(mValue, 31), shift));
#else
  return (mU32[0] >> 31) | ((mU32[1] >> 31) << 1) | ((mU32[2] >> 31) << 2) |
         ((mU32[3] >> 31) << 3);
#endif
}

bool UVec4::TestAnyTrue() const { return GetTrues() != 0; }

bool UVec4::TestAnyXYZTrue() const { return (GetTrues() & 0b111) != 0; }

bool UVec4::TestAllTrue() const { return GetTrues() == 0b1111; }

bool UVec4::TestAllXYZTrue() const { return (GetTrues() & 0b111) == 0b111; }

template <const uint32 Count>
UVec4 UVec4::LogicalShiftLeft() const {
  static_assert(Count <= 31, "Invalid shift");

#if AMARYL_USE_SSE
  return _mm_slli_epi32(mValue, Count);
#elif AMARYL_SIMD_NEON
  return vshlq_n_u32(mValue, Count);
#else
  return UVec4(mU32[0] << Count, mU32[1] << Count, mU32[2] << Count, mU32[3] << Count);
#endif
}

template <const uint32 Count>
UVec4 UVec4::LogicalShiftRight() const {
  static_assert(Count <= 31, "Invalid shift");

#if AMARYL_USE_SSE
  return _mm_srli_epi32(mValue, Count);
#elif AMARYL_SIMD_NEON
  return vshrq_n_u32(mValue, Count);
#else
  return UVec4(mU32[0] >> Count, mU32[1] >> Count, mU32[2] >> Count, mU32[3] >> Count);
#endif
}

template <const uint32 Count>
UVec4 UVec4::ArithmeticShiftRight() const {
  static_assert(Count <= 31, "Invalid shift");

#if AMARYL_USE_SSE
  return _mm_srai_epi32(mValue, Count);
#elif AMARYL_SIMD_NEON
  return vshrq_n_s32(mValue, Count);
#else
  return UVec4(uint32(int32_t(mU32[0]) >> Count), uint32(int32_t(mU32[1]) >> Count),
               uint32(int32_t(mU32[2]) >> Count), uint32(int32_t(mU32[3]) >> Count));
#endif
}

UVec4 UVec4::Expand4Uint16Lo() const {
#if AMARYL_USE_SSE
  return _mm_unpacklo_epi16(mValue, _mm_castps_si128(_mm_setzero_ps()));
#elif AMARYL_SIMD_NEON
  int16x4_t value = vget_low_s16(mValue);
  int16x4_t zero = vdup_n_s16(0);
  return vcombine_s16(vzip1_s16(value, zero), vzip2_s16(value, zero));
#else
  return UVec4(mU32[0] & 0xffff, (mU32[0] >> 16) & 0xffff, mU32[1] & 0xffff,
               (mU32[1] >> 16) & 0xffff);
#endif
}

UVec4 UVec4::Expand4Uint16Hi() const {
#if AMARYL_USE_SSE
  return _mm_unpackhi_epi16(mValue, _mm_castps_si128(_mm_setzero_ps()));
#elif AMARYL_SIMD_NEON
  int16x4_t value = vget_high_s16(mValue);
  int16x4_t zero = vdup_n_s16(0);
  return vcombine_s16(vzip1_s16(value, zero), vzip2_s16(value, zero));
#else
  return UVec4(mU32[2] & 0xffff, (mU32[2] >> 16) & 0xffff, mU32[3] & 0xffff,
               (mU32[3] >> 16) & 0xffff);
#endif
}

UVec4 UVec4::Expand4Byte0() const {
#if AMARYL_USE_SSE4_1
  return _mm_shuffle_epi8(mValue, _mm_set_epi32(int(0xffffff03), int(0xffffff02),
                                                int(0xffffff01), int(0xffffff00)));
#elif AMARYL_SIMD_NEON
  int8x16_t idx = ARM_NEON_INT8x16(0x00, 0x7f, 0x7f, 0x7f, 0x01, 0x7f, 0x7f, 0x7f, 0x02,
                                   0x7f, 0x7f, 0x7f, 0x03, 0x7f, 0x7f, 0x7f);
  return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
  UVec4 result;
  for (int i = 0; i < 4; i++) result.mU32[i] = (mU32[0] >> (i * 8)) & 0xff;
  return result;
#endif
}

UVec4 UVec4::Expand4Byte4() const {
#if AMARYL_USE_SSE4_1
  return _mm_shuffle_epi8(mValue, _mm_set_epi32(int(0xffffff07), int(0xffffff06),
                                                int(0xffffff05), int(0xffffff04)));
#elif AMARYL_SIMD_NEON
  int8x16_t idx = ARM_NEON_INT8x16(0x04, 0x7f, 0x7f, 0x7f, 0x05, 0x7f, 0x7f, 0x7f, 0x06,
                                   0x7f, 0x7f, 0x7f, 0x07, 0x7f, 0x7f, 0x7f);
  return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
  UVec4 result;
  for (int i = 0; i < 4; i++) result.mU32[i] = (mU32[1] >> (i * 8)) & 0xff;
  return result;
#endif
}

UVec4 UVec4::Expand4Byte8() const {
#if AMARYL_USE_SSE4_1
  return _mm_shuffle_epi8(mValue, _mm_set_epi32(int(0xffffff0b), int(0xffffff0a),
                                                int(0xffffff09), int(0xffffff08)));
#elif AMARYL_SIMD_NEON
  int8x16_t idx = ARM_NEON_INT8x16(0x08, 0x7f, 0x7f, 0x7f, 0x09, 0x7f, 0x7f, 0x7f, 0x0a,
                                   0x7f, 0x7f, 0x7f, 0x0b, 0x7f, 0x7f, 0x7f);
  return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
  UVec4 result;
  for (int i = 0; i < 4; i++) result.mU32[i] = (mU32[2] >> (i * 8)) & 0xff;
  return result;
#endif
}

UVec4 UVec4::Expand4Byte12() const {
#if AMARYL_USE_SSE4_1
  return _mm_shuffle_epi8(mValue, _mm_set_epi32(int(0xffffff0f), int(0xffffff0e),
                                                int(0xffffff0d), int(0xffffff0c)));
#elif AMARYL_SIMD_NEON
  int8x16_t idx = ARM_NEON_INT8x16(0x0c, 0x7f, 0x7f, 0x7f, 0x0d, 0x7f, 0x7f, 0x7f, 0x0e,
                                   0x7f, 0x7f, 0x7f, 0x0f, 0x7f, 0x7f, 0x7f);
  return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
  UVec4 result;
  for (int i = 0; i < 4; i++) result.mU32[i] = (mU32[3] >> (i * 8)) & 0xff;
  return result;
#endif
}

UVec4 UVec4::ShiftComponents4Minus(int inCount) const {
#if AMARYL_USE_SSE4_1 || AMARYL_SIMD_NEON
  alignas(UVec4) static constexpr uint32 sFourMinusXShuffle[5][4] = {
      {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff},
      {0x0f0e0d0c, 0xffffffff, 0xffffffff, 0xffffffff},
      {0x0b0a0908, 0x0f0e0d0c, 0xffffffff, 0xffffffff},
      {0x07060504, 0x0b0a0908, 0x0f0e0d0c, 0xffffffff},
      {0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c}};
#endif

#if AMARYL_USE_SSE4_1
  return _mm_shuffle_epi8(
      mValue, *reinterpret_cast<const UVec4::Type*>(sFourMinusXShuffle[inCount]));
#elif AMARYL_SIMD_NEON
  u328x16_t idx = vreinterpretq_u8_u32(
      *reinterpret_cast<const UVec4::Type*>(sFourMinusXShuffle[inCount]));
  return vreinterpretq_u32_s8(vqtbl1q_s8(vreinterpretq_s8_u32(mValue), idx));
#else
  UVec4 result = UVec4::sZero();
  for (int i = 0; i < inCount; i++) result.mU32[i] = mU32[i + 4 - inCount];
  return result;
#endif
}

}  // namespace amaryl

#pragma once

#include "amaryl/core/math/math_types.h"
#include "amaryl/core/math/quat.h"
#include "amaryl/core/math/vec3.h"
#include "amaryl/core/math/vec4.h"

namespace amaryl {

/// Holds a 4x4 matrix of floats, but supports also operations on the 3x3 upper
/// left part of the matrix.
class [[nodiscard]] alignas(VECTOR_ALIGNMENT) Mat44 {
 public:
  AMARYL_MATH_OVERRIDE_NEW_DELETE

  // Underlying column type
  using Type = Vec4::Type;

  // Argument type
  using ArgType = Mat44Arg;

  /// Constructor
  Mat44() = default;  ///< Intentionally not initialized for performance reasons
  FORCE_INLINE Mat44(Vec4Arg inC1, Vec4Arg inC2, Vec4Arg inC3, Vec4Arg inC4);
  FORCE_INLINE Mat44(Vec4Arg inC1, Vec4Arg inC2, Vec4Arg inC3, Vec3Arg inC4);
  Mat44(const Mat44& inM2) = default;
  Mat44& operator=(const Mat44& inM2) = default;
  FORCE_INLINE Mat44(Type inC1, Type inC2, Type inC3, Type inC4);

  /// Zero matrix
  static FORCE_INLINE Mat44 sZero();

  /// Identity matrix
  static FORCE_INLINE Mat44 sIdentity();

  /// Matrix filled with NaN's
  static FORCE_INLINE Mat44 sNaN();

  /// Load 16 floats from memory
  static FORCE_INLINE Mat44 sLoadFloat4x4(const float4* inV);

  /// Load 16 floats from row-major array
  static FORCE_INLINE Mat44 sLoadFloatsByRow(const float inV[16]);

  /// Load 16 floats from column-major array
  static FORCE_INLINE Mat44 sLoadFloatsByColumn(const float inV[16]);

  /// Load 16 floats from memory, 16 bytes aligned
  static FORCE_INLINE Mat44 sLoadFloat4x4Aligned(const float4* inV);

  /// Rotate around X, Y or Z axis (angle in radians)
  static FORCE_INLINE Mat44 sRotationX(float inX);
  static FORCE_INLINE Mat44 sRotationY(float inY);
  static FORCE_INLINE Mat44 sRotationZ(float inZ);

  /// Rotate around arbitrary axis
  static FORCE_INLINE Mat44 sRotation(Vec3Arg inAxis, float inAngle);

  /// Rotate from quaternion
  static FORCE_INLINE Mat44 sRotation(QuatArg inQuat);

  /// Get matrix that translates
  static FORCE_INLINE Mat44 sTranslation(Vec3Arg inV);

  /// Get matrix that rotates and translates
  static FORCE_INLINE Mat44 sRotationTranslation(QuatArg inR, Vec3Arg inT);

  /// Get inverse matrix of sRotation
  static FORCE_INLINE Mat44 sInverseRotation(QuatArg inR);

  /// Get inverse matrix of sRotationTranslation
  static FORCE_INLINE Mat44 sInverseRotationTranslation(QuatArg inR, Vec3Arg inT);

  /// Get matrix that scales uniformly
  static FORCE_INLINE Mat44 sScale(float inScale);

  /// Get matrix that scales (produces a matrix with (inV, 1) on its diagonal)
  static FORCE_INLINE Mat44 sScale(Vec3Arg inV);

  /// Get matrix that scales, rotates and translates
  static FORCE_INLINE Mat44 sTRS(Vec3Arg inT, QuatArg inQ, Vec3Arg inS);

  /// Get outer product of inV and inV2 (equivalent to \f$inV1 \otimes inV2\f$)
  static FORCE_INLINE Mat44 sOuterProduct(Vec3Arg inV1, Vec3Arg inV2);

  /// Get matrix that represents a cross product \f$A \times B =
  /// \text{sCrossProduct}(A) \: B\f$
  static FORCE_INLINE Mat44 sCrossProduct(Vec3Arg inV);

  /// Returns matrix ML so that \f$ML(q) \: p = q \: p\f$ (where p and q are
  /// quaternions)
  static FORCE_INLINE Mat44 sQuatLeftMultiply(QuatArg inQ);

  /// Returns matrix MR so that \f$MR(q) \: p = p \: q\f$ (where p and q are
  /// quaternions)
  static FORCE_INLINE Mat44 sQuatRightMultiply(QuatArg inQ);

  /// Returns a look at matrix that transforms from world space to view space
  /// @param inPos Position of the camera
  /// @param inTarget Target of the camera
  /// @param inUp Up vector
  static FORCE_INLINE Mat44 sLookAt(Vec3Arg inPos, Vec3Arg inTarget, Vec3Arg inUp);

  /// Returns a right-handed perspective projection matrix
  static FORCE_INLINE Mat44 sPerspective(float inFovY, float inAspect, float inNear,
                                         float inFar);

  /// Get float component by element index
  FORCE_INLINE float operator()(uint32 inRow, uint32 inColumn) const {
    ASSERT(inRow < 4);
    ASSERT(inColumn < 4);
    return mCol[inColumn].mF32[inRow];
  }
  FORCE_INLINE float& operator()(uint32 inRow, uint32 inColumn) {
    ASSERT(inRow < 4);
    ASSERT(inColumn < 4);
    return mCol[inColumn].mF32[inRow];
  }

  /// Comparsion
  FORCE_INLINE bool operator==(Mat44Arg inM2) const;
  FORCE_INLINE bool operator!=(Mat44Arg inM2) const { return !(*this == inM2); }

  /// Test if two matrices are close
  FORCE_INLINE bool IsClose(Mat44Arg inM2, float inMaxDistSq = 1.0e-12f) const;

  /// Multiply matrix by matrix
  FORCE_INLINE Mat44 operator*(Mat44Arg inM) const;

  /// Multiply vector by matrix
  FORCE_INLINE Vec3 operator*(Vec3Arg inV) const;
  FORCE_INLINE Vec4 operator*(Vec4Arg inV) const;

  /// Multiply vector by only 3x3 part of the matrix
  FORCE_INLINE Vec3 Multiply3x3(Vec3Arg inV) const;

  /// Multiply vector by only 3x3 part of the transpose of the matrix (\f$result
  /// = this^T \: inV\f$)
  FORCE_INLINE Vec3 Multiply3x3Transposed(Vec3Arg inV) const;

  /// Multiply 3x3 matrix by 3x3 matrix
  FORCE_INLINE Mat44 Multiply3x3(Mat44Arg inM) const;

  /// Multiply transpose of 3x3 matrix by 3x3 matrix (\f$result = this^T \:
  /// inM\f$)
  FORCE_INLINE Mat44 Multiply3x3LeftTransposed(Mat44Arg inM) const;

  /// Multiply 3x3 matrix by the transpose of a 3x3 matrix (\f$result = this \:
  /// inM^T\f$)
  FORCE_INLINE Mat44 Multiply3x3RightTransposed(Mat44Arg inM) const;

  /// Multiply matrix with float
  FORCE_INLINE Mat44 operator*(float inV) const;
  friend FORCE_INLINE Mat44 operator*(float inV, Mat44Arg inM) { return inM * inV; }

  /// Multiply matrix with float
  FORCE_INLINE Mat44& operator*=(float inV);

  /// Per element addition of matrix
  FORCE_INLINE Mat44 operator+(Mat44Arg inM) const;

  /// Negate
  FORCE_INLINE Mat44 operator-() const;

  /// Per element subtraction of matrix
  FORCE_INLINE Mat44 operator-(Mat44Arg inM) const;

  /// Per element addition of matrix
  FORCE_INLINE Mat44& operator+=(Mat44Arg inM);

  /// Access to the columns
  FORCE_INLINE Vec3 GetAxisX() const { return Vec3(mCol[0]); }
  FORCE_INLINE void SetAxisX(Vec3Arg inV) { mCol[0] = Vec4(inV, 0.0f); }
  FORCE_INLINE Vec3 GetAxisY() const { return Vec3(mCol[1]); }
  FORCE_INLINE void SetAxisY(Vec3Arg inV) { mCol[1] = Vec4(inV, 0.0f); }
  FORCE_INLINE Vec3 GetAxisZ() const { return Vec3(mCol[2]); }
  FORCE_INLINE void SetAxisZ(Vec3Arg inV) { mCol[2] = Vec4(inV, 0.0f); }
  FORCE_INLINE Vec3 GetTranslation() const { return Vec3(mCol[3]); }
  FORCE_INLINE void SetTranslation(Vec3Arg inV) { mCol[3] = Vec4(inV, 1.0f); }
  FORCE_INLINE Vec3 GetDiagonal3() const {
    return Vec3(mCol[0][0], mCol[1][1], mCol[2][2]);
  }
  FORCE_INLINE void SetDiagonal3(Vec3Arg inV) {
    mCol[0][0] = inV.GetX();
    mCol[1][1] = inV.GetY();
    mCol[2][2] = inV.GetZ();
  }
  FORCE_INLINE Vec4 GetDiagonal4() const {
    return Vec4(mCol[0][0], mCol[1][1], mCol[2][2], mCol[3][3]);
  }
  FORCE_INLINE void SetDiagonal4(Vec4Arg inV) {
    mCol[0][0] = inV.GetX();
    mCol[1][1] = inV.GetY();
    mCol[2][2] = inV.GetZ();
    mCol[3][3] = inV.GetW();
  }
  FORCE_INLINE Vec3 GetColumn3(uint32 inCol) const {
    ASSERT(inCol < 4);
    return Vec3(mCol[inCol]);
  }
  FORCE_INLINE void SetColumn3(uint32 inCol, Vec3Arg inV) {
    ASSERT(inCol < 4);
    mCol[inCol] = Vec4(inV, inCol == 3 ? 1.0f : 0.0f);
  }
  FORCE_INLINE Vec4 GetColumn4(uint32 inCol) const {
    ASSERT(inCol < 4);
    return mCol[inCol];
  }
  FORCE_INLINE void SetColumn4(uint32 inCol, Vec4Arg inV) {
    ASSERT(inCol < 4);
    mCol[inCol] = inV;
  }

  /// Store matrix to memory
  FORCE_INLINE void StoreFloat4x4(float4* outV) const;

  /// Transpose matrix
  FORCE_INLINE Mat44 Transposed() const;

  /// Transpose 3x3 subpart of matrix
  FORCE_INLINE Mat44 Transposed3x3() const;

  /// Inverse 4x4 matrix
  Mat44 Inversed() const;

  /// Inverse 4x4 matrix when it only contains rotation and translation
  Mat44 InversedRotationTranslation() const;

  /// Get the determinant of a 3x3 matrix
  float GetDeterminant3x3() const;

  /// Get the adjoint of a 3x3 matrix
  Mat44 Adjointed3x3() const;

  /// Inverse 3x3 matrix
  Mat44 Inversed3x3() const;

  /// *this = inM.Inversed3x3(), returns false if the matrix is singular in
  /// which case *this is unchanged
  bool SetInversed3x3(Mat44Arg inM);

  /// Get rotation part only (note: retains the first 3 values from the bottom
  /// row)
  FORCE_INLINE Mat44 GetRotation() const;

  /// Get rotation part only (note: also clears the bottom row)
  FORCE_INLINE Mat44 GetRotationSafe() const;

  /// Updates the rotation part of this matrix (the first 3 columns)
  FORCE_INLINE void SetRotation(Mat44Arg inRotation);

  /// Convert to quaternion
  Quat GetQuaternion() const;

  /// Get matrix that transforms a direction with the same transform as this
  /// matrix (length is not preserved)
  FORCE_INLINE Mat44 GetDirectionPreservingMatrix() const {
    return GetRotation().Inversed3x3().Transposed3x3();
  }

  /// Pre multiply by translation matrix: result = this *
  /// Mat44::sTranslation(inTranslation)
  FORCE_INLINE Mat44 PreTranslated(Vec3Arg inTranslation) const;

  /// Post multiply by translation matrix: result =
  /// Mat44::sTranslation(inTranslation) * this (i.e. add inTranslation to the
  /// 4-th column)
  FORCE_INLINE Mat44 PostTranslated(Vec3Arg inTranslation) const;

  /// Scale a matrix: result = this * Mat44::sScale(inScale)
  FORCE_INLINE Mat44 PreScaled(Vec3Arg inScale) const;

  /// Scale a matrix: result = Mat44::sScale(inScale) * this
  FORCE_INLINE Mat44 PostScaled(Vec3Arg inScale) const;

  /// Decompose a matrix into a rotation & translation part and into a scale
  /// part so that: this = return_value * Mat44::sScale(outScale). This equation
  /// only holds when the matrix is orthogonal, if it is not the returned matrix
  /// will be made orthogonal using the modified Gram-Schmidt algorithm (see:
  /// https://en.wikipedia.org/wiki/Gram%E2%80%93Schmidt_process)
  Mat44 Decompose(Vec3& outScale) const;

  FORCE_INLINE void SetIdentity() { *this = Mat44::sIdentity(); }

 private:
  Vec4 mCol[4];  ///< Column
};

static_assert(std::is_trivial<Mat44>(), "Is supposed to be a trivial type!");

}  // namespace amaryl

namespace amaryl {

Mat44::Mat44(Vec4Arg inC1, Vec4Arg inC2, Vec4Arg inC3, Vec4Arg inC4)
    : mCol{inC1, inC2, inC3, inC4} {}

Mat44::Mat44(Vec4Arg inC1, Vec4Arg inC2, Vec4Arg inC3, Vec3Arg inC4)
    : mCol{inC1, inC2, inC3, Vec4(inC4, 1.0f)} {}

Mat44::Mat44(Type inC1, Type inC2, Type inC3, Type inC4) : mCol{inC1, inC2, inC3, inC4} {}

Mat44 Mat44::sZero() {
  return Mat44(Vec4::sZero(), Vec4::sZero(), Vec4::sZero(), Vec4::sZero());
}

Mat44 Mat44::sIdentity() {
  return Mat44(Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, 0, 1, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sNaN() {
  return Mat44(Vec4::sNaN(), Vec4::sNaN(), Vec4::sNaN(), Vec4::sNaN());
}

Mat44 Mat44::sLoadFloat4x4(const float4* inV) {
  Mat44 result;
  for (int c = 0; c < 4; ++c) result.mCol[c] = Vec4::sLoadFloat4(inV + c);
  return result;
}

Mat44 Mat44::sLoadFloatsByRow(const float inV[16]) {
  return Mat44(
      Vec4(inV[0], inV[4], inV[8], inV[12]), Vec4(inV[1], inV[5], inV[9], inV[13]),
      Vec4(inV[2], inV[6], inV[10], inV[14]), Vec4(inV[3], inV[7], inV[11], inV[15]));
}

Mat44 Mat44::sLoadFloatsByColumn(const float inV[16]) {
  return Mat44(Vec4(inV[0], inV[1], inV[2], inV[3]), Vec4(inV[4], inV[5], inV[6], inV[7]),
               Vec4(inV[8], inV[9], inV[10], inV[11]),
               Vec4(inV[12], inV[13], inV[14], inV[15]));
}

Mat44 Mat44::sLoadFloat4x4Aligned(const float4* inV) {
  Mat44 result;
  for (int c = 0; c < 4; ++c) result.mCol[c] = Vec4::sLoadFloat4Aligned(inV + c);
  return result;
}

Mat44 Mat44::sRotationX(float inX) {
  Vec4 sv, cv;
  Vec4::sReplicate(inX).SinCos(sv, cv);
  float s = sv.GetX(), c = cv.GetX();
  return Mat44(Vec4(1, 0, 0, 0), Vec4(0, c, s, 0), Vec4(0, -s, c, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sRotationY(float inY) {
  Vec4 sv, cv;
  Vec4::sReplicate(inY).SinCos(sv, cv);
  float s = sv.GetX(), c = cv.GetX();
  return Mat44(Vec4(c, 0, -s, 0), Vec4(0, 1, 0, 0), Vec4(s, 0, c, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sRotationZ(float inZ) {
  Vec4 sv, cv;
  Vec4::sReplicate(inZ).SinCos(sv, cv);
  float s = sv.GetX(), c = cv.GetX();
  return Mat44(Vec4(c, s, 0, 0), Vec4(-s, c, 0, 0), Vec4(0, 0, 1, 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sRotation(QuatArg inQuat) {
  ASSERT(inQuat.IsNormalized());

  // See: https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation section
  // 'Quaternion-derived rotation matrix'
#if AMARYL_USE_SSE4_1
  __m128 xyzw = inQuat.mValue.mValue;
  __m128 two_xyzw = _mm_add_ps(xyzw, xyzw);
  __m128 yzxw = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(3, 0, 2, 1));
  __m128 two_yzxw = _mm_add_ps(yzxw, yzxw);
  __m128 zxyw = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(3, 1, 0, 2));
  __m128 two_zxyw = _mm_add_ps(zxyw, zxyw);
  __m128 wwww = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(3, 3, 3, 3));
  // (1 - 2 y^2 - 2 z^2, 1 - 2 x^2 - 2 z^2, 1 - 2 x^2 - 2 y^2, 1 - 4 w^2)
  __m128 diagonal = _mm_sub_ps(_mm_sub_ps(_mm_set1_ps(1.0f), _mm_mul_ps(two_yzxw, yzxw)),
                               _mm_mul_ps(two_zxyw, zxyw));
  // 2 * (xz + yw, xy + zw, yz + xw, ww)
  __m128 plus = _mm_add_ps(_mm_mul_ps(two_xyzw, zxyw), _mm_mul_ps(two_yzxw, wwww));
  // 2 * (xy - zw, yz - xw, xz - yw, 0)
  __m128 minus = _mm_sub_ps(_mm_mul_ps(two_yzxw, xyzw), _mm_mul_ps(two_zxyw, wwww));

  // Workaround for compiler changing _mm_sub_ps(_mm_mul_ps(...), ...) into a
  // fused multiply sub instruction, resulting in w not being 0 There doesn't
  // appear to be a reliable way to turn this off in Clang
  minus = _mm_insert_ps(minus, minus, 0b1000);

  // (1 - 2 y^2 - 2 z^2, 2 xy + 2 zw, 2 xz - 2 yw, 0)
  __m128 col0 = _mm_blend_ps(_mm_blend_ps(plus, diagonal, 0b0001), minus, 0b1100);
  // (2 xy - 2 zw, 1 - 2 x^2 - 2 z^2, 2 yz + 2 xw, 0)
  __m128 col1 = _mm_blend_ps(_mm_blend_ps(diagonal, minus, 0b1001), plus, 0b0100);
  // (2 xz + 2 yw, 2 yz - 2 xw, 1 - 2 x^2 - 2 y^2, 0)
  __m128 col2 = _mm_blend_ps(_mm_blend_ps(minus, plus, 0b0001), diagonal, 0b0100);
  __m128 col3 = _mm_set_ps(1, 0, 0, 0);

  return Mat44(col0, col1, col2, col3);
#else
  float x = inQuat.GetX();
  float y = inQuat.GetY();
  float z = inQuat.GetZ();
  float w = inQuat.GetW();

  // Note: Using x + x instead of 2.0f * x to force this function to return the
  // same value as the SSE4.1 version across platforms.
  float tx = x + x;
  float ty = y + y;
  float tz = z + z;

  float xx = tx * x;
  float yy = ty * y;
  float zz = tz * z;
  float xy = tx * y;
  float xz = tx * z;
  float xw = tx * w;
  float yz = ty * z;
  float yw = ty * w;
  float zw = tz * w;

  // clang-format off
  return Mat44(
      // Note: Added extra brackets to force this function to return the same
      // value as the SSE4.1 version across platforms.
      Vec4((1.0f - yy) - zz, xy + zw,          xz - yw,          0.0f),
      Vec4(xy - zw,          (1.0f - zz) - xx, yz + xw,          0.0f),
      Vec4(xz + yw,          yz - xw,          (1.0f - xx) - yy, 0.0f),
      Vec4(0.0f,             0.0f,             0.0f,             1.0f));
  // clang-format on
#endif
}

Mat44 Mat44::sRotation(Vec3Arg inAxis, float inAngle) {
  return sRotation(Quat::sRotation(inAxis, inAngle));
}

Mat44 Mat44::sTranslation(Vec3Arg inV) {
  return Mat44(Vec4(1, 0, 0, 0), Vec4(0, 1, 0, 0), Vec4(0, 0, 1, 0), Vec4(inV, 1));
}

Mat44 Mat44::sRotationTranslation(QuatArg inR, Vec3Arg inT) {
  Mat44 m = sRotation(inR);
  m.SetTranslation(inT);
  return m;
}

Mat44 Mat44::sInverseRotation(QuatArg inR) { return sRotation(inR.Conjugated()); }

Mat44 Mat44::sInverseRotationTranslation(QuatArg inR, Vec3Arg inT) {
  Mat44 m = sRotation(inR.Conjugated());
  m.SetTranslation(-m.Multiply3x3(inT));
  return m;
}

Mat44 Mat44::sScale(float inScale) {
  return Mat44(Vec4(inScale, 0, 0, 0), Vec4(0, inScale, 0, 0), Vec4(0, 0, inScale, 0),
               Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sScale(Vec3Arg inV) {
  return Mat44(Vec4(inV.GetX(), 0, 0, 0), Vec4(0, inV.GetY(), 0, 0),
               Vec4(0, 0, inV.GetZ(), 0), Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sTRS(Vec3Arg inT, QuatArg inQ, Vec3Arg inS) {
  Mat44 s(Vec4(inS.GetX(), 0, 0, 0), Vec4(0, inS.GetY(), 0, 0), Vec4(0, 0, inS.GetZ(), 0),
          Vec4(0, 0, 0, 1));
  inQ = inQ.Normalized();
  Mat44 r = sRotation(inQ.Conjugated());
  r.SetTranslation(inT);
  return r * s;
}

Mat44 Mat44::sOuterProduct(Vec3Arg inV1, Vec3Arg inV2) {
  Vec4 v1(inV1, 0);
  return Mat44(v1 * inV2.SplatX(), v1 * inV2.SplatY(), v1 * inV2.SplatZ(),
               Vec4(0, 0, 0, 1));
}

Mat44 Mat44::sCrossProduct(Vec3Arg inV) {
#ifdef AMARYL_USE_SSE4_1
  // Zero out the W component
  __m128 zero = _mm_setzero_ps();
  __m128 v = _mm_blend_ps(inV.mValue, zero, 0b1000);

  // Negate
  __m128 min_v = _mm_sub_ps(zero, v);

  return Mat44(_mm_shuffle_ps(v, min_v, _MM_SHUFFLE(3, 1, 2, 3)),  // [0, z, -y, 0]
               _mm_shuffle_ps(min_v, v, _MM_SHUFFLE(3, 0, 3, 2)),  // [-z, 0, x, 0]
               _mm_blend_ps(_mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 3, 1)),
                            _mm_shuffle_ps(min_v, min_v, _MM_SHUFFLE(3, 3, 0, 3)),
                            0b0010),  // [y, -x, 0, 0]
               Vec4(0, 0, 0, 1));
#else
  float x = inV.GetX();
  float y = inV.GetY();
  float z = inV.GetZ();

  return Mat44(Vec4(0, z, -y, 0), Vec4(-z, 0, x, 0), Vec4(y, -x, 0, 0), Vec4(0, 0, 0, 1));
#endif
}

Mat44 Mat44::sLookAt(Vec3Arg inPos, Vec3Arg inTarget, Vec3Arg inUp) {
  Vec3 direction = (inTarget - inPos).NormalizedOr(-Vec3::sAxisZ());
  Vec3 right = direction.Cross(inUp).NormalizedOr(Vec3::sAxisX());
  Vec3 up = right.Cross(direction);

  return Mat44(Vec4(right, 0), Vec4(up, 0), Vec4(-direction, 0), Vec4(inPos, 1))
      .InversedRotationTranslation();
}

Mat44 Mat44::sPerspective(float inFovY, float inAspect, float inNear, float inFar) {
  float height = 1.0f / Tan(0.5f * inFovY);
  float width = height / inAspect;
  float range = inFar / (inNear - inFar);

  return Mat44(Vec4(width, 0.0f, 0.0f, 0.0f), Vec4(0.0f, height, 0.0f, 0.0f),
               Vec4(0.0f, 0.0f, range, -1.0f), Vec4(0.0f, 0.0f, range * inNear, 0.0f));
}

bool Mat44::operator==(Mat44Arg inM2) const {
  return UVec4::sAnd(UVec4::sAnd(Vec4::sEquals(mCol[0], inM2.mCol[0]),
                                 Vec4::sEquals(mCol[1], inM2.mCol[1])),
                     UVec4::sAnd(Vec4::sEquals(mCol[2], inM2.mCol[2]),
                                 Vec4::sEquals(mCol[3], inM2.mCol[3])))
      .TestAllTrue();
}

bool Mat44::IsClose(Mat44Arg inM2, float inMaxDistSq) const {
  for (int i = 0; i < 4; ++i)
    if (!mCol[i].IsClose(inM2.mCol[i], inMaxDistSq)) return false;
  return true;
}

Mat44 Mat44::operator*(Mat44Arg inM) const {
  Mat44 result;
#if AMARYL_USE_SSE
  for (int i = 0; i < 4; ++i) {
    __m128 c = inM.mCol[i].mValue;
    __m128 t = _mm_mul_ps(mCol[0].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)));
    t = _mm_add_ps(
        t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1))));
    t = _mm_add_ps(
        t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2))));
    t = _mm_add_ps(
        t, _mm_mul_ps(mCol[3].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 3, 3, 3))));
    result.mCol[i].mValue = t;
  }
#elif AMARYL_SIMD_NEON
  for (int i = 0; i < 4; ++i) {
    Type c = inM.mCol[i].mValue;
    Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(c, 0));
    t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(c, 1));
    t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(c, 2));
    t = vmlaq_f32(t, mCol[3].mValue, vdupq_laneq_f32(c, 3));
    result.mCol[i].mValue = t;
  }
#else
  for (int i = 0; i < 4; ++i)
    result.mCol[i] = mCol[0] * inM.mCol[i].mF32[0] + mCol[1] * inM.mCol[i].mF32[1] +
                     mCol[2] * inM.mCol[i].mF32[2] + mCol[3] * inM.mCol[i].mF32[3];
#endif
  return result;
}

Vec3 Mat44::operator*(Vec3Arg inV) const {
#if AMARYL_USE_SSE
  __m128 t = _mm_mul_ps(mCol[0].mValue,
                        _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(0, 0, 0, 0)));
  t = _mm_add_ps(t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue,
                                                              _MM_SHUFFLE(1, 1, 1, 1))));
  t = _mm_add_ps(t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue,
                                                              _MM_SHUFFLE(2, 2, 2, 2))));
  t = _mm_add_ps(t, mCol[3].mValue);
  return Vec3::sFixW(t);
#elif AMARYL_SIMD_NEON
  Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(inV.mValue, 0));
  t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(inV.mValue, 1));
  t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(inV.mValue, 2));
  t = vaddq_f32(t, mCol[3].mValue);  // Don't combine this with the first mul into a fused
                                     // multiply add, causes precision issues
  return Vec3::sFixW(t);
#else
  return Vec3(mCol[0].mF32[0] * inV.mF32[0] + mCol[1].mF32[0] * inV.mF32[1] +
                  mCol[2].mF32[0] * inV.mF32[2] + mCol[3].mF32[0],
              mCol[0].mF32[1] * inV.mF32[0] + mCol[1].mF32[1] * inV.mF32[1] +
                  mCol[2].mF32[1] * inV.mF32[2] + mCol[3].mF32[1],
              mCol[0].mF32[2] * inV.mF32[0] + mCol[1].mF32[2] * inV.mF32[1] +
                  mCol[2].mF32[2] * inV.mF32[2] + mCol[3].mF32[2]);
#endif
}

Vec4 Mat44::operator*(Vec4Arg inV) const {
#if AMARYL_USE_SSE
  __m128 t = _mm_mul_ps(mCol[0].mValue,
                        _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(0, 0, 0, 0)));
  t = _mm_add_ps(t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue,
                                                              _MM_SHUFFLE(1, 1, 1, 1))));
  t = _mm_add_ps(t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue,
                                                              _MM_SHUFFLE(2, 2, 2, 2))));
  t = _mm_add_ps(t, _mm_mul_ps(mCol[3].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue,
                                                              _MM_SHUFFLE(3, 3, 3, 3))));
  return t;
#elif AMARYL_SIMD_NEON
  Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(inV.mValue, 0));
  t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(inV.mValue, 1));
  t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(inV.mValue, 2));
  t = vmlaq_f32(t, mCol[3].mValue, vdupq_laneq_f32(inV.mValue, 3));
  return t;
#else
  return Vec4(mCol[0].mF32[0] * inV.mF32[0] + mCol[1].mF32[0] * inV.mF32[1] +
                  mCol[2].mF32[0] * inV.mF32[2] + mCol[3].mF32[0] * inV.mF32[3],
              mCol[0].mF32[1] * inV.mF32[0] + mCol[1].mF32[1] * inV.mF32[1] +
                  mCol[2].mF32[1] * inV.mF32[2] + mCol[3].mF32[1] * inV.mF32[3],
              mCol[0].mF32[2] * inV.mF32[0] + mCol[1].mF32[2] * inV.mF32[1] +
                  mCol[2].mF32[2] * inV.mF32[2] + mCol[3].mF32[2] * inV.mF32[3],
              mCol[0].mF32[3] * inV.mF32[0] + mCol[1].mF32[3] * inV.mF32[1] +
                  mCol[2].mF32[3] * inV.mF32[2] + mCol[3].mF32[3] * inV.mF32[3]);
#endif
}

Vec3 Mat44::Multiply3x3(Vec3Arg inV) const {
#if AMARYL_USE_SSE
  __m128 t = _mm_mul_ps(mCol[0].mValue,
                        _mm_shuffle_ps(inV.mValue, inV.mValue, _MM_SHUFFLE(0, 0, 0, 0)));
  t = _mm_add_ps(t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue,
                                                              _MM_SHUFFLE(1, 1, 1, 1))));
  t = _mm_add_ps(t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(inV.mValue, inV.mValue,
                                                              _MM_SHUFFLE(2, 2, 2, 2))));
  return Vec3::sFixW(t);
#elif AMARYL_SIMD_NEON
  Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(inV.mValue, 0));
  t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(inV.mValue, 1));
  t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(inV.mValue, 2));
  return Vec3::sFixW(t);
#else
  return Vec3(mCol[0].mF32[0] * inV.mF32[0] + mCol[1].mF32[0] * inV.mF32[1] +
                  mCol[2].mF32[0] * inV.mF32[2],
              mCol[0].mF32[1] * inV.mF32[0] + mCol[1].mF32[1] * inV.mF32[1] +
                  mCol[2].mF32[1] * inV.mF32[2],
              mCol[0].mF32[2] * inV.mF32[0] + mCol[1].mF32[2] * inV.mF32[1] +
                  mCol[2].mF32[2] * inV.mF32[2]);
#endif
}

Vec3 Mat44::Multiply3x3Transposed(Vec3Arg inV) const {
#if AMARYL_USE_SSE4_1
  __m128 x = _mm_dp_ps(mCol[0].mValue, inV.mValue, 0x7f);
  __m128 y = _mm_dp_ps(mCol[1].mValue, inV.mValue, 0x7f);
  __m128 xy = _mm_blend_ps(x, y, 0b0010);
  __m128 z = _mm_dp_ps(mCol[2].mValue, inV.mValue, 0x7f);
  __m128 xyzz = _mm_blend_ps(xy, z, 0b1100);
  return xyzz;
#else
  return Transposed3x3().Multiply3x3(inV);
#endif
}

Mat44 Mat44::Multiply3x3(Mat44Arg inM) const {
  ASSERT(mCol[0][3] == 0.0f);
  ASSERT(mCol[1][3] == 0.0f);
  ASSERT(mCol[2][3] == 0.0f);

  Mat44 result;
#if AMARYL_USE_SSE
  for (int i = 0; i < 3; ++i) {
    __m128 c = inM.mCol[i].mValue;
    __m128 t = _mm_mul_ps(mCol[0].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(0, 0, 0, 0)));
    t = _mm_add_ps(
        t, _mm_mul_ps(mCol[1].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(1, 1, 1, 1))));
    t = _mm_add_ps(
        t, _mm_mul_ps(mCol[2].mValue, _mm_shuffle_ps(c, c, _MM_SHUFFLE(2, 2, 2, 2))));
    result.mCol[i].mValue = t;
  }
#elif AMARYL_SIMD_NEON
  for (int i = 0; i < 3; ++i) {
    Type c = inM.mCol[i].mValue;
    Type t = vmulq_f32(mCol[0].mValue, vdupq_laneq_f32(c, 0));
    t = vmlaq_f32(t, mCol[1].mValue, vdupq_laneq_f32(c, 1));
    t = vmlaq_f32(t, mCol[2].mValue, vdupq_laneq_f32(c, 2));
    result.mCol[i].mValue = t;
  }
#else
  for (int i = 0; i < 3; ++i) {
    result.mCol[i] = mCol[0] * inM.mCol[i].mF32[0] + mCol[1] * inM.mCol[i].mF32[1] +
                     mCol[2] * inM.mCol[i].mF32[2];
  }
#endif
  result.mCol[3] = Vec4(0, 0, 0, 1);
  return result;
}

Mat44 Mat44::Multiply3x3LeftTransposed(Mat44Arg inM) const {
  // Transpose left hand side
  Mat44 trans = Transposed3x3();

  // Do 3x3 matrix multiply
  Mat44 result;
  result.mCol[0] = trans.mCol[0] * inM.mCol[0].SplatX() +
                   trans.mCol[1] * inM.mCol[0].SplatY() +
                   trans.mCol[2] * inM.mCol[0].SplatZ();
  result.mCol[1] = trans.mCol[0] * inM.mCol[1].SplatX() +
                   trans.mCol[1] * inM.mCol[1].SplatY() +
                   trans.mCol[2] * inM.mCol[1].SplatZ();
  result.mCol[2] = trans.mCol[0] * inM.mCol[2].SplatX() +
                   trans.mCol[1] * inM.mCol[2].SplatY() +
                   trans.mCol[2] * inM.mCol[2].SplatZ();
  result.mCol[3] = Vec4(0, 0, 0, 1);
  return result;
}

Mat44 Mat44::Multiply3x3RightTransposed(Mat44Arg inM) const {
  ASSERT(mCol[0][3] == 0.0f);
  ASSERT(mCol[1][3] == 0.0f);
  ASSERT(mCol[2][3] == 0.0f);

  Mat44 result;
  result.mCol[0] = mCol[0] * inM.mCol[0].SplatX() + mCol[1] * inM.mCol[1].SplatX() +
                   mCol[2] * inM.mCol[2].SplatX();
  result.mCol[1] = mCol[0] * inM.mCol[0].SplatY() + mCol[1] * inM.mCol[1].SplatY() +
                   mCol[2] * inM.mCol[2].SplatY();
  result.mCol[2] = mCol[0] * inM.mCol[0].SplatZ() + mCol[1] * inM.mCol[1].SplatZ() +
                   mCol[2] * inM.mCol[2].SplatZ();
  result.mCol[3] = Vec4(0, 0, 0, 1);
  return result;
}

Mat44 Mat44::operator*(float inV) const {
  Vec4 multiplier = Vec4::sReplicate(inV);

  Mat44 result;
  for (int c = 0; c < 4; ++c) result.mCol[c] = mCol[c] * multiplier;
  return result;
}

Mat44& Mat44::operator*=(float inV) {
  for (int c = 0; c < 4; ++c) mCol[c] *= inV;

  return *this;
}

Mat44 Mat44::operator+(Mat44Arg inM) const {
  Mat44 result;
  for (int i = 0; i < 4; ++i) result.mCol[i] = mCol[i] + inM.mCol[i];
  return result;
}

Mat44 Mat44::operator-() const {
  Mat44 result;
  for (int i = 0; i < 4; ++i) result.mCol[i] = -mCol[i];
  return result;
}

Mat44 Mat44::operator-(Mat44Arg inM) const {
  Mat44 result;
  for (int i = 0; i < 4; ++i) result.mCol[i] = mCol[i] - inM.mCol[i];
  return result;
}

Mat44& Mat44::operator+=(Mat44Arg inM) {
  for (int c = 0; c < 4; ++c) mCol[c] += inM.mCol[c];

  return *this;
}

void Mat44::StoreFloat4x4(float4* outV) const {
  for (int c = 0; c < 4; ++c) mCol[c].StoreFloat4(outV + c);
}

Mat44 Mat44::Transposed() const {
#if AMARYL_USE_SSE
  __m128 tmp1 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(1, 0, 1, 0));
  __m128 tmp3 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(3, 2, 3, 2));
  __m128 tmp2 = _mm_shuffle_ps(mCol[2].mValue, mCol[3].mValue, _MM_SHUFFLE(1, 0, 1, 0));
  __m128 tmp4 = _mm_shuffle_ps(mCol[2].mValue, mCol[3].mValue, _MM_SHUFFLE(3, 2, 3, 2));

  Mat44 result;
  result.mCol[0].mValue = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(2, 0, 2, 0));
  result.mCol[1].mValue = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(3, 1, 3, 1));
  result.mCol[2].mValue = _mm_shuffle_ps(tmp3, tmp4, _MM_SHUFFLE(2, 0, 2, 0));
  result.mCol[3].mValue = _mm_shuffle_ps(tmp3, tmp4, _MM_SHUFFLE(3, 1, 3, 1));
  return result;
#elif AMARYL_SIMD_NEON
  float32x4x2_t tmp1 = vzipq_f32(mCol[0].mValue, mCol[2].mValue);
  float32x4x2_t tmp2 = vzipq_f32(mCol[1].mValue, mCol[3].mValue);
  float32x4x2_t tmp3 = vzipq_f32(tmp1.val[0], tmp2.val[0]);
  float32x4x2_t tmp4 = vzipq_f32(tmp1.val[1], tmp2.val[1]);

  Mat44 result;
  result.mCol[0].mValue = tmp3.val[0];
  result.mCol[1].mValue = tmp3.val[1];
  result.mCol[2].mValue = tmp4.val[0];
  result.mCol[3].mValue = tmp4.val[1];
  return result;
#else
  Mat44 result;
  for (int c = 0; c < 4; ++c)
    for (int r = 0; r < 4; ++r) result.mCol[r].mF32[c] = mCol[c].mF32[r];
  return result;
#endif
}

Mat44 Mat44::Transposed3x3() const {
#if AMARYL_USE_SSE
  __m128 zero = _mm_setzero_ps();
  __m128 tmp1 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(1, 0, 1, 0));
  __m128 tmp3 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(3, 2, 3, 2));
  __m128 tmp2 = _mm_shuffle_ps(mCol[2].mValue, zero, _MM_SHUFFLE(1, 0, 1, 0));
  __m128 tmp4 = _mm_shuffle_ps(mCol[2].mValue, zero, _MM_SHUFFLE(3, 2, 3, 2));

  Mat44 result;
  result.mCol[0].mValue = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(2, 0, 2, 0));
  result.mCol[1].mValue = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(3, 1, 3, 1));
  result.mCol[2].mValue = _mm_shuffle_ps(tmp3, tmp4, _MM_SHUFFLE(2, 0, 2, 0));
#elif AMARYL_SIMD_NEON
  float32x4x2_t tmp1 = vzipq_f32(mCol[0].mValue, mCol[2].mValue);
  float32x4x2_t tmp2 = vzipq_f32(mCol[1].mValue, vdupq_n_f32(0));
  float32x4x2_t tmp3 = vzipq_f32(tmp1.val[0], tmp2.val[0]);
  float32x4x2_t tmp4 = vzipq_f32(tmp1.val[1], tmp2.val[1]);

  Mat44 result;
  result.mCol[0].mValue = tmp3.val[0];
  result.mCol[1].mValue = tmp3.val[1];
  result.mCol[2].mValue = tmp4.val[0];
#else
  Mat44 result;
  for (int c = 0; c < 3; ++c) {
    for (int r = 0; r < 3; ++r) {
      result.mCol[c].mF32[r] = mCol[r].mF32[c];
    }
    result.mCol[c].mF32[3] = 0;
  }
#endif
  result.mCol[3] = Vec4(0, 0, 0, 1);
  return result;
}

Mat44 Mat44::sQuatLeftMultiply(QuatArg inQ) {
  return Mat44(Vec4(1, 1, -1, -1) *
                   inQ.mValue.Swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(),
               Vec4(-1, 1, 1, -1) *
                   inQ.mValue.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(),
               Vec4(1, -1, 1, -1) *
                   inQ.mValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(),
               inQ.mValue);
}

Mat44 Mat44::sQuatRightMultiply(QuatArg inQ) {
  return Mat44(Vec4(1, -1, 1, -1) *
                   inQ.mValue.Swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(),
               Vec4(1, 1, -1, -1) *
                   inQ.mValue.Swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(),
               Vec4(-1, 1, 1, -1) *
                   inQ.mValue.Swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(),
               inQ.mValue);
}

Mat44 Mat44::GetRotation() const {
  ASSERT(mCol[0][3] == 0.0f);
  ASSERT(mCol[1][3] == 0.0f);
  ASSERT(mCol[2][3] == 0.0f);

  return Mat44(mCol[0], mCol[1], mCol[2], Vec4(0, 0, 0, 1));
}

Mat44 Mat44::GetRotationSafe() const {
#if AMARYL_SIMD_AVX2
  return Mat44(_mm_maskz_mov_ps(0b0111, mCol[0].mValue),
               _mm_maskz_mov_ps(0b0111, mCol[1].mValue),
               _mm_maskz_mov_ps(0b0111, mCol[2].mValue), Vec4(0, 0, 0, 1));
#elif AMARYL_USE_SSE4_1
  __m128 zero = _mm_setzero_ps();
  return Mat44(_mm_blend_ps(mCol[0].mValue, zero, 8),
               _mm_blend_ps(mCol[1].mValue, zero, 8),
               _mm_blend_ps(mCol[2].mValue, zero, 8), Vec4(0, 0, 0, 1));
#elif AMARYL_SIMD_NEON
  return Mat44(vsetq_lane_f32(0, mCol[0].mValue, 3), vsetq_lane_f32(0, mCol[1].mValue, 3),
               vsetq_lane_f32(0, mCol[2].mValue, 3), Vec4(0, 0, 0, 1));
#else
  return Mat44(Vec4(mCol[0].mF32[0], mCol[0].mF32[1], mCol[0].mF32[2], 0),
               Vec4(mCol[1].mF32[0], mCol[1].mF32[1], mCol[1].mF32[2], 0),
               Vec4(mCol[2].mF32[0], mCol[2].mF32[1], mCol[2].mF32[2], 0),
               Vec4(0, 0, 0, 1));
#endif
}

void Mat44::SetRotation(Mat44Arg inRotation) {
  mCol[0] = inRotation.mCol[0];
  mCol[1] = inRotation.mCol[1];
  mCol[2] = inRotation.mCol[2];
}

Mat44 Mat44::PreTranslated(Vec3Arg inTranslation) const {
  return Mat44(mCol[0], mCol[1], mCol[2],
               Vec4(GetTranslation() + Multiply3x3(inTranslation), 1));
}

Mat44 Mat44::PostTranslated(Vec3Arg inTranslation) const {
  return Mat44(mCol[0], mCol[1], mCol[2], Vec4(GetTranslation() + inTranslation, 1));
}

Mat44 Mat44::PreScaled(Vec3Arg inScale) const {
  return Mat44(inScale.GetX() * mCol[0], inScale.GetY() * mCol[1],
               inScale.GetZ() * mCol[2], mCol[3]);
}

Mat44 Mat44::PostScaled(Vec3Arg inScale) const {
  Vec4 scale(inScale, 1);
  return Mat44(scale * mCol[0], scale * mCol[1], scale * mCol[2], scale * mCol[3]);
}

}  // namespace amaryl

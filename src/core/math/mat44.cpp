#include "amaryl/core/math/mat44.h"

namespace amaryl {

#define MEL(r, c) mCol[c].mF32[r]

Mat44 Mat44::Inversed() const {
#if AMARYL_USE_SSE
  // Algorithm from:
  // http://download.intel.com/design/PentiumIII/sml/24504301.pdf Streaming SIMD
  // Extensions - Inverse of 4x4 Matrix Adapted to load data using
  // _mm_shuffle_ps instead of loading from memory Replaced _mm_rcp_ps with
  // _mm_div_ps for better accuracy

  __m128 tmp1 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(1, 0, 1, 0));
  __m128 row1 = _mm_shuffle_ps(mCol[2].mValue, mCol[3].mValue, _MM_SHUFFLE(1, 0, 1, 0));
  __m128 row0 = _mm_shuffle_ps(tmp1, row1, _MM_SHUFFLE(2, 0, 2, 0));
  row1 = _mm_shuffle_ps(row1, tmp1, _MM_SHUFFLE(3, 1, 3, 1));
  tmp1 = _mm_shuffle_ps(mCol[0].mValue, mCol[1].mValue, _MM_SHUFFLE(3, 2, 3, 2));
  __m128 row3 = _mm_shuffle_ps(mCol[2].mValue, mCol[3].mValue, _MM_SHUFFLE(3, 2, 3, 2));
  __m128 row2 = _mm_shuffle_ps(tmp1, row3, _MM_SHUFFLE(2, 0, 2, 0));
  row3 = _mm_shuffle_ps(row3, tmp1, _MM_SHUFFLE(3, 1, 3, 1));

  tmp1 = _mm_mul_ps(row2, row3);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
  __m128 minor0 = _mm_mul_ps(row1, tmp1);
  __m128 minor1 = _mm_mul_ps(row0, tmp1);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
  minor0 = _mm_sub_ps(_mm_mul_ps(row1, tmp1), minor0);
  minor1 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor1);
  minor1 = _mm_shuffle_ps(minor1, minor1, _MM_SHUFFLE(1, 0, 3, 2));

  tmp1 = _mm_mul_ps(row1, row2);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
  minor0 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor0);
  __m128 minor3 = _mm_mul_ps(row0, tmp1);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
  minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row3, tmp1));
  minor3 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor3);
  minor3 = _mm_shuffle_ps(minor3, minor3, _MM_SHUFFLE(1, 0, 3, 2));

  tmp1 = _mm_mul_ps(_mm_shuffle_ps(row1, row1, _MM_SHUFFLE(1, 0, 3, 2)), row3);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
  row2 = _mm_shuffle_ps(row2, row2, _MM_SHUFFLE(1, 0, 3, 2));
  minor0 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor0);
  __m128 minor2 = _mm_mul_ps(row0, tmp1);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
  minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row2, tmp1));
  minor2 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor2);
  minor2 = _mm_shuffle_ps(minor2, minor2, _MM_SHUFFLE(1, 0, 3, 2));

  tmp1 = _mm_mul_ps(row0, row1);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
  minor2 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor2);
  minor3 = _mm_sub_ps(_mm_mul_ps(row2, tmp1), minor3);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
  minor2 = _mm_sub_ps(_mm_mul_ps(row3, tmp1), minor2);
  minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row2, tmp1));

  tmp1 = _mm_mul_ps(row0, row3);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
  minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row2, tmp1));
  minor2 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor2);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
  minor1 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor1);
  minor2 = _mm_sub_ps(minor2, _mm_mul_ps(row1, tmp1));

  tmp1 = _mm_mul_ps(row0, row2);
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(2, 3, 0, 1));
  minor1 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor1);
  minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row1, tmp1));
  tmp1 = _mm_shuffle_ps(tmp1, tmp1, _MM_SHUFFLE(1, 0, 3, 2));
  minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row3, tmp1));
  minor3 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor3);

  __m128 det = _mm_mul_ps(row0, minor0);
  det = _mm_add_ps(_mm_shuffle_ps(det, det, _MM_SHUFFLE(2, 3, 0, 1)),
                   det);  // Original code did (x + z) + (y + w), changed to (x
                          // + y) + (z + w) to match the ARM code below and make
                          // the result cross platform deterministic
  det = _mm_add_ss(_mm_shuffle_ps(det, det, _MM_SHUFFLE(1, 0, 3, 2)), det);
  det = _mm_div_ss(_mm_set_ss(1.0f), det);
  det = _mm_shuffle_ps(det, det, _MM_SHUFFLE(0, 0, 0, 0));

  Mat44 result;
  result.mCol[0].mValue = _mm_mul_ps(det, minor0);
  result.mCol[1].mValue = _mm_mul_ps(det, minor1);
  result.mCol[2].mValue = _mm_mul_ps(det, minor2);
  result.mCol[3].mValue = _mm_mul_ps(det, minor3);
  return result;
#elif AMARYL_SIMD_NEON
  // Adapted from the SSE version, there's surprising few articles about
  // efficient ways of calculating an inverse for ARM on the internet
  Type tmp1 = ARM_NEON_SHUFFLE_F32x4(mCol[0].mValue, mCol[1].mValue, 0, 1, 4, 5);
  Type row1 = ARM_NEON_SHUFFLE_F32x4(mCol[2].mValue, mCol[3].mValue, 0, 1, 4, 5);
  Type row0 = ARM_NEON_SHUFFLE_F32x4(tmp1, row1, 0, 2, 4, 6);
  row1 = ARM_NEON_SHUFFLE_F32x4(row1, tmp1, 1, 3, 5, 7);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(mCol[0].mValue, mCol[1].mValue, 2, 3, 6, 7);
  Type row3 = ARM_NEON_SHUFFLE_F32x4(mCol[2].mValue, mCol[3].mValue, 2, 3, 6, 7);
  Type row2 = ARM_NEON_SHUFFLE_F32x4(tmp1, row3, 0, 2, 4, 6);
  row3 = ARM_NEON_SHUFFLE_F32x4(row3, tmp1, 1, 3, 5, 7);

  tmp1 = vmulq_f32(row2, row3);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
  Type minor0 = vmulq_f32(row1, tmp1);
  Type minor1 = vmulq_f32(row0, tmp1);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
  minor0 = vsubq_f32(vmulq_f32(row1, tmp1), minor0);
  minor1 = vsubq_f32(vmulq_f32(row0, tmp1), minor1);
  minor1 = ARM_NEON_SHUFFLE_F32x4(minor1, minor1, 2, 3, 0, 1);

  tmp1 = vmulq_f32(row1, row2);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
  minor0 = vaddq_f32(vmulq_f32(row3, tmp1), minor0);
  Type minor3 = vmulq_f32(row0, tmp1);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
  minor0 = vsubq_f32(minor0, vmulq_f32(row3, tmp1));
  minor3 = vsubq_f32(vmulq_f32(row0, tmp1), minor3);
  minor3 = ARM_NEON_SHUFFLE_F32x4(minor3, minor3, 2, 3, 0, 1);

  tmp1 = ARM_NEON_SHUFFLE_F32x4(row1, row1, 2, 3, 0, 1);
  tmp1 = vmulq_f32(tmp1, row3);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
  row2 = ARM_NEON_SHUFFLE_F32x4(row2, row2, 2, 3, 0, 1);
  minor0 = vaddq_f32(vmulq_f32(row2, tmp1), minor0);
  Type minor2 = vmulq_f32(row0, tmp1);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
  minor0 = vsubq_f32(minor0, vmulq_f32(row2, tmp1));
  minor2 = vsubq_f32(vmulq_f32(row0, tmp1), minor2);
  minor2 = ARM_NEON_SHUFFLE_F32x4(minor2, minor2, 2, 3, 0, 1);

  tmp1 = vmulq_f32(row0, row1);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
  minor2 = vaddq_f32(vmulq_f32(row3, tmp1), minor2);
  minor3 = vsubq_f32(vmulq_f32(row2, tmp1), minor3);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
  minor2 = vsubq_f32(vmulq_f32(row3, tmp1), minor2);
  minor3 = vsubq_f32(minor3, vmulq_f32(row2, tmp1));

  tmp1 = vmulq_f32(row0, row3);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
  minor1 = vsubq_f32(minor1, vmulq_f32(row2, tmp1));
  minor2 = vaddq_f32(vmulq_f32(row1, tmp1), minor2);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
  minor1 = vaddq_f32(vmulq_f32(row2, tmp1), minor1);
  minor2 = vsubq_f32(minor2, vmulq_f32(row1, tmp1));

  tmp1 = vmulq_f32(row0, row2);
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 1, 0, 3, 2);
  minor1 = vaddq_f32(vmulq_f32(row3, tmp1), minor1);
  minor3 = vsubq_f32(minor3, vmulq_f32(row1, tmp1));
  tmp1 = ARM_NEON_SHUFFLE_F32x4(tmp1, tmp1, 2, 3, 0, 1);
  minor1 = vsubq_f32(minor1, vmulq_f32(row3, tmp1));
  minor3 = vaddq_f32(vmulq_f32(row1, tmp1), minor3);

  Type det = vmulq_f32(row0, minor0);
  det = vdupq_n_f32(vaddvq_f32(det));
  det = vdivq_f32(vdupq_n_f32(1.0f), det);

  Mat44 result;
  result.mCol[0].mValue = vmulq_f32(det, minor0);
  result.mCol[1].mValue = vmulq_f32(det, minor1);
  result.mCol[2].mValue = vmulq_f32(det, minor2);
  result.mCol[3].mValue = vmulq_f32(det, minor3);
  return result;
#else
  float m00 = MEL(0, 0), m10 = MEL(1, 0), m20 = MEL(2, 0), m30 = MEL(3, 0);
  float m01 = MEL(0, 1), m11 = MEL(1, 1), m21 = MEL(2, 1), m31 = MEL(3, 1);
  float m02 = MEL(0, 2), m12 = MEL(1, 2), m22 = MEL(2, 2), m32 = MEL(3, 2);
  float m03 = MEL(0, 3), m13 = MEL(1, 3), m23 = MEL(2, 3), m33 = MEL(3, 3);

  float m10211120 = m10 * m21 - m11 * m20;
  float m10221220 = m10 * m22 - m12 * m20;
  float m10231320 = m10 * m23 - m13 * m20;
  float m10311130 = m10 * m31 - m11 * m30;
  float m10321230 = m10 * m32 - m12 * m30;
  float m10331330 = m10 * m33 - m13 * m30;
  float m11221221 = m11 * m22 - m12 * m21;
  float m11231321 = m11 * m23 - m13 * m21;
  float m11321231 = m11 * m32 - m12 * m31;
  float m11331331 = m11 * m33 - m13 * m31;
  float m12231322 = m12 * m23 - m13 * m22;
  float m12331332 = m12 * m33 - m13 * m32;
  float m20312130 = m20 * m31 - m21 * m30;
  float m20322230 = m20 * m32 - m22 * m30;
  float m20332330 = m20 * m33 - m23 * m30;
  float m21322231 = m21 * m32 - m22 * m31;
  float m21332331 = m21 * m33 - m23 * m31;
  float m22332332 = m22 * m33 - m23 * m32;

  Vec4 col0(m11 * m22332332 - m12 * m21332331 + m13 * m21322231,
            -m10 * m22332332 + m12 * m20332330 - m13 * m20322230,
            m10 * m21332331 - m11 * m20332330 + m13 * m20312130,
            -m10 * m21322231 + m11 * m20322230 - m12 * m20312130);
  Vec4 col1(-m01 * m22332332 + m02 * m21332331 - m03 * m21322231,
            m00 * m22332332 - m02 * m20332330 + m03 * m20322230,
            -m00 * m21332331 + m01 * m20332330 - m03 * m20312130,
            m00 * m21322231 - m01 * m20322230 + m02 * m20312130);
  Vec4 col2(m01 * m12331332 - m02 * m11331331 + m03 * m11321231,
            -m00 * m12331332 + m02 * m10331330 - m03 * m10321230,
            m00 * m11331331 - m01 * m10331330 + m03 * m10311130,
            -m00 * m11321231 + m01 * m10321230 - m02 * m10311130);
  Vec4 col3(-m01 * m12231322 + m02 * m11231321 - m03 * m11221221,
            m00 * m12231322 - m02 * m10231320 + m03 * m10221220,
            -m00 * m11231321 + m01 * m10231320 - m03 * m10211120,
            m00 * m11221221 - m01 * m10221220 + m02 * m10211120);

  float det =
      m00 * col0.mF32[0] + m01 * col0.mF32[1] + m02 * col0.mF32[2] + m03 * col0.mF32[3];

  return Mat44(col0 / det, col1 / det, col2 / det, col3 / det);
#endif
}

Mat44 Mat44::InversedRotationTranslation() const {
  Mat44 m = Transposed3x3();
  m.SetTranslation(-m.Multiply3x3(GetTranslation()));
  return m;
}

float Mat44::GetDeterminant3x3() const {
  return GetAxisX().Dot(GetAxisY().Cross(GetAxisZ()));
}

Mat44 Mat44::Adjointed3x3() const {
  return Mat44(Vec4(MEL(1, 1), MEL(1, 2), MEL(1, 0), 0) *
                       Vec4(MEL(2, 2), MEL(2, 0), MEL(2, 1), 0) -
                   Vec4(MEL(1, 2), MEL(1, 0), MEL(1, 1), 0) *
                       Vec4(MEL(2, 1), MEL(2, 2), MEL(2, 0), 0),
               Vec4(MEL(0, 2), MEL(0, 0), MEL(0, 1), 0) *
                       Vec4(MEL(2, 1), MEL(2, 2), MEL(2, 0), 0) -
                   Vec4(MEL(0, 1), MEL(0, 2), MEL(0, 0), 0) *
                       Vec4(MEL(2, 2), MEL(2, 0), MEL(2, 1), 0),
               Vec4(MEL(0, 1), MEL(0, 2), MEL(0, 0), 0) *
                       Vec4(MEL(1, 2), MEL(1, 0), MEL(1, 1), 0) -
                   Vec4(MEL(0, 2), MEL(0, 0), MEL(0, 1), 0) *
                       Vec4(MEL(1, 1), MEL(1, 2), MEL(1, 0), 0),
               Vec4(0, 0, 0, 1));
}

Mat44 Mat44::Inversed3x3() const {
  float det = GetDeterminant3x3();

  return Mat44((Vec4(MEL(1, 1), MEL(1, 2), MEL(1, 0), 0) *
                    Vec4(MEL(2, 2), MEL(2, 0), MEL(2, 1), 0) -
                Vec4(MEL(1, 2), MEL(1, 0), MEL(1, 1), 0) *
                    Vec4(MEL(2, 1), MEL(2, 2), MEL(2, 0), 0)) /
                   det,
               (Vec4(MEL(0, 2), MEL(0, 0), MEL(0, 1), 0) *
                    Vec4(MEL(2, 1), MEL(2, 2), MEL(2, 0), 0) -
                Vec4(MEL(0, 1), MEL(0, 2), MEL(0, 0), 0) *
                    Vec4(MEL(2, 2), MEL(2, 0), MEL(2, 1), 0)) /
                   det,
               (Vec4(MEL(0, 1), MEL(0, 2), MEL(0, 0), 0) *
                    Vec4(MEL(1, 2), MEL(1, 0), MEL(1, 1), 0) -
                Vec4(MEL(0, 2), MEL(0, 0), MEL(0, 1), 0) *
                    Vec4(MEL(1, 1), MEL(1, 2), MEL(1, 0), 0)) /
                   det,
               Vec4(0, 0, 0, 1));
}

bool Mat44::SetInversed3x3(Mat44Arg inM) {
  float det = inM.GetDeterminant3x3();

  // If the determinant is zero the matrix is singular and we return false
  if (det == 0.0f) return false;

  // Finish calculating the inverse
  *this = inM.Adjointed3x3();
  mCol[0] /= det;
  mCol[1] /= det;
  mCol[2] /= det;
  return true;
}

Quat Mat44::GetQuaternion() const {
  ASSERT(mCol[3] == Vec4(0, 0, 0, 1));

  float tr = mCol[0].mF32[0] + mCol[1].mF32[1] + mCol[2].mF32[2];

  if (tr >= 0.0f) {
    float s = sqrt(tr + 1.0f);
    float is = 0.5f / s;
    return Quat((mCol[1].mF32[2] - mCol[2].mF32[1]) * is,
                (mCol[2].mF32[0] - mCol[0].mF32[2]) * is,
                (mCol[0].mF32[1] - mCol[1].mF32[0]) * is, 0.5f * s);
  } else {
    int i = 0;
    if (mCol[1].mF32[1] > mCol[0].mF32[0]) i = 1;
    if (mCol[2].mF32[2] > mCol[i].mF32[i]) i = 2;

    if (i == 0) {
      float s = sqrt(mCol[0].mF32[0] - (mCol[1].mF32[1] + mCol[2].mF32[2]) + 1);
      float is = 0.5f / s;
      return Quat(0.5f * s, (mCol[1].mF32[0] + mCol[0].mF32[1]) * is,
                  (mCol[0].mF32[2] + mCol[2].mF32[0]) * is,
                  (mCol[1].mF32[2] - mCol[2].mF32[1]) * is);
    } else if (i == 1) {
      float s = sqrt(mCol[1].mF32[1] - (mCol[2].mF32[2] + mCol[0].mF32[0]) + 1);
      float is = 0.5f / s;
      return Quat((mCol[1].mF32[0] + mCol[0].mF32[1]) * is, 0.5f * s,
                  (mCol[2].mF32[1] + mCol[1].mF32[2]) * is,
                  (mCol[2].mF32[0] - mCol[0].mF32[2]) * is);
    } else {
      ASSERT(i == 2);

      float s = sqrt(mCol[2].mF32[2] - (mCol[0].mF32[0] + mCol[1].mF32[1]) + 1);
      float is = 0.5f / s;
      return Quat((mCol[0].mF32[2] + mCol[2].mF32[0]) * is,
                  (mCol[2].mF32[1] + mCol[1].mF32[2]) * is, 0.5f * s,
                  (mCol[0].mF32[1] - mCol[1].mF32[0]) * is);
    }
  }
}

Mat44 Mat44::Decompose(Vec3& outScale) const {
  // Start the modified Gram-Schmidt algorithm
  // X axis will just be normalized
  Vec3 x = GetAxisX();

  // Make Y axis perpendicular to X
  Vec3 y = GetAxisY();
  float x_dot_x = x.LengthSq();
  y -= (x.Dot(y) / x_dot_x) * x;

  // Make Z axis perpendicular to X
  Vec3 z = GetAxisZ();
  z -= (x.Dot(z) / x_dot_x) * x;

  // Make Z axis perpendicular to Y
  float y_dot_y = y.LengthSq();
  z -= (y.Dot(z) / y_dot_y) * y;

  // Determine the scale
  float z_dot_z = z.LengthSq();
  outScale = Vec3(x_dot_x, y_dot_y, z_dot_z).Sqrt();

  // If the resulting x, y and z vectors don't form a right handed matrix, flip
  // the z axis.
  if (x.Cross(y).Dot(z) < 0.0f) outScale.SetZ(-outScale.GetZ());

  // Determine the rotation and translation
  return Mat44(Vec4(x / outScale.GetX(), 0), Vec4(y / outScale.GetY(), 0),
               Vec4(z / outScale.GetZ(), 0), GetColumn4(3));
}

#undef MEL

}  // namespace amaryl

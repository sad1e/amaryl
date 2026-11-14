#pragma once

namespace amaryl {

/// Sine of x (input in radians)
FORCE_INLINE float Sin(float inX) {
  Vec4 s, c;
  Vec4::sReplicate(inX).SinCos(s, c);
  return s.GetX();
}

/// Cosine of x (input in radians)
FORCE_INLINE float Cos(float inX) {
  Vec4 s, c;
  Vec4::sReplicate(inX).SinCos(s, c);
  return c.GetX();
}

/// Tangent of x (input in radians)
FORCE_INLINE float Tan(float inX) { return Vec4::sReplicate(inX).Tan().GetX(); }

/// Arc sine of x (returns value in the range [-PI / 2, PI / 2])
/// Note that all input values will be clamped to the range [-1, 1] and this
/// function will not return NaNs like std::asin
FORCE_INLINE float ASin(float inX) { return Vec4::sReplicate(inX).ASin().GetX(); }

/// Arc cosine of x (returns value in the range [0, PI])
/// Note that all input values will be clamped to the range [-1, 1] and this
/// function will not return NaNs like std::acos
FORCE_INLINE float ACos(float inX) { return Vec4::sReplicate(inX).ACos().GetX(); }

/// Arc tangent of x (returns value in the range [-PI / 2, PI / 2])
FORCE_INLINE float ATan(float inX) { return Vec4::sReplicate(inX).ATan().GetX(); }

/// Arc tangent of y / x using the signs of the arguments to determine the
/// correct quadrant (returns value in the range [-PI, PI])
FORCE_INLINE float ATan2(float inY, float inX) {
  return Vec4::sATan2(Vec4::sReplicate(inY), Vec4::sReplicate(inX)).GetX();
}

}  // namespace amaryl

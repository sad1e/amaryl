#include <cmath>

#include "amaryl/core/math/math.h"
#include "amaryl/core/math/math_utils.h"
#include "amaryl/core/math/uvec4.h"
#include "amaryl/core/math/vector_aliases.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

bool Near(float actual, float expected, float tolerance = 1.0e-4f) {
  return std::abs(actual - expected) <= tolerance;
}

bool NearVec3(Vec3 actual, Vec3 expected, float tolerance = 1.0e-4f) {
  return Near(actual.GetX(), expected.GetX(), tolerance) &&
         Near(actual.GetY(), expected.GetY(), tolerance) &&
         Near(actual.GetZ(), expected.GetZ(), tolerance);
}

bool NearVec4(Vec4 actual, Vec4 expected, float tolerance = 1.0e-4f) {
  return Near(actual.GetX(), expected.GetX(), tolerance) &&
         Near(actual.GetY(), expected.GetY(), tolerance) &&
         Near(actual.GetZ(), expected.GetZ(), tolerance) &&
         Near(actual.GetW(), expected.GetW(), tolerance);
}

}  // namespace

int main() {
  "math utilities"_test = [] {
    expect(Abs(-7) == 7);
    expect(Min(2, 5) == 2);
    expect(Max(2, 5) == 5);
    expect(Clamp(8, 0, 4) == 4);
    expect(Square(3) == 9);
    expect(Cube(-2) == -8);
    expect(CountTrailingZeros(8) == 3u);
    expect(CountLeadingZeros(1) == 31u);
    expect(CountBits(0b101101u) == 4u);
    expect(GetNextPowerOfTwo(9) == 16u);
    expect(BitCast<uint32>(1.0f) == 0x3f800000u);
    expect(Near(Sin(PI / 2), 1.0f));
    expect(Near(Cos(0.0f), 1.0f));
    expect(Near(ATan2(1.0f, 0.0f), PI / 2));
  };

  "VecN aliases"_test = [] {
    int3 values(1, 2, 3);
    expect(values.X() == 1);
    expect(values.Y() == 2);
    values.Z() = 4;
    expect(values == int3(1, 2, 4));
    expect(values != int3::sZero());
    expect(float4(2.0f) == float4(2.0f, 2.0f, 2.0f, 2.0f));
  };

  "UVec4 lanes and masks"_test = [] {
    const UVec4 values(1, 2, 3, 4);
    expect(values.Swizzle<3, 2, 1, 0>() == UVec4(4, 3, 2, 1));
    expect(values * UVec4(2, 3, 4, 5) == UVec4(2, 6, 12, 20));
    expect(UVec4::sMin(values, UVec4(0, 3, 2, 5)) == UVec4(0, 2, 2, 4));
    expect(UVec4::sMax(values, UVec4(0, 3, 2, 5)) == UVec4(1, 3, 3, 5));
    const UVec4 mask(0xffffffffu, 0, 0xffffffffu, 0);
    expect(mask.GetTrues() == 0b0101);
    expect(mask.CountTrues() == 2);
    expect(mask.TestAnyTrue());
    expect(!mask.TestAllTrue());
    expect(UVec4::sSelect(values, UVec4(9, 9, 9, 9), mask) == UVec4(9, 2, 9, 4));
    expect(values.LogicalShiftLeft<1>() == UVec4(2, 4, 6, 8));
  };

  "Vec3 arithmetic and geometry"_test = [] {
    const Vec3 a(1.0f, 2.0f, 3.0f);
    const Vec3 b(4.0f, -1.0f, 2.0f);
    expect(a + b == Vec3(5, 1, 5));
    expect(a - b == Vec3(-3, 3, 1));
    expect(a * 2.0f == Vec3(2, 4, 6));
    expect(a * b == Vec3(4, -2, 6));
    expect(Near(a.Dot(b), 8.0f));
    expect(NearVec3(Vec3::sAxisX().Cross(Vec3::sAxisY()), Vec3::sAxisZ()));
    expect(Near(a.LengthSq(), 14.0f));
    expect(a.Normalized().IsNormalized());
    expect(Vec3::sZero().NormalizedOr(Vec3::sAxisY()) == Vec3::sAxisY());
    expect(Vec3::sClamp(a, Vec3::sZero(), Vec3::sReplicate(2)) == Vec3(1, 2, 2));
    expect(a.Swizzle<2, 1, 0>() == Vec3(3, 2, 1));
    float3 stored;
    a.StoreFloat3(&stored);
    expect(stored == float3(1.0f, 2.0f, 3.0f));
  };

  "Vec4 arithmetic and trigonometry"_test = [] {
    const Vec4 a(1, 2, 3, 4);
    const Vec4 b(2, 3, 4, 5);
    expect(a + b == Vec4(3, 5, 7, 9));
    expect(a * b == Vec4(2, 6, 12, 20));
    expect(Near(a.Dot(b), 40.0f));
    expect(Near(a.LengthSq(), 30.0f));
    expect(a.Normalized().IsNormalized());
    expect(a.Swizzle<3, 2, 1, 0>() == Vec4(4, 3, 2, 1));
    expect(a.ReduceMin() == 1.0f);
    expect(a.ReduceMax() == 4.0f);
    expect(NearVec4(Vec4(1, 4, 9, 16).Sqrt(), Vec4(1, 2, 3, 4)));
    Vec4 sine, cosine;
    Vec4(0, PI / 2, PI, -PI / 2).SinCos(sine, cosine);
    expect(NearVec4(sine, Vec4(0, 1, 0, -1), 2.0e-4f));
    expect(Near(cosine.GetX(), 1.0f));
  };

  "Quat rotations and interpolation"_test = [] {
    const Quat identity = Quat::sIdentity();
    const Quat quarter = Quat::sRotation(Vec3::sAxisZ(), PI / 2);
    expect(identity.IsNormalized());
    expect(quarter.IsNormalized());
    expect(NearVec3(quarter * Vec3::sAxisX(), Vec3::sAxisY()));
    expect(NearVec3(quarter.InverseRotate(Vec3::sAxisY()), Vec3::sAxisX()));
    expect(NearVec3((quarter * quarter.Conjugated()) * Vec3::sAxisX(), Vec3::sAxisX()));
    expect(NearVec3(quarter.RotateAxisX(), Vec3::sAxisY()));
    expect(NearVec3(identity.SLERP(quarter, 0.5f) * Vec3::sAxisX(),
                    Vec3(std::sqrt(0.5f), std::sqrt(0.5f), 0.0f)));
    Vec3 axis;
    float angle;
    quarter.GetAxisAngle(axis, angle);
    expect(NearVec3(axis, Vec3::sAxisZ()));
    expect(Near(angle, PI / 2));
  };

  "Rotator degrees and quaternion conversion"_test = [] {
    Rotator rotation(0.0f, 0.0f, 90.0f);
    expect(rotation.GetPitch() == 0.0f);
    expect(rotation.GetRoll() == 0.0f);
    expect(rotation.GetYaw() == 90.0f);
    expect(NearVec3(rotation.GetEulers(), Vec3(0, 0, 90)));
    expect(NearVec3(rotation.GetQuat() * Vec3::sAxisX(), Vec3::sAxisY()));
    const Rotator from_quat(rotation.GetQuat());
    expect(Near(from_quat.GetYaw(), 90.0f));
    rotation += Rotator(10, 20, -30);
    expect(rotation == Rotator(10, 20, 60));
    expect(rotation * 2.0f == Rotator(20, 40, 120));
  };

  "Mat44 affine transforms and inverse"_test = [] {
    const Mat44 identity = Mat44::sIdentity();
    const Mat44 translation = Mat44::sTranslation(Vec3(3, -2, 5));
    const Mat44 scale = Mat44::sScale(Vec3(2, 3, 4));
    expect(NearVec3(identity * Vec3(1, 2, 3), Vec3(1, 2, 3)));
    expect(NearVec3(translation * Vec3(1, 2, 3), Vec3(4, 0, 8)));
    expect(NearVec3(scale * Vec3(1, 2, 3), Vec3(2, 6, 12)));
    const Mat44 combined = translation * scale;
    expect(NearVec3(combined * Vec3(1, 2, 3), Vec3(5, 4, 17)));
    expect(combined.Inversed() * combined == identity);
    expect(Near(combined.GetDeterminant3x3(), 24.0f));
    expect(NearVec3(combined.GetTranslation(), Vec3(3, -2, 5)));
    expect(NearVec3(combined.Multiply3x3(Vec3(1, 2, 3)), Vec3(2, 6, 12)));
    expect(NearVec4(combined * Vec4(1, 2, 3, 0), Vec4(2, 6, 12, 0)));
    expect(combined.Transposed().Transposed() == combined);
    expect(NearVec3(Mat44::sRotationZ(PI / 2) * Vec3::sAxisX(), Vec3::sAxisY()));
    const Mat44 trs = Mat44::sTRS(Vec3(3, -2, 5), Quat::sIdentity(), Vec3(2, 3, 4));
    expect(trs.IsClose(combined));
  };

  return cfg<>.run();
}

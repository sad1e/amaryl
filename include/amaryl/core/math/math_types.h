#pragma once

#include <limits>
#include <type_traits>

#include "amaryl/core/amaryl.h"

namespace amaryl {

enum {
  SWIZZLE_X = 0,
  SWIZZLE_Y = 1,
  SWIZZLE_Z = 2,
  SWIZZLE_W = 3,
  SWIZZLE_UNUSED = 2,
};

class Vec3;
class DVec4;
class Vec4;
class DVec4;
class UVec4;
class Quat;
class Mat44;
class DMat44;

class Rotator;

using Vec3Arg = Vec3;
using DVec4Arg = DVec4;
using Vec4Arg = Vec4;
using DVec4Arg = DVec4;
using UVec4Arg = UVec4;
using QuatArg = Quat;
using Mat44Arg = const Mat44&;
using DMat44Arg = const DMat44&;

}  // namespace amaryl

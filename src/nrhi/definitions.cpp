#include "amaryl/nrhi/definitions.h"

#include <cassert>

namespace amaryl {

uint32 getVertexFormatSize(EVertexFormat vertex_format) {
  // clang-format off
#define SIZE4(VKBaseType, BaseType)           \
  case EVertexFormat::VKBaseType##1: return sizeof(BaseType) * 1u; \
  case EVertexFormat::VKBaseType##2: return sizeof(BaseType) * 2u; \
  case EVertexFormat::VKBaseType##3: return sizeof(BaseType) * 3u; \
  case EVertexFormat::VKBaseType##4: return sizeof(BaseType) * 4u

#define SIZE2_4_NORM(VKBaseType, BaseType)           \
  case EVertexFormat::VKBaseType##2Norm: return sizeof(BaseType) * 2u; \
  case EVertexFormat::VKBaseType##4Norm: return sizeof(BaseType) * 4u
  // clang-format on

  switch (vertex_format) {
    SIZE4(Float, float);
    SIZE4(Byte, uint8);
    SIZE4(UByte, uint8);
    SIZE4(Short, uint16);
    SIZE4(UShort, uint16);
    SIZE2_4_NORM(Byte, uint8);
    SIZE2_4_NORM(UByte, uint8);
    SIZE2_4_NORM(Short, uint16);
    SIZE2_4_NORM(UShort, uint16);
    SIZE4(Int, uint32);
    SIZE4(UInt, uint32);
    SIZE4(HalfFloat, uint16);
    case EVertexFormat::Int_2_10_10_10_REV:
      return sizeof(uint32);
    default:
      assert(false);
      return 0;
  }
#undef SIZE4
#undef SIZE2_4_NORM
}

const char* getEntryName(EShaderStage shader_type) {
  switch (shader_type) {
    case EShaderStage::Vertex:
      return kVertexEntryName;
    case EShaderStage::Geometry:
      return kGeometryEntryName;
    case EShaderStage::Fragment:
      return kFragmentEntryName;
    case EShaderStage::Compute:
      return kComputeEntryName;
    case EShaderStage::Task:
      return kTaskEntryName;
    case EShaderStage::Mesh:
      return kMeshEntryName;
    case EShaderStage::RayGeneration:
      return kRayGenEntryName;
    case EShaderStage::AnyHit:
      return kAnyHitEntryName;
    case EShaderStage::ClosestHit:
      return kClosestHitEntryName;
    case EShaderStage::Miss:
      return kMissEntryName;
    case EShaderStage::Intersection:
      return kIntersectionEntryName;
    case EShaderStage::Callable:
      return kCallableEntryName;
    default:
      return "ERROR_ENTRY_NAME";
  }
}

}  // namespace amaryl

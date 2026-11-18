#pragma once

// GPU scene data shared between C++ and Slang.
//
// Layout rule: structs only contain 4-byte scalars, 16-byte vectors on 16-byte offsets
// and 8-byte device addresses on 8-byte offsets, so C++, std430 and scalar layouts are
// identical.
//
// Indexing:
//   GpuInstanceId (stable slot, never compacted) indexes instances[], worldCurr[],
//   worldPrev[]. GpuInstance::meshIndex          -> meshes[]
//   GpuInstance::materialSlotOffset -> materialSlots[] (GpuMesh::sectionCount entries)
//                                      -> materials[]
//   GpuMesh::lods[i].meshletOffset  -> meshlets[]
//   GpuMesh::lods[i].sectionOffset  -> meshSections[] (GpuMesh::sectionCount entries)

#include "amaryl/nrhi/descriptor.shared.h"

#if defined(__SLANG__)
// clang-format off
#define GPU_SCENE_UINT    uint
#define GPU_SCENE_UINT64  uint64_t
#define GPU_SCENE_CONST   static const uint
#define GPU_SCENE_PTR(T)  T*
#define GPU_SCENE_INLINE  [ForceInline]
// clang-format on
#else
#include <cstddef>
#include <type_traits>

#include "amaryl/core/amaryl.h"
#include "amaryl/core/math/vector_aliases.h"

namespace amaryl {

// clang-format off
#define GPU_SCENE_UINT    uint32
#define GPU_SCENE_UINT64  uint64
#define GPU_SCENE_CONST   inline constexpr uint32
#define GPU_SCENE_PTR(T)  uint64
#define GPU_SCENE_INLINE  inline
// clang-format on
#endif

GPU_SCENE_CONST GPU_SCENE_INVALID_INDEX = 0xffffffffu;
GPU_SCENE_CONST GPU_SCENE_MAX_LODS = 8u;
GPU_SCENE_CONST GPU_SCENE_MAX_MESHLET_VERTICES = 64u;
GPU_SCENE_CONST GPU_SCENE_MAX_MESHLET_TRIANGLES = 124u;

// vertexAttributes, GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS per vertex:
//   [0] normal, octahedral: bits [0, 16) x snorm16, [16, 32) y snorm16.
//   [1] tangent, octahedral: bits [0, 16) x snorm16, [16, 31) y snorm15, bit 31 is set
//       when the bitangent sign (tangent.w) is negative.
//   [2] uv0.x, [3] uv0.y as float bits.
GPU_SCENE_CONST GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS = 4u;

// GpuInstance::flags, bits [24, 32) hold the ray tracing visibility mask.
GPU_SCENE_CONST GPU_INSTANCE_FLAG_VALID = 1u << 0;
GPU_SCENE_CONST GPU_INSTANCE_FLAG_CAST_SHADOW = 1u << 1;
// Transform is uploaded once at creation and never changes.
GPU_SCENE_CONST GPU_INSTANCE_FLAG_STATIC = 1u << 2;
GPU_SCENE_CONST GPU_INSTANCE_FLAG_SKINNED = 1u << 3;
GPU_SCENE_CONST GPU_INSTANCE_FLAG_RAY_TRACING = 1u << 4;
GPU_SCENE_CONST GPU_INSTANCE_RT_MASK_SHIFT = 24u;

// GpuMaterial::flags
GPU_SCENE_CONST GPU_MATERIAL_FLAG_ALPHA_MASK = 1u << 0;
GPU_SCENE_CONST GPU_MATERIAL_FLAG_ALPHA_BLEND = 1u << 1;
GPU_SCENE_CONST GPU_MATERIAL_FLAG_DOUBLE_SIDED = 1u << 2;

// GpuTransformUpdate::flags
// prev = curr = world: no motion vectors across a teleport.
GPU_SCENE_CONST GPU_TRANSFORM_UPDATE_TELEPORT = 1u << 0;
// prev = curr, world is ignored: the instance moved last frame but not this frame.
GPU_SCENE_CONST GPU_TRANSFORM_UPDATE_SETTLE = 1u << 1;

// Affine 3x4, row-major: rows[r] = (m[r][0], m[r][1], m[r][2], translation[r]).
struct GpuTransform {
  float4 rows[3];
};

struct GpuInstance {
  // xyz center, w radius, in local space. Copied from GpuMesh at creation and overridden
  // for skinned instances, so culling never has to fetch the mesh.
  float4 localBoundingSphere;
  GPU_SCENE_UINT meshIndex;
  GPU_SCENE_UINT materialSlotOffset;
  // First matrix in skinningMatrices[], GPU_SCENE_INVALID_INDEX when not skinned.
  GPU_SCENE_UINT skinningOffset;
  GPU_SCENE_UINT flags;
};

struct GpuMeshLod {
  GPU_SCENE_UINT meshletOffset;
  GPU_SCENE_UINT meshletCount;
  GPU_SCENE_UINT sectionOffset;
  // Object-space error at scale 1, projected to screen space for LOD selection.
  float error;
};

// Index range of one material section, used by the MDI fallback path.
struct GpuMeshSection {
  GPU_SCENE_UINT firstIndex;
  GPU_SCENE_UINT indexCount;
  GPU_SCENE_UINT materialSlot;
  GPU_SCENE_UINT pad0;
};

struct GpuMesh {
  float4 boundingSphere;
  float4 aabbCenter;
  float4 aabbExtents;
  GPU_SCENE_UINT baseVertex;
  GPU_SCENE_UINT lodCount;
  GPU_SCENE_UINT sectionCount;
  GPU_SCENE_UINT flags;
  // VkAccelerationStructureInstanceKHR::accelerationStructureReference, 0 if none.
  GPU_SCENE_UINT64 blasAddress;
  GPU_SCENE_UINT pad0;
  GPU_SCENE_UINT pad1;
  GpuMeshLod lods[GPU_SCENE_MAX_LODS];
};

struct GpuMeshlet {
  float4 boundingSphere;
  // Into meshletVertices[]; the stored values are relative to GpuMesh::baseVertex.
  GPU_SCENE_UINT vertexOffset;
  // Byte offset into meshletTriangles[] (3 x uint8 per triangle), 4-byte aligned.
  GPU_SCENE_UINT triangleOffset;
  // [0, 8) vertex count, [8, 16) triangle count, [16, 32) section index.
  GPU_SCENE_UINT packedCounts;
  // snorm8 cone axis xyz + snorm8 cone cutoff.
  GPU_SCENE_UINT packedCone;
};

struct GpuMaterial {
  float4 baseColorFactor;
  // xyz emissive factor, w alpha cutoff.
  float4 emissiveAlphaCutoff;
  float metallicFactor;
  float roughnessFactor;
  float normalScale;
  float occlusionStrength;
  // Descriptor index 0 is the RHI dummy texture and means "no texture": skip sampling.
  TextureDescriptorIndex baseColorTexture;
  TextureDescriptorIndex metallicRoughnessTexture;
  TextureDescriptorIndex normalTexture;
  TextureDescriptorIndex occlusionTexture;
  TextureDescriptorIndex emissiveTexture;
  SamplerDescriptorIndex sampler;
  // Selects the pipeline bin draws are compacted into.
  GPU_SCENE_UINT pipelineBin;
  GPU_SCENE_UINT flags;
};

// Consumed by the transform scatter pass.
struct GpuTransformUpdate {
  GPU_SCENE_UINT instanceId;
  GPU_SCENE_UINT flags;
  GPU_SCENE_UINT pad0;
  GPU_SCENE_UINT pad1;
  GpuTransform world;
};

// Generic scatter for instances, materials, meshes, ... The destination base address is
// provided per dispatch. Offsets and sizes are in dwords.
struct GpuScatterCopy {
  GPU_SCENE_UINT dstOffset;
  GPU_SCENE_UINT srcOffset;
  GPU_SCENE_UINT dwordCount;
  GPU_SCENE_UINT pad0;
};

// Device addresses of every scene buffer, bound once per frame.
struct GpuSceneRoot {
  GPU_SCENE_PTR(GpuInstance) instances;
  GPU_SCENE_PTR(GpuTransform) worldCurr;
  GPU_SCENE_PTR(GpuTransform) worldPrev;
  GPU_SCENE_PTR(GpuTransform) skinningMatrices;
  GPU_SCENE_PTR(GPU_SCENE_UINT) materialSlots;
  GPU_SCENE_PTR(GpuMaterial) materials;
  GPU_SCENE_PTR(GpuMesh) meshes;
  GPU_SCENE_PTR(GpuMeshSection) meshSections;
  GPU_SCENE_PTR(GpuMeshlet) meshlets;
  GPU_SCENE_PTR(GPU_SCENE_UINT) meshletVertices;
  GPU_SCENE_PTR(GPU_SCENE_UINT) meshletTriangles;
  GPU_SCENE_PTR(GPU_SCENE_UINT) indices;
  // Tightly packed float3.
  GPU_SCENE_PTR(float) vertexPositions;
  GPU_SCENE_PTR(GPU_SCENE_UINT) vertexAttributes;
  // Highest allocated GpuInstanceId + 1, i.e. the culling dispatch size.
  GPU_SCENE_UINT instanceCapacity;
  GPU_SCENE_UINT meshCount;
  GPU_SCENE_UINT materialCount;
  GPU_SCENE_UINT frameIndex;
};

GPU_SCENE_INLINE GPU_SCENE_UINT gpuPackMeshletCounts(GPU_SCENE_UINT vertex_count,
                                                     GPU_SCENE_UINT triangle_count,
                                                     GPU_SCENE_UINT section) {
  return (vertex_count & 0xffu) | ((triangle_count & 0xffu) << 8u) | (section << 16u);
}

GPU_SCENE_INLINE GPU_SCENE_UINT gpuMeshletVertexCount(GPU_SCENE_UINT packed_counts) {
  return packed_counts & 0xffu;
}

GPU_SCENE_INLINE GPU_SCENE_UINT gpuMeshletTriangleCount(GPU_SCENE_UINT packed_counts) {
  return (packed_counts >> 8u) & 0xffu;
}

GPU_SCENE_INLINE GPU_SCENE_UINT gpuMeshletSection(GPU_SCENE_UINT packed_counts) {
  return packed_counts >> 16u;
}

GPU_SCENE_INLINE GPU_SCENE_UINT gpuInstanceRayTracingMask(GPU_SCENE_UINT flags) {
  return flags >> GPU_INSTANCE_RT_MASK_SHIFT;
}

#if defined(__SLANG__)
GPU_SCENE_INLINE float3 gpuTransformPoint(GpuTransform t, float3 p) {
  return float3(dot(t.rows[0].xyz, p) + t.rows[0].w, dot(t.rows[1].xyz, p) + t.rows[1].w,
                dot(t.rows[2].xyz, p) + t.rows[2].w);
}

GPU_SCENE_INLINE float3 gpuTransformVector(GpuTransform t, float3 v) {
  return float3(dot(t.rows[0].xyz, v), dot(t.rows[1].xyz, v), dot(t.rows[2].xyz, v));
}

GPU_SCENE_INLINE float gpuTransformMaxScale(GpuTransform t) {
  const float3 x = float3(t.rows[0].x, t.rows[1].x, t.rows[2].x);
  const float3 y = float3(t.rows[0].y, t.rows[1].y, t.rows[2].y);
  const float3 z = float3(t.rows[0].z, t.rows[1].z, t.rows[2].z);
  return sqrt(max(dot(x, x), max(dot(y, y), dot(z, z))));
}

GPU_SCENE_INLINE float4 gpuTransformSphere(GpuTransform t, float4 sphere) {
  return float4(gpuTransformPoint(t, sphere.xyz), sphere.w * gpuTransformMaxScale(t));
}

GPU_SCENE_INLINE float3 gpuDecodeOctahedral(float2 e) {
  float3 n = float3(e.x, e.y, 1.0 - abs(e.x) - abs(e.y));
  const float t = max(-n.z, 0.0);
  n.x += n.x >= 0.0 ? -t : t;
  n.y += n.y >= 0.0 ? -t : t;
  return normalize(n);
}

GPU_SCENE_INLINE float3 gpuUnpackVertexNormal(uint packed) {
  const int x = int(packed << 16u) >> 16;
  const int y = int(packed) >> 16;
  return gpuDecodeOctahedral(max(float2(x, y) / 32767.0, -1.0));
}

GPU_SCENE_INLINE float4 gpuUnpackVertexTangent(uint packed) {
  const int x = int(packed << 16u) >> 16;
  const int y = int(packed << 1u) >> 17;
  const float3 t = gpuDecodeOctahedral(max(float2(x / 32767.0, y / 16383.0), -1.0));
  return float4(t, (packed >> 31u) != 0u ? -1.0 : 1.0);
}
#else
static_assert(sizeof(GpuTransform) == 48);
static_assert(sizeof(GpuInstance) == 32);
static_assert(sizeof(GpuMeshLod) == 16);
static_assert(sizeof(GpuMeshSection) == 16);
static_assert(sizeof(GpuMesh) == 80 + 16 * GPU_SCENE_MAX_LODS);
static_assert(offsetof(GpuMesh, blasAddress) % 8 == 0);
static_assert(offsetof(GpuMesh, lods) % 16 == 0);
static_assert(sizeof(GpuMeshlet) == 32);
static_assert(sizeof(GpuMaterial) == 80);
static_assert(sizeof(GpuTransformUpdate) == 64);
static_assert(sizeof(GpuScatterCopy) == 16);
static_assert(sizeof(GpuSceneRoot) == 128);
static_assert(std::is_trivially_copyable_v<GpuInstance>);
static_assert(std::is_trivially_copyable_v<GpuMesh>);
static_assert(std::is_trivially_copyable_v<GpuMaterial>);
static_assert(std::is_trivially_copyable_v<GpuTransformUpdate>);

}  // namespace amaryl
#endif

#undef GPU_SCENE_INLINE
#undef GPU_SCENE_PTR
#undef GPU_SCENE_CONST
#undef GPU_SCENE_UINT64
#undef GPU_SCENE_UINT

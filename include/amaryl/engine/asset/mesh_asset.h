#pragma once

#include <string>
#include <vector>

#include "amaryl/core/math/math.h"
#include "amaryl/core/span.h"
#include "amaryl/engine/engine.h"
#include "amaryl/engine/render/gpu_scene.shared.h"

namespace amaryl {

// Cooked static mesh in the GPU scene layout, read-only once built and shared by every
// instance. All offsets are relative to the arrays of this asset, `MeshRegistry` rebases
// them when the mesh is uploaded. Only LOD0 is built for now.
struct MeshAsset {
  std::string name;

  // Mesh space bounds, xyz center and w radius for the sphere.
  float4 boundingSphere{0.0f, 0.0f, 0.0f, 0.0f};
  float3 aabbMin{0.0f, 0.0f, 0.0f};
  float3 aabbMax{0.0f, 0.0f, 0.0f};

  std::vector<float3> positions;
  // GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS per vertex.
  std::vector<uint32> attributes;
  // Triangle list of every LOD, relative to the first vertex of the mesh.
  std::vector<uint32> indices;

  std::vector<GpuMeshLod> lods;
  // `sectionCount` entries per LOD, firstIndex is relative to `indices`.
  std::vector<GpuMeshSection> sections;
  std::vector<GpuMeshlet> meshlets;
  // Relative to the first vertex of the mesh.
  std::vector<uint32> meshletVertices;
  // 3 x uint8 per triangle, every meshlet starts on a 4-byte boundary and the size is a
  // multiple of 4.
  std::vector<uint8> meshletTriangles;
  uint32 sectionCount = 0;

  uint32 getVertexCount() const { return static_cast<uint32>(positions.size()); }
};

// One material section of the source geometry, e.g. a glTF primitive.
struct MeshSourceSection {
  Span<const float3> positions;
  // Optional streams, either empty or one entry per position. Flat normals are generated
  // when normals are missing, MikkTSpace compatible tangents when tangents are missing.
  Span<const float3> normals;
  // xyz tangent, w bitangent sign.
  Span<const float4> tangents;
  Span<const float2> uvs;
  // Triangle list, empty for non-indexed geometry.
  Span<const uint32> indices;
};

struct MeshBuildOptions {
  // Meshlet size versus normal cone tightness, see meshopt_buildMeshlets().
  float coneWeight = 0.25f;
  bool optimizeVertexCache = true;
};

// Welds identical vertices, optimizes the index order, builds meshlets, packed vertex
// attributes and bounds. Section i uses material slot i. `out.name` is kept. Returns
// false and leaves `out` untouched when a section has invalid streams or the mesh has no
// triangle.
ENGINE_API bool buildMeshAsset(Span<const MeshSourceSection> sections,
                               const MeshBuildOptions& options, MeshAsset& out);

// Encoders and decoders of the vertexAttributes layout, see gpu_scene.shared.h.
ENGINE_API uint32 packVertexNormal(Vec3Arg normal);
ENGINE_API uint32 packVertexTangent(Vec3Arg tangent, float sign);
ENGINE_API Vec3 unpackVertexNormal(uint32 packed);
// w is the bitangent sign.
ENGINE_API Vec4 unpackVertexTangent(uint32 packed);

}  // namespace amaryl

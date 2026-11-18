#include "amaryl/engine/asset/mesh_asset.h"

#include <meshoptimizer.h>

#include <bit>
#include <cmath>

#include "amaryl/core/logger.h"

namespace amaryl {
namespace {

constexpr uint32 kAttributeDwords = GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS;
constexpr size_t kAttributeBytes = kAttributeDwords * sizeof(uint32);

// Maps a direction to [-1, 1]^2, inverse of gpuDecodeOctahedral().
void encodeOctahedral(Vec3Arg v, float& out_x, float& out_y) {
  const float l1 = Abs(v.GetX()) + Abs(v.GetY()) + Abs(v.GetZ());
  if (!(l1 > 1.0e-20f)) {
    out_x = 0.0f;
    out_y = 0.0f;
    return;
  }

  const float x = v.GetX() / l1;
  const float y = v.GetY() / l1;
  if (v.GetZ() >= 0.0f) {
    out_x = x;
    out_y = y;
  } else {
    out_x = (1.0f - Abs(y)) * (x >= 0.0f ? 1.0f : -1.0f);
    out_y = (1.0f - Abs(x)) * (y >= 0.0f ? 1.0f : -1.0f);
  }
}

Vec3 decodeOctahedral(float x, float y) {
  const float z = 1.0f - Abs(x) - Abs(y);
  const float t = Max(-z, 0.0f);
  return Vec3(x >= 0.0f ? x - t : x + t, y >= 0.0f ? y - t : y + t, z).Normalized();
}

uint32 packSnorm(float value, float scale, uint32 mask) {
  const long quantized = std::lround(Clamp(value, -1.0f, 1.0f) * scale);
  return static_cast<uint32>(static_cast<int32>(quantized)) & mask;
}

float unpackSnorm(int32 value, float scale) {
  return Max(static_cast<float>(value) / scale, -1.0f);
}

uint32 packCone(const meshopt_Bounds& bounds) {
  const auto byte = [](signed char value) {
    return static_cast<uint32>(static_cast<uint8>(value));
  };
  return byte(bounds.cone_axis_s8[0]) | (byte(bounds.cone_axis_s8[1]) << 8u) |
         (byte(bounds.cone_axis_s8[2]) << 16u) | (byte(bounds.cone_cutoff_s8) << 24u);
}

size_t getCornerCount(const MeshSourceSection& section) {
  return section.indices.empty() ? section.positions.size() : section.indices.size();
}

bool validateSection(const MeshSourceSection& section, size_t section_index) {
  const size_t vertex_count = section.positions.size();
  const auto valid_stream = [vertex_count](size_t size) {
    return size == 0 || size == vertex_count;
  };
  if (!valid_stream(section.normals.size()) || !valid_stream(section.tangents.size()) ||
      !valid_stream(section.uvs.size())) {
    ALOGE("buildMeshAsset: streams of section %zu differ in size", section_index);
    return false;
  }

  if (getCornerCount(section) % 3 != 0) {
    ALOGE("buildMeshAsset: section %zu is not a triangle list", section_index);
    return false;
  }

  for (const uint32 index : section.indices) {
    if (index >= vertex_count) {
      ALOGE("buildMeshAsset: section %zu has out of range index %u", section_index,
            index);
      return false;
    }
  }
  return true;
}

// Writes the unindexed positions and packed attributes of every corner of `section`,
// generating the missing normals and tangents.
void buildCorners(const MeshSourceSection& section, float3* positions,
                  uint32* attributes) {
  const size_t count = getCornerCount(section);
  if (count == 0) {
    return;
  }

  std::vector<float3> normals(count);
  std::vector<float4> tangents(count);
  std::vector<float2> uvs(count, float2{0.0f, 0.0f});
  for (size_t corner = 0; corner < count; ++corner) {
    const size_t vertex = section.indices.empty() ? corner : section.indices[corner];
    positions[corner] = section.positions[vertex];
    if (!section.normals.empty()) {
      Vec3(section.normals[vertex])
          .NormalizedOr(Vec3::sAxisZ())
          .StoreFloat3(&normals[corner]);
    }
    if (!section.tangents.empty()) {
      tangents[corner] = section.tangents[vertex];
    }
    if (!section.uvs.empty()) {
      uvs[corner] = section.uvs[vertex];
    }
  }

  if (section.normals.empty()) {
    for (size_t corner = 0; corner < count; corner += 3) {
      const Vec3 p0(positions[corner]);
      const Vec3 edge1 = Vec3(positions[corner + 1]) - p0;
      const Vec3 edge2 = Vec3(positions[corner + 2]) - p0;
      const Vec3 normal = edge1.Cross(edge2).NormalizedOr(Vec3::sAxisZ());
      for (size_t i = 0; i < 3; ++i) {
        normal.StoreFloat3(&normals[corner + i]);
      }
    }
  }

  if (section.tangents.empty() && !section.uvs.empty()) {
    meshopt_generateTangents(tangents[0].Data, nullptr, count, positions[0].Data, count,
                             sizeof(float3), normals[0].Data, sizeof(float3), uvs[0].Data,
                             sizeof(float2), meshopt_TangentCompatible);
  } else if (section.tangents.empty()) {
    // Without UVs there is no normal mapping, any tangent frame works.
    for (size_t corner = 0; corner < count; ++corner) {
      const Vec3 tangent = Vec3(normals[corner]).GetNormalizedPerpendicular();
      tangents[corner] = float4{tangent.GetX(), tangent.GetY(), tangent.GetZ(), 1.0f};
    }
  }

  for (size_t corner = 0; corner < count; ++corner) {
    const float4& tangent = tangents[corner];
    uint32* attribute = attributes + corner * kAttributeDwords;
    attribute[0] = packVertexNormal(Vec3(normals[corner]));
    attribute[1] =
        packVertexTangent(Vec3(tangent[0], tangent[1], tangent[2]), tangent[3]);
    attribute[2] = std::bit_cast<uint32>(uvs[corner][0]);
    attribute[3] = std::bit_cast<uint32>(uvs[corner][1]);
  }
}

void buildMeshlets(MeshAsset& asset, const GpuMeshSection& section, uint32 section_index,
                   float cone_weight) {
  const size_t max_vertices = GPU_SCENE_MAX_MESHLET_VERTICES;
  const size_t max_triangles = GPU_SCENE_MAX_MESHLET_TRIANGLES;
  const uint32* indices = asset.indices.data() + section.firstIndex;
  const float* positions = asset.positions[0].Data;
  const size_t vertex_count = asset.positions.size();

  std::vector<meshopt_Meshlet> meshlets(
      meshopt_buildMeshletsBound(section.indexCount, max_vertices, max_triangles));
  std::vector<uint32> meshlet_vertices(section.indexCount);
  std::vector<uint8> meshlet_triangles(section.indexCount);
  meshlets.resize(meshopt_buildMeshlets(
      meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(), indices,
      section.indexCount, positions, vertex_count, sizeof(float3), max_vertices,
      max_triangles, cone_weight));

  for (const meshopt_Meshlet& meshlet : meshlets) {
    uint32* vertices = meshlet_vertices.data() + meshlet.vertex_offset;
    uint8* triangles = meshlet_triangles.data() + meshlet.triangle_offset;
    meshopt_optimizeMeshlet(vertices, triangles, meshlet.triangle_count,
                            meshlet.vertex_count);
    const meshopt_Bounds bounds =
        meshopt_computeMeshletBounds(vertices, triangles, meshlet.triangle_count,
                                     positions, vertex_count, sizeof(float3));

    GpuMeshlet& gpu_meshlet = asset.meshlets.emplace_back();
    gpu_meshlet.boundingSphere =
        float4{bounds.center[0], bounds.center[1], bounds.center[2], bounds.radius};
    gpu_meshlet.vertexOffset = static_cast<uint32>(asset.meshletVertices.size());
    gpu_meshlet.triangleOffset = static_cast<uint32>(asset.meshletTriangles.size());
    gpu_meshlet.packedCounts =
        gpuPackMeshletCounts(meshlet.vertex_count, meshlet.triangle_count, section_index);
    gpu_meshlet.packedCone = packCone(bounds);

    asset.meshletVertices.insert(asset.meshletVertices.end(), vertices,
                                 vertices + meshlet.vertex_count);
    asset.meshletTriangles.insert(asset.meshletTriangles.end(), triangles,
                                  triangles + meshlet.triangle_count * 3);
    asset.meshletTriangles.resize((asset.meshletTriangles.size() + 3) & ~size_t{3}, 0);
  }
}

}  // namespace

uint32 packVertexNormal(Vec3Arg normal) {
  float x = 0.0f;
  float y = 0.0f;
  encodeOctahedral(normal, x, y);
  return packSnorm(x, 32767.0f, 0xffffu) | (packSnorm(y, 32767.0f, 0xffffu) << 16u);
}

uint32 packVertexTangent(Vec3Arg tangent, float sign) {
  float x = 0.0f;
  float y = 0.0f;
  encodeOctahedral(tangent, x, y);
  return packSnorm(x, 32767.0f, 0xffffu) | (packSnorm(y, 16383.0f, 0x7fffu) << 16u) |
         (sign < 0.0f ? 1u << 31u : 0u);
}

Vec3 unpackVertexNormal(uint32 packed) {
  const int32 x = static_cast<int16>(packed & 0xffffu);
  const int32 y = static_cast<int16>(packed >> 16u);
  return decodeOctahedral(unpackSnorm(x, 32767.0f), unpackSnorm(y, 32767.0f));
}

Vec4 unpackVertexTangent(uint32 packed) {
  const int32 x = static_cast<int16>(packed & 0xffffu);
  const int32 y = static_cast<int32>(packed << 1u) >> 17;
  const Vec3 tangent =
      decodeOctahedral(unpackSnorm(x, 32767.0f), unpackSnorm(y, 16383.0f));
  return Vec4(tangent, (packed >> 31u) != 0u ? -1.0f : 1.0f);
}

bool buildMeshAsset(Span<const MeshSourceSection> sections,
                    const MeshBuildOptions& options, MeshAsset& out) {
  if (sections.empty() || sections.size() > 0xffffu) {
    ALOGE("buildMeshAsset: invalid section count %zu", sections.size());
    return false;
  }

  size_t corner_count = 0;
  for (size_t i = 0; i < sections.size(); ++i) {
    if (!validateSection(sections[i], i)) {
      return false;
    }
    corner_count += getCornerCount(sections[i]);
  }
  if (corner_count == 0 || corner_count > 0xffffffffu) {
    ALOGE("buildMeshAsset: invalid triangle count %zu", corner_count / 3);
    return false;
  }

  // Every section is unindexed first, then identical corners are welded back together,
  // including corners of different sections.
  std::vector<float3> corner_positions(corner_count);
  std::vector<uint32> corner_attributes(corner_count * kAttributeDwords);
  MeshAsset asset;
  asset.sectionCount = static_cast<uint32>(sections.size());
  uint32 first_index = 0;
  for (uint32 i = 0; i < asset.sectionCount; ++i) {
    const uint32 index_count = static_cast<uint32>(getCornerCount(sections[i]));
    buildCorners(sections[i], corner_positions.data() + first_index,
                 corner_attributes.data() + size_t{first_index} * kAttributeDwords);
    asset.sections.push_back({first_index, index_count, i, 0});
    first_index += index_count;
  }

  const meshopt_Stream streams[] = {
      {corner_positions.data(), sizeof(float3), sizeof(float3)},
      {corner_attributes.data(), kAttributeBytes, kAttributeBytes},
  };
  std::vector<uint32> remap(corner_count);
  size_t vertex_count = meshopt_generateVertexRemapMulti(
      remap.data(), nullptr, corner_count, corner_count, streams, std::size(streams));
  asset.positions.resize(vertex_count);
  asset.attributes.resize(vertex_count * kAttributeDwords);
  asset.indices.resize(corner_count);
  meshopt_remapVertexBuffer(asset.positions.data(), corner_positions.data(), corner_count,
                            sizeof(float3), remap.data());
  meshopt_remapVertexBuffer(asset.attributes.data(), corner_attributes.data(),
                            corner_count, kAttributeBytes, remap.data());
  meshopt_remapIndexBuffer(asset.indices.data(), nullptr, corner_count, remap.data());

  if (options.optimizeVertexCache) {
    for (const GpuMeshSection& section : asset.sections) {
      uint32* indices = asset.indices.data() + section.firstIndex;
      meshopt_optimizeVertexCache(indices, indices, section.indexCount, vertex_count);
    }
  }

  // Orders vertices by first use so every section reads a compact vertex range.
  vertex_count = meshopt_optimizeVertexFetchRemap(remap.data(), asset.indices.data(),
                                                  corner_count, vertex_count);
  meshopt_remapVertexBuffer(asset.positions.data(), asset.positions.data(),
                            asset.positions.size(), sizeof(float3), remap.data());
  meshopt_remapVertexBuffer(asset.attributes.data(), asset.attributes.data(),
                            asset.positions.size(), kAttributeBytes, remap.data());
  meshopt_remapIndexBuffer(asset.indices.data(), asset.indices.data(), corner_count,
                           remap.data());
  asset.positions.resize(vertex_count);
  asset.attributes.resize(vertex_count * kAttributeDwords);

  for (uint32 i = 0; i < asset.sectionCount; ++i) {
    if (asset.sections[i].indexCount != 0) {
      buildMeshlets(asset, asset.sections[i], i, options.coneWeight);
    }
  }

  GpuMeshLod& lod = asset.lods.emplace_back();
  lod.meshletOffset = 0;
  lod.meshletCount = static_cast<uint32>(asset.meshlets.size());
  lod.sectionOffset = 0;
  lod.error = 0.0f;

  Vec3 aabb_min(asset.positions[0]);
  Vec3 aabb_max = aabb_min;
  for (const float3& position : asset.positions) {
    aabb_min = Vec3::sMin(aabb_min, Vec3(position));
    aabb_max = Vec3::sMax(aabb_max, Vec3(position));
  }
  aabb_min.StoreFloat3(&asset.aabbMin);
  aabb_max.StoreFloat3(&asset.aabbMax);
  const meshopt_Bounds sphere = meshopt_computeSphereBounds(
      asset.positions[0].Data, vertex_count, sizeof(float3), nullptr, 0);
  asset.boundingSphere =
      float4{sphere.center[0], sphere.center[1], sphere.center[2], sphere.radius};

  asset.name = std::move(out.name);
  out = std::move(asset);
  return true;
}

}  // namespace amaryl

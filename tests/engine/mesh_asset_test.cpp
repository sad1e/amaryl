#include "amaryl/engine/asset/mesh_asset.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <random>
#include <vector>

#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

using Triangle = std::array<uint32, 3>;

// Rotates the smallest index first, keeps the winding.
Triangle canonical(uint32 a, uint32 b, uint32 c) {
  if (b < a && b < c) {
    return {b, c, a};
  }
  if (c < a && c < b) {
    return {c, a, b};
  }
  return {a, b, c};
}

// Triangles of every section from the index buffer.
std::vector<std::vector<Triangle>> getIndexTriangles(const MeshAsset& asset) {
  std::vector<std::vector<Triangle>> out(asset.sectionCount);
  for (uint32 s = 0; s < asset.sectionCount; ++s) {
    const GpuMeshSection& section = asset.sections[s];
    for (uint32 i = 0; i < section.indexCount; i += 3) {
      const uint32* index = asset.indices.data() + section.firstIndex + i;
      out[s].push_back(canonical(index[0], index[1], index[2]));
    }
    std::sort(out[s].begin(), out[s].end());
  }
  return out;
}

// Triangles of every section from the meshlets.
std::vector<std::vector<Triangle>> getMeshletTriangles(const MeshAsset& asset) {
  std::vector<std::vector<Triangle>> out(asset.sectionCount);
  for (const GpuMeshlet& meshlet : asset.meshlets) {
    const uint32 vertex_count = gpuMeshletVertexCount(meshlet.packedCounts);
    const uint32 section = gpuMeshletSection(meshlet.packedCounts);
    expect(section < asset.sectionCount);
    expect(vertex_count <= GPU_SCENE_MAX_MESHLET_VERTICES);
    expect(meshlet.triangleOffset % 4 == 0u);
    for (uint32 t = 0; t < gpuMeshletTriangleCount(meshlet.packedCounts); ++t) {
      uint32 vertices[3];
      for (uint32 k = 0; k < 3; ++k) {
        const uint32 local = asset.meshletTriangles[meshlet.triangleOffset + t * 3 + k];
        expect(local < vertex_count);
        vertices[k] = asset.meshletVertices[meshlet.vertexOffset + local];
      }
      out[section].push_back(canonical(vertices[0], vertices[1], vertices[2]));
    }
  }
  for (std::vector<Triangle>& triangles : out) {
    std::sort(triangles.begin(), triangles.end());
  }
  return out;
}

Vec3 getPosition(const MeshAsset& asset, uint32 vertex) {
  return Vec3(asset.positions[vertex]);
}

float2 getUv(const MeshAsset& asset, uint32 vertex) {
  const uint32* attribute =
      asset.attributes.data() + vertex * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS;
  return float2{std::bit_cast<float>(attribute[2]), std::bit_cast<float>(attribute[3])};
}

// Unit quad in the XY plane at height z, facing +Z, uv = xy.
struct Quad {
  std::vector<float3> positions;
  std::vector<float3> normals;
  std::vector<float2> uvs;
  std::vector<uint32> indices;

  Quad(float z, bool indexed) {
    const float corners[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const uint32 quad_indices[6] = {0, 1, 2, 0, 2, 3};
    for (uint32 i = 0; i < (indexed ? 4u : 6u); ++i) {
      const float* corner = corners[indexed ? i : quad_indices[i]];
      positions.push_back(float3{corner[0], corner[1], z});
      normals.push_back(float3{0.0f, 0.0f, 1.0f});
      uvs.push_back(float2{corner[0], corner[1]});
    }
    if (indexed) {
      indices.assign(quad_indices, quad_indices + 6);
    }
  }

  MeshSourceSection getSection() const { return {positions, normals, {}, uvs, indices}; }
};

}  // namespace

int main() {
  "normals and tangents survive packing"_test = [] {
    std::vector<Vec3> directions = {Vec3::sAxisX(),
                                    -Vec3::sAxisX(),
                                    Vec3::sAxisY(),
                                    -Vec3::sAxisY(),
                                    Vec3::sAxisZ(),
                                    -Vec3::sAxisZ(),
                                    Vec3(1, 1, 1).Normalized(),
                                    Vec3(-1, 1, -1).Normalized()};
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);
    for (int i = 0; i < 1000; ++i) {
      directions.push_back(Vec3(distribution(rng), distribution(rng), distribution(rng))
                               .NormalizedOr(Vec3::sAxisX()));
    }

    for (const Vec3 direction : directions) {
      expect((unpackVertexNormal(packVertexNormal(direction)) - direction).Length() <
             5e-4f);
      for (const float sign : {-1.0f, 1.0f}) {
        const Vec4 tangent = unpackVertexTangent(packVertexTangent(direction, sign));
        expect((Vec3(tangent) - direction).Length() < 5e-4f);
        expect(tangent.GetW() == sign);
      }
    }
  };

  "sections are welded, optimized and split into meshlets"_test = [] {
    const Quad unindexed(0.0f, false);
    const Quad indexed(1.0f, true);
    const MeshSourceSection sections[] = {unindexed.getSection(), indexed.getSection()};
    MeshAsset asset;
    asset.name = "quads";
    expect(buildMeshAsset(sections, {}, asset));

    expect(asset.name == "quads");
    expect(asset.getVertexCount() == 8u);
    expect(asset.attributes.size() == 8u * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS);
    expect(asset.sectionCount == 2u && asset.sections.size() == 2u);
    for (uint32 s = 0; s < 2; ++s) {
      expect(asset.sections[s].indexCount == 6u);
      expect(asset.sections[s].materialSlot == s);
    }
    expect(asset.indices.size() == 12u);
    expect(asset.lods.size() == 1u);
    expect(asset.lods[0].meshletCount == asset.meshlets.size());
    expect(asset.meshletTriangles.size() % 4 == 0u);

    // Same triangles and winding in the index buffer and the meshlets.
    const auto index_triangles = getIndexTriangles(asset);
    expect(index_triangles == getMeshletTriangles(asset));
    for (uint32 s = 0; s < 2; ++s) {
      expect(index_triangles[s].size() == 2u);
      for (const Triangle& triangle : index_triangles[s]) {
        const Vec3 p0 = getPosition(asset, triangle[0]);
        const Vec3 normal = (getPosition(asset, triangle[1]) - p0)
                                .Cross(getPosition(asset, triangle[2]) - p0);
        expect(normal.GetZ() > 0.0f);
        expect(p0.GetZ() == float(s));
      }
    }

    for (uint32 vertex = 0; vertex < asset.getVertexCount(); ++vertex) {
      const Vec3 position = getPosition(asset, vertex);
      const uint32* attribute =
          asset.attributes.data() + vertex * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS;
      expect(unpackVertexNormal(attribute[0]).IsClose(Vec3::sAxisZ(), 1e-6f));
      // MikkTSpace: the tangent follows +u, the bitangent +v.
      const Vec4 tangent = unpackVertexTangent(attribute[1]);
      expect(Vec3(tangent).IsClose(Vec3::sAxisX(), 1e-6f));
      expect(tangent.GetW() == 1.0f);
      const float2 uv = getUv(asset, vertex);
      expect(uv[0] == position.GetX() && uv[1] == position.GetY());
    }

    expect(Vec3(asset.aabbMin) == Vec3::sZero());
    expect(Vec3(asset.aabbMax) == Vec3::sOne());
    const Vec3 center(asset.boundingSphere[0], asset.boundingSphere[1],
                      asset.boundingSphere[2]);
    for (const float3& position : asset.positions) {
      expect((Vec3(position) - center).Length() <= asset.boundingSphere[3] + 1e-5f);
    }
  };

  "missing streams are generated"_test = [] {
    const float3 positions[] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    const MeshSourceSection section{positions};
    MeshAsset asset;
    expect(buildMeshAsset({&section, 1}, {}, asset));
    expect(asset.getVertexCount() == 3u);
    for (uint32 vertex = 0; vertex < 3; ++vertex) {
      const uint32* attribute =
          asset.attributes.data() + vertex * GPU_SCENE_VERTEX_ATTRIBUTE_DWORDS;
      const Vec3 normal = unpackVertexNormal(attribute[0]);
      const Vec3 tangent(unpackVertexTangent(attribute[1]));
      expect(normal.IsClose(Vec3::sAxisZ(), 1e-6f));
      expect(std::abs(normal.Dot(tangent)) < 1e-3f);
      expect(std::abs(tangent.Length() - 1.0f) < 1e-3f);
    }
  };

  "invalid sections leave the output untouched"_test = [] {
    const Quad quad(0.0f, true);
    MeshAsset asset;
    asset.name = "untouched";
    asset.sectionCount = 77;

    MeshSourceSection section = quad.getSection();
    section.normals = section.normals.subspan(0, 2);
    expect(!buildMeshAsset({&section, 1}, {}, asset));

    const uint32 out_of_range[] = {0, 1, 4};
    section = quad.getSection();
    section.indices = out_of_range;
    expect(!buildMeshAsset({&section, 1}, {}, asset));

    section = quad.getSection();
    section.indices = section.indices.subspan(0, 4);
    expect(!buildMeshAsset({&section, 1}, {}, asset));

    expect(!buildMeshAsset({}, {}, asset));
    expect(asset.name == "untouched" && asset.sectionCount == 77u);
  };

  return 0;
}

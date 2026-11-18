#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "amaryl/engine/asset/asset_importer.h"
#include "boost/ut.hpp"

using namespace amaryl;
using namespace boost::ut;

namespace {

constexpr const char* kPath = "assets/models/test.gltf";

// Quad positions @0, normals @48, uvs @96 and uint16 indices @128.
std::vector<uint8> makeBuffer() {
  const float positions[] = {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0};
  const float normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
  const float uvs[] = {0, 1, 1, 1, 1, 0, 0, 0};
  const uint16 indices[] = {0, 1, 2, 0, 2, 3};
  std::vector<uint8> buffer(140);
  std::memcpy(buffer.data(), positions, sizeof(positions));
  std::memcpy(buffer.data() + 48, normals, sizeof(normals));
  std::memcpy(buffer.data() + 96, uvs, sizeof(uvs));
  std::memcpy(buffer.data() + 128, indices, sizeof(indices));
  return buffer;
}

std::string encodeBase64(const std::vector<uint8>& bytes) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  for (size_t i = 0; i < bytes.size(); i += 3) {
    const uint32 b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
    const uint32 b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
    const uint32 triple = (uint32{bytes[i]} << 16) | (b1 << 8) | b2;
    out += kAlphabet[(triple >> 18) & 63];
    out += kAlphabet[(triple >> 12) & 63];
    out += i + 1 < bytes.size() ? kAlphabet[(triple >> 6) & 63] : '=';
    out += i + 2 < bytes.size() ? kAlphabet[triple & 63] : '=';
  }
  return out;
}

// mesh 0: indexed quad, unindexed triangle and points, mesh 1: points only.
// The orphan node is not in the scene.
std::string makeGltf() {
  std::string json = R"json({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_unlit", "KHR_materials_emissive_strength"],
  "scene": 0,
  "scenes": [{"nodes": [0]}],
  "nodes": [
    {"name": "root", "translation": [1, 2, 3], "children": [1, 2]},
    {"name": "child_matrix", "mesh": 0,
     "matrix": [0, 0, -2, 0, 0, 2, 0, 0, 2, 0, 0, 0, 0, 0, 5, 1]},
    {"name": "child_trs", "mesh": 1, "rotation": [0, 0, 2, 0], "scale": [1, 1, 3],
     "children": [3]},
    {"name": "leaf", "mesh": 0},
    {"name": "orphan", "mesh": 0}
  ],
  "meshes": [
    {"name": "quad", "primitives": [
      {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3,
       "material": 0},
      {"attributes": {"POSITION": 4}},
      {"attributes": {"POSITION": 0}, "mode": 0}
    ]},
    {"name": "points", "primitives": [{"attributes": {"POSITION": 0}, "mode": 0}]}
  ],
  "materials": [
    {"name": "painted", "alphaMode": "MASK", "alphaCutoff": 0.3, "doubleSided": true,
     "pbrMetallicRoughness": {
       "baseColorFactor": [0.5, 0.25, 1, 0.75], "metallicFactor": 0.2,
       "roughnessFactor": 0.7, "baseColorTexture": {"index": 0},
       "metallicRoughnessTexture": {"index": 1}},
     "normalTexture": {"index": 0, "scale": 0.5},
     "emissiveTexture": {"index": 0}, "emissiveFactor": [1, 0.5, 0.25]},
    {"name": "unlit_blend", "alphaMode": "BLEND", "emissiveFactor": [0.5, 0.5, 0.5],
     "occlusionTexture": {"index": 1, "strength": 0.6},
     "extensions": {"KHR_materials_unlit": {},
                    "KHR_materials_emissive_strength": {"emissiveStrength": 4}}}
  ],
  "textures": [{"source": 0, "sampler": 0}, {"source": 1}],
  "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33648}],
  "images": [
    {"name": "albedo", "uri": "textures/albedo%20map.png"},
    {"uri": "data:image/png;base64,AAECAw=="}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]},
    {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5126, "count": 4, "type": "VEC2"},
    {"bufferView": 3, "componentType": 5123, "count": 6, "type": "SCALAR"},
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
     "min": [0, 0, 0], "max": [1, 1, 0]}
  ],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 48},
    {"buffer": 0, "byteOffset": 48, "byteLength": 48},
    {"buffer": 0, "byteOffset": 96, "byteLength": 32},
    {"buffer": 0, "byteOffset": 128, "byteLength": 12}
  ],
  "buffers": [{"byteLength": 140, "uri": "data:application/octet-stream;base64,@BUFFER@"}]
})json";
  const std::string_view marker = "@BUFFER@";
  json.replace(json.find(marker), marker.size(), encodeBase64(makeBuffer()));
  return json;
}

bool importJson(std::string_view json, const GltfImportOptions& options,
                GltfImportResult& out) {
  const Span<const uint8> bytes(reinterpret_cast<const uint8*>(json.data()), json.size());
  return importGltfFromMemory(bytes, kPath, options, out);
}

struct ImageCall {
  uint32 index = 0;
  std::string name;
  std::string path;
  std::vector<uint8> data;
  std::string mimeType;
  bool srgb = false;
};

bool isNear(float a, float b) { return std::abs(a - b) <= 1e-6f; }

template <std::size_t N>
bool isNear(const VecN<float, N>& a, const VecN<float, N>& b) {
  for (std::size_t i = 0; i < N; ++i) {
    if (!isNear(a[i], b[i])) {
      return false;
    }
  }
  return true;
}

bool isSampler(const GltfSamplerDesc& desc, uint32 mag, uint32 min, uint32 wrap_s,
               uint32 wrap_t) {
  return desc.magFilter == mag && desc.minFilter == min && desc.wrapS == wrap_s &&
         desc.wrapT == wrap_t;
}

}  // namespace

int main() {
  "gltf import converts meshes, materials and nodes"_test = [] {
    std::vector<ImageCall> images;
    std::vector<GltfSamplerDesc> samplers;
    GltfImportOptions options;
    options.loadImage = [&](const GltfImageDesc& desc) {
      images.push_back({desc.index, desc.name, desc.path,
                        std::vector<uint8>(desc.data.begin(), desc.data.end()),
                        desc.mimeType, desc.srgb});
      return TextureDescriptorIndex{static_cast<uint32>(9 + images.size())};
    };
    options.createSampler = [&](const GltfSamplerDesc& desc) {
      samplers.push_back(desc);
      return SamplerDescriptorIndex{static_cast<uint32>(19 + samplers.size())};
    };

    GltfImportResult result;
    expect(importJson(makeGltf(), options, result)) << fatal;

    // Every image is loaded once per color space, every sampler once.
    expect(images.size() == 3u) << fatal;
    const std::string albedo_path = "assets/models/textures/albedo map.png";
    expect(images[0].index == 0u && images[0].srgb && images[0].name == "albedo");
    expect(images[0].path == albedo_path && images[0].data.empty());
    expect(images[1].index == 1u && !images[1].srgb && images[1].path.empty());
    expect(images[1].data == std::vector<uint8>{0, 1, 2, 3});
    expect(images[1].mimeType == "image/png");
    expect(images[2].index == 0u && !images[2].srgb && images[2].path == albedo_path);
    expect(samplers.size() == 2u) << fatal;
    expect(isSampler(samplers[0], 9729, 9987, 33071, 33648));
    expect(isSampler(samplers[1], 0, 0, 10497, 10497));

    expect(result.materials.size() == 2u) << fatal;
    const MaterialAsset& painted = result.materials[0];
    expect(painted.name == "painted" && painted.shadingModel == EShadingModel::Lit);
    expect(painted.alphaMode == EAlphaMode::Mask && painted.doubleSided);
    expect(isNear(painted.alphaCutoff, 0.3f));
    expect(isNear(painted.baseColorFactor, float4{0.5f, 0.25f, 1.0f, 0.75f}));
    expect(isNear(painted.metallicFactor, 0.2f) && isNear(painted.roughnessFactor, 0.7f));
    expect(isNear(painted.emissiveFactor, float3{1.0f, 0.5f, 0.25f}));
    expect(isNear(painted.normalScale, 0.5f) && isNear(painted.occlusionStrength, 1.0f));
    expect(painted.baseColorTexture.value == 10u);
    expect(painted.metallicRoughnessTexture.value == 11u);
    expect(painted.normalTexture.value == 12u);
    expect(painted.occlusionTexture.value == 0u);
    expect(painted.emissiveTexture.value == 10u);
    expect(painted.sampler.value == 20u);

    const MaterialAsset& unlit = result.materials[1];
    expect(unlit.name == "unlit_blend" && unlit.shadingModel == EShadingModel::Unlit);
    expect(unlit.alphaMode == EAlphaMode::Blend && !unlit.doubleSided);
    expect(isNear(unlit.alphaCutoff, 0.5f));
    expect(isNear(unlit.baseColorFactor, float4{1.0f, 1.0f, 1.0f, 1.0f}));
    expect(isNear(unlit.emissiveFactor, float3{2.0f, 2.0f, 2.0f}));
    expect(isNear(unlit.occlusionStrength, 0.6f));
    expect(unlit.occlusionTexture.value == 11u && unlit.baseColorTexture.value == 0u);
    expect(unlit.sampler.value == 21u);

    // Points are skipped, a mesh without triangles fails.
    expect(result.meshes.size() == 2u) << fatal;
    const GltfMesh& quad = result.meshes[0];
    expect(quad.sectionMaterials == std::vector<int32>{0, -1});
    expect(quad.asset != nullptr) << fatal;
    expect(quad.asset->name == "quad" && quad.asset->sectionCount == 2u);
    expect(!quad.asset->lods.empty()) << fatal;
    const uint32 first_section = quad.asset->lods[0].sectionOffset;
    expect(quad.asset->sections[first_section].indexCount == 6u);
    expect(quad.asset->sections[first_section + 1].indexCount == 3u);
    expect(result.meshes[1].asset == nullptr &&
           result.meshes[1].sectionMaterials.empty());

    expect(result.nodes.size() == 4u) << fatal;
    const GltfNode& root = result.nodes[0];
    const GltfNode& child_matrix = result.nodes[1];
    const GltfNode& child_trs = result.nodes[2];
    const GltfNode& leaf = result.nodes[3];
    expect(root.name == "root" && root.parent == -1 && root.mesh == -1);
    expect(root.position.IsClose(Vec3(1, 2, 3)) && root.scale.IsClose(Vec3::sOne()));
    expect(root.rotation.IsClose(Quat::sIdentity()));

    expect(child_matrix.name == "child_matrix" && child_matrix.parent == 0);
    expect(child_matrix.mesh == 0);
    expect(child_matrix.position.IsClose(Vec3(0, 0, 5), 1e-10f));
    expect(child_matrix.scale.IsClose(Vec3(2, 2, 2), 1e-10f));
    expect((child_matrix.rotation * Vec3(1, 0, 0)).IsClose(Vec3(0, 0, -1), 1e-10f));
    expect((child_matrix.rotation * Vec3(0, 0, 1)).IsClose(Vec3(1, 0, 0), 1e-10f));

    expect(child_trs.name == "child_trs" && child_trs.parent == 0);
    expect(child_trs.mesh == -1);
    expect(child_trs.position.IsClose(Vec3::sZero()));
    expect(child_trs.scale.IsClose(Vec3(1, 1, 3)));
    expect((child_trs.rotation * Vec3(1, 0, 0)).IsClose(Vec3(-1, 0, 0), 1e-10f));
    expect((child_trs.rotation * Vec3(0, 1, 0)).IsClose(Vec3(0, -1, 0), 1e-10f));

    expect(leaf.name == "leaf" && leaf.parent == 2 && leaf.mesh == 0);
  };

  "gltf import without callbacks has no textures"_test = [] {
    GltfImportResult result;
    expect(importJson(makeGltf(), {}, result)) << fatal;
    expect(result.materials.size() == 2u) << fatal;
    for (const MaterialAsset& material : result.materials) {
      expect(material.baseColorTexture.value == 0u && material.normalTexture.value == 0u);
      expect(material.metallicRoughnessTexture.value == 0u);
      expect(material.occlusionTexture.value == 0u &&
             material.emissiveTexture.value == 0u);
      expect(material.sampler.value == 0u);
    }
    expect(result.meshes.size() == 2u && result.nodes.size() == 4u);
  };

  "gltf import rejects unsupported files"_test = [] {
    GltfImportResult result;
    result.nodes.push_back({.name = "sentinel"});
    const auto untouched = [&] {
      return result.nodes.size() == 1u && result.nodes[0].name == "sentinel";
    };

    for (const char* extension : {"KHR_draco_mesh_compression", "EXT_meshopt_compression",
                                  "KHR_meshopt_compression"}) {
      const std::string json = std::string(R"({"asset": {"version": "2.0"},)") +
                               R"("extensionsUsed": [")" + extension + R"("],)" +
                               R"("extensionsRequired": [")" + extension + R"("]})";
      expect(!importJson(json, {}, result)) << extension;
      expect(untouched());
    }
    expect(!importJson("{\"asset\": ", {}, result));
    expect(!importJson("", {}, result));
    expect(untouched());

    // Other required extensions are accepted, the result is replaced.
    const std::string supported =
        R"({"asset": {"version": "2.0"}, "extensionsUsed": ["KHR_materials_unlit"],
            "extensionsRequired": ["KHR_materials_unlit"]})";
    expect(importJson(supported, {}, result));
    expect(result.nodes.empty() && result.meshes.empty() && result.materials.empty());
  };

  return 0;
}

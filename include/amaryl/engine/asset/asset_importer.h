#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "amaryl/core/span.h"
#include "amaryl/engine/asset/material_asset.h"
#include "amaryl/engine/asset/mesh_asset.h"

namespace amaryl {

// An image used by the textures of the imported materials.
struct GltfImageDesc {
  // Index in the glTF images.
  uint32 index = 0;
  std::string name;
  // UTF-8 path of an external image, relative uris are resolved against the glTF file.
  std::string path;
  // Embedded image (GLB buffer view or data uri), only valid during the callback.
  Span<const uint8> data;
  std::string mimeType;
  // Base color and emissive textures are sRGB, the other ones are linear.
  bool srgb = false;
};

// glTF (OpenGL) sampler enums, the filters are 0 when undefined.
struct GltfSamplerDesc {
  uint32 magFilter = 0;
  uint32 minFilter = 0;
  uint32 wrapS = 10497;
  uint32 wrapT = 10497;
};

struct GltfImportOptions {
  MeshBuildOptions meshBuild{};
  // Called once per image and color space used by the materials, returns the descriptor
  // of the loaded texture. Every texture is 0 (none) without callback.
  std::function<TextureDescriptorIndex(const GltfImageDesc&)> loadImage;
  // Called once per sampler used by the materials, textures without sampler use the
  // default GltfSamplerDesc. Every sampler is 0 without callback.
  std::function<SamplerDescriptorIndex(const GltfSamplerDesc&)> createSampler;
};

struct GltfMesh {
  // Null when the mesh has no supported primitive.
  std::shared_ptr<const MeshAsset> asset;
  // Per section, index in `GltfImportResult::materials` or -1 for the default material.
  std::vector<int32> sectionMaterials;
};

struct GltfNode {
  std::string name;
  // Index in `GltfImportResult::nodes`, lower than the index of the node, -1 for roots.
  int32 parent = -1;
  Vec3 position = Vec3::sZero();
  Quat rotation = Quat::sIdentity();
  Vec3 scale = Vec3::sOne();
  // Index in `GltfImportResult::meshes`, -1 without mesh or when the mesh failed.
  int32 mesh = -1;
};

struct GltfImportResult {
  std::vector<GltfMesh> meshes;
  std::vector<MaterialAsset> materials;
  // Nodes of the default scene, parents before children.
  std::vector<GltfNode> nodes;
};

// Imports the static meshes, the materials and the node hierarchy of a .gltf or .glb
// file. Primitives become mesh sections and only TEXCOORD_0 is kept. Skins and morph
// targets are ignored (bind pose), non-triangle primitives are skipped. Returns false and
// leaves `out` untouched when the file cannot be loaded.
ENGINE_API bool importGltf(const char* path, const GltfImportOptions& options,
                           GltfImportResult& out);
// `path` resolves the relative uris of external buffers and images, it may be empty.
ENGINE_API bool importGltfFromMemory(Span<const uint8> data, const char* path,
                                     const GltfImportOptions& options,
                                     GltfImportResult& out);

}  // namespace amaryl

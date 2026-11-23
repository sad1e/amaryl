#pragma once

#include <functional>
#include <string>

#include "amaryl/core/span.h"
#include "amaryl/engine/asset/scene_asset.h"

namespace amaryl {

// An image used by the textures of the imported materials.
struct GltfImageDesc {
  // Index in the glTF images.
  uint32 index = 0;
  std::string name;
  // UTF-8 path of an external image, relative uris are resolved against the glTF file
  // (a virtual path when the glTF path is virtual).
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
  // When disabled, the camera nodes are skipped with their whole subtree.
  bool importCameras = true;
  // KHR_lights_punctual. When disabled, the light nodes are skipped with their whole
  // subtree.
  bool importLights = true;
};

struct GltfImportResult {
  ModelAsset model;
  // Nodes of the default scene, the meshes index `model.meshes`.
  SceneAsset scene;
};

// Imports the static meshes, the materials and the node hierarchy of a .gltf or .glb
// file. Primitives become mesh sections and only TEXCOORD_0 is kept. Skins and morph
// targets are ignored (bind pose), non-triangle primitives are skipped. Returns false and
// leaves `out` untouched when the file cannot be loaded.
// `path` is a virtual path ("project://models/a.gltf") or a native UTF-8 path, the file
// and its external buffers are read through the virtual file system.
ENGINE_API bool importGltf(const char* path, const GltfImportOptions& options,
                           GltfImportResult& out);
// `path` resolves the relative uris of external buffers and images, it may be empty.
ENGINE_API bool importGltfFromMemory(Span<const uint8> data, const char* path,
                                     const GltfImportOptions& options,
                                     GltfImportResult& out);

// Imports the meshes and materials as standalone assets for the editor, the scene
// structure is dropped and every mesh node becomes an asset.
// TODO: not implemented yet, always returns false.
ENGINE_API bool importGltfAssets(const char* path, const GltfImportOptions& options,
                                 ModelAsset& out);

}  // namespace amaryl

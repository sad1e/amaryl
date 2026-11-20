#pragma once

#include <string>

#include "amaryl/core/math/math.h"
#include "amaryl/engine/render/gpu_scene.shared.h"

namespace amaryl {

enum class EShadingModel : uint8 {
  Lit,
  Unlit,
  Count,
};

enum class EAlphaMode : uint8 {
  Opaque,
  // Discards fragments below `alphaCutoff`.
  Mask,
  Blend,
  Count,
};

// glTF metallic-roughness material. Textures are bindless descriptor indices, 0 means no
// texture.
struct MaterialAsset {
  std::string name;

  EShadingModel shadingModel = EShadingModel::Lit;
  EAlphaMode alphaMode = EAlphaMode::Opaque;
  bool doubleSided = false;
  float alphaCutoff = 0.5f;

  float4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
  float3 emissiveFactor{0.0f, 0.0f, 0.0f};
  float metallicFactor = 1.0f;
  float roughnessFactor = 1.0f;
  float normalScale = 1.0f;
  float occlusionStrength = 1.0f;

  TextureDescriptorIndex baseColorTexture{};
  TextureDescriptorIndex metallicRoughnessTexture{};
  TextureDescriptorIndex normalTexture{};
  TextureDescriptorIndex occlusionTexture{};
  TextureDescriptorIndex emissiveTexture{};
  // Shared by every texture of the material.
  SamplerDescriptorIndex sampler{};
};

inline constexpr uint32 kMaterialPipelineBinCount =
    static_cast<uint32>(EShadingModel::Count) * static_cast<uint32>(EAlphaMode::Count) *
    2;

// Materials of the same bin are drawn with the same pipeline.
constexpr uint32 getMaterialPipelineBin(EShadingModel shading_model,
                                        EAlphaMode alpha_mode, bool double_sided) {
  return (static_cast<uint32>(shading_model) * static_cast<uint32>(EAlphaMode::Count) +
          static_cast<uint32>(alpha_mode)) *
             2 +
         (double_sided ? 1 : 0);
}

inline GpuMaterial toGpuMaterial(const MaterialAsset& material) {
  GpuMaterial gpu{};
  gpu.baseColorFactor = material.baseColorFactor;
  gpu.emissiveAlphaCutoff = float4{material.emissiveFactor[0], material.emissiveFactor[1],
                                   material.emissiveFactor[2], material.alphaCutoff};
  gpu.metallicFactor = material.metallicFactor;
  gpu.roughnessFactor = material.roughnessFactor;
  gpu.normalScale = material.normalScale;
  gpu.occlusionStrength = material.occlusionStrength;
  gpu.baseColorTexture = material.baseColorTexture;
  gpu.metallicRoughnessTexture = material.metallicRoughnessTexture;
  gpu.normalTexture = material.normalTexture;
  gpu.occlusionTexture = material.occlusionTexture;
  gpu.emissiveTexture = material.emissiveTexture;
  gpu.sampler = material.sampler;
  gpu.pipelineBin = getMaterialPipelineBin(material.shadingModel, material.alphaMode,
                                           material.doubleSided);
  gpu.flags =
      (material.alphaMode == EAlphaMode::Mask ? GPU_MATERIAL_FLAG_ALPHA_MASK : 0u) |
      (material.alphaMode == EAlphaMode::Blend ? GPU_MATERIAL_FLAG_ALPHA_BLEND : 0u) |
      (material.doubleSided ? GPU_MATERIAL_FLAG_DOUBLE_SIDED : 0u);
  return gpu;
}

}  // namespace amaryl

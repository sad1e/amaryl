#pragma once

#define NRHI_GLOBAL_DESCRIPTOR_SET_INDEX 0
#define NRHI_SAMPLED_TEXTURE_BINDING 0
#define NRHI_SAMPLER_BINDING 1
#define NRHI_STORAGE_TEXTURE_BINDING 2
#define NRHI_YUV_TEXTURE_BINDING 3
#define NRHI_ACCELERATION_STRUCTURE_BINDING 4

#if defined(__SLANG__)
#define NRHI_DESCRIPTOR_INDEX_TYPE uint
#define NRHI_DESCRIPTOR_INDEX_INIT
#else

#include "amaryl/nrhi/config.h"

namespace amaryl {

#define NRHI_DESCRIPTOR_INDEX_TYPE uint32
#define NRHI_DESCRIPTOR_INDEX_INIT \
  {                                \
  }
#endif

struct TextureDescriptorIndex {
  NRHI_DESCRIPTOR_INDEX_TYPE value NRHI_DESCRIPTOR_INDEX_INIT;
};

struct SamplerDescriptorIndex {
  NRHI_DESCRIPTOR_INDEX_TYPE value NRHI_DESCRIPTOR_INDEX_INIT;
};

#if defined(__SLANG__)
[[vk::binding(NRHI_SAMPLED_TEXTURE_BINDING, NRHI_GLOBAL_DESCRIPTOR_SET_INDEX)]]
Texture2D<float4> NRHISampledTexture2DFloat4Descriptors[];

[[vk::binding(NRHI_SAMPLER_BINDING, NRHI_GLOBAL_DESCRIPTOR_SET_INDEX)]]
SamplerState NRHISamplerDescriptors[];

[[vk::binding(NRHI_STORAGE_TEXTURE_BINDING, NRHI_GLOBAL_DESCRIPTOR_SET_INDEX)]]
RWTexture2D<float4> NRHIStorageTexture2DFloat4Descriptors[];

[ForceInline] Texture2D<float4> NRHISampledTexture2DFloat4(
    TextureDescriptorIndex descriptor) {
  return NRHISampledTexture2DFloat4Descriptors[NonUniformResourceIndex(descriptor.value)];
}

    [ForceInline] SamplerState NRHISampler(SamplerDescriptorIndex descriptor) {
  return NRHISamplerDescriptors[NonUniformResourceIndex(descriptor.value)];
}

[ForceInline] RWTexture2D<float4> NRHIStorageTexture2DFloat4(
    TextureDescriptorIndex descriptor) {
  return NRHIStorageTexture2DFloat4Descriptors[NonUniformResourceIndex(descriptor.value)];
}
#else
inline constexpr uint32 GLOBAL_DESCRIPTOR_SET_INDEX = NRHI_GLOBAL_DESCRIPTOR_SET_INDEX;

enum class EGlobalDescriptorBinding : uint32 {
  SampledTextures = NRHI_SAMPLED_TEXTURE_BINDING,
  Samplers = NRHI_SAMPLER_BINDING,
  StorageTextures = NRHI_STORAGE_TEXTURE_BINDING,
  YuvTextures = NRHI_YUV_TEXTURE_BINDING,
  AccelerationStructures = NRHI_ACCELERATION_STRUCTURE_BINDING,
  Count,
};

static_assert(sizeof(TextureDescriptorIndex) == sizeof(uint32));
static_assert(alignof(TextureDescriptorIndex) == alignof(uint32));
static_assert(sizeof(SamplerDescriptorIndex) == sizeof(uint32));
static_assert(alignof(SamplerDescriptorIndex) == alignof(uint32));
static_assert(static_cast<uint32>(EGlobalDescriptorBinding::Count) == 5u);
}
#endif

#undef NRHI_DESCRIPTOR_INDEX_INIT
#undef NRHI_DESCRIPTOR_INDEX_TYPE
#undef NRHI_ACCELERATION_STRUCTURE_BINDING
#undef NRHI_YUV_TEXTURE_BINDING
#undef NRHI_STORAGE_TEXTURE_BINDING
#undef NRHI_SAMPLER_BINDING
#undef NRHI_SAMPLED_TEXTURE_BINDING
#undef NRHI_GLOBAL_DESCRIPTOR_SET_INDEX

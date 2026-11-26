#pragma once

#include <string>
#include <unordered_map>

#include "amaryl/engine/asset/asset_importer.h"
#include "amaryl/engine/render/material_registry.h"

namespace amaryl {

// Owns the sampled textures and samplers referenced by the materials through bindless
// descriptor indices. Textures are deduplicated by key and color space and destroyed
// when their last reference is released, samplers are deduplicated by state and live as
// long as the registry. Register it with `MaterialRegistry::setTextureReferences()` so
// materials keep their textures alive.
struct ENGINE_API TextureRegistry final : ITextureReferences {
  // `rhi` must outlive the registry.
  explicit TextureRegistry(IContext* rhi);
  ~TextureRegistry() override;

  NO_COPYABLE(TextureRegistry);

  // Returns the texture of (`key`, `srgb`) with one more reference, decoding `encoded`
  // when it is not loaded yet. Returns 0 (no texture) when the image cannot be decoded.
  TextureDescriptorIndex acquire(const std::string& key, Span<const uint8> encoded,
                                 bool srgb);
  // Same as acquire() with an image file read through the virtual file system, `path`
  // is the key.
  TextureDescriptorIndex acquireFile(const std::string& path, bool srgb);

  void addRef(TextureDescriptorIndex texture) override;
  void release(TextureDescriptorIndex texture) override;
  // 0 for unknown textures.
  uint32 getRefCount(TextureDescriptorIndex texture) const;
  uint32 getCount() const { return static_cast<uint32>(entries_.size()); }

  SamplerDescriptorIndex getSampler(const GltfSamplerDesc& desc);

 private:
  struct Entry {
    Holder<TextureHandle> texture;
    std::string key;
    uint32 refCount{0};
  };

  IContext* rhi_{nullptr};
  // Key + color space -> descriptor index.
  std::unordered_map<std::string, uint32> by_key_;
  // Descriptor index -> texture.
  std::unordered_map<uint32, Entry> entries_;
  std::unordered_map<uint64, Holder<SamplerHandle>> samplers_;
};

}  // namespace amaryl

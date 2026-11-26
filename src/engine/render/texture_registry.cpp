#include "amaryl/engine/render/texture_registry.h"

#include <algorithm>
#include <bit>
#include <vector>

#include "amaryl/core/filesystem/vfs.h"
#include "amaryl/core/logger.h"
#include "amaryl/engine/asset/image_decoder.h"

namespace amaryl {

namespace {

// glTF (OpenGL) sampler enums.
constexpr uint32 kGlNearest = 9728;
constexpr uint32 kGlLinear = 9729;
constexpr uint32 kGlNearestMipmapNearest = 9984;
constexpr uint32 kGlLinearMipmapNearest = 9985;
constexpr uint32 kGlNearestMipmapLinear = 9986;
constexpr uint32 kGlClampToEdge = 33071;
constexpr uint32 kGlMirroredRepeat = 33648;

constexpr uint8 kMaxAnisotropy = 8;

ESamplerFilter toFilter(uint32 gl_filter) {
  switch (gl_filter) {
    case kGlNearest:
    case kGlNearestMipmapNearest:
    case kGlNearestMipmapLinear:
      return ESamplerFilter::Nearest;
    default:
      return ESamplerFilter::Linear;
  }
}

ESamplerMip toMip(uint32 gl_min_filter) {
  switch (gl_min_filter) {
    case kGlNearest:
    case kGlLinear:
      return ESamplerMip::Disabled;
    case kGlNearestMipmapNearest:
    case kGlLinearMipmapNearest:
      return ESamplerMip::Nearest;
    default:
      // Undefined filters get trilinear filtering.
      return ESamplerMip::Linear;
  }
}

ESamplerWrap toWrap(uint32 gl_wrap) {
  switch (gl_wrap) {
    case kGlClampToEdge:
      return ESamplerWrap::Clamp;
    case kGlMirroredRepeat:
      return ESamplerWrap::MirrorRepeat;
    default:
      return ESamplerWrap::Repeat;
  }
}

uint32 getMipCount(uint32 width, uint32 height) {
  return static_cast<uint32>(std::bit_width(std::max(width, height)));
}

}  // namespace

TextureRegistry::TextureRegistry(IContext* rhi) : rhi_(rhi) {
  ASSERT(rhi_ != nullptr);
}

TextureRegistry::~TextureRegistry() {
  if (!entries_.empty()) {
    ALOGW("TextureRegistry: %u textures still referenced at destruction",
          static_cast<uint32>(entries_.size()));
  }
}

TextureDescriptorIndex TextureRegistry::acquire(const std::string& key,
                                                Span<const uint8> encoded, bool srgb) {
  std::string full_key = key;
  full_key += srgb ? "|srgb" : "|linear";
  if (const auto it = by_key_.find(full_key); it != by_key_.end()) {
    ++entries_.at(it->second).refCount;
    return TextureDescriptorIndex{it->second};
  }

  DecodedImage image;
  if (!decodeImageRGBA8(encoded, image)) {
    ALOGW("TextureRegistry: cannot decode '%s'", key.c_str());
    return {};
  }

  Result result;
  Holder<TextureHandle> texture = rhi_->createTexture(
    {
      .type = ETextureType::Texture2D,
      .format = srgb ? EFormat::RGBA_SRGB8 : EFormat::RGBA_UN8,
      .dimensions = {.width = image.width, .height = image.height, .depth = 1},
      .usage = ETextureUsageBits::Sampled,
      .numMipLevels = getMipCount(image.width, image.height),
      .data = image.pixels.data(),
      .generateMipmaps = true,
      .debugName = full_key.c_str(),
    },
    full_key.c_str(), &result);
  if (!result.isOk() || !texture.valid()) {
    ALOGW("TextureRegistry: cannot create '%s'", key.c_str());
    return {};
  }

  const uint32 index = rhi_->getTextureDescriptorIndex(texture).value;
  ASSERT(index != 0 && !entries_.contains(index));
  by_key_.emplace(full_key, index);
  entries_.emplace(index, Entry{std::move(texture), std::move(full_key), 1});
  return TextureDescriptorIndex{index};
}

TextureDescriptorIndex TextureRegistry::acquireFile(const std::string& path, bool srgb) {
  std::vector<uint8> bytes;
  if (!GetVirtualFileSystem().readFile(path, bytes)) {
    ALOGW("TextureRegistry: cannot read '%s'", path.c_str());
    return {};
  }
  return acquire(path, bytes, srgb);
}

void TextureRegistry::addRef(TextureDescriptorIndex texture) {
  const auto it = entries_.find(texture.value);
  ASSERT(it != entries_.end(), "Unknown texture");
  if (it != entries_.end()) {
    ++it->second.refCount;
  }
}

void TextureRegistry::release(TextureDescriptorIndex texture) {
  const auto it = entries_.find(texture.value);
  ASSERT(it != entries_.end(), "Unknown texture");
  if (it == entries_.end()) {
    return;
  }
  ASSERT(it->second.refCount > 0);
  if (--it->second.refCount == 0) {
    by_key_.erase(it->second.key);
    // The RHI defers the destruction until the GPU is done with the texture.
    entries_.erase(it);
  }
}

uint32 TextureRegistry::getRefCount(TextureDescriptorIndex texture) const {
  const auto it = entries_.find(texture.value);
  return it != entries_.end() ? it->second.refCount : 0;
}

SamplerDescriptorIndex TextureRegistry::getSampler(const GltfSamplerDesc& desc) {
  // The GL enums fit in 16 bits.
  const uint64 key = uint64{desc.magFilter & 0xffffu} |
                     (uint64{desc.minFilter & 0xffffu} << 16) |
                     (uint64{desc.wrapS & 0xffffu} << 32) |
                     (uint64{desc.wrapT & 0xffffu} << 48);
  auto it = samplers_.find(key);
  if (it == samplers_.end()) {
    Result result;
    Holder<SamplerHandle> sampler = rhi_->createSampler(
      {
        .minFilter = toFilter(desc.minFilter),
        .magFilter = toFilter(desc.magFilter),
        .mipMap = toMip(desc.minFilter),
        .wrapU = toWrap(desc.wrapS),
        .wrapV = toWrap(desc.wrapT),
        .maxAnisotropic = kMaxAnisotropy,
        .debugName = "TextureRegistry sampler",
      },
      &result);
    if (!result.isOk() || !sampler.valid()) {
      ALOGW("TextureRegistry: cannot create a sampler");
      return {};
    }
    it = samplers_.emplace(key, std::move(sampler)).first;
  }
  return rhi_->getSamplerDescriptorIndex(it->second);
}

}  // namespace amaryl

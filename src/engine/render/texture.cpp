#include "amaryl/engine/render/texture.h"

#include "amaryl/nrhi/definitions.h"

namespace amaryl {

TextureHandle RenderTargetPool::acquire(const TextureDesc& desc, const char* debug_name) {
  for (auto& rt : targets_) {
    if (!rt->inUse && compatible(rt->desc, desc)) {
      rt->inUse = true;
      return rt->texture;
    }
  }

  ASSERT(ctx_ != nullptr, "RenderTargetPool has no context");
  if (!ctx_) {
    return {};
  }

  auto rt = std::make_unique<RenderTarget>();
  rt->debugName = debug_name ? debug_name : "RenderTarget";
  rt->desc = desc;
  rt->desc.data = nullptr;
  rt->desc.dataNumMipLevels = 1;
  rt->desc.generateMipmaps = false;
  rt->desc.debugName = rt->debugName.c_str();

  Result res;
  rt->texture = ctx_->createTexture(rt->desc, rt->debugName.c_str(), &res);
  if (!res.isOk() || rt->texture.empty()) {
    return {};
  }

  rt->inUse = true;
  const TextureHandle handle = rt->texture;
  targets_.push_back(std::move(rt));

  return handle;
}

void RenderTargetPool::release(TextureHandle handle) {
  if (handle.empty()) {
    return;
  }

  RenderTarget* rt = find(handle);
  ASSERT(rt != nullptr, TEXT("Releasing a render target not owned by this pool"));
  ASSERT(rt == nullptr || rt->inUse, TEXT("Render target released more than once"));
  if (rt) {
    rt->inUse = false;
  }
}

const RenderTarget* RenderTargetPool::get(TextureHandle handle) const {
  return find(handle);
}

void RenderTargetPool::releaseAll() { targets_.clear(); }

bool RenderTargetPool::compatible(const TextureDesc& a, const TextureDesc& b) {
  return a.type == b.type && a.format == b.format && a.dimensions == b.dimensions &&
         a.numLayers == b.numLayers && a.numSamples == b.numSamples &&
         a.usage == b.usage && a.numMipLevels == b.numMipLevels &&
         a.storage == b.storage && a.components.r == b.components.r &&
         a.components.g == b.components.g && a.components.b == b.components.b &&
         a.components.a == b.components.a;
}

RenderTarget* RenderTargetPool::find(TextureHandle handle) const {
  if (handle.empty()) {
    return nullptr;
  }

  for (const auto& rt : targets_) {
    if (static_cast<TextureHandle>(rt->texture) == handle) {
      return rt.get();
    }
  }
  return nullptr;
}

}  // namespace amaryl

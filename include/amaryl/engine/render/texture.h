#pragma once

#include <memory>
#include <string>
#include <vector>

#include "amaryl/engine/common.h"
#include "amaryl/nrhi/definitions.h"
#include "amaryl/nrhi/nrhi.h"

namespace amaryl {

struct RenderTarget {
  Holder<TextureHandle> texture;
  // `desc.debugName` points to `debugName`, `desc.data` is always null.
  TextureDesc desc;

  std::string debugName;
  // is the texture be used.
  bool inUse{false};
};

// Owns the render target textures and hands out non-owning handles. A released render
// target can be reused by a later `acquire()` with a compatible desc. `ctx` must outlive
// the pool.
struct ENGINE_API RenderTargetPool final {
  explicit RenderTargetPool(IContext* ctx) : ctx_(ctx) {}

  NO_COPYABLE(RenderTargetPool);

  // Returns an empty handle when the texture creation failed.
  TextureHandle acquire(const TextureDesc& desc, const char* debug_name);

  // Returns the render target to the pool, the texture stays alive for reuse.
  void release(TextureHandle handle);

  // Returns nullptr when the handle is not owned by this pool.
  const RenderTarget* get(TextureHandle handle) const;

  uint32 getCount() const { return static_cast<uint32>(targets_.size()); }

  // Destroys all render targets, the handles returned by `acquire()` become invalid.
  void releaseAll();

 private:
  static bool compatible(const TextureDesc& a, const TextureDesc& b);

  RenderTarget* find(TextureHandle handle) const;

  IContext* ctx_{nullptr};
  // `unique_ptr` keeps `RenderTarget::debugName` stable for `desc.debugName`.
  std::vector<std::unique_ptr<RenderTarget>> targets_;
};

}  // namespace amaryl

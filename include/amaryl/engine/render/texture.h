#pragma once

#include "amaryl/engine/common.h"
#include "amaryl/nrhi/definitions.h"

namespace amaryl {

struct RenderTargetPool {
  TextureHandle createRenderTarget(const TextureDesc& desc);
  void get(TextureHandle handle);
  void releaseAll();

 private:
  std::vector<TextureDesc> descs_;
};

struct RenderTarget {
  TextureHandle texture_handle;
  RenderTarget();

  TextureDesc textureDesc;
  char debugName[kMaxDebugName];
};

}  // namespace amaryl

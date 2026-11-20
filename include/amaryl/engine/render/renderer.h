#pragma once

#include "amaryl/engine/render/texture.h"
#include "amaryl/nrhi/nrhi.h"
#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

struct Engine;

struct SceneRenderTargets {
  // Format: RGBA8_SRGB. Content: RGB, Albedo; A: Metallic
  RenderTarget GBufferA;
  // Format: R10G10B10A2_UNORM. Content: RG, Normal(Encoding); B, Roughenss; A: Misc
  RenderTarget GBufferB;
  // Format: RGBA16_SNORM. Content: RG, Velocity.xy; B, AO; A, SSS Mask
  RenderTarget GBufferC;
  // Format: R11G11B10
  RenderTarget GBufferD;
  // Format: D32S8
  RenderTarget DepthTexture;

  void init(uint32 width, uint32 height);
};

struct Renderer {
  void init(Engine* engine);
  void renderGBuffersPass();
  void renderPostProcessPass();
};

}  // namespace amaryl

#pragma once

#include "amaryl/engine/render/texture.h"
#include "amaryl/nrhi/nrhi.h"
#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

struct Engine;

struct SceneRenderTargets {
  // Format: RGBA8_SRGB. Content: RGB, Albedo; A: Metallic
  RenderTarget gbufferA;
  // Format: R10G10B10A2_UNORM. Content: RG, Normal(Encoding); B, Roughenss; A: Misc
  RenderTarget gbufferB;
  // Format: RGBA16_SNORM. Content: RG, Velocity.xy; B, AO; A, SSS Mask
  RenderTarget gbufferC;
  // Format: R11G11B10
  RenderTarget gbufferD;
  // Format: D32S8
  RenderTarget depthTexture;

  void init(uint32 width, uint32 height);
};

struct DeferredRenderer {
  void init(Engine* engine);

  void beginFrame();
  void endFrame();

  void renderGBuffersPass();
  void renderPostProcessPass();

 private:
  SceneRenderTargets render_targets_;
};

}  // namespace amaryl

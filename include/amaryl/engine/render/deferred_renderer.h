#pragma once

#include "amaryl/engine/render/texture.h"
#include "amaryl/nrhi/nrhi.h"

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

// Per frame: beginFrame(), the passes, endFrame(). The passes and endFrame() must only
// be called when beginFrame() succeeded.
struct ENGINE_API DeferredRenderer final {
  // `engine` must outlive the renderer and own a valid RHI.
  void init(Engine* engine);
  // Releases the GPU resources, the GPU must be idle.
  void shutdown();

  // Acquires the swapchain image and the command buffer. Returns false when no swapchain
  // image is available, the swapchain then has to be recreated.
  bool beginFrame();
  // Submits the command buffer and presents the swapchain image.
  void endFrame();

  void renderGBuffersPass();
  void renderPostProcessPass();

 private:
  IContext* rhi_{nullptr};
  SceneRenderTargets render_targets_;

  // Valid between beginFrame() and endFrame().
  ICommandBuffer* commands_{nullptr};
  TextureHandle swapchain_texture_{};
};

}  // namespace amaryl

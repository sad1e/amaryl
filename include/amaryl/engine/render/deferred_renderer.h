#pragma once

#include "amaryl/engine/render/gpu_scene_buffers.h"
#include "amaryl/engine/render/imgui_renderer.h"
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

  // Records the GPU scene uploads of this frame, before the passes reading the scene.
  void uploadScene(const GpuSceneUploads& uploads);
  void renderGBuffersPass();
  void renderPostProcessPass();
  // Draws the ImGui draw data over the swapchain image, after the post process pass.
  // Does nothing when `draw_data` is null or the ImGui renderer is not initialized.
  void renderImGuiPass(ImDrawData* draw_data);

  ImGuiRenderer& getImGuiRenderer() { return imgui_renderer_; }
  const GpuSceneBuffers& getGpuScene() const { return gpu_scene_; }

 private:
  // Creates the full-screen "debug lighting only" pipeline together with the small
  // bindless test texture, sampler and device-address test buffer used to verify shader
  // loading, bindless sampling and push-constant address passing.
  void initDebugLightingOnly();
  void shutdownDebugLightingOnly();

  IContext* rhi_{nullptr};
  SceneRenderTargets render_targets_;

  // Valid between beginFrame() and endFrame().
  ICommandBuffer* commands_{nullptr};
  TextureHandle swapchain_texture_{};

  // Debug lighting-only full-screen pass.
  Holder<ShaderModuleHandle> debug_vert_;
  Holder<ShaderModuleHandle> debug_frag_;
  Holder<RenderPipelineHandle> debug_pipeline_;
  Holder<TextureHandle> debug_texture_;
  Holder<SamplerHandle> debug_sampler_;
  Holder<BufferHandle> debug_buffer_;

  GpuSceneBuffers gpu_scene_;
  ImGuiRenderer imgui_renderer_;
};

}  // namespace amaryl

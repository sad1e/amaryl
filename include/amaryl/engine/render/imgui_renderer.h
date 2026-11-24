#pragma once

#include <imgui.h>

#include <vector>

#include "amaryl/engine/common.h"
#include "amaryl/nrhi/nrhi.h"

namespace amaryl {

// Exposed through `ImGui::GetPlatformIO().Renderer_RenderState` while ImDrawCmd user
// callbacks run.
struct ImGuiRenderState {
  ICommandBuffer* commands{nullptr};
};

// Dear ImGui renderer backend on top of nrhi.
//
// - Bindless: ImTextureID is a TextureDescriptorIndex (see `toTextureID()`). Textures
//   owned by ImGui (font atlas) follow ImGuiBackendFlags_RendererHasTextures.
// - Vertex pulling: ImDrawVert is read from a device address passed in push constants.
// - One host-visible vertex/index buffer pair per swapchain image.
// - Main viewport only, ImGuiConfigFlags_ViewportsEnable is not supported.
struct ENGINE_API ImGuiRenderer final {
  ImGuiRenderer() = default;
  ~ImGuiRenderer();

  NO_COPYABLE_AND_MOVEABLE(ImGuiRenderer);

  // Registers the renderer backend into the current ImGui context and creates the
  // pipeline. The ImGui context must outlive the renderer.
  bool init(IContext* rhi);
  // Releases the ImGui textures and unregisters the backend. Must be called before the
  // ImGui context is destroyed. Safe to call twice.
  void shutdown();
  bool isInitialized() const { return rhi_ != nullptr; }

  // Records `draw_data` into a render pass that keeps the content of `target`. Call at
  // most once per frame, after the swapchain image was acquired. `sampled_textures` are
  // the engine textures shown by ImGui::Image() this frame, they are transitioned to
  // shader read-only and kept alive until the frame completes.
  void render(ICommandBuffer& commands, TextureHandle target, ImDrawData* draw_data,
              Span<TextureHandle> sampled_textures = {});

  // ImTextureID of an engine texture for ImGui::Image(), the texture must be sampled.
  static ImTextureID toTextureID(IContext* rhi, TextureHandle texture);

 private:
  struct FrameBuffers {
    Holder<BufferHandle> vertex;
    Holder<BufferHandle> index;
    size_t vertex_capacity{0};
    size_t index_capacity{0};
  };

  void updateTexture(ImTextureData* tex);
  void destroyTexture(ImTextureData* tex);
  bool reserveFrameBuffers(FrameBuffers& frame, size_t vertex_bytes, size_t index_bytes);

  IContext* rhi_{nullptr};
  ImGuiContext* context_{nullptr};

  Holder<ShaderModuleHandle> vert_;
  Holder<ShaderModuleHandle> frag_;
  Holder<RenderPipelineHandle> pipeline_;
  Holder<SamplerHandle> sampler_;
  // The swapchain is sRGB: vertex colors are converted to linear in the shader.
  bool linear_output_{false};

  std::vector<FrameBuffers> frames_;
  std::vector<TextureHandle> sampled_textures_;
  std::vector<uint8> upload_scratch_;
};

}  // namespace amaryl

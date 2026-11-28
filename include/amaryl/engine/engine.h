#pragma once

#include <memory>

#include "amaryl/engine/common.h"
#include "amaryl/engine/render/deferred_renderer.h"
#include "amaryl/engine/render/texture_registry.h"
#include "amaryl/engine/scene/model_library.h"
#include "amaryl/engine/scene/world.h"
#include "amaryl/nrhi/nrhi.h"

namespace amaryl {

struct ENGINE_API Engine final {
  Engine() = default;
  ~Engine();

  NO_COPYABLE_AND_MOVEABLE(Engine);

  // `width` and `height` are the drawable size of `window` in pixels. engine:// must be
  // mounted in the virtual file system.
  bool init(SDL_Window* window, uint32 width, uint32 height);
  // Waits for the GPU, then releases the renderer and the RHI. Safe to call twice.
  void shutdown();

  // Recreates the swapchain, a zero size is ignored (minimized window).
  void processResize(uint32 width, uint32 height);

  // Registers the nrhi ImGui renderer backend into the current ImGui context.
  bool initImGui();
  // Must be called before the ImGui context is destroyed. Safe to call twice.
  void shutdownImGui();

  // Game update of one frame.
  void update(float delta_time);
  // Renders and presents one frame, `imgui_draw_data` (ImGui::GetDrawData()) is drawn
  // over the scene when not null. Returns false on unrecoverable errors, a frame skipped
  // because the swapchain is out of date is not an error.
  bool render(ImDrawData* imgui_draw_data = nullptr);

  IContext* getRHI() const { return rhi_.get(); }
  World& getWorld() { return world_; }
  // Valid between init() and shutdown().
  TextureRegistry& getTextures() { return *textures_; }
  ModelLibrary& getModels() { return *models_; }
  const DeferredRenderer& getRenderer() const { return renderer_; }

  void LoadTestScene();

 private:
  std::unique_ptr<IContext> rhi_;
  DeferredRenderer renderer_;
  World world_;
  std::unique_ptr<TextureRegistry> textures_;
  std::unique_ptr<ModelLibrary> models_;
  // Reused every frame to keep the allocations.
  GpuSceneUploads uploads_;
  uint32 width_{0};
  uint32 height_{0};
};

ENGINE_API extern Engine* kEngine;

// Creates kEngine, returns false and leaves kEngine null on failure.
ENGINE_API bool InitializeEngine(SDL_Window* window, uint32 width, uint32 height);
ENGINE_API void ShutdownEngine();

}  // namespace amaryl

#pragma once

#include <memory>

#include "amaryl/engine/common.h"
#include "amaryl/engine/render/deferred_renderer.h"
#include "amaryl/engine/scene/world.h"
#include "amaryl/nrhi/nrhi.h"

namespace amaryl {

struct ENGINE_API Engine final {
  Engine() = default;
  ~Engine();

  NO_COPYABLE_AND_MOVEABLE(Engine);

  // `width` and `height` are the drawable size of `window` in pixels.
  bool init(SDL_Window* window, uint32 width, uint32 height);
  // Waits for the GPU, then releases the renderer and the RHI. Safe to call twice.
  void shutdown();

  // Recreates the swapchain, a zero size is ignored (minimized window).
  void processResize(uint32 width, uint32 height);

  // Game update of one frame.
  void update(float delta_time);
  // Renders and presents one frame. Returns false on unrecoverable errors, a frame
  // skipped because the swapchain is out of date is not an error.
  bool render();

  IContext* getRHI() const { return rhi_.get(); }
  World& getWorld() { return world_; }

 private:
  std::unique_ptr<IContext> rhi_;
  DeferredRenderer renderer_;
  World world_;
  uint32 width_{0};
  uint32 height_{0};
};

ENGINE_API extern Engine* kEngine;

// Creates kEngine, returns false and leaves kEngine null on failure.
ENGINE_API bool InitializeEngine(SDL_Window* window, uint32 width, uint32 height);
ENGINE_API void ShutdownEngine();

}  // namespace amaryl

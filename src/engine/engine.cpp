#include "amaryl/engine/engine.h"

#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

Engine* kEngine = nullptr;

Engine::~Engine() { shutdown(); }

bool Engine::init(SDL_Window* window, uint32 width, uint32 height) {
  ASSERT(rhi_ == nullptr, "Engine initialized twice");

  vk::ContextConfig context_config{};
  rhi_ = createVulkanContextWithSwapchain(window, width, height, context_config);
  if (!rhi_) {
    ALOGE("Failed to create the Vulkan context");
    return false;
  }
  width_ = width;
  height_ = height;

  renderer_.init(this);
  return true;
}

void Engine::shutdown() {
  if (!rhi_) {
    return;
  }

  rhi_->wait({});
  renderer_.shutdown();
  rhi_.reset();
}

void Engine::processResize(uint32 width, uint32 height) {
  if (!rhi_ || width == 0 || height == 0) {
    return;
  }

  width_ = width;
  height_ = height;
  rhi_->recreateSwapchain(static_cast<int>(width), static_cast<int>(height));
}

void Engine::update(float delta_time) {
  (void)delta_time;
  world_.sync();
  world_.entities.updateComponents();
}

bool Engine::render() {
  if (!rhi_) {
    return false;
  }

  if (!renderer_.beginFrame()) {
    // The swapchain is out of date, e.g. resized before the window event arrived.
    processResize(width_, height_);
    return true;
  }

  renderer_.renderGBuffersPass();
  renderer_.renderPostProcessPass();
  renderer_.endFrame();
  return true;
}

bool InitializeEngine(SDL_Window* window, uint32 width, uint32 height) {
  ASSERT(kEngine == nullptr, "Engine initialized twice");

  auto* engine = new Engine();
  if (!engine->init(window, width, height)) {
    delete engine;
    return false;
  }
  kEngine = engine;
  return true;
}

void ShutdownEngine() {
  delete kEngine;
  kEngine = nullptr;
}

}  // namespace amaryl

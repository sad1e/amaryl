#include "amaryl/engine/engine.h"

#include "amaryl/core/filesystem/vfs.h"
#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

Engine* kEngine = nullptr;

Engine::~Engine() { shutdown(); }

bool Engine::init(SDL_Window* window, uint32 width, uint32 height) {
  ASSERT(rhi_ == nullptr, "Engine initialized twice");

  if (!GetVirtualFileSystem().isMounted(kEngineScheme)) {
    ALOGE("engine:// is not mounted, the engine content cannot be loaded");
    return false;
  }

  vk::ContextConfig context_config{};
  rhi_ = createVulkanContextWithSwapchain(window, width, height, context_config);
  if (!rhi_) {
    ALOGE("Failed to create the Vulkan context");
    return false;
  }
  width_ = width;
  height_ = height;

  renderer_.init(this);
  textures_ = std::make_unique<TextureRegistry>(rhi_.get());
  world_.materials.setTextureReferences(textures_.get());
  models_ = std::make_unique<ModelLibrary>(world_, textures_.get());
  return true;
}

void Engine::shutdown() {
  if (!rhi_) {
    return;
  }

  rhi_->wait({});
  // The world outlives the RHI, its materials drop their texture references (their
  // descriptor indices dangle from here on).
  models_.reset();
  world_.materials.setTextureReferences(nullptr);
  textures_.reset();
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

bool Engine::initImGui() {
  ASSERT(rhi_ != nullptr, "Engine is not initialized");
  return renderer_.getImGuiRenderer().init(rhi_.get());
}

void Engine::shutdownImGui() {
  if (!rhi_) {
    return;
  }
  renderer_.getImGuiRenderer().shutdown();
}

bool Engine::render(ImDrawData* imgui_draw_data) {
  if (!rhi_) {
    return false;
  }

  if (!renderer_.beginFrame()) {
    // The swapchain is out of date, e.g. resized before the window event arrived.
    processResize(width_, height_);
    return true;
  }

  // Flushed only when the frame is recorded, skipped frames keep their changes pending.
  uploads_.clear();
  world_.meshes.flushUploads(uploads_);
  world_.materials.flushUploads(uploads_);
  world_.scene.flushUploads(uploads_);
  renderer_.uploadScene(uploads_);

  renderer_.renderGBuffersPass();
  renderer_.renderPostProcessPass();
  renderer_.renderImGuiPass(imgui_draw_data);
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

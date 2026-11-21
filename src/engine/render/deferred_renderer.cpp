#include "amaryl/engine/render/deferred_renderer.h"

#include "amaryl/engine/engine.h"

namespace amaryl {

namespace {

constexpr ClearColorValue kClearColor{.float32 = {0.06f, 0.07f, 0.09f, 1.0f}};

}  // namespace

void DeferredRenderer::init(Engine* engine) {
  ASSERT(engine != nullptr && engine->getRHI() != nullptr);
  rhi_ = engine->getRHI();
}

void DeferredRenderer::shutdown() {
  ASSERT(commands_ == nullptr, "Shutdown inside a frame");
  rhi_ = nullptr;
}

bool DeferredRenderer::beginFrame() {
  ASSERT(rhi_ != nullptr && commands_ == nullptr);

  swapchain_texture_ = rhi_->getCurrentSwapchainTexture();
  if (swapchain_texture_.empty()) {
    return false;
  }

  commands_ = &rhi_->acquireCommandBuffer();
  return true;
}

void DeferredRenderer::endFrame() {
  ASSERT(commands_ != nullptr);

  rhi_->submit(*commands_, swapchain_texture_);
  commands_ = nullptr;
  swapchain_texture_ = {};
}

void DeferredRenderer::renderGBuffersPass() {}

void DeferredRenderer::renderPostProcessPass() {
  commands_->cmdBeginRendering(
      {.color = {{.loadOp = ELoadOp::Clear, .clearColor = kClearColor}}},
      {.color = {{.texture = swapchain_texture_}}});
  commands_->cmdEndRendering();
}

}  // namespace amaryl

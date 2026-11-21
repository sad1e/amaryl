#include "amaryl/engine/engine.h"

#include <memory>

#include "amaryl/nrhi/nrhi.h"
#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

void Engine::init(SDL_Window* pWindow, int32 width, int32 height) {
  vk::ContextConfig context_config{};
  kRHI = createVulkanContextWithSwapchain(pWindow, width, height, context_config);
  ASSERT(kRHI != nullptr, TEXT("kRHI create failed."));
}

void Engine::tick() {}

void Engine::shutdown() {}

}  // namespace amaryl

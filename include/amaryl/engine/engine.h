#pragma once

#include "amaryl/engine/common.h"
#include "amaryl/engine/render/deferred_renderer.h"
#include "amaryl/engine/scene/world.h"
#include "amaryl/nrhi/nrhi.h"

namespace amaryl {

struct Engine {
  ENGINE_API void init(SDL_Window* pWindow, int32 width, int32 height);

  ENGINE_API void tick();

  ENGINE_API void shutdown();

 private:
  DeferredRenderer renderer_;
  World world_;
};

}  // namespace amaryl

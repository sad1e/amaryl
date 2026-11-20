#pragma once

#include "amaryl/engine/common.h"
#include "amaryl/engine/render/renderer.h"
#include "amaryl/engine/scene/world.h"

namespace amaryl {

struct Engine {
  void init(SDL_Window* pWindow, int32 width, int32 height);

 private:
  Renderer renderer_;
  World world_;
};

}  // namespace amaryl

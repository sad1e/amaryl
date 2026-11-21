#include "amaryl/engine/render/deferred_renderer.h"

namespace amaryl {

void SceneRenderTargets::init(uint32 width, uint32 height) {
  gbufferA.debugName = "GBufferA";

  gbufferB.debugName = "GBufferB";

  gbufferC.debugName = "GBufferC";

  gbufferD.debugName = "GBufferD";
}

}  // namespace amaryl

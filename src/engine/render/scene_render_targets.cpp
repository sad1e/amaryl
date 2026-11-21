#include "amaryl/engine/render/renderer.h"
#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

void SceneRenderTargets::init(uint32 width, uint32 height) {
  gbufferA.debugName = "GBufferA";

  gbufferB.debugName = "GBufferB";

  gbufferC.debugName = "GBufferC";

  gbufferD.debugName = "GBufferD";
}

}  // namespace amaryl

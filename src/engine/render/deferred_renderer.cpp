#include "amaryl/engine/render/deferred_renderer.h"

#include <string>
#include <vector>

#include "amaryl/core/filesystem/vfs.h"
#include "amaryl/engine/engine.h"
#include "amaryl/engine/render/shader_loader.h"

namespace amaryl {

namespace {

constexpr ClearColorValue kClearColor{.float32 = {0.06f, 0.07f, 0.09f, 1.0f}};

// Mirrors `DebugLightingPushConstants` in shaders/debug-lighting-only.slang:
// a 16-byte-aligned-friendly block holding a device address and four uint32.
struct alignas(8) DebugLightingPushConstants {
  uint64 debugBufferAddress;
  uint32 textureIndex;
  uint32 samplerIndex;
  uint32 frameIndex;
  uint32 pad0;
};
static_assert(sizeof(DebugLightingPushConstants) == 24);

// Contents of the small buffer whose address is delivered through push constants.
struct DebugLightingBuffer {
  float color0[4];
  float color1[4];
};
static_assert(sizeof(DebugLightingBuffer) == 32);

std::string resolveShaderPath(const char* relative_path) {
  return std::string(kEngineScheme) + "://shaders/" + relative_path;
}

}  // namespace

void DeferredRenderer::init(Engine* engine) {
  ASSERT(engine != nullptr && engine->getRHI() != nullptr);
  rhi_ = engine->getRHI();
  initDebugLightingOnly();
}

void DeferredRenderer::shutdown() {
  ASSERT(commands_ == nullptr, "Shutdown inside a frame");
  imgui_renderer_.shutdown();
  shutdownDebugLightingOnly();
  rhi_ = nullptr;
}

void DeferredRenderer::initDebugLightingOnly() {
  // A small RGBA gradient so bindless sampling is visibly verifiable.
  constexpr uint32 kTexWidth = 64;
  constexpr uint32 kTexHeight = 64;
  std::vector<uint32> pixels(kTexWidth * kTexHeight);
  for (uint32 y = 0; y < kTexHeight; ++y) {
    for (uint32 x = 0; x < kTexWidth; ++x) {
      const uint8 r = static_cast<uint8>(x * 255u / (kTexWidth - 1));
      const uint8 g = static_cast<uint8>(y * 255u / (kTexHeight - 1));
      const uint8 b = 128u;
      const uint8 a = 255u;
      pixels[y * kTexWidth + x] = (static_cast<uint32>(a) << 24) |
                                  (static_cast<uint32>(b) << 16) |
                                  (static_cast<uint32>(g) << 8) | r;
    }
  }

  Result result;
  debug_texture_ = rhi_->createTexture(
    {
      .type = ETextureType::Texture2D,
      .format = EFormat::RGBA_UN8,
      .dimensions = {.width = kTexWidth, .height = kTexHeight, .depth = 1},
      .usage = ETextureUsageBits::Sampled,
      .data = pixels.data(),
      .debugName = "Debug lighting texture",
    },
    "Debug lighting texture", &result);
  ASSERT(result.isOk() && debug_texture_.valid());

  debug_sampler_ = rhi_->createSampler(
    {
      .minFilter = ESamplerFilter::Linear,
      .magFilter = ESamplerFilter::Linear,
      .mipMap = ESamplerMip::Nearest,
      .wrapU = ESamplerWrap::Clamp,
      .wrapV = ESamplerWrap::Clamp,
      .wrapW = ESamplerWrap::Clamp,
      .debugName = "Debug lighting sampler",
    },
    &result);
  ASSERT(result.isOk() && debug_sampler_.valid());

  // A small storage buffer read through a raw device address from push constants.
  const DebugLightingBuffer debug_buffer_data = {
    {1.0f, 0.0f, 0.0f, 1.0f},  // red
    {0.0f, 1.0f, 0.0f, 1.0f},  // green
  };
  debug_buffer_ = rhi_->createBuffer(
    {
      .usage = static_cast<uint8>(EBufferUsageBits::Storage),
      .storageType = EStorageType::Device,
      .size = sizeof(debug_buffer_data),
      .data = &debug_buffer_data,
      .debugName = "Debug lighting buffer",
    },
    "Debug lighting buffer", &result);
  ASSERT(result.isOk() && debug_buffer_.valid());

  debug_vert_ = ShaderLoader::loadShaderModule(rhi_,
    resolveShaderPath("debug-lighting-only.vertex.spv"), EShaderStage::Vertex,
    "debug-lighting-only.vertex", &result);
  ASSERT(result.isOk() && debug_vert_.valid());

  debug_frag_ = ShaderLoader::loadShaderModule(rhi_,
    resolveShaderPath("debug-lighting-only.fragment.spv"), EShaderStage::Fragment,
    "debug-lighting-only.fragment", &result);
  ASSERT(result.isOk() && debug_frag_.valid());

  RenderPipelineDesc pipeline_desc;
  pipeline_desc.smVert = debug_vert_;
  pipeline_desc.smFrag = debug_frag_;
  pipeline_desc.entryPointVert = kVertexEntryName;
  pipeline_desc.entryPointFrag = kFragmentEntryName;
  pipeline_desc.color[0].format = rhi_->getSwapchainFormat();
  pipeline_desc.cullMode = ECullMode::None;
  pipeline_desc.debugName = "debug-lighting-only";

  debug_pipeline_ = rhi_->createRenderPipeline(pipeline_desc, &result);
  ASSERT(result.isOk() && debug_pipeline_.valid());
}

void DeferredRenderer::shutdownDebugLightingOnly() {
  debug_pipeline_.reset();
  debug_frag_.reset();
  debug_vert_.reset();
  debug_buffer_.reset();
  debug_sampler_.reset();
  debug_texture_.reset();
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
  ASSERT(commands_ != nullptr);

  commands_->cmdBeginRendering(
    {.color = {{.loadOp = ELoadOp::Clear, .clearColor = kClearColor}}},
    {.color = {{.texture = swapchain_texture_}}});

  commands_->cmdBindRenderPipeline(debug_pipeline_);

  const DebugLightingPushConstants push_constants{
    .debugBufferAddress = rhi_->gpuAddress(debug_buffer_, 0),
    .textureIndex = rhi_->getTextureDescriptorIndex(debug_texture_).value,
    .samplerIndex = rhi_->getSamplerDescriptorIndex(debug_sampler_).value,
    .frameIndex = 0u,
    .pad0 = 0u,
  };
  commands_->cmdPushConstants(&push_constants, sizeof(push_constants), 0);

  commands_->cmdDraw(3);

  commands_->cmdEndRendering();
}

void DeferredRenderer::renderImGuiPass(ImDrawData* draw_data) {
  ASSERT(commands_ != nullptr);
  if (!draw_data || !imgui_renderer_.isInitialized()) {
    return;
  }
  imgui_renderer_.render(*commands_, swapchain_texture_, draw_data);
}

}  // namespace amaryl

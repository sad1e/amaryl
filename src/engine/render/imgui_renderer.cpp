#include "amaryl/engine/render/imgui_renderer.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "amaryl/core/filesystem/vfs.h"
#include "amaryl/engine/render/shader_loader.h"

namespace amaryl {

namespace {

// Mirrors `ImGuiPushConstants` in shaders/imgui.slang.
struct alignas(8) ImGuiPushConstants {
  uint64 vertexBufferAddress;
  float scale[2];
  float translate[2];
  uint32 textureIndex;
  uint32 samplerIndex;
  uint32 flags;
  uint32 pad0;
};
static_assert(sizeof(ImGuiPushConstants) == 40);

// imgui.slang reads ImDrawVert as 5 x uint32, the index buffer is bound as UI16.
static_assert(sizeof(ImDrawVert) == 20);
static_assert(sizeof(ImDrawIdx) == 2);

constexpr uint32 kImGuiFlagLinearOutput = 1u;
constexpr size_t kMinVertexBufferSize = 64u * 1024u;
constexpr size_t kMinIndexBufferSize = 32u * 1024u;

// GPU copy of an ImTextureData, stored in ImTextureData::BackendUserData.
struct ImGuiTexture {
  Holder<TextureHandle> texture;
};

ImGuiTexture* getImGuiTexture(const ImTextureData* tex) {
  return static_cast<ImGuiTexture*>(tex->BackendUserData);
}

std::string resolveShaderPath(const char* relative_path) {
  return std::string(kEngineScheme) + "://shaders/" + relative_path;
}

bool isSRGBFormat(EFormat format) {
  return format == EFormat::RGBA_SRGB8 || format == EFormat::BGRA_SRGB8;
}

// Copies a rectangle of `tex` into `out` as tightly packed RGBA8. The rectangle is
// repacked instead of uploaded with a row length, the staging copy would otherwise read
// past the end of the ImGui pixel buffer on the last row.
void copyRectToRGBA8(
  ImTextureData* tex, int x, int y, int w, int h, std::vector<uint8>& out) {
  out.resize(static_cast<size_t>(w) * h * 4);
  for (int row = 0; row < h; ++row) {
    const auto* src = static_cast<const uint8*>(tex->GetPixelsAt(x, y + row));
    uint8* dst = out.data() + static_cast<size_t>(row) * w * 4;
    if (tex->Format == ImTextureFormat_RGBA32) {
      std::memcpy(dst, src, static_cast<size_t>(w) * 4);
      continue;
    }
    // ImTextureFormat_Alpha8
    for (int i = 0; i < w; ++i) {
      dst[i * 4 + 0] = 255u;
      dst[i * 4 + 1] = 255u;
      dst[i * 4 + 2] = 255u;
      dst[i * 4 + 3] = src[i];
    }
  }
}

}  // namespace

ImGuiRenderer::~ImGuiRenderer() {
  ASSERT(rhi_ == nullptr, "ImGuiRenderer::shutdown() was not called");
}

bool ImGuiRenderer::init(IContext* rhi) {
  ASSERT(rhi != nullptr && rhi_ == nullptr, "ImGuiRenderer initialized twice");
  ASSERT(ImGui::GetCurrentContext() != nullptr, "Create the ImGui context first");

  ImGuiIO& io = ImGui::GetIO();
  ASSERT(io.BackendRendererUserData == nullptr, "A renderer backend is already set");

  Result result;
  vert_ = ShaderLoader::loadShaderModule(rhi, resolveShaderPath("imgui.vertex.spv"),
    EShaderStage::Vertex, "imgui.vertex", &result);
  if (result.isOk()) {
    frag_ = ShaderLoader::loadShaderModule(rhi, resolveShaderPath("imgui.fragment.spv"),
      EShaderStage::Fragment, "imgui.fragment", &result);
  }
  if (result.isOk()) {
    sampler_ = rhi->createSampler(
      {
        .minFilter = ESamplerFilter::Linear,
        .magFilter = ESamplerFilter::Linear,
        .mipMap = ESamplerMip::Disabled,
        .wrapU = ESamplerWrap::Clamp,
        .wrapV = ESamplerWrap::Clamp,
        .wrapW = ESamplerWrap::Clamp,
        .debugName = "ImGui sampler",
      },
      &result);
  }

  const EFormat color_format = rhi->getSwapchainFormat();
  if (result.isOk()) {
    RenderPipelineDesc desc;
    desc.smVert = vert_;
    desc.smFrag = frag_;
    desc.entryPointVert = kVertexEntryName;
    desc.entryPointFrag = kFragmentEntryName;
    desc.color[0] = {
      .format = color_format,
      .blendEnabled = true,
      .rgbBlendOp = EBlendOp::Add,
      .alphaBlendOp = EBlendOp::Add,
      .srcRGBBlendFactor = EBlendFactor::SrcAlpha,
      .srcAlphaBlendFactor = EBlendFactor::One,
      .dstRGBBlendFactor = EBlendFactor::OneMinusSrcAlpha,
      .dstAlphaBlendFactor = EBlendFactor::OneMinusSrcAlpha,
    };
    desc.cullMode = ECullMode::None;
    desc.debugName = "imgui";
    pipeline_ = rhi->createRenderPipeline(desc, &result);
  }

  if (!result.isOk()) {
    ALOGE("Failed to create the ImGui renderer");
    pipeline_.reset();
    sampler_.reset();
    frag_.reset();
    vert_.reset();
    return false;
  }

  rhi_ = rhi;
  context_ = ImGui::GetCurrentContext();
  linear_output_ = isSRGBFormat(color_format);

  io.BackendRendererName = "amaryl_nrhi";
  io.BackendRendererUserData = this;
  io.BackendFlags |=
    ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
  return true;
}

void ImGuiRenderer::shutdown() {
  if (!rhi_) {
    return;
  }
  ASSERT(ImGui::GetCurrentContext() == context_,
    "The ImGui context must be alive and current when the renderer shuts down");

  // nrhi defers the destruction until the frames that used the textures complete.
  for (ImTextureData* tex : ImGui::GetPlatformIO().Textures) {
    if (tex->RefCount == 1) {
      destroyTexture(tex);
    }
  }

  ImGuiIO& io = ImGui::GetIO();
  io.BackendRendererName = nullptr;
  io.BackendRendererUserData = nullptr;
  io.BackendFlags &=
    ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);

  frames_.clear();
  pipeline_.reset();
  sampler_.reset();
  frag_.reset();
  vert_.reset();
  sampled_textures_.clear();
  upload_scratch_.clear();
  rhi_ = nullptr;
  context_ = nullptr;
}

ImTextureID ImGuiRenderer::toTextureID(IContext* rhi, TextureHandle texture) {
  // Descriptor 0 is the dummy texture, so a valid texture never maps to
  // ImTextureID_Invalid.
  return static_cast<ImTextureID>(rhi->getTextureDescriptorIndex(texture).value);
}

void ImGuiRenderer::updateTexture(ImTextureData* tex) {
  switch (tex->Status) {
    case ImTextureStatus_WantCreate: {
      ASSERT(tex->BackendUserData == nullptr && tex->TexID == ImTextureID_Invalid);
      copyRectToRGBA8(tex, 0, 0, tex->Width, tex->Height, upload_scratch_);

      Result result;
      auto* gpu_texture = new ImGuiTexture();
      gpu_texture->texture = rhi_->createTexture(
        {
          .type = ETextureType::Texture2D,
          .format = EFormat::RGBA_UN8,
          .dimensions = {.width = static_cast<uint32>(tex->Width),
            .height = static_cast<uint32>(tex->Height),
            .depth = 1},
          .usage = ETextureUsageBits::Sampled,
          .data = upload_scratch_.data(),
          .debugName = "ImGui texture",
        },
        "ImGui texture", &result);
      ASSERT(result.isOk() && gpu_texture->texture.valid());

      tex->BackendUserData = gpu_texture;
      tex->SetTexID(toTextureID(rhi_, gpu_texture->texture));
      tex->SetStatus(ImTextureStatus_OK);
      break;
    }
    case ImTextureStatus_WantUpdates: {
      ImGuiTexture* gpu_texture = getImGuiTexture(tex);
      ASSERT(gpu_texture != nullptr);
      for (const ImTextureRect& rect : tex->Updates) {
        copyRectToRGBA8(tex, rect.x, rect.y, rect.w, rect.h, upload_scratch_);
        const Result result = rhi_->upload(gpu_texture->texture,
          {
            .offset = {.x = rect.x, .y = rect.y, .z = 0},
            .dimensions = {.width = rect.w, .height = rect.h, .depth = 1},
          },
          upload_scratch_.data(), 0);
        ASSERT(result.isOk());
      }
      tex->SetStatus(ImTextureStatus_OK);
      break;
    }
    case ImTextureStatus_WantDestroy:
      destroyTexture(tex);
      break;
    default:
      break;
  }
}

void ImGuiRenderer::destroyTexture(ImTextureData* tex) {
  if (ImGuiTexture* gpu_texture = getImGuiTexture(tex)) {
    delete gpu_texture;
    tex->BackendUserData = nullptr;
  }
  tex->SetTexID(ImTextureID_Invalid);
  tex->SetStatus(ImTextureStatus_Destroyed);
}

bool ImGuiRenderer::reserveFrameBuffers(
  FrameBuffers& frame, size_t vertex_bytes, size_t index_bytes) {
  Result result;
  // The replaced buffers were used by older frames only, nrhi defers their destruction.
  if (frame.vertex_capacity < vertex_bytes) {
    const size_t capacity = std::max(vertex_bytes + vertex_bytes / 2, kMinVertexBufferSize);
    frame.vertex = rhi_->createBuffer(
      {
        .usage = static_cast<uint8>(EBufferUsageBits::Storage),
        .storageType = EStorageType::HostVisible,
        .size = capacity,
        .debugName = "ImGui vertex buffer",
      },
      "ImGui vertex buffer", &result);
    if (!result.isOk() || frame.vertex.empty()) {
      ALOGE("Failed to create the ImGui vertex buffer");
      frame.vertex_capacity = 0;
      return false;
    }
    frame.vertex_capacity = capacity;
  }

  if (frame.index_capacity < index_bytes) {
    const size_t capacity = std::max(index_bytes + index_bytes / 2, kMinIndexBufferSize);
    frame.index = rhi_->createBuffer(
      {
        .usage = static_cast<uint8>(EBufferUsageBits::Index),
        .storageType = EStorageType::HostVisible,
        .size = capacity,
        .debugName = "ImGui index buffer",
      },
      "ImGui index buffer", &result);
    if (!result.isOk() || frame.index.empty()) {
      ALOGE("Failed to create the ImGui index buffer");
      frame.index_capacity = 0;
      return false;
    }
    frame.index_capacity = capacity;
  }
  return true;
}

void ImGuiRenderer::render(ICommandBuffer& commands, TextureHandle target,
  ImDrawData* draw_data, Span<TextureHandle> sampled_textures) {
  ASSERT(rhi_ != nullptr, "ImGuiRenderer is not initialized");
  if (!draw_data) {
    return;
  }

  // Texture requests are honored even when nothing is drawn.
  if (draw_data->Textures) {
    for (ImTextureData* tex : *draw_data->Textures) {
      if (tex->Status != ImTextureStatus_OK) {
        updateTexture(tex);
      }
    }
  }

  const float fb_width = draw_data->DisplaySize.x * draw_data->FramebufferScale.x;
  const float fb_height = draw_data->DisplaySize.y * draw_data->FramebufferScale.y;
  if (fb_width <= 0.0f || fb_height <= 0.0f || draw_data->TotalVtxCount <= 0) {
    return;
  }

  // Acquiring swapchain image N waits for the frame that last rendered to it, so its
  // buffers are free to be rewritten.
  const uint32 num_frames = std::max(rhi_->getNumSwapchainImages(), 1u);
  if (frames_.size() != num_frames) {
    frames_.clear();
    frames_.resize(num_frames);
  }
  FrameBuffers& frame = frames_[rhi_->getSwapchainCurrentImageIndex() % num_frames];

  const size_t vertex_bytes =
    static_cast<size_t>(draw_data->TotalVtxCount) * sizeof(ImDrawVert);
  const size_t index_bytes =
    static_cast<size_t>(draw_data->TotalIdxCount) * sizeof(ImDrawIdx);
  if (!reserveFrameBuffers(frame, vertex_bytes, index_bytes)) {
    return;
  }

  auto* vertex_dst = reinterpret_cast<ImDrawVert*>(rhi_->getMappedPtr(frame.vertex));
  auto* index_dst = reinterpret_cast<ImDrawIdx*>(rhi_->getMappedPtr(frame.index));
  ASSERT(vertex_dst != nullptr && index_dst != nullptr);
  for (const ImDrawList* list : draw_data->CmdLists) {
    std::memcpy(vertex_dst, list->VtxBuffer.Data,
      static_cast<size_t>(list->VtxBuffer.Size) * sizeof(ImDrawVert));
    std::memcpy(index_dst, list->IdxBuffer.Data,
      static_cast<size_t>(list->IdxBuffer.Size) * sizeof(ImDrawIdx));
    vertex_dst += list->VtxBuffer.Size;
    index_dst += list->IdxBuffer.Size;
  }
  rhi_->flushMappedMemory(frame.vertex, 0, vertex_bytes);
  rhi_->flushMappedMemory(frame.index, 0, index_bytes);

  // Bindless and device-address accesses are invisible to nrhi: list them as
  // dependencies so they are tracked by this submit and not released too early.
  sampled_textures_.clear();
  if (draw_data->Textures) {
    for (ImTextureData* tex : *draw_data->Textures) {
      if (const ImGuiTexture* gpu_texture = getImGuiTexture(tex)) {
        sampled_textures_.push_back(gpu_texture->texture);
      }
    }
  }
  for (TextureHandle texture : sampled_textures) {
    sampled_textures_.push_back(texture);
  }
  BufferHandle dependency_buffers[] = {frame.vertex};

  commands.cmdPushDebugGroupLabel("ImGui", 0xff99ff99);
  commands.cmdBeginRendering(
    {.color = {{.loadOp = ELoadOp::Load, .storeOp = EStoreOp::Store}}},
    {.color = {{.texture = target}}},
    {.sampledImages = sampled_textures_, .buffers = dependency_buffers});

  // NDC y = +1 is the top of the target, cmdBindViewport() flips the viewport.
  const float scale_x = 2.0f / draw_data->DisplaySize.x;
  const float scale_y = -2.0f / draw_data->DisplaySize.y;
  ImGuiPushConstants push_constants{
    .vertexBufferAddress = 0,
    .scale = {scale_x, scale_y},
    .translate = {-1.0f - draw_data->DisplayPos.x * scale_x,
      1.0f - draw_data->DisplayPos.y * scale_y},
    .textureIndex = 0,
    .samplerIndex = rhi_->getSamplerDescriptorIndex(sampler_).value,
    .flags = linear_output_ ? kImGuiFlagLinearOutput : 0u,
    .pad0 = 0,
  };
  const uint64 vertex_base_address = rhi_->gpuAddress(frame.vertex, 0);

  auto setup_render_state = [&] {
    commands.cmdBindRenderPipeline(pipeline_);
    commands.cmdBindIndexBuffer(frame.index, EIndexFormat::UI16, 0, index_bytes);
  };
  setup_render_state();

  ImGuiRenderState render_state{.commands = &commands};
  ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
  platform_io.Renderer_RenderState = &render_state;

  const ImVec2 clip_offset = draw_data->DisplayPos;
  const ImVec2 clip_scale = draw_data->FramebufferScale;
  uint32 global_vertex_offset = 0;
  uint32 global_index_offset = 0;
  for (const ImDrawList* list : draw_data->CmdLists) {
    for (const ImDrawCmd& cmd : list->CmdBuffer) {
      if (cmd.UserCallback) {
        if (cmd.UserCallback == ImDrawCallback_ResetRenderState) {
          setup_render_state();
        } else {
          cmd.UserCallback(list, &cmd);
        }
        continue;
      }

      const float min_x = std::max((cmd.ClipRect.x - clip_offset.x) * clip_scale.x, 0.0f);
      const float min_y = std::max((cmd.ClipRect.y - clip_offset.y) * clip_scale.y, 0.0f);
      const float max_x =
        std::min((cmd.ClipRect.z - clip_offset.x) * clip_scale.x, fb_width);
      const float max_y =
        std::min((cmd.ClipRect.w - clip_offset.y) * clip_scale.y, fb_height);
      if (max_x <= min_x || max_y <= min_y) {
        continue;
      }
      commands.cmdBindScissorRect({
        .x = static_cast<uint32>(min_x),
        .y = static_cast<uint32>(min_y),
        .width = static_cast<uint32>(max_x - min_x),
        .height = static_cast<uint32>(max_y - min_y),
      });

      // VtxOffset goes into the address, the draw keeps vertexOffset = 0 so SV_VertexID
      // is the raw index whatever the base-vertex semantic of the shader compiler.
      push_constants.vertexBufferAddress =
        vertex_base_address +
        static_cast<uint64>(global_vertex_offset + cmd.VtxOffset) * sizeof(ImDrawVert);
      push_constants.textureIndex = static_cast<uint32>(cmd.GetTexID());
      commands.cmdPushConstants(push_constants, 0);
      commands.cmdDrawIndexed(cmd.ElemCount, 1, global_index_offset + cmd.IdxOffset, 0, 0);
    }
    global_vertex_offset += static_cast<uint32>(list->VtxBuffer.Size);
    global_index_offset += static_cast<uint32>(list->IdxBuffer.Size);
  }

  platform_io.Renderer_RenderState = nullptr;
  commands.cmdEndRendering();
  commands.cmdPopDebugGroupLabel();
}

}  // namespace amaryl

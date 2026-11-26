#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

namespace vk {
namespace {

void _transition_to_color_attachment(VkCommandBuffer cmd_buffer,
                                     VulkanImage* color_image) {
  ASSERT(color_image,
         TEXT("Transntion to color attachment, the color_image shouldn't be nullptr"));
  ASSERT(!color_image->isDepthFormat() && !color_image->isStencilFormat(),
         TEXT("Color attachments cannot have depth/stencil formats"));

  ASSERT(color_image->getFormat() != VK_FORMAT_UNDEFINED,
         TEXT("Invalid color attachment format"));
  color_image->transitionLayout(cmd_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VkImageSubresourceRange{
                                    VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    VK_REMAINING_MIP_LEVELS,
                                    0,
                                    VK_REMAINING_ARRAY_LAYERS,
                                });
}

}  // namespace

#define HEX_TO_FLOATS(x)                \
  {                                     \
      float((x >> 0) & 0xff) / 255.0f,  \
      float((x >> 8) & 0xff) / 255.0f,  \
      float((x >> 16) & 0xff) / 255.0f, \
      float((x >> 24) & 0xff) / 255.0f, \
  }

CommandBuffer::CommandBuffer(Context* ctx, VulkanImmediateCommands& immediate,
                             uint32 queue_family_index)
    : ctx_(ctx),
      wrapper_(&immediate.acquire()),
      immediate_(&immediate),
      queue_family_index_(queue_family_index) {}

CommandBuffer::~CommandBuffer() {
  ASSERT(!is_rendering_, TEXT("Did you forget to call cmdEndRendering()?"));
}

void CommandBuffer::trackResource(BufferHandle handle) const {
  ctx_->resource_tracker_.record(handle, wrapper_->handle, isComputeOnlyQueue());
}

void CommandBuffer::trackResource(TextureHandle handle) const {
  bool following_parent = false;
  while (!handle.empty()) {
    const VulkanImage* image = ctx_->texture_pool_.get(handle);
    ASSERT(following_parent || !image->pending_destroy_,
           "Cannot record a texture pending destruction");
    ctx_->resource_tracker_.record(handle, wrapper_->handle, isComputeOnlyQueue());
    if (!image || image->parent_.empty()) {
      break;
    }
    handle = image->parent_;
    following_parent = true;
  }
}

void CommandBuffer::trackResource(SamplerHandle handle) const {
  ctx_->resource_tracker_.record(handle, wrapper_->handle, isComputeOnlyQueue());
}

void CommandBuffer::trackResource(QueryPoolHandle handle) const {
  ctx_->resource_tracker_.record(handle, wrapper_->handle, isComputeOnlyQueue());
}

void CommandBuffer::trackResource(ComputePipelineHandle handle) const {
  ctx_->resource_tracker_.record(handle, wrapper_->handle, isComputeOnlyQueue());
}

void CommandBuffer::trackResource(RenderPipelineHandle handle) const {
  ctx_->resource_tracker_.record(handle, wrapper_->handle, isComputeOnlyQueue());
}

void CommandBuffer::trackResource(RayTracingPipelineHandle handle) const {
  ctx_->resource_tracker_.record(handle, wrapper_->handle, isComputeOnlyQueue());
}

void CommandBuffer::trackResource(AccelerationStructHandle handle) const {
  ctx_->resource_tracker_.record(handle, wrapper_->handle, isComputeOnlyQueue());
}

void CommandBuffer::trackResources(const Dependencies& deps) const {
  for (TextureHandle handle : deps.sampledImages) {
    trackResource(handle);
  }
  for (TextureHandle handle : deps.storageImages) {
    trackResource(handle);
  }
  for (BufferHandle handle : deps.buffers) {
    trackResource(handle);
  }
  for (SamplerHandle handle : deps.samplers) {
    trackResource(handle);
  }
  for (AccelerationStructHandle handle : deps.accelerationStructures) {
    trackResource(handle);
  }
  for (TextureHandle handle : deps.inputAttachments) {
    trackResource(handle);
  }
}

void CommandBuffer::trackResources(const Framebuffer& frame_buffer) const {
  for (const Framebuffer::AttachmentDesc& attachment : frame_buffer.color) {
    trackResource(attachment.texture);
    trackResource(attachment.resolveTexture);
  }

  trackResource(frame_buffer.depthStencil.texture);
  trackResource(frame_buffer.depthStencil.resolveTexture);
  trackResource(frame_buffer.fragmentDensityMap);
  trackResource(frame_buffer.shadingRateAttachment);
}

void CommandBuffer::cmdTransitionToGeneral(const Span<TextureHandle>& textures,
                                           EShaderStage extra_dst_stage) const {
  StageAccess extra_dst_access = {};

  if (extra_dst_stage == EShaderStage::Compute) {
    extra_dst_access.stage |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }

  if (EnumHasAnyFlags(extra_dst_stage,
                      EShaderStage::RayGeneration | EShaderStage::ClosestHit |
                          EShaderStage::Miss | EShaderStage::Intersection |
                          EShaderStage::AnyHit | EShaderStage::Callable)) {
    extra_dst_access.stage |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
  }

  for (TextureHandle handle : textures) {
    ASSERT(!handle.empty());
    trackResource(handle);

    VulkanImage& image = *ctx_->texture_pool_.get(handle);

    StageAccess dst = getPipelineStageAccess(VK_IMAGE_LAYOUT_GENERAL);
    dst.stage |= extra_dst_access.stage;
    dst.access |= extra_dst_access.access;

    // Check the image is ownered by another family queue, if it is, acquire the
    // ownership.
    const bool acquired = acquireOwnershipIfPending(image, dst);

    if (!acquired && image.owner_queue_family_ != VK_QUEUE_FAMILY_IGNORED &&
        image.owner_queue_family_ != queue_family_index_) {
      image.layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    image.transitionLayout(wrapper_->cmdBuf,         //
                           VK_IMAGE_LAYOUT_GENERAL,  //
                           VkImageSubresourceRange{
                               .aspectMask = image.getImageAspectFlags(),
                               .baseMipLevel = 0,
                               .levelCount = VK_REMAINING_MIP_LEVELS,
                               .baseArrayLayer = 0,
                               .layerCount = VK_REMAINING_ARRAY_LAYERS,
                           },                 //
                           extra_dst_access,  //
                           isComputeOnlyQueue());

    image.owner_queue_family_ = queue_family_index_;
  }
}

void CommandBuffer::cmdTransitionToShaderReadOnly(const Span<TextureHandle>& textures,
                                                  EShaderStage extra_dst_stage) const {
  StageAccess extra_dst_access{};

  if (extra_dst_stage == EShaderStage::Compute) {
    extra_dst_access.stage |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }
  if (EnumHasAnyFlags(extra_dst_stage,
                      EShaderStage::RayGeneration | EShaderStage::ClosestHit |
                          EShaderStage::Miss | EShaderStage::Intersection |
                          EShaderStage::AnyHit | EShaderStage::Callable)) {
    extra_dst_access.stage |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
  }

  for (TextureHandle handle : textures) {
    trackResource(handle);
    VulkanImage& image = *ctx_->texture_pool_.get(handle);
    ASSERT(!image.is_swapchain_image_, TEXT("The texture should not be swapchain."));

    if (image.sample_count_ != VK_SAMPLE_COUNT_1_BIT) {
      continue;
    }

    ASSERT(image.usage_flags_ & VK_IMAGE_USAGE_SAMPLED_BIT,
           TEXT("Texture must has VK_IMAGE_USAGE_SAMPLED_BIT."));

    StageAccess dst = getPipelineStageAccess(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    dst.stage |= extra_dst_access.stage;
    dst.access |= extra_dst_access.access;

    // Complete a pending cross-queue handoff (acquire half). It replays the producer's
    // rendezvous layout. The layout must be same beweteen producer and consumer, which
    // the rendezvous layout means.
    if (acquireOwnershipIfPending(image, dst) &&
        image.layout_ == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      continue;
    }

    const VkImageAspectFlags aspect_flags = image.getImageAspectFlags();
    image.transitionLayout(
        wrapper_->cmdBuf,                          //
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,  //
        VkImageSubresourceRange{aspect_flags, 0, VK_REMAINING_MIP_LEVELS, 0,
                                VK_REMAINING_ARRAY_LAYERS},  //
        extra_dst_access,                                    //
        isComputeOnlyQueue());

    // Record the new owner queue family.
    if (image.owner_queue_family_ == VK_QUEUE_FAMILY_IGNORED) {
      image.owner_queue_family_ = queue_family_index_;
    }
  }
}

void CommandBuffer::cmdTransitionToRenderingLocalRead(
    const Span<TextureHandle>& textures) const {
  for (TextureHandle handle : textures) {
    trackResource(handle);
    VulkanImage& image = *ctx_->texture_pool_.get(handle);

    ASSERT(!image.is_swapchain_image_, TEXT("The texture should not be swapchain."));
    ASSERT(image.usage_flags_ & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
           TEXT("Input attachment texture must has VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT"));

    const VkImageAspectFlags aspect_flags = image.getImageAspectFlags();
    image.transitionLayout(
        wrapper_->cmdBuf,                          //
        VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR,  //
        VkImageSubresourceRange{aspect_flags, 0, VK_REMAINING_MIP_LEVELS, 0,
                                VK_REMAINING_ARRAY_LAYERS}  //
    );
  }
}

void CommandBuffer::cmdPushDebugGroupLabel(const char* label, uint32 colorRGBA) const {
  if (!label || !vkCmdBeginDebugUtilsLabelEXT) {
    return;
  }

  const VkDebugUtilsLabelEXT debug_utils_label{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = label,
      .color = HEX_TO_FLOATS(colorRGBA),
  };

  vkCmdBeginDebugUtilsLabelEXT(wrapper_->cmdBuf, &debug_utils_label);
}

void CommandBuffer::cmdInsertDebugEventLabel(const char* label, uint32 colorRGBA) const {
  if (!label || !vkCmdInsertDebugUtilsLabelEXT) {
    return;
  }

  const VkDebugUtilsLabelEXT debug_utils_label{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
      .pNext = nullptr,
      .pLabelName = label,
      .color = HEX_TO_FLOATS(colorRGBA),
  };

  vkCmdInsertDebugUtilsLabelEXT(wrapper_->cmdBuf, &debug_utils_label);
}

void CommandBuffer::cmdPopDebugGroupLabel() const {
  if (!vkCmdEndDebugUtilsLabelEXT) {
    return;
  }
  vkCmdEndDebugUtilsLabelEXT(wrapper_->cmdBuf);
}

void CommandBuffer::cmdBindRayTracingPipeline(RayTracingPipelineHandle handle) {
  if (!(!handle.empty() && ctx_->has_KHR_ray_tracing_pipeline_)) {
    return;
  }

  current_graphics_pipeline_ = {};
  current_compute_pipeline_ = {};
  current_ray_tracing_pipeline_ = handle;
  trackResource(handle);

  VkPipeline pipeline = ctx_->getVkPipeline(handle);
  const RayTracingPipelineState* rtps = ctx_->ray_tracing_pipeline_pool_.get(handle);

  ASSERT(rtps != nullptr);
  ASSERT(pipeline != VK_NULL_HANDLE);

  if (last_pipeline_bound_ != pipeline) {
    last_pipeline_bound_ = pipeline;
    vkCmdBindPipeline(wrapper_->cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);

    ctx_->checkAndUpdateDescriptorSets();
    ctx_->bindDefaultDescriptorSets(
        wrapper_->cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtps->pipelineLayout,
        wrapper_->handle, isComputeOnlyQueue());
  }
}

void CommandBuffer::cmdBindComputePipeline(ComputePipelineHandle handle) {
  if (handle.empty()) {
    return;
  }

  current_graphics_pipeline_ = {};
  current_compute_pipeline_ = handle;
  current_ray_tracing_pipeline_ = {};
  trackResource(handle);

  VkPipeline pipeline = ctx_->getVkPipeline(handle);
  const ComputePipelineState* cps = ctx_->compute_pipeline_pool_.get(handle);

  ASSERT(cps != nullptr);
  ASSERT(pipeline != VK_NULL_HANDLE);

  if (last_pipeline_bound_ != pipeline) {
    last_pipeline_bound_ = pipeline;
    vkCmdBindPipeline(wrapper_->cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    ctx_->checkAndUpdateDescriptorSets();
    ctx_->bindDefaultDescriptorSets(wrapper_->cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    cps->pipelineLayout, wrapper_->handle,
                                    isComputeOnlyQueue());
  }
}

void CommandBuffer::cmdDispatch(const Dimensions& group_count, const Dependencies& deps) {
  // TODO: profiling
  ASSERT(!is_rendering_);

  trackResources(deps);
  addCrossQueueDependencies(deps);
  cmdTransitionToShaderReadOnly(deps.sampledImages, EShaderStage::Compute);
  cmdTransitionToGeneral(deps.storageImages, EShaderStage::Compute);

  for (size_t i = 0; i < deps.buffers.size(); ++i) {
    [[maybe_unused]] const VulkanBuffer* buffer = ctx_->buffer_pool_.get(deps.buffers[i]);
    ASSERT(buffer->usage_flags_ & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
           TEXT("The buffer used by `cmdDispatch` should be specified to "
                "EBufferUsage::Storage"));

    bufferBarrier(
        deps.buffers[i],
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
  }

  vkCmdDispatch(wrapper_->cmdBuf, group_count.width, group_count.height,
                group_count.depth);
}

void CommandBuffer::cmdDispatchIndirect(BufferHandle indirect_buffer,
                                        size_t indirect_buffer_offset,
                                        const Dependencies& deps) {
  ASSERT(!is_rendering_);

  trackResources(deps);
  addCrossQueueDependencies(deps);
  cmdTransitionToShaderReadOnly(deps.sampledImages, EShaderStage::Compute);
  cmdTransitionToGeneral(deps.storageImages, EShaderStage::Compute);

  for (size_t i = 0; i < deps.buffers.size(); ++i) {
    [[maybe_unused]] const VulkanBuffer* buffer = ctx_->buffer_pool_.get(deps.buffers[i]);
    ASSERT(buffer->usage_flags_ & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
           TEXT("The buffer used by `cmdDispatch` should be specified to "
                "EBufferUsage::Storage"));

    bufferBarrier(
        deps.buffers[i],
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
  }

  const VulkanBuffer* buffer = ctx_->buffer_pool_.get(indirect_buffer);
  ASSERT(buffer);
  ASSERT(buffer->usage_flags_ & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
  ASSERT(indirect_buffer_offset % sizeof(uint32) == 0);

  bufferBarrier(indirect_buffer,
                VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);

  vkCmdDispatchIndirect(wrapper_->cmdBuf, buffer->vk_handle_, indirect_buffer_offset);
}

void CommandBuffer::cmdBeginRendering(const RenderPass& render_pass,
                                      const Framebuffer& frame_buffer,
                                      const Dependencies& deps) {
  ASSERT(!is_rendering_);

  is_rendering_ = true;
  view_mask_ = render_pass.viewMask;

  trackResources(deps);
  trackResources(frame_buffer);
  addCrossQueueDependencies(deps);
  cmdTransitionToShaderReadOnly(deps.sampledImages, {});
  cmdTransitionToGeneral(deps.storageImages, {});

  for (size_t i = 0; i < deps.buffers.size(); ++i) {
    VkPipelineStageFlags2 dst_stage_flags =
        VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    VulkanBuffer* buffer = ctx_->buffer_pool_.get(deps.buffers[i]);

    ASSERT(buffer, TEXT("The dependent buffer is null, it shouldn't be"));

    if (buffer->usage_flags_ & VK_BUFFER_USAGE_INDEX_BUFFER_BIT ||
        buffer->usage_flags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
      dst_stage_flags |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
    }

    if (buffer->usage_flags_ & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
      dst_stage_flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
    }

    bufferBarrier(deps.buffers[i], VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                  dst_stage_flags);
  }

  const uint32 num_fb_color_attachments = frame_buffer.getNumColorAttachments();
  ASSERT(render_pass.getNumColorAttachments() == num_fb_color_attachments);

  frame_buffer_ = frame_buffer;

  // transition all color attachment to `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`
  for (uint32 i = 0; i < num_fb_color_attachments; ++i) {
    if (TextureHandle handle = frame_buffer.color[i].texture) {
      VulkanImage* color_image = ctx_->texture_pool_.get(handle);
      _transition_to_color_attachment(wrapper_->cmdBuf, color_image);
    }

    // color attachment MSAA
    if (TextureHandle handle = frame_buffer.color[i].resolveTexture) {
      VulkanImage* color_resolve_image = ctx_->texture_pool_.get(handle);
      color_resolve_image->is_resolve_attachment_ = true;
      _transition_to_color_attachment(wrapper_->cmdBuf, color_resolve_image);
    }
  }

  // transition depth stencil attachment to
  // `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL`
  TextureHandle depth_texture_handle = frame_buffer.depthStencil.texture;
  if (depth_texture_handle) {
    const VulkanImage& depth_image = *ctx_->texture_pool_.get(depth_texture_handle);
    ASSERT(depth_image.format_ != VK_FORMAT_UNDEFINED,
           TEXT("Invalid depth attachment format"));
    ASSERT(depth_image.is_depth_format_, "Invalid depth attachment format");

    const VkImageAspectFlags aspect_flags = depth_image.getImageAspectFlags();
    depth_image.transitionLayout(
        wrapper_->cmdBuf, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkImageSubresourceRange{aspect_flags, 0, VK_REMAINING_MIP_LEVELS, 0,
                                VK_REMAINING_ARRAY_LAYERS});
  }

  // depth stencil MSAA
  if (TextureHandle handle = frame_buffer.depthStencil.resolveTexture) {
    VulkanImage& depth_resolve_image = *ctx_->texture_pool_.get(handle);
    ASSERT(depth_resolve_image.is_depth_format_, "Invalid depth attachment format");

    depth_resolve_image.is_resolve_attachment_ = true;
    const VkImageAspectFlags aspect_flags = depth_resolve_image.getImageAspectFlags();
    depth_resolve_image.transitionLayout(
        wrapper_->cmdBuf, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkImageSubresourceRange{aspect_flags, 0, VK_REMAINING_MIP_LEVELS, 0,
                                VK_REMAINING_ARRAY_LAYERS});
  }

  // calculate and transition input attachment
  {
    uint32 i = 0;
    if (deps.inputAttachments.size() <= MAX_COLOR_ATTACHMENTS) {
      cmdTransitionToRenderingLocalRead(deps.inputAttachments);
      for (; i < deps.inputAttachments.size(); ++i) {
        const TextureHandle handle = deps.inputAttachments[i];
        const VulkanImage* input_image = ctx_->texture_pool_.get(handle);

        ASSERT(input_image != nullptr);

        input_attachments_.imageInfos[i] = {
            .sampler = VK_NULL_HANDLE,
            .imageView = input_image->image_view_,
            .imageLayout = VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ,
        };
        input_attachments_.writeDstSet[i] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = VK_NULL_HANDLE,
            .dstBinding = i,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .pImageInfo = &input_attachments_.imageInfos[i],
        };
      }
    }
    input_attachments_.count = i;
  }

  VkSampleCountFlags color_attachment_sample_count = VK_SAMPLE_COUNT_1_BIT;
  uint32 mip_level = 0;
  uint32 frame_buffer_width = 0;
  uint32 frame_buffer_height = 0;

  VkRenderingAttachmentInfo color_attachment_infos[MAX_COLOR_ATTACHMENTS];

  for (uint32 i = 0; i < num_fb_color_attachments; ++i) {
    const Framebuffer::AttachmentDesc& attachment_desc = frame_buffer.color[i];
    ASSERT(!attachment_desc.texture.empty());

    VulkanImage& color_image = *ctx_->texture_pool_.get(attachment_desc.texture);
    const RenderPass::AttachmentDesc& rp_attachment_desc = render_pass.color[i];

    if (mip_level && rp_attachment_desc.level) {
      ASSERT(rp_attachment_desc.level == mip_level,
             TEXT("All color attachments should have the same mip-level"));
    }

    const VkExtent3D dim = color_image.extent_;
    if (frame_buffer_width) {
      ASSERT(dim.width == frame_buffer_width,
             TEXT("All attachments should have the same width"))
    }
    if (frame_buffer_height) {
      ASSERT(dim.height == frame_buffer_height,
             TEXT("All attachments should have the same height"))
    }

    mip_level = rp_attachment_desc.level;
    frame_buffer_width = dim.width;
    frame_buffer_height = dim.height;
    color_attachment_sample_count = color_image.sample_count_;

    color_attachment_infos[i] = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = color_image.getOrCreateVkImageViewForFramebuffer(
            *ctx_, rp_attachment_desc.level, rp_attachment_desc.layer, view_mask_),
        .imageLayout = color_image.layout_,
        .resolveMode = (color_attachment_sample_count > 1)
                           ? toVkResolveModeFlagBits(rp_attachment_desc.resolveMode,
                                                     VK_RESOLVE_MODE_FLAG_BITS_MAX_ENUM)
                           : VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = toVkAttachmentLoadOp(rp_attachment_desc.loadOp),
        .storeOp = toVkAttachmentStoreOp(rp_attachment_desc.storeOp),
    };

    memcpy(&color_attachment_infos[i].clearValue.color, &rp_attachment_desc.clearColor,
           sizeof(rp_attachment_desc.clearColor));

    // handle color attachment with MSAA
    if (attachment_desc.resolveTexture) {
      ASSERT(color_attachment_sample_count > 1,
             TEXT("If attachment resolveTexture is valid, "
                  "the sample count should larger than 1"));
      // @note: The `DONT_CARE` not needed by vulkan specs, if `DONT_CARE` flag set, the
      // driver will drop the miltisampled data and only keep resolved results on
      // tiled-based GPUs
      ASSERT(color_attachment_infos[i].storeOp == VK_ATTACHMENT_STORE_OP_DONT_CARE,
             TEXT("Multi-sampled attachments should have store op DONT_CARE"));

      VulkanImage& color_resolved_image =
          *ctx_->texture_pool_.get(attachment_desc.resolveTexture);
      color_attachment_infos[i].resolveImageView =
          color_resolved_image.getOrCreateVkImageViewForFramebuffer(
              *ctx_, rp_attachment_desc.level, rp_attachment_desc.layer, view_mask_);
      color_attachment_infos[i].resolveImageLayout =
          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
  }

  VkRenderingAttachmentInfo depth_attachment_info = {};

  if (frame_buffer.depthStencil.texture) {
    VulkanImage& depth_image =
        *ctx_->texture_pool_.get(frame_buffer.depthStencil.texture);

    const RenderPass::AttachmentDesc& rp_attachment_desc = render_pass.depth;

    ASSERT(rp_attachment_desc.level == mip_level,
           TEXT("Depth attachment should have the same mip-level as color attachmetns"));

    depth_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = depth_image.getOrCreateVkImageViewForFramebuffer(
            *ctx_, rp_attachment_desc.level, rp_attachment_desc.layer, view_mask_),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .resolveMode = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = toVkAttachmentLoadOp(rp_attachment_desc.loadOp),
        .storeOp = toVkAttachmentStoreOp(rp_attachment_desc.storeOp),
        .clearValue = {.depthStencil = {.depth = rp_attachment_desc.clearDepth,
                                        .stencil = rp_attachment_desc.clearStencil}},
    };

    // handle depth attachment with MSAA
    if (frame_buffer.depthStencil.resolveTexture) {
      ASSERT(depth_image.sample_count_ > 1, TEXT("The depth stencil has resolve texture, "
                                                 "sample count should larger than 1"));
      ASSERT(
          !num_fb_color_attachments ||
              depth_image.sample_count_ == color_attachment_sample_count,
          TEXT(
              "The depth stencil has resolve texture, the sample count shoule be same as "
              "color attachments"));

      const Framebuffer::AttachmentDesc& fb_attachment_desc = frame_buffer.depthStencil;
      ASSERT(!fb_attachment_desc.resolveTexture.empty(),
             TEXT("Framebuffer depth attachment should cantain a resolve texture"));

      VulkanImage& depth_resolve_image =
          *ctx_->texture_pool_.get(fb_attachment_desc.resolveTexture);
      depth_attachment_info.resolveImageView =
          depth_image.getOrCreateVkImageViewForFramebuffer(
              *ctx_, rp_attachment_desc.level, rp_attachment_desc.layer, view_mask_);
      depth_attachment_info.resolveImageLayout =
          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      depth_attachment_info.resolveMode = toVkResolveModeFlagBits(
          rp_attachment_desc.resolveMode,
          ctx_->physical_device_props12_.supportedDepthResolveModes);
    }

    const VkExtent3D dim = depth_image.extent_;
    if (frame_buffer_width) {
      ASSERT(dim.width == frame_buffer_width,
             TEXT("All attachments should have the same width"))
    }
    if (frame_buffer_height) {
      ASSERT(dim.height == frame_buffer_height,
             TEXT("All attachments should have the same height"))
    }

    mip_level = rp_attachment_desc.level;
    frame_buffer_width = dim.width;
    frame_buffer_height = dim.height;
  }

  const uint32 width = std::max(frame_buffer_width >> mip_level, 1u);
  const uint32 height = std::max(frame_buffer_height >> mip_level, 1u);
  const Viewport viewport{0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
  const ScissorRect scissor{0u, 0u, width, height};

  VkRenderingAttachmentInfo stencil_attachment_info = depth_attachment_info;

  const bool is_stencil_format = render_pass.stencil.loadOp != ELoadOp::DontCare ||
                                 render_pass.stencil.storeOp != EStoreOp::DontCare;

  // optional fragment density map ( VK_EXT_fragment_density_map )
  const VkRenderingFragmentDensityMapAttachmentInfoEXT
      fragment_density_map_attachment_info =  //
      [this, &frame_buffer]() {
        if (!frame_buffer.fragmentDensityMap) {
          return VkRenderingFragmentDensityMapAttachmentInfoEXT{};
        }

        ASSERT(ctx_->has_EXT_fragment_density_map_,
               TEXT("VK_EXT_fragment_density_map is not supported"));
        ASSERT(ctx_->fragment_density_map_features_.fragmentDensityMapNonSubsampledImages,
               TEXT("fragmentDensityMapNonSubsampledImages is required to use a fragment "
                    "density "
                    "map with vulkan non-subsampled "
                    "attachments"));

        const VulkanImage& fdm_image =
            *ctx_->texture_pool_.get(frame_buffer.fragmentDensityMap);
        ASSERT(fdm_image.usage_flags_ & VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT,
               "Fragment density map must be created with "
               "ETextureUsageBits::FragmentDensityMap");
        fdm_image.transitionLayout(
            wrapper_->cmdBuf, VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS,
                                    0, VK_REMAINING_ARRAY_LAYERS});

        return VkRenderingFragmentDensityMapAttachmentInfoEXT{
            .sType = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_INFO_EXT,
            .imageView = fdm_image.image_view_,
            .imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
        };
      }();

  // optional shading rate attachment ( VK_KHR_fragment_shading_rate )
  const VkRenderingFragmentShadingRateAttachmentInfoKHR shading_rate_info =
      [this, &frame_buffer]() {
        if (!frame_buffer.shadingRateAttachment) {
          return VkRenderingFragmentShadingRateAttachmentInfoKHR{};
        }

        const Dimensions& texel_size = frame_buffer.shadingRateAttachmentTexelSize;
        const Dimensions min_texel_size = ctx_->getShadingRateAttachmentMinTexelSize();
        const Dimensions max_texel_size = ctx_->getShadingRateAttachmentMaxTexelSize();
        ASSERT(texel_size.width >= min_texel_size.width &&
                   texel_size.width <= max_texel_size.width &&
                   texel_size.height >= min_texel_size.height &&
                   texel_size.height <= max_texel_size.height,
               TEXT("Framebuffer::shadingRateAttachmentTexelSize is outside min/max "
                    "FragmentShadingAttachmentTexelSize"));

        const VulkanImage& fsr_image =
            *ctx_->texture_pool_.get(frame_buffer.shadingRateAttachment);
        ASSERT(fsr_image.usage_flags_ &
                   VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
               TEXT("Shading rate attachment must be created with "
                    "ETextureUsageBits::ShadingRateAttachment"));

        fsr_image.transitionLayout(
            wrapper_->cmdBuf,
            VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
            VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS,
                                    0, VK_REMAINING_ARRAY_LAYERS});

        return VkRenderingFragmentShadingRateAttachmentInfoKHR{
            .sType =
                VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR,
            .imageView = fsr_image.image_view_,
            .imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR,
            .shadingRateAttachmentTexelSize = {texel_size.width, texel_size.height},
        };
      }();

  const VkRenderingInfo rendering_info{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = frame_buffer.fragmentDensityMap
                   ? (const void*)&fragment_density_map_attachment_info
                   : (frame_buffer.shadingRateAttachment ? &shading_rate_info : nullptr),
      .flags = 0,
      .renderArea = {VkOffset2D{(int32)scissor.x, (int32)scissor.y},
                     VkExtent2D{scissor.width, scissor.height}},
      .layerCount = render_pass.layerCount,
      .viewMask = render_pass.viewMask,
      .colorAttachmentCount = num_fb_color_attachments,
      .pColorAttachments = color_attachment_infos,
      .pDepthAttachment = depth_texture_handle ? &depth_attachment_info : nullptr,
      .pStencilAttachment = is_stencil_format ? &stencil_attachment_info : nullptr,
  };

  cmdBindViewport(viewport);
  cmdBindScissorRect(scissor);
  cmdBindDepthState({});

  ctx_->checkAndUpdateDescriptorSets();

  vkCmdSetDepthCompareOp(wrapper_->cmdBuf, VK_COMPARE_OP_ALWAYS);
  vkCmdSetDepthBiasEnable(wrapper_->cmdBuf, VK_FALSE);

  if (ctx_->has_KHR_fragment_shading_rate_) {
    // the dynamic state is enabled on every pipeline, so it must be set before any
    // draw; 1x1 is the full rate and is always supported
    cmdSetFragmentShadingRate({}, EShadingRateCombinerOp::Keep,
                              EShadingRateCombinerOp::Keep);
  }

  vkCmdBeginRendering(wrapper_->cmdBuf, &rendering_info);
}

void CommandBuffer::cmdEndRendering() {
  ASSERT(is_rendering_);
  is_rendering_ = false;

  vkCmdEndRendering(wrapper_->cmdBuf);

  frame_buffer_ = {};
}

void CommandBuffer::cmdNextSubpass() {
  ASSERT(is_rendering_);

  const VkMemoryBarrier2 memory_barrier{
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
      .dstAccessMask = VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
  };
  const VkDependencyInfo dep_info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
      .memoryBarrierCount = 1,
      .pMemoryBarriers = &memory_barrier,
  };

  vkCmdPipelineBarrier2(wrapper_->cmdBuf, &dep_info);
}

void CommandBuffer::cmdBindViewport(const Viewport& viewport) {
  // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
  const VkViewport vp = {
      .x = viewport.x,                    // float x;
      .y = viewport.height - viewport.y,  // float y;
      .width = viewport.width,            // float width;
      .height = -viewport.height,         // float height;
      .minDepth = viewport.minDepth,      // float minDepth;
      .maxDepth = viewport.maxDepth,      // float maxDepth;
  };
  vkCmdSetViewport(wrapper_->cmdBuf, 0, 1, &vp);
}

void CommandBuffer::cmdBindScissorRect(const ScissorRect& rect) {
  const VkRect2D scissor = {
      VkOffset2D{(int32_t)rect.x, (int32_t)rect.y},
      VkExtent2D{rect.width, rect.height},
  };
  vkCmdSetScissor(wrapper_->cmdBuf, 0, 1, &scissor);
}

void CommandBuffer::cmdBindRenderPipeline(RenderPipelineHandle handle) {
  if (handle.empty()) {
    return;
  }

  current_graphics_pipeline_ = handle;
  current_compute_pipeline_ = {};
  current_ray_tracing_pipeline_ = {};
  trackResource(handle);

  const RenderPipelineState* rps = ctx_->render_pipeline_pool_.get(handle);
  ASSERT(rps, TEXT("RenderPipelineState should in render pipeline pool"));

  const bool has_depth_attachment_pipeline = rps->desc.depthFormat != EFormat::NONE;
  const bool has_depth_attachment_pass = !frame_buffer_.depthStencil.texture.empty();

  ASSERT(has_depth_attachment_pipeline == has_depth_attachment_pass,
         TEXT("Make sure your render pass and render pipeline both have matching depth "
              "attachments"));

  VkPipeline pipeline = ctx_->getVkPipeline(handle, view_mask_);
  ASSERT(pipeline != VK_NULL_HANDLE);

  if (last_pipeline_bound_ != pipeline) {
    last_pipeline_bound_ = pipeline;

    vkCmdBindPipeline(wrapper_->cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    constexpr uint32 DESCRIPTOR_SET_INPUT_ATTACHMENTS = 1u;

    ctx_->bindDefaultDescriptorSets(wrapper_->cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    rps->pipelineLayout, wrapper_->handle,
                                    isComputeOnlyQueue());
    if (input_attachments_.count) {
      vkCmdPushDescriptorSetKHR(wrapper_->cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rps->pipelineLayout, DESCRIPTOR_SET_INPUT_ATTACHMENTS,
                                input_attachments_.count, input_attachments_.writeDstSet);
    }
  }
}

void CommandBuffer::cmdBindDepthState(const DepthState& state) {
  const VkCompareOp op = toVkCompareOp(state.compareOp);
  vkCmdSetDepthWriteEnable(wrapper_->cmdBuf,
                           state.bDepthWriteEnabled ? VK_TRUE : VK_FALSE);
  vkCmdSetDepthTestEnable(
      wrapper_->cmdBuf,
      (op != VK_COMPARE_OP_ALWAYS || state.bDepthWriteEnabled) ? VK_TRUE : VK_FALSE);

#if defined(ANDROID)
  // This is a workaround for the issue.
  // On Android (Mali-G715-Immortalis MC11
  // v1.r38p1-01eac0.c1a71ccca2acf211eb87c5db5322f569) if depth-stencil texture is not
  // set, call of vkCmdSetDepthCompareOp leads to disappearing of all content.
  if (!framebuffer_.depthStencil.texture) {
    return;
  }
#endif
  vkCmdSetDepthCompareOp(wrapper_->cmdBuf, op);
}

void CommandBuffer::cmdBindVertexBuffer(uint32 index, BufferHandle buffer_handle,
                                        uint64 offset, uint64 size) {
  if (buffer_handle.empty()) {
    return;
  }

  trackResource(buffer_handle);
  VulkanBuffer* buffer = ctx_->buffer_pool_.get(buffer_handle);
  ASSERT(buffer->usage_flags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  vkCmdBindVertexBuffers2(wrapper_->cmdBuf, index, 1, &buffer->vk_handle_, &offset, &size,
                          nullptr);
}

void CommandBuffer::cmdBindIndexBuffer(BufferHandle buffer_handle, EIndexFormat format,
                                       uint64 offset, uint64 size) {
  trackResource(buffer_handle);
  VulkanBuffer* buffer = ctx_->buffer_pool_.get(buffer_handle);
  ASSERT(buffer->usage_flags_ & VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  const VkIndexType type = toVkIndexType(format);
  vkCmdBindIndexBuffer2(wrapper_->cmdBuf, buffer->vk_handle_, offset, size, type);
}

void CommandBuffer::cmdPushConstants(const void* data, size_t size, size_t offset) {
  // VUID-vkCmdPushConstants-size-00369: size must be a multiple of 4
  ASSERT(size % 4 == 0, TEXT("The push constant buffer size must be a multiple of 4."));
  // check push constant size is within max size
  const VkPhysicalDeviceLimits& limits = ctx_->getVkPhysicalDeviceProperties().limits;
  ASSERT(size + offset <= limits.maxPushConstantsSize,
         TEXT("Push constants size exceeded %u (max %u bytes)"), size + offset,
         limits.maxPushConstantsSize);

  if (current_graphics_pipeline_.empty() && current_compute_pipeline_.empty() &&
      current_ray_tracing_pipeline_.empty()) {
    ASSERT(false, TEXT("No pipeline bound - cannot set push constants"));
    return;
  }

  const RenderPipelineState* rps =
      ctx_->render_pipeline_pool_.get(current_graphics_pipeline_);
  const ComputePipelineState* cps =
      ctx_->compute_pipeline_pool_.get(current_compute_pipeline_);
  const RayTracingPipelineState* rtps =
      ctx_->ray_tracing_pipeline_pool_.get(current_ray_tracing_pipeline_);

  ASSERT(rps || cps || rtps);

  VkPipelineLayout layout =
      rps ? rps->pipelineLayout : (cps ? cps->pipelineLayout : rtps->pipelineLayout);
  VkShaderStageFlags shader_stage_flags =
      rps ? rps->shaderStageFlags
          : (cps ? VK_SHADER_STAGE_COMPUTE_BIT : rtps->shaderStageFlags);

  vkCmdPushConstants(wrapper_->cmdBuf, layout, shader_stage_flags, (uint32)offset,
                     (uint32)size, data);
}

void CommandBuffer::cmdCopyBuffer(BufferHandle src_handle, BufferHandle dst_handle,
                                  size_t src_offset, size_t dst_offset, size_t size) {
  ASSERT(src_handle.valid());
  ASSERT(dst_handle.valid());
  ASSERT(size);

  // If the buffers have same index/gen, the copy region cannot be overlapped.
  if (src_handle == dst_handle) {
    if (src_offset < dst_offset) {
      ASSERT(dst_offset - src_offset >= size);
    } else {
      ASSERT(src_offset - dst_offset >= size);
    }
  }

  VulkanBuffer* src_buffer = ctx_->buffer_pool_.get(src_handle);
  VulkanBuffer* dst_buffer = ctx_->buffer_pool_.get(dst_handle);

  // TOOD: the buffer barrier will not be compled together with copy function from other
  // open-source engine.

  bufferBarrier(src_handle, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT);
  bufferBarrier(dst_handle, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT);

  const VkBufferCopy2 copy_region{
      .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
      .srcOffset = src_offset,
      .dstOffset = dst_offset,
      .size = size,
  };

  const VkCopyBufferInfo2 copy_buffer_info2{
      .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
      .srcBuffer = src_buffer->vk_handle_,
      .dstBuffer = dst_buffer->vk_handle_,
      .regionCount = 1,
      .pRegions = &copy_region,
  };

  vkCmdCopyBuffer2(wrapper_->cmdBuf, &copy_buffer_info2);

  VkPipelineStageFlags2 src_stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  if (src_buffer->usage_flags_ & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
    src_stage |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  }
  if (src_buffer->usage_flags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
    src_stage |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
  }

  VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  if (dst_buffer->usage_flags_ & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
    dst_stage |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  }
  if (dst_buffer->usage_flags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
    dst_stage |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
  }
  if (dst_buffer->usage_flags_ & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
    dst_stage |= VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
  }

  bufferBarrier(src_handle, VK_PIPELINE_STAGE_2_TRANSFER_BIT, src_stage);
  bufferBarrier(dst_handle, VK_PIPELINE_STAGE_2_TRANSFER_BIT, dst_stage);
}

void CommandBuffer::cmdFillBuffer(BufferHandle handle, size_t offset, size_t size,
                                  uint32 data) {
  ASSERT(handle.valid());
  ASSERT(size);
  ASSERT((size == VK_WHOLE_SIZE) || (size % 4 == 0));
  ASSERT(offset % 4 == 0);

  VulkanBuffer* buffer = ctx_->buffer_pool_.get(handle);
  bufferBarrier(handle, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT);

  vkCmdFillBuffer(wrapper_->cmdBuf, buffer->vk_handle_, offset, size, data);

  VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  if (buffer->usage_flags_ & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
    dst_stage |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  }
  if (buffer->usage_flags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
    dst_stage |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
  }
  bufferBarrier(handle, VK_PIPELINE_STAGE_2_TRANSFER_BIT, dst_stage);
}

void CommandBuffer::cmdUpdateBuffer(BufferHandle handle, size_t offset, size_t size,
                                    const void* data) {
  ASSERT(handle.valid());
  ASSERT(data);
  ASSERT(size && size <= 65536);
  ASSERT(size % 4 == 0);
  ASSERT(offset % 4 == 0);

  VulkanBuffer* buffer = ctx_->buffer_pool_.get(handle);
  bufferBarrier(handle, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT);

  vkCmdUpdateBuffer(wrapper_->cmdBuf, buffer->vk_handle_, offset, size, data);

  VkPipelineStageFlags2 dst_stage = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  if (buffer->usage_flags_ & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) {
    dst_stage |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  }
  if (buffer->usage_flags_ & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) {
    dst_stage |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
  }
  bufferBarrier(handle, VK_PIPELINE_STAGE_2_TRANSFER_BIT, dst_stage);
}

void CommandBuffer::cmdDraw(uint32 vertex_count, uint32 instance_count,
                            uint32 first_vertex, uint32 base_instance) {
  if (vertex_count == 0) {
    return;
  }

  vkCmdDraw(wrapper_->cmdBuf, vertex_count, instance_count, first_vertex, base_instance);
}

void CommandBuffer::cmdDrawIndexed(uint32 index_count, uint32 instance_count,
                                   uint32 first_index, int32_t vertex_offset,
                                   uint32 base_instance) {
  if (index_count == 0) {
    return;
  }

  ASSERT(!ctx_->awaiting_descriptor_update_);
  vkCmdDrawIndexed(wrapper_->cmdBuf, index_count, instance_count, first_index,
                   vertex_offset, base_instance);
}

void CommandBuffer::cmdDrawIndirect(BufferHandle indirect_buffer,
                                    size_t indirect_buffer_offset, uint32 draw_count,
                                    uint32 stride) {
  trackResource(indirect_buffer);
  VulkanBuffer* buffer = ctx_->buffer_pool_.get(indirect_buffer);
  ASSERT(buffer);
  vkCmdDrawIndirect(wrapper_->cmdBuf, buffer->vk_handle_, indirect_buffer_offset,
                    draw_count, stride ? stride : sizeof(VkDrawIndirectCommand));
}

void CommandBuffer::cmdDrawIndexedIndirect(BufferHandle indirect_buffer,
                                           size_t indirect_buffer_offset,
                                           uint32 draw_count, uint32 stride) {
  trackResource(indirect_buffer);
  VulkanBuffer* buffer = ctx_->buffer_pool_.get(indirect_buffer);
  ASSERT(buffer);
  vkCmdDrawIndexedIndirect(wrapper_->cmdBuf, buffer->vk_handle_, indirect_buffer_offset,
                           draw_count, stride ? stride : sizeof(VkDrawIndirectCommand));
}

void CommandBuffer::cmdDrawIndexedIndirectCount(BufferHandle indirect_buffer_handle,
                                                size_t indirect_buffer_offset,
                                                BufferHandle count_buffer_handle,
                                                size_t count_buffer_Offset,
                                                uint32 max_draw_count, uint32 stride) {
  trackResource(indirect_buffer_handle);
  trackResource(count_buffer_handle);
  VulkanBuffer* indirect_buffer = ctx_->buffer_pool_.get(indirect_buffer_handle);
  VulkanBuffer* count_buffer = ctx_->buffer_pool_.get(count_buffer_handle);
  ASSERT(indirect_buffer);
  ASSERT(count_buffer);

  vkCmdDrawIndexedIndirectCount(wrapper_->cmdBuf, indirect_buffer->vk_handle_,
                                indirect_buffer_offset, count_buffer->vk_handle_,
                                count_buffer_Offset, max_draw_count,
                                stride ? stride : sizeof(VkDrawIndexedIndirectCommand));
}

void CommandBuffer::cmdDrawMeshTasks(const Dimensions& thread_group_count) {
  ASSERT(ctx_->has_EXT_mesh_shader_, TEXT("Mesh shaders not supported"));

  vkCmdDrawMeshTasksEXT(wrapper_->cmdBuf, thread_group_count.width,
                        thread_group_count.height, thread_group_count.depth);
}

void CommandBuffer::cmdDrawMeshTasksIndirect(BufferHandle indirect_buffer,
                                             size_t indirect_buffer_offset,
                                             uint32 draw_count, uint32 stride) {
  ASSERT(ctx_->has_EXT_mesh_shader_, TEXT("Mesh shaders not supported"));

  trackResource(indirect_buffer);
  VulkanBuffer* buffer = ctx_->buffer_pool_.get(indirect_buffer);
  ASSERT(buffer);
  vkCmdDrawMeshTasksIndirectEXT(
      wrapper_->cmdBuf, buffer->vk_handle_, indirect_buffer_offset, draw_count,
      stride ? stride : sizeof(VkDrawMeshTasksIndirectCommandEXT));
}

void CommandBuffer::cmdDrawMeshTasksIndirectCount(BufferHandle indirect_buffer_handle,
                                                  size_t indirect_buffer_offset,
                                                  BufferHandle count_buffer_handle,
                                                  size_t count_buffer_offset,
                                                  uint32 max_draw_count, uint32 stride) {
  ASSERT(ctx_->has_EXT_mesh_shader_, TEXT("Mesh shaders not supported"));

  trackResource(indirect_buffer_handle);
  trackResource(count_buffer_handle);
  VulkanBuffer* indirect_buffer = ctx_->buffer_pool_.get(indirect_buffer_handle);
  VulkanBuffer* count_buffer = ctx_->buffer_pool_.get(count_buffer_handle);
  ASSERT(indirect_buffer);
  ASSERT(count_buffer);

  vkCmdDrawMeshTasksIndirectCountEXT(
      wrapper_->cmdBuf, indirect_buffer->vk_handle_, indirect_buffer_offset,
      count_buffer->vk_handle_, count_buffer_offset, max_draw_count,
      stride ? stride : sizeof(VkDrawMeshTasksIndirectCommandEXT));
}

void CommandBuffer::cmdTraceRays(uint32 width, uint32 height, uint32 depth,
                                 const Dependencies& deps) {
  RayTracingPipelineState* rtps =
      ctx_->ray_tracing_pipeline_pool_.get(current_ray_tracing_pipeline_);
  if (!rtps) {
    return;
  }

  ASSERT(!is_rendering_);

  trackResources(deps);
  addCrossQueueDependencies(deps);
  cmdTransitionToShaderReadOnly(deps.sampledImages, EShaderStage::RayGeneration);
  cmdTransitionToGeneral(deps.storageImages, EShaderStage::RayGeneration);

  for (size_t i = 0; i < deps.buffers.size(); ++i) {
    bufferBarrier(
        deps.buffers[i],
        VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR);
  }

  vkCmdTraceRaysKHR(wrapper_->cmdBuf, &rtps->sbtEntryRayGen, &rtps->sbtEntryMiss,
                    &rtps->sbtEntryHit, &rtps->sbtEntryCallable, width, height, depth);
}

void CommandBuffer::cmdSetBlendColor(const float color[4]) {
  vkCmdSetBlendConstants(wrapper_->cmdBuf, color);
}

void CommandBuffer::cmdSetDepthBias(float constant_factor, float slope_factor,
                                    float clamp) {
  vkCmdSetDepthBias(wrapper_->cmdBuf, constant_factor, clamp, slope_factor);
}

void CommandBuffer::cmdSetDepthBiasEnable(bool enable) {
  vkCmdSetDepthBiasEnable(wrapper_->cmdBuf, enable ? VK_TRUE : VK_FALSE);
}

void CommandBuffer::cmdSetFragmentShadingRate(const Dimensions& fragment_size,
                                              EShadingRateCombinerOp primitive_op,
                                              EShadingRateCombinerOp attachment_op) {
  ASSERT(ctx_->has_KHR_fragment_shading_rate_,
         "VK_KHR_fragment_shading_rate is not enabled (see "
         "`ContextConfig::enableFragmentShadingRate`)");
  ASSERT(fragment_size.depth == 1, "The fragment shading rate is 2D: `depth` must be 1");
  // `VkPhysicalDeviceFragmentShadingRateKHR::sampleCounts` restricts some fragment
  // sizes to fewer samples (e.g. 4x4 to 1 sample). It is not checked here because the
  // pipeline used for the draw is not known yet - this is dynamic state
  ASSERT(
      std::find_if(ctx_->device_fragment_shading_rates_.cbegin(),
                   ctx_->device_fragment_shading_rates_.cend(),
                   [&fragment_size](const VkPhysicalDeviceFragmentShadingRateKHR& rate) {
                     return rate.fragmentSize.width == fragment_size.width &&
                            rate.fragmentSize.height == fragment_size.height;
                   }) != ctx_->device_fragment_shading_rates_.cend(),
      "This fragment size is not in vkGetPhysicalDeviceFragmentShadingRatesKHR()");

  const VkExtent2D vk_fragment_size = {fragment_size.width, fragment_size.height};
  const VkFragmentShadingRateCombinerOpKHR combiner_ops[2] = {
      (VkFragmentShadingRateCombinerOpKHR)primitive_op,
      (VkFragmentShadingRateCombinerOpKHR)attachment_op,
  };

  vkCmdSetFragmentShadingRateKHR(wrapper_->cmdBuf, &vk_fragment_size, combiner_ops);
}

void CommandBuffer::cmdResetQueryPool(QueryPoolHandle pool, uint32 firstQuery,
                                      uint32 queryCount) {
  trackResource(pool);
  VkQueryPool query_pool = *ctx_->query_pool_.get(pool);
  vkCmdResetQueryPool(wrapper_->cmdBuf, query_pool, firstQuery, queryCount);
}

void CommandBuffer::cmdWriteTimestamp(QueryPoolHandle pool, uint32 query) {
  trackResource(pool);
  VkQueryPool query_pool = *ctx_->query_pool_.get(pool);
  vkCmdWriteTimestamp2(wrapper_->cmdBuf, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                       query_pool, query);
}

void CommandBuffer::cmdClearColorImage(TextureHandle tex, const ClearColorValue& value,
                                       const TextureLayers& layers) {
  static_assert(sizeof(ClearColorValue) == sizeof(VkClearColorValue));

  trackResource(tex);
  VulkanImage* image = ctx_->texture_pool_.get(tex);
  if (!image) {
    return;
  }

  const VkImageSubresourceRange range{
      .aspectMask = image->getImageAspectFlags(),
      .baseMipLevel = layers.mipLevel,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = layers.layer,
      .layerCount = layers.numLayers,
  };

  imageMemoryBarrier2(
      wrapper_->cmdBuf, image->vk_handle_,
      StageAccess{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                  .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT},
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
      image->layout_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, range);

  vkCmdClearColorImage(wrapper_->cmdBuf, image->vk_handle_,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       reinterpret_cast<const VkClearColorValue*>(&value), 1, &range);

  const VkImageLayout new_layout =
      image->layout_ == VK_IMAGE_LAYOUT_UNDEFINED
          ? (image->isAttachment()     ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
             : image->isSampledImage() ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                       : VK_IMAGE_LAYOUT_GENERAL)
          : image->layout_;

  imageMemoryBarrier2(
      wrapper_->cmdBuf, image->vk_handle_,
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
      StageAccess{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                  .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT},
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, new_layout, range);

  image->layout_ = new_layout;
}

void CommandBuffer::cmdCopyImage(TextureHandle src, TextureHandle dst,
                                 const Dimensions& extent, const Offset3D& srcOffset,
                                 const Offset3D& dstOffset,
                                 const TextureLayers& srcLayers,
                                 const TextureLayers& dstLayers) {
  trackResource(src);
  trackResource(dst);
  VulkanImage* src_image = ctx_->texture_pool_.get(src);
  VulkanImage* dst_image = ctx_->texture_pool_.get(dst);

  ASSERT(src_image && dst_image);
  ASSERT(srcLayers.numLayers == dstLayers.numLayers);

  if (!src_image || !dst_image) {
    return;
  }

  const VkImageSubresourceRange src_range{
      .aspectMask = src_image->getImageAspectFlags(),
      .baseMipLevel = srcLayers.mipLevel,
      .levelCount = 1,
      .baseArrayLayer = srcLayers.layer,
      .layerCount = srcLayers.numLayers,
  };

  const VkImageSubresourceRange dst_range{
      .aspectMask = dst_image->getImageAspectFlags(),
      .baseMipLevel = dstLayers.mipLevel,
      .levelCount = 1,
      .baseArrayLayer = dstLayers.layer,
      .layerCount = dstLayers.numLayers,
  };

  ASSERT(src_image->layout_ != VK_IMAGE_LAYOUT_UNDEFINED);

  const VkExtent3D dst_extent = dst_image->extent_;
  const bool covers_full_dst = dst_extent.width == extent.width &&
                               dst_extent.height == extent.height &&
                               dst_extent.depth == extent.depth && dstOffset.x == 0 &&
                               dstOffset.y == 0 && dstOffset.z == 0;

  // NOTE: should cover full destination image
  ASSERT(covers_full_dst && dst_image->layout_ != VK_IMAGE_LAYOUT_UNDEFINED);

  imageMemoryBarrier2(
      wrapper_->cmdBuf, src_image->vk_handle_,
      StageAccess{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                  .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT},
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_READ_BIT},
      src_image->layout_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_range);

  imageMemoryBarrier2(
      wrapper_->cmdBuf, dst_image->vk_handle_,
      StageAccess{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                  .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT},
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
      covers_full_dst ? VK_IMAGE_LAYOUT_UNDEFINED : dst_image->layout_,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dst_range);

  const VkImageCopy2 region_copy{
      .sType = VK_STRUCTURE_TYPE_IMAGE_COPY_2,
      .srcSubresource = {.aspectMask = src_image->getImageAspectFlags(),
                         .mipLevel = srcLayers.mipLevel,
                         .baseArrayLayer = srcLayers.layer,
                         .layerCount = srcLayers.numLayers},
      .srcOffset = {.x = srcOffset.x, .y = srcOffset.y, .z = srcOffset.z},
      .dstSubresource = {.aspectMask = dst_image->getImageAspectFlags(),
                         .mipLevel = dstLayers.mipLevel,
                         .baseArrayLayer = dstLayers.layer,
                         .layerCount = dstLayers.numLayers},
      .dstOffset = {.x = dstOffset.x, .y = dstOffset.y, .z = dstOffset.z},
      .extent = {.width = extent.width, .height = extent.height, .depth = extent.depth},
  };

  const VkImageBlit2 region_blit{
      .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
      .srcSubresource = region_copy.srcSubresource,
      .srcOffsets = {{},
                     {.x = int32(srcOffset.x + src_image->extent_.width),
                      .y = int32(srcOffset.y + src_image->extent_.height),
                      .z = int32(srcOffset.z + src_image->extent_.depth)}},
      .dstSubresource = region_copy.dstSubresource,
      .dstOffsets = {{},
                     {.x = int32(dstOffset.x + dst_image->extent_.width),
                      .y = int32(dstOffset.y + dst_image->extent_.height),
                      .z = int32(dstOffset.z + dst_image->extent_.depth)}},
  };

  const bool is_compatible =
      getBytesPerPixel(src_image->format_) == getBytesPerPixel(dst_image->format_);
  if (is_compatible) {
    vkCmdCopyImage2(wrapper_->cmdBuf,
                    &(const VkCopyImageInfo2&)VkCopyImageInfo2{
                        .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
                        .srcImage = src_image->vk_handle_,
                        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .dstImage = dst_image->vk_handle_,
                        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .regionCount = 1,
                        .pRegions = &region_copy,
                    });
  } else {
    vkCmdBlitImage2(wrapper_->cmdBuf,
                    &(const VkBlitImageInfo2&)VkBlitImageInfo2{
                        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                        .srcImage = src_image->vk_handle_,
                        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .dstImage = dst_image->vk_handle_,
                        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .regionCount = 1,
                        .pRegions = &region_blit,
                        .filter = VK_FILTER_LINEAR,
                    });
  }

  imageMemoryBarrier2(
      wrapper_->cmdBuf, src_image->vk_handle_,
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_READ_BIT},
      StageAccess{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                  .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT},
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, src_image->layout_, src_range);

  const VkImageLayout new_layout =
      dst_image->layout_ == VK_IMAGE_LAYOUT_UNDEFINED
          ? (dst_image->isAttachment()     ? VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
             : dst_image->isSampledImage() ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                           : VK_IMAGE_LAYOUT_GENERAL)
          : dst_image->layout_;

  imageMemoryBarrier2(wrapper_->cmdBuf, dst_image->vk_handle_,
                      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                  .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
                      StageAccess{.stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                  .access = VK_ACCESS_2_MEMORY_READ_BIT},
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, new_layout, dst_range);

  dst_image->layout_ = new_layout;
}

void CommandBuffer::cmdGenerateMipmap(TextureHandle handle) {
  if (handle.empty()) {
    return;
  }

  trackResource(handle);
  VulkanImage* image = ctx_->texture_pool_.get(handle);
  if (image->num_levels_ <= 1) {
    return;
  }

  ASSERT(image->layout_ != VK_IMAGE_LAYOUT_UNDEFINED, TEXT("Image layout is undefined"));
  image->generateMipmap(wrapper_->cmdBuf);
}

void CommandBuffer::cmdUpdateTLAS(AccelerationStructHandle handle,
                                  BufferHandle instances_buffer) {
  if (handle.empty()) {
    return;
  }

  trackResource(handle);
  trackResource(instances_buffer);
  AccelerationStructure* as = ctx_->acceleration_structure_pool_.get(handle);
  const VkAccelerationStructureGeometryKHR as_geom{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry =
          {
              .instances =
                  {
                      .sType =
                          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                      .arrayOfPointers = VK_FALSE,
                      .data = {.deviceAddress = ctx_->gpuAddress(instances_buffer, 0)},
                  },
          },
      .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
  };
  VkAccelerationStructureBuildGeometryInfoKHR as_build_geom_info{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .geometryCount = 1,
      .pGeometries = &as_geom,
  };
  VkAccelerationStructureBuildSizesInfoKHR as_build_sizes_info{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };
  vkGetAccelerationStructureBuildSizesKHR(
      ctx_->device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &as_build_geom_info,
      &as->buildRangeInfo.primitiveCount, &as_build_sizes_info);

  const uint32 alignment = ctx_->acceleration_structure_properties_
                               .minAccelerationStructureScratchOffsetAlignment;
  as_build_sizes_info.accelerationStructureSize += alignment;
  as_build_sizes_info.updateScratchSize += alignment;
  as_build_sizes_info.buildScratchSize += alignment;

  if (!as->scratchBuffer.valid() ||
      getBufferSize(ctx_, as->scratchBuffer) < as_build_sizes_info.updateScratchSize) {
    as->scratchBuffer = ctx_->createBuffer(
        BufferDesc{
            .usage = static_cast<uint8>(EBufferUsageBits::Storage),
            .storageType = EStorageType::Device,
            .size = as_build_sizes_info.updateScratchSize,
            .debugName = "ScratchBuffer",
        },
        nullptr, nullptr);
  }

  const VkAccelerationStructureBuildGeometryInfoKHR as_build_geom_info2{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR,
      .srcAccelerationStructure = as->vkHandle,
      .dstAccelerationStructure = as->vkHandle,
      .geometryCount = 1,
      .pGeometries = &as_geom,
      .scratchData =
          {
              .deviceAddress =
                  Align<uint64>(ctx_->gpuAddress(as->scratchBuffer, 0),
                                ctx_->acceleration_structure_properties_
                                    .minAccelerationStructureScratchOffsetAlignment),
          },
  };
  const VkAccelerationStructureBuildRangeInfoKHR* as_build_range_infos[] = {
      &as->buildRangeInfo};

  {
    const VkBufferMemoryBarrier2 barriers[] = {
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            .buffer = getVkBuffer(ctx_, handle),
            .size = VK_WHOLE_SIZE,
        },
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .buffer = getVkBuffer(ctx_, handle),
            .size = VK_WHOLE_SIZE,
        },
    };
    const VkDependencyInfo dep_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = ARRAY_COUNT(barriers),
        .pBufferMemoryBarriers = barriers,
    };
    vkCmdPipelineBarrier2(wrapper_->cmdBuf, &dep_info);
  }
  vkCmdBuildAccelerationStructuresKHR(wrapper_->cmdBuf, 1, &as_build_geom_info,
                                      as_build_range_infos);
  {
    const VkBufferMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .buffer = getVkBuffer(ctx_, handle),
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo dep_info{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(wrapper_->cmdBuf, &dep_info);
  }
}

void CommandBuffer::bufferBarrier(BufferHandle handle, VkPipelineStageFlags2 src_stage,
                                  VkPipelineStageFlags2 dst_stage, VkDeviceSize offset,
                                  VkDeviceSize size) {
  trackResource(handle);
  VulkanBuffer* buffer = ctx_->buffer_pool_.get(handle);
  ASSERT(buffer);

  const bool compute_only_queue = isComputeOnlyQueue();
  src_stage = stripGraphicsStages(src_stage, compute_only_queue);
  dst_stage = stripGraphicsStages(dst_stage, compute_only_queue);

  VkBufferMemoryBarrier2 barrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR,
      .srcStageMask = src_stage,
      .srcAccessMask = 0,
      .dstStageMask = dst_stage,
      .dstAccessMask = 0,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = buffer->vk_handle_,
      .offset = offset,
      .size = size,
  };

  const VkPipelineStageFlags2 non_shader_stages =
      VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT |
      VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
  const VkPipelineStageFlags2 index_read_stages =
      VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

  if (src_stage & VK_PIPELINE_STAGE_2_TRANSFER_BIT) {
    barrier.srcAccessMask |=
        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
  }
  if (buffer->usage_flags_ & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) {
    if (src_stage & index_read_stages) {
      barrier.srcAccessMask |= VK_ACCESS_2_INDEX_READ_BIT;
    }
  }
  if (src_stage & ~non_shader_stages) {
    barrier.srcAccessMask |= VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
  }

  if (dst_stage & VK_PIPELINE_STAGE_2_TRANSFER_BIT) {
    barrier.dstAccessMask |=
        VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
  }
  if (dst_stage & ~non_shader_stages) {
    barrier.dstAccessMask |= VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
  }

  // The indirect draw args are read by GPU command processing, not by shader.
  if (dst_stage & VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT) {
    barrier.dstAccessMask |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
  }

  if ((buffer->usage_flags_ & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) &&
      (dst_stage & index_read_stages)) {
    barrier.dstAccessMask |= VK_ACCESS_2_INDEX_READ_BIT;
  }

  const VkDependencyInfo dep_info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(wrapper_->cmdBuf, &dep_info);
}

void CommandBuffer::addCrossQueueDependencies(const Dependencies& deps) {
  if (!ctx_->immediate_compute_) {
    return;
  }

  for (SubmitHandle dep : deps.waitCompute) {
    ASSERT(dep.empty() ||
               dep.getQueueFamilyIndex() == ctx_->device_queues_.computeQueueFamilyIndex,
           TEXT("Dependencies::waitCompute expects SubmitHandles produced by "
                "acquireCommandBuffer(dedicatedCompute = true)"));
  }

  for (SubmitHandle dep : deps.waitGraphics) {
    ASSERT(dep.empty() ||
               dep.getQueueFamilyIndex() == ctx_->device_queues_.graphicsQueueFamilyIndex,
           TEXT("Dependencies::waitGraphics expects SubmitHandles produced by "
                "acquireCommandBuffer(dedicatedCompute = false)"));
  }

  const bool is_compute = immediate_ == ctx_->immediate_compute_.get();
  const Span<SubmitHandle> cross_deps = is_compute ? deps.waitGraphics : deps.waitCompute;
  VulkanImmediateCommands& other =
      is_compute ? *ctx_->immediate_ : *ctx_->immediate_compute_;
  uint64& wait_value =
      is_compute ? cross_queue_graphics_wait_value_ : cross_queue_compute_wait_value_;

  for (SubmitHandle dep : cross_deps) {
    if (dep.empty()) {
      continue;
    }

    wait_value = std::max<uint64>(wait_value, other.getTimelineValue(dep));
  }
}

bool CommandBuffer::acquireOwnershipIfPending(VulkanImage& image, StageAccess dst) const {
  if (image.pending_acquire_src_family_ == VK_QUEUE_FAMILY_IGNORED ||
      image.pending_acquire_src_family_ == queue_family_index_) {
    return false;
  }

  // acquire half of a cross-queue ownship transfer: it must reply the producer's
  // release layouts(src/dst) exactly
  dst.stage = stripGraphicsStages(dst.stage, isComputeOnlyQueue());

  emitImageQueueFamilyOwnershipTransfer(
      wrapper_->cmdBuf, image, image.qfot_src_layout_, image.qfot_dst_layout_,
      StageAccess{}, dst, image.pending_acquire_src_family_, queue_family_index_);

  image.pending_acquire_src_family_ = VK_QUEUE_FAMILY_IGNORED;
  image.owner_queue_family_ = queue_family_index_;
  image.layout_ = image.qfot_dst_layout_;

  return true;
}

bool CommandBuffer::isComputeOnlyQueue() const {
  return ctx_->immediate_compute_ && immediate_ == ctx_->immediate_compute_.get();
}

}  // namespace vk
}  // namespace amaryl

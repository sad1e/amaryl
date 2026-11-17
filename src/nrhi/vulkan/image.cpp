#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

namespace vk {
VkImageView VulkanImage::createImageView(VkDevice device, VkImageViewType image_view_type,
                                         VkFormat format, VkImageAspectFlags aspect_flags,
                                         uint32 base_level, uint32 num_levels,
                                         uint32 base_layer, uint32 num_layers,
                                         const VkComponentMapping component_mapping,
                                         const VkSamplerYcbcrConversion* ycbr_conversion,
                                         const char* debug_name) {
  const VkImageViewCreateInfo ci{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = ycbr_conversion,
      .image = vk_handle_,
      .viewType = image_view_type,
      .format = format,
      .components = component_mapping,
      .subresourceRange =
          {
              .aspectMask = aspect_flags,
              .baseMipLevel = base_level,
              .levelCount = num_levels ? num_levels : num_levels_,
              .baseArrayLayer = base_layer,
              .layerCount = num_layers,
          },
  };

  VkImageView image_view = VK_NULL_HANDLE;

  VK_CHECK(vkCreateImageView(device, &ci, nullptr, &image_view));
  VK_CHECK(setDebugObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64)image_view,
                              debug_name));

  return image_view;
}

void VulkanImage::generateMipmap(VkCommandBuffer vk_cmd_buffer) const {
  {
    constexpr uint32 format_feature_mask =
        VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const bool hw_downscaling_supported = (format_properties_.optimalTilingFeatures &
                                           format_feature_mask) == format_feature_mask;

    if (!hw_downscaling_supported) {
      ALOGW(TEXT("Target format %p doesn't support hardware downscaling."), format_);
      return;
    }
  }

  // Get the filter type of specified format
  const VkFilter filter =
      [](bool is_depth_or_stencil_format, bool image_filter_linear) {
        if (is_depth_or_stencil_format) {
          return VK_FILTER_NEAREST;
        }
        if (image_filter_linear) {
          return VK_FILTER_LINEAR;
        }
        return VK_FILTER_NEAREST;
      }(is_depth_format_ || is_stencil_format_,
        format_properties_.optimalTilingFeatures &
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);

  const VkImageAspectFlags image_aspect = getImageAspectFlags();

  if (vkCmdBeginDebugUtilsLabelEXT != nullptr) {
    constexpr VkDebugUtilsLabelEXT label{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = "GenerateMipmaps",
        .color = {1.0f, 0.75f, 1.0f, 1.0f},
    };
    vkCmdBeginDebugUtilsLabelEXT(vk_cmd_buffer, &label);
  }

  const VkImageLayout original_image_layout = layout_;
  ASSERT(original_image_layout != VK_IMAGE_LAYOUT_UNDEFINED,
         TEXT("Original image layout is undefined."));

  // 1. Transition the first level and all layeres into
  // `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
  transitionLayout(vk_cmd_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   {
                       .aspectMask = image_aspect,
                       .baseMipLevel = 0,
                       .levelCount = 1,
                       .baseArrayLayer = 0,
                       .layerCount = num_layers_,
                   });

  for (uint32 layer = 0; layer < num_layers_; ++layer) {
    int32 mipw = (int32)extent_.width;
    int32 miph = (int32)extent_.height;
    int32 mipd = (int32)extent_.depth;

    for (uint32 i = 1; i < num_levels_; ++i) {
      // 2. Transition the i-th level to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`, it will
      // be copied into from the (i-1)th layer.
      imageMemoryBarrier2(vk_cmd_buffer, vk_handle_,
                          StageAccess{.stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                      .access = VK_ACCESS_2_NONE},
                          StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
                          VK_IMAGE_LAYOUT_UNDEFINED,             // old image layout
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // new image layout
                          VkImageSubresourceRange{
                              .aspectMask = image_aspect,
                              .baseMipLevel = i,
                              .levelCount = 1,
                              .baseArrayLayer = layer,
                              .layerCount = 1,
                          });

      // 3. Blit each mip
      const int32 next_mipw = mipw > 1 ? mipw / 2 : 1;
      const int32 next_miph = miph > 1 ? miph / 2 : 1;
      const int32 next_mipd = mipd > 1 ? mipd / 2 : 1;

      const VkOffset3D src_offsets[2] = {VkOffset3D{0, 0, 0},
                                         VkOffset3D{mipw, miph, mipd}};
      const VkOffset3D dst_offsets[2] = {VkOffset3D{0, 0, 0},
                                         VkOffset3D{next_mipw, next_miph, next_mipd}};

      const VkImageBlit2 image_blit2{
          .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
          .srcSubresource = VkImageSubresourceLayers{image_aspect, i - 1, layer, 1},
          .srcOffsets = {src_offsets[0], src_offsets[1]},
          .dstSubresource = VkImageSubresourceLayers{image_aspect, i, layer, 1},
          .dstOffsets = {dst_offsets[0], dst_offsets[1]},
      };

      const VkBlitImageInfo2 blit_image_info2{
          .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
          .srcImage = vk_handle_,
          .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
          .dstImage = vk_handle_,
          .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .regionCount = 1,
          .pRegions = &image_blit2,
          .filter = filter,
      };

      vkCmdBlitImage2(vk_cmd_buffer, &blit_image_info2);

      // 4. Transition i-th level to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` as it will
      // be read from in the next iteration
      imageMemoryBarrier2(vk_cmd_buffer, vk_handle_,
                          StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                      .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
                          StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                      .access = VK_ACCESS_2_TRANSFER_READ_BIT},
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VkImageSubresourceRange{image_aspect, i, 1, layer, 1});

      mipw = next_mipw;
      miph = next_miph;
      mipd = next_mipd;
    }
  }

  // 5. Transition the whole image to the final layout
  imageMemoryBarrier2(
      vk_cmd_buffer, vk_handle_,
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_WRITE_BIT},
      StageAccess{.stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  .access = VK_ACCESS_2_TRANSFER_READ_BIT},
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,  //
      original_image_layout,                 //
      VkImageSubresourceRange{image_aspect, 0, num_levels_, 0, num_layers_});

  if (vkCmdEndDebugUtilsLabelEXT) {
    vkCmdEndDebugUtilsLabelEXT(vk_cmd_buffer);
  }

  layout_ = original_image_layout;
}

void VulkanImage::transitionLayout(VkCommandBuffer vk_cmd_buffer,
                                   VkImageLayout new_image_layout,
                                   const VkImageSubresourceRange& subresource_range,
                                   StageAccess extra_dst_stage,
                                   bool compute_only_queue) const {
  const VkImageLayout old_image_layout =
      layout_ == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
          ? (isDepthAttachment() ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                                 : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
          : layout_;

  if (new_image_layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) {
    new_image_layout = isDepthAttachment()
                           ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                           : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  }

  StageAccess src = getPipelineStageAccess(old_image_layout);
  StageAccess dst = getPipelineStageAccess(new_image_layout);

  dst.stage |= extra_dst_stage.stage;
  dst.access |= extra_dst_stage.access;

  src.stage = stripGraphicsStages(src.stage, compute_only_queue);
  dst.stage = stripGraphicsStages(dst.stage, compute_only_queue);

  if (isDepthAttachment() && is_resolve_attachment_) {
    // https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#renderpass-resolve-operations
    src.stage |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    dst.stage |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    src.access |=
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    dst.access |=
        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  }

  const VkImageMemoryBarrier2 barrier2{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = src.stage,
      .srcAccessMask = src.access,
      .dstStageMask = dst.stage,
      .dstAccessMask = dst.access,
      .oldLayout = layout_,
      .newLayout = new_image_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = vk_handle_,
      .subresourceRange = subresource_range,
  };

  const VkDependencyInfo depinfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier2,
  };

  vkCmdPipelineBarrier2(vk_cmd_buffer, &depinfo);

  layout_ = new_image_layout;
}

VkImageAspectFlags VulkanImage::getImageAspectFlags() const {
  VkImageAspectFlags flags = 0;

  flags |= is_depth_format_ ? VK_IMAGE_ASPECT_DEPTH_BIT : 0;
  flags |= is_stencil_format_ ? VK_IMAGE_ASPECT_STENCIL_BIT : 0;
  flags |= !(is_depth_format_ || is_stencil_format_) ? VK_IMAGE_ASPECT_COLOR_BIT : 0;

  return flags;
}

VkImageView VulkanImage::getOrCreateVkImageViewForFramebuffer(Context& ctx, uint8_t level,
                                                              uint16_t layer,
                                                              uint32_t view_mask) {
  ASSERT(level < MAX_MIP_LEVELS, TEXT("Image level should less than MAX_MIP_LEVELS."));
  ASSERT(layer < ARRAY_COUNT(image_view_for_framebuffer_[0]),
         TEXT("Image layer should less than 6"));
  ASSERT(!view_mask || layer == 0);

  if (level >= MAX_MIP_LEVELS || layer >= ARRAY_COUNT(image_view_for_framebuffer_)) {
    return VK_NULL_HANDLE;
  }

  if (!view_mask && image_view_for_framebuffer_[level][layer] != VK_NULL_HANDLE) {
    return image_view_for_framebuffer_[level][layer];
  }
  if (view_mask && image_view_for_framebuffer_multiview_[level] != VK_NULL_HANDLE) {
    return image_view_for_framebuffer_multiview_[level];
  }

  char debug_name_image_view[320] = {0};
  (void)snprintf(debug_name_image_view, sizeof(debug_name_image_view) - 1,
                 "ImageView: '%s' ImageViewForFrameBuffer[%u][%u][%u]", debug_name_,
                 level, layer, view_mask);

  const uint32 num_views = view_mask ? CountBits(view_mask) : 1;

  VkImageView view = createImageView(
      ctx.getVkDevice(), view_mask ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D,
      format_, getImageAspectFlags(), level, 1u, layer, num_views, {}, nullptr,
      debug_name_image_view);

  if (view_mask) {
    image_view_for_framebuffer_multiview_[level] = view;
  } else {
    image_view_for_framebuffer_[level][layer] = view;
  }

  return view;
}

bool VulkanImage::isDepthFormat(VkFormat format) {
  return format == VK_FORMAT_D16_UNORM ||            //
         format == VK_FORMAT_X8_D24_UNORM_PACK32 ||  //
         format == VK_FORMAT_D32_SFLOAT ||           //
         format == VK_FORMAT_D16_UNORM_S8_UINT ||    //
         format == VK_FORMAT_D24_UNORM_S8_UINT ||    //
         format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

bool VulkanImage::isStencilFormat(VkFormat format) {
  return format == VK_FORMAT_S8_UINT ||            //
         format == VK_FORMAT_D16_UNORM_S8_UINT ||  //
         format == VK_FORMAT_D24_UNORM_S8_UINT ||  //
         format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

}  // namespace vk

}  // namespace amaryl

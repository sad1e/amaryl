#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

namespace {
VkSurfaceFormatKHR _choose_swap_surface_format(
    const std::vector<VkSurfaceFormat2KHR>& formats, EColorSpace requested_colorspace,
    bool has_swapchain_colorspace) {
  ASSERT(!formats.empty(), TEXT("No valid VkSurfaceFormatKHR for surface formats"));

  auto is_native_swapchain_BGR =
      [](const std::vector<VkSurfaceFormat2KHR>& formats) -> bool {
    for (const VkSurfaceFormat2KHR& fmt2 : formats) {
      const VkFormat fmt = fmt2.surfaceFormat.format;
      if (fmt == VK_FORMAT_R8G8B8A8_UNORM || fmt == VK_FORMAT_R8G8B8A8_SRGB ||
          fmt == VK_FORMAT_A2R10G10B10_UNORM_PACK32) {
        return false;
      }

      if (fmt == VK_FORMAT_B8G8R8A8_UNORM || fmt == VK_FORMAT_B8G8R8A8_SRGB ||
          fmt == VK_FORMAT_A2B10G10R10_UNORM_PACK32) {
        return true;
      }
    }
    return false;
  };

  auto colorspace_to_VkSurfaceFormat =
      [](EColorSpace colorspace, bool is_BGR,
         bool has_swapchain_colorspace_EXT) -> VkSurfaceFormatKHR {
    switch (colorspace) {
      case EColorSpace::SRGB_NONLINEAR:
        return VkSurfaceFormatKHR{
            .format = is_BGR ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R8G8B8A8_UNORM,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        };
      case EColorSpace::SRGB_EXTENDED_LINEAR:
        if (has_swapchain_colorspace_EXT) {
          return VkSurfaceFormatKHR{
              .format = VK_FORMAT_R16G16B16A16_SFLOAT,
              .colorSpace = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
          };
        }
        [[fallthrough]];
      case EColorSpace::HDR10:
        if (has_swapchain_colorspace_EXT) {
          return VkSurfaceFormatKHR{
              .format = is_BGR ? VK_FORMAT_A2B10G10R10_UNORM_PACK32
                               : VK_FORMAT_A2R10G10B10_UNORM_PACK32,
              .colorSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT,
          };
        }
        [[fallthrough]];
      default:
        return VkSurfaceFormatKHR{
            .format = is_BGR ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_R8G8B8A8_SRGB,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        };
    }
  };

  const VkSurfaceFormatKHR perferred_surface_format = colorspace_to_VkSurfaceFormat(
      requested_colorspace, is_native_swapchain_BGR(formats), has_swapchain_colorspace);

  for (const VkSurfaceFormat2KHR& fmt2 : formats) {
    if (fmt2.surfaceFormat.format == perferred_surface_format.format) {
      return fmt2.surfaceFormat;
    }
  }

  // If the format and colorSpace not matching same time, fall back to match format.
  for (const VkSurfaceFormat2KHR& fmt2 : formats) {
    if (fmt2.surfaceFormat.format == perferred_surface_format.format) {
      return fmt2.surfaceFormat;
    }
  }

  ALOGW(
      "Could not find a native swap chain format that matched our designed swapchain "
      "format. Defaulting to first "
      "supported format.");

  return formats[0].surfaceFormat;
}

}  // namespace

namespace vk {

VulkanSwapchain::VulkanSwapchain(Context& ctx, uint32 width, uint32 height)
    : ctx_(ctx),
      device_(ctx.device_),
      graphics_queue_(ctx.device_queues_.graphicsQueue),
      width_(width),
      height_(height) {
  surface_format_ = _choose_swap_surface_format(ctx.device_surface_formats_,
                                                ctx.config_.swapchainRequestedColorSpace,
                                                ctx.has_EXT_swapchain_colorspace_);

  ASSERT(ctx.surface_ != VK_NULL_HANDLE,
         "Create a swapchain with surface is VK_NULL_HANDLE, not support offscreen "
         "rendering");

  // query the queue family supports presentation, it seems like that the queue not
  // supports presentation is not normal.
  VkBool32 queue_family_supports_presentation = VK_FALSE;

  VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(
      ctx_.getVkPhysicalDevice(), ctx.device_queues_.graphicsQueueFamilyIndex,
      ctx.surface_, &queue_family_supports_presentation));
  ASSERT(queue_family_supports_presentation == VK_TRUE,
         "The queue family used with the swapchain does not support presentation");

  auto choose_swap_image_count = [](const VkSurfaceCapabilitiesKHR& caps) -> uint32 {
    const uint32 desired = caps.minImageCount + 1;
    const bool exceeded = caps.maxImageCount > 0 && desired > caps.maxImageCount;
    return exceeded ? caps.maxImageCount : desired;
  };
  auto choose_swap_present_mode =
      [config =
           &ctx.config_](const std::vector<VkPresentModeKHR>& modes) -> VkPresentModeKHR {
    for (EPresentMode present_mode : config->presentModes) {
      const VkPresentModeKHR vk_present_mode = toVkPresentMode(present_mode);
      if (std::find(modes.cbegin(), modes.cend(), vk_present_mode) != modes.cend()) {
        return vk_present_mode;
      }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  };

  VkSurfaceCapabilitiesKHR surf_caps{};
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.getVkPhysicalDevice(),
                                                     ctx.surface_, &surf_caps));
  VkFormatProperties2 props{
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
  };
  vkGetPhysicalDeviceFormatProperties2(ctx.getVkPhysicalDevice(), surface_format_.format,
                                       &props);

  width_ = width = std::min(width, surf_caps.maxImageExtent.width);
  height_ = height = std::min(height, surf_caps.maxImageExtent.height);

  auto choose_usage_flags = [](const VkSurfaceCapabilitiesKHR& caps,
                               const VkFormatProperties& props) -> VkImageUsageFlags {
    VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    const bool is_storage_supported =
        (caps.supportedUsageFlags & VK_IMAGE_USAGE_STORAGE_BIT) > 0;
    const bool is_tiling_optimal_supported =
        (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) > 0;

    if (is_storage_supported && is_tiling_optimal_supported) {
      usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    return usage_flags;
  };

  current_present_mode_ = choose_swap_present_mode(ctx_.device_present_modes_);

  std::vector<VkPresentModeKHR> surf_caps_present_models;
  if (ctx_.has_KHR_swapchain_maintenance1_) {
    const VkSurfacePresentModeKHR surface_present_mode{
        .sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_KHR,
        .presentMode = current_present_mode_,
    };
    const VkPhysicalDeviceSurfaceInfo2KHR surface_info2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .pNext = &surface_present_mode,
        .surface = ctx.surface_,
    };
    VkSurfacePresentModeCompatibilityKHR compatibility{
        .sType = VK_STRUCTURE_TYPE_SURFACE_PRESENT_MODE_COMPATIBILITY_KHR,
    };
    VkSurfaceCapabilities2KHR surface_capabilities{
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
        .pNext = &compatibility,
    };

    vkGetPhysicalDeviceSurfaceCapabilities2KHR(ctx.getVkPhysicalDevice(), &surface_info2,
                                               &surface_capabilities);
    surf_caps_present_models.resize(compatibility.presentModeCount);
    compatibility.pPresentModes = surf_caps_present_models.data();
    vkGetPhysicalDeviceSurfaceCapabilities2KHR(ctx.getVkPhysicalDevice(), &surface_info2,
                                               &surface_capabilities);
  }

  if (std::ranges::count(std::as_const(surf_caps_present_models),
                         current_present_mode_)) {
    registered_present_modes_[num_registered_present_modes_++] = current_present_mode_;
  }

  for (EPresentMode present_mode : ctx.config_.presentModes) {
    const VkPresentModeKHR vk_present_mode = toVkPresentMode(present_mode);

    // if present mode not in surface capabilities present modes
    if (!std::ranges::count(surf_caps_present_models, vk_present_mode)) {
      continue;
    }

    // if present mode not in registered present modes
    if (std::ranges::count(
            std::cbegin(registered_present_modes_),
            std::cbegin(registered_present_modes_) + num_registered_present_modes_,
            vk_present_mode)) {
      continue;
    }

    // if Context not has KHR fifo lastest ready present mode
    if (!ctx.has_KHR_present_mode_fifo_latest_ready_ &&
        (vk_present_mode == VK_PRESENT_MODE_FIFO_LATEST_READY_KHR)) {
      continue;
    }

    // if Context not has shared presentable images
    if (!ctx.has_KHR_shared_presentable_image_ &&
        (vk_present_mode == VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR ||
         vk_present_mode == VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR)) {
      continue;
    }

    registered_present_modes_[num_registered_present_modes_++] = vk_present_mode;
  }

  if (num_registered_present_modes_) {
    (void)setCurrentPresentMode(current_present_mode_);
  }

  const VkImageUsageFlags vk_image_usage_flags =
      choose_usage_flags(surf_caps, props.formatProperties);
  const bool is_composite_alpha_opaque_supported =
      (ctx.device_surface_capabilities_.surfaceCapabilities.supportedCompositeAlpha &
       VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0;

  const VkSwapchainPresentModesCreateInfoKHR present_modes_ci{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR,
      .presentModeCount = num_registered_present_modes_,
      .pPresentModes = registered_present_modes_,
  };

  const VkSwapchainCreateInfoKHR swapchain_ci{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = num_registered_present_modes_ ? &present_modes_ci : nullptr,
      .surface = ctx.surface_,
      .minImageCount =
          choose_swap_image_count(ctx.device_surface_capabilities_.surfaceCapabilities),
      .imageFormat = surface_format_.format,
      .imageColorSpace = surface_format_.colorSpace,
      .imageExtent = {.width = width, .height = height},
      .imageArrayLayers = 1,
      .imageUsage = vk_image_usage_flags,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &ctx.device_queues_.graphicsQueueFamilyIndex,
#if PLATFORM_ANDROID
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
#else
      .preTransform =
          ctx.device_surface_capabilities_.surfaceCapabilities.currentTransform,
#endif
      .compositeAlpha = is_composite_alpha_opaque_supported
                            ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
                            : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
      .presentMode = current_present_mode_,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  VK_CHECK(vkCreateSwapchainKHR(device_, &swapchain_ci, nullptr, &vk_handle_));

  if (ctx.has_EXT_hdr_metadata_) {
    constexpr VkHdrMetadataEXT metadata = {
        .sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT,
        .displayPrimaryRed = {.x = 0.680f, .y = 0.320f},
        .displayPrimaryGreen = {.x = 0.265f, .y = 0.690f},
        .displayPrimaryBlue = {.x = 0.150f, .y = 0.060f},
        .whitePoint = {.x = 0.3127f, .y = 0.3290f},
        .maxLuminance = 80.0f,
        .minLuminance = 0.001f,
        .maxContentLightLevel = 2000.0f,
        .maxFrameAverageLightLevel = 500.0f,
    };
    vkSetHdrMetadataEXT(device_, 1, &vk_handle_, &metadata);
  }

  VkImage swapchain_images[MAX_SWAPCHAIN_IMAGES];
  VK_CHECK(vkGetSwapchainImagesKHR(device_, vk_handle_, &num_swapchain_images_, nullptr));
  if (num_swapchain_images_ > MAX_SWAPCHAIN_IMAGES) {
    ALOGW(
        "The swapchain images larger than the maximum number of swapchain images, clamp "
        "to "
        "MAX_SWAPCHAIN_IMAGES");
    num_swapchain_images_ = MAX_SWAPCHAIN_IMAGES;
  }
  VK_CHECK(vkGetSwapchainImagesKHR(device_, vk_handle_, &num_swapchain_images_,
                                   swapchain_images));

  ASSERT(num_swapchain_images_ > 0);

  char debug_name_image[MAX_DEBUG_STRING_LENGTH] = {0};
  char debug_name_imageview[MAX_DEBUG_STRING_LENGTH] = {0};

  for (uint32 i = 0; i < num_swapchain_images_; ++i) {
    acquire_semaphores_[i] = createSemaphore(device_, "Semaphore: swapchain-acquire");

    if (ctx.has_KHR_swapchain_maintenance1_) {
      char debug_name_fence[MAX_DEBUG_STRING_LENGTH] = {0};
      (void)snprintf(debug_name_fence, sizeof(debug_name_fence) - 1,
                     "Fence: swapchain %u", i);
      acquire_fences_[i] = createFence(device_, debug_name_fence, true);
    }

    (void)snprintf(debug_name_image, 255, "Image: swapchain %u", i);
    (void)snprintf(debug_name_imageview, 255, "ImageView: swapchain %u", i);

    VulkanImage image;
    image.vk_handle_ = swapchain_images[i];
    image.usage_flags_ = vk_image_usage_flags;
    image.extent_ = VkExtent3D{.width = width_, .height = height_, .depth = 1};
    image.type_ = VK_IMAGE_TYPE_2D;
    image.format_ = surface_format_.format;
    image.is_swapchain_image_ = true;
    image.is_owning_VkImage_ = false;
    image.is_depth_format_ =
        false;  // VulkanImage::isDepthFormat(surface_format_.format);
    image.is_stencil_format_ =
        false;  // VulkanImage::isStencilFormat(surface_format_.format);

    VK_CHECK(setDebugObjectName(device_, VK_OBJECT_TYPE_IMAGE, (uint64)image.vk_handle_,
                                debug_name_image));

    image.image_view_ = image.createImageView(
        device_, VK_IMAGE_VIEW_TYPE_2D, surface_format_.format, VK_IMAGE_ASPECT_COLOR_BIT,
        0, VK_REMAINING_MIP_LEVELS, 0, 1, {}, nullptr, debug_name_imageview);

    swapchain_texture_handles_[i] = ctx.texture_pool_.create(std::move(image));
  }
}

VulkanSwapchain::~VulkanSwapchain() {
  for (TextureHandle handle : swapchain_texture_handles_) {
    ctx_.destroy(handle);
  }
  vkDestroySwapchainKHR(device_, vk_handle_, nullptr);

  for (VkSemaphore semaphore : acquire_semaphores_) {
    vkDestroySemaphore(device_, semaphore, nullptr);
  }

  for (VkFence fence : present_fences_) {
    if (fence) vkDestroyFence(device_, fence, nullptr);
  }

  for (VkFence fence : acquire_fences_) {
    if (fence) vkDestroyFence(device_, fence, nullptr);
  }
}

Result VulkanSwapchain::present(VkSemaphore wait_semaphore) {
  if (ctx_.has_KHR_swapchain_maintenance1_ && !present_fences_[current_image_index_]) {
    present_fences_[current_image_index_] = createFence(device_, "Fence: present-fence");
  }

  present_fence_info_.pFences = &present_fences_[current_image_index_];
  const VkPresentInfoKHR present_info{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = ctx_.has_KHR_swapchain_maintenance1_ ? &present_fence_info_ : nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &wait_semaphore,
      .swapchainCount = 1u,
      .pSwapchains = &vk_handle_,
      .pImageIndices = &current_image_index_,
  };

  VkResult result = vkQueuePresentKHR(graphics_queue_, &present_info);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR &&
      result != VK_ERROR_OUT_OF_DATE_KHR) {
    VK_CHECK(result);
  }

  // let acquire() gate reuse of this command buffer's slot until the present has consumed
  // it wait semaphore
  ctx_.immediate_->setLastPresentSemaphore(wait_semaphore,
                                           present_fences_[current_image_index_]);

  // drop the previous present mode so we don't set it again in the next `present()` call
  // if the present mode is not switched at runtime
  present_fence_info_.pNext = nullptr;

  // ready to call `acquireNextImage()` on the next `getCurrentVulkanTexture()`
  get_next_image_ = true;
  current_frame_index_++;

  return Result();
}

VkImage VulkanSwapchain::getCurrentVkImage() const {
  if (current_image_index_ < num_swapchain_images_) {
    VulkanImage* image =
        ctx_.texture_pool_.get(swapchain_texture_handles_[current_image_index_]);
    return image->vk_handle_;
  }

  return VK_NULL_HANDLE;
}

VkImageView VulkanSwapchain::getCurrentVkImageView() const {
  if (current_image_index_ < num_swapchain_images_) {
    VulkanImage* image =
        ctx_.texture_pool_.get(swapchain_texture_handles_[current_image_index_]);
    return image->image_view_;
  }

  return VK_NULL_HANDLE;
}

TextureHandle VulkanSwapchain::getCurrentTexture() {
  // if `get_next_image_` is true, which means the code will call `vkAcquireNextIamgeKHR`
  // to get next image for rendering. If the value is false means the code already got an
  // image, and the following `getCurrentTexture` will return the same image.
  if (get_next_image_) {
    const VkSemaphoreWaitInfo wait_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO_KHR,
        .semaphoreCount = 1,
        .pSemaphores = &ctx_.timeline_semaphore_,
        .pValues = &timeline_wait_values_[current_image_index_],
    };
    VK_CHECK(vkWaitSemaphores(device_, &wait_info, UINT64_MAX));

    VkFence acquire_fence = VK_NULL_HANDLE;

    if (ctx_.has_KHR_swapchain_maintenance1_) {
      // `VK_KHR_swapchain_maintenance1`: before acquiring again, wait for the
      // presentation operation to finish
      if (present_fences_[current_image_index_]) {
        VK_CHECK(vkWaitForFences(device_, 1, &present_fences_[current_image_index_],
                                 VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(device_, 1, &present_fences_[current_image_index_]));

        // previous present done, drop the reuse guard
        ctx_.immediate_->setLastPresentSemaphore(VK_NULL_HANDLE, VK_NULL_HANDLE);
      }
    } else {
      // without `VK_KHR_swapchain_maintenance1`: use acquire fences to synchronize
      // semaphore reuse
      VK_CHECK(vkWaitForFences(device_, 1, &acquire_fences_[current_image_index_],
                               VK_TRUE, UINT64_MAX));
      VK_CHECK(vkResetFences(device_, 1, &acquire_fences_[current_image_index_]));

      acquire_fence = acquire_fences_[current_image_index_];
    }

    VkSemaphore acquire_semaphore = acquire_semaphores_[current_image_index_];

    // When timeout is set to UINT64_MAX, we wait until the next image has been acquired
    VkResult result =
        vkAcquireNextImageKHR(device_, vk_handle_, UINT64_MAX, acquire_semaphore,
                              acquire_fence, &current_image_index_);

    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
      get_next_image_ = false;
      ctx_.immediate_->waitSemaphore(acquire_semaphore);
    } else {
      ALOGW(TEXT(
          "If `vkAcquireNextImageKHR` failed, the semaphore not be signaled, cannot send "
          "to submit, try to "
          "recreate swapchain."));

      get_next_image_ = true;
      return {};
    }
  }

  if (current_image_index_ < num_swapchain_images_) {
    return swapchain_texture_handles_[current_image_index_];
  }

  return {};
}

const VkSurfaceFormatKHR& VulkanSwapchain::getSurfaceFormat() const {
  return surface_format_;
}

uint32_t VulkanSwapchain::getSwapchainCurrentImageIndex() const {
  return current_image_index_;
}

uint32_t VulkanSwapchain::getNumSwapchainImages() const { return num_swapchain_images_; }

bool VulkanSwapchain::setCurrentPresentMode(VkPresentModeKHR mode) {
  for (uint32 i = 0; i < num_registered_present_modes_; ++i) {
    if (registered_present_modes_[i] == mode) {
      current_present_mode_ = mode;
      present_fence_info_.pNext = &present_mode_info_;
      return true;
    }
  }

  return false;
}

}  // namespace vk

}  // namespace amaryl

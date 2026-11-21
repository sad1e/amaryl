#include "amaryl/nrhi/vulkan/nvk.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstring>

namespace amaryl {

#define IMPL_CTX_DESTROY_HANDLE(HandleType)        \
  void destroy(IContext* ctx, HandleType handle) { \
    if (ctx) {                                     \
      ctx->destroy(handle);                        \
    }                                              \
  }

IMPL_CTX_DESTROY_HANDLE(ComputePipelineHandle)
IMPL_CTX_DESTROY_HANDLE(RenderPipelineHandle)
IMPL_CTX_DESTROY_HANDLE(RayTracingPipelineHandle)
IMPL_CTX_DESTROY_HANDLE(ShaderModuleHandle)
IMPL_CTX_DESTROY_HANDLE(SamplerHandle)
IMPL_CTX_DESTROY_HANDLE(BufferHandle)
IMPL_CTX_DESTROY_HANDLE(TextureHandle)
IMPL_CTX_DESTROY_HANDLE(QueryPoolHandle)
IMPL_CTX_DESTROY_HANDLE(AccelerationStructHandle)

#undef IMPL_CTX_DESTROY_HANDLE

std::unique_ptr<IContext> createVulkanContextWithSwapchain(
    SDL_Window* window, uint32 width, uint32 height, vk::ContextConfig& cfg,
    EHWDeviceType perferred_device_type, int selected_device) {
  SDL_PropertiesID props = SDL_GetWindowProperties(window);
  void* hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

  if (!hwnd) {
    ASSERT(false, "Failed to get Win32 window handle.");
    return nullptr;
  }

  std::unique_ptr<vk::Context> ctx = std::make_unique<vk::Context>(cfg, (void*)hwnd);

  HWDeviceDesc devices[16];
  const uint32 num_devices = ctx->queryDevices(devices, ARRAY_COUNT(devices));

  ASSERT(num_devices != 0, "GPU not found.");
  if (selected_device < 0) {
    selected_device = [perferred_device_type, &devices, num_devices]() -> int {
      EHWDeviceType priority[4] = {perferred_device_type};
      {
        int index = 1;
        for (int type = static_cast<int>(EHWDeviceType::Integrated);
             type <= static_cast<int>(EHWDeviceType::Software); ++type) {
          if (type != static_cast<int>(perferred_device_type)) {
            priority[index++] = static_cast<EHWDeviceType>(type);
          }
        }
      }

      for (EHWDeviceType type : priority) {
        for (uint32 i = 0; i < num_devices; ++i) {
          if (devices[i].type == type) {
            return static_cast<int32>(i);
          }
        }
      }
      return 0;
    }();
  }

  if (selected_device >= num_devices) {
    ASSERT(false, "Invalid device index");
    return nullptr;
  }

  Result result = ctx->initContext(devices[selected_device]);

  if (!result.isOk()) {
    ASSERT(false, "createVulkanContextWithSwapchain failed");
    return nullptr;
  }

  if (width > 0 && height > 0) {
    result = ctx->initSwapchain(width, height);
    if (!result.isOk()) {
      ASSERT(false, "initSwapchain() failed");
      return nullptr;
    }
  }

  return std::move(ctx);
}

namespace vk {
static std::vector<const char*> GWantedInstanceExtensions = {
    VK_KHR_SURFACE_EXTENSION_NAME,
// Defined in hplatform_*.h files
#ifdef VK_USE_PLATFORM_WIN32_KHR
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
    VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
    VK_KHR_DEVICE_GROUP_CREATION_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
};

static std::vector<const char*> GWantedDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
    VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
    VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
    VK_KHR_MAINTENANCE3_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
    VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
    VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
    VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
    VK_EXT_SAMPLE_LOCATIONS_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    VK_KHR_MULTIVIEW_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    VK_KHR_COMPUTE_SHADER_DERIVATIVES_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
#ifdef VK_USE_PLATFORM_WIN32_KHR
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
#endif
    VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
};

// ─── Debug callback ──────────────────────────────────────────────────────────

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                   VkDebugUtilsMessageTypeFlagsEXT /*type*/,
                   const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*user*/) {
  if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    ALOGE("[Vulkan] %s (%d)", data->pMessage, data->messageIdNumber);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    ALOGW("[Vulkan] %s (%d)", data->pMessage, data->messageIdNumber);
  } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    ALOGI("[Vulkan] %s (%d)", data->pMessage, data->messageIdNumber);
  }
  return VK_FALSE;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

static bool isExtensionAvailable(const std::vector<VkExtensionProperties>& available,
                                 const char* name) {
  for (auto& ext : available) {
    if (strcmp(ext.extensionName, name) == 0) return true;
  }
  return false;
}

static bool isLayerAvailable(const std::vector<VkLayerProperties>& available,
                             const char* name) {
  for (auto& layer : available) {
    if (strcmp(layer.layerName, name) == 0) return true;
  }
  return false;
}

static uint32 scorePhysicalDevice(VkPhysicalDevice device) {
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(device, &props);

  uint32 score = 0;
  if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    score += 10000;
  else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
    score += 1000;

  score += props.limits.maxImageDimension2D;
  return score;
}

namespace {
struct ResolvedImageTransition {
  VkImageAspectFlags aspectMask = 0;
  uint32 baseMipLevel = 0;
  uint32 mipEnd = 0;
  uint32 baseArrayLayer = 0;
  uint32 layerEnd = 0;
  TransitionData src;
  TransitionData dst;
  uint32 srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  uint32 dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

void _validate_transition_data(TransitionData data) {
  ASSERT(data.access == VK_ACCESS_2_NONE || data.stage != VK_PIPELINE_STAGE_2_NONE,
         "Access mask requires a non-empty stage mask.");
}

void validateTransitionDesc(const ImageTransitionDesc& desc) {
  ASSERT(desc.subresourceRange.aspectMask != 0u,
         "Image transition aspect mask must be non-zero.");
  _validate_transition_data(desc.src);
  _validate_transition_data(desc.dst);
}

uint32 _resolve_range_end(uint32 base, uint32 count, uint32 total, uint32 remaining_value,
                          const char* name) {
  ASSERT(total > 0u, "%s total count must be non-zero.", name);
  ASSERT(base < total, "%s base is out of range.", name);

  const uint32 resolved_count = count == remaining_value ? total - base : count;
  ASSERT(resolved_count > 0u, "%s count must be non-zero.", name);
  ASSERT(base <= total - resolved_count, "%s range exceeds image subresource bounds.",
         name);
  return base + resolved_count;
}

void _append_aspect_bis(std::vector<VkImageAspectFlags>& aspect_bits,
                        VkImageAspectFlags aspect_mask) {
  VkImageAspectFlags known_mask = 0;
  auto append_known_bit = [&](VkImageAspectFlags bit) {
    known_mask |= bit;
    if ((aspect_mask & bit) != 0u) {
      aspect_bits.push_back(bit);
    }
  };

  append_known_bit(VK_IMAGE_ASPECT_COLOR_BIT);
  append_known_bit(VK_IMAGE_ASPECT_DEPTH_BIT);
  append_known_bit(VK_IMAGE_ASPECT_STENCIL_BIT);
  append_known_bit(VK_IMAGE_ASPECT_METADATA_BIT);
#ifdef VK_IMAGE_ASPECT_PLANE_0_BIT
  append_known_bit(VK_IMAGE_ASPECT_PLANE_0_BIT);
#endif
#ifdef VK_IMAGE_ASPECT_PLANE_1_BIT
  append_known_bit(VK_IMAGE_ASPECT_PLANE_1_BIT);
#endif
#ifdef VK_IMAGE_ASPECT_PLANE_2_BIT
  append_known_bit(VK_IMAGE_ASPECT_PLANE_2_BIT);
#endif

  ASSERT((aspect_mask & ~known_mask) == 0u,
         "Image transition uses unsupported VkImageAspectFlags bits.");
}

bool _covers_cell(const ResolvedImageTransition& transition, VkImageAspectFlags aspect,
                  uint32 mip_begin, uint32 mip_end, uint32 layer_begin,
                  uint32 layer_end) {
  return (transition.aspectMask & aspect) != 0u &&    //
         transition.baseMipLevel <= mip_begin &&      //
         mip_end <= transition.mipEnd &&              //
         transition.baseArrayLayer <= layer_begin &&  //
         layer_end <= transition.layerEnd;
}

bool _is_same_layout_and_ownership(const ResolvedImageTransition& a,
                                   const ResolvedImageTransition& b) {
  return a.src.layout == b.src.layout && a.dst.layout == b.dst.layout &&
         a.srcQueueFamilyIndex == b.srcQueueFamilyIndex &&
         a.dstQueueFamilyIndex == b.dstQueueFamilyIndex;
}

void _sort_unique(std::vector<uint32>& values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
}
}  // namespace

void transitionImage(VkCommandBuffer cmdBuffer, VkImage image, TransitionData src,
                     TransitionData dst) {
  VkImageSubresourceRange subresource_range = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = 0,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
  };
  transitionImage(cmdBuffer, image, subresource_range, src, dst);
}

void transitionImage(VkCommandBuffer cmdBuffer, VkImage image,
                     VkImageSubresourceRange subresourceRange, TransitionData src,
                     TransitionData dst, uint32 srcQueueFamilyIndex,
                     uint32 dstQueueFamilyIndex) {
  ImageTransitionDesc transition{
      .subresourceRange = subresourceRange,
      .src = src,
      .dst = dst,
      .srcQueueFamilyIndex = srcQueueFamilyIndex,
      .dstQueueFamilyIndex = dstQueueFamilyIndex,
  };
  validateTransitionDesc(transition);

  ASSERT(cmdBuffer != VK_NULL_HANDLE, "VkCommandBuffer must be valid.");
  ASSERT(image != VK_NULL_HANDLE, "VkImage must be valid.");

  VkImageMemoryBarrier2 image_memory_barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = src.stage,
      .srcAccessMask = src.access,
      .dstStageMask = dst.stage,
      .dstAccessMask = dst.access,
      .oldLayout = src.layout,
      .newLayout = dst.layout,
      .srcQueueFamilyIndex = srcQueueFamilyIndex,
      .dstQueueFamilyIndex = dstQueueFamilyIndex,
      .image = image,
      .subresourceRange = subresourceRange,
  };
  VkDependencyInfo dependency_info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &image_memory_barrier,
  };
  vkCmdPipelineBarrier2(cmdBuffer, &dependency_info);
}

void transitionImage(VkCommandBuffer cmdBuffer, VkImage image, uint32 mipCount,
                     uint32 layerCount, const ImageTransitionDesc* transitions,
                     uint32 transitionCount) {
  ASSERT(cmdBuffer != VK_NULL_HANDLE, "VkCommandBuffer must be valid.");
  ASSERT(image != VK_NULL_HANDLE, "VkImage must be valid.");
  ASSERT(transitionCount == 0u || transitions != nullptr,
         "Image transition list must be valid.");

  if (transitionCount == 0u) {
    return;
  }

  std::vector<ResolvedImageTransition> resolved_transitions;
  resolved_transitions.reserve(transitionCount);

  std::vector<VkImageAspectFlags> aspect_bits;
  aspect_bits.reserve(transitionCount);

  for (uint32 i = 0; i < transitionCount; ++i) {
    const ImageTransitionDesc& transition = transitions[i];
    validateTransitionDesc(transition);

    const uint32 mip_end = _resolve_range_end(transition.subresourceRange.baseMipLevel,
                                              transition.subresourceRange.levelCount,
                                              mipCount, VK_REMAINING_MIP_LEVELS, "Mip");
    const uint32 layer_end =
        _resolve_range_end(transition.subresourceRange.baseArrayLayer,
                           transition.subresourceRange.layerCount, layerCount,
                           VK_REMAINING_ARRAY_LAYERS, "Array layer");

    _append_aspect_bis(aspect_bits, transition.subresourceRange.aspectMask);
    resolved_transitions.push_back({
        .aspectMask = transition.subresourceRange.aspectMask,
        .baseMipLevel = transition.subresourceRange.baseMipLevel,
        .mipEnd = mip_end,
        .baseArrayLayer = transition.subresourceRange.baseArrayLayer,
        .layerEnd = layer_end,
        .src = transition.src,
        .dst = transition.dst,
        .srcQueueFamilyIndex = transition.srcQueueFamilyIndex,
        .dstQueueFamilyIndex = transition.dstQueueFamilyIndex,
    });
  }

  std::sort(aspect_bits.begin(), aspect_bits.end());
  aspect_bits.erase(std::unique(aspect_bits.begin(), aspect_bits.end()),
                    aspect_bits.end());

  std::vector<VkImageMemoryBarrier2> image_memory_barriers;

  for (VkImageAspectFlags aspect : aspect_bits) {
    std::vector<uint32> mip_bounds;
    std::vector<uint32> layer_bounds;

    for (const ResolvedImageTransition& transition : resolved_transitions) {
      if ((transition.aspectMask & aspect) == 0u) {
        continue;
      }

      mip_bounds.push_back(transition.baseMipLevel);
      mip_bounds.push_back(transition.mipEnd);
      layer_bounds.push_back(transition.baseArrayLayer);
      layer_bounds.push_back(transition.layerEnd);
    }

    _sort_unique(mip_bounds);
    _sort_unique(layer_bounds);

    for (uint32 mip_bound_index = 0; mip_bound_index + 1u < mip_bounds.size();
         ++mip_bound_index) {
      const uint32 mip_begin = mip_bounds[mip_bound_index];
      const uint32 mip_end = mip_bounds[mip_bound_index + 1u];

      for (uint32 layer_bound_index = 0; layer_bound_index + 1u < layer_bounds.size();
           ++layer_bound_index) {
        const uint32 layer_begin = layer_bounds[layer_bound_index];
        const uint32 layer_end = layer_bounds[layer_bound_index + 1u];

        ResolvedImageTransition combined_transition{};
        bool has_transition = false;

        for (const ResolvedImageTransition& transition : resolved_transitions) {
          if (!_covers_cell(transition, aspect, mip_begin, mip_end, layer_begin,
                            layer_end)) {
            continue;
          }

          if (!has_transition) {
            combined_transition = transition;
            has_transition = true;
            continue;
          }

          ASSERT(_is_same_layout_and_ownership(combined_transition, transition),
                 "Overlapping image layout transitions must agree on old/new layouts and "
                 "queue ownership.");
          combined_transition.src.stage |= transition.src.stage;
          combined_transition.src.access |= transition.src.access;
          combined_transition.dst.stage |= transition.dst.stage;
          combined_transition.dst.access |= transition.dst.access;
        }

        if (!has_transition) {
          continue;
        }

        image_memory_barriers.push_back({
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = combined_transition.src.stage,
            .srcAccessMask = combined_transition.src.access,
            .dstStageMask = combined_transition.dst.stage,
            .dstAccessMask = combined_transition.dst.access,
            .oldLayout = combined_transition.src.layout,
            .newLayout = combined_transition.dst.layout,
            .srcQueueFamilyIndex = combined_transition.srcQueueFamilyIndex,
            .dstQueueFamilyIndex = combined_transition.dstQueueFamilyIndex,
            .image = image,
            .subresourceRange =
                {
                    .aspectMask = aspect,
                    .baseMipLevel = mip_begin,
                    .levelCount = mip_end - mip_begin,
                    .baseArrayLayer = layer_begin,
                    .layerCount = layer_end - layer_begin,
                },
        });
      }
    }
  }

  if (image_memory_barriers.empty()) {
    return;
  }

  VkDependencyInfo dependency_info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = static_cast<uint32>(image_memory_barriers.size()),
      .pImageMemoryBarriers = image_memory_barriers.data(),
  };
  vkCmdPipelineBarrier2(cmdBuffer, &dependency_info);
}

const std::string toString(EShaderStage shader_type) {
  switch (shader_type) {
    case EShaderStage::Vertex:
      return "Vertex";
    case EShaderStage::Fragment:
      return "Fragment";
    case EShaderStage::Compute:
      return "Compute";
    case EShaderStage::Geometry:
      return "Geometry";
    case EShaderStage::Task:
      return "Task";
    case EShaderStage::Mesh:
      return "Mesh";
    case EShaderStage::RayGeneration:
      return "RayGen";
    case EShaderStage::ClosestHit:
      return "ClosestHit";
    case EShaderStage::Miss:
      return "Miss";
    case EShaderStage::Intersection:
      return "Intersection";
    case EShaderStage::AnyHit:
      return "AnyHit";
    case EShaderStage::Callable:
      return "Callable";
    default:
      return "";
  }
}

const std::string toString(VkSystemAllocationScope sys_alloc_scope) {
  switch (sys_alloc_scope) {
    case VK_SYSTEM_ALLOCATION_SCOPE_COMMAND:
      return "VK_SYSTEM_ALLOCATION_SCOPE_COMMAND";
    case VK_SYSTEM_ALLOCATION_SCOPE_OBJECT:
      return "VK_SYSTEM_ALLOCATION_SCOPE_OBJECT";
    case VK_SYSTEM_ALLOCATION_SCOPE_CACHE:
      return "VK_SYSTEM_ALLOCATION_SCOPE_CACHE";
    case VK_SYSTEM_ALLOCATION_SCOPE_DEVICE:
      return "VK_SYSTEM_ALLOCATION_SCOPE_DEVICE";
    case VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE:
      return "VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE";
    default:
      return "VK_SYSTEM_ALLOCATION_SCOPE_UNKNOWN";
  }
}

const std::string toString(VkResult result) {
  switch (result) {
    case VK_NOT_READY:
      return "VK_NOT_READY";
    case VK_TIMEOUT:
      return "VK_TIMEOUT";
    case VK_EVENT_SET:
      return "VK_EVENT_SET";
    case VK_EVENT_RESET:
      return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
      return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
      return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
      return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
      return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
      return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
      return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
      return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
      return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
      return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:
      return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
      return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_PIPELINE_COMPILE_REQUIRED:
      return "VK_PIPELINE_COMPILE_REQUIRED";
    case VK_ERROR_SURFACE_LOST_KHR:
      return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
      return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
      return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
      return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
      return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
      return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV:
      return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:
      return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
    case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:
      return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
      return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_NOT_PERMITTED_KHR:
      return "VK_ERROR_NOT_PERMITTED_KHR";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
      return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_THREAD_IDLE_KHR:
      return "VK_THREAD_IDLE_KHR";
    case VK_THREAD_DONE_KHR:
      return "VK_THREAD_DONE_KHR";
    case VK_OPERATION_DEFERRED_KHR:
      return "VK_OPERATION_DEFERRED_KHR";
    case VK_OPERATION_NOT_DEFERRED_KHR:
      return "VK_OPERATION_NOT_DEFERRED_KHR";
    case VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR:
      return "VK_ERROR_INVALID_VIDEO_STD_PARAMETERS_KHR";
    case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:
      return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
    // case VK_INCOMPATIBLE_SHADER_BINARY_EXT:
    // 	return "VK_INCOMPATIBLE_SHADER_BINARY_EXT";
    // case VK_PIPELINE_BINARY_MISSING_KHR:
    // 	return "VK_PIPELINE_BINARY_MISSING_KHR";
    // case VK_ERROR_NOT_ENOUGH_SPACE_KHR:
    // 	return "VK_ERROR_NOT_ENOUGH_SPACE_KHR";
    default:
      return "UNKNOWN_ERROR";
  }
}

const std::string toString(VkPresentModeKHR present_mode) {
  switch (present_mode) {
    case VK_PRESENT_MODE_MAILBOX_KHR:
      return "VK_PRESENT_MODE_MAILBOX_KHR";
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
      return "VK_PRESENT_MODE_IMMEDIATE_KHR";
    case VK_PRESENT_MODE_FIFO_KHR:
      return "VK_PRESENT_MODE_FIFO_KHR";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
      return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
      return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
      return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
    default:
      return "UNKNOWN_PRESENT_MODE";
  }
}

const std::string toString(VkPhysicalDeviceType type) {
  switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
      return "VK_PHYSICAL_DEVICE_TYPE_OTHER";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      return "VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      return "VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      return "VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      return "VK_PHYSICAL_DEVICE_TYPE_CPU";
    default:
      return "UNKNOWN_DEVICE_TYPE";
  }
}

const std::string toString(VkSurfaceTransformFlagBitsKHR transform_flag) {
  switch (transform_flag) {
    case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:
      return "SURFACE_TRANSFORM_IDENTITY";
    case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
      return "SURFACE_TRANSFORM_ROTATE_90";
    case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
      return "SURFACE_TRANSFORM_ROTATE_180";
    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
      return "SURFACE_TRANSFORM_ROTATE_270";
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:
      return "SURFACE_TRANSFORM_HORIZONTAL_MIRROR";
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR:
      return "SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90";
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR:
      return "SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180";
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR:
      return "SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270";
    case VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR:
      return "SURFACE_TRANSFORM_INHERIT";
    case VK_SURFACE_TRANSFORM_FLAG_BITS_MAX_ENUM_KHR:
      return "SURFACE_TRANSFORM_FLAG_BITS_MAX_ENUM";
    default:
      return "SURFACE_TRANSFORM_UNKOWN_FLAG";
  }
}

const std::string toString(VkSurfaceFormatKHR surface_format) {
  std::string surface_format_string = toString(surface_format.format) + ", ";

  switch (surface_format.colorSpace) {
    case VK_COLORSPACE_SRGB_NONLINEAR_KHR:
      surface_format_string += "VK_COLORSPACE_SRGB_NONLINEAR_KHR";
      break;
    default:
      surface_format_string += "UNKNOWN COLOR SPACE";
  }
  return surface_format_string;
}

const std::string toString(VkFormat format) {
  switch (format) {
    case VK_FORMAT_R4G4_UNORM_PACK8:
      return "VK_FORMAT_R4G4_UNORM_PACK8";
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
      return "VK_FORMAT_R4G4B4A4_UNORM_PACK16";
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      return "VK_FORMAT_B4G4R4A4_UNORM_PACK16";
    case VK_FORMAT_R5G6B5_UNORM_PACK16:
      return "VK_FORMAT_R5G6B5_UNORM_PACK16";
    case VK_FORMAT_B5G6R5_UNORM_PACK16:
      return "VK_FORMAT_B5G6R5_UNORM_PACK16";
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
      return "VK_FORMAT_R5G5B5A1_UNORM_PACK16";
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
      return "VK_FORMAT_B5G5R5A1_UNORM_PACK16";
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
      return "VK_FORMAT_A1R5G5B5_UNORM_PACK16";
    case VK_FORMAT_R8_UNORM:
      return "VK_FORMAT_R8_UNORM";
    case VK_FORMAT_R8_SNORM:
      return "VK_FORMAT_R8_SNORM";
    case VK_FORMAT_R8_USCALED:
      return "VK_FORMAT_R8_USCALED";
    case VK_FORMAT_R8_SSCALED:
      return "VK_FORMAT_R8_SSCALED";
    case VK_FORMAT_R8_UINT:
      return "VK_FORMAT_R8_UINT";
    case VK_FORMAT_R8_SINT:
      return "VK_FORMAT_R8_SINT";
    case VK_FORMAT_R8_SRGB:
      return "VK_FORMAT_R8_SRGB";
    case VK_FORMAT_R8G8_UNORM:
      return "VK_FORMAT_R8G8_UNORM";
    case VK_FORMAT_R8G8_SNORM:
      return "VK_FORMAT_R8G8_SNORM";
    case VK_FORMAT_R8G8_USCALED:
      return "VK_FORMAT_R8G8_USCALED";
    case VK_FORMAT_R8G8_SSCALED:
      return "VK_FORMAT_R8G8_SSCALED";
    case VK_FORMAT_R8G8_UINT:
      return "VK_FORMAT_R8G8_UINT";
    case VK_FORMAT_R8G8_SINT:
      return "VK_FORMAT_R8G8_SINT";
    case VK_FORMAT_R8G8_SRGB:
      return "VK_FORMAT_R8G8_SRGB";
    case VK_FORMAT_R8G8B8_UNORM:
      return "VK_FORMAT_R8G8B8_UNORM";
    case VK_FORMAT_R8G8B8_SNORM:
      return "VK_FORMAT_R8G8B8_SNORM";
    case VK_FORMAT_R8G8B8_USCALED:
      return "VK_FORMAT_R8G8B8_USCALED";
    case VK_FORMAT_R8G8B8_SSCALED:
      return "VK_FORMAT_R8G8B8_SSCALED";
    case VK_FORMAT_R8G8B8_UINT:
      return "VK_FORMAT_R8G8B8_UINT";
    case VK_FORMAT_R8G8B8_SINT:
      return "VK_FORMAT_R8G8B8_SINT";
    case VK_FORMAT_R8G8B8_SRGB:
      return "VK_FORMAT_R8G8B8_SRGB";
    case VK_FORMAT_B8G8R8_UNORM:
      return "VK_FORMAT_B8G8R8_UNORM";
    case VK_FORMAT_B8G8R8_SNORM:
      return "VK_FORMAT_B8G8R8_SNORM";
    case VK_FORMAT_B8G8R8_USCALED:
      return "VK_FORMAT_B8G8R8_USCALED";
    case VK_FORMAT_B8G8R8_SSCALED:
      return "VK_FORMAT_B8G8R8_SSCALED";
    case VK_FORMAT_B8G8R8_UINT:
      return "VK_FORMAT_B8G8R8_UINT";
    case VK_FORMAT_B8G8R8_SINT:
      return "VK_FORMAT_B8G8R8_SINT";
    case VK_FORMAT_B8G8R8_SRGB:
      return "VK_FORMAT_B8G8R8_SRGB";
    case VK_FORMAT_R8G8B8A8_UNORM:
      return "VK_FORMAT_R8G8B8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_SNORM:
      return "VK_FORMAT_R8G8B8A8_SNORM";
    case VK_FORMAT_R8G8B8A8_USCALED:
      return "VK_FORMAT_R8G8B8A8_USCALED";
    case VK_FORMAT_R8G8B8A8_SSCALED:
      return "VK_FORMAT_R8G8B8A8_SSCALED";
    case VK_FORMAT_R8G8B8A8_UINT:
      return "VK_FORMAT_R8G8B8A8_UINT";
    case VK_FORMAT_R8G8B8A8_SINT:
      return "VK_FORMAT_R8G8B8A8_SINT";
    case VK_FORMAT_R8G8B8A8_SRGB:
      return "VK_FORMAT_R8G8B8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM:
      return "VK_FORMAT_B8G8R8A8_UNORM";
    case VK_FORMAT_B8G8R8A8_SNORM:
      return "VK_FORMAT_B8G8R8A8_SNORM";
    case VK_FORMAT_B8G8R8A8_USCALED:
      return "VK_FORMAT_B8G8R8A8_USCALED";
    case VK_FORMAT_B8G8R8A8_SSCALED:
      return "VK_FORMAT_B8G8R8A8_SSCALED";
    case VK_FORMAT_B8G8R8A8_UINT:
      return "VK_FORMAT_B8G8R8A8_UINT";
    case VK_FORMAT_B8G8R8A8_SINT:
      return "VK_FORMAT_B8G8R8A8_SINT";
    case VK_FORMAT_B8G8R8A8_SRGB:
      return "VK_FORMAT_B8G8R8A8_SRGB";
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
      return "VK_FORMAT_A8B8G8R8_UNORM_PACK32";
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
      return "VK_FORMAT_A8B8G8R8_SNORM_PACK32";
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
      return "VK_FORMAT_A8B8G8R8_USCALED_PACK32";
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
      return "VK_FORMAT_A8B8G8R8_SSCALED_PACK32";
    case VK_FORMAT_A8B8G8R8_UINT_PACK32:
      return "VK_FORMAT_A8B8G8R8_UINT_PACK32";
    case VK_FORMAT_A8B8G8R8_SINT_PACK32:
      return "VK_FORMAT_A8B8G8R8_SINT_PACK32";
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
      return "VK_FORMAT_A8B8G8R8_SRGB_PACK32";
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      return "VK_FORMAT_A2R10G10B10_UNORM_PACK32";
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
      return "VK_FORMAT_A2R10G10B10_SNORM_PACK32";
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
      return "VK_FORMAT_A2R10G10B10_USCALED_PACK32";
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
      return "VK_FORMAT_A2R10G10B10_SSCALED_PACK32";
    case VK_FORMAT_A2R10G10B10_UINT_PACK32:
      return "VK_FORMAT_A2R10G10B10_UINT_PACK32";
    case VK_FORMAT_A2R10G10B10_SINT_PACK32:
      return "VK_FORMAT_A2R10G10B10_SINT_PACK32";
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      return "VK_FORMAT_A2B10G10R10_UNORM_PACK32";
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
      return "VK_FORMAT_A2B10G10R10_SNORM_PACK32";
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
      return "VK_FORMAT_A2B10G10R10_USCALED_PACK32";
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
      return "VK_FORMAT_A2B10G10R10_SSCALED_PACK32";
    case VK_FORMAT_A2B10G10R10_UINT_PACK32:
      return "VK_FORMAT_A2B10G10R10_UINT_PACK32";
    case VK_FORMAT_A2B10G10R10_SINT_PACK32:
      return "VK_FORMAT_A2B10G10R10_SINT_PACK32";
    case VK_FORMAT_R16_UNORM:
      return "VK_FORMAT_R16_UNORM";
    case VK_FORMAT_R16_SNORM:
      return "VK_FORMAT_R16_SNORM";
    case VK_FORMAT_R16_USCALED:
      return "VK_FORMAT_R16_USCALED";
    case VK_FORMAT_R16_SSCALED:
      return "VK_FORMAT_R16_SSCALED";
    case VK_FORMAT_R16_UINT:
      return "VK_FORMAT_R16_UINT";
    case VK_FORMAT_R16_SINT:
      return "VK_FORMAT_R16_SINT";
    case VK_FORMAT_R16_SFLOAT:
      return "VK_FORMAT_R16_SFLOAT";
    case VK_FORMAT_R16G16_UNORM:
      return "VK_FORMAT_R16G16_UNORM";
    case VK_FORMAT_R16G16_SNORM:
      return "VK_FORMAT_R16G16_SNORM";
    case VK_FORMAT_R16G16_USCALED:
      return "VK_FORMAT_R16G16_USCALED";
    case VK_FORMAT_R16G16_SSCALED:
      return "VK_FORMAT_R16G16_SSCALED";
    case VK_FORMAT_R16G16_UINT:
      return "VK_FORMAT_R16G16_UINT";
    case VK_FORMAT_R16G16_SINT:
      return "VK_FORMAT_R16G16_SINT";
    case VK_FORMAT_R16G16_SFLOAT:
      return "VK_FORMAT_R16G16_SFLOAT";
    case VK_FORMAT_R16G16B16_UNORM:
      return "VK_FORMAT_R16G16B16_UNORM";
    case VK_FORMAT_R16G16B16_SNORM:
      return "VK_FORMAT_R16G16B16_SNORM";
    case VK_FORMAT_R16G16B16_USCALED:
      return "VK_FORMAT_R16G16B16_USCALED";
    case VK_FORMAT_R16G16B16_SSCALED:
      return "VK_FORMAT_R16G16B16_SSCALED";
    case VK_FORMAT_R16G16B16_UINT:
      return "VK_FORMAT_R16G16B16_UINT";
    case VK_FORMAT_R16G16B16_SINT:
      return "VK_FORMAT_R16G16B16_SINT";
    case VK_FORMAT_R16G16B16_SFLOAT:
      return "VK_FORMAT_R16G16B16_SFLOAT";
    case VK_FORMAT_R16G16B16A16_UNORM:
      return "VK_FORMAT_R16G16B16A16_UNORM";
    case VK_FORMAT_R16G16B16A16_SNORM:
      return "VK_FORMAT_R16G16B16A16_SNORM";
    case VK_FORMAT_R16G16B16A16_USCALED:
      return "VK_FORMAT_R16G16B16A16_USCALED";
    case VK_FORMAT_R16G16B16A16_SSCALED:
      return "VK_FORMAT_R16G16B16A16_SSCALED";
    case VK_FORMAT_R16G16B16A16_UINT:
      return "VK_FORMAT_R16G16B16A16_UINT";
    case VK_FORMAT_R16G16B16A16_SINT:
      return "VK_FORMAT_R16G16B16A16_SINT";
    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return "VK_FORMAT_R16G16B16A16_SFLOAT";
    case VK_FORMAT_R32_UINT:
      return "VK_FORMAT_R32_UINT";
    case VK_FORMAT_R32_SINT:
      return "VK_FORMAT_R32_SINT";
    case VK_FORMAT_R32_SFLOAT:
      return "VK_FORMAT_R32_SFLOAT";
    case VK_FORMAT_R32G32_UINT:
      return "VK_FORMAT_R32G32_UINT";
    case VK_FORMAT_R32G32_SINT:
      return "VK_FORMAT_R32G32_SINT";
    case VK_FORMAT_R32G32_SFLOAT:
      return "VK_FORMAT_R32G32_SFLOAT";
    case VK_FORMAT_R32G32B32_UINT:
      return "VK_FORMAT_R32G32B32_UINT";
    case VK_FORMAT_R32G32B32_SINT:
      return "VK_FORMAT_R32G32B32_SINT";
    case VK_FORMAT_R32G32B32_SFLOAT:
      return "VK_FORMAT_R32G32B32_SFLOAT";
    case VK_FORMAT_R32G32B32A32_UINT:
      return "VK_FORMAT_R32G32B32A32_UINT";
    case VK_FORMAT_R32G32B32A32_SINT:
      return "VK_FORMAT_R32G32B32A32_SINT";
    case VK_FORMAT_R32G32B32A32_SFLOAT:
      return "VK_FORMAT_R32G32B32A32_SFLOAT";
    case VK_FORMAT_R64_UINT:
      return "VK_FORMAT_R64_UINT";
    case VK_FORMAT_R64_SINT:
      return "VK_FORMAT_R64_SINT";
    case VK_FORMAT_R64_SFLOAT:
      return "VK_FORMAT_R64_SFLOAT";
    case VK_FORMAT_R64G64_UINT:
      return "VK_FORMAT_R64G64_UINT";
    case VK_FORMAT_R64G64_SINT:
      return "VK_FORMAT_R64G64_SINT";
    case VK_FORMAT_R64G64_SFLOAT:
      return "VK_FORMAT_R64G64_SFLOAT";
    case VK_FORMAT_R64G64B64_UINT:
      return "VK_FORMAT_R64G64B64_UINT";
    case VK_FORMAT_R64G64B64_SINT:
      return "VK_FORMAT_R64G64B64_SINT";
    case VK_FORMAT_R64G64B64_SFLOAT:
      return "VK_FORMAT_R64G64B64_SFLOAT";
    case VK_FORMAT_R64G64B64A64_UINT:
      return "VK_FORMAT_R64G64B64A64_UINT";
    case VK_FORMAT_R64G64B64A64_SINT:
      return "VK_FORMAT_R64G64B64A64_SINT";
    case VK_FORMAT_R64G64B64A64_SFLOAT:
      return "VK_FORMAT_R64G64B64A64_SFLOAT";
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
      return "VK_FORMAT_B10G11R11_UFLOAT_PACK32";
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
      return "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32";
    case VK_FORMAT_D16_UNORM:
      return "VK_FORMAT_D16_UNORM";
    case VK_FORMAT_X8_D24_UNORM_PACK32:
      return "VK_FORMAT_X8_D24_UNORM_PACK32";
    case VK_FORMAT_D32_SFLOAT:
      return "VK_FORMAT_D32_SFLOAT";
    case VK_FORMAT_S8_UINT:
      return "VK_FORMAT_S8_UINT";
    case VK_FORMAT_D16_UNORM_S8_UINT:
      return "VK_FORMAT_D16_UNORM_S8_UINT";
    case VK_FORMAT_D24_UNORM_S8_UINT:
      return "VK_FORMAT_D24_UNORM_S8_UINT";
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return "VK_FORMAT_D32_SFLOAT_S8_UINT";
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
      return "VK_FORMAT_BC1_RGB_UNORM_BLOCK";
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
      return "VK_FORMAT_BC1_RGB_SRGB_BLOCK";
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
      return "VK_FORMAT_BC1_RGBA_UNORM_BLOCK";
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
      return "VK_FORMAT_BC1_RGBA_SRGB_BLOCK";
    case VK_FORMAT_BC2_UNORM_BLOCK:
      return "VK_FORMAT_BC2_UNORM_BLOCK";
    case VK_FORMAT_BC2_SRGB_BLOCK:
      return "VK_FORMAT_BC2_SRGB_BLOCK";
    case VK_FORMAT_BC3_UNORM_BLOCK:
      return "VK_FORMAT_BC3_UNORM_BLOCK";
    case VK_FORMAT_BC3_SRGB_BLOCK:
      return "VK_FORMAT_BC3_SRGB_BLOCK";
    case VK_FORMAT_BC4_UNORM_BLOCK:
      return "VK_FORMAT_BC4_UNORM_BLOCK";
    case VK_FORMAT_BC4_SNORM_BLOCK:
      return "VK_FORMAT_BC4_SNORM_BLOCK";
    case VK_FORMAT_BC5_UNORM_BLOCK:
      return "VK_FORMAT_BC5_UNORM_BLOCK";
    case VK_FORMAT_BC5_SNORM_BLOCK:
      return "VK_FORMAT_BC5_SNORM_BLOCK";
    case VK_FORMAT_BC6H_UFLOAT_BLOCK:
      return "VK_FORMAT_BC6H_UFLOAT_BLOCK";
    case VK_FORMAT_BC6H_SFLOAT_BLOCK:
      return "VK_FORMAT_BC6H_SFLOAT_BLOCK";
    case VK_FORMAT_BC7_UNORM_BLOCK:
      return "VK_FORMAT_BC7_UNORM_BLOCK";
    case VK_FORMAT_BC7_SRGB_BLOCK:
      return "VK_FORMAT_BC7_SRGB_BLOCK";
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
      return "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK";
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
      return "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK";
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK:
      return "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOK ";
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK:
      return "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK";
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
      return "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK";
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
      return "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK";
    case VK_FORMAT_EAC_R11_UNORM_BLOCK:
      return "VK_FORMAT_EAC_R11_UNORM_BLOCK";
    case VK_FORMAT_EAC_R11_SNORM_BLOCK:
      return "VK_FORMAT_EAC_R11_SNORM_BLOCK";
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
      return "VK_FORMAT_EAC_R11G11_UNORM_BLOCK";
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK:
      return "VK_FORMAT_EAC_R11G11_SNORM_BLOCK";
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_4x4_UNORM_BLOCK";
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_4x4_SRGB_BLOCK";
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_5x4_UNORM_BLOCK";
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_5x4_SRGB_BLOCK";
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_5x5_UNORM_BLOCK";
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_5x5_SRGB_BLOCK";
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_6x5_UNORM_BLOCK";
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_6x5_SRGB_BLOCK";
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_6x6_UNORM_BLOCK";
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_6x6_SRGB_BLOCK";
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_8x5_UNORM_BLOCK";
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_8x5_SRGB_BLOCK";
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_8x6_UNORM_BLOCK";
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_8x6_SRGB_BLOCK";
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_8x8_UNORM_BLOCK";
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_8x8_SRGB_BLOCK";
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_10x5_UNORM_BLOCK";
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_10x5_SRGB_BLOCK";
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_10x6_UNORM_BLOCK";
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_10x6_SRGB_BLOCK";
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_10x8_UNORM_BLOCK";
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_10x8_SRGB_BLOCK";
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_10x10_UNORM_BLOCK";
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_10x10_SRGB_BLOCK";
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_12x10_UNORM_BLOCK";
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_12x10_SRGB_BLOCK";
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK:
      return "VK_FORMAT_ASTC_12x12_UNORM_BLOCK";
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK:
      return "VK_FORMAT_ASTC_12x12_SRGB_BLOCK";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G8B8G8R8_422_UNORM:
      return "VK_FORMAT_G8B8G8R8_422_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_B8G8R8G8_422_UNORM:
      return "VK_FORMAT_B8G8R8G8_422_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
      return "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
      return "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
      return "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
      return "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
      return "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_R10X6_UNORM_PACK16:
      return "VK_FORMAT_R10X6_UNORM_PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
      return "VK_FORMAT_R10X6G10X6_UNORM_2PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
      return "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
      return "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
      return "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
      return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
      return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
      return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
      return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
      return "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_R12X4_UNORM_PACK16:
      return "VK_FORMAT_R12X4_UNORM_PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
      return "VK_FORMAT_R12X4G12X4_UNORM_2PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
      return "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
      return "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
      return "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
      return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
      return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
      return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
      return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
      return "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G16B16G16R16_422_UNORM:
      return "VK_FORMAT_G16B16G16R16_422_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_B16G16R16G16_422_UNORM:
      return "VK_FORMAT_B16G16R16G16_422_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
      return "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
      return "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
      return "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
      return "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM";
    // Provided by VK_VERSION_1_1
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
      return "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM:
      return "VK_FORMAT_G8_B8R8_2PLANE_444_UNORM";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16:
      return "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16:
      return "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM:
      return "VK_FORMAT_G16_B16R16_2PLANE_444_UNORM";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_A4R4G4B4_UNORM_PACK16:
      return "VK_FORMAT_A4R4G4B4_UNORM_PACK16";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16:
      return "VK_FORMAT_A4B4G4R4_UNORM_PACK16";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK";
    // Provided by VK_VERSION_1_3
    case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK:
      return "VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK";
    // Provided by VK_IMG_format_pvrtc
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG:
      return "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG";
    // Provided by VK_IMG_format_pvrtc
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG:
      return "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG";
    // Provided by VK_IMG_format_pvrtc
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
      return "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG";
    // Provided by VK_IMG_format_pvrtc
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
      return "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG";
    // Provided by VK_IMG_format_pvrtc
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG:
      return "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG";
    // Provided by VK_IMG_format_pvrtc
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG:
      return "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG";
    // Provided by VK_IMG_format_pvrtc
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
      return "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG";
    // Provided by VK_IMG_format_pvrtc
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG:
      return "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG";
    // Provided by VK_NV_optical_flow
    case VK_FORMAT_R16G16_S10_5_NV:
      return "VK_FORMAT_R16G16_S10_5_NV";
    // Provided by VK_KHR_maintenance5
    case VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR:
      return "VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR";
    // Provided by VK_KHR_maintenance5
    case VK_FORMAT_A8_UNORM_KHR:
      return "VK_FORMAT_A8_UNORM_KHR";
    case VK_FORMAT_UNDEFINED:
      return "VK_FORMAT_UNDEFINED";
    default:
      return "VK_FORMAT_INVALID";
  }
}
}  // namespace vk

}  // namespace amaryl

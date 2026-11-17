#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

namespace vk {
void emitImageQueueFamilyOwnershipTransfer(
    VkCommandBuffer cb, const VulkanImage& image, VkImageLayout old_layout,
    VkImageLayout new_layout, StageAccess src_stage_access, StageAccess dst_stage_access,
    uint32 src_queue_family, uint32 dst_queue_family) {
  const VkImageMemoryBarrier2 image_mem_barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = src_stage_access.stage,
      .srcAccessMask = src_stage_access.access,
      .dstStageMask = dst_stage_access.stage,
      .dstAccessMask = dst_stage_access.access,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = src_queue_family,
      .dstQueueFamilyIndex = dst_queue_family,
      .image = image.getVkImage(),
      .subresourceRange =
          {
              .aspectMask = image.getImageAspectFlags(),
              .baseMipLevel = 0,
              .levelCount = VK_REMAINING_MIP_LEVELS,
              .baseArrayLayer = 0,
              .layerCount = VK_REMAINING_ARRAY_LAYERS,
          },
  };
  const VkDependencyInfo dependency_info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &image_mem_barrier,
  };

  vkCmdPipelineBarrier2(cb, &dependency_info);
}

namespace {

struct TextureFormatProperties {
  const EFormat format{EFormat::NONE};
  const uint8 bytesPerBlock : 5 {1u};
  const uint8 blockWidth : 3 {1};
  const uint8 blockHeight : 3 {1};
  const uint8 minBlocksX : 2 {1};
  const uint8 minBlocksY : 2 {1};
  const bool depth : 1 {false};
  const bool stencil : 1 {false};
  const bool compressed : 1 {false};
  const uint8 numPlanes : 2 {1};
};

// clang-format off
#define PROPS(fmt, bpb, ...) \
  TextureFormatProperties { .format = EFormat::fmt, .bytesPerBlock = bpb, ## __VA_ARGS__ }
// clang-format on

static constexpr TextureFormatProperties properties[] = {
    PROPS(NONE, 1),
    PROPS(R_UN8, 1),
    PROPS(R_UI16, 2),
    PROPS(R_UI32, 4),
    PROPS(R_UN16, 2),
    PROPS(R_F16, 2),
    PROPS(R_F32, 4),
    PROPS(A_UN8, 1),
    PROPS(RG_UN8, 2),
    PROPS(RG_UI16, 4),
    PROPS(RG_UI32, 8),
    PROPS(RG_UN16, 4),
    PROPS(RG_F16, 4),
    PROPS(RG_F32, 8),
    PROPS(RGBA_UN8, 4),
    PROPS(RGBA_UI32, 16),
    PROPS(RGBA_F16, 8),
    PROPS(RGBA_F32, 16),
    PROPS(RGBA_SRGB8, 4),
    PROPS(BGRA_UN8, 4),
    PROPS(BGRA_SRGB8, 4),
    PROPS(A2B10G10R10_UN, 4),
    PROPS(A2R10G10B10_UN, 4),
    PROPS(A1B5G5R5_UN, 2),
    PROPS(ETC2_RGB8, 8, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(ETC2_SRGB8, 8, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(BC7_RGBA, 16, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(BC7_SRGBA, 16, .blockWidth = 4, .blockHeight = 4, .compressed = true),
    PROPS(Z_UN16, 2, .depth = true),
    PROPS(Z_UN24, 3, .depth = true),
    PROPS(Z_F32, 4, .depth = true),
    PROPS(Z_UN24_S_UI8, 4, .depth = true, .stencil = true),
    PROPS(Z_F32_S_UI8, 5, .depth = true, .stencil = true),
    // PROPS(
    //   YUV_NV12,
    //   24,
    //   .blockWidth = 4,
    //   .blockHeight = 4,
    //   .compressed = true,
    //   .numPlanes = 2),  // Subsampled 420
    // PROPS(
    //   YUV_420p,
    //   24,
    //   .blockWidth = 4,
    //   .blockHeight = 4,
    //   .compressed = true,
    //   .numPlanes = 3),  // Subsampled 420
};

#undef PROPS

static_assert(sizeof(TextureFormatProperties) <= sizeof(uint32_t));
static_assert(ARRAY_COUNT(properties) == static_cast<uint32>(EFormat::Z_F32_S_UI8) + 1u);

}  // namespace

bool operator&(uint8_t lhs, EAccelStructBuildFlagBits rhs) {
  return (lhs & static_cast<uint8>(rhs)) != 0;
}

StageAccess getPipelineStageAccess(VkImageLayout layout) {
  switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      return {
          .stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
          .access = VK_ACCESS_2_NONE,
      };
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return {
          .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
          .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      };
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return {
          .stage = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
                   VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
          .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      };
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return {
          .stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
                   VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT,
          .access = VK_ACCESS_2_SHADER_READ_BIT,
      };
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return {
          .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
          .access = VK_ACCESS_2_TRANSFER_READ_BIT,
      };
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return {
          .stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
          .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      };
    case VK_IMAGE_LAYOUT_GENERAL:
      return {
          .stage =
              VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_TRANSFER_BIT,
          .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT |
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
      };
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return {
          .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                   VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
          .access = VK_ACCESS_2_NONE | VK_ACCESS_2_SHADER_WRITE_BIT,
      };
    case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR:
      return {
          .stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
          .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT,
      };
    default:
      ASSERT(false, "Unsupported image layout transition!");
      return {
          .stage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
          .access = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
      };
  }
}

VkPipelineStageFlags2 stripGraphicsStages(VkPipelineStageFlags2 stages,
                                          bool compute_only_queue) {
  constexpr VkPipelineStageFlags2 graphics_only_stags =
      VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT |
      VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT |
      VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT |
      VK_PIPELINE_STAGE_2_TESSELLATION_CONTROL_SHADER_BIT |
      VK_PIPELINE_STAGE_2_TESSELLATION_EVALUATION_SHADER_BIT |
      VK_PIPELINE_STAGE_2_GEOMETRY_SHADER_BIT |
      VK_PIPELINE_STAGE_2_PRE_RASTERIZATION_SHADERS_BIT |
      VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT |
      VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
      VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT |
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT |
      VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;

  return compute_only_queue ? (stages & ~graphics_only_stags) : stages;
}

VkResult setDebugObjectName(VkDevice device, VkObjectType type, uint64_t handle,
                            const char* name) {
  if (!name || !*name || !vkSetDebugUtilsObjectNameEXT) {
    return VK_SUCCESS;
  }
  const VkDebugUtilsObjectNameInfoEXT ni = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = type,
      .objectHandle = handle,
      .pObjectName = name,
  };
  return vkSetDebugUtilsObjectNameEXT(device, &ni);
}

VkSpecializationInfo getPipelineShaderStageSpecializationInfo(
    SpecializationConstantDesc desc, VkSpecializationMapEntry* entires) {
  const uint32 num_entries = desc.getNumSpecializationConstants();
  if (entires) {
    for (uint32 i = 0; i < num_entries; ++i) {
      entires[i] = VkSpecializationMapEntry{
          .constantID = desc.entries[i].constantId,
          .offset = desc.entries[i].offset,
          .size = desc.entries[i].size,
      };
    }
  }

  return VkSpecializationInfo{
      .mapEntryCount = num_entries,
      .pMapEntries = entires,
      .dataSize = desc.dataSize,
      .pData = desc.data,
  };
}

VkSampleCountFlagBits getVkSampleCountFlags(uint32 num_samples,
                                            VkSampleCountFlags max_samples_mask) {
  if (num_samples <= 1 || VK_SAMPLE_COUNT_2_BIT > max_samples_mask) {
    return VK_SAMPLE_COUNT_1_BIT;
  }
  if (num_samples <= 2 || VK_SAMPLE_COUNT_4_BIT > max_samples_mask) {
    return VK_SAMPLE_COUNT_2_BIT;
  }
  if (num_samples <= 4 || VK_SAMPLE_COUNT_8_BIT > max_samples_mask) {
    return VK_SAMPLE_COUNT_4_BIT;
  }
  if (num_samples <= 8 || VK_SAMPLE_COUNT_16_BIT > max_samples_mask) {
    return VK_SAMPLE_COUNT_8_BIT;
  }
  if (num_samples <= 16 || VK_SAMPLE_COUNT_32_BIT > max_samples_mask) {
    return VK_SAMPLE_COUNT_16_BIT;
  }
  if (num_samples <= 32 || VK_SAMPLE_COUNT_64_BIT > max_samples_mask) {
    return VK_SAMPLE_COUNT_32_BIT;
  }
  return VK_SAMPLE_COUNT_64_BIT;
}

uint32 getBytesPerPixel(VkFormat format) {
  switch (format) {
    case VK_FORMAT_R8_UNORM:
      return 1;
    case VK_FORMAT_R16_UNORM:
    case VK_FORMAT_R16_SFLOAT:
      return 2;
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_B8G8R8_UNORM:
      return 3;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R16G16_SFLOAT:
    case VK_FORMAT_R32_SFLOAT:
    case VK_FORMAT_R32_UINT:
      return 4;
    case VK_FORMAT_R16G16B16_SFLOAT:
      return 6;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT:
    case VK_FORMAT_R32G32_UINT:
      return 8;
    case VK_FORMAT_R32G32B32_SFLOAT:
      return 12;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
      return 16;
    default:;
  }
  ASSERT(false, "VkFormat value not handled: %d", (int)format);
  return 1;
}

uint32 getTextureBytesPerLayer(uint32 width, uint32 height, EFormat format,
                               uint32 level) {
  const uint32 level_width = std::max(width >> level, 1u);
  const uint32 level_height = std::max(height >> level, 1u);

  const size_t format_index = static_cast<size_t>(format);
  const TextureFormatProperties props = properties[format_index];

  if (!props.compressed) {
    return props.bytesPerBlock * level_width * level_height;
  }

  const uint32 block_width = std::max((uint32)props.blockWidth, 1u);
  const uint32 block_height = std::max((uint32)props.blockHeight, 1u);
  const uint32 width_in_blocks = (level_width + block_width - 1) / block_width;
  const uint32 height_in_blocks = (level_height + block_height - 1) / block_height;

  return width_in_blocks * height_in_blocks * props.bytesPerBlock;
}

uint32 getNumImagePlanes(VkFormat format) {
  switch (format) {
    case VK_FORMAT_UNDEFINED:
      return 0;
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
      return 3;
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM:
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16:
    case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM:
      return 2;
    default:
      return 1;
  }
}

VkExtent2D getImagePlaneExtent(VkExtent2D plane0, EFormat foramt, uint32 plane) {
  // switch (format) {
  //   case EFormat::YUV_NV12:
  //     return VkExtent2D{
  //       .width = plane0.width >> plane,
  //       .height = plane0.height >> plane,
  //     };
  //   case EFormat::YUV_420p:
  //     return VkExtent2D{
  //       .width = plane0.width >> (plane ? 1 : 0),
  //       .height = plane0.height >> (plane ? 1 : 0),
  //     };
  //   default:;
  // }

  return plane0;
}

bool validateImageLimits(VkImageType image_type, VkSampleCountFlagBits sample_count,
                         const VkExtent3D& extent, const VkPhysicalDeviceLimits& limits,
                         Result* out_result) {
  if (sample_count != VK_SAMPLE_COUNT_1_BIT && !(image_type == VK_IMAGE_TYPE_2D)) {
    Result::setResult(out_result,
                      Result(Result::Code::ArgumentOutOfRange,
                             "Multisampling is supported only for 2D images"));
    return false;
  }

  if (image_type == VK_IMAGE_TYPE_2D && !(extent.width <= limits.maxImageDimension2D &&
                                          extent.height <= limits.maxImageDimension2D)) {
    Result::setResult(
        out_result, Result(Result::Code::ArgumentOutOfRange, "2D texture size exceeded"));
    return false;
  }
  if (image_type == VK_IMAGE_TYPE_3D && !(extent.width <= limits.maxImageDimension3D &&
                                          extent.height <= limits.maxImageDimension3D &&
                                          extent.depth <= limits.maxImageDimension3D)) {
    Result::setResult(
        out_result, Result(Result::Code::ArgumentOutOfRange, "3D texture size exceeded"));
    return false;
  }

  return true;
}

VmaAllocator createVmaAllocator(VkPhysicalDevice physical_device, VkDevice device,
                                VkInstance instance, uint32 api_version) {
  const VmaVulkanFunctions funcs = {
      .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
      .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
      .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
      .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
      .vkAllocateMemory = vkAllocateMemory,
      .vkFreeMemory = vkFreeMemory,
      .vkMapMemory = vkMapMemory,
      .vkUnmapMemory = vkUnmapMemory,
      .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
      .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
      .vkBindBufferMemory = vkBindBufferMemory,
      .vkBindImageMemory = vkBindImageMemory,
      .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
      .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
      .vkCreateBuffer = vkCreateBuffer,
      .vkDestroyBuffer = vkDestroyBuffer,
      .vkCreateImage = vkCreateImage,
      .vkDestroyImage = vkDestroyImage,
      .vkCmdCopyBuffer = vkCmdCopyBuffer,
      .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
      .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
      .vkBindBufferMemory2KHR = vkBindBufferMemory2,
      .vkBindImageMemory2KHR = vkBindImageMemory2,
      .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
      .vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
      .vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements,
      // .vkGetPhysicalDeviceProperties2KHR = vkGetPhysicalDeviceProperties2,
  };

  const VmaAllocatorCreateInfo ci = {
      .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
      .physicalDevice = physical_device,
      .device = device,
      .preferredLargeHeapBlockSize = 0,
      .pAllocationCallbacks = nullptr,
      .pDeviceMemoryCallbacks = nullptr,
      .pHeapSizeLimit = nullptr,
      .pVulkanFunctions = &funcs,
      .instance = instance,
      .vulkanApiVersion = api_version,
  };
  VmaAllocator vma = VK_NULL_HANDLE;
  VK_CHECK(vmaCreateAllocator(&ci, &vma));
  return vma;
}

VkIndexType toVkIndexType(EIndexFormat fmt) {
  switch (fmt) {
    case EIndexFormat::UI8:
      return VK_INDEX_TYPE_UINT8_EXT;
    case EIndexFormat::UI16:
      return VK_INDEX_TYPE_UINT16;
    case EIndexFormat::UI32:
      return VK_INDEX_TYPE_UINT32;
  };
  ASSERT(false);
  return VK_INDEX_TYPE_NONE_KHR;
}

VkPrimitiveTopology toVkPrimitiveTopology(ETopology t) {
  switch (t) {
    case ETopology::Point:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case ETopology::Line:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case ETopology::LineStrip:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case ETopology::Triangle:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case ETopology::TriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case ETopology::Patch:
      return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  }
  ASSERT(false, "Implement Topology = %u", (uint32_t)t);
  return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
}

VkAttachmentLoadOp toVkAttachmentLoadOp(ELoadOp a) {
  switch (a) {
    case ELoadOp::Load:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    case ELoadOp::Clear:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case ELoadOp::DontCare:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case ELoadOp::None:
      return VK_ATTACHMENT_LOAD_OP_NONE;
    case ELoadOp::Invalid:
      ASSERT(false);
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  }
  ASSERT(false);
  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp toVkAttachmentStoreOp(EStoreOp a) {
  switch (a) {
    case EStoreOp::Store:
      return VK_ATTACHMENT_STORE_OP_STORE;
    case EStoreOp::DontCare:
      return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case EStoreOp::None:
      return VK_ATTACHMENT_STORE_OP_NONE;
  }
  ASSERT(false);
  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkResolveModeFlagBits toVkResolveModeFlagBits(EResolveMode mode,
                                              VkResolveModeFlags supported) {
  switch (mode) {
    case EResolveMode::None:
      return VK_RESOLVE_MODE_NONE;
    case EResolveMode::SampleZero:
      return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    case EResolveMode::Average:
      return supported & VK_RESOLVE_MODE_AVERAGE_BIT ? VK_RESOLVE_MODE_AVERAGE_BIT
                                                     : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    case EResolveMode::Min:
      return supported & VK_RESOLVE_MODE_MIN_BIT ? VK_RESOLVE_MODE_MIN_BIT
                                                 : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    case EResolveMode::Max:
      return supported & VK_RESOLVE_MODE_MAX_BIT ? VK_RESOLVE_MODE_MAX_BIT
                                                 : VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
  }
  ASSERT(false);
  return VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
}

VkShaderStageFlagBits toVkShaderStage(EShaderStage stage) {
  switch (stage) {
    case EShaderStage::Vertex:
      return VK_SHADER_STAGE_VERTEX_BIT;
    // case EShaderStage::Tessellation: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    // case EShaderStage::Tese: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    // case EShaderStage::Geom: return VK_SHADER_STAGE_GEOMETRY_BIT;
    case EShaderStage::Fragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    case EShaderStage::Compute:
      return VK_SHADER_STAGE_COMPUTE_BIT;
    case EShaderStage::Task:
      return VK_SHADER_STAGE_TASK_BIT_EXT;
    case EShaderStage::Mesh:
      return VK_SHADER_STAGE_MESH_BIT_EXT;
    case EShaderStage::RayGeneration:
      return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case EShaderStage::AnyHit:
      return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case EShaderStage::ClosestHit:
      return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case EShaderStage::Miss:
      return VK_SHADER_STAGE_MISS_BIT_KHR;
    case EShaderStage::Intersection:
      return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    case EShaderStage::Callable:
      return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    default: {
      ASSERT(false, TEXT("Not support shader stage"));
      return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }
  };

  ASSERT(false);
  return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}

VkMemoryPropertyFlags toVkMemoryPropertyFlags(EStorageType storage) {
  VkMemoryPropertyFlags memFlags{0};

  switch (storage) {
    case EStorageType::Device:
      memFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
      break;
    case EStorageType::HostVisible:
      memFlags |=
          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
      break;
    case EStorageType::Memoryless:
      memFlags |=
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
      break;
  }
  return memFlags;
}

VkBuildAccelerationStructureFlagsKHR toVkBuildAccelerationStructureFlags(
    uint8_t buildFlags) {
  VkBuildAccelerationStructureFlagsKHR flags = 0;

  if (buildFlags & EAccelStructBuildFlagBits::AllowUpdate) {
    flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  }
  if (buildFlags & EAccelStructBuildFlagBits::AllowCompaction) {
    flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;
  }
  if (buildFlags & EAccelStructBuildFlagBits::PreferFastTrace) {
    flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  }
  if (buildFlags & EAccelStructBuildFlagBits::PreferFastBuild) {
    flags |= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
  }
  if (buildFlags & EAccelStructBuildFlagBits::LowMemory) {
    flags |= VK_BUILD_ACCELERATION_STRUCTURE_LOW_MEMORY_BIT_KHR;
  }

  return flags;
}

VkPolygonMode toVkPolygonMode(EPolygonMode mode) {
  switch (mode) {
    case EPolygonMode::Fill:
      return VK_POLYGON_MODE_FILL;
    case EPolygonMode::Line:
      return VK_POLYGON_MODE_LINE;
    case EPolygonMode::Point:
      return VK_POLYGON_MODE_POINT;
  }
  ASSERT(false, "Implement a missing polygon fill mode");
  return VK_POLYGON_MODE_FILL;
}

VkBlendFactor toVkBlendFactor(EBlendFactor value) {
  switch (value) {
    case EBlendFactor::Zero:
      return VK_BLEND_FACTOR_ZERO;
    case EBlendFactor::One:
      return VK_BLEND_FACTOR_ONE;
    case EBlendFactor::SrcColor:
      return VK_BLEND_FACTOR_SRC_COLOR;
    case EBlendFactor::OneMinusSrcColor:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case EBlendFactor::DstColor:
      return VK_BLEND_FACTOR_DST_COLOR;
    case EBlendFactor::OneMinusDstColor:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case EBlendFactor::SrcAlpha:
      return VK_BLEND_FACTOR_SRC_ALPHA;
    case EBlendFactor::OneMinusSrcAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case EBlendFactor::DstAlpha:
      return VK_BLEND_FACTOR_DST_ALPHA;
    case EBlendFactor::OneMinusDstAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case EBlendFactor::BlendColor:
      return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case EBlendFactor::OneMinusBlendColor:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case EBlendFactor::BlendAlpha:
      return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case EBlendFactor::OneMinusBlendAlpha:
      return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case EBlendFactor::SrcAlphaSaturated:
      return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case EBlendFactor::Src1Color:
      return VK_BLEND_FACTOR_SRC1_COLOR;
    case EBlendFactor::OneMinusSrc1Color:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case EBlendFactor::Src1Alpha:
      return VK_BLEND_FACTOR_SRC1_ALPHA;
    case EBlendFactor::OneMinusSrc1Alpha:
      return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    default:
      ASSERT(false);
      return VK_BLEND_FACTOR_ONE;  // default for unsupported values
  }
}

VkBlendOp toVkBlendOp(EBlendOp value) {
  switch (value) {
    case EBlendOp::Add:
      return VK_BLEND_OP_ADD;
    case EBlendOp::Subtract:
      return VK_BLEND_OP_SUBTRACT;
    case EBlendOp::ReverseSubtract:
      return VK_BLEND_OP_REVERSE_SUBTRACT;
    case EBlendOp::Min:
      return VK_BLEND_OP_MIN;
    case EBlendOp::Max:
      return VK_BLEND_OP_MAX;
  }

  ASSERT(false);
  return VK_BLEND_OP_ADD;
}

VkCullModeFlags toVkCullMode(ECullMode mode) {
  switch (mode) {
    case ECullMode::None:
      return VK_CULL_MODE_NONE;
    case ECullMode::Front:
      return VK_CULL_MODE_FRONT_BIT;
    case ECullMode::Back:
      return VK_CULL_MODE_BACK_BIT;
  }
  ASSERT(false, "Implement a missing cull mode");
  return VK_CULL_MODE_NONE;
}

VkFrontFace toVkFrontFace(EWindingMode mode) {
  switch (mode) {
    case EWindingMode::CCW:
      return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case EWindingMode::CW:
      return VK_FRONT_FACE_CLOCKWISE;
  }
  ASSERT(false, "Wrong winding order (cannot be more than 2)");
  return VK_FRONT_FACE_CLOCKWISE;
}

VkStencilOp toVkStencilOp(EStencilOp op) {
  switch (op) {
    case EStencilOp::Keep:
      return VK_STENCIL_OP_KEEP;
    case EStencilOp::Zero:
      return VK_STENCIL_OP_ZERO;
    case EStencilOp::Replace:
      return VK_STENCIL_OP_REPLACE;
    case EStencilOp::IncrementClamp:
      return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case EStencilOp::DecrementClamp:
      return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case EStencilOp::Invert:
      return VK_STENCIL_OP_INVERT;
    case EStencilOp::IncrementWrap:
      return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case EStencilOp::DecrementWrap:
      return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }
  ASSERT(false);
  return VK_STENCIL_OP_KEEP;
}

VkVertexInputRate toVkVertexInputRate(EVertexInputRate rate) {
  switch (rate) {
    case EVertexInputRate::Vertex:
      return VK_VERTEX_INPUT_RATE_VERTEX;
    case EVertexInputRate::Instance:
      return VK_VERTEX_INPUT_RATE_INSTANCE;
  }
  ASSERT(false);
  return VK_VERTEX_INPUT_RATE_VERTEX;
}

VkFormat toVkFormat(EVertexFormat fmt) {
  switch (fmt) {
    case EVertexFormat::Invalid:
      ASSERT(false);
      return VK_FORMAT_UNDEFINED;
    case EVertexFormat::Float1:
      return VK_FORMAT_R32_SFLOAT;
    case EVertexFormat::Float2:
      return VK_FORMAT_R32G32_SFLOAT;
    case EVertexFormat::Float3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case EVertexFormat::Float4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case EVertexFormat::Byte1:
      return VK_FORMAT_R8_SINT;
    case EVertexFormat::Byte2:
      return VK_FORMAT_R8G8_SINT;
    case EVertexFormat::Byte3:
      return VK_FORMAT_R8G8B8_SINT;
    case EVertexFormat::Byte4:
      return VK_FORMAT_R8G8B8A8_SINT;
    case EVertexFormat::UByte1:
      return VK_FORMAT_R8_UINT;
    case EVertexFormat::UByte2:
      return VK_FORMAT_R8G8_UINT;
    case EVertexFormat::UByte3:
      return VK_FORMAT_R8G8B8_UINT;
    case EVertexFormat::UByte4:
      return VK_FORMAT_R8G8B8A8_UINT;
    case EVertexFormat::Short1:
      return VK_FORMAT_R16_SINT;
    case EVertexFormat::Short2:
      return VK_FORMAT_R16G16_SINT;
    case EVertexFormat::Short3:
      return VK_FORMAT_R16G16B16_SINT;
    case EVertexFormat::Short4:
      return VK_FORMAT_R16G16B16A16_SINT;
    case EVertexFormat::UShort1:
      return VK_FORMAT_R16_UINT;
    case EVertexFormat::UShort2:
      return VK_FORMAT_R16G16_UINT;
    case EVertexFormat::UShort3:
      return VK_FORMAT_R16G16B16_UINT;
    case EVertexFormat::UShort4:
      return VK_FORMAT_R16G16B16A16_UINT;
    // Normalized variants
    case EVertexFormat::Byte2Norm:
      return VK_FORMAT_R8G8_SNORM;
    case EVertexFormat::Byte4Norm:
      return VK_FORMAT_R8G8B8A8_SNORM;
    case EVertexFormat::UByte2Norm:
      return VK_FORMAT_R8G8_UNORM;
    case EVertexFormat::UByte4Norm:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case EVertexFormat::Short2Norm:
      return VK_FORMAT_R16G16_SNORM;
    case EVertexFormat::Short4Norm:
      return VK_FORMAT_R16G16B16A16_SNORM;
    case EVertexFormat::UShort2Norm:
      return VK_FORMAT_R16G16_UNORM;
    case EVertexFormat::UShort4Norm:
      return VK_FORMAT_R16G16B16A16_UNORM;
    case EVertexFormat::Int1:
      return VK_FORMAT_R32_SINT;
    case EVertexFormat::Int2:
      return VK_FORMAT_R32G32_SINT;
    case EVertexFormat::Int3:
      return VK_FORMAT_R32G32B32_SINT;
    case EVertexFormat::Int4:
      return VK_FORMAT_R32G32B32A32_SINT;
    case EVertexFormat::UInt1:
      return VK_FORMAT_R32_UINT;
    case EVertexFormat::UInt2:
      return VK_FORMAT_R32G32_UINT;
    case EVertexFormat::UInt3:
      return VK_FORMAT_R32G32B32_UINT;
    case EVertexFormat::UInt4:
      return VK_FORMAT_R32G32B32A32_UINT;
    case EVertexFormat::HalfFloat1:
      return VK_FORMAT_R16_SFLOAT;
    case EVertexFormat::HalfFloat2:
      return VK_FORMAT_R16G16_SFLOAT;
    case EVertexFormat::HalfFloat3:
      return VK_FORMAT_R16G16B16_SFLOAT;
    case EVertexFormat::HalfFloat4:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case EVertexFormat::Int_2_10_10_10_REV:
      return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
  }
  ASSERT(false);
  return VK_FORMAT_UNDEFINED;
}

VkPresentModeKHR toVkPresentMode(EPresentMode mode) {
  switch (mode) {
    case EPresentMode::Immediate:
      return VK_PRESENT_MODE_IMMEDIATE_KHR;
    case EPresentMode::Mailbox:
      return VK_PRESENT_MODE_MAILBOX_KHR;
    case EPresentMode::FIFO:
      return VK_PRESENT_MODE_FIFO_KHR;
    case EPresentMode::FIFO_Relaxed:
      return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    case EPresentMode::Shared_Demand_Refresh:
      return VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR;
    case EPresentMode::Shared_Continuous_Refresh:
      return VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR;
    case EPresentMode::FIFO_Latest_Ready:
      return VK_PRESENT_MODE_FIFO_LATEST_READY_KHR;
    default:
      return VK_PRESENT_MODE_FIFO_KHR;
  }

#if defined(_MSC_VER) || defined(__GNUC__)
  ASSERT(false);
  return VK_PRESENT_MODE_FIFO_KHR;
#endif  // _MSC_VER
}

EPresentMode toPresentMode(VkPresentModeKHR mode) {
  switch (mode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
      return EPresentMode::Immediate;
    case VK_PRESENT_MODE_MAILBOX_KHR:
      return EPresentMode::Mailbox;
    case VK_PRESENT_MODE_FIFO_KHR:
      return EPresentMode::FIFO;
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
      return EPresentMode::FIFO_Relaxed;
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
      return EPresentMode::Shared_Demand_Refresh;
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
      return EPresentMode::Shared_Continuous_Refresh;
    case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR:
      return EPresentMode::FIFO_Latest_Ready;
    case VK_PRESENT_MODE_MAX_ENUM_KHR:
      ASSERT(false);
      return EPresentMode::FIFO;
  }
#if defined(_MSC_VER) || defined(__GNUC__)
  ASSERT(false);
  return EPresentMode::FIFO;
#endif  // _MSC_VER
}

std::vector<VkFormat> getCompatibleDepthStencilFormats(EFormat format) {
  switch (format) {
    case EFormat::Z_UN16:
      return {VK_FORMAT_D16_UNORM, VK_FORMAT_D16_UNORM_S8_UINT,
              VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT};
    case EFormat::Z_UN24:
      return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT,
              VK_FORMAT_D16_UNORM_S8_UINT};
    case EFormat::Z_F32:
      return {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
              VK_FORMAT_D24_UNORM_S8_UINT};
    case EFormat::Z_UN24_S_UI8:
      return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT};
    case EFormat::Z_F32_S_UI8:
      return {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
              VK_FORMAT_D16_UNORM_S8_UINT};
    default:
      return {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT};
  }
}

VkFormat toVkFormat(EFormat format) {
  switch (format) {
    case EFormat::NONE:
      return VK_FORMAT_UNDEFINED;
    case EFormat::R_UN8:
      return VK_FORMAT_R8_UNORM;
    case EFormat::A_UN8:
      return VK_FORMAT_A8_UNORM;
    case EFormat::R_UN16:
      return VK_FORMAT_R16_UNORM;
    case EFormat::R_F16:
      return VK_FORMAT_R16_SFLOAT;
    case EFormat::R_UI16:
      return VK_FORMAT_R16_UINT;
    case EFormat::R_UI32:
      return VK_FORMAT_R32_UINT;
    case EFormat::RG_UN8:
      return VK_FORMAT_R8G8_UNORM;
    case EFormat::RG_UI16:
      return VK_FORMAT_R16G16_UINT;
    case EFormat::RG_UI32:
      return VK_FORMAT_R32G32_UINT;
    case EFormat::RG_UN16:
      return VK_FORMAT_R16G16_UNORM;
    case EFormat::BGRA_UN8:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case EFormat::RGBA_UN8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case EFormat::RGBA_SRGB8:
      return VK_FORMAT_R8G8B8A8_SRGB;
    case EFormat::BGRA_SRGB8:
      return VK_FORMAT_B8G8R8A8_SRGB;
    case EFormat::RG_F16:
      return VK_FORMAT_R16G16_SFLOAT;
    case EFormat::RG_F32:
      return VK_FORMAT_R32G32_SFLOAT;
    case EFormat::R_F32:
      return VK_FORMAT_R32_SFLOAT;
    case EFormat::RGBA_F16:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case EFormat::RGBA_UI32:
      return VK_FORMAT_R32G32B32A32_UINT;
    case EFormat::RGBA_F32:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case EFormat::A2B10G10R10_UN:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case EFormat::A2R10G10B10_UN:
      return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case EFormat::A1B5G5R5_UN:
      return VK_FORMAT_A1B5G5R5_UNORM_PACK16;
    case EFormat::ETC2_RGB8:
      return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case EFormat::ETC2_SRGB8:
      return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
    case EFormat::BC7_RGBA:
      return VK_FORMAT_BC7_UNORM_BLOCK;
    case EFormat::BC7_SRGBA:
      return VK_FORMAT_BC7_SRGB_BLOCK;
    case EFormat::Z_UN16:
      return VK_FORMAT_D16_UNORM;
    case EFormat::Z_UN24:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case EFormat::Z_F32:
      return VK_FORMAT_D32_SFLOAT;
    case EFormat::Z_UN24_S_UI8:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case EFormat::Z_F32_S_UI8:
      return VK_FORMAT_D32_SFLOAT_S8_UINT;

      // case EFormat::YUV_NV12: return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
      // case EFormat::YUV_420p: return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
  }
#if defined(_MSC_VER) || defined(__GNUC__)
  ASSERT(false, "TextureFormat value not handled: %d", (int)format);
  return VK_FORMAT_UNDEFINED;
#endif  // _MSC_VER
}

EFormat toEFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_UNDEFINED:
      return EFormat::NONE;
    case VK_FORMAT_R8_UNORM:
      return EFormat::R_UN8;
    case VK_FORMAT_A8_UNORM:
      return EFormat::A_UN8;
    case VK_FORMAT_R16_UNORM:
      return EFormat::R_UN16;
    case VK_FORMAT_R16_SFLOAT:
      return EFormat::R_F16;
    case VK_FORMAT_R16_UINT:
      return EFormat::R_UI16;
    case VK_FORMAT_R8G8_UNORM:
      return EFormat::RG_UN8;
    case VK_FORMAT_B8G8R8A8_UNORM:
      return EFormat::BGRA_UN8;
    case VK_FORMAT_R8G8B8A8_UNORM:
      return EFormat::RGBA_UN8;
    case VK_FORMAT_R8G8B8A8_SRGB:
      return EFormat::RGBA_SRGB8;
    case VK_FORMAT_B8G8R8A8_SRGB:
      return EFormat::BGRA_SRGB8;
    case VK_FORMAT_R16G16_UNORM:
      return EFormat::RG_UN16;
    case VK_FORMAT_R16G16_SFLOAT:
      return EFormat::RG_F16;
    case VK_FORMAT_R32G32_SFLOAT:
      return EFormat::RG_F32;
    case VK_FORMAT_R16G16_UINT:
      return EFormat::RG_UI16;
    case VK_FORMAT_R32_SFLOAT:
      return EFormat::R_F32;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return EFormat::RGBA_F16;
    case VK_FORMAT_R32G32B32A32_UINT:
      return EFormat::RGBA_UI32;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
      return EFormat::RGBA_F32;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
      return EFormat::A2B10G10R10_UN;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
      return EFormat::A2R10G10B10_UN;
    case VK_FORMAT_A1B5G5R5_UNORM_PACK16:
      return EFormat::A1B5G5R5_UN;
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
      return EFormat::ETC2_RGB8;
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
      return EFormat::ETC2_SRGB8;
    case VK_FORMAT_D16_UNORM:
      return EFormat::Z_UN16;
    case VK_FORMAT_BC7_UNORM_BLOCK:
      return EFormat::BC7_RGBA;
    case VK_FORMAT_BC7_SRGB_BLOCK:
      return EFormat::BC7_SRGBA;
    case VK_FORMAT_X8_D24_UNORM_PACK32:
      return EFormat::Z_UN24;
    case VK_FORMAT_D24_UNORM_S8_UINT:
      return EFormat::Z_UN24_S_UI8;
    case VK_FORMAT_D32_SFLOAT:
      return EFormat::Z_F32;
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
      return EFormat::Z_F32_S_UI8;
    // case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
    //   return EFormat::YUV_NV12;
    // case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    //   return EFormat::YUV_420p;
    default:;
  }
  ASSERT(false, "VkFormat value not handled: %d", (int)format);
  return EFormat::NONE;
}

VkColorSpaceKHR toVkColorSpaceKHR(EColorSpace colorSpace) {
  switch (colorSpace) {
    case EColorSpace::SRGB_NONLINEAR:
      return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    case EColorSpace::SRGB_EXTENDED_LINEAR:
      return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
    case EColorSpace::HDR10:
      return VK_COLOR_SPACE_HDR10_ST2084_EXT;
    case EColorSpace::BT709_LINEAR:
      return VK_COLOR_SPACE_BT709_LINEAR_EXT;
    default:
      ASSERT(false, "Unsupported color space %u", (uint32_t)colorSpace);
      return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  }
}

EColorSpace toEColorSpace(VkColorSpaceKHR colorSpace) {
  switch (colorSpace) {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
      return EColorSpace::SRGB_NONLINEAR;
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:
      return EColorSpace::SRGB_EXTENDED_LINEAR;
    case VK_COLOR_SPACE_HDR10_ST2084_EXT:
      return EColorSpace::HDR10;
    case VK_COLOR_SPACE_BT709_LINEAR_EXT:
      return EColorSpace::BT709_LINEAR;
    default:
      ASSERT(false, "Unsupported color space %u", (uint32_t)colorSpace);
      return EColorSpace::SRGB_NONLINEAR;
  }
}

bool isDepthStencilFormat(EFormat format) {
  uint32 format_index = static_cast<uint32>(format);
  return properties[format_index].depth || properties[format_index].stencil;
}

uint32 getNumImagePlanes(EFormat format) {
  uint32 format_index = static_cast<uint32>(format);
  return properties[format_index].numPlanes;
}

VkCompareOp toVkCompareOp(ECompareOp func) {
  switch (func) {
    case ECompareOp::Never:
      return VK_COMPARE_OP_NEVER;
    case ECompareOp::Less:
      return VK_COMPARE_OP_LESS;
    case ECompareOp::Equal:
      return VK_COMPARE_OP_EQUAL;
    case ECompareOp::LessEqual:
      return VK_COMPARE_OP_LESS_OR_EQUAL;
    case ECompareOp::Greater:
      return VK_COMPARE_OP_GREATER;
    case ECompareOp::NotEqual:
      return VK_COMPARE_OP_NOT_EQUAL;
    case ECompareOp::GreaterEqual:
      return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case ECompareOp::AlwaysPass:
      return VK_COMPARE_OP_ALWAYS;
  }
  ASSERT(false, "CompareFunction value not handled: %d", func);
  return VK_COMPARE_OP_ALWAYS;
}

namespace {

VkFilter samplerFilterToVkFilter(ESamplerFilter filter) {
  switch (filter) {
    case ESamplerFilter::Nearest:
      return VK_FILTER_NEAREST;
    case ESamplerFilter::Linear:
      return VK_FILTER_LINEAR;
    case ESamplerFilter::Cubic:
      return VK_FILTER_CUBIC_EXT;
  }

  ASSERT(false, "SamplerFilter value not handled: %d", (int)filter);
  return VK_FILTER_LINEAR;
}

VkSamplerMipmapMode samplerMipMapToVkSamplerMipmapMode(ESamplerMip filter) {
  switch (filter) {
    case ESamplerMip::Disabled:
    case ESamplerMip::Nearest:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case ESamplerMip::Linear:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }

  ASSERT(false, "SamplerMipMap value not handled: %d", (int)filter);
  return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode samplerWrapModeToVkSamplerAddressMode(ESamplerWrap mode) {
  switch (mode) {
    case ESamplerWrap::Repeat:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case ESamplerWrap::Clamp:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case ESamplerWrap::ClampToBorder:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case ESamplerWrap::MirrorRepeat:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case ESamplerWrap::MirrorClampToEdge:
      return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  }

  ASSERT(false, "SamplerWrapMode value not handled: %d", (int)mode);
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

}  // namespace

VkSamplerCreateInfo samplerStateDescToVkSamplerCreateInfo(
    const SamplerStateDesc& desc, const VkPhysicalDeviceLimits& limits) {
  ASSERT(desc.mipLodMax >= desc.mipLodMin,
         "mipLodMax (%d) must be greater than or equal to mipLodMin (%d)",
         (int)desc.mipLodMax, (int)desc.mipLodMin);

  VkSamplerCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = samplerFilterToVkFilter(desc.magFilter),
      .minFilter = samplerFilterToVkFilter(desc.minFilter),
      .mipmapMode = samplerMipMapToVkSamplerMipmapMode(desc.mipMap),
      .addressModeU = samplerWrapModeToVkSamplerAddressMode(desc.wrapU),
      .addressModeV = samplerWrapModeToVkSamplerAddressMode(desc.wrapV),
      .addressModeW = samplerWrapModeToVkSamplerAddressMode(desc.wrapW),
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0.0f,
      .compareEnable = desc.depthCompareEnabled ? VK_TRUE : VK_FALSE,
      .compareOp = desc.depthCompareEnabled ? toVkCompareOp(desc.depthCompareOp)
                                            : VK_COMPARE_OP_ALWAYS,
      .minLod = static_cast<float>(desc.mipLodMin),
      .maxLod = desc.mipMap == ESamplerMip::Disabled ? static_cast<float>(desc.mipLodMin)
                                                     : static_cast<float>(desc.mipLodMax),
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };

  if (desc.maxAnisotropic > 1) {
    const bool isAnisotropicFilteringSupported = limits.maxSamplerAnisotropy > 1;
    ASSERT(isAnisotropicFilteringSupported,
           "Anisotropic filtering is not supported by the device.");
    ci.anisotropyEnable = isAnisotropicFilteringSupported ? VK_TRUE : VK_FALSE;

    if (limits.maxSamplerAnisotropy < desc.maxAnisotropic) {
      ALOGW(
          "Supplied sampler anisotropic value greater than max supported by the device, "
          "setting to "
          "%.0f",
          limits.maxSamplerAnisotropy);
    }
    ci.maxAnisotropy =
        std::min(limits.maxSamplerAnisotropy, static_cast<float>(desc.maxAnisotropic));
  }

  return ci;
}
uint32 findMemoryTypeIndex(VkPhysicalDevice physical_device, uint32 memory_type_bits,
                           VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties2 props{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
  };
  vkGetPhysicalDeviceMemoryProperties2(physical_device, &props);

  for (uint32 i = 0; i < props.memoryProperties.memoryTypeCount; i++) {
    const bool has_props =
        (props.memoryProperties.memoryTypes[i].propertyFlags & flags) == flags;
    if (static_cast<bool>(memory_type_bits & (1 << i)) & has_props) {
      return i;
    }
  }

  assert(false);
  return VK_MAX_MEMORY_TYPES;
}

VkResult allocateMemory2(VkPhysicalDevice physical_device, VkDevice device,
                         const VkMemoryRequirements2* mem_requirements2,
                         VkMemoryPropertyFlags props, VkDeviceMemory* device_memory) {
  assert(mem_requirements2);

  const VkMemoryAllocateFlagsInfo memory_allocate_flags_info{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
      .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR,
  };
  const VkMemoryAllocateInfo memory_allocate_info{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = &memory_allocate_flags_info,
      .allocationSize = mem_requirements2->memoryRequirements.size,
      .memoryTypeIndex = findMemoryTypeIndex(
          physical_device, mem_requirements2->memoryRequirements.memoryTypeBits, props),
  };

  return vkAllocateMemory(device, &memory_allocate_info, nullptr, device_memory);
}

VkSemaphore createSemaphore(VkDevice device, const char* debug_name) {
  const VkSemaphoreCreateInfo semaphore_ci{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .flags = 0,
  };

  VkSemaphore semaphore{VK_NULL_HANDLE};
  VK_CHECK(vkCreateSemaphore(device, &semaphore_ci, nullptr, &semaphore));
  VK_CHECK(setDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, (uint64)semaphore,
                              debug_name));

  return semaphore;
}

VkSemaphore createSemaphoreTimeline(VkDevice device, uint64 initial_value,
                                    const char* debug_name) {
  const VkSemaphoreTypeCreateInfo semaphore_type_ci{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = initial_value,
  };

  const VkSemaphoreCreateInfo semaphore_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &semaphore_type_ci,
      .flags = 0,
  };

  VkSemaphore semaphore{VK_NULL_HANDLE};
  VK_CHECK(vkCreateSemaphore(device, &semaphore_info, nullptr, &semaphore));
  VK_CHECK(setDebugObjectName(device, VK_OBJECT_TYPE_SEMAPHORE, (uint64)semaphore,
                              debug_name));

  return semaphore;
}

VkFence createFence(VkDevice device, const char* debug_name, bool create_with_signaled) {
  const VkFenceCreateInfo fence_ci{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = create_with_signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0u,
  };

  VkFence fence{VK_NULL_HANDLE};
  VK_CHECK(vkCreateFence(device, &fence_ci, nullptr, &fence));
  VK_CHECK(setDebugObjectName(device, VK_OBJECT_TYPE_FENCE, (uint64)fence, debug_name));

  return fence;
}

const char* getVkDeviceFaultAddressTypeString(VkDeviceFaultAddressTypeEXT type) {
#define RESULT_CASE(res) \
  case res:              \
    return #res
  switch (type) {
    RESULT_CASE(VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT);
    RESULT_CASE(VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT);
    RESULT_CASE(VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT);
    RESULT_CASE(VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT);
    RESULT_CASE(VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT);
    RESULT_CASE(VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT);
    RESULT_CASE(VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT);
    default:
      return "Unknown VkDeviceFaultAddressTypeEXT value";
  };
#undef RESULT_CASE
}

void imageMemoryBarrier2(VkCommandBuffer cmd, VkImage image, StageAccess src,
                         StageAccess dst, VkImageLayout old_layout,
                         VkImageLayout new_layout, const VkImageSubresourceRange& range) {
  const VkImageMemoryBarrier2 barrier2{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcStageMask = src.stage,
      .srcAccessMask = src.access,
      .dstStageMask = dst.stage,
      .dstAccessMask = dst.access,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = range,
  };

  const VkDependencyInfo depinfo{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier2,
  };

  vkCmdPipelineBarrier2(cmd, &depinfo);
}

VkPrimitiveTopology toVkTopology(ETopology topology) {
  switch (topology) {
    case ETopology::Point:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case ETopology::Line:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case ETopology::LineStrip:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case ETopology::Triangle:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case ETopology::TriangleStrip:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case ETopology::Patch:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    default:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
}

VkPolygonMode toVkFillMode(EFillMode fillMode) {
  switch (fillMode) {
    case EFillMode::Solid:
      return VK_POLYGON_MODE_FILL;
    case EFillMode::Wireframe:
      return VK_POLYGON_MODE_LINE;
    default:
      return VK_POLYGON_MODE_FILL;
  }
}

VkImageType toVkImageType(ETextureType type) {
  switch (type) {
    case ETextureType::Texture1D: {
      return VK_IMAGE_TYPE_1D;
    }
    case ETextureType::Texture2D:
    case ETextureType::TextureCube: {
      return VK_IMAGE_TYPE_2D;
    }
    case ETextureType::Texture3D:
      return VK_IMAGE_TYPE_3D;
    default: {
      ALOGE("Unknown texture type: %d.", (int)type);
      return VK_IMAGE_TYPE_2D;
    }
  }
}

VkImageCreateFlags toVkImageCreateFlags(ETextureType type) {
  switch (type) {
    case ETextureType::TextureCube:
      return VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    // todo: others?
    default:
      return 0;
  }
}

VkExtent2D toVkExtent2D(ETextureType texture_type, const uint32 dim[3]) {
  VkExtent2D extent{.width = dim[0], .height = 1u};

  switch (texture_type) {
    case ETextureType::Texture2D:
    case ETextureType::TextureCube:
      extent.height = dim[1];
      break;

    case ETextureType::Texture3D:
      ALOGE(TEXT("Call toVkExtent2D, but you pass a Texture3D type."));
    default:
      break;
  }
  return extent;
}

VkExtent3D toVkExtent3D(ETextureType texture_type, const uint32 dim[3]) {
  VkExtent3D extent{.width = dim[0], .height = 1u, .depth = 1u};

  switch (texture_type) {
    case ETextureType::Texture2D:
    case ETextureType::TextureCube:
      extent.height = dim[1];
      break;
    case ETextureType::Texture3D:
      extent.height = dim[1];
      extent.depth = dim[2];
      break;
    default:
      break;
  }

  return extent;
}

VkSampleCountFlagBits toVkSampleCount(uint32 sample_count) {
  switch (sample_count) {
    case 1u:
      return VK_SAMPLE_COUNT_1_BIT;
    case 2u:
      return VK_SAMPLE_COUNT_2_BIT;
    case 4u:
      return VK_SAMPLE_COUNT_4_BIT;
    case 8u:
      return VK_SAMPLE_COUNT_8_BIT;
    case 16u:
      return VK_SAMPLE_COUNT_16_BIT;
    case 32u:
      return VK_SAMPLE_COUNT_32_BIT;
    case 64u:
      return VK_SAMPLE_COUNT_64_BIT;
    default: {
      ASSERT(false, "Unsupported texture sample count %d.", sample_count)
      return VK_SAMPLE_COUNT_1_BIT;
    }
  }
}

// VkImageUsageFlags
// toVkImageUsageFlags(ETextureUsage usage_flags) {
//   VkImageUsageFlags flags = 0;
//
//   if (EnumHasAnyFlags(usage_flags, ETextureUsage::Sampled)) {
//     flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
//   }
//   if (EnumHasAnyFlags(usage_flags, ETextureUsage::Storage)) {
//     flags |= VK_IMAGE_USAGE_STORAGE_BIT;
//   }
//   if (EnumHasAnyFlags(usage_flags, ETextureUsage::ColorAttachment)) {
//     flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
//   }
//   if (EnumHasAnyFlags(usage_flags, ETextureUsage::DepthStencilAttachment)) {
//     flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
//   }
//   if (EnumHasAnyFlags(usage_flags, ETextureUsage::TransferSrc)) {
//     flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
//   }
//   if (EnumHasAnyFlags(usage_flags, ETextureUsage::TransferDst)) {
//     flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
//   }
//
//   return flags;
// }

uint32 findImageMemoryTypeIndex(uint32 memory_type_bits,
                                VkMemoryPropertyFlags required_flags,
                                const VkPhysicalDeviceMemoryProperties& mem_props) {
  for (uint32 i = 0; i < mem_props.memoryTypeCount; ++i) {
    // check the VkMemoryType support or not?
    const bool supported = (memory_type_bits & (1u << i)) != 0;
    const bool has_flags =
        (mem_props.memoryTypes[i].propertyFlags & required_flags) == required_flags;
    if (supported && has_flags) {
      return i;
    }
  }

  ASSERT(false, "Failed to find suitable image memory type.")
  return std::numeric_limits<uint32>::max();
}

VkBufferUsageFlags toVkBufferUsageFlags(EBufferUsage usage_flags) {
  VkBufferUsageFlags flags = 0;

  if (EnumHasAnyFlags(usage_flags, EBufferUsage::Vertex)) {
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::Index)) {
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::Constant)) {
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::ShaderRead)) {
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::Storage)) {
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::TransferSrc)) {
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::TransferDst)) {
    flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::Indirect)) {
    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::ShaderDeviceAddress)) {
    flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::AccelerationStructureBuildInput)) {
    flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::AccelerationStructureStorage)) {
    flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;
  }
  if (EnumHasAnyFlags(usage_flags, EBufferUsage::ShaderBindingTable)) {
    flags |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
  }

  return flags;
}

VkShaderStageFlagBits toVkShaderStageFlagBits(EShaderStage shader_type) {
  switch (shader_type) {
    case EShaderStage::Vertex:
      return VK_SHADER_STAGE_VERTEX_BIT;
    case EShaderStage::Fragment:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    case EShaderStage::Compute:
      return VK_SHADER_STAGE_COMPUTE_BIT;
    case EShaderStage::Geometry:
      return VK_SHADER_STAGE_GEOMETRY_BIT;
    case EShaderStage::Task:
      return VK_SHADER_STAGE_TASK_BIT_EXT;
    case EShaderStage::Mesh:
      return VK_SHADER_STAGE_MESH_BIT_EXT;
    case EShaderStage::RayGeneration:
      return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    case EShaderStage::ClosestHit:
      return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    case EShaderStage::Miss:
      return VK_SHADER_STAGE_MISS_BIT_KHR;
    case EShaderStage::Intersection:
      return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
    case EShaderStage::AnyHit:
      return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
    case EShaderStage::Callable:
      return VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    default:
      ASSERT(false, TEXT("Not support shader type: %d"), static_cast<int>(shader_type));
      return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
  }
}

EShaderStage toShaderType(VkShaderStageFlagBits vk_shader_stage_flag) {
  switch (vk_shader_stage_flag) {
    case VK_SHADER_STAGE_VERTEX_BIT:
      return EShaderStage::Vertex;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
      return EShaderStage::Fragment;
    case VK_SHADER_STAGE_COMPUTE_BIT:
      return EShaderStage::Compute;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
      return EShaderStage::Geometry;
    case VK_SHADER_STAGE_TASK_BIT_EXT:
      return EShaderStage::Task;
    case VK_SHADER_STAGE_MESH_BIT_EXT:
      return EShaderStage::Mesh;
    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
      return EShaderStage::RayGeneration;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
      return EShaderStage::ClosestHit;
    case VK_SHADER_STAGE_MISS_BIT_KHR:
      return EShaderStage::Miss;
    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
      return EShaderStage::Intersection;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
      return EShaderStage::AnyHit;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
      return EShaderStage::Callable;
    default:
      ASSERT(false, TEXT("Not support shader type: %d"),
             static_cast<int>(vk_shader_stage_flag));
      return EShaderStage::None;
  }
}

}  // namespace vk

}  // namespace amaryl

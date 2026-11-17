#pragma once

#include "amaryl/nrhi/vulkan/config.h"

namespace amaryl {
#if USE_DEBUG
#define VK_CHECK(expr)                                                                  \
  do {                                                                                  \
    VkResult _vk_result = expr;                                                         \
    if (_vk_result != VK_SUCCESS) {                                                     \
      ALOGW("Error %s at %d:%d", vk::toString(_vk_result).c_str(), __FILE__, __LINE__); \
    }                                                                                   \
    ASSERT(_vk_result == VK_SUCCESS);                                                   \
  } while (0)
#else
#define VK_CHECK(expr) (void)(expr)
#endif

namespace vk {

struct StageAccess {
  VkPipelineStageFlags2 stage{VK_PIPELINE_STAGE_2_NONE};
  VkAccessFlags2 access{VK_ACCESS_2_NONE};
};

StageAccess getPipelineStageAccess(VkImageLayout layout);

VkPipelineStageFlags2 stripGraphicsStages(VkPipelineStageFlags2 stages,
                                          bool compute_only_queue);

VkResult setDebugObjectName(VkDevice device, VkObjectType type, uint64 handle,
                            const char* name);

VkSpecializationInfo getPipelineShaderStageSpecializationInfo(
    SpecializationConstantDesc desc, VkSpecializationMapEntry* entires);

VmaAllocator createVmaAllocator(VkPhysicalDevice physical_device, VkDevice device,
                                VkInstance instance, uint32 api_version);

constexpr uint32 calcNumMipLevels(uint32 width, uint32 height) {
  uint32 levels = 1;
  while ((width | height) >> levels) {
    levels++;
  }

  return levels;
}

VkSampleCountFlagBits getVkSampleCountFlags(uint32 num_samples,
                                            VkSampleCountFlags max_samples_mask);

uint32 getBytesPerPixel(VkFormat format);

uint32 getTextureBytesPerLayer(uint32 width, uint32 height, EFormat format, uint32 level);

uint32 getNumImagePlanes(VkFormat format);

VkExtent2D getImagePlaneExtent(VkExtent2D plane0, EFormat foramt, uint32 plane);

VkCompareOp toVkCompareOp(ECompareOp func);

bool validateImageLimits(VkImageType image_type, VkSampleCountFlagBits sample_count,
                         const VkExtent3D& extent, const VkPhysicalDeviceLimits& limits,
                         Result* out_result);

VkIndexType toVkIndexType(EIndexFormat fmt);

VkPrimitiveTopology toVkPrimitiveTopology(ETopology t);

VkAttachmentLoadOp toVkAttachmentLoadOp(ELoadOp a);

VkAttachmentStoreOp toVkAttachmentStoreOp(EStoreOp a);

VkResolveModeFlagBits toVkResolveModeFlagBits(EResolveMode mode,
                                              VkResolveModeFlags supported);

VkShaderStageFlagBits toVkShaderStage(EShaderStage stage);

VkMemoryPropertyFlags toVkMemoryPropertyFlags(EStorageType storage);

VkBuildAccelerationStructureFlagsKHR toVkBuildAccelerationStructureFlags(
    uint8_t buildFlags);

VkPolygonMode toVkPolygonMode(EPolygonMode mode);

VkBlendFactor toVkBlendFactor(EBlendFactor value);

VkBlendOp toVkBlendOp(EBlendOp value);

VkCullModeFlags toVkCullMode(ECullMode mode);

VkFrontFace toVkFrontFace(EWindingMode mode);

VkStencilOp toVkStencilOp(EStencilOp op);

VkVertexInputRate toVkVertexInputRate(EVertexInputRate rate);

VkFormat toVkFormat(EVertexFormat fmt);

VkPresentModeKHR toVkPresentMode(EPresentMode mode);

EPresentMode toPresentMode(VkPresentModeKHR mode);

std::vector<VkFormat> getCompatibleDepthStencilFormats(EFormat format);

// convert `EFormat` to `VkFormat`
VkFormat toVkFormat(EFormat format);

EFormat toEFormat(VkFormat format);

VkColorSpaceKHR toVkColorSpaceKHR(EColorSpace colorSpace);

EColorSpace toEColorSpace(VkColorSpaceKHR colorSpace);

bool isDepthStencilFormat(EFormat format);

uint32 getNumImagePlanes(EFormat format);

VkSamplerCreateInfo samplerStateDescToVkSamplerCreateInfo(
    const SamplerStateDesc& desc, const VkPhysicalDeviceLimits& limits);
//
// return the index of memoryTypes in
// `VkPhysicalDeviceMemoryProperties2.memoryProperties.memoryTypes`
//
uint32 findMemoryTypeIndex(VkPhysicalDevice physical_device, uint32 memory_type_bits,
                           VkMemoryPropertyFlags flags);

VkResult allocateMemory2(VkPhysicalDevice physical_device, VkDevice device,
                         const VkMemoryRequirements2* mem_requirements2,
                         VkMemoryPropertyFlags props, VkDeviceMemory* device_memory);

VkSemaphore createSemaphore(VkDevice device,
                            const char* debug_name = "default_semaphore");
VkSemaphore createSemaphoreTimeline(
    VkDevice device, uint64 initial_value,
    const char* debug_name = "default_semaphore_timeline");
VkFence createFence(VkDevice device, const char* debug_name = "default_fence",
                    bool create_with_signaled = false);

const char* getVkDeviceFaultAddressTypeString(VkDeviceFaultAddressTypeEXT type);

void imageMemoryBarrier2(VkCommandBuffer cmd, VkImage image, StageAccess src,
                         StageAccess dst, VkImageLayout old_layout,
                         VkImageLayout new_layout, const VkImageSubresourceRange& range);

// old methods.
// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

// transition should be abstract to interface for different rendering backends.
struct TransitionData {
  VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
  VkAccessFlags2 access = VK_ACCESS_2_NONE;
};

struct ImageTransitionDesc {
  VkImageSubresourceRange subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = VK_REMAINING_MIP_LEVELS,
      .baseArrayLayer = 0,
      .layerCount = VK_REMAINING_ARRAY_LAYERS,
  };
  TransitionData src;
  TransitionData dst;
  uint32 srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  uint32 dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
};

void transitionImage(VkCommandBuffer cmdBuffer, VkImage image, TransitionData src,
                     TransitionData dst);

void transitionImage(VkCommandBuffer cmdBuffer, VkImage image,
                     VkImageSubresourceRange subresourceRange, TransitionData src,
                     TransitionData dst,
                     uint32 srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                     uint32 dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED);

void transitionImage(VkCommandBuffer cmdBuffer, VkImage image, uint32 mipCount,
                     uint32 layerCount, const ImageTransitionDesc* transitions,
                     uint32 transitionCount);

VkPrimitiveTopology toVkTopology(ETopology topology);
VkPolygonMode toVkFillMode(EFillMode fillMode);
VkCompareOp toVkCompareOp(ECompareOp compareOp);
VkStencilOp toVkStencilOp(EStencilOp stencilOp);
VkAttachmentLoadOp toVkAttachmentLoadOp(ELoadOp op);
VkAttachmentStoreOp toVkAttachmentStoreOp(EStoreOp op);
VkImageType toVkImageType(ETextureType type);
VkImageCreateFlags toVkImageCreateFlags(ETextureType type);
inline VkImageViewType toVkImageViewType(ETextureType type) {
  return VK_IMAGE_VIEW_TYPE_2D;
}
VkExtent2D toVkExtent2D(ETextureType texture_type, const uint32 dim[3]);
VkExtent3D toVkExtent3D(ETextureType texture_type, const uint32 dim[3]);
inline uint32 toVkArrayLayers(ETextureType texture_type, uint32 layer_count) {
  return 1u;
}
VkSampleCountFlagBits toVkSampleCount(uint32 sample_count);
// VkImageUsageFlags toVkImageUsageFlags(ETextureUsage usage_flags);
// Find memoryTypeIndex of VkMemoryAllocateInfo.
uint32 findImageMemoryTypeIndex(uint32 memory_type_bits,
                                VkMemoryPropertyFlags required_flags,
                                const VkPhysicalDeviceMemoryProperties& mem_props);
VkBufferUsageFlags toVkBufferUsageFlags(EBufferUsage usage_flags);

NRHI_API VkShaderStageFlagBits toVkShaderStageFlagBits(EShaderStage shader_type);
NRHI_API EShaderStage toShaderType(VkShaderStageFlagBits vk_shader_stage_flag);
NRHI_API const std::string toString(EShaderStage shader_type);

NRHI_API const std::string toString(VkSystemAllocationScope sys_alloc_scope);
NRHI_API const std::string toString(VkPresentModeKHR present_mode);
NRHI_API const std::string toString(VkResult result);
NRHI_API const std::string toString(VkPhysicalDeviceType type);
NRHI_API const std::string toString(VkSurfaceTransformFlagBitsKHR transform_flag);
NRHI_API const std::string toString(VkSurfaceFormatKHR surface_format);
NRHI_API const std::string toString(VkFormat format);

// raw Vulkan helpers: use this if you want to interop LightweightVK API with your own
// raw Vulkan API calls
VkDevice getVkDevice(const IContext* ctx);
VkPhysicalDevice getVkPhysicalDevice(const IContext* ctx);
VkCommandBuffer getVkCommandBuffer(const ICommandBuffer& buffer);
VkBuffer getVkBuffer(const IContext* ctx, BufferHandle buffer);
VkImage getVkImage(const IContext* ctx, TextureHandle texture);
VkImageView getVkImageView(const IContext* ctx, TextureHandle texture);
VkDeviceAddress getVkAccelerationStructureDeviceAddress(
    const IContext* ctx, AccelerationStructHandle accelStruct);
VkAccelerationStructureKHR getVkAccelerationStructure(
    const IContext* ctx, AccelerationStructHandle accelStruct);
VkBuffer getVkBuffer(const IContext* ctx, AccelerationStructHandle accelStruct);
VkPipeline getVkPipeline(const IContext* ctx, RayTracingPipelineHandle pipeline);
VkPipelineLayout getVkPipelineLayout(const IContext* ctx,
                                     RayTracingPipelineHandle pipeline);
VkDeviceSize getBufferSize(const IContext* ctx, BufferHandle handle);

// properties/limits
const VkPhysicalDeviceProperties2& getVkPhysicalDeviceProperties2(const IContext* ctx);
const VkPhysicalDeviceVulkan11Properties& getVkPhysicalDeviceVulkan11Properties(
    const IContext* ctx);
const VkPhysicalDeviceVulkan12Properties& getVkPhysicalDeviceVulkan12Properties(
    const IContext* ctx);
const VkPhysicalDeviceVulkan13Properties& getVkPhysicalDeviceVulkan13Properties(
    const IContext* ctx);

}  // namespace vk

}  // namespace amaryl

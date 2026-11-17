#include <cstring>  // memcpy

#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

namespace vk {
void VulkanBuffer::bufferSubData(const Context& ctx, size_t offset, size_t size,
                                 const void* data) {
  ASSERT(mapped_ptr_ != nullptr, TEXT("mapped_ptr_ should not be nullptr"));
  if (!mapped_ptr_) {
    return;
  }

  ASSERT(offset + size <= size_, TEXT("The offset+size larger than buffer size"));

  if (data) {
    std::memcpy((uint8*)mapped_ptr_ + offset, data, size);
  } else {
    std::memcpy((uint8*)mapped_ptr_ + offset, 0, size);
  }

  if (!is_coherent_memory_) {
    flushMappedMemory(ctx, offset, size);
  }
}

void VulkanBuffer::getBufferSubData(const Context& ctx, size_t offset, size_t size,
                                    void* data) {
  ASSERT(mapped_ptr_ != nullptr);

  if (!mapped_ptr_) {
    return;
  }

  ASSERT(offset + size <= size_, TEXT("The offset+size larger than buffer size"));

  if (!is_coherent_memory_) {
    invalidateMappedMemory(ctx, offset, size);
  }

  const uint8* src = static_cast<uint8*>(mapped_ptr_) + offset;
  std::memcpy(data, src, size);
}

void VulkanBuffer::flushMappedMemory(const Context& ctx, VkDeviceSize offset,
                                     VkDeviceSize size) const {
  if (!isMapped()) {
    return;
  }

  if (VULKAN_USE_VMA) {
    vmaFlushAllocation((VmaAllocator)ctx.getVmaAllocator(), vma_allocation_, offset,
                       size);
  } else {
    const VkMappedMemoryRange range{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = device_memory_,
        .offset = offset,
        .size = size,
    };
    vkFlushMappedMemoryRanges(ctx.getVkDevice(), 1, &range);
  }
}

void VulkanBuffer::invalidateMappedMemory(const Context& ctx, VkDeviceSize offset,
                                          VkDeviceSize size) const {
  if (!isMapped()) {
    return;
  }

  if (VULKAN_USE_VMA) {
    vmaInvalidateAllocation((VmaAllocator)ctx.getVmaAllocator(), vma_allocation_, offset,
                            size);
  } else {
    const VkMappedMemoryRange range{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = device_memory_,
        .offset = offset,
        .size = size,
    };
    vkInvalidateMappedMemoryRanges(ctx.getVkDevice(), 1, &range);
  }
}

}  // namespace vk

}  // namespace amaryl

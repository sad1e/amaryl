#include <cinttypes>

#include "amaryl/nrhi/vulkan/nvk.h"

namespace amaryl {

namespace vk {
VulkanImmediateCommands::VulkanImmediateCommands(VkDevice device,
                                                 uint32 queue_family_index,
                                                 bool has_EXT_device_fault,
                                                 const char* debug_name) {
  device_ = device;
  queue_family_index_ = queue_family_index;
  has_EXT_device_fault_ = has_EXT_device_fault;

  // the queue you don't have to create it, just get it from device.
  vkGetDeviceQueue(device_, queue_family_index_, 0, &queue_);

  const VkCommandPoolCreateInfo command_pool_ci{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
               VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = queue_family_index_,
  };

  VK_CHECK(vkCreateCommandPool(device_, &command_pool_ci, nullptr, &command_pool_));
  VK_CHECK(setDebugObjectName(device_, VK_OBJECT_TYPE_COMMAND_POOL, (uint64)command_pool_,
                              debug_name));

  const VkCommandBufferAllocateInfo command_buffer_allocate_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = command_pool_,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  for (uint32 i = 0; i != MAX_COMMAND_BUFFERS; ++i) {
    CommandBufferWrapper& buffer_wrapper = buffers_[i];
    char fence_name[MAX_DEBUG_STRING_LENGTH] = {0};
    char semaphore_name[MAX_DEBUG_STRING_LENGTH] = {0};

    if (debug_name) {
      (void)snprintf(fence_name, sizeof(fence_name) - 1, "Fence: %s (cmdbuf: %u)",
                     debug_name, i);
      (void)snprintf(semaphore_name, sizeof(semaphore_name) - 1,
                     "Semaphore: %s (cmdbuf: %u)", debug_name, i);
    }

    buffer_wrapper.semaphore = createSemaphore(device_, semaphore_name);
    buffer_wrapper.fence = createFence(device_, fence_name);
    // Why assign the value to cmdBufAllocated???
    VK_CHECK(vkAllocateCommandBuffers(device_, &command_buffer_allocate_info,
                                      &buffer_wrapper.cmdBufAllocated));

    buffer_wrapper.handle.setBufferIndexAndQueueFamilyIndex(i, queue_family_index);
  }

  char timeline_name[MAX_DEBUG_STRING_LENGTH] = {0};
  if (debug_name) {
    (void)snprintf(timeline_name, sizeof(timeline_name) - 1, "Semaphore: %s (timeline)",
                   debug_name);
  }

  submit_timeline_semaphore_ = createSemaphoreTimeline(device_, 0, timeline_name);
}

VulkanImmediateCommands::~VulkanImmediateCommands() {
  waitAll();

  for (CommandBufferWrapper& buffer_wrapper : buffers_) {
    vkDestroyFence(device_, buffer_wrapper.fence, nullptr);
    vkDestroySemaphore(device_, buffer_wrapper.semaphore, nullptr);
  }

  vkDestroySemaphore(device_, submit_timeline_semaphore_, nullptr);
  vkDestroyCommandPool(device_, command_pool_, nullptr);
}

VulkanImmediateCommands::CommandBufferWrapper& VulkanImmediateCommands::acquire() {
  if (!num_available_command_buffers_) {
    purge();
  }

  while (!num_available_command_buffers_) {
    ALOGI("Waiting for command buffers to be available");
    purge();
  }

  CommandBufferWrapper* current = nullptr;

  for (CommandBufferWrapper& buffer_wrapper : buffers_) {
    if (buffer_wrapper.cmdBuf == VK_NULL_HANDLE) {
      current = &buffer_wrapper;
      break;
    }
  }

  assert(current);

  ASSERT(num_available_command_buffers_ != 0, "No available command buffers");
  ASSERT(current, "No available command buffers");
  ASSERT(current->cmdBufAllocated != VK_NULL_HANDLE, "No command buffers allocated");

  // wait for a pending present still using this slot's semaphore before re-using it
  // (VUID-vkQueueSubmit2-semaphore-03868)
  // The `semaphore` member of any binary semaphore element of the
  // `pSignalSemaphoreInfos` member of any element of `pSubmits` must be unsignaled when
  // then semaphore signal operation it defines is executed on the device.
  if (current->semaphore == last_present_semaphore_ && last_present_fence_) {
    ASSERT(vkWaitForFences(device_, 1, &last_present_fence_, VK_TRUE, UINT64_MAX));

    last_present_semaphore_ = VK_NULL_HANDLE;
    last_present_fence_ = VK_NULL_HANDLE;
  }

  current->handle.setSubmitId(submit_counter_);
  num_available_command_buffers_--;

  current->cmdBuf = current->cmdBufAllocated;
  current->bRecording = true;
  const VkCommandBufferBeginInfo bi{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  VK_CHECK(vkBeginCommandBuffer(current->cmdBuf, &bi));
  next_submit_handle_ = current->handle;

  return *current;
}

SubmitHandle VulkanImmediateCommands::submit(CommandBufferWrapper& wrapper) {
  ASSERT(wrapper.bRecording);
  VK_CHECK(vkEndCommandBuffer(wrapper.cmdBuf));

  // waits:
  VkSemaphoreSubmitInfo wait_semaphores[3];
  uint32 num_wait_semaphores = 0;
  // from `vkAcquireNextImageKHR`, the gpu must wait swapchain image become valid, then
  // gpu can access it
  if (wait_semaphore_.semaphore) {
    wait_semaphores[num_wait_semaphores++] = wait_semaphore_;
  }
  // the last submit
  if (last_submit_semaphore_.semaphore) {
    wait_semaphores[num_wait_semaphores++] = last_submit_semaphore_;
  }
  // `waitTimelineSemaphore` will set timeline semaphore for sync cross queue, the
  // graphics queue may wait async- compute queue
  if (wait_timeline_.semaphore) {
    wait_semaphores[num_wait_semaphores++] = wait_timeline_;
  }

  // Signals:
  // 1. per-slot binary semaphore (intra-queue chain / present)
  // 2. monotonic cross-queue timeline
  // 3. an optional extral signal
  //
  const uint64 prev_timeline_value =
      last_submit_handle_.empty()
          ? 0
          : buffers_[last_submit_handle_.getBufferIndex()].signaledTimelineValue;
  const uint64 timeline_value = prev_timeline_value + 1;

  VkSemaphoreSubmitInfo signal_semaphores[] = {
      {
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
          .semaphore = wrapper.semaphore,
          .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      },
      {
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
          .semaphore = submit_timeline_semaphore_,
          .value = timeline_value,
          .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      },
      {},
  };
  uint32 num_signal_semaphores = 2;
  if (signal_semaphore_.semaphore) {
    signal_semaphores[num_signal_semaphores++] = signal_semaphore_;
  }

  wrapper.signaledTimelineValue = timeline_value;

  const VkCommandBufferSubmitInfo cmdbuf_submit_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer = wrapper.cmdBuf,
  };

  // NOTE: all semaphores. the waited semaphores, the signaled semaphores...
  const VkSubmitInfo2 submit_info2{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .waitSemaphoreInfoCount = num_wait_semaphores,
      .pWaitSemaphoreInfos = wait_semaphores,
      .commandBufferInfoCount = 1,
      .pCommandBufferInfos = &cmdbuf_submit_info,
      .signalSemaphoreInfoCount = num_signal_semaphores,
      .pSignalSemaphoreInfos = signal_semaphores,
  };

  const VkResult result = vkQueueSubmit2(queue_, 1u, &submit_info2, wrapper.fence);

  // Handle device lost error.
  if (has_EXT_device_fault_ && result == VK_ERROR_DEVICE_LOST) {
    VkDeviceFaultCountsEXT count = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT,
    };
    vkGetDeviceFaultInfoEXT(device_, &count, nullptr);
    std::vector<VkDeviceFaultAddressInfoEXT> addressInfo(count.addressInfoCount);
    std::vector<VkDeviceFaultVendorInfoEXT> vendorInfo(count.vendorInfoCount);
    std::vector<uint8_t> binary(count.vendorBinarySize);
    VkDeviceFaultInfoEXT info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT,
        .pAddressInfos = addressInfo.data(),
        .pVendorInfos = vendorInfo.data(),
        .pVendorBinaryData = binary.data(),
    };
    vkGetDeviceFaultInfoEXT(device_, &count, &info);
    ALOGW("VK_ERROR_DEVICE_LOST: %s\n", info.description);
    for (const VkDeviceFaultAddressInfoEXT& aInfo : addressInfo) {
      VkDeviceSize lowerAddress = aInfo.reportedAddress & ~(aInfo.addressPrecision - 1);
      VkDeviceSize upperAddress = aInfo.reportedAddress | (aInfo.addressPrecision - 1);
      ALOGW("...address range [ %" PRIx64 ", %" PRIx64 " ]: %s\n", lowerAddress,
            upperAddress, getVkDeviceFaultAddressTypeString(aInfo.addressType));
    }
    for (const VkDeviceFaultVendorInfoEXT& vInfo : vendorInfo) {
      ALOGW("...caused by `%s` with error code %" PRIx64 " and data %" PRIx64 "\n",
            vInfo.description, vInfo.vendorFaultCode, vInfo.vendorFaultData);
    }
    const VkDeviceSize binarySize = count.vendorBinarySize;
    if (info.pVendorBinaryData &&
        binarySize >= sizeof(VkDeviceFaultVendorBinaryHeaderVersionOneEXT)) {
      const VkDeviceFaultVendorBinaryHeaderVersionOneEXT* header = std::launder(
          reinterpret_cast<const VkDeviceFaultVendorBinaryHeaderVersionOneEXT*>(
              info.pVendorBinaryData));
      const char hexDigits[] = "0123456789abcdef";
      char uuid[VK_UUID_SIZE * 2 + 1] = {};
      for (uint32_t i = 0; i < VK_UUID_SIZE; ++i) {
        uuid[i * 2 + 0] = hexDigits[(header->pipelineCacheUUID[i] >> 4) & 0xF];
        uuid[i * 2 + 1] = hexDigits[header->pipelineCacheUUID[i] & 0xF];
      }
      ALOGW("VkDeviceFaultVendorBinaryHeaderVersionOne:");
      ALOGW("   headerSize        : %u\n", header->headerSize);
      ALOGW("   headerVersion     : %u\n", (uint32_t)header->headerVersion);
      ALOGW("   vendorID          : %u\n", header->vendorID);
      ALOGW("   deviceID          : %u\n", header->deviceID);
      ALOGW("   driverVersion     : %u\n", header->driverVersion);
      ALOGW("   pipelineCacheUUID : %s\n", uuid);
      if (header->applicationNameOffset && header->applicationNameOffset < binarySize) {
        ALOGW("   applicationName   : %s\n",
              (const char*)info.pVendorBinaryData + header->applicationNameOffset);
      }
      ALOGW("   applicationVersion: %i.%i.%i\n",
            VK_API_VERSION_MAJOR(header->applicationVersion),
            VK_API_VERSION_MINOR(header->applicationVersion),
            VK_API_VERSION_PATCH(header->applicationVersion));
      if (header->engineNameOffset && header->engineNameOffset < binarySize) {
        ALOGW("   engineName        : %s\n",
              (const char*)info.pVendorBinaryData + header->engineNameOffset);
      }
      ALOGW("   engineVersion     : %i.%i.%i\n",
            VK_API_VERSION_MAJOR(header->engineVersion),
            VK_API_VERSION_MINOR(header->engineVersion),
            VK_API_VERSION_PATCH(header->engineVersion));
      ALOGW("   apiVersion        : %i.%i.%i.%i\n",
            VK_API_VERSION_MAJOR(header->apiVersion),
            VK_API_VERSION_MINOR(header->apiVersion),
            VK_API_VERSION_PATCH(header->apiVersion),
            VK_API_VERSION_VARIANT(header->apiVersion));
    }
  }

  VK_CHECK(result);

  last_submit_semaphore_.semaphore = wrapper.semaphore;
  last_submit_handle_ = wrapper.handle;
  wait_semaphore_.semaphore = VK_NULL_HANDLE;
  wait_timeline_.semaphore = VK_NULL_HANDLE;
  signal_semaphore_.semaphore = VK_NULL_HANDLE;

  wrapper.bRecording = false;
  submit_counter_++;
  if (!submit_counter_) {
    submit_counter_++;
  }

  return last_submit_handle_;
}

void VulkanImmediateCommands::waitSemaphore(VkSemaphore semaphore) {
  ASSERT(wait_semaphore_.semaphore == VK_NULL_HANDLE);
  wait_semaphore_.semaphore = semaphore;
}

void VulkanImmediateCommands::waitTimelineSemaphore(VkSemaphore semaphore, uint64 value) {
  ASSERT(wait_timeline_.semaphore == VK_NULL_HANDLE);

  wait_timeline_.semaphore = semaphore;
  wait_timeline_.value = value;
}

void VulkanImmediateCommands::signalSemaphore(VkSemaphore semaphore,
                                              uint64 signal_value) {
  ASSERT(signal_semaphore_.semaphore == VK_NULL_HANDLE);

  signal_semaphore_.semaphore = semaphore;
  signal_semaphore_.value = signal_value;
}

VkSemaphore VulkanImmediateCommands::acquireLastSubmitSemaphore() {
  return std::exchange(last_submit_semaphore_.semaphore, VK_NULL_HANDLE);
}

uint64 VulkanImmediateCommands::getTimelineValue(SubmitHandle submit_handle) const {
  if (submit_handle.empty()) return 0u;

  ASSERT(submit_handle.getBufferIndex() < MAX_COMMAND_BUFFERS);
  ASSERT(submit_handle.getQueueFamilyIndex() == queue_family_index_);
  if (submit_handle.getBufferIndex() >= MAX_COMMAND_BUFFERS ||
      submit_handle.getQueueFamilyIndex() != queue_family_index_) {
    return 0u;
  }

  const CommandBufferWrapper& wrapper = buffers_[submit_handle.getBufferIndex()];
  return wrapper.handle.getSubmitId() == submit_handle.getSubmitId()
             ? wrapper.signaledTimelineValue
             : 0u;
}

void VulkanImmediateCommands::setLastPresentSemaphore(VkSemaphore semaphore,
                                                      VkFence fence) {
  last_present_semaphore_ = semaphore;
  last_present_fence_ = fence;
}

VkFence VulkanImmediateCommands::getVkFence(SubmitHandle submit_handle) const {
  if (submit_handle.empty()) return VK_NULL_HANDLE;

  ASSERT(submit_handle.getBufferIndex() < MAX_COMMAND_BUFFERS);
  ASSERT(submit_handle.getQueueFamilyIndex() == queue_family_index_);
  if (submit_handle.getBufferIndex() >= MAX_COMMAND_BUFFERS ||
      submit_handle.getQueueFamilyIndex() != queue_family_index_) {
    return VK_NULL_HANDLE;
  }

  return buffers_[submit_handle.getBufferIndex()].fence;
}

SubmitHandle VulkanImmediateCommands::getLastSubmitHandle() const {
  return last_submit_handle_;
}

SubmitHandle VulkanImmediateCommands::getNextSubmitHandle() const {
  return next_submit_handle_;
}

bool VulkanImmediateCommands::isSubmitted(SubmitHandle submit_handle) const {
  if (submit_handle.empty()) {
    return false;
  }

  ASSERT(submit_handle.getBufferIndex() < MAX_COMMAND_BUFFERS);
  ASSERT(submit_handle.getQueueFamilyIndex() == queue_family_index_);
  if (submit_handle.getBufferIndex() >= MAX_COMMAND_BUFFERS ||
      submit_handle.getQueueFamilyIndex() != queue_family_index_) {
    return false;
  }

  const CommandBufferWrapper& wrapper = buffers_[submit_handle.getBufferIndex()];
  if (wrapper.cmdBuf == VK_NULL_HANDLE ||
      wrapper.handle.getSubmitId() != submit_handle.getSubmitId()) {
    return true;
  }

  return !wrapper.bRecording;
}

bool VulkanImmediateCommands::isReady(SubmitHandle submit_handle,
                                      bool fast_check_no_vulkan) const {
  if (submit_handle.empty()) return false;

  ASSERT(submit_handle.getBufferIndex() < MAX_COMMAND_BUFFERS);
  ASSERT(submit_handle.getQueueFamilyIndex() == queue_family_index_);
  if (submit_handle.getBufferIndex() >= MAX_COMMAND_BUFFERS ||
      submit_handle.getQueueFamilyIndex() != queue_family_index_) {
    return false;
  }

  const CommandBufferWrapper& wrapper = buffers_[submit_handle.getBufferIndex()];
  // already recycled and not reused.
  if (wrapper.cmdBuf == VK_NULL_HANDLE) {
    return true;
  }
  // already recycled and reused by another command buffer
  if (wrapper.handle.getSubmitId() != submit_handle.getSubmitId()) {
    return true;
  }

  if (fast_check_no_vulkan) {
    return false;
  }

  return vkWaitForFences(device_, 1, &wrapper.fence, VK_TRUE, 0) == VK_SUCCESS;
}

void VulkanImmediateCommands::wait(SubmitHandle handle) {
  if (handle.empty()) {
    VK_CHECK(vkDeviceWaitIdle(device_));
    purge();
    return;
  }

  if (isReady(handle)) return;

  ASSERT(isSubmitted(handle),
         "Cannot wait for a command buffer that has not been submitted");
  if (!isSubmitted(handle)) {
    return;
  }

  VK_CHECK(vkWaitForFences(device_, 1, &buffers_[handle.getBufferIndex()].fence, VK_TRUE,
                           UINT64_MAX));

  purge();
}
void VulkanImmediateCommands::waitAll() {
  VkFence fences[MAX_COMMAND_BUFFERS];
  uint32 num_fences = 0;

  for (const CommandBufferWrapper& wrapper : buffers_) {
    if (wrapper.cmdBuf != VK_NULL_HANDLE && !wrapper.bRecording) {
      fences[num_fences++] = wrapper.fence;
    }
  }

  if (num_fences) {
    VK_CHECK(vkWaitForFences(device_, num_fences, fences, VK_TRUE, UINT64_MAX));
  }

  purge();
}

void VulkanImmediateCommands::purge() {
  const uint32 num_buffers = static_cast<uint32>(ARRAY_COUNT(buffers_));

  for (uint32 i = 0; i != num_buffers; ++i) {
    // `(i + last_submit_handle_.getBufferIndex() + 1) % num_buffers` which means the
    // submit command buffer is a ring buffer, the recycle procedure start from next
    // buffer of the newest submit slot.
    CommandBufferWrapper& buffer_wrapper =
        buffers_[(i + last_submit_handle_.getBufferIndex() + 1) % num_buffers];

    // ignore the invalid command buffer handle, or the command buffer is recording
    if (buffer_wrapper.cmdBuf == VK_NULL_HANDLE || buffer_wrapper.bRecording) {
      continue;
    }

    const VkResult result =
        vkWaitForFences(device_, 1, &buffer_wrapper.fence, VK_TRUE, 0);

    if (result == VK_SUCCESS) {
      // reset command buffer, the state of command buffer will change to initial from
      // executbale.
      VK_CHECK(vkResetCommandBuffer(buffer_wrapper.cmdBuf, VkCommandBufferResetFlags{0}));
      VK_CHECK(vkResetFences(device_, 1, &buffer_wrapper.fence));

      buffer_wrapper.cmdBuf = VK_NULL_HANDLE;
      ++num_available_command_buffers_;
    } else {
      if (result != VK_TIMEOUT) VK_CHECK(result);
    }
  }
}

}  // namespace vk

}  // namespace amaryl

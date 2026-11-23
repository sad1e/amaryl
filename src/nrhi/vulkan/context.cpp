#include <spirv_reflect.h>

#include <memory>

#include "amaryl/nrhi/descriptor.shared.h"
#include "amaryl/nrhi/vulkan/nvk.h"
#include "pystring/pystring.h"

namespace amaryl {

namespace vk {
namespace {

constexpr uint32 BINDINGS_TYPE_SIZE =
  static_cast<uint32>(EGlobalDescriptorBinding::Count);

#if PLATFORM_WINDOWS

// The `_debug_utils_callback` won't print the real position of error occured.
void _format_call_source(char* out_message, size_t out_message_size) {
  HANDLE process = GetCurrentProcess();
  SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
  SymInitialize(process, nullptr, TRUE);

  void* frames[64];
  const USHORT count = CaptureStackBackTrace(0, 64, frames, nullptr);

  SYMBOL_INFO* symbol = static_cast<SYMBOL_INFO*>(
    calloc(sizeof(SYMBOL_INFO) + MAX_SYM_NAME + sizeof(char), 1u));
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  symbol->MaxNameLen = MAX_SYM_NAME;

  strcpy_s(out_message, out_message_size, "unknown");

  for (USHORT i = 0; i < count; ++i) {
    const DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);

    IMAGEHLP_MODULE64 mod = {};
    mod.SizeOfStruct = sizeof(mod);

    if (!SymGetModuleInfo(process, addr, &mod)) {
      continue;
    }

    if (SymFromAddr(process, addr, nullptr, symbol)) {
      if (strstr(symbol->Name, "_debug_utils_callback") ||
          strstr(symbol->Name, "_format_call_source")) {
        continue;
      }
    }

    if (!strstr(mod.ImageName, ".exe")) {
      continue;
    }

    DWORD disp = 0;
    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);
    if (SymGetLineFromAddr64(process, addr, &disp, &line)) {
      sprintf_s(out_message, out_message_size, "%s:%lu", line.FileName, line.LineNumber);
    } else if (SymFromAddr(process, addr, nullptr, symbol)) {
      sprintf_s(out_message, out_message_size, "%s", symbol->Name);
    }
    break;
  }

  free(symbol);
}

#else
void _format_call_source(char* out_message, size_t out_message_size) {}
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL _debug_utils_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT msg_type,
  const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data) {
  if (severity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    return VK_FALSE;
  }

  const bool isError = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0;
  const bool isWarning =
    (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0;

  const size_t len = data->pMessage ? strlen(data->pMessage) : 128u;

  ASSERT(len < 65536);

  const auto error_name = static_cast<char*>(alloca(len + 1));
  int object = 0;
  void* handle = nullptr;
  char type_name[128] = {};
  void* msg_id = nullptr;

  if (!isError && !isWarning && data->pMessageIdName) {
    if (strcmp(data->pMessageIdName, "Loader Message") == 0) {
      return VK_FALSE;
    }
  }

  if (sscanf(data->pMessage,
        "Validation Error: [ %[^]] ] Object %i: handle = %p, type = %127s | "
        "MessageID = "
        "%p",
        error_name, &object, &handle, type_name, &msg_id) >= 2) {
    const char* message = strrchr(data->pMessage, '|') + 1;

    ALOGW(
      "%sValidation layer:\n Validation Error: %s \n Object %i: handle = %p, type = "
      "%s\n "
      "MessageID = %p \n%s \n",
      isError ? "\nERROR:\n" : "", error_name, object, handle, type_name, msg_id,
      message);
  } else {
    char error_message[512];
    _format_call_source(error_message, sizeof(error_message));

    ALOGE("%sValidation layer:\n%s\n", isError ? "\nERROR:\n" : "", data->pMessage);
  }

  if (isError) {
    if (const auto ctx = static_cast<Context*>(user_data);
      ctx->getConfig().bTerminateOnValidationEror) {
      ASSERT(false);
      std::terminate();
    }
  }

  return VK_FALSE;
}

const char* DEFAULT_VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};

bool _has_extension(const char* ext, const std::vector<VkExtensionProperties>& props) {
  for (const VkExtensionProperties& p : props) {
    if (strcmp(ext, p.extensionName) == 0) {
      return true;
    }
  }
  return false;
}

// Check the GPU support UMA or not, if the device only one heap memory, and the
// physical device support both HOST_VISIBLE and DEVICE_LOCAL, which means the device is
// unified memory architecture.
bool _is_host_visible_single_heap_memory(VkPhysicalDevice physical_device) {
  VkPhysicalDeviceMemoryProperties2 props = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
  };
  vkGetPhysicalDeviceMemoryProperties2(physical_device, &props);

  if (props.memoryProperties.memoryHeapCount != 1) {
    return false;
  }

  constexpr uint32 flag =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

  for (uint32 i = 0; i < props.memoryProperties.memoryTypeCount; ++i) {
    if ((props.memoryProperties.memoryTypes[i].propertyFlags & flag) == flag) {
      return true;
    }
  }

  return false;
}

uint32 _find_queue_family_index(VkPhysicalDevice physical_device, VkQueueFlags flags) {
  uint32 queue_family_count = 0;

  vkGetPhysicalDeviceQueueFamilyProperties2(
    physical_device, &queue_family_count, nullptr);

  std::vector<VkQueueFamilyProperties2> props(queue_family_count);
  for (VkQueueFamilyProperties2& p : props) {
    p.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
  }
  vkGetPhysicalDeviceQueueFamilyProperties2(
    physical_device, &queue_family_count, props.data());

  auto find_dedicated_queue_family_index = [&props](VkQueueFlags require,
                                             VkQueueFlags avoid) -> uint32 {
    for (uint32 i = 0; i != props.size(); ++i) {
      const VkQueueFamilyProperties& p = props[i].queueFamilyProperties;
      const bool is_suitable = (p.queueFlags & require) == require;
      const bool is_dedicated = (p.queueFlags & avoid) == 0;
      if (p.queueCount && is_suitable && is_dedicated) return i;
    }

    return DeviceQueues::INVALID;
  };

  if (flags & VK_QUEUE_COMPUTE_BIT) {
    const uint32 q = find_dedicated_queue_family_index(flags, VK_QUEUE_GRAPHICS_BIT);
    if (q != DeviceQueues::INVALID) {
      return q;
    }
  }

  if (flags & VK_QUEUE_TRANSFER_BIT) {
    const uint32 q = find_dedicated_queue_family_index(flags, VK_QUEUE_GRAPHICS_BIT);
    if (q != DeviceQueues::INVALID) {
      return q;
    }
  }

  return find_dedicated_queue_family_index(flags, 0);
}

struct DeferredTask final {
  DeferredTask(std::packaged_task<void()>&& task, RetirementPoint retirement)
      : task(std::move(task)), retirement(retirement) {}

  std::packaged_task<void()> task;
  RetirementPoint retirement;
};

Result _validate_range(
  const VkExtent3D& ext, uint32 num_levels, const TextureRangeDesc& range) {
  if (!(range.dimensions.width > 0 && range.dimensions.height > 0 ||
        range.dimensions.depth > 0 || range.numLayers > 0 || range.numMipLevels > 0)) {
    return Result{Result::Code::ArgumentOutOfRange,
      "Width, height, depth, num layers and num mip levels must be > 0"};
  }
  if (range.mipLevel > num_levels) {
    return Result{
      Result::Code::ArgumentOutOfRange, "range.mipLevel exceeds texture mip-levels"};
  }
  const uint32 width = std::max(ext.width >> range.mipLevel, 1u);
  const uint32 height = std::max(ext.height >> range.mipLevel, 1u);
  const uint32 depth = std::max(ext.depth >> range.mipLevel, 1u);

  if (range.dimensions.width > width || range.dimensions.height > height ||
      range.dimensions.depth > depth) {
    return Result{
      Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }

  if (range.offset.x > width - range.dimensions.width ||    //
      range.offset.y > height - range.dimensions.height ||  //
      range.offset.z > depth - range.dimensions.depth) {
    return Result{
      Result::Code::ArgumentOutOfRange, "range dimensions exceed texture dimensions"};
  }

  return Result{};
}

VkPipelineShaderStageCreateInfo _get_pipeline_shader_stage_create_info(
  VkShaderStageFlagBits stage, VkShaderModule module, const char* entry_point,
  const VkSpecializationInfo* spec_info) {
  return VkPipelineShaderStageCreateInfo{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .stage = stage,
    .module = module,
    .pName = entry_point ? entry_point : "main",
    .pSpecializationInfo = spec_info,
  };
}

}  // namespace

struct ContextImpl final {
  void setMaxCombinedImageSamplerDescriptorCount(uint32 v) {
    max_combined_image_sampler_descriptor_count_ = v;
  }

  void setVmaAllocator(VmaAllocator vma_allocator) { vma_allocator_ = vma_allocator; }

  CommandBuffer& getCurrentCommandBuffer() { return graphics_command_buffer_; }
  CommandBuffer& getCurrentComputeCommandBuffer() { return compute_command_buffer_; }

 private:
  VmaAllocator vma_allocator_;
  CommandBuffer graphics_command_buffer_;
  CommandBuffer compute_command_buffer_;

  std::vector<DeferredTask> deferred_tasks_;

  // NOTE: ignored all other multiple plane related fields.
  uint32 max_combined_image_sampler_descriptor_count_{0u};

  friend struct Context;
};

Context::Context(
  const ContextConfig& config, void* window, void* display, VkSurfaceKHR surface)
    : surface_(surface), config_(config) {
  pimpl_ = std::make_unique<ContextImpl>();

  if (volkInitialize() != VK_SUCCESS) {
    ALOGE("volkInitialize() failed");
    exit(255);
  }

  if (!createInstance().isOk()) {
    ALOGE("createInstance() failed");
    exit(255);
  }

  if (!surface) {
    createSurface(window, display);
  }
}

Context::~Context() {
  VK_CHECK(vkDeviceWaitIdle(device_));

  staging_device_.reset(nullptr);

  swapchain_.reset(nullptr);

  vkDestroySemaphore(device_, timeline_semaphore_, nullptr);

  destroy(dummy_texture_);
  waitDeferredTasks(true);

  if (shader_module_pool_.getNumObjects()) {
    ALOGW("Leaked %u shader modules", shader_module_pool_.getNumObjects());
  }
  if (render_pipeline_pool_.getNumObjects()) {
    ALOGW("Leaked %u render_pipeline modules", render_pipeline_pool_.getNumObjects());
  }
  if (compute_pipeline_pool_.getNumObjects()) {
    ALOGW("Leaked %u compute_pipeline modules", compute_pipeline_pool_.getNumObjects());
  }
  if (ray_tracing_pipeline_pool_.getNumObjects()) {
    ALOGW("Leaked %u ray_tracing_pipeline modules",
      ray_tracing_pipeline_pool_.getNumObjects());
  }
  if (sampler_pool_.getNumObjects() > 1) {
    ALOGW("Leaked %u samplers", sampler_pool_.getNumObjects() - 1);
  }
  if (buffer_pool_.getNumObjects()) {
    ALOGW("Leaked %u buffers", buffer_pool_.getNumObjects());
  }
  if (texture_pool_.getNumObjects()) {
    ALOGW("Leaked %u textures", texture_pool_.getNumObjects());
  }
  if (query_pool_.getNumObjects()) {
    ALOGW("Leaked %u query pools", query_pool_.getNumObjects());
  }
  if (acceleration_structure_pool_.getNumObjects()) {
    ALOGW(
      "Leaked %u acceleration structures", acceleration_structure_pool_.getNumObjects());
  }

  vkDestroySampler(device_, sampler_pool_.front(), nullptr);

  ray_tracing_pipeline_pool_.clear();
  acceleration_structure_pool_.clear();
  waitDeferredTasks(true);

  sampler_pool_.clear();
  compute_pipeline_pool_.clear();
  render_pipeline_pool_.clear();
  shader_module_pool_.clear();
  texture_pool_.clear();
  query_pool_.clear();
  buffer_pool_.clear();

  immediate_compute_.reset(nullptr);
  immediate_.reset(nullptr);

  for (const DescriptorSet& descriptor_set : dst_sets_) {
    vkDestroyDescriptorPool(device_, descriptor_set.dstPool, nullptr);
    vkDestroyDescriptorSetLayout(device_, descriptor_set.dstSetLayout, nullptr);
  }

  vkDestroyDescriptorSetLayout(device_, input_attachments_dst_set_layout_, nullptr);
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  vkDestroyPipelineCache(device_, pipeline_cache_, nullptr);

  if (VULKAN_USE_VMA) {
    vmaDestroyAllocator(pimpl_->vma_allocator_);
  }

  vkDestroyDevice(device_, nullptr);

  if (debug_utils_messenger_) {
    vkDestroyDebugUtilsMessengerEXT(instance_, debug_utils_messenger_, nullptr);
  }

  vkDestroyInstance(instance_, nullptr);
}

ICommandBuffer& Context::acquireCommandBuffer(bool dedicated_compute) {
  if (dedicated_compute && !immediate_compute_) {
    return pimpl_->graphics_command_buffer_.ctx_ ? pimpl_->graphics_command_buffer_
                                                 : acquireCommandBuffer(false);
  }

  CommandBuffer& command_buffer = dedicated_compute
                                    ? pimpl_->getCurrentComputeCommandBuffer()
                                    : pimpl_->getCurrentCommandBuffer();

  ASSERT(
    !command_buffer.ctx_, "Cannot acquire more than 1 command buffer simultaneously");

  // NOTE: this will use move ctor.
  command_buffer =
    dedicated_compute
      ? CommandBuffer(this, *immediate_compute_, device_queues_.computeQueueFamilyIndex)
      : CommandBuffer(this, *immediate_, device_queues_.graphicsQueueFamilyIndex);

  return command_buffer;
}

SubmitHandle Context::submit(ICommandBuffer& commandBuffer, TextureHandle present,
  const Span<TextureHandle>& release) {
  auto command_buffer = static_cast<CommandBuffer*>(&commandBuffer);

  ASSERT(command_buffer);
  ASSERT(command_buffer->ctx_);
  ASSERT(command_buffer->wrapper_);

  command_buffer->trackResource(present);
  for (TextureHandle handle : release) {
    command_buffer->trackResource(handle);
  }

  if (present) {
    const VulkanImage& image = *texture_pool_.get(present);

    ASSERT(image.is_swapchain_image_);

    image.transitionLayout(command_buffer->wrapper_->cmdBuf,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VkImageSubresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
      },
      {}, command_buffer->isComputeOnlyQueue());
  }

  const bool should_present = hasSwapchain() && present;

  if (should_present) {
    // if we a presenting a swapchain image, signal our timeline semaphore
    const uint64 signal_value =
      swapchain_->current_frame_index_ + swapchain_->getNumSwapchainImages();
    // we wait for this value next time we want to acquire this swapchain image
    swapchain_->timeline_wait_values_[swapchain_->current_image_index_] = signal_value;
    immediate_->signalSemaphore(timeline_semaphore_, signal_value);
  }

  ASSERT(command_buffer->immediate_);

  VulkanImmediateCommands& imm = *command_buffer->immediate_;

  // Queue family ownership transfer.
  //
  // QFOT release: hand the named images to the other queue (destination implied by this
  // command buffer's queue). The matching acquire is emitted automatically when the
  // destination queue first uses the image
  if (immediate_compute_ && !release.empty()) {
    const bool is_compute = command_buffer->isComputeOnlyQueue();

    const uint32 src_queue_family = is_compute ? device_queues_.computeQueueFamilyIndex
                                               : device_queues_.graphicsQueueFamilyIndex;
    const uint32 dst_queue_family = is_compute ? device_queues_.graphicsQueueFamilyIndex
                                               : device_queues_.computeQueueFamilyIndex;

    // compute -> graphics: graphics samples the storage output as
    // SHADER_READ_ONLY_OPTIMAL graphics -> compute: GENERAL is valid for both sampled
    // and storage reads on compute
    const VkImageLayout dst_layout =
      is_compute ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
    for (TextureHandle texture_handle : release) {
      ASSERT(!texture_handle.empty());

      VulkanImage& image = *texture_pool_.get(texture_handle);
      const VkImageLayout old_layout = image.layout_;
      // compute->graphics: the producer's scope is the compute storage write
      // graphics->compute: implied by the final layout;
      const StageAccess src_stage_access =
        is_compute ? StageAccess{.stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                       .access = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT}
                   : getPipelineStageAccess(old_layout);
      emitImageQueueFamilyOwnershipTransfer(command_buffer->wrapper_->cmdBuf, image,
        old_layout, dst_layout, src_stage_access, StageAccess{}, src_queue_family,
        dst_queue_family);

      image.qfot_src_layout_ = old_layout;
      image.qfot_dst_layout_ = dst_layout;
      image.layout_ = dst_layout;

      // the estination queue completes the transfer on first use
      image.pending_acquire_src_family_ = src_queue_family;
    }
  }

  if (&imm == immediate_compute_.get()) {
    // Compute waits on the graphics submit timeline for BOTH the write-after-read guard
    // (don't overwrite images the graphics queue may still be reading) and any explicit
    // Dependencies::waitGraphics (read-after-write). A single wait on the highest value
    // covers both.
    const SubmitHandle last_graphics = immediate_->getLastSubmitHandle();
    const uint64 wait_value =
      std::max<uint64>(immediate_->getTimelineValue(last_graphics),
        command_buffer->cross_queue_graphics_wait_value_);
    if (wait_value) {
      immediate_compute_->waitTimelineSemaphore(
        immediate_->getTimelineSemaphore(), wait_value);
    }
  } else if (command_buffer->cross_queue_compute_wait_value_) {
    // Cross-queue execution dependency: wait until the required async-compute submits
    // complete (from Dependencies::waitCompute)
    imm.waitTimelineSemaphore(immediate_compute_->getTimelineSemaphore(),
      command_buffer->cross_queue_compute_wait_value_);
  }

  command_buffer->last_submit_handle_ = immediate_->submit(*command_buffer->wrapper_);

  if (should_present) {
    swapchain_->present(immediate_->acquireLastSubmitSemaphore());
  }

  processDeferredTasks();

  SubmitHandle handle = command_buffer->last_submit_handle_;

  // Reset the just-submitted slot (graphics or async-compute). Consumers reference a
  // compute submission by SubmitHandle (a value looked up against the compute queue's
  // timeline), not by a CommandBuffer pointer, so nothing needs this object to outlive
  // submit().
  if (&imm == immediate_compute_.get()) {
    pimpl_->compute_command_buffer_ = {};
  } else {
    pimpl_->graphics_command_buffer_ = {};
  }

  return handle;
}

void Context::wait(SubmitHandle handle) {
  if (handle.empty()) {
    immediate_->wait(handle);
    if (immediate_compute_) {
      immediate_compute_->waitAll();
    }
    processDeferredTasks();
    return;
  }

  VulkanImmediateCommands* queue = immediate_.get();
  if (immediate_compute_ &&
      handle.getQueueFamilyIndex() == device_queues_.computeQueueFamilyIndex) {
    queue = immediate_compute_.get();
  } else {
    ASSERT(handle.getQueueFamilyIndex() == device_queues_.graphicsQueueFamilyIndex);
  }

  ASSERT(queue != nullptr);
  if (!queue) {
    return;
  }

  queue->wait(handle);
  processDeferredTasks();
}

bool Context::isSubmitComplete(SubmitHandle handle) const {
  if (handle.empty()) {
    return false;
  }

  if (immediate_compute_ &&
      handle.getQueueFamilyIndex() == device_queues_.computeQueueFamilyIndex) {
    return immediate_compute_->isReady(handle);
  }

  ASSERT(handle.getQueueFamilyIndex() == device_queues_.graphicsQueueFamilyIndex);
  if (handle.getQueueFamilyIndex() != device_queues_.graphicsQueueFamilyIndex) {
    return false;
  }
  return immediate_->isReady(handle);
}

Holder<BufferHandle> Context::createBuffer(
  const BufferDesc& desc, const char* debug_name, Result* out_result) {
  BufferDesc d = desc;

  if (debug_name && *debug_name) {
    d.debugName = debug_name;
  }

  // The cpu need access
  if (!require_staging_for_device_buffers_ && (d.storageType == EStorageType::Device)) {
    d.storageType = EStorageType::HostVisible;
  }

  VkBufferUsageFlags usage_flags =
    (d.storageType == EStorageType::Device)
      ? VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
      : 0;

  if (d.usage == 0) {
    Result::setResult(
      out_result, Result::Code::ArgumentOutOfRange, "Invalid buffer usage");
    return {};
  }

  EBufferUsageBits buffer_usage_bits = static_cast<EBufferUsageBits>(d.usage);
  if (EnumHasAnyFlags(buffer_usage_bits, EBufferUsageBits::Index)) {
    usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (EnumHasAnyFlags(buffer_usage_bits, EBufferUsageBits::Vertex)) {
    usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (EnumHasAnyFlags(buffer_usage_bits, EBufferUsageBits::Uniform)) {
    usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |  //
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |    //
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  if (EnumHasAnyFlags(buffer_usage_bits, EBufferUsageBits::Storage)) {
    usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |  //
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |    //
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |  //
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |    //
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT |    //
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  if (EnumHasAnyFlags(buffer_usage_bits, EBufferUsageBits::Indirect)) {
    usage_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |  //
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  if (EnumHasAnyFlags(buffer_usage_bits, EBufferUsageBits::ShaderBindingTable)) {
    usage_flags |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |  //
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  if (EnumHasAnyFlags(
        buffer_usage_bits, EBufferUsageBits::AccelStructBuildInputReadOnly)) {
    usage_flags |=
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |  //
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |                             //
      VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  }

  if (EnumHasAnyFlags(buffer_usage_bits, EBufferUsageBits::AccelStructStorage)) {
    usage_flags |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |  //
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  }

  const VkMemoryPropertyFlags mem_flags = toVkMemoryPropertyFlags(d.storageType);

  Result result;
  BufferHandle handle =
    createBuffer(desc.size, usage_flags, mem_flags, &result, desc.debugName);

  if (!result.isOk()) {
    Result::setResult(out_result, result);
    return {};
  }

  if (desc.data) {
    upload(handle, desc.data, desc.size, 0);
  }

  Result::setResult(out_result, Result());

  return {this, handle};
}

Holder<SamplerHandle> Context::createSampler(
  const SamplerStateDesc& desc, Result* out_result) {
  Result result;
  const VkSamplerCreateInfo info =
    samplerStateDescToVkSamplerCreateInfo(desc, getVkPhysicalDeviceProperties().limits);
  SamplerHandle handle = createSampler(info, &result, desc.debugName);

  if (!result.isOk()) {
    Result::setResult(
      out_result, Result(Result::Code::RuntimeError, "Cannot create Sampler"));
    return {};
  }

  Result::setResult(out_result, result);

  return {this, handle};
}

Holder<TextureHandle> Context::createTexture(
  const TextureDesc& desc, const char* debug_name, Result* out_result) {
  TextureDesc d(desc);

  if (debug_name && *debug_name) {
    d.debugName = debug_name;
  }

  // acquire the vulkan texture format.
  // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
  auto get_closest_depth_stencil_format = [this](EFormat desired_format) -> VkFormat {
    const auto compatible_df_formats = getCompatibleDepthStencilFormats(desired_format);
    for (VkFormat format : compatible_df_formats) {
      if (std::find(device_depth_formats_.cbegin(), device_depth_formats_.cend(),
            format) != device_depth_formats_.cend()) {
        return format;
      }
    }

    return !device_depth_formats_.empty() ? device_depth_formats_.front()
                                          : VK_FORMAT_D24_UNORM_S8_UINT;
  };

  const VkFormat format = isDepthStencilFormat(d.format)
                            ? get_closest_depth_stencil_format(d.format)
                            : toVkFormat(d.format);

  ASSERT(format != VK_FORMAT_UNDEFINED, "Invalid VkFormat.");

  const ETextureType tex_type = d.type;
  const bool is_valid_type =
    tex_type == ETextureType::Texture1D || tex_type == ETextureType::Texture2D ||
    tex_type == ETextureType::Texture3D || tex_type == ETextureType::TextureCube;
  if (!is_valid_type) {
    Result::setResult(out_result, Result::Code::RuntimeError);
    return {};
  }
  // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  if (d.numMipLevels == 0u) {
    ALOGW("The number of mip levels specified must be greater than 0");
    d.numMipLevels = 1u;
  }

  // multi-sampled textures not contains mip-maps.
  if (d.numSamples > 1u && d.numMipLevels != 1u) {
    ALOGW(R"(The number of mip levels for multi-sampled image should be 1)");
    Result::setResult(out_result, Result::Code::ArgumentOutOfRange,
      "The number of mip-levles for multi-sampled images should be 1");

    return {};
  }

  if (d.numSamples > 1u && tex_type == ETextureType::Texture3D) {
    ALOGW("Multi-sampled 3D images are not supported");
    Result::setResult(out_result, Result::Code::ArgumentOutOfRange,
      "Multi-sampled 3D images are not supported");

    return {};
  }

  if (!(d.numMipLevels <= calcNumMipLevels(d.dimensions.width, d.dimensions.height))) {
    Result::setResult(out_result, Result::Code::ArgumentOutOfRange,
      "The number of specified mip-levels is greater than the maximum possible number "
      "of "
      "mip-levels.");

    return {};
  }

  if (!any(d.usage)) {
    ALOGW("Texture usage flags are not set, use sampled by default.");
    d.usage = ETextureUsageBits::Sampled;
  }

  // confirm the usage flags of image
  VkImageUsageFlags usage_flags =
    (d.storage == EStorageType::Device) ? VK_IMAGE_USAGE_TRANSFER_DST_BIT : 0;

  if (EnumHasAnyFlags(d.usage, ETextureUsageBits::Sampled)) {
    usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (EnumHasAnyFlags(d.usage, ETextureUsageBits::Storage)) {
    ASSERT(d.numSamples <= 1, "Storage images cannot be multi-sampled.");
    usage_flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (EnumHasAnyFlags(d.usage, ETextureUsageBits::Attachment)) {
    usage_flags |= isDepthStencilFormat(d.format)
                     ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                     : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // memoryless attachment should be transient.
    if (d.storage == EStorageType::Memoryless) {
      usage_flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    }
  }
  if (EnumHasAnyFlags(d.usage, ETextureUsageBits::InputAttachment)) {
    ASSERT(desc.usage & ETextureUsageBits::Attachment,
      TEXT("Input attachments must be TextureUsageBits_Attachment"));
    usage_flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  }
  if (EnumHasAnyFlags(d.usage, ETextureUsageBits::FragmentDensityMap)) {
    ASSERT(has_EXT_fragment_density_map_,
      TEXT("VK_EXT_fragment_density_map is not supported"));
    usage_flags |= VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT;
  }
  if (EnumHasAnyFlags(d.usage, ETextureUsageBits::ShadingRateAttachment)) {
    ASSERT(has_KHR_fragment_shading_rate_,
      TEXT("VK_KHR_fragment_shading_rate is not enabled (see "
           "`ContextConfig::enableFragmentShadingRate`)"));
    usage_flags |= VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
  }

  if (d.storage != EStorageType::Memoryless) {
    // TODO: for now, always set `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` , so we can read it
    // back.
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  ASSERT(usage_flags != 0, "Invalid usage flags");

  const VkMemoryPropertyFlags mem_prop_flags = toVkMemoryPropertyFlags(d.storage);
  const bool has_debug_name = d.debugName && *d.debugName;

  char debug_name_image[MAX_DEBUG_STRING_LENGTH] = {0};
  char debug_name_imageview[MAX_DEBUG_STRING_LENGTH] = {0};
  if (has_debug_name) {
    (void)snprintf(
      debug_name_image, sizeof(debug_name_image) - 1, "Image: %s", d.debugName);
    (void)snprintf(debug_name_imageview, sizeof(debug_name_imageview) - 1,
      "ImageView: %s", d.debugName);
  }

  VkImageCreateFlags create_flags = 0;
  VkImageType image_type;
  VkImageViewType image_view_type;
  VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
  uint32 num_layers = d.numLayers;
  switch (d.type) {
    case ETextureType::Texture1D:
      image_view_type =
        num_layers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
      image_type = VK_IMAGE_TYPE_1D;
      sample_count = getVkSampleCountFlags(d.numSamples, getFramebufferMSAABitMask());
      break;
    case ETextureType::Texture2D:
      image_view_type =
        num_layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
      image_type = VK_IMAGE_TYPE_2D;
      sample_count = getVkSampleCountFlags(d.numSamples, getFramebufferMSAABitMask());
      break;
    case ETextureType::Texture3D:
      image_view_type = VK_IMAGE_VIEW_TYPE_3D;
      image_type = VK_IMAGE_TYPE_3D;
      break;
    case ETextureType::TextureCube:
      image_view_type =
        num_layers > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
      image_type = VK_IMAGE_TYPE_2D;
      create_flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
      num_layers *= 6;
      break;
  }

  const VkExtent3D extent{d.dimensions.width, d.dimensions.height, d.dimensions.depth};
  const uint32 num_levels = d.numMipLevels;

  if (!validateImageLimits(image_type, sample_count, extent,
        getVkPhysicalDeviceProperties().limits, out_result)) {
    return {};
  }

  ASSERT(num_levels > 0, "The image must contain at least one mip-level");
  ASSERT(num_layers > 0, "The image must contain at least one layer");
  ASSERT(sample_count > 0, "The image must contain at least one sample");
  ASSERT(extent.width > 0);
  ASSERT(extent.height > 0);
  ASSERT(extent.depth > 0);

  VulkanImage image;
  image.usage_flags_ = usage_flags;
  image.extent_ = extent;
  image.type_ = image_type;
  image.sample_count_ = sample_count;
  image.num_layers_ = num_layers;
  image.num_levels_ = num_levels;
  image.is_depth_format_ = VulkanImage::isDepthFormat(format);
  image.is_stencil_format_ = VulkanImage::isStencilFormat(format);
  image.format_ = format;
  if (debug_name != nullptr && *debug_name) {
    (void)snprintf(image.debug_name_, sizeof(image.debug_name_) - 1, "%s", debug_name);
  }

  const uint32 num_planes = getNumImagePlanes(d.format);
  ASSERT(num_planes == 1, "Plane of vulkan image should be 1, multi-plane ignored.");

  const VkImageCreateInfo image_ci{
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .pNext = nullptr,
    .flags = create_flags,
    .imageType = image_type,
    .format = format,
    .extent = extent,
    .mipLevels = num_levels,
    .arrayLayers = num_layers,
    .samples = sample_count,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = usage_flags,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  if (VULKAN_USE_VMA && num_planes == 1) {
    // `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE` requires the host-access flag to land in
    // host-visible memory so `vmaMapMemory()` below works
    const VmaAllocationCreateInfo vma_alloc_ci{
      .flags = mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                 ? VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                 : VmaAllocationCreateFlags{0},
      .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    };

    VkResult result = vmaCreateImage((VmaAllocator)getVmaAllocator(), &image_ci,
      &vma_alloc_ci, &image.vk_handle_, &image.vma_allocation_, nullptr);

    if (result != VK_SUCCESS) {
      ALOGW("Failed: error result: %d, memflags: %d, image foramt: %d\n", result,
        mem_prop_flags, image.format_);
      Result::setResult(
        out_result, Result::Code::RuntimeError, "vmaCreateImage() failed");

      return {};
    }

    if (mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory(
        (VmaAllocator)getVmaAllocator(), image.vma_allocation_, &image.mapped_ptr_);
    }
  } else {
    // create the description of image, it does not allocate device memory for image,
    // but the driver will use small host memory to store the meta-data of image.
    VK_CHECK(vkCreateImage(device_, &image_ci, nullptr, &image.vk_handle_));

    VkMemoryRequirements2 mem_requirements2 = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2};

    const VkImage vk_image = image.vk_handle_;
    const VkImageMemoryRequirementsInfo2 mem_requirements_info2{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
      .pNext = nullptr,
      .image = vk_image,
    };

    [[maybe_unused]] const VkDeviceSize max_memory_allocation_size =
      physical_device_props11_.maxMemoryAllocationSize;

    // query the image size/alignment/compressed-meta-data/multiple-sample/mip-padding/
    // hardward layout, etc.
    // then allocate memory.
    vkGetImageMemoryRequirements2(device_, &mem_requirements_info2, &mem_requirements2);
    ASSERT(mem_requirements2.memoryRequirements.size <= max_memory_allocation_size);
    VK_CHECK(allocateMemory2(physical_device_, device_, &mem_requirements2,
      mem_prop_flags, &image.device_memory_));

    VkBindImageMemoryInfo bind_image_memory_info{
      .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
      .pNext = nullptr,
      .image = image.vk_handle_,
      .memory = image.device_memory_,
      .memoryOffset = 0,
    };

    // bind the image and storage
    VK_CHECK(vkBindImageMemory2(device_, 1, &bind_image_memory_info));

    if (mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VK_CHECK(vkMapMemory(
        device_, image.device_memory_, 0, VK_WHOLE_SIZE, 0, &image.mapped_ptr_));
    }
  }

  VK_CHECK(setDebugObjectName(
    device_, VK_OBJECT_TYPE_IMAGE, (uint64)image.vk_handle_, debug_name_image));

  {
    VkFormatProperties2 props = {
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    };

    vkGetPhysicalDeviceFormatProperties2(physical_device_, image.format_, &props);
    image.format_properties_ = props.formatProperties;
  }

  VkImageAspectFlags aspect = 0;
  if (image.is_depth_format_ || image.is_stencil_format_) {
    if (image.is_depth_format_) {
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (image.is_stencil_format_) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    aspect |= VK_IMAGE_ASPECT_COLOR_BIT;
  }

  const VkComponentMapping components = {
    .r = static_cast<VkComponentSwizzle>(d.components.r),
    .g = static_cast<VkComponentSwizzle>(d.components.g),
    .b = static_cast<VkComponentSwizzle>(d.components.b),
    .a = static_cast<VkComponentSwizzle>(d.components.a),
  };

  image.image_view_ = image.createImageView(device_, image_view_type, format, aspect, 0,
    VK_REMAINING_MIP_LEVELS, 0, num_layers, components, nullptr, debug_name_imageview);

  if (image.usage_flags_ & VK_IMAGE_USAGE_STORAGE_BIT) {
    if (!d.components.identity()) {
      image.image_view_storage_ =
        image.createImageView(device_, image_view_type, format, aspect, 0,
          VK_REMAINING_MIP_LEVELS, 0, num_layers, {}, nullptr, debug_name_imageview);
      ASSERT(image.image_view_storage_ != VK_NULL_HANDLE);
    }
  }

  if (image.image_view_ == VK_NULL_HANDLE) {
    Result::setResult(
      out_result, Result::Code::RuntimeError, "Cannot create VkImageView");
    return {};
  }

  TextureHandle handle = texture_pool_.create(std::move(image));

  awaiting_descriptor_update_ = true;

  if (d.data) {
    ASSERT(d.type == ETextureType::Texture2D || d.type == ETextureType::Texture3D);
    ASSERT(d.dataNumMipLevels <= d.numMipLevels);

    const uint32 upload_num_layers = d.type == ETextureType::TextureCube ? 6u : 1u;
    Result ret = upload(handle,
      {
        .dimensions = d.dimensions,
        .numLayers = upload_num_layers,
        .numMipLevels = d.dataNumMipLevels,
      },
      d.data, 0);
    if (!ret.isOk()) {
      Result::setResult(out_result, ret);
      return {};
    }

    if (d.generateMipmaps) {
      generateMipmap(handle);
    }
  }

  Result::setResult(out_result, Result());
  return {this, handle};
}

TextureDescriptorIndex Context::getTextureDescriptorIndex(TextureHandle handle) const {
  const VulkanImage* image = texture_pool_.get(handle);
  ASSERT(image != nullptr, TEXT("Texture descriptor requires a live texture handle."));
  ASSERT(!image || !image->pending_destroy_,
    TEXT("Texture descriptor requires an owned texture handle."));
  return image ? TextureDescriptorIndex{handle.index()} : TextureDescriptorIndex{};
}

SamplerDescriptorIndex Context::getSamplerDescriptorIndex(SamplerHandle handle) const {
  const VkSampler* sampler = sampler_pool_.get(handle);
  ASSERT(sampler != nullptr, TEXT("Sampler descriptor requires a live sampler handle."));
  return sampler ? SamplerDescriptorIndex{handle.index()} : SamplerDescriptorIndex{};
}

Holder<TextureHandle> Context::createTextureView(TextureHandle texture,
  const TextureViewDesc& desc, const char* debug_name, Result* out_result) {
  if (!texture) {
    ASSERT(texture.valid());
    return {};
  }

  VulkanImage* parent = texture_pool_.get(texture);
  ASSERT(!parent->pending_destroy_,
    "Cannot create a view after parent texture destruction was requested");
  if (parent->pending_destroy_) {
    Result::setResult(out_result, Result::Code::ArgumentOutOfRange,
      "Parent texture is pending destruction");
    return {};
  }
  ASSERT(parent->parent_.empty(),
    "Creating a texture view from another view is not supported");
  if (!parent->parent_.empty()) {
    Result::setResult(out_result, Result::Code::ArgumentOutOfRange,
      "Cannot create a view from a texture view");
    return {};
  }

  VulkanImage image = *parent;
  image.is_owning_VkImage_ = false;
  image.parent_ = texture;
  image.live_view_count_ = 0;
  image.pending_destroy_ = false;

  // memset(&image.image_view_storage_, 0, sizeof(image.image_view_storage_));
  image.image_view_storage_ = VK_NULL_HANDLE;
  memset(
    &image.image_view_for_framebuffer_, 0, sizeof(image.image_view_for_framebuffer_));
  memset(&image.image_view_for_framebuffer_multiview_, 0,
    sizeof(image.image_view_for_framebuffer_multiview_));

  VkImageAspectFlags aspect = 0;
  if (image.is_depth_format_ || image.is_stencil_format_) {
    if (image.is_depth_format_) {
      aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if (image.is_stencil_format_) {
      aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  } else {
    aspect |= VK_IMAGE_ASPECT_COLOR_BIT;
  }

  VkImageViewType image_view_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
  switch (desc.type) {
    case ETextureType::Texture1D:
      image_view_type =
        desc.numLayers > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
      break;
    case ETextureType::Texture2D:
      image_view_type =
        desc.numLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
      break;
    case ETextureType::Texture3D:
      image_view_type = VK_IMAGE_VIEW_TYPE_3D;
      break;
    case ETextureType::TextureCube:
      image_view_type =
        desc.numLayers > 1 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
      break;
    default:
      ASSERT(false, "Code should NOT be reached");
      Result::setResult(
        out_result, Result::Code::RuntimeError, "Unsupported texture view type");
      return {};
  }

  const VkComponentMapping components = {
    .r = static_cast<VkComponentSwizzle>(desc.components.r),
    .g = static_cast<VkComponentSwizzle>(desc.components.g),
    .b = static_cast<VkComponentSwizzle>(desc.components.b),
    .a = static_cast<VkComponentSwizzle>(desc.components.a),
  };

  ASSERT(getNumImagePlanes(image.format_) == 1, "Unsupported multiplanar image");

  image.image_view_ =
    image.createImageView(device_, image_view_type, image.format_, aspect, desc.mipLevel,
      desc.numMipLevels, desc.layer, desc.numLayers, components, nullptr, debug_name);

  if (image.image_view_ == VK_NULL_HANDLE) {
    Result::setResult(
      out_result, Result::Code::RuntimeError, "Cannot create VkImageView");
    return {};
  }

  if (image.usage_flags_ & VK_IMAGE_USAGE_STORAGE_BIT) {
    if (!desc.components.identity()) {
      image.image_view_storage_ = image.createImageView(device_, image_view_type,
        image.format_, aspect, desc.mipLevel, desc.numMipLevels, desc.layer,
        desc.numLayers, {}, nullptr, debug_name);
      ASSERT(image.image_view_storage_ != VK_NULL_HANDLE, "Cannot create VkImageView");
    }
  }

  ++parent->live_view_count_;
  TextureHandle handle = texture_pool_.create(std::move(image));

  awaiting_descriptor_update_ = true;
  return {this, handle};
}

Holder<ComputePipelineHandle> Context::createComputePipeline(
  const ComputePipelineDesc& desc, Result* out_result) {
  if (desc.smCompute.valid()) {
    Result::setResult(
      out_result, Result::Code::ArgumentOutOfRange, "Missing compute shader");
    return {};
  }

  ComputePipelineState cps{desc};

  if (desc.specConstantDesc.data && desc.specConstantDesc.dataSize) {
    // copy the data into a local storage.
    cps.specConstantDataStorage = malloc(desc.specConstantDesc.dataSize);
    memcpy(cps.specConstantDataStorage, desc.specConstantDesc.data,
      desc.specConstantDesc.dataSize);
    cps.desc.specConstantDesc.data = cps.specConstantDataStorage;
  }

  return {this, compute_pipeline_pool_.create(std::move(cps))};
}

Holder<RenderPipelineHandle> Context::createRenderPipeline(
  const RenderPipelineDesc& desc, Result* out_result) {
  const bool has_color_attachments = desc.getNumColorAttachments() != 0;
  const bool has_depth_attachment = desc.depthFormat != EFormat::NONE;
  const bool has_any_attachments = has_color_attachments || has_depth_attachment;

  if (!has_any_attachments) {
    Result::setResult(
      out_result, Result::Code::ArgumentOutOfRange, "Need at least one attachment");
    return {};
  }

  if (desc.smMesh.valid()) {
    if (desc.vertexInput.getNumAttributes() || desc.vertexInput.getNumInputBindings()) {
      Result::setResult(out_result, Result::Code::ArgumentOutOfRange,
        "Cannot have vertexInput with mesh shaders");
      return {};
    }
    if (desc.smVert.valid()) {
      Result::setResult(out_result, Result::Code::ArgumentOutOfRange,
        "Cannot have both vertex and mesh shaders");
      return {};
    }
  } else {
    if (!desc.smVert.valid()) {
      Result::setResult(
        out_result, Result::Code::ArgumentOutOfRange, "Missing vertex shaders");
      return {};
    }
  }

  if (!desc.smFrag.valid()) {
    Result::setResult(
      out_result, Result::Code::ArgumentOutOfRange, "Missing fragment shaders");
    return {};
  }

  RenderPipelineState rps = {.desc = desc};

  const VertexInput& vertex_input = rps.desc.vertexInput;
  bool buffer_bounded[VertexInput::MAX_VERTEX_BUFFER]{};

  rps.numAttributes = vertex_input.getNumAttributes();
  for (uint32 i = 0; i < rps.numAttributes; ++i) {
    const VertexInput::VertexAttribute& attr = vertex_input.attributes[i];

    rps.attributes[i] = {
      .location = attr.location,
      .binding = attr.binding,
      .format = toVkFormat(attr.format),
      .offset = (uint32)attr.offset,
    };

    if (!buffer_bounded[attr.binding]) {
      buffer_bounded[attr.binding] = true;
      rps.bindings[rps.numBindings++] = {
        .binding = attr.binding,
        .stride = vertex_input.inputBindings[attr.binding].stride,
        .inputRate =
          toVkVertexInputRate(vertex_input.inputBindings[attr.binding].inputRate),
      };
    }
  }

  if (desc.specConstantDesc.data && desc.specConstantDesc.dataSize) {
    // copy specialization constant info local storage
    rps.specConstantDataStorage = malloc(desc.specConstantDesc.dataSize);
    std::memcpy(rps.specConstantDataStorage, desc.specConstantDesc.data,
      desc.specConstantDesc.dataSize);
    rps.desc.specConstantDesc.data = rps.specConstantDataStorage;
  }

  return {this, render_pipeline_pool_.create(std::move(rps))};
}

Holder<RayTracingPipelineHandle> Context::createRayTracingPipeline(
  const RayTracingPipelineDesc& desc, Result* out_result) {
  if (!has_KHR_ray_tracing_pipeline_) {
    Result::setResult(out_result,
      Result(Result::Code::RuntimeError, "VK_KHR_ray_tracing_pipeline is not enabled"));
    return {};
  }

  RayTracingPipelineState rtps{
    .smRayGen = {desc.smRayGen.cbegin(), desc.smRayGen.cend()},
    .smMiss = {desc.smMiss.cbegin(), desc.smMiss.cend()},
    .smCallable = {desc.smCallable.cbegin(), desc.smCallable.cend()},
    .hitGroups = {desc.hitGroups.cbegin(), desc.hitGroups.cend()},
    .specConstantDesc = desc.specConstantDesc,
    .debugName = desc.debugName,
  };

  if (desc.specConstantDesc.data && desc.specConstantDesc.dataSize) {
    // copy specialization constant info local storage
    rtps.specConstantDataStorage = malloc(desc.specConstantDesc.dataSize);
    std::memcpy(rtps.specConstantDataStorage, desc.specConstantDesc.data,
      desc.specConstantDesc.dataSize);
    rtps.specConstantDesc.data = rtps.specConstantDataStorage;
  }

  return {this, ray_tracing_pipeline_pool_.create(std::move(rtps))};
}

Holder<ShaderModuleHandle> Context::createShaderModule(
  const ShaderModuleDesc& desc, Result* out_result) {
  Result result;
  ShaderModuleState sms{};
  if (desc.dataSize) {
    sms = createShaderModuleFromSPIRV(desc.data, desc.dataSize, desc.debugName, &result);
  } else {
    Result::setResult(&result, Result(Result::Code::ArgumentOutOfRange,
                                 "Vulkan shader modules require precompiled SPIR-V."));
  }

  Result::setResult(out_result, result);

  if (!result.isOk()) {
    return {};
  }

  return {this, shader_module_pool_.create(std::move(sms))};
}

Holder<QueryPoolHandle> Context::createQueryPool(
  uint32 num_queries, const char* debug_name, Result* out_result) {
  const VkQueryPoolCreateInfo ci{
    .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
    .flags = 0,
    .queryType = VK_QUERY_TYPE_TIMESTAMP,
    .queryCount = num_queries,
    .pipelineStatistics = 0,
  };

  VkQueryPool query_pool = VK_NULL_HANDLE;
  VK_CHECK(vkCreateQueryPool(device_, &ci, nullptr, &query_pool));

  if (!query_pool) {
    Result::setResult(
      out_result, Result(Result::Code::RuntimeError, "Cannot create query pool"));
    return {};
  }

  if (debug_name && *debug_name) {
    VK_CHECK(setDebugObjectName(
      device_, VK_OBJECT_TYPE_QUERY_POOL, (uint64)query_pool, debug_name));
  }

  QueryPoolHandle query_pool_handle = query_pool_.create(std::move(query_pool));
  return {this, query_pool_handle};
}

Holder<AccelerationStructHandle> Context::createAccelerationStructure(
  const AccelStructDesc& desc, Result* outResult) {
  if (!has_KHR_acceleration_structure_) {
    Result::setResult(outResult,
      Result(Result::Code::RuntimeError, "VK_KHR_acceleration_structure is not enabled"));
    return {};
  }

  Result result;

  AccelerationStructHandle handle;

  switch (desc.type) {
    case EAccelStructType::BLAS:
      handle = createBLAS(desc, &result);
      break;
    case EAccelStructType::TLAS:
      handle = createTLAS(desc, &result);
      break;
    default:
      ASSERT(false, "Invalid acceleration structure type");
      Result::setResult(outResult,
        Result(Result::Code::ArgumentOutOfRange, "Invalid acceleration structure type"));
      return {};
  }

  if (!(result.isOk() && handle.valid())) {
    Result::setResult(outResult,
      Result(Result::Code::RuntimeError, "Cannot create AccelerationStructure"));
    return {};
  }

  Result::setResult(outResult, result);

  awaiting_descriptor_update_ = true;

  return {this, handle};
}

void Context::destroy(ComputePipelineHandle handle) {
  if (handle.empty()) {
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }
  deferredTask(std::packaged_task([this, handle] {
    ComputePipelineState* pipeline = compute_pipeline_pool_.get(handle);
    ASSERT(pipeline != nullptr);
    if (pipeline) {
      free(pipeline->specConstantDataStorage);
      vkDestroyPipeline(device_, pipeline->pipeline, nullptr);
      vkDestroyPipelineLayout(device_, pipeline->pipelineLayout, nullptr);
      compute_pipeline_pool_.destroy(handle);
    }
    resource_tracker_.release(handle);
  }),
    retirement.point);
}

void Context::destroy(RenderPipelineHandle handle) {
  if (handle.empty()) {
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }
  deferredTask(std::packaged_task([this, handle] {
    RenderPipelineState* pipeline = render_pipeline_pool_.get(handle);
    ASSERT(pipeline != nullptr);
    if (pipeline) {
      free(pipeline->specConstantDataStorage);
      vkDestroyPipeline(device_, pipeline->pipeline, nullptr);
      vkDestroyPipelineLayout(device_, pipeline->pipelineLayout, nullptr);
      render_pipeline_pool_.destroy(handle);
    }
    resource_tracker_.release(handle);
  }),
    retirement.point);
}

void Context::destroy(RayTracingPipelineHandle handle) {
  if (handle.empty()) {
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }
  deferredTask(std::packaged_task([this, handle] {
    RayTracingPipelineState* pipeline = ray_tracing_pipeline_pool_.get(handle);
    ASSERT(pipeline != nullptr);
    if (pipeline) {
      free(pipeline->specConstantDataStorage);
      vkDestroyPipeline(device_, pipeline->pipeline, nullptr);
      vkDestroyPipelineLayout(device_, pipeline->pipelineLayout, nullptr);
      ray_tracing_pipeline_pool_.destroy(handle);
    }
    resource_tracker_.release(handle);
  }),
    retirement.point);
}

void Context::destroy(ShaderModuleHandle handle) {
  if (handle.empty()) {
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }
  deferredTask(std::packaged_task([this, handle] {
    const ShaderModuleState* shader = shader_module_pool_.get(handle);
    ASSERT(shader != nullptr);
    if (shader) {
      vkDestroyShaderModule(device_, shader->module, nullptr);
      free(const_cast<uint32*>(shader->createInfo.pCode));
      shader_module_pool_.destroy(handle);
    }
    resource_tracker_.release(handle);
  }),
    retirement.point);
}

void Context::destroy(SamplerHandle handle) {
  if (handle.empty()) {
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }
  deferredTask(std::packaged_task([this, handle] {
    const VkSampler* sampler = sampler_pool_.get(handle);
    ASSERT(sampler != nullptr);
    if (sampler) {
      vkDestroySampler(device_, *sampler, nullptr);
      sampler_pool_.destroy(handle);
      awaiting_descriptor_update_ = true;
    }
    resource_tracker_.release(handle);
  }),
    retirement.point);
}

void Context::destroy(BufferHandle handle) {
  if (handle.empty()) {
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }
  deferredTask(std::packaged_task([this, handle] {
    VulkanBuffer* buffer = buffer_pool_.get(handle);
    ASSERT(buffer != nullptr);
    if (buffer) {
      if (VULKAN_USE_VMA) {
        if (buffer->mapped_ptr_) {
          vmaUnmapMemory(
            static_cast<VmaAllocator>(getVmaAllocator()), buffer->vma_allocation_);
        }
        vmaDestroyBuffer(static_cast<VmaAllocator>(getVmaAllocator()), buffer->vk_handle_,
          buffer->vma_allocation_);
      } else {
        if (buffer->mapped_ptr_) {
          vkUnmapMemory(device_, buffer->device_memory_);
        }
        vkDestroyBuffer(device_, buffer->vk_handle_, nullptr);
        vkFreeMemory(device_, buffer->device_memory_, nullptr);
      }
      buffer_pool_.destroy(handle);
    }
    resource_tracker_.release(handle);
  }),
    retirement.point);
}

void Context::destroy(TextureHandle handle) {
  if (handle.empty()) {
    return;
  }

  VulkanImage* image = texture_pool_.get(handle);
  ASSERT(image != nullptr);
  if (!image) {
    return;
  }

  if (image->live_view_count_ > 0) {
    image->pending_destroy_ = true;
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }

  TextureHandle pending_parent;
  if (!image->parent_.empty()) {
    VulkanImage* parent = texture_pool_.get(image->parent_);
    ASSERT(parent != nullptr && parent->live_view_count_ > 0);
    if (parent && parent->live_view_count_ > 0) {
      --parent->live_view_count_;
      if (parent->live_view_count_ == 0 && parent->pending_destroy_) {
        pending_parent = image->parent_;
      }
    }
  }

  retireTexture(handle, retirement.point);
  if (!pending_parent.empty()) {
    const RetirementRequest parent_retirement = resource_tracker_.retire(pending_parent);
    if (parent_retirement.accepted) {
      retireTexture(pending_parent, parent_retirement.point);
    }
  }
}

void Context::retireTexture(TextureHandle handle, RetirementPoint retirement) {
  deferredTask(std::packaged_task([this, handle] {
    VulkanImage* retired_image = texture_pool_.get(handle);
    ASSERT(retired_image != nullptr);
    if (retired_image) {
      vkDestroyImageView(device_, retired_image->image_view_, nullptr);
      if (retired_image->image_view_storage_) {
        vkDestroyImageView(device_, retired_image->image_view_storage_, nullptr);
      }

      for (uint32 mip = 0; mip < MAX_MIP_LEVELS; ++mip) {
        for (VkImageView view : retired_image->image_view_for_framebuffer_[mip]) {
          if (view) {
            vkDestroyImageView(device_, view, nullptr);
          }
        }
        const VkImageView multiview =
          retired_image->image_view_for_framebuffer_multiview_[mip];
        if (multiview) {
          vkDestroyImageView(device_, multiview, nullptr);
        }
      }

      if (retired_image->is_owning_VkImage_) {
        if (retired_image->vma_allocation_) {
          if (retired_image->mapped_ptr_) {
            vmaUnmapMemory(static_cast<VmaAllocator>(getVmaAllocator()),
              retired_image->vma_allocation_);
          }
          vmaDestroyImage(static_cast<VmaAllocator>(getVmaAllocator()),
            retired_image->vk_handle_, retired_image->vma_allocation_);
        } else {
          if (retired_image->mapped_ptr_) {
            vkUnmapMemory(device_, retired_image->device_memory_);
          }
          vkDestroyImage(device_, retired_image->vk_handle_, nullptr);
          if (retired_image->device_memory_) {
            vkFreeMemory(device_, retired_image->device_memory_, nullptr);
          }
        }
      }

      texture_pool_.destroy(handle);
      awaiting_descriptor_update_ = true;
    }
    resource_tracker_.release(handle);
  }),
    retirement);
}

void Context::destroy(QueryPoolHandle handle) {
  if (handle.empty()) {
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }
  deferredTask(std::packaged_task([this, handle] {
    const VkQueryPool* pool = query_pool_.get(handle);
    ASSERT(pool != nullptr);
    if (pool) {
      vkDestroyQueryPool(device_, *pool, nullptr);
      query_pool_.destroy(handle);
    }
    resource_tracker_.release(handle);
  }),
    retirement.point);
}

void Context::destroy(AccelerationStructHandle handle) {
  if (handle.empty()) {
    return;
  }

  const RetirementRequest retirement = resource_tracker_.retire(handle);
  if (!retirement.accepted) {
    return;
  }
  deferredTask(std::packaged_task([this, handle] {
    const AccelerationStructure* acceleration_structure =
      acceleration_structure_pool_.get(handle);
    ASSERT(acceleration_structure != nullptr);
    if (acceleration_structure) {
      vkDestroyAccelerationStructureKHR(
        device_, acceleration_structure->vkHandle, nullptr);
      acceleration_structure_pool_.destroy(handle);
    }
    resource_tracker_.release(handle);
  }),
    retirement.point);
}

void Context::destroy(Framebuffer& fb) {
  auto destroy_framebuffer_texture = [this](TextureHandle& handle) {
    if (handle.empty()) {
      return;
    }

    VulkanImage* image = texture_pool_.get(handle);
    if (!image || !image->is_owning_VkImage_) {
      return;
    }

    destroy(handle);
    handle = {};
  };

  for (Framebuffer::AttachmentDesc& attachment_desc : fb.color) {
    destroy_framebuffer_texture(attachment_desc.texture);
    destroy_framebuffer_texture(attachment_desc.resolveTexture);
  }

  destroy_framebuffer_texture(fb.depthStencil.texture);
  destroy_framebuffer_texture(fb.depthStencil.resolveTexture);
}

uint64 Context::gpuAddress(AccelerationStructHandle handle) const {
  const AccelerationStructure* as = acceleration_structure_pool_.get(handle);

  ASSERT(as && as->deviceAddress, "AccelerationStructure or deviceAddress is nullptr");
  if (as && as->deviceAddress) {
    return as->deviceAddress;
  }

  return 0u;
}

AccelStructSizes Context::getAccelerationStructSizes(
  const AccelStructDesc& desc, Result* out_result) const {
  if (!has_KHR_acceleration_structure_) {
    Result::setResult(out_result,
      Result(Result::Code::RuntimeError, "VK_KHR_acceleration_structure is not enabled"));
    return {};
  }

  VkAccelerationStructureGeometryKHR accel_struct_geometry{};
  VkAccelerationStructureBuildSizesInfoKHR accel_struct_build_sizes_info{};

  switch (desc.type) {
    case EAccelStructType::BLAS:
      getBuildInfoBLAS(desc, accel_struct_geometry, accel_struct_build_sizes_info);
      break;
    case EAccelStructType::TLAS:
      getBuildInfoTLAS(desc, accel_struct_geometry, accel_struct_build_sizes_info);
      break;
    default:
      ASSERT(false, TEXT("Invalid acceleration structure type"));
      Result::setResult(out_result,
        Result(Result::Code::ArgumentOutOfRange, "Invalid acceleration structure type"));
      return {};
  }

  Result::setResult(out_result, Result::Code::Ok);

  return AccelStructSizes{
    .accelerationStructureSize = accel_struct_build_sizes_info.accelerationStructureSize,
    .updateScratchSize = accel_struct_build_sizes_info.updateScratchSize,
    .buildScratchSize = accel_struct_build_sizes_info.buildScratchSize,
  };
}

Result Context::upload(
  BufferHandle handle, const void* data, size_t size, size_t offset) {
  if (!data) {
    return Result(Result::Code::ArgumentOutOfRange);
  }
  ASSERT(size, TEXT("Data size should be non-zero."));

  VulkanBuffer* buffer = buffer_pool_.get(handle);
  if (!buffer) {
    return Result(Result::Code::ArgumentOutOfRange);
  }
  if (!(offset + size <= buffer->size_)) {
    return Result(Result::Code::ArgumentOutOfRange, "Out of range");
  }

  const SubmitHandle before = immediate_->getLastSubmitHandle();
  staging_device_->bufferSubData(*buffer, offset, size, data);
  const SubmitHandle after = immediate_->getLastSubmitHandle();
  if (after.handle() != before.handle()) {
    resource_tracker_.record(handle, after, false);
  }

  return Result{};
}

Result Context::download(BufferHandle handle, void* data, size_t size, size_t offset) {
  if (!data) {
    return Result(Result::Code::ArgumentOutOfRange);
  }
  ASSERT(size, TEXT("Data size should be non-zero."));

  VulkanBuffer* buffer = buffer_pool_.get(handle);
  if (!buffer) {
    return Result(Result::Code::ArgumentOutOfRange);
  }
  if (offset + size > buffer->size_) {
    return Result(Result::Code::ArgumentOutOfRange, "Out of range");
  }

  buffer->getBufferSubData(*this, offset, size, data);

  return Result{};
}

uint8_t* Context::getMappedPtr(BufferHandle handle) const {
  const VulkanBuffer* buffer = buffer_pool_.get(handle);
  ASSERT(buffer, TEXT("Buffer cannot be nullptr."));

  return buffer->isMapped() ? buffer->getMappedPtr() : nullptr;
}

uint64 Context::gpuAddress(BufferHandle handle, size_t offset) const {
  ASSERT((offset & 0x0007) == 0, TEXT("Buffer offset must be 8 bytes aligned as per "
                                      "GLSL_EXT_buffer_reference spec."));

  const VulkanBuffer* buffer = buffer_pool_.get(handle);
  ASSERT(buffer && buffer->device_address_,
    TEXT("Buffer cannot be nullptr or device address should be valid."));

  return buffer ? (uint64)buffer->device_address_ + offset : 0u;
}

void Context::flushMappedMemory(BufferHandle handle, size_t offset, size_t size) const {
  const VulkanBuffer* buffer = buffer_pool_.get(handle);
  ASSERT(buffer, TEXT("Buffer cannot be nullptr."));
  buffer->flushMappedMemory(*this, offset, size);
}

uint32 Context::getMaxStorageBufferRange() const {
  return physical_device_properties2_.properties.limits.maxStorageBufferRange;
}

Result Context::upload(TextureHandle handle, const TextureRangeDesc& range,
  const void* data, uint32 buffer_row_length) {
  if (!data) {
    return Result(Result::Code::ArgumentOutOfRange);
  }

  VulkanImage* image = texture_pool_.get(handle);
  if (!image) {
    return Result(Result::Code::ArgumentOutOfRange);
  }
  ASSERT(!image->pending_destroy_, "Cannot upload to a texture pending destruction");

  const Result result = _validate_range(image->extent_, image->num_levels_, range);
  if (!result.isOk()) {
    return Result(Result::Code::ArgumentOutOfRange);
  }

  VkFormat format = image->format_;
  const SubmitHandle before = immediate_->getLastSubmitHandle();
  if (image->type_ == VK_IMAGE_TYPE_3D) {
    staging_device_->imageData3D(*image,                           //
      VkOffset3D{range.offset.x, range.offset.y, range.offset.z},  //
      VkExtent3D{
        range.dimensions.width, range.dimensions.height, range.dimensions.depth},  //
      format,                                                                      //
      data);
  } else {
    const VkRect2D image_region{
      .offset = {.x = range.offset.x, .y = range.offset.y},
      .extent = {.width = range.dimensions.width, .height = range.dimensions.height},
    };
    staging_device_->imageData2D(*image, image_region, range.mipLevel, range.numMipLevels,
      range.layer, range.numLayers, format, data, buffer_row_length);
  }

  const SubmitHandle after = immediate_->getLastSubmitHandle();
  if (after.handle() != before.handle()) {
    TextureHandle tracked = handle;
    while (!tracked.empty()) {
      resource_tracker_.record(tracked, after, false);
      const VulkanImage* tracked_image = texture_pool_.get(tracked);
      tracked = tracked_image ? tracked_image->parent_ : TextureHandle{};
    }
  }

  return Result();
}

Result Context::download(
  TextureHandle handle, const TextureRangeDesc& range, void* out_data) {
  if (!out_data) {
    return Result(Result::Code::ArgumentOutOfRange);
  }

  VulkanImage* image = texture_pool_.get(handle);
  if (!image) {
    return Result(Result::Code::RuntimeError);
  }

  // VUID-VkBufferImageCopy2-aspectMask-09103: a buffer-image copy region can reference
  // only one image aspect, so combined depth/stencil textures would need one copy
  // region per aspect
  if (image->isDepthFormat() && image->isStencilFormat()) {
    ASSERT(false, TEXT("Cannot download combined depth-stencil texture"));
    return Result(
      Result::Code::ArgumentOutOfRange, "Cannot download combined depth-stencil texture");
  }

  const Result result = _validate_range(image->extent_, image->num_levels_, range);
  if (!result.isOk()) {
    return result;
  }

  staging_device_->getImageData(*image,
    VkOffset3D(range.offset.x, range.offset.y, range.offset.z),
    VkExtent3D(range.dimensions.width, range.dimensions.height, range.dimensions.depth),
    VkImageSubresourceRange{
      .aspectMask = image->getImageAspectFlags(),
      .baseMipLevel = range.mipLevel,
      .levelCount = range.numMipLevels,
      .baseArrayLayer = range.layer,
      .layerCount = range.numLayers,
    },
    image->format_, out_data);

  return Result();
}

Dimensions Context::getDimensions(TextureHandle handle) const {
  if (!handle) return {};

  const VulkanImage* image = texture_pool_.get(handle);
  ASSERT(image);

  if (!image) return {};

  return {
    .width = image->extent_.width,
    .height = image->extent_.height,
    .depth = image->extent_.depth,
  };
}

float Context::getAspectRatio(TextureHandle handle) const {
  if (!handle) return {};

  const VulkanImage* image = texture_pool_.get(handle);
  ASSERT(image);

  if (!image) return {};

  return static_cast<float>(image->extent_.width) /
         static_cast<float>(image->extent_.height);
}

EFormat Context::getFormat(TextureHandle handle) const {
  if (handle.empty()) return EFormat::NONE;

  return toEFormat(texture_pool_.get(handle)->format_);
}

TextureHandle Context::getCurrentSwapchainTexture() {
  if (!hasSwapchain()) return {};

  TextureHandle image = swapchain_->getCurrentTexture();

  // Out of date swapchain, the caller has to recreate it.
  if (!image.valid()) {
    return {};
  }

  ASSERT(
    texture_pool_.get(image)->format_ != VK_FORMAT_UNDEFINED, "Invalid image format");

  return image;
}

EFormat Context::getSwapchainFormat() const {
  return hasSwapchain() ? toEFormat(swapchain_->getSurfaceFormat().format)
                        : EFormat::NONE;
}

EColorSpace Context::getSwapchainColorSpace() const {
  if (!hasSwapchain()) {
    return EColorSpace::SRGB_NONLINEAR;
  }

  return toEColorSpace(swapchain_->getSurfaceFormat().colorSpace);
}

uint32 Context::getSwapchainCurrentImageIndex() const {
  if (hasSwapchain()) {
    (void)swapchain_->getCurrentTexture();
  }

  return hasSwapchain() ? swapchain_->getSwapchainCurrentImageIndex() : 0u;
}

uint32 Context::getNumSwapchainImages() const {
  return hasSwapchain() ? swapchain_->getNumSwapchainImages() : 0u;
}

void Context::recreateSwapchain(int newWidth, int newHeight) {
  initSwapchain(newWidth, newHeight);
}

bool Context::setCurrentPresentMode(EPresentMode mode) {
  if (!hasSwapchain() || !has_KHR_swapchain_maintenance1_) {
    return false;
  }

  return swapchain_->setCurrentPresentMode(toVkPresentMode(mode));
}

EPresentMode Context::getCurrentPresentMode() const {
  return swapchain_ ? toPresentMode(swapchain_->current_present_mode_)
                    : EPresentMode::FIFO;
}

uint32 Context::getFramebufferMSAABitMask() const {
  const VkPhysicalDeviceLimits limits = physical_device_properties2_.properties.limits;
  return limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
}

Dimensions Context::getShadingRateAttachmentMinTexelSize() const {
  ASSERT(has_KHR_fragment_shading_rate_,
    "VK_KHR_fragment_shading_rate is not enabled (see "
    "`ContextConfig::enableFragmentShadingRate`)");
  const VkExtent2D& size =
    fragment_shading_rate_properties_.minFragmentShadingRateAttachmentTexelSize;
  return {.width = size.width, .height = size.height};
}

Dimensions Context::getShadingRateAttachmentMaxTexelSize() const {
  ASSERT(has_KHR_fragment_shading_rate_,
    "VK_KHR_fragment_shading_rate is not enabled (see "
    "`ContextConfig::enableFragmentShadingRate`)");
  const VkExtent2D& size =
    fragment_shading_rate_properties_.maxFragmentShadingRateAttachmentTexelSize;
  return {.width = size.width, .height = size.height};
}

bool Context::isExtensionEnabled(const char* ext) const {
  for (const char* name : enabled_instance_extension_names_) {
    if (strcmp(ext, name) == 0) return true;
  }

  for (const char* name : enabled_device_extension_names_) {
    if (strcmp(ext, name) == 0) return true;
  }

  return false;
}

bool Context::supportsAsyncCompute() const { return immediate_compute_ != nullptr; }

double Context::getTimestampPeriodToMs() const {
  return double(getVkPhysicalDeviceProperties().limits.timestampPeriod) * 1e-6;
}

bool Context::getQueryPoolResults(QueryPoolHandle pool, uint32 firstQuery,
  uint32 queryCount, size_t dataSize, void* outData, size_t stride) const {
  VkQueryPool query_pool = *query_pool_.get(pool);

  VK_CHECK(vkGetQueryPoolResults(device_, query_pool, firstQuery, queryCount, dataSize,
    outData, stride, VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT));

  return true;
}

VkPipeline Context::getVkPipeline(ComputePipelineHandle handle) {
  ComputePipelineState* cps = compute_pipeline_pool_.get(handle);
  if (!cps) {
    return VK_NULL_HANDLE;
  }

  checkAndUpdateDescriptorSets();

  const DescriptorSet& descriptor_set = dst_sets_[last_updated_dst_set_];
  if (cps->lastVkDescriptorSetLayout != descriptor_set.dstSetLayout) {
    deferredTask(std::packaged_task([device = device_, pipeline = cps->pipeline]() {
      vkDestroyPipeline(device, pipeline, nullptr);
    }));
    deferredTask(std::packaged_task([device = device_, layout = cps->pipelineLayout]() {
      vkDestroyPipelineLayout(device, layout, nullptr);
    }));

    cps->pipeline = VK_NULL_HANDLE;
    cps->pipelineLayout = VK_NULL_HANDLE;
    cps->lastVkDescriptorSetLayout = VK_NULL_HANDLE;
  }

  if (cps->pipeline == VK_NULL_HANDLE) {
    ShaderModuleState* sms = shader_module_pool_.get(cps->desc.smCompute);
    ASSERT(sms != nullptr);

    VkSpecializationMapEntry
      entries[SpecializationConstantDesc::SPECIALIZATION_CONSTANTS_MAX] = {};
    const VkSpecializationInfo spec_info =
      getPipelineShaderStageSpecializationInfo(cps->desc.specConstantDesc, entries);

    {
      const VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = (uint32)Align(sms->pushConstantsSize, 16u),
      };

      const VkPipelineLayoutCreateInfo pipeline_layout_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_set.dstSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range,
      };

      VK_CHECK(vkCreatePipelineLayout(
        device_, &pipeline_layout_ci, nullptr, &cps->pipelineLayout));

      char pipeline_layout_name[MAX_DEBUG_STRING_LENGTH] = {0};
      if (cps->desc.debugName) {
        (void)snprintf(pipeline_layout_name, sizeof(pipeline_layout_name) - 1,
          "PipelineLayout: %s", cps->desc.debugName);
      }
      VK_CHECK(setDebugObjectName(device_, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
        (uint64)cps->pipelineLayout, pipeline_layout_name));
    }

    VkPipelineShaderStageCreateInfo pipeline_shader_stage_ci{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    };

    const VkComputePipelineCreateInfo compute_pipeline_ci{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .flags = 0,
      .stage = _get_pipeline_shader_stage_create_info(
        VK_SHADER_STAGE_COMPUTE_BIT, sms->module, cps->desc.entryPoint, &spec_info),
      .layout = cps->pipelineLayout,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,
    };

    VK_CHECK(vkCreateComputePipelines(
      device_, pipeline_cache_, 1, &compute_pipeline_ci, nullptr, &cps->pipeline));
    VK_CHECK(setDebugObjectName(
      device_, VK_OBJECT_TYPE_PIPELINE, (uint64)cps->pipeline, cps->desc.debugName));
  }

  return cps->pipeline;
}

VkPipeline Context::getVkPipeline(RenderPipelineHandle handle, uint32 view_mask) {
  RenderPipelineState* rps = render_pipeline_pool_.get(handle);
  if (!rps) {
    return VK_NULL_HANDLE;
  }

  // TODO: ycbcr

  checkAndUpdateDescriptorSets();

  const DescriptorSet& descriptor_set = dst_sets_[last_updated_dst_set_];

  if (rps->lastVkDescriptorSetLayout != descriptor_set.dstSetLayout ||
      rps->viewMask != view_mask) {
    deferredTask(std::packaged_task([device = getVkDevice(), pipeline = rps->pipeline] {
      vkDestroyPipeline(device, pipeline, nullptr);
    }));
    deferredTask(
      std::packaged_task([device = getVkDevice(), layout = rps->pipelineLayout] {
        vkDestroyPipelineLayout(device, layout, nullptr);
      }));

    rps->pipeline = VK_NULL_HANDLE;
    rps->lastVkDescriptorSetLayout = descriptor_set.dstSetLayout;
    rps->viewMask = view_mask;
  }

  if (rps->pipeline != VK_NULL_HANDLE) {
    return rps->pipeline;
  }

  // build a new vulkan pipeline
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  VkPipeline pipeline = VK_NULL_HANDLE;

  const RenderPipelineDesc& rpdesc = rps->desc;
  const uint32 num_color_attachments = rps->desc.getNumColorAttachments();

  // Not all attachments are valid. We need to create color blend attachments only for
  // active attachments.
  VkPipelineColorBlendAttachmentState
    color_blend_attachment_states[MAX_COLOR_ATTACHMENTS] = {};
  VkFormat color_attachment_formats[MAX_COLOR_ATTACHMENTS] = {};

  for (uint32_t i = 0; i != num_color_attachments; i++) {
    const ColorAttachment& attachment = rpdesc.color[i];
    ASSERT(attachment.format != EFormat::NONE);
    color_attachment_formats[i] = toVkFormat(attachment.format);
    if (!attachment.blendEnabled) {
      color_blend_attachment_states[i] = VkPipelineColorBlendAttachmentState{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };
    } else {
      color_blend_attachment_states[i] = VkPipelineColorBlendAttachmentState{
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = toVkBlendFactor(attachment.srcRGBBlendFactor),
        .dstColorBlendFactor = toVkBlendFactor(attachment.dstRGBBlendFactor),
        .colorBlendOp = toVkBlendOp(attachment.rgbBlendOp),
        .srcAlphaBlendFactor = toVkBlendFactor(attachment.srcAlphaBlendFactor),
        .dstAlphaBlendFactor = toVkBlendFactor(attachment.dstAlphaBlendFactor),
        .alphaBlendOp = toVkBlendOp(attachment.alphaBlendOp),
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      };
    }
  }
  const ShaderModuleState* vertModule = shader_module_pool_.get(rpdesc.smVert);
  // const ShaderModuleState* tescModule = shader_module_pool_.get(rpdesc.smTesc);
  // const ShaderModuleState* teseModule = shader_module_pool_.get(rpdesc.smTese);
  // const ShaderModuleState* geomModule = shader_module_pool_.get(rpdesc.smGeom);
  const ShaderModuleState* fragModule = shader_module_pool_.get(rpdesc.smFrag);
  const ShaderModuleState* taskModule = shader_module_pool_.get(rpdesc.smTask);
  const ShaderModuleState* meshModule = shader_module_pool_.get(rpdesc.smMesh);

  // if (tescModule || teseModule || rpdesc.patchControlPoints) {
  //   ASSERT(tescModule && teseModule, "Both tessellation control and evaluation
  //   shaders should be provided"); ASSERT(
  //     rpdesc.patchControlPoints > 0 &&
  //     rpdesc.patchControlPoints <=
  //     physical_device_properties2_.properties.limits.maxTessellationPatchSize);
  // }

  const VkPipelineVertexInputStateCreateInfo vertex_input_state_ci{
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = rps->numBindings,
    .pVertexBindingDescriptions = rps->numBindings ? rps->bindings : nullptr,
    .vertexAttributeDescriptionCount = rps->numAttributes,
    .pVertexAttributeDescriptions = rps->numAttributes ? rps->attributes : nullptr,
  };

  VkSpecializationMapEntry
    spec_map_entries[SpecializationConstantDesc::SPECIALIZATION_CONSTANTS_MAX];
  const VkSpecializationInfo spec_info =
    getPipelineShaderStageSpecializationInfo(rpdesc.specConstantDesc, spec_map_entries);

  // Create render pieline layout
  {
#define UPDATE_PUSH_CONSTANT_SIZE(SM, BIT)                                  \
  if (SM) {                                                                 \
    pushConstantsSize = std::max(pushConstantsSize, SM->pushConstantsSize); \
    rps->shaderStageFlags |= BIT;                                           \
  }
    rps->shaderStageFlags = 0;
    uint32 pushConstantsSize = 0u;

    UPDATE_PUSH_CONSTANT_SIZE(vertModule, VK_SHADER_STAGE_VERTEX_BIT);
    // UPDATE_PUSH_CONSTANT_SIZE(tescModule, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    // UPDATE_PUSH_CONSTANT_SIZE(teseModule,
    // VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    // UPDATE_PUSH_CONSTANT_SIZE(geomModule, VK_SHADER_STAGE_GEOMETRY_BIT);
    UPDATE_PUSH_CONSTANT_SIZE(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT);
    UPDATE_PUSH_CONSTANT_SIZE(taskModule, VK_SHADER_STAGE_TASK_BIT_EXT);
    UPDATE_PUSH_CONSTANT_SIZE(meshModule, VK_SHADER_STAGE_MESH_BIT_EXT);

#undef UPDATE_PUSH_CONSTANT_SIZE

    // maxPushConstantsSize is guaranteed to be at least 128 bytes
    // https://www.khronos.org/registry/vulkan/specs/1.3/html/vkspec.html#features-limits
    // Table 32. Required Limits
    const VkPhysicalDeviceLimits limits = getVkPhysicalDeviceProperties().limits;
    ASSERT(pushConstantsSize <= limits.maxPushConstantsSize,
      TEXT("Push constants size exceeded %u (max %u bytes)"), pushConstantsSize,
      limits.maxPushConstantsSize);

    const VkDescriptorSetLayout descriptor_set_layouts[] = {
      descriptor_set.dstSetLayout, input_attachments_dst_set_layout_};
    const VkPushConstantRange range = {
      .stageFlags = rps->shaderStageFlags,
      .offset = 0,
      .size = Align<uint32>(pushConstantsSize, 16),
    };
    const VkPipelineLayoutCreateInfo pipeline_layout_ci{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = (uint32)ARRAY_COUNT(descriptor_set_layouts),
      .pSetLayouts = descriptor_set_layouts,
      .pushConstantRangeCount = pushConstantsSize ? 1u : 0u,
      .pPushConstantRanges = pushConstantsSize ? &range : nullptr,
    };

    VK_CHECK(
      vkCreatePipelineLayout(device_, &pipeline_layout_ci, nullptr, &pipeline_layout));

    char pipeline_layout_name[MAX_DEBUG_STRING_LENGTH] = {0};
    if (rps->desc.debugName) {
      (void)snprintf(pipeline_layout_name, sizeof(pipeline_layout_name) - 1,
        "PipelineLayout: %s", rps->desc.debugName);
    }

    VK_CHECK(setDebugObjectName(device_, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
      (uint64)pipeline_layout, pipeline_layout_name));
  }

  VulkanPipelineBuilder()
    // from Vulkan 1.0
    .dynamicState(VK_DYNAMIC_STATE_VIEWPORT)
    .dynamicState(VK_DYNAMIC_STATE_SCISSOR)
    .dynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS)
    .dynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS)
    // from Vulkan 1.3 or VK_EXT_extended_dynamic_state
    .dynamicState(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE)
    .dynamicState(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE)
    .dynamicState(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP)
    // from Vulkan 1.3 or VK_EXT_extended_dynamic_state2
    .dynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE)
    // from VK_KHR_fragment_shading_rate
    .dynamicState(
      VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR, has_KHR_fragment_shading_rate_)
    .createFlags(VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR,
      has_KHR_fragment_shading_rate_)
    // from VK_EXT_fragment_density_map
    .createFlags(VK_PIPELINE_CREATE_RENDERING_FRAGMENT_DENSITY_MAP_ATTACHMENT_BIT_EXT,
      has_EXT_fragment_density_map_)
    .primitiveTopology(toVkPrimitiveTopology(rpdesc.topology))
    .rasterizationSamples(
      getVkSampleCountFlags(rpdesc.samplesCount, getFramebufferMSAABitMask()),
      rpdesc.minSampleShading)
    .alphaToCoverage(rpdesc.alphaToCoverage)
    .polygonMode(toVkPolygonMode(rpdesc.polygonMode))
    .stencilStateOps(VK_STENCIL_FACE_FRONT_BIT,
      toVkStencilOp(rpdesc.frontFaceStencil.stencilFailureOp),
      toVkStencilOp(rpdesc.frontFaceStencil.depthStencilPassOp),
      toVkStencilOp(rpdesc.frontFaceStencil.depthFailureOp),
      toVkCompareOp(rpdesc.frontFaceStencil.stencilCompareOp))
    .stencilStateOps(VK_STENCIL_FACE_BACK_BIT,
      toVkStencilOp(rpdesc.backFaceStencil.stencilFailureOp),
      toVkStencilOp(rpdesc.backFaceStencil.depthStencilPassOp),
      toVkStencilOp(rpdesc.backFaceStencil.depthFailureOp),
      toVkCompareOp(rpdesc.backFaceStencil.stencilCompareOp))
    .stencilMasks(VK_STENCIL_FACE_FRONT_BIT, 0xFF, rpdesc.frontFaceStencil.writeMask,
      rpdesc.frontFaceStencil.readMask)
    .stencilMasks(VK_STENCIL_FACE_BACK_BIT, 0xFF, rpdesc.backFaceStencil.writeMask,
      rpdesc.backFaceStencil.readMask)
    .shaderStage(taskModule
                   ? _get_pipeline_shader_stage_create_info(VK_SHADER_STAGE_TASK_BIT_EXT,
                       taskModule->module, rpdesc.entryPointTask, &spec_info)
                   : VkPipelineShaderStageCreateInfo{.module = VK_NULL_HANDLE})
    .shaderStage(meshModule
                   ? _get_pipeline_shader_stage_create_info(VK_SHADER_STAGE_MESH_BIT_EXT,
                       meshModule->module, rpdesc.entryPointMesh, &spec_info)
                   : _get_pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                       vertModule->module, rpdesc.entryPointVert, &spec_info))
    .shaderStage(_get_pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
      fragModule->module, rpdesc.entryPointFrag, &spec_info))
    // .shaderStage(
    //   tescModule ? _get_pipeline_shader_stage_create_info(
    //                  VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    //                  tescModule->createInfo, rpdesc.entryPointTesc, &spec_info)
    //              : VkPipelineShaderStageCreateInfo{.module = VK_NULL_HANDLE})
    // .shaderStage(
    //   teseModule ? _get_pipeline_shader_stage_create_info(
    //                  VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    //                  teseModule->createInfo, rpdesc.entryPointTese, &spec_info)
    //              : VkPipelineShaderStageCreateInfo{.module = VK_NULL_HANDLE})
    // .shaderStage(
    //   geomModule
    //     ? _get_pipeline_shader_stage_create_info(VK_SHADER_STAGE_GEOMETRY_BIT,
    //     geomModule->createInfo, rpdesc.entryPointGeom, &spec_info) :
    //     VkPipelineShaderStageCreateInfo{.module = VK_NULL_HANDLE})
    .cullMode(toVkCullMode(rpdesc.cullMode))
    .frontFace(toVkFrontFace(rpdesc.frontFace))
    .vertexInputState(vertex_input_state_ci)
    .viewMask(view_mask)
    .colorAttachments(
      color_blend_attachment_states, color_attachment_formats, num_color_attachments)
    .depthAttachmentFormat(toVkFormat(rpdesc.depthFormat))
    .stencilAttachmentFormat(toVkFormat(rpdesc.stencilFormat))
    // .patchControlPoints(rpdesc.patchControlPoints)
    .build(device_, pipeline_cache_, pipeline_layout, &pipeline, rpdesc.debugName);

  rps->pipeline = pipeline;
  rps->pipelineLayout = pipeline_layout;

  return pipeline;
}

VkPipeline Context::getVkPipeline(RayTracingPipelineHandle handle) {
  RayTracingPipelineState* rtps = ray_tracing_pipeline_pool_.get(handle);
  if (!rtps) {
    return VK_NULL_HANDLE;
  }

  checkAndUpdateDescriptorSets();

  const DescriptorSet& descriptor_set = dst_sets_[last_updated_dst_set_];
  if (rtps->lastVkDescriptorSetLayout != descriptor_set.dstSetLayout) {
    deferredTask(std::packaged_task([device = device_, pipeline = rtps->pipeline]() {
      vkDestroyPipeline(device, pipeline, nullptr);
    }));

    deferredTask(std::packaged_task([device = device_, layout = rtps->pipelineLayout]() {
      vkDestroyPipelineLayout(device, layout, nullptr);
    }));

    rtps->pipeline = VK_NULL_HANDLE;
    rtps->pipelineLayout = VK_NULL_HANDLE;
    rtps->lastVkDescriptorSetLayout = descriptor_set.dstSetLayout;
  }

  if (rtps->pipeline) {
    return rtps->pipeline;
  }

  ASSERT(!rtps->smRayGen.empty());

  {
#define UPDATE_PUSH_CONSTANT_SIZE(sm, bit)                                     \
  if (const ShaderModuleState* sms = shader_module_pool_.get(sm);              \
    sms && sms->pushConstantsSize) {                                           \
    push_constant_size = std::max(push_constant_size, sms->pushConstantsSize); \
    rtps->shaderStageFlags |= bit;                                             \
  }

    rtps->shaderStageFlags = 0;
    uint32 push_constant_size = 0u;

    for (const ShaderModuleHandle sm : rtps->smRayGen)
      UPDATE_PUSH_CONSTANT_SIZE(sm, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
    for (const ShaderModuleHandle sm : rtps->smMiss)
      UPDATE_PUSH_CONSTANT_SIZE(sm, VK_SHADER_STAGE_MISS_BIT_KHR)
    for (const ShaderModuleHandle sm : rtps->smCallable)
      UPDATE_PUSH_CONSTANT_SIZE(sm, VK_SHADER_STAGE_CALLABLE_BIT_KHR)
    for (const RayTracingHitGroupDesc hit_group : rtps->hitGroups) {
      UPDATE_PUSH_CONSTANT_SIZE(hit_group.smAnyHit, VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
      UPDATE_PUSH_CONSTANT_SIZE(hit_group.smCloseHit, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
      UPDATE_PUSH_CONSTANT_SIZE(
        hit_group.smIntersection, VK_SHADER_STAGE_INTERSECTION_BIT_KHR)
    }
#undef UPDATE_PUSH_CONSTANT_SIZE

    const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;
    ASSERT(push_constant_size < limits.maxPushConstantsSize,
      TEXT("Push constants size exceeded %u (max %u bytes)"), push_constant_size,
      limits.maxPushConstantsSize);

    const VkPushConstantRange push_constant_range{
      .stageFlags = rtps->shaderStageFlags,
      .size = (uint32)Align(push_constant_size, 16u),
    };
    const VkPipelineLayoutCreateInfo pipeline_layout_ci{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &descriptor_set.dstSetLayout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &push_constant_range,
    };
    VK_CHECK(vkCreatePipelineLayout(
      device_, &pipeline_layout_ci, nullptr, &rtps->pipelineLayout));

    char pipeline_layout_name[MAX_DEBUG_STRING_LENGTH] = {0};
    if (rtps->debugName) {
      (void)snprintf(pipeline_layout_name, sizeof(pipeline_layout_name) - 1,
        "PipelineLayout: %s", rtps->debugName);
    }
    VK_CHECK(setDebugObjectName(device_, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
      (uint64)rtps->pipelineLayout, pipeline_layout_name));
  }

  VkSpecializationMapEntry
    entries[SpecializationConstantDesc::SPECIALIZATION_CONSTANTS_MAX] = {};
  const VkSpecializationInfo spec_info =
    getPipelineShaderStageSpecializationInfo(rtps->specConstantDesc, entries);

  const size_t max_ray_tracing_shader_stages =
    rtps->smRayGen.size() + rtps->smMiss.size() + rtps->smCallable.size() +
    3 * rtps->hitGroups.size();
  std::vector<VkPipelineShaderStageCreateInfo> shader_stages_ci(
    max_ray_tracing_shader_stages);

  uint32 num_shader_stages = 0u;

  auto add_stage = [&](VkShaderStageFlagBits flag, ShaderModuleHandle sm) -> uint32 {
    const ShaderModuleState* state = shader_module_pool_.get(sm);
    if (!state || !state->createInfo.pCode) {
      return VK_SHADER_UNUSED_KHR;
    }
    shader_stages_ci[num_shader_stages] =
      _get_pipeline_shader_stage_create_info(flag, state->module, "main", &spec_info);
    return num_shader_stages++;
  };

  const uint32 max_shader_groups =
    (uint32)(rtps->smRayGen.size() + rtps->smMiss.size() + rtps->smCallable.size() +
             rtps->hitGroups.size());
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups_ci(max_shader_groups);

  uint32 num_shader_groups = 0u;
  // used to record offset
  uint32 idx_miss = 0u;
  uint32 num_miss_groups = 0u;
  uint32 idx_hit = 0u;
  uint32 num_hit_groups = 0u;
  uint32 idx_callable = 0u;
  uint32 num_callable_groups = 0u;

  // ray generation groups
  for (const ShaderModuleHandle sm : rtps->smRayGen) {
    if (shader_module_pool_.get(sm)) {
      shader_groups_ci[num_shader_groups++] = VkRayTracingShaderGroupCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = add_stage(VK_SHADER_STAGE_RAYGEN_BIT_KHR, sm),
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
      };
    }
  }

  // miss groups
  for (const ShaderModuleHandle sm : rtps->smMiss) {
    if (shader_module_pool_.get(sm)) {
      if (!num_miss_groups) idx_miss = num_shader_groups;

      num_miss_groups++;

      shader_groups_ci[num_shader_groups++] = VkRayTracingShaderGroupCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = add_stage(VK_SHADER_STAGE_MISS_BIT_KHR, sm),
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
      };
    }
  }

  // hit groups
  for (const RayTracingHitGroupDesc& hg : rtps->hitGroups) {
    if (hg.smAnyHit || hg.smCloseHit || hg.smIntersection) {
      if (!num_hit_groups) {
        idx_hit = num_shader_groups;
      }

      num_hit_groups++;

      shader_groups_ci[num_shader_groups++] = VkRayTracingShaderGroupCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = add_stage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, hg.smCloseHit),
        .anyHitShader = add_stage(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, hg.smAnyHit),
        .intersectionShader =
          add_stage(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, hg.smIntersection),
      };
    }
  }

  // callable groups
  for (const ShaderModuleHandle sm : rtps->smCallable) {
    if (shader_module_pool_.get(sm)) {
      if (!num_miss_groups) idx_callable = num_shader_groups;

      num_callable_groups++;

      shader_groups_ci[num_shader_groups++] = VkRayTracingShaderGroupCreateInfoKHR{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = add_stage(VK_SHADER_STAGE_CALLABLE_BIT_KHR, sm),
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
      };
    }
  }

  const VkRayTracingPipelineCreateInfoKHR ray_tracing_pipeline_ci{
    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
    .stageCount = num_shader_stages,
    .pStages = shader_stages_ci.data(),
    .groupCount = num_shader_groups,
    .pGroups = shader_groups_ci.data(),
    .maxPipelineRayRecursionDepth = ray_tracing_pipeline_properties_.maxRayRecursionDepth,
    .layout = rtps->pipelineLayout,
  };
  VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1,
    &ray_tracing_pipeline_ci, nullptr, &rtps->pipeline));

  // shader binding table
  const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& props =
    ray_tracing_pipeline_properties_;
  const uint32 handle_size = props.shaderGroupHandleSize;
  const uint32 handle_size_aligned =
    Align(props.shaderGroupHandleSize, props.shaderGroupHandleAlignment);
  const uint32 sbt_size = num_shader_groups * handle_size_aligned;

  ASSERT(sbt_size);

  std::vector<uint8> shader_handle_storage(sbt_size);
  VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtps->pipeline, 0,
    num_shader_groups, sbt_size, shader_handle_storage.data()));

  const VkDeviceSize sbtEntrySizeAligned =
    Align(handle_size_aligned, props.shaderGroupBaseAlignment);
  const VkDeviceSize sbtBufferSize = num_shader_groups * sbtEntrySizeAligned;

  // repack SBT respecting `shaderGroupBaseAlignment`
  std::vector<uint8_t> sbtStorage(sbtBufferSize);
  for (uint32_t i = 0; i != num_shader_groups; i++) {
    memcpy(sbtStorage.data() + i * sbtEntrySizeAligned,
      shader_handle_storage.data() + i * handle_size_aligned, handle_size);
  }

  rtps->sbt = createBuffer(
    {
      .usage = static_cast<uint8>(EBufferUsageBits::ShaderBindingTable),
      .storageType = EStorageType::Device,
      .size = sbtBufferSize + props.shaderGroupBaseAlignment,
      .data = nullptr,
      .debugName = "Buffer: SBT",
    },
    nullptr, nullptr);

  const uint64 base_addr =
    Align<uint64>(gpuAddress(rtps->sbt, 0), props.shaderGroupBaseAlignment);
  const uint64 offset = base_addr - gpuAddress(rtps->sbt, 0);
  upload(rtps->sbt, sbtStorage.data(), sbtBufferSize, offset);
  // generate SBT entries; stride must be sbtEntrySizeAligned (respects
  // shaderGroupBaseAlignment), and size must cover all records in each group so the
  // hardware can index into them correctly
  rtps->sbtEntryRayGen = {
    .deviceAddress = base_addr,
    .stride = sbtEntrySizeAligned,
    .size = sbtEntrySizeAligned,  // ray gen: size == stride (exactly one record used at
                                  // a time)
  };
  rtps->sbtEntryMiss = {
    .deviceAddress = num_miss_groups ? base_addr + idx_miss * sbtEntrySizeAligned : 0u,
    .stride = sbtEntrySizeAligned,
    .size = num_miss_groups * sbtEntrySizeAligned,
  };
  rtps->sbtEntryHit = {
    .deviceAddress = num_hit_groups ? base_addr + idx_hit * sbtEntrySizeAligned : 0u,
    .stride = sbtEntrySizeAligned,
    .size = num_hit_groups * sbtEntrySizeAligned,
  };
  rtps->sbtEntryCallable = {
    .deviceAddress =
      num_callable_groups ? base_addr + idx_callable * sbtEntrySizeAligned : 0u,
    .stride = sbtEntrySizeAligned,
    .size = num_callable_groups * sbtEntrySizeAligned,
  };

  return rtps->pipeline;
}

uint32 Context::queryDevices(HWDeviceDesc* out_devices, uint32 max_out_devices) {
  uint32 device_count = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(instance_, &device_count, nullptr));
  std::vector<VkPhysicalDevice> devices(device_count);
  VK_CHECK(vkEnumeratePhysicalDevices(instance_, &device_count, devices.data()));

  auto convert_vk_physical_device =
    [](const VkPhysicalDeviceType device_type) -> EHWDeviceType {
    switch (device_type) {
      case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return EHWDeviceType::Integrated;
      case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return EHWDeviceType::Discrete;
      case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return EHWDeviceType::External;
      case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return EHWDeviceType::Software;
      default:
        return EHWDeviceType::Software;
    }
  };

  uint32 compatible_device_count = 0;
  for (uint32 i = 0; i < device_count; ++i) {
    VkPhysicalDevice physical_device = devices[i];
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);

    const EHWDeviceType hw_device_type =
      convert_vk_physical_device(device_properties.deviceType);

    if (out_devices && compatible_device_count < max_out_devices) {
      out_devices[compatible_device_count] = {
        .guid = (uintptr_t)devices[i],
        .type = hw_device_type,
      };

      strncpy(out_devices[compatible_device_count].name, device_properties.deviceName,
        strlen(device_properties.deviceName));

      compatible_device_count++;
    }
  }

  return compatible_device_count;
}

Result Context::initContext(const HWDeviceDesc& desc) {
  if (desc.guid == 0ul) {
    ALOGE("Invalid hardware guid(%lu)", desc.guid);
    return Result(Result::Code::RuntimeError, "Vulkan is not supported");
  }

  physical_device_ = reinterpret_cast<VkPhysicalDevice>(desc.guid);
  //< whether the GPU is uma, which don't need use staging buffer to upload.
  require_staging_for_device_buffers_ =
    !_is_host_visible_single_heap_memory(physical_device_);

  std::vector<VkExtensionProperties> device_extensions;
  auto get_device_extension_props = [](VkPhysicalDevice dev,
                                      std::vector<VkExtensionProperties>& props,
                                      const char* validation_layer = nullptr) -> void {
    uint32 num_extensions = 0;
    vkEnumerateDeviceExtensionProperties(dev, validation_layer, &num_extensions, nullptr);
    std::vector<VkExtensionProperties> p(num_extensions);
    vkEnumerateDeviceExtensionProperties(
      dev, validation_layer, &num_extensions, p.data());
    props.insert(props.end(), p.begin(), p.end());
  };

  get_device_extension_props(physical_device_, device_extensions);
  if (config_.bEnableValidation) {
    for (const char* layer : DEFAULT_VALIDATION_LAYERS) {
      get_device_extension_props(physical_device_, device_extensions, layer);
    }
  }

  auto add_next_physical_device_props = [this](void* props) {
    if (!props) return;

    std::launder(reinterpret_cast<VkBaseOutStructure*>(props))->pNext = std::launder(
      reinterpret_cast<VkBaseOutStructure*>(this->physical_device_properties2_.pNext));
  };

  if (_has_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, device_extensions)) {
    add_next_physical_device_props(&acceleration_structure_properties_);
  }

  if (_has_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, device_extensions)) {
    add_next_physical_device_props(&ray_tracing_pipeline_properties_);
  }

  if (_has_extension(VK_EXT_MESH_SHADER_EXTENSION_NAME, device_extensions)) {
    add_next_physical_device_props(&mesh_shader_properties_);
    mesh_shader_features_.pNext = physical_device_features10_.pNext;
    physical_device_features10_.pNext = &mesh_shader_features_;
  }

  if (_has_extension(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME, device_extensions)) {
    add_next_physical_device_props(&fragment_density_map_properties_);
    fragment_density_map_features_.pNext = physical_device_features10_.pNext;
    physical_device_features10_.pNext = &fragment_density_map_features_;
  }

  if (config_.bEnableFragmentShadingRate &&
      _has_extension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME, device_extensions)) {
    add_next_physical_device_props(&fragment_shading_rate_properties_);
  }

  if (config_.vulkanVersion >= VkVersion_1_4) {
    add_next_physical_device_props(&physical_device_features14_);
    physical_device_features13_.pNext = &physical_device_features14_;
  } else if (_has_extension(VK_KHR_MAINTENANCE_6_EXTENSION_NAME, device_extensions)) {
    add_next_physical_device_props(&maintenance6_properties_);
  }

  vkGetPhysicalDeviceFeatures2(physical_device_, &physical_device_features10_);
  vkGetPhysicalDeviceProperties2(physical_device_, &physical_device_properties2_);

  if (config_.vulkanVersion >= VkVersion_1_4) {
    pimpl_->setMaxCombinedImageSamplerDescriptorCount(
      physical_device_props14_.maxCombinedImageSamplerDescriptorCount);
  } else if (_has_extension(VK_KHR_MAINTENANCE_6_EXTENSION_NAME, device_extensions)) {
    pimpl_->setMaxCombinedImageSamplerDescriptorCount(
      maintenance6_properties_.maxCombinedImageSamplerDescriptorCount);
  }

  const uint32 api_version = physical_device_properties2_.properties.apiVersion;

  workaround_fixed_size_accel_struct_array_ =
    strstr(physical_device_properties2_.properties.deviceName, "Adreno") != nullptr;
  ALOGI("Vulkan physical device: %s", physical_device_properties2_.properties.deviceName);
  ALOGI("\tAPI version: %i.%i.%i.%i", VK_API_VERSION_MAJOR(api_version),
    VK_API_VERSION_MINOR(api_version), VK_API_VERSION_PATCH(api_version),
    VK_API_VERSION_VARIANT(api_version));
  ALOGI("\tDriver info: %s %s", physical_device_driver_properties_.driverName,
    physical_device_driver_properties_.driverInfo);

  ALOGI("Vulkan physical extensions:");
  for (const VkExtensionProperties& p : device_extensions) {
    ALOGI("  %s", p.extensionName);
  }

  device_queues_.graphicsQueueFamilyIndex =
    _find_queue_family_index(physical_device_, VK_QUEUE_GRAPHICS_BIT);
  device_queues_.computeQueueFamilyIndex =
    _find_queue_family_index(physical_device_, VK_QUEUE_COMPUTE_BIT);
  // device_queues_.transferQueueFamilyIndex =
  //   _find_queue_family_index(physical_device_, VK_QUEUE_TRANSFER_BIT);

  if (device_queues_.graphicsQueueFamilyIndex == DeviceQueues::INVALID) {
    ALOGW("VK_QUEUE_GRAPHICS_BIT is not supported.");
    return Result(Result::Code::RuntimeError, "VK_QUEUE_GRAPHICS_BIT is not supported.");
  }
  if (device_queues_.graphicsQueueFamilyIndex == DeviceQueues::INVALID) {
    ALOGW("VK_COMPUTE_GRAPHICS_BIT is not supported.");
    return Result(
      Result::Code::RuntimeError, "VK_COMPUTE_GRAPHICS_BIT is not supported.");
  }

  float queue_priority = 1.0f;
  const VkDeviceQueueCreateInfo queue_cis[2] = {
    {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = device_queues_.graphicsQueueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority,
    },
    {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = device_queues_.computeQueueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &queue_priority,
    },
    // {
    //   .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    //   .queueFamilyIndex = device_queues_.transferQueueFamilyIndex,
    //   .queueCount = 1,
    //   .pQueuePriorities = &queue_priority,
    // },
  };

  const uint32 num_queues =
    queue_cis[0].queueFamilyIndex == queue_cis[1].queueFamilyIndex ? 1 : 2;
  enabled_device_extension_names_ = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  if (_has_extension("VK_KHR_portability_subset", device_extensions)) {
    enabled_device_extension_names_.push_back("VK_KHR_portability_subset");
  }
  for (const char* ext : config_.extensionsDevice) {
    if (ext) {
      enabled_device_extension_names_.push_back(ext);
    }
  }
  VkPhysicalDeviceFeatures device_features10 = {
    .fullDrawIndexUint32 = VK_TRUE,
    .imageCubeArray = VK_TRUE,
    .independentBlend = VK_TRUE,
    .geometryShader =
      physical_device_features10_.features.geometryShader,  // enable if supported
    .tessellationShader =
      physical_device_features10_.features.tessellationShader,  // enable if supported
    .sampleRateShading = VK_TRUE,
    .dualSrcBlend = VK_TRUE,
    .multiDrawIndirect = VK_TRUE,
    .drawIndirectFirstInstance = VK_TRUE,
    .depthClamp = VK_TRUE,
    .depthBiasClamp = VK_TRUE,
    .fillModeNonSolid =
      physical_device_features10_.features.fillModeNonSolid,  // enable if supported
    .largePoints = VK_TRUE,
    .samplerAnisotropy = VK_TRUE,
    .textureCompressionETC2 =
      physical_device_features10_.features.textureCompressionETC2,  // enable if supported
    .textureCompressionASTC_LDR = physical_device_features10_.features
      .textureCompressionASTC_LDR,  // enable if supported
    .textureCompressionBC =
      physical_device_features10_.features.textureCompressionBC,  // enable if supported
    .vertexPipelineStoresAndAtomics = physical_device_features10_.features
      .vertexPipelineStoresAndAtomics,  // enable if supported
    .fragmentStoresAndAtomics = VK_TRUE,
    .shaderImageGatherExtended = VK_TRUE,
    .shaderStorageImageExtendedFormats = VK_TRUE,
    .shaderStorageImageMultisample =
      physical_device_features10_.features.shaderStorageImageMultisample,
    .shaderStorageImageReadWithoutFormat =
      physical_device_features10_.features.shaderStorageImageReadWithoutFormat,
    .shaderStorageImageWriteWithoutFormat = VK_TRUE,
    .shaderUniformBufferArrayDynamicIndexing = VK_TRUE,
    .shaderSampledImageArrayDynamicIndexing = VK_TRUE,
    .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
    .shaderStorageImageArrayDynamicIndexing = VK_TRUE,
    .shaderInt64 =
      physical_device_features10_.features.shaderInt64,  // enable if supported
    .shaderInt16 = VK_TRUE,
  };

  VkPhysicalDeviceVulkan11Features device_features11 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    .pNext = config_.extensionsDeviceFeatures,
    .storageBuffer16BitAccess = VK_TRUE,
    .storagePushConstant16 =
      physical_device_features11_.storagePushConstant16,  // enable if supported
    .storageInputOutput16 =
      physical_device_features11_.storageInputOutput16,  // enable if supported
    .multiview = physical_device_features11_.multiview,  // enable if supported
    .variablePointersStorageBuffer = VK_TRUE,
    .variablePointers = VK_TRUE,
    .samplerYcbcrConversion = VK_TRUE,
    .shaderDrawParameters = VK_TRUE,
  };

  VkPhysicalDeviceVulkan12Features device_features12 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    .pNext = &device_features11,
    .samplerMirrorClampToEdge = VK_TRUE,
    .drawIndirectCount =
      physical_device_features12_.drawIndirectCount,  // enable if supported
    .storageBuffer8BitAccess = VK_TRUE,
    .uniformAndStorageBuffer8BitAccess = physical_device_features12_
      .uniformAndStorageBuffer8BitAccess,  // enable if supported
    .shaderBufferInt64Atomics =
      physical_device_features12_.shaderBufferInt64Atomics,  // enable if supported
    .shaderSharedInt64Atomics =
      physical_device_features12_.shaderSharedInt64Atomics,  // enable if supported
    .shaderFloat16 = VK_TRUE,
    .shaderInt8 = VK_TRUE,
    .descriptorIndexing = VK_TRUE,
    .shaderInputAttachmentArrayDynamicIndexing = VK_TRUE,
    .shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE,
    .shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .shaderStorageImageArrayNonUniformIndexing = VK_TRUE,
    .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
    .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
    .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
    .descriptorBindingPartiallyBound = VK_TRUE,
    .descriptorBindingVariableDescriptorCount = VK_TRUE,
    .runtimeDescriptorArray = VK_TRUE,
    .scalarBlockLayout = VK_TRUE,
    .uniformBufferStandardLayout = VK_TRUE,
    .hostQueryReset = physical_device_features12_.hostQueryReset,  // enable if supported
    .timelineSemaphore = VK_TRUE,
    .bufferDeviceAddress = VK_TRUE,
    .vulkanMemoryModel = VK_TRUE,
    .vulkanMemoryModelDeviceScope = VK_TRUE,
    .vulkanMemoryModelAvailabilityVisibilityChains = physical_device_features12_
      .vulkanMemoryModelAvailabilityVisibilityChains,  // enable if supported
  };
  VkPhysicalDeviceVulkan13Features device_features13 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    .pNext = &device_features12,
    .shaderDemoteToHelperInvocation =
      physical_device_features13_.shaderDemoteToHelperInvocation,  // enable if supported
    .shaderTerminateInvocation =
      physical_device_features13_.shaderTerminateInvocation,  // enable if supported
    .subgroupSizeControl = VK_TRUE,
    .synchronization2 = VK_TRUE,
    .textureCompressionASTC_HDR =
      physical_device_features13_.textureCompressionASTC_HDR,  // enable if supported
    .dynamicRendering = VK_TRUE,
    .maintenance4 = VK_TRUE,
  };

  VkPhysicalDeviceVulkan14Features device_features14 = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
    .pNext = &device_features13,
    .indexTypeUint8 = VK_TRUE,
    .dynamicRenderingLocalRead = VK_TRUE,
    .maintenance5 = VK_TRUE,
    .maintenance6 = VK_TRUE,
    .hostImageCopy = physical_device_features14_.hostImageCopy,  // enable if supported
    .pushDescriptor = VK_TRUE,
  };

  void* create_info_next = config_.vulkanVersion >= VkVersion_1_4
                             ? static_cast<void*>(&device_features14)
                             : static_cast<void*>(&device_features13);

  VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    .accelerationStructure = VK_TRUE,
    .accelerationStructureCaptureReplay = VK_FALSE,
    .accelerationStructureIndirectBuild = VK_FALSE,
    .accelerationStructureHostCommands = VK_FALSE,
    .descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE,
  };
  VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    .rayTracingPipeline = VK_TRUE,
    .rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE,
    .rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE,
    .rayTracingPipelineTraceRaysIndirect = VK_TRUE,
    .rayTraversalPrimitiveCulling = VK_FALSE,
  };
  VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    .rayQuery = VK_TRUE,
  };
  // TODO: need higher version.
  // VkPhysicalDeviceRayTracingInvocationReorderFeaturesEXT
  //   rayTracingInvocationReorderFeatures = {
  //     .sType =
  //       VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_INVOCATION_REORDER_FEATURES_EXT,
  //     .rayTracingInvocationReorder = VK_TRUE,
  // };
  VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR swapchain_maintenance1_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR,
    .swapchainMaintenance1 = VK_TRUE,
  };
  VkPhysicalDeviceIndexTypeUint8FeaturesEXT index_type_Uint8_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_INDEX_TYPE_UINT8_FEATURES_EXT,
    .indexTypeUint8 = VK_TRUE,
  };
  // VkPhysicalDeviceFaultFeaturesEXT device_fault_features = {
  //   .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT,
  //   .deviceFault = VK_TRUE,
  // };
  VkPhysicalDeviceMaintenance5FeaturesKHR maintenance5_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR,
    .maintenance5 = VK_TRUE,
  };
  VkPhysicalDeviceMaintenance6FeaturesKHR maintenance6_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR,
    .maintenance6 = VK_TRUE,
  };
  VkPhysicalDeviceDynamicRenderingLocalReadFeaturesKHR
    dynamic_rendering_local_read_features = {
      .sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_LOCAL_READ_FEATURES_KHR,
      .dynamicRenderingLocalRead = VK_TRUE,
  };
  VkPhysicalDeviceShaderTileImageFeaturesEXT shader_tile_image_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_TILE_IMAGE_FEATURES_EXT,
    .shaderTileImageColorReadAccess = VK_TRUE,
    .shaderTileImageDepthReadAccess = VK_TRUE,
  };
  VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
    .taskShader = VK_TRUE,
    .meshShader = VK_TRUE,
  };
  VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR
    present_mode_latest_ready_features = {
      .sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR,
      .presentModeFifoLatestReady = VK_TRUE,
  };
  VkPhysicalDeviceHostImageCopyFeaturesEXT host_image_copy_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_FEATURES_EXT,
    .hostImageCopy = VK_TRUE,
  };
  VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragment_density_map_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT,
    .fragmentDensityMap = VK_TRUE,
    .fragmentDensityMapDynamic = fragment_density_map_features_.fragmentDensityMapDynamic,
    .fragmentDensityMapNonSubsampledImages =
      fragment_density_map_features_.fragmentDensityMapNonSubsampledImages,
  };
  VkPhysicalDeviceFragmentDensityMap2FeaturesEXT fragment_density_map2_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_2_FEATURES_EXT,
    .fragmentDensityMapDeferred = VK_TRUE,
  };
  VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragment_shading_rate_features = {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
    .pipelineFragmentShadingRate = VK_TRUE,
    .primitiveFragmentShadingRate = VK_TRUE,
    .attachmentFragmentShadingRate = VK_TRUE,
  };

  auto addExtension = [&device_extensions, this, &create_info_next](
                        const char* name, void* features = nullptr) mutable -> void {
    if (!_has_extension(name, device_extensions)) {
      ALOGW("Unsupported mandatory extension `%s`\n", name);
      return;
    }
    enabled_device_extension_names_.push_back(name);
    if (features) {
      std::launder(reinterpret_cast<VkBaseOutStructure*>(features))->pNext =
        std::launder(reinterpret_cast<VkBaseOutStructure*>(create_info_next));
      create_info_next = features;
    }
  };

  auto add_optional_extension = [&device_extensions, this, &create_info_next](
                                  const char* name, bool& enabled,
                                  void* features = nullptr) mutable -> bool {
    if (!_has_extension(name, device_extensions)) return false;
    enabled = true;
    enabled_device_extension_names_.push_back(name);
    if (features) {
      std::launder(reinterpret_cast<VkBaseOutStructure*>(features))->pNext =
        std::launder(reinterpret_cast<VkBaseOutStructure*>(create_info_next));
      create_info_next = features;
    }
    return true;
  };
  auto add_optional_extensions = [&device_extensions, this, &create_info_next](
                                   const char* name1, const char* name2, bool& enabled,
                                   void* features = nullptr) mutable {
    if (!_has_extension(name1, device_extensions) ||
        !_has_extension(name2, device_extensions))
      return;
    enabled = true;
    enabled_device_extension_names_.push_back(name1);
    enabled_device_extension_names_.push_back(name2);
    if (features) {
      std::launder(reinterpret_cast<VkBaseOutStructure*>(features))->pNext =
        std::launder(reinterpret_cast<VkBaseOutStructure*>(create_info_next));
      create_info_next = features;
    }
  };

  if (config_.vulkanVersion < VkVersion_1_4) {
    addExtension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, nullptr);
    addExtension(VK_KHR_MAINTENANCE_5_EXTENSION_NAME, &maintenance5_features);
    addExtension(VK_KHR_DYNAMIC_RENDERING_LOCAL_READ_EXTENSION_NAME,
      &dynamic_rendering_local_read_features);
    add_optional_extension(
      VK_KHR_MAINTENANCE_6_EXTENSION_NAME, has_KHR_maintenance6_, &maintenance6_features);
    if (!add_optional_extension(VK_KHR_INDEX_TYPE_UINT8_EXTENSION_NAME, has_8Bit_indices_,
          &index_type_Uint8_features)) {
      add_optional_extension(VK_EXT_INDEX_TYPE_UINT8_EXTENSION_NAME, has_8Bit_indices_,
        &index_type_Uint8_features);
    }
    add_optional_extension(VK_EXT_HOST_IMAGE_COPY_EXTENSION_NAME,
      has_EXT_host_image_copy_, &host_image_copy_features);
  } else {
    // promoted to core in Vulkan 1.4
    has_KHR_maintenance6_ = physical_device_features14_.maintenance6 == VK_TRUE;
    // promoted to core in Vulkan 1.4
    has_EXT_host_image_copy_ = physical_device_features14_.hostImageCopy == VK_TRUE;
  }
#if defined(WITH_TRACY)
  add_optional_extension(
    VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME, has_KHR_calibrated_timestamps_, nullptr);
#endif
  add_optional_extensions(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, has_KHR_acceleration_structure_,
    &acceleration_structure_features);
  add_optional_extension(
    VK_KHR_RAY_QUERY_EXTENSION_NAME, has_KHR_ray_query_, &ray_query_features);
  add_optional_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    has_KHR_ray_tracing_pipeline_, &ray_tracing_features);
  // add_optional_extension(
  //   VK_EXT_RAY_TRACING_INVOCATION_REORDER_EXTENSION_NAME,
  //   has_EXT_ray_tracing_invocation_reorder,
  //   &rayTracingInvocationReorderFeatures);
  if (!add_optional_extension(VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        has_KHR_swapchain_maintenance1_, &swapchain_maintenance1_features)) {
    add_optional_extension(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
      has_KHR_swapchain_maintenance1_, &swapchain_maintenance1_features);
  }
  add_optional_extension(VK_EXT_HDR_METADATA_EXTENSION_NAME, has_EXT_hdr_metadata_);
  // if (!add_optional_extension(
  //       VK_KHR_DEVICE_FAULT_EXTENSION_NAME,
  //       has_EXT_device_fault_,
  //       &device_fault_features)) {
  //   add_optional_extension(
  //     VK_EXT_DEVICE_FAULT_EXTENSION_NAME,
  //     has_EXT_device_fault_,
  //     &device_fault_features);
  // }
  add_optional_extension(VK_EXT_SHADER_TILE_IMAGE_EXTENSION_NAME,
    has_EXT_shader_tile_image, &shader_tile_image_features);
  add_optional_extension(
    VK_EXT_MESH_SHADER_EXTENSION_NAME, has_EXT_mesh_shader_, &mesh_shader_features);

  // VUID-VkDeviceCreateInfo-fragmentDensityMap-04481/04482/04483: the FSR is mutually
  // exclusive with FDM, so enable only one of them
  if (!config_.bEnableFragmentShadingRate ||
      !add_optional_extension(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
        has_KHR_fragment_shading_rate_, &fragment_shading_rate_features)) {
    add_optional_extension(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME,
      has_EXT_fragment_density_map_, &fragment_density_map_features);
    add_optional_extension(VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME,
      has_EXT_fragment_density_map2_, &fragment_density_map2_features);
  }

  add_optional_extension(
    VK_KHR_SHARED_PRESENTABLE_IMAGE_EXTENSION_NAME, has_KHR_shared_presentable_image_);
  add_optional_extension(VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME,
    has_KHR_present_mode_fifo_latest_ready_, &present_mode_latest_ready_features);

  if (has_EXT_host_image_copy_) {
    // query VK_EXT_host_image_copy properties (copy dst layouts + memory-type
    // requirements)
    VkPhysicalDeviceHostImageCopyProperties props = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_IMAGE_COPY_PROPERTIES,
    };
    VkPhysicalDeviceProperties2 props2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &props,
    };
    vkGetPhysicalDeviceProperties2(physical_device_, &props2);
    std::vector<VkImageLayout> dstLayouts(props.copyDstLayoutCount);
    props.copySrcLayoutCount = 0;
    props.pCopyDstLayouts = dstLayouts.data();
    vkGetPhysicalDeviceProperties2(physical_device_, &props2);

    // imageData2D() copies into whichever layout the uploaded image can use
    host_image_copy_to_shader_readonly_ =
      std::find(dstLayouts.begin(), dstLayouts.end(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) != dstLayouts.end();
    host_image_copy_to_general_ = std::find(dstLayouts.begin(), dstLayouts.end(),
                                    VK_IMAGE_LAYOUT_GENERAL) != dstLayouts.end();
    host_image_copy_identical_memory_type_requirements_ =
      props.identicalMemoryTypeRequirements == VK_TRUE;

    // device-local memory type mask, used to check whether HOST_TRANSFER images stay
    // device-local
    VkPhysicalDeviceMemoryProperties2 memProps2 = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
    vkGetPhysicalDeviceMemoryProperties2(physical_device_, &memProps2);
    device_local_memory_type_mask_ = 0;
    for (uint32 i = 0; i < memProps2.memoryProperties.memoryTypeCount; ++i) {
      if (memProps2.memoryProperties.memoryTypes[i].propertyFlags &
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
        device_local_memory_type_mask_ |= (1u << i);
      }
    }

    ALOGI(
      "VK_EXT_host_image_copy: enabled (identicalMemoryTypeRequirements: %s, "
      "SHADER_READ_ONLY_OPTIMAL: %s)\n",
      host_image_copy_identical_memory_type_requirements_ ? "true" : "false",
      host_image_copy_to_shader_readonly_ ? "true" : "false");
  }

  if (has_KHR_fragment_shading_rate_) {
    uint32 num_rates = 0;
    VK_CHECK(
      vkGetPhysicalDeviceFragmentShadingRatesKHR(physical_device_, &num_rates, nullptr));
    device_fragment_shading_rates_.assign(
      num_rates, VkPhysicalDeviceFragmentShadingRateKHR{
                   .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_KHR});
    VK_CHECK(vkGetPhysicalDeviceFragmentShadingRatesKHR(
      physical_device_, &num_rates, device_fragment_shading_rates_.data()));
  }

  // check extensions
  {
    std::string missing_exts;
    for (const char* ext : enabled_device_extension_names_) {
      if (!_has_extension(ext, device_extensions))
        missing_exts += "\n   " + std::string(ext);
    }
    if (!missing_exts.empty()) {
      ALOGE("Missing Vulkan device extensions: %s\n", missing_exts.c_str());
      assert(false);
      return Result(Result::Code::RuntimeError);
    }
  }

  // check features
  {
    std::string missing_features;
#define CHECK_VULKAN_FEATURE(reqFeatures, availFeatures, feature, version)     \
  if ((reqFeatures.feature == VK_TRUE) && (availFeatures.feature == VK_FALSE)) \
    missing_features.append("\n   " version " ." #feature);
#define CHECK_FEATURE_1_0(feature) \
  CHECK_VULKAN_FEATURE(            \
    device_features10, physical_device_features10_.features, feature, "1.0 ");
    CHECK_FEATURE_1_0(robustBufferAccess);
    CHECK_FEATURE_1_0(fullDrawIndexUint32);
    CHECK_FEATURE_1_0(imageCubeArray);
    CHECK_FEATURE_1_0(independentBlend);
    CHECK_FEATURE_1_0(geometryShader);
    CHECK_FEATURE_1_0(tessellationShader);
    CHECK_FEATURE_1_0(sampleRateShading);
    CHECK_FEATURE_1_0(dualSrcBlend);
    CHECK_FEATURE_1_0(logicOp);
    CHECK_FEATURE_1_0(multiDrawIndirect);
    CHECK_FEATURE_1_0(drawIndirectFirstInstance);
    CHECK_FEATURE_1_0(depthClamp);
    CHECK_FEATURE_1_0(depthBiasClamp);
    CHECK_FEATURE_1_0(fillModeNonSolid);
    CHECK_FEATURE_1_0(depthBounds);
    CHECK_FEATURE_1_0(wideLines);
    CHECK_FEATURE_1_0(largePoints);
    CHECK_FEATURE_1_0(alphaToOne);
    CHECK_FEATURE_1_0(multiViewport);
    CHECK_FEATURE_1_0(samplerAnisotropy);
    CHECK_FEATURE_1_0(textureCompressionETC2);
    CHECK_FEATURE_1_0(textureCompressionASTC_LDR);
    CHECK_FEATURE_1_0(textureCompressionBC);
    CHECK_FEATURE_1_0(occlusionQueryPrecise);
    CHECK_FEATURE_1_0(pipelineStatisticsQuery);
    CHECK_FEATURE_1_0(vertexPipelineStoresAndAtomics);
    CHECK_FEATURE_1_0(fragmentStoresAndAtomics);
    CHECK_FEATURE_1_0(shaderTessellationAndGeometryPointSize);
    CHECK_FEATURE_1_0(shaderImageGatherExtended);
    CHECK_FEATURE_1_0(shaderStorageImageExtendedFormats);
    CHECK_FEATURE_1_0(shaderStorageImageMultisample);
    CHECK_FEATURE_1_0(shaderStorageImageReadWithoutFormat);
    CHECK_FEATURE_1_0(shaderStorageImageWriteWithoutFormat);
    CHECK_FEATURE_1_0(shaderUniformBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderSampledImageArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderStorageBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderStorageImageArrayDynamicIndexing);
    CHECK_FEATURE_1_0(shaderClipDistance);
    CHECK_FEATURE_1_0(shaderCullDistance);
    CHECK_FEATURE_1_0(shaderFloat64);
    CHECK_FEATURE_1_0(shaderInt64);
    CHECK_FEATURE_1_0(shaderInt16);
    CHECK_FEATURE_1_0(shaderResourceResidency);
    CHECK_FEATURE_1_0(shaderResourceMinLod);
    CHECK_FEATURE_1_0(sparseBinding);
    CHECK_FEATURE_1_0(sparseResidencyBuffer);
    CHECK_FEATURE_1_0(sparseResidencyImage2D);
    CHECK_FEATURE_1_0(sparseResidencyImage3D);
    CHECK_FEATURE_1_0(sparseResidency2Samples);
    CHECK_FEATURE_1_0(sparseResidency4Samples);
    CHECK_FEATURE_1_0(sparseResidency8Samples);
    CHECK_FEATURE_1_0(sparseResidency16Samples);
    CHECK_FEATURE_1_0(sparseResidencyAliased);
    CHECK_FEATURE_1_0(variableMultisampleRate);
    CHECK_FEATURE_1_0(inheritedQueries);
#undef CHECK_FEATURE_1_0
#define CHECK_FEATURE_1_1(feature) \
  CHECK_VULKAN_FEATURE(device_features11, physical_device_features11_, feature, "1.1 ");
    CHECK_FEATURE_1_1(storageBuffer16BitAccess);
    CHECK_FEATURE_1_1(uniformAndStorageBuffer16BitAccess);
    CHECK_FEATURE_1_1(storagePushConstant16);
    CHECK_FEATURE_1_1(storageInputOutput16);
    CHECK_FEATURE_1_1(multiview);
    CHECK_FEATURE_1_1(multiviewGeometryShader);
    CHECK_FEATURE_1_1(multiviewTessellationShader);
    CHECK_FEATURE_1_1(variablePointersStorageBuffer);
    CHECK_FEATURE_1_1(variablePointers);
    CHECK_FEATURE_1_1(protectedMemory);
    CHECK_FEATURE_1_1(samplerYcbcrConversion);
    CHECK_FEATURE_1_1(shaderDrawParameters);
#undef CHECK_FEATURE_1_1
#define CHECK_FEATURE_1_2(feature) \
  CHECK_VULKAN_FEATURE(device_features12, physical_device_features12_, feature, "1.2 ");
    CHECK_FEATURE_1_2(samplerMirrorClampToEdge);
    CHECK_FEATURE_1_2(drawIndirectCount);
    CHECK_FEATURE_1_2(storageBuffer8BitAccess);
    CHECK_FEATURE_1_2(uniformAndStorageBuffer8BitAccess);
    CHECK_FEATURE_1_2(storagePushConstant8);
    CHECK_FEATURE_1_2(shaderBufferInt64Atomics);
    CHECK_FEATURE_1_2(shaderSharedInt64Atomics);
    CHECK_FEATURE_1_2(shaderFloat16);
    CHECK_FEATURE_1_2(shaderInt8);
    CHECK_FEATURE_1_2(descriptorIndexing);
    CHECK_FEATURE_1_2(shaderInputAttachmentArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderUniformTexelBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderStorageTexelBufferArrayDynamicIndexing);
    CHECK_FEATURE_1_2(shaderUniformBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderSampledImageArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageImageArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderInputAttachmentArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderUniformTexelBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(shaderStorageTexelBufferArrayNonUniformIndexing);
    CHECK_FEATURE_1_2(descriptorBindingUniformBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingSampledImageUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageImageUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingUniformTexelBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingStorageTexelBufferUpdateAfterBind);
    CHECK_FEATURE_1_2(descriptorBindingUpdateUnusedWhilePending);
    CHECK_FEATURE_1_2(descriptorBindingPartiallyBound);
    CHECK_FEATURE_1_2(descriptorBindingVariableDescriptorCount);
    CHECK_FEATURE_1_2(runtimeDescriptorArray);
    CHECK_FEATURE_1_2(samplerFilterMinmax);
    CHECK_FEATURE_1_2(scalarBlockLayout);
    CHECK_FEATURE_1_2(imagelessFramebuffer);
    CHECK_FEATURE_1_2(uniformBufferStandardLayout);
    CHECK_FEATURE_1_2(shaderSubgroupExtendedTypes);
    CHECK_FEATURE_1_2(separateDepthStencilLayouts);
    CHECK_FEATURE_1_2(hostQueryReset);
    CHECK_FEATURE_1_2(timelineSemaphore);
    CHECK_FEATURE_1_2(bufferDeviceAddress);
    CHECK_FEATURE_1_2(bufferDeviceAddressCaptureReplay);
    CHECK_FEATURE_1_2(bufferDeviceAddressMultiDevice);
    CHECK_FEATURE_1_2(vulkanMemoryModel);
    CHECK_FEATURE_1_2(vulkanMemoryModelDeviceScope);
    CHECK_FEATURE_1_2(vulkanMemoryModelAvailabilityVisibilityChains);
    CHECK_FEATURE_1_2(shaderOutputViewportIndex);
    CHECK_FEATURE_1_2(shaderOutputLayer);
    CHECK_FEATURE_1_2(subgroupBroadcastDynamicId);
#undef CHECK_FEATURE_1_2
#define CHECK_FEATURE_1_3(feature) \
  CHECK_VULKAN_FEATURE(device_features13, physical_device_features13_, feature, "1.3 ");
    CHECK_FEATURE_1_3(robustImageAccess);
    CHECK_FEATURE_1_3(inlineUniformBlock);
    CHECK_FEATURE_1_3(descriptorBindingInlineUniformBlockUpdateAfterBind);
    CHECK_FEATURE_1_3(pipelineCreationCacheControl);
    CHECK_FEATURE_1_3(privateData);
    CHECK_FEATURE_1_3(shaderDemoteToHelperInvocation);
    CHECK_FEATURE_1_3(shaderTerminateInvocation);
    CHECK_FEATURE_1_3(subgroupSizeControl);
    CHECK_FEATURE_1_3(computeFullSubgroups);
    CHECK_FEATURE_1_3(synchronization2);
    CHECK_FEATURE_1_3(textureCompressionASTC_HDR);
    CHECK_FEATURE_1_3(shaderZeroInitializeWorkgroupMemory);
    CHECK_FEATURE_1_3(dynamicRendering);
    CHECK_FEATURE_1_3(shaderIntegerDotProduct);
    CHECK_FEATURE_1_3(maintenance4);
#undef CHECK_FEATURE_1_3
    if (!missing_features.empty()) {
      ALOGE("Missing Vulkan features: %s\n", missing_features.c_str());
      assert(false);
      return Result(Result::Code::RuntimeError);
    }
  }

  const VkDeviceCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = create_info_next,
    .queueCreateInfoCount = num_queues,
    .pQueueCreateInfos = queue_cis,
    .enabledExtensionCount = static_cast<uint32>(enabled_device_extension_names_.size()),
    .ppEnabledExtensionNames = enabled_device_extension_names_.data(),
    .pEnabledFeatures = &device_features10,
  };

  VK_CHECK(vkCreateDevice(physical_device_, &ci, nullptr, &device_));

  volkLoadDevice(device_);

  if (!vkTransitionImageLayout) {
    vkTransitionImageLayout = vkTransitionImageLayoutEXT;
  }
  if (!vkCopyMemoryToImage) {
    vkCopyMemoryToImage = vkCopyMemoryToImageEXT;
  }

  vkGetDeviceQueue(
    device_, device_queues_.graphicsQueueFamilyIndex, 0, &device_queues_.graphicsQueue);
  vkGetDeviceQueue(
    device_, device_queues_.computeQueueFamilyIndex, 0, &device_queues_.computeQueue);

  VK_CHECK(setDebugObjectName(device_, VK_OBJECT_TYPE_DEVICE,
    reinterpret_cast<uint64>(device_), "Device::VulkanContext::device_"));

  immediate_ = std::make_unique<VulkanImmediateCommands>(device_,
    device_queues_.graphicsQueueFamilyIndex, has_EXT_device_fault_,
    "Context::immediate_");
  if (device_queues_.computeQueueFamilyIndex != DeviceQueues::INVALID &&
      device_queues_.computeQueueFamilyIndex != device_queues_.graphicsQueueFamilyIndex) {
    immediate_compute_ = std::make_unique<VulkanImmediateCommands>(device_,
      device_queues_.computeQueueFamilyIndex, has_EXT_device_fault_,
      "Context::immediate_compute_");
  }

  // TODO: vulkan pipelien cache.
  {
    const VkPipelineCacheCreateInfo pipeline_cache_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .initialDataSize = config_.pipelineCacheDataSize,
      .pInitialData = config_.pipelineCacheData,
    };
    VK_CHECK(
      vkCreatePipelineCache(device_, &pipeline_cache_ci, nullptr, &pipeline_cache_));
  }

  if (VULKAN_USE_VMA) {
    pimpl_->setVmaAllocator(createVmaAllocator(physical_device_, device_, instance_,
      api_version > VK_API_VERSION_1_3 ? VK_API_VERSION_1_3 : api_version));
  }

  staging_device_ = std::make_unique<StagingDevice>(*this);

  {
    constexpr uint32 pixel = 0xff000000;
    Result result;
    dummy_texture_ =
      this
        ->createTexture(
          {
            .format = EFormat::RGBA_UN8,
            .dimensions = {.width = 1, .height = 1, .depth = 1},
            .usage = ETextureUsageBits::Sampled | ETextureUsageBits::Storage,
            .data = &pixel,
          },
          "Dummy 1x1 (black)", &result)
        .release();
    if (!result.isOk()) {
      return result;
    }

    ASSERT(texture_pool_.getNumObjects() == 1);
  }

  ASSERT(sampler_pool_.getNumObjects() == 0);
  createSampler(
    {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0.0f,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = static_cast<float>(MAX_MIP_LEVELS - 1),
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
    },
    nullptr, "Sampler: default");

  querySurfaceCapabilities();

  dst_sets_.push_back({});

  {
    VkDescriptorSetLayoutBinding bindings[MAX_COLOR_ATTACHMENTS];
    for (uint32 i = 0; i != std::size(bindings); ++i) {
      bindings[i] = {
        .binding = i,
        .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr,
      };
    }

    const VkPhysicalDeviceLimits& limits = this->getVkPhysicalDeviceProperties().limits;
    const VkDescriptorSetLayoutCreateInfo dslci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT,
      .bindingCount = std::min(
        (uint32)MAX_COLOR_ATTACHMENTS, limits.maxPerStageDescriptorInputAttachments),
      .pBindings = bindings,
    };
    VK_CHECK(vkCreateDescriptorSetLayout(
      device_, &dslci, nullptr, &input_attachments_dst_set_layout_));
    VK_CHECK(setDebugObjectName(device_, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
      (uint64)input_attachments_dst_set_layout_,
      "Descriptor Set Layout: "
      "VulkanContext::input_attachments_descriptor_set_layouts"));
  }

  return Result();
}

Result Context::initSwapchain(uint32 width, uint32 height) {
  if (!device_ || !immediate_) {
    ALOGW("Call initContext() first");
    return Result(Result::Code::RuntimeError, "Call initContext() first");
  }

  if (swapchain_) {
    VK_CHECK(vkDeviceWaitIdle(device_));

    immediate_->setLastPresentSemaphore(VK_NULL_HANDLE, VK_NULL_HANDLE);
    immediate_->wait_semaphore_.semaphore = VK_NULL_HANDLE;
    swapchain_ = nullptr;

    vkDestroySemaphore(device_, timeline_semaphore_, nullptr);
  }

  if (!width || !height) {
    return Result(Result::Code::ArgumentOutOfRange, "Width and Height cannot be 0.");
  }

  swapchain_ = std::make_unique<VulkanSwapchain>(*this, width, height);
  timeline_semaphore_ = createSemaphoreTimeline(
    device_, swapchain_->getNumSwapchainImages() - 1, "Semaphore: timeline_semaphore_");

  return swapchain_ ? Result()
                    : Result(Result::Code::RuntimeError, "Failed to create swapchain.");
}

BufferHandle Context::createBuffer(VkDeviceSize buffer_size,
  VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_property_flags,
  Result* out_result, const char* debug_name) {
  ASSERT(buffer_size > 0);

#define ENSURE_BUFFER_SIZE(flag, max_size)                                             \
  if (usage_flags & flag) {                                                            \
    if (buffer_size > max_size) {                                                      \
      Result::setResult(                                                               \
        out_result, Result(Result::Code::RuntimeError, "Buffer size exceeded" #flag)); \
      return {};                                                                       \
    }                                                                                  \
  }

  const VkPhysicalDeviceLimits& limits = getVkPhysicalDeviceProperties().limits;

  ENSURE_BUFFER_SIZE(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, limits.maxUniformBufferRange);
  ENSURE_BUFFER_SIZE(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, limits.maxStorageBufferRange);

  ENSURE_BUFFER_SIZE(
    VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM, physical_device_props11_.maxMemoryAllocationSize);

#undef ENSURE_BUFFER_SIZE

  VulkanBuffer buffer;
  buffer.size_ = buffer_size;
  buffer.usage_flags_ = usage_flags;
  buffer.mem_prop_flags_ = memory_property_flags;

  const VkBufferCreateInfo buffer_ci{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .size = buffer_size,
    .usage = usage_flags,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
  };

  if (VULKAN_USE_VMA) {
    VmaAllocationCreateInfo vma_alloc_info{};
    if (memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vma_alloc_info.flags =
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
      vma_alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
      vma_alloc_info.preferredFlags =
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }

    if (memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      // Ugly, you should create a buffer first, and then get the memory requirements of
      // the created buffer.
      VK_CHECK(vkCreateBuffer(device_, &buffer_ci, nullptr, &buffer.vk_handle_));

      VkMemoryRequirements requirements{};
      vkGetBufferMemoryRequirements(device_, buffer.vk_handle_, &requirements);
      vkDestroyBuffer(device_, buffer.vk_handle_, nullptr);

      buffer.vk_handle_ = VK_NULL_HANDLE;

      if (requirements.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        vma_alloc_info.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        buffer.is_coherent_memory_ = true;
      }
    }

    vma_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    vma_alloc_info.minAlignment = 16;

    vmaCreateBuffer((VmaAllocator)getVmaAllocator(), &buffer_ci, &vma_alloc_info,
      &buffer.vk_handle_, &buffer.vma_allocation_, nullptr);

    // handle memory-mapped buffers
    if (memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      vmaMapMemory(
        (VmaAllocator)getVmaAllocator(), buffer.vma_allocation_, &buffer.mapped_ptr_);
    }
  } else {
    VK_CHECK(vkCreateBuffer(device_, &buffer_ci, nullptr, &buffer.vk_handle_));

    {
      const VkBufferMemoryRequirementsInfo2 buffer_memory_requirements_info2{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
        .buffer = buffer.vk_handle_,
      };
      VkMemoryRequirements2 requirements2{
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
      };

      vkGetBufferMemoryRequirements2(
        device_, &buffer_memory_requirements_info2, &requirements2);
      if (requirements2.memoryRequirements.memoryTypeBits &
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
        buffer.is_coherent_memory_ = true;
      }

      VK_CHECK(allocateMemory2(physical_device_, device_, &requirements2,
        memory_property_flags, &buffer.device_memory_));
      VK_CHECK(vkBindBufferMemory(device_, buffer.vk_handle_, buffer.device_memory_, 0));
    }

    if (memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VK_CHECK(vkMapMemory(
        device_, buffer.device_memory_, 0, buffer.size_, 0, &buffer.mapped_ptr_));
    }
  }

  ASSERT(buffer.vk_handle_ != VK_NULL_HANDLE);

  VK_CHECK(setDebugObjectName(
    device_, VK_OBJECT_TYPE_BUFFER, (uint64)buffer.vk_handle_, debug_name));

  if (usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
    const VkBufferDeviceAddressInfo bda_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = buffer.vk_handle_,
    };

    buffer.device_address_ = vkGetBufferDeviceAddress(device_, &bda_info);
    ASSERT(buffer.device_address_);
  }

  return buffer_pool_.create(std::move(buffer));
}

SamplerHandle Context::createSampler(const VkSamplerCreateInfo& ci, Result* out_result,
  // EFormat yuv_foramt,
  const char* debug_name) {
  VkSamplerCreateInfo sampler_ci = ci;

  VkSampler sampler = VK_NULL_HANDLE;

  VK_CHECK(vkCreateSampler(device_, &sampler_ci, nullptr, &sampler));
  VK_CHECK(
    setDebugObjectName(device_, VK_OBJECT_TYPE_SAMPLER, (uint64)sampler, debug_name));

  SamplerHandle handle = sampler_pool_.create(VkSampler(sampler));

  awaiting_descriptor_update_ = true;

  return handle;
}

AccelerationStructHandle Context::createBLAS(
  const AccelStructDesc& desc, Result* out_result) {
  ASSERT(false, TEXT("Support ray tracing later."));
  return AccelerationStructHandle{};
}

AccelerationStructHandle Context::createTLAS(
  const AccelStructDesc& desc, Result* out_result) {
  ASSERT(false, TEXT("Support ray tracing later."));
  return AccelerationStructHandle{};
}

std::vector<uint8_t> Context::getPipelineCacheData() const {
  size_t size = 0;
  vkGetPipelineCacheData(device_, pipeline_cache_, &size, nullptr);

  std::vector<uint8_t> data(size);
  if (size) {
    vkGetPipelineCacheData(device_, pipeline_cache_, &size, data.data());
  }

  return data;
}

void* Context::getVmaAllocator() const { return pimpl_->vma_allocator_; }

void Context::checkAndUpdateDescriptorSets() {
  if (!awaiting_descriptor_update_) {
    return;
  }

  last_updated_dst_set_ = (last_updated_dst_set_ + 1) % dst_sets_.size();

  if (const DescriptorSet& descriptor_set = dst_sets_[last_updated_dst_set_];
    descriptor_set.dstSet) {
    if (!descriptor_set.retirement.empty() &&
        !isRetirementComplete(descriptor_set.retirement)) {
      // If the old descriptor set still be used, create a new one.
      last_updated_dst_set_ = dst_sets_.size();
      dst_sets_.push_back({});
    }
  }

  DescriptorSet& descriptor_set = dst_sets_[last_updated_dst_set_];
  descriptor_set.retirement = {};

  ASSERT(texture_pool_.getNumObjects() >= 1);
  ASSERT(sampler_pool_.getNumObjects() >= 1);

  // check the size of descriptor pool can hold the descriptors. If not, grow up the
  // descriptor pool.
  uint32 new_max_textures = std::max(descriptor_set.maxTextures, 16u);
  uint32 new_max_samplers = std::max(descriptor_set.maxSamplers, 16u);
  uint32 new_max_accel_structs = std::max(descriptor_set.maxAccelStructures,
    workaround_fixed_size_accel_struct_array_ ? 128u : 1u);

  while (texture_pool_.getSizeOfObjects() > new_max_textures) {
    new_max_textures *= 2u;
  }
  while (sampler_pool_.getSizeOfObjects() > new_max_samplers) {
    new_max_samplers *= 2u;
  }
  while (acceleration_structure_pool_.getSizeOfObjects() > new_max_accel_structs) {
    new_max_accel_structs *= 2u;
  }
  growDescriptorPool(
    descriptor_set, new_max_textures, new_max_samplers, new_max_accel_structs);

  struct RecordInfos {
    std::vector<VkDescriptorImageInfo> sampled_images;
    std::vector<VkDescriptorImageInfo> storage_images;
    // std::vector<VkDescriptorImageInfo> yuv_images;
    VkImageView dummy_image_view;
    VkImageView dummy_storage_image_view;
    VkSampler dummy_sampler;

    std::vector<VkDescriptorImageInfo> samplers;

    std::vector<VkAccelerationStructureKHR> accele_structs;
  };

  RecordInfos record_infos;

  // 1. Sampled and storage images, TODO: YUV images
  record_infos.sampled_images.reserve(texture_pool_.getNumObjects());
  record_infos.storage_images.reserve(texture_pool_.getNumObjects());

  record_infos.dummy_image_view = texture_pool_.front().image_view_;
  record_infos.dummy_storage_image_view = texture_pool_.front().image_view_storage_
                                            ? texture_pool_.front().image_view_storage_
                                            : texture_pool_.front().image_view_;
  record_infos.dummy_sampler = sampler_pool_.front();

  texture_pool_.for_each([&record_infos](const VulkanImage& image) {
    const VkImageView image_view = image.image_view_;
    const VkImageView storage_view =
      image.image_view_storage_ ? image.image_view_storage_ : image_view;

    const bool is_image_available =
      (image.sample_count_ & VK_SAMPLE_COUNT_1_BIT) == VK_SAMPLE_COUNT_1_BIT;
    const bool is_yuv_image = false;
    const bool is_sampled_image =
      is_image_available && image.isSampledImage() && !is_yuv_image;
    const bool is_storage_image = is_image_available && image.isStorageImage();

    record_infos.sampled_images.push_back(VkDescriptorImageInfo{
      .sampler = VK_NULL_HANDLE,
      .imageView = is_sampled_image ? image_view : record_infos.dummy_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    });
    record_infos.storage_images.push_back(VkDescriptorImageInfo{
      .sampler = VK_NULL_HANDLE,
      .imageView =
        is_storage_image ? storage_view : record_infos.dummy_storage_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    });
    // todo: yuv images
  });

  // 2. Samplers
  record_infos.samplers.reserve(sampler_pool_.getNumObjects());
  sampler_pool_.for_each([&record_infos](const VkSampler& sampler) {
    record_infos.samplers.push_back({
      .sampler = sampler ? sampler : record_infos.dummy_sampler,
      .imageView = VK_NULL_HANDLE,
      .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    });
  });

  // 3. Acceleration structures
  record_infos.accele_structs.reserve(acceleration_structure_pool_.getNumObjects());

  VkAccelerationStructureKHR dummy_TLAS = VK_NULL_HANDLE;
  acceleration_structure_pool_.for_each_until(
    [&dummy_TLAS](const AccelerationStructure& as) {
      if (as.vkHandle && as.isTLAS) {
        dummy_TLAS = as.vkHandle;
        return true;
      }
      return false;
    });

  acceleration_structure_pool_.for_each(
    [&record_infos, &dummy_TLAS](const AccelerationStructure& as) {
      record_infos.accele_structs.push_back(as.isTLAS ? as.vkHandle : dummy_TLAS);
    });

  VkWriteDescriptorSetAccelerationStructureKHR write_descriptor_set_accel_struct{
    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
    .accelerationStructureCount = (uint32)record_infos.accele_structs.size(),
    .pAccelerationStructures = record_infos.accele_structs.data(),
  };

  VkWriteDescriptorSet write[BINDINGS_TYPE_SIZE] = {};
  uint32 num_writes = 0;

  if (!record_infos.accele_structs.empty()) {
    write[num_writes++] = VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = &write_descriptor_set_accel_struct,
      .dstSet = descriptor_set.dstSet,
      .dstBinding = static_cast<uint32>(EGlobalDescriptorBinding::AccelerationStructures),
      .dstArrayElement = 0,
      .descriptorCount = (uint32)record_infos.accele_structs.size(),
      .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
    };
  }

  if (!record_infos.sampled_images.empty()) {
    write[num_writes++] = VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_set.dstSet,
      .dstBinding = static_cast<uint32>(EGlobalDescriptorBinding::SampledTextures),
      .dstArrayElement = 0,
      .descriptorCount = (uint32_t)record_infos.sampled_images.size(),
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .pImageInfo = record_infos.sampled_images.data(),
    };
  }

  if (!record_infos.samplers.empty()) {
    write[num_writes++] = VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_set.dstSet,
      .dstBinding = static_cast<uint32>(EGlobalDescriptorBinding::Samplers),
      .dstArrayElement = 0,
      .descriptorCount = (uint32_t)record_infos.samplers.size(),
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .pImageInfo = record_infos.samplers.data(),
    };
  }

  if (!record_infos.storage_images.empty()) {
    write[num_writes++] = VkWriteDescriptorSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_set.dstSet,
      .dstBinding = static_cast<uint32>(EGlobalDescriptorBinding::StorageTextures),
      .dstArrayElement = 0,
      .descriptorCount = (uint32_t)record_infos.storage_images.size(),
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .pImageInfo = record_infos.storage_images.data(),
    };
  }

  // TODO: yuv images
  // if (!record_infos.yuv_images.empty()) {
  //   write[num_writes++] = VkWriteDescriptorSet{
  //     .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
  //     .dstSet = descriptor_set.dstSet,
  //     .dstBinding = static_cast<uint32>(EGlobalDescriptorBinding::YuvTextures),
  //     .dstArrayElement = 0,
  //     .descriptorCount = (uint32_t)record_infos.yuv_images.size(),
  //     .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
  //     .pImageInfo = record_infos.yuv_images.data(),
  //   };
  // }

  if (num_writes) {
    // TODO: profiling this.
    vkUpdateDescriptorSets(device_, num_writes, write, 0, nullptr);
  }

  awaiting_descriptor_update_ = false;
}

void Context::bindDefaultDescriptorSets(VkCommandBuffer cmdBuf,
  VkPipelineBindPoint bindPoint, VkPipelineLayout layout, SubmitHandle submit,
  bool compute) {
  DescriptorSet& descriptor_set = dst_sets_[last_updated_dst_set_];
  (compute ? descriptor_set.retirement.compute : descriptor_set.retirement.graphics) =
    submit;
  vkCmdBindDescriptorSets(cmdBuf, bindPoint, layout, 0, 1,
    &dst_sets_[last_updated_dst_set_].dstSet, 0, nullptr);
}

Result Context::createInstance() {
  instance_ = VK_NULL_HANDLE;

  // Check the system has `VK_LAYER_KHRONOS_validation` or not.
  {
    uint32 num_layer_props = 0;
    vkEnumerateInstanceLayerProperties(&num_layer_props, nullptr);
    std::vector<VkLayerProperties> layer_props(num_layer_props);
    vkEnumerateInstanceLayerProperties(&num_layer_props, layer_props.data());

    [this, &layer_props]() -> void {
      for (const auto& layer_prop : layer_props) {
        for (const char* layer_name : DEFAULT_VALIDATION_LAYERS) {
          if (!strcmp(layer_prop.layerName, layer_name)) {
            khronos_validation_version_ = layer_prop.specVersion;
            return;
          }
        }
      }
      config_.bEnableValidation = false;
    }();
  }

  std::vector<VkExtensionProperties> instance_extensions;
  {
    uint32 count = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr));
    instance_extensions.resize(count);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(
      nullptr, &count, instance_extensions.data()));
  }
  {
    if (config_.bEnableValidation) {
      for (const char* layer_name : DEFAULT_VALIDATION_LAYERS) {
        uint32 count = 0;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(layer_name, &count, nullptr));
        if (count > 0) {
          const size_t size = instance_extensions.size();
          instance_extensions.resize(size + count);
          VK_CHECK(vkEnumerateInstanceExtensionProperties(
            layer_name, &count, instance_extensions.data() + size));
        }
      }
    }
  }

  enabled_instance_extension_names_ = {
    VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(PLATFORM_WINDOWS)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(PLATFORM_ANDROID)
    VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#else
    ASSERT(false, TEXT("Unsupported platform."));
#endif
  };

  const bool has_debug_utils =
    _has_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instance_extensions);
  if (has_debug_utils) {
    enabled_instance_extension_names_.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  if (_has_extension(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME, instance_extensions)) {
    enabled_instance_extension_names_.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
  }

  if (_has_extension(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME, instance_extensions)) {
    enabled_instance_extension_names_.push_back(
      VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  }
  // Lavapipe 25.3.3 supports both `VK_KHR_surface_maintenance` and
  // `VK_EXT_surface_maintenance`, but only supports `VK_EXT_swapchain_maintenance1`,
  // which in turn requires `VK_EXT_surface_maintenance1`
  if (_has_extension(VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME, instance_extensions)) {
    // remove once VK_KHR_surface_maintenance1 becomes mandatory
    enabled_instance_extension_names_.push_back(
      VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
  }

  if (_has_extension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME, instance_extensions)) {
    has_EXT_swapchain_colorspace_ = true;
    enabled_instance_extension_names_.push_back(
      VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
  }

  if (_has_extension(
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, instance_extensions)) {
    // required by the instance extension VK_EXT_surface_maintenance1
    enabled_instance_extension_names_.push_back(
      VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
  }

  for (const char* ext : config_.extensionsInstance) {
    if (ext) {
      enabled_instance_extension_names_.push_back(ext);
    }
  }

  // GPU Assisted Validation doesn't work on Android.
  // It implicitly requires vertexPipelineStoresAndAtomics feature that's not supported
  // even on high-end devices.
#if defined(ANDROID)
  const bool enable_gpu_av = false;
#else
  const bool enable_gpu_av = config_.bEnableValidation && config_.bEnableValidationGpuAV;
#endif  // ANDROID

  const VkBool32 gpuav_enable = enable_gpu_av ? VK_TRUE : VK_FALSE;
  const VkBool32 gpuav_post_process_descriptor_indexing =
    VK_FALSE;  // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/9222

#define LAYER_SETTINGS_BOOL32(name, var)                                       \
  VkLayerSettingEXT {                                                          \
    .pLayerName = DEFAULT_VALIDATION_LAYERS[0], .pSettingName = name,          \
    .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT, .valueCount = 1, .pValues = var, \
  }
  const VkLayerSettingEXT settings[] = {
    LAYER_SETTINGS_BOOL32("gpuav_enable", &gpuav_enable),
    LAYER_SETTINGS_BOOL32(
      "gpuav_post_process_descriptor_indexing", &gpuav_post_process_descriptor_indexing),
  };
#undef LAYER_SETTINGS_BOOL32

  const VkLayerSettingsCreateInfoEXT layerSettingsCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
    .settingCount = static_cast<uint32>(ARRAY_COUNT(settings)),
    .pSettings = settings,
  };

  // check missing extensions
  {
    std::string missing_extension_str;
    std::vector<std::string> missing_extensions;
    for (const char* ext : enabled_instance_extension_names_) {
      if (!_has_extension(ext, instance_extensions)) {
        missing_extensions.push_back(ext);
      }
    }

    if (!missing_extensions.empty()) {
      pystring::join(missing_extension_str, missing_extensions);
      ALOGE("Missing vulkan instance extensiosn: %s", missing_extension_str.c_str());

      return Result(Result::Code::RuntimeError);
    }
  }

  const VkApplicationInfo app_info{
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext = nullptr,
    .pApplicationName = "B2Engine",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "B2Engine",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion =
      config_.vulkanVersion == VkVersion_1_3 ? VK_API_VERSION_1_3 : VK_API_VERSION_1_4,
  };

  const VkInstanceCreateInfo ci{
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = _has_extension(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME, instance_extensions)
               ? &layerSettingsCreateInfo
               : nullptr,
    .flags = 0u,
    .pApplicationInfo = &app_info,
    .enabledLayerCount = config_.bEnableValidation
                           ? static_cast<uint32>(ARRAY_COUNT(DEFAULT_VALIDATION_LAYERS))
                           : 0u,
    .ppEnabledLayerNames =
      config_.bEnableValidation ? DEFAULT_VALIDATION_LAYERS : nullptr,
    .enabledExtensionCount =
      static_cast<uint32>(enabled_instance_extension_names_.size()),
    .ppEnabledExtensionNames = enabled_instance_extension_names_.data(),
  };

  VK_CHECK(vkCreateInstance(&ci, nullptr, &instance_));

  volkLoadInstance(instance_);

  // debug messenger
  if (has_debug_utils) {
    const VkDebugUtilsMessengerCreateInfoEXT ci = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = &_debug_utils_callback,
      .pUserData = this,
    };

    VK_CHECK(
      vkCreateDebugUtilsMessengerEXT(instance_, &ci, nullptr, &debug_utils_messenger_));
  }

  ALOGI("%s layer version: %u.%u.%u", DEFAULT_VALIDATION_LAYERS[0],
    VK_VERSION_MAJOR(khronos_validation_version_),
    VK_VERSION_MINOR(khronos_validation_version_),
    VK_VERSION_PATCH(khronos_validation_version_));

  // log available instance extensions
  ALOGI("Vulkan instance extensions:");

  for (const VkExtensionProperties& extension : instance_extensions) {
    ALOGI("  %s", extension.extensionName);
  }

  return Result();
}  // namespace vk

void Context::createSurface(void* window, void* display) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  const VkWin32SurfaceCreateInfoKHR ci = {
    .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
    .hinstance = GetModuleHandle(nullptr),
    .hwnd = (HWND)window,
  };
  VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &ci, nullptr, &surface_));
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
  const VkAndroidSurfaceCreateInfoKHR ci = {
    .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .window = (ANativeWindow*)window,
  };
  VK_CHECK(vkCreateAndroidSurfaceKHR(instance_, &ci, nullptr, &surface_));
#else
  ASSERT(false, "No windowed surface API available; use createHeadlessSurface()");
#endif
}

void Context::createHeadlessSurface() {
  ASSERT(false, TEXT("Support headless surface later."));
}

void Context::querySurfaceCapabilities() {
  constexpr VkFormat depth_formats[] = {
    VK_FORMAT_D32_SFLOAT_S8_UINT,  //
    VK_FORMAT_D24_UNORM_S8_UINT,   //
    VK_FORMAT_D16_UNORM_S8_UINT,   //
    VK_FORMAT_D32_SFLOAT,          //
    VK_FORMAT_D16_UNORM,           //
  };
  for (const VkFormat& depth_format : depth_formats) {
    VkFormatProperties2 props{
      .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
    };
    vkGetPhysicalDeviceFormatProperties2(physical_device_, depth_format, &props);

    if (props.formatProperties.optimalTilingFeatures) {
      device_depth_formats_.push_back(depth_format);
    }
  }

  if (surface_ == VK_NULL_HANDLE) return;

  VkPhysicalDeviceSurfaceInfo2KHR physical_device_surface_info2{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
    .pNext = nullptr,
    .surface = surface_,
  };
  vkGetPhysicalDeviceSurfaceCapabilities2KHR(
    physical_device_, &physical_device_surface_info2, &device_surface_capabilities_);

  uint32 format_count;
  vkGetPhysicalDeviceSurfaceFormats2KHR(
    physical_device_, &physical_device_surface_info2, &format_count, nullptr);

  if (format_count) {
    device_surface_formats_.resize(format_count);
    // NOTE: the structure should set `sType` either.
    for (auto& device_surface_format : device_surface_formats_)
      device_surface_format.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
    vkGetPhysicalDeviceSurfaceFormats2KHR(physical_device_,
      &physical_device_surface_info2, &format_count, device_surface_formats_.data());
  }

  uint32 present_mode_count;
  vkGetPhysicalDeviceSurfacePresentModesKHR(
    physical_device_, surface_, &present_mode_count, nullptr);
  if (present_mode_count) {
    device_present_modes_.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device_, surface_, &present_mode_count, device_present_modes_.data());
  }
}

RetirementPoint Context::contextRetirementPoint() const {
  RetirementPoint retirement{
    .graphics = immediate_->getLastSubmitHandle(),
    .compute =
      immediate_compute_ ? immediate_compute_->getLastSubmitHandle() : SubmitHandle{},
  };

  if (pimpl_->graphics_command_buffer_.ctx_) {
    retirement.graphics = immediate_->getNextSubmitHandle();
  }
  if (immediate_compute_ && pimpl_->compute_command_buffer_.ctx_) {
    retirement.compute = immediate_compute_->getNextSubmitHandle();
  }

  return retirement;
}

bool Context::isRetirementComplete(const RetirementPoint& retirement) const {
  const bool graphics_complete =
    retirement.graphics.empty() || immediate_->isReady(retirement.graphics);
  const bool compute_complete =
    retirement.compute.empty() ||
    (immediate_compute_ && immediate_compute_->isReady(retirement.compute));
  return graphics_complete && compute_complete;
}

bool Context::waitRetirement(const RetirementPoint& retirement) {
  if (!retirement.graphics.empty()) {
    if (!immediate_->isSubmitted(retirement.graphics)) {
      return false;
    }
    immediate_->wait(retirement.graphics);
  }

  if (!retirement.compute.empty()) {
    if (!immediate_compute_ || !immediate_compute_->isSubmitted(retirement.compute)) {
      return false;
    }
    immediate_compute_->wait(retirement.compute);
  }

  return true;
}

void Context::deferredTask(std::packaged_task<void()>&& task) const {
  deferredTask(std::move(task), contextRetirementPoint());
}

void Context::deferredTask(
  std::packaged_task<void()>&& task, RetirementPoint retirement) const {
  pimpl_->deferred_tasks_.emplace_back(std::move(task), retirement);
  processDeferredTasks();
}

void Context::processDeferredTasks() const {
  size_t index = 0;
  while (index < pimpl_->deferred_tasks_.size()) {
    if (!isRetirementComplete(pimpl_->deferred_tasks_[index].retirement)) {
      ++index;
      continue;
    }

    std::packaged_task<void()> task = std::move(pimpl_->deferred_tasks_[index].task);
    pimpl_->deferred_tasks_.erase(pimpl_->deferred_tasks_.begin() + index);
    task();
    index = 0;
  }
}

void Context::waitDeferredTasks(bool device_idle) {
  size_t index = 0;
  while (index < pimpl_->deferred_tasks_.size()) {
    const RetirementPoint retirement = pimpl_->deferred_tasks_[index].retirement;
    if (!device_idle && !waitRetirement(retirement)) {
      ++index;
      continue;
    }

    std::packaged_task<void()> task = std::move(pimpl_->deferred_tasks_[index].task);
    pimpl_->deferred_tasks_.erase(pimpl_->deferred_tasks_.begin() + index);
    task();
    index = 0;
  }
}

void Context::generateMipmap(TextureHandle handle) {
  if (handle.empty()) return;

  const VulkanImage* image = texture_pool_.get(handle);
  ASSERT(!image->pending_destroy_,
    "Cannot generate mipmaps for a texture pending destruction");

  if (image->num_levels_ <= 1) {
    return;
  }

  ASSERT(image->layout_ != VK_IMAGE_LAYOUT_UNDEFINED,
    TEXT("Texture layout should not be undefined."));

  VulkanImmediateCommands::CommandBufferWrapper& wrapper = immediate_->acquire();
  image->generateMipmap(wrapper.cmdBuf);
  const SubmitHandle submit = immediate_->submit(wrapper);
  TextureHandle tracked = handle;
  while (!tracked.empty()) {
    resource_tracker_.record(tracked, submit, false);
    const VulkanImage* tracked_image = texture_pool_.get(tracked);
    tracked = tracked_image ? tracked_image->parent_ : TextureHandle{};
  }
}

Result Context::growDescriptorPool(DescriptorSet& dset, uint32 max_textures,
  uint32 max_samplers, uint32 max_accel_structs) {
  if (max_textures == dset.maxTextures && max_samplers == dset.maxSamplers &&
      max_accel_structs == dset.maxAccelStructures && !awaiting_new_immutable_samplers_) {
    return Result();
  }

  ASSERT(
    max_textures <= physical_device_props12_.maxDescriptorSetUpdateAfterBindSampledImages,
    TEXT("Max textures exceeded: %u (max %u)"), max_textures,
    physical_device_props12_.maxDescriptorSetUpdateAfterBindSampledImages);
  ASSERT(max_samplers <= physical_device_props12_.maxDescriptorSetUpdateAfterBindSamplers,
    TEXT("Max samplers exceeded: %u (max %u)"), max_samplers,
    physical_device_props12_.maxDescriptorSetUpdateAfterBindSamplers);

  dset.maxTextures = max_textures;
  dset.maxSamplers = max_samplers;
  dset.maxAccelStructures = max_accel_structs;

  // With growing the pool, the descriptorSetLayout and descriptorPool will create a new
  // one, push the old resources to deferred deleted queue.
  if (dset.dstSetLayout != VK_NULL_HANDLE) {
    deferredTask(std::packaged_task([device = device_, dsl = dset.dstSetLayout] {
      vkDestroyDescriptorSetLayout(device, dsl, nullptr);
    }));
  }
  if (dset.dstPool != VK_NULL_HANDLE) {
    deferredTask(std::packaged_task([device = device_, pool = dset.dstPool] {
      vkDestroyDescriptorPool(device, pool, nullptr);
    }));
  }

  // TODO: YUV images.

  // create default descriptor set layout which is going to be shared by graphics
  // pipelines
  VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT |
                                   VK_SHADER_STAGE_FRAGMENT_BIT |
                                   VK_SHADER_STAGE_COMPUTE_BIT;
  if (has_KHR_ray_tracing_pipeline_) {
    stage_flags |=
      (VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR);
  }

  VkDescriptorSetLayoutBinding bindings[BINDINGS_TYPE_SIZE]{};
  uint32 num_bindings = 0;
  bindings[num_bindings++] = VkDescriptorSetLayoutBinding{
    .binding = static_cast<uint32>(EGlobalDescriptorBinding::SampledTextures),
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    .descriptorCount = max_textures,
    .stageFlags = stage_flags,
  };
  bindings[num_bindings++] = VkDescriptorSetLayoutBinding{
    .binding = static_cast<uint32>(EGlobalDescriptorBinding::Samplers),
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
    .descriptorCount = max_samplers,
    .stageFlags = stage_flags,
  };
  bindings[num_bindings++] = VkDescriptorSetLayoutBinding{
    .binding = static_cast<uint32>(EGlobalDescriptorBinding::StorageTextures),
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    .descriptorCount = max_textures,
    .stageFlags = stage_flags,
  };
  if (has_KHR_acceleration_structure_) {
    bindings[num_bindings++] = VkDescriptorSetLayoutBinding{
      .binding = static_cast<uint32>(EGlobalDescriptorBinding::AccelerationStructures),
      .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
      .descriptorCount = max_accel_structs,
      .stageFlags = stage_flags,
    };
  }

  const uint32 flags =
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |            // update after bind
    VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |  // update the unused slot
                                                             // while pending
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;               // make empty slot legal

  VkDescriptorBindingFlags binding_flags[BINDINGS_TYPE_SIZE]{};
  std::fill_n(binding_flags, num_bindings, flags);

  const VkDescriptorSetLayoutBindingFlagsCreateInfo dsl_binding_flags_ci{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
    .bindingCount = num_bindings,
    .pBindingFlags = binding_flags,
  };

  const VkDescriptorSetLayoutCreateInfo dsl_ci{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = &dsl_binding_flags_ci,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    .bindingCount = num_bindings,
    .pBindings = bindings,
  };

  VK_CHECK(vkCreateDescriptorSetLayout(device_, &dsl_ci, nullptr, &dset.dstSetLayout));
  VK_CHECK(setDebugObjectName(
    device_, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64)dset.dstSetLayout, "DSL"));

  {
    VkDescriptorPoolSize pool_sizes[BINDINGS_TYPE_SIZE]{
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, max_textures},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, max_samplers},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_textures},
    };

    uint32 num_pool_sizes = 3;
    if (has_KHR_acceleration_structure_) {
      pool_sizes[num_pool_sizes++] = VkDescriptorPoolSize{
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, max_accel_structs};
    }

    ASSERT(num_pool_sizes <= BINDINGS_TYPE_SIZE,
      TEXT("The pool size should less than BINDINGS_TYPE_SIZE"));

    // How many pools should created.
    const VkDescriptorPoolCreateInfo dp_ci{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      .maxSets = 1,
      .poolSizeCount = num_pool_sizes,
      .pPoolSizes = pool_sizes,
    };
    VK_CHECK(vkCreateDescriptorPool(device_, &dp_ci, nullptr, &dset.dstPool));

    const VkDescriptorSetAllocateInfo dst_allocate_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = dset.dstPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &dset.dstSetLayout,
    };
    VK_CHECK(vkAllocateDescriptorSets(device_, &dst_allocate_info, &dset.dstSet));
  }

  awaiting_new_immutable_samplers_ = false;

  return Result();
}

ShaderModuleState Context::createShaderModuleFromSPIRV(
  const void* spirv, size_t num_bytes, const char* debug_name, Result* out_result) const {
  UNREF_PARAM(debug_name);

  if (!spirv || !num_bytes) {
    Result::setResult(out_result,
      Result(Result::Code::ArgumentOutOfRange, "Expecting non-empty SPIR-V code."));
    return {};
  }

  SpvReflectShaderModule srsm;
  const SpvReflectResult result = spvReflectCreateShaderModule(num_bytes, spirv, &srsm);
  ASSERT(
    result == SPV_REFLECT_RESULT_SUCCESS, TEXT("spvReflectCreateShaderModule failed"));
  SCOPE_EXIT { spvReflectDestroyShaderModule(&srsm); };

  uint32 push_constant_size = 0;

  for (uint32 i = 0; i < srsm.push_constant_block_count; ++i) {
    const SpvReflectBlockVariable& srbv = srsm.push_constant_blocks[i];
    push_constant_size = std::max(push_constant_size, srbv.offset + srbv.size);
  }

  const VkShaderModuleCreateInfo ci{
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = num_bytes,
    .pCode = static_cast<const uint32*>(malloc(num_bytes)),
  };

  memcpy((void*)ci.pCode, spirv, num_bytes);

  ShaderModuleState state{
    .createInfo = ci,
    .pushConstantsSize = push_constant_size,
  };

  const VkResult create_result =
    vkCreateShaderModule(device_, &ci, nullptr, &state.module);
  if (create_result != VK_SUCCESS) {
    free(const_cast<uint32*>(ci.pCode));
    Result::setResult(
      out_result, Result(Result::Code::RuntimeError, "Cannot create VkShaderModule"));
    return {};
  }

  return state;
}

const VkSamplerYcbcrConversionInfo* Context::getOrCreateYcbcrConversionInfo(
  EFormat format) {
  ASSERT(false, TEXT("Support Ycbcr later."));
  return nullptr;
}

VkSampler Context::getOrCreateYcbcrSampler(EFormat format) {
  ASSERT(false, TEXT("Support Ycbcr later."));
  return VK_NULL_HANDLE;
}

void Context::getBuildInfoBLAS(const AccelStructDesc& desc,
  VkAccelerationStructureGeometryKHR& out_geom,
  VkAccelerationStructureBuildSizesInfoKHR& out_info) const {
  ASSERT(desc.type == EAccelStructType::BLAS);
  ASSERT(desc.geometryType == EAccelStructGeomType::Triangles);
  ASSERT(desc.numVertices);
  ASSERT(desc.indexBuffer.valid());
  ASSERT(desc.vertexBuffer.valid());
  ASSERT(desc.transformBuffer.valid());
  ASSERT(desc.buildRange.primitiveCount);

  ASSERT(buffer_pool_.get(desc.indexBuffer)->usage_flags_ &
         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  ASSERT(buffer_pool_.get(desc.vertexBuffer)->usage_flags_ &
         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
  ASSERT(buffer_pool_.get(desc.transformBuffer)->usage_flags_ &
         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

  VkGeometryFlagsKHR geometry_flags = 0;
  if (EnumHasAllFlags(desc.geometryFlags, EAccelStructGeometryFlagBits::Opaque)) {
    geometry_flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
  }
  if (EnumHasAllFlags(
        desc.geometryFlags, EAccelStructGeometryFlagBits::NoDuplicateAnyHit)) {
    geometry_flags |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
  }

  out_geom = VkAccelerationStructureGeometryKHR{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
    .geometry =
      {
        .triangles =
          {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = toVkFormat(desc.vertexFormat),
            .vertexData = {.deviceAddress = gpuAddress(desc.vertexBuffer, 0)},
            .maxVertex = desc.numVertices - 1,  //?
            .indexType = toVkIndexType(desc.indexFormat),
            .indexData = {.deviceAddress = gpuAddress(desc.indexBuffer, 0)},
            .transformData = {.deviceAddress = gpuAddress(desc.transformBuffer, 0)},
          },
      },
    .flags = geometry_flags,
  };

  const VkAccelerationStructureBuildGeometryInfoKHR accel_struct_build_geom_info{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
    .geometryCount = 1,
    .pGeometries = &out_geom,
  };

  out_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  vkGetAccelerationStructureBuildSizesKHR(device_,
    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &accel_struct_build_geom_info,
    &desc.buildRange.primitiveCount, &out_info);

  const uint32 alignment =
    acceleration_structure_properties_.minAccelerationStructureScratchOffsetAlignment;
  out_info.accelerationStructureSize += alignment;
  out_info.updateScratchSize += alignment;
  out_info.buildScratchSize += alignment;
}

void Context::getBuildInfoTLAS(const AccelStructDesc& desc,
  VkAccelerationStructureGeometryKHR& out_geom,
  VkAccelerationStructureBuildSizesInfoKHR& out_info) const {
  ASSERT(desc.type == EAccelStructType::TLAS);
  ASSERT(desc.geometryType == EAccelStructGeomType::Instances);
  ASSERT(desc.numVertices == 0);
  ASSERT(desc.instancesBuffer.valid());
  ASSERT(desc.buildRange.primitiveCount);

  ASSERT(buffer_pool_.get(desc.indexBuffer)->usage_flags_ &
         VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);

  VkGeometryFlagsKHR geometry_flags = 0;
  if (EnumHasAllFlags(desc.geometryFlags, EAccelStructGeometryFlagBits::Opaque)) {
    geometry_flags |= VK_GEOMETRY_OPAQUE_BIT_KHR;
  }
  if (EnumHasAllFlags(
        desc.geometryFlags, EAccelStructGeometryFlagBits::NoDuplicateAnyHit)) {
    geometry_flags |= VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
  }

  out_geom = VkAccelerationStructureGeometryKHR{
    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
    .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
    .geometry =
      {
        .instances =
          {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE,
            .data = {.deviceAddress = gpuAddress(desc.instancesBuffer, 0)},
          },
      },
    .flags = geometry_flags,
  };

  const VkAccelerationStructureBuildGeometryInfoKHR
    accelerationStructureBuildGeometryInfo = {
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = toVkBuildAccelerationStructureFlags(desc.buildFlags),
      .geometryCount = 1,
      .pGeometries = &out_geom,
  };

  out_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  vkGetAccelerationStructureBuildSizesKHR(device_,
    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    &accelerationStructureBuildGeometryInfo, &desc.buildRange.primitiveCount, &out_info);
  const uint32_t alignment =
    acceleration_structure_properties_.minAccelerationStructureScratchOffsetAlignment;
  out_info.accelerationStructureSize += alignment;
  out_info.updateScratchSize += alignment;
  out_info.buildScratchSize += alignment;
}

VkBuffer Context::getVkBuffer(BufferHandle handle) const {
  if (handle.empty()) {
    return VK_NULL_HANDLE;
  }
  const VulkanBuffer* buffer = buffer_pool_.get(handle);
  ASSERT(buffer, TEXT("Buffer not found in pool."));
  return buffer->vk_handle_;
}

VkImage Context::getVkImage(TextureHandle handle) const {
  if (handle.empty()) {
    return VK_NULL_HANDLE;
  }

  const VulkanImage* image = texture_pool_.get(handle);
  ASSERT(image, TEXT("Image not found in texture pool."));
  return image->vk_handle_;
}

VkImageView Context::getVkImageView(TextureHandle handle) const {
  if (handle.empty()) {
    return VK_NULL_HANDLE;
  }

  const VulkanImage* image = texture_pool_.get(handle);
  ASSERT(image, TEXT("Image not found in texture pool."));
  return image->image_view_;
}

VkDeviceAddress Context::getVkAccelerationStructureDeviceAddress(
  AccelerationStructHandle handle) const {
  if (handle.empty()) {
    return 0;
  }
  return acceleration_structure_pool_.get(handle)->deviceAddress;
}

VkAccelerationStructureKHR Context::getVkAccelerationStructure(
  AccelerationStructHandle handle) const {
  if (handle.empty()) {
    return VK_NULL_HANDLE;
  }
  return acceleration_structure_pool_.get(handle)->vkHandle;
}

VkBuffer Context::getVkBuffer(AccelerationStructHandle handle) const {
  if (handle.empty()) {
    return VK_NULL_HANDLE;
  }

  return getVkBuffer(acceleration_structure_pool_.get(handle)->buffer);
}

VkPipeline Context::getVkPipeline(RayTracingPipelineHandle handle) const {
  if (handle.empty()) {
    return VK_NULL_HANDLE;
  }

  return ray_tracing_pipeline_pool_.get(handle)->pipeline;
}

VkPipelineLayout Context::getVkPipelineLayout(RayTracingPipelineHandle handle) const {
  if (handle.empty()) {
    return VK_NULL_HANDLE;
  }

  return ray_tracing_pipeline_pool_.get(handle)->pipelineLayout;
}

VkDeviceSize Context::getBufferSize(BufferHandle handle) const {
  if (handle.empty()) {
    return 0;
  }

  const VulkanBuffer* buffer = buffer_pool_.get(handle);
  ASSERT(buffer, TEXT("Buffer not found in pool."));
  return buffer->size_;
}

VkDevice getVkDevice(const IContext* ctx) {
  if (!ctx) {
    return VK_NULL_HANDLE;
  }
  return static_cast<const Context*>(ctx)->getVkDevice();
}

VkPhysicalDevice getVkPhysicalDevice(const IContext* ctx) {
  if (!ctx) {
    return VK_NULL_HANDLE;
  }
  return static_cast<const Context*>(ctx)->getVkPhysicalDevice();
}

VkBuffer getVkBuffer(const IContext* ctx, BufferHandle buffer) {
  if (!ctx || buffer.empty()) {
    return VK_NULL_HANDLE;
  }

  return static_cast<const Context*>(ctx)->getVkBuffer(buffer);
}

VkImage getVkImage(const IContext* ctx, TextureHandle texture) {
  if (!ctx || texture.empty()) {
    return VK_NULL_HANDLE;
  }

  return static_cast<const Context*>(ctx)->getVkImage(texture);
}

VkImageView getVkImageView(const IContext* ctx, TextureHandle texture) {
  if (!ctx || texture.empty()) {
    return VK_NULL_HANDLE;
  }

  return static_cast<const Context*>(ctx)->getVkImageView(texture);
}

VkDeviceAddress getVkAccelerationStructureDeviceAddress(
  const IContext* ctx, AccelerationStructHandle as) {
  if (!ctx || as.empty()) {
    return 0;
  }

  return static_cast<const Context*>(ctx)->getVkAccelerationStructureDeviceAddress(as);
}

VkAccelerationStructureKHR getVkAccelerationStructure(
  const IContext* ctx, AccelerationStructHandle as) {
  if (!ctx || as.empty()) {
    return VK_NULL_HANDLE;
  }
  return static_cast<const Context*>(ctx)->getVkAccelerationStructure(as);
}

VkBuffer getVkBuffer(const IContext* ctx, AccelerationStructHandle buffer) {
  if (!ctx || buffer.empty()) {
    return VK_NULL_HANDLE;
  }

  return static_cast<const Context*>(ctx)->getVkBuffer(buffer);
}

VkPipeline getVkPipeline(const IContext* ctx, RayTracingPipelineHandle pipeline) {
  if (!ctx || pipeline.empty()) {
    return VK_NULL_HANDLE;
  }

  return static_cast<const Context*>(ctx)->getVkPipeline(pipeline);
}

VkPipelineLayout getVkPipelineLayout(
  const IContext* ctx, RayTracingPipelineHandle pipeline) {
  if (!ctx || pipeline.empty()) {
    return VK_NULL_HANDLE;
  }

  return static_cast<const Context*>(ctx)->getVkPipelineLayout(pipeline);
}

VkDeviceSize getBufferSize(const IContext* ctx, BufferHandle handle) {
  if (!ctx || handle.empty()) {
    return 0;
  }
  return static_cast<const Context*>(ctx)->getBufferSize(handle);
}

}  // namespace vk

}  // namespace amaryl

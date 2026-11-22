#pragma once

#include <array>
#include <future>
#include <string>
#include <vector>

#include "amaryl/nrhi/nrhi.h"
#include "amaryl/nrhi/resource_tracker.h"
#include "amaryl/nrhi/vulkan/config.h"
#include "amaryl/nrhi/vulkan/utils.h"

struct SDL_Window;

namespace amaryl {

namespace vk {
struct Context;
struct VulkanBuffer;
struct VulkanImage;
struct VulkanSwapchain;
struct VulkanImmediateCommands;

constexpr uint32 MAX_CUSOTM_EXTENSIONS = 32;
constexpr uint32 MAX_PRESENT_MODES = 8;

enum VulkanVersion {
  VkVersion_1_3,
  VkVersion_1_4,
};

struct ShaderModuleState final {
  VkShaderModuleCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .codeSize = 0,
      .pCode = nullptr,
  };
  VkShaderModule module{VK_NULL_HANDLE};
  uint32 pushConstantsSize = 0u;
};

struct RenderPipelineState final {
  RenderPipelineDesc desc;

  // Adreno 840: YUV texture index from spec constant, cached at creation time before
  // the data pointer goes stale NOTE: mobile yuv texture, do this later...
  uint32 workaround_yuvTextureIndex_ = UINT32_MAX;

  uint32 numBindings{0u};
  uint32 numAttributes{0u};

  VkVertexInputBindingDescription bindings[VertexInput::MAX_VERTEX_BUFFER]{};
  VkVertexInputAttributeDescription attributes[VertexInput::MAX_VERTEX_ATTRIBUTES]{};

  // non-owning, the last seen VkDescriptorSetLayout from VulkanContext::vkDSL_ (if the
  // context has a new layout, invalidate all VkPipeline objects)
  VkDescriptorSetLayout lastVkDescriptorSetLayout{VK_NULL_HANDLE};

  VkShaderStageFlags shaderStageFlags{0};
  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
  VkPipeline pipeline{VK_NULL_HANDLE};

  void* specConstantDataStorage{nullptr};
  uint32 viewMask{0u};
};

struct VulkanPipelineBuilder final {
  VulkanPipelineBuilder();
  ~VulkanPipelineBuilder() = default;

  VulkanPipelineBuilder& dynamicState(VkDynamicState state, bool enable = true);
  VulkanPipelineBuilder& createFlags(VkPipelineCreateFlags flags, bool enable = true);
  VulkanPipelineBuilder& primitiveTopology(VkPrimitiveTopology topology);
  VulkanPipelineBuilder& rasterizationSamples(VkSampleCountFlagBits samples,
                                              float min_sample_shading);
  VulkanPipelineBuilder& alphaToCoverage(bool enable);
  VulkanPipelineBuilder& shaderStage(VkPipelineShaderStageCreateInfo stage);
  VulkanPipelineBuilder& stencilStateOps(VkStencilFaceFlags face_mask,
                                         VkStencilOp fail_op, VkStencilOp pass_op,
                                         VkStencilOp depth_fail_op,
                                         VkCompareOp compare_op);
  VulkanPipelineBuilder& stencilMasks(VkStencilFaceFlags face_mask, uint32 compare_mask,
                                      uint32 write_mask, uint32 ref);
  VulkanPipelineBuilder& cullMode(VkCullModeFlags mode);
  VulkanPipelineBuilder& frontFace(VkFrontFace mode);
  VulkanPipelineBuilder& polygonMode(VkPolygonMode mode);
  VulkanPipelineBuilder& vertexInputState(
      const VkPipelineVertexInputStateCreateInfo& state);
  VulkanPipelineBuilder& viewMask(uint32 mask);
  VulkanPipelineBuilder& colorAttachments(
      const VkPipelineColorBlendAttachmentState* states, const VkFormat* formats,
      uint32 num_color_attachments);
  VulkanPipelineBuilder& depthAttachmentFormat(VkFormat format);
  VulkanPipelineBuilder& stencilAttachmentFormat(VkFormat format);
  VulkanPipelineBuilder& patchControlPoints(uint32 num_points);

  VkResult build(VkDevice device, VkPipelineCache pipeline_cache,
                 VkPipelineLayout pipeline_layout, VkPipeline* out_pipeline,
                 const char* debug_name = nullptr) noexcept;

  static uint32 getNumPipelineCreated() { return num_pipelines_created_; }

 private:
  VkPipelineCreateFlags flags_{0};

  enum { MAX_DYNAMIC_STATES = 128 };
  uint32 num_dynamic_states_{0u};
  VkDynamicState dynamic_states_[MAX_DYNAMIC_STATES]{};

  uint32 num_shader_stages_{0u};
  VkPipelineShaderStageCreateInfo shader_stages_[NUM_GRAPHICS_SHADER_STAGES]{};

  VkPipelineVertexInputStateCreateInfo vertex_input_state_;
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_;
  VkPipelineRasterizationStateCreateInfo rasterization_state_;
  VkPipelineMultisampleStateCreateInfo multisample_state_;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_;
  VkPipelineTessellationStateCreateInfo tessellation_state_;

  uint32 view_mask_ = 0;
  uint32 num_color_attachments_ = 0;
  VkPipelineColorBlendAttachmentState
      color_blend_attachment_states_[MAX_COLOR_ATTACHMENTS] = {};

  VkFormat color_attachment_formats_[MAX_COLOR_ATTACHMENTS] = {};
  VkFormat depth_attachment_format_ = VK_FORMAT_UNDEFINED;
  VkFormat stencil_attachment_format_ = VK_FORMAT_UNDEFINED;

  static uint32 num_pipelines_created_;
};

struct ComputePipelineState final {
  ComputePipelineDesc desc;

  VkDescriptorSetLayout lastVkDescriptorSetLayout{VK_NULL_HANDLE};
  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
  VkPipeline pipeline{VK_NULL_HANDLE};

  void* specConstantDataStorage{nullptr};
};

struct RayTracingPipelineState final {
  std::vector<ShaderModuleHandle> smRayGen;
  std::vector<ShaderModuleHandle> smMiss;
  std::vector<ShaderModuleHandle> smCallable;
  std::vector<RayTracingHitGroupDesc> hitGroups;
  SpecializationConstantDesc specConstantDesc{};

  const char* debugName{""};

  VkDescriptorSetLayout lastVkDescriptorSetLayout{VK_NULL_HANDLE};

  VkShaderStageFlags shaderStageFlags{0};
  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
  VkPipeline pipeline{VK_NULL_HANDLE};

  void* specConstantDataStorage{nullptr};

  Holder<BufferHandle> sbt;
  VkStridedDeviceAddressRegionKHR sbtEntryRayGen{};
  VkStridedDeviceAddressRegionKHR sbtEntryMiss{};
  VkStridedDeviceAddressRegionKHR sbtEntryHit{};
  VkStridedDeviceAddressRegionKHR sbtEntryCallable{};
};

struct AccelerationStructure {
  bool isTLAS{false};
  VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {};
  VkAccelerationStructureKHR vkHandle{VK_NULL_HANDLE};
  uint64 deviceAddress{0u};
  Holder<BufferHandle> buffer;
  Holder<BufferHandle> scratchBuffer;  // Store only for TLAS
};

struct DeviceQueues final {
  static constexpr uint32 INVALID = 0xffffffff;

  uint32 graphicsQueueFamilyIndex{INVALID};
  uint32 computeQueueFamilyIndex{INVALID};

  VkQueue graphicsQueue{VK_NULL_HANDLE};
  VkQueue computeQueue{VK_NULL_HANDLE};

  // graphics/compute queue also can copy and upload datas. the benifits of using
  // transfer queue is parallel copy/upload data, when the benifits lower than the
  // barrier and ownship transition, using a standalone transfer queue is not a better
  // choice.
  //
  // uint32 transferQueueFamilyIndex{INVALID};
  // VkQueue transferQueue{VK_NULL_HANDLE};
};

struct ContextConfig {
  //< default version is 1.4.
  VulkanVersion vulkanVersion = VkVersion_1_4;

  bool bTerminateOnValidationEror{false};
  //< enable validation by default.
  bool bEnableValidation{true};
  bool bEnableValidationGpuAV = false;
  bool bGenerateSPIRVDebugInfo = true;

  // VK_KHR_fragment_shading_rate is mutually exclusive with
  // VK_EXT_fragment_density_map. When the device supports fragment shading rate,
  // requesting it here disables fragment density map (otherwise fragment density map is
  // enabled as usual) VUID-VkDeviceCreateInfo-fragmentDensityMap-04481/04482/04483
  bool bEnableFragmentShadingRate = false;

  // add extra instance extensions on top of required ones
  const char* extensionsInstance[MAX_CUSOTM_EXTENSIONS] = {};
  // add extra device extensions on top of required ones
  const char* extensionsDevice[MAX_CUSOTM_EXTENSIONS] = {};
  void* extensionsDeviceFeatures =
      nullptr;  // inserted into VkPhysicalDeviceVulkan11Features::pNext

  EColorSpace swapchainRequestedColorSpace = EColorSpace::SRGB_NONLINEAR;

  //< TODO: pipeline cache, do it later.
  const void* pipelineCacheData{nullptr};
  size_t pipelineCacheDataSize{0};

  EPresentMode presentModes[MAX_PRESENT_MODES] = {
      EPresentMode::Mailbox,
      EPresentMode::Immediate,
      EPresentMode::FIFO_Relaxed,
      EPresentMode::FIFO,
  };

  //< Staging buffer size 128mb
  uint64 maxStagingBufferSize = 128ull * 1024ull * 1024ull;
};

struct VulkanBuffer final {
  [[nodiscard]] FORCE_INLINE uint8_t* getMappedPtr() const {
    return static_cast<uint8_t*>(mapped_ptr_);
  }
  [[nodiscard]] FORCE_INLINE bool isMapped() const { return mapped_ptr_ != nullptr; }

  void bufferSubData(const Context& ctx, size_t offset, size_t size, const void* data);
  void getBufferSubData(const Context& ctx, size_t offset, size_t size, void* data);
  void flushMappedMemory(const Context& ctx, VkDeviceSize offset,
                         VkDeviceSize size) const;
  void invalidateMappedMemory(const Context& ctx, VkDeviceSize offset,
                              VkDeviceSize size) const;

  FORCE_INLINE VkBuffer getVkBuffer() const { return vk_handle_; }

 private:
  VkBuffer vk_handle_{VK_NULL_HANDLE};
  VkDeviceMemory device_memory_{VK_NULL_HANDLE};
  VmaAllocation vma_allocation_{VK_NULL_HANDLE};
  VkDeviceAddress device_address_{0};
  VkDeviceSize size_{0};
  VkBufferUsageFlags usage_flags_{0};
  VkMemoryPropertyFlags mem_prop_flags_{0};
  void* mapped_ptr_{nullptr};
  // `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT`
  bool is_coherent_memory_{false};

 private:
  friend struct Context;
  friend struct StagingDevice;
  friend struct CommandBuffer;
};

struct VulkanImage final {
  [[nodiscard]] FORCE_INLINE bool isSampledImage() const {
    return (usage_flags_ & VK_IMAGE_USAGE_SAMPLED_BIT) > 0;
  }

  [[nodiscard]] FORCE_INLINE bool isStorageImage() const {
    return (usage_flags_ & VK_IMAGE_USAGE_STORAGE_BIT) > 0;
  }

  [[nodiscard]] FORCE_INLINE bool isColorAttachment() const {
    return (usage_flags_ & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) > 0;
  }

  [[nodiscard]] FORCE_INLINE bool isDepthAttachment() const {
    return (usage_flags_ & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) > 0;
  }

  [[nodiscard]] FORCE_INLINE bool isAttachment() const {
    return (usage_flags_ & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) > 0;
  }

  [[nodiscard]] VkImageView createImageView(
      VkDevice device, VkImageViewType image_view_type, VkFormat format,
      VkImageAspectFlags aspect_flags, uint32 base_level,
      uint32 num_levels = VK_REMAINING_MIP_LEVELS, uint32 base_layer = 0,
      uint32 num_layers = 1,
      const VkComponentMapping component_mapping =
          {
              .r = VK_COMPONENT_SWIZZLE_IDENTITY,
              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
              .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      const VkSamplerYcbcrConversion* ycbr_conversion = nullptr,
      const char* debug_name = nullptr);

  void generateMipmap(VkCommandBuffer vk_cmd_buffer) const;
  void transitionLayout(VkCommandBuffer vk_cmd_buffer, VkImageLayout new_image_layout,
                        const VkImageSubresourceRange& subresource_range,
                        StageAccess extra_dst_stage = {},
                        bool compute_only_queue = false) const;

  [[nodiscard]] VkImageAspectFlags getImageAspectFlags() const;

  // framebuffers can render only into one level/layer
  [[nodiscard]] VkImageView getOrCreateVkImageViewForFramebuffer(Context& ctx,
                                                                 uint8_t level,
                                                                 uint16_t layer,
                                                                 uint32 view_mask);

  [[nodiscard]] static bool isDepthFormat(VkFormat format);
  [[nodiscard]] static bool isStencilFormat(VkFormat format);

  const char* getDebugName() { return debug_name_; }

  VkImage getVkImage() const { return vk_handle_; }

  FORCE_INLINE bool isDepthFormat() const { return is_depth_format_; }
  FORCE_INLINE bool isStencilFormat() const { return is_stencil_format_; }
  FORCE_INLINE VkFormat getFormat() const { return format_; }

 private:
  VkImage vk_handle_{VK_NULL_HANDLE};
  VkImageUsageFlags usage_flags_{0};
  // VkDeviceMemory device_memory_[3]{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
  VkDeviceMemory device_memory_;
  VmaAllocation vma_allocation_{VK_NULL_HANDLE};
  VkFormatProperties format_properties_{};
  VkExtent3D extent_{.width = 0, .height = 0, .depth = 0};
  VkImageType type_{VK_IMAGE_TYPE_MAX_ENUM};
  VkFormat format_{VK_FORMAT_UNDEFINED};
  VkSampleCountFlags sample_count_{VK_SAMPLE_COUNT_1_BIT};
  void* mapped_ptr_{nullptr};
  bool is_swapchain_image_{false};
  bool is_owning_VkImage_{true};
  bool is_resolve_attachment_{false};  // subpass related?
  uint32 num_levels_{1u};
  uint32 num_layers_{1u};
  bool is_depth_format_{false};
  bool is_stencil_format_{false};
  char debug_name_[256]{0};

  // A texture view does not own its VkImage. Tracking its parent propagates GPU use to
  // the image, while the view count enforces that views are retired before the owning
  // texture.
  TextureHandle parent_;
  uint32 live_view_count_{0u};
  bool pending_destroy_{false};

  mutable VkImageLayout layout_{VK_IMAGE_LAYOUT_UNDEFINED};

  // QFOT means Queue-Family Ownership Tranfer.
  // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv the rendezvous means,

  // current owner queue family, the value will be changed when the transition occured
  mutable uint32 owner_queue_family_{VK_QUEUE_FAMILY_IGNORED};
  // released in `submit()`, consumed by `acquiredOwnershipIfPending()`
  mutable uint32 pending_acquire_src_family_{VK_QUEUE_FAMILY_IGNORED};
  // old layout the acquire must match the release
  mutable VkImageLayout qfot_src_layout_{VK_IMAGE_LAYOUT_UNDEFINED};
  // new layout(rendezvous layout) the acquire must match release
  mutable VkImageLayout qfot_dst_layout_{VK_IMAGE_LAYOUT_UNDEFINED};
  // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  // precached image views - owned by this VulkanImage
  // default view with all mip-levels
  VkImageView image_view_{VK_NULL_HANDLE};
  // default view with identity swizzle wih all mip-levels
  VkImageView image_view_storage_{VK_NULL_HANDLE};
  // max 6 faces for cubemap rendering
  VkImageView image_view_for_framebuffer_[MAX_MIP_LEVELS][6]{};
  //
  VkImageView image_view_for_framebuffer_multiview_[MAX_MIP_LEVELS]{};

 private:
  friend struct Context;
  friend struct VulkanSwapchain;
  friend struct CommandBuffer;
  friend struct StagingDevice;
};

struct VulkanSwapchain final {
  enum { MAX_SWAPCHAIN_IMAGES = 16 };

  VulkanSwapchain(Context& ctx, uint32 width, uint32 height);
  ~VulkanSwapchain();

  Result present(VkSemaphore wait_semaphore);
  VkImage getCurrentVkImage() const;
  VkImageView getCurrentVkImageView() const;

  // get next renderable texture from swapchain,
  TextureHandle getCurrentTexture();

  const VkSurfaceFormatKHR& getSurfaceFormat() const;
  uint32 getSwapchainCurrentImageIndex() const;
  uint32 getNumSwapchainImages() const;
  // runtime present mode switching without swapchain recreation
  // (VK_KHR_swapchain_maintenance1), returns `false` if the mode cannot be set
  [[nodiscard]] bool setCurrentPresentMode(VkPresentModeKHR mode);

 private:
  friend struct Context;

  Context& ctx_;
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue graphics_queue_{VK_NULL_HANDLE};
  uint32 width_{0u};
  uint32 height_{0u};
  uint32 num_swapchain_images_{0u};
  uint32 current_image_index_{0u};
  uint32 current_frame_index_{0u};

  // the flag will set to false after acquiring image, set to true after presenting.
  bool get_next_image_{true};

  VkSwapchainKHR vk_handle_{VK_NULL_HANDLE};
  VkSurfaceFormatKHR surface_format_{.format = VK_FORMAT_UNDEFINED};
  VkPresentModeKHR current_present_mode_{VK_PRESENT_MODE_FIFO_KHR};
  VkPresentModeKHR registered_present_modes_[MAX_PRESENT_MODES]{};
  uint32 num_registered_present_modes_{0u};
  const VkSwapchainPresentModeInfoKHR present_mode_info_{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_KHR,
      .swapchainCount = 1,
      .pPresentModes = &current_present_mode_,
  };
  VkSwapchainPresentFenceInfoKHR present_fence_info_{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
      .swapchainCount = 1,
      .pFences = nullptr,
  };
  TextureHandle swapchain_texture_handles_[MAX_SWAPCHAIN_IMAGES]{};
  // image available semaphores, set by `vkAcquireNextImageKHR`
  VkSemaphore acquire_semaphores_[MAX_SWAPCHAIN_IMAGES]{};
  VkFence present_fences_[MAX_SWAPCHAIN_IMAGES]{};
  VkFence acquire_fences_[MAX_SWAPCHAIN_IMAGES]{};
  uint64 timeline_wait_values_[MAX_SWAPCHAIN_IMAGES]{};
};

struct VulkanImmediateCommands final {
  static constexpr uint32 MAX_COMMAND_BUFFERS = 64;

  VulkanImmediateCommands(VkDevice device, uint32 queue_family_index,
                          bool has_EXT_device_fault, const char* debug_name);
  ~VulkanImmediateCommands();

  NO_COPYABLE(VulkanImmediateCommands);

  struct CommandBufferWrapper {
    //?
    VkCommandBuffer cmdBuf{VK_NULL_HANDLE};
    //?
    VkCommandBuffer cmdBufAllocated{VK_NULL_HANDLE};

    SubmitHandle handle;

    VkFence fence{VK_NULL_HANDLE};

    VkSemaphore semaphore{VK_NULL_HANDLE};

    // value signaled on submitTimelineSemaphore by this submission (cross-queue waits)
    uint64 signaledTimelineValue{0u};

    // is command buffer recording
    bool bRecording{false};
  };

  CommandBufferWrapper& acquire();
  // ending of a recording command buffer
  SubmitHandle submit(CommandBufferWrapper& wrapper);
  void waitSemaphore(VkSemaphore semaphore);
  void waitTimelineSemaphore(VkSemaphore semaphore, uint64 value);
  void signalSemaphore(VkSemaphore semaphore, uint64 signal_value);
  VkSemaphore acquireLastSubmitSemaphore();
  VkSemaphore getTimelineSemaphore() const { return submit_timeline_semaphore_; }

  uint64 getTimelineValue(SubmitHandle submit_handle) const;
  void setLastPresentSemaphore(VkSemaphore semaphore, VkFence fence);
  VkFence getVkFence(SubmitHandle submit_handle) const;
  SubmitHandle getLastSubmitHandle() const;
  SubmitHandle getNextSubmitHandle() const;
  bool isSubmitted(SubmitHandle submit_handle) const;
  bool isReady(SubmitHandle submit_handle, bool fast_check_no_vulkan = false) const;
  void wait(SubmitHandle submit_handle);
  void waitAll();

 private:
  // reste command buffers and related fences, prepare a valid command buffer for next
  // using.
  void purge();

  friend struct Context;

 private:
  VkDevice device_{VK_NULL_HANDLE};
  VkQueue queue_{VK_NULL_HANDLE};
  VkCommandPool command_pool_{VK_NULL_HANDLE};
  uint32 queue_family_index_{0u};
  bool has_EXT_device_fault_{false};
  const char* debug_name_{""};
  CommandBufferWrapper buffers_[MAX_COMMAND_BUFFERS]{};
  SubmitHandle last_submit_handle_;
  SubmitHandle next_submit_handle_;

  VkSemaphoreSubmitInfo last_submit_semaphore_{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
  };
  // NOTE: this wait semaphore is passed from VulkanSwapchain, the semaphore set by
  // `vkAcquireNextImageKHR`
  VkSemaphoreSubmitInfo wait_semaphore_{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
  };
  // timeline wait, cross-queue, TODO: usage???
  VkSemaphoreSubmitInfo wait_timeline_{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
  };
  // extra "signal" semaphore
  VkSemaphoreSubmitInfo signal_semaphore_{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .stageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
  };

  // present-wait semaphore of the last vkQueuePresentKHR()
  VkSemaphore last_present_semaphore_{VK_NULL_HANDLE};
  // its present fence; acquire() waits it before reusing that slot
  VkFence last_present_fence_{VK_NULL_HANDLE};
  // monotonic timeline signaled by every submit() (cross-queue waits)
  VkSemaphore submit_timeline_semaphore_{VK_NULL_HANDLE};
  // record the number of available command buffers
  uint32 num_available_command_buffers_{MAX_COMMAND_BUFFERS};
  uint32 submit_counter_{1u};
};

// If the hardware is not UMA, the memory should use staging device upload to device.
struct StagingDevice final {
  explicit StagingDevice(Context& cxt);
  ~StagingDevice() = default;

  NO_COPYABLE(StagingDevice);

  // Copy buffer data via staging device
  void bufferSubData(VulkanBuffer& buffer, size_t offset, size_t size, const void* data);

  // Copy image data
  void imageData2D(VulkanImage& image, const VkRect2D& region, uint32 base_mip_level,
                   uint32 num_mip_levels, uint32 base_layer, uint32 num_layers, VkFormat,
                   const void* data, uint32 buffer_row_length);

  // Copy image3d data
  void imageData3D(VulkanImage& image, const VkOffset3D& offset, const VkExtent3D& extent,
                   VkFormat format, const void* data);

  void getImageData(VulkanImage& image, const VkOffset3D& offset,
                    const VkExtent3D& extent, VkImageSubresourceRange range,
                    VkFormat format, void* out_data);

 private:
  enum { kStagingBufferAlignment = 16 };

  struct MemoryRegionDesc {
    uint64 offset{0u};
    uint64 size{0u};
    SubmitHandle handle;
  };

  MemoryRegionDesc getNextFreeOffset(VkDeviceSize size);
  void ensureStagingBufferSize(VkDeviceSize size_needed);
  // the insert action will keep the `MemoryRegionDesc` ordered by offset.
  void insertRegion(const MemoryRegionDesc& region);
  void waitAndReset();

 private:
  Context& ctx_;
  Holder<BufferHandle> handle_;
  VkDeviceSize staging_buffer_size_{0u};
  uint32 staging_buffer_count_{0u};
  // the staging buffer grows from min_buffer_size_ up to max_buffer_size_ as needed
  VkDeviceSize max_buffer_size_{0u};
  // ad-hoc value to avoid frequent reallocations
  VkDeviceSize min_buffer_size_{4u * 2048u * 2048u};

  std::vector<MemoryRegionDesc> regions_;
};

struct CommandBuffer final : ICommandBuffer {
  void cmdTransitionToGeneral(const Span<TextureHandle>& textures,
                              EShaderStage extra_dst_stage) const override;
  // Convert the textures to shader readonly.
  void cmdTransitionToShaderReadOnly(const Span<TextureHandle>& textures,
                                     EShaderStage extra_dst_stage) const override;
  void cmdTransitionToRenderingLocalRead(
      const Span<TextureHandle>& textures) const override;
  void cmdPushDebugGroupLabel(const char* label, uint32 colorRGBA) const override;
  void cmdInsertDebugEventLabel(const char* label, uint32 colorRGBA) const override;
  void cmdPopDebugGroupLabel() const override;
  void cmdBindRayTracingPipeline(RayTracingPipelineHandle handle) override;
  void cmdBindComputePipeline(ComputePipelineHandle handle) override;
  void cmdDispatch(const Dimensions& groupCount, const Dependencies& deps) override;
  void cmdDispatchIndirect(BufferHandle indirect_buffer, size_t indirect_buffer_offset,
                           const Dependencies& deps) override;
  void cmdBeginRendering(const RenderPass& renderPass, const Framebuffer& frame_buffer,
                         const Dependencies& deps = {}) override;
  void cmdEndRendering() override;
  void cmdNextSubpass() override;
  void cmdBindViewport(const Viewport& viewport) override;
  void cmdBindScissorRect(const ScissorRect& rect) override;
  void cmdBindRenderPipeline(RenderPipelineHandle handle) override;
  void cmdBindDepthState(const DepthState& state) override;
  void cmdBindVertexBuffer(uint32 index, BufferHandle buffer, uint64 offset,
                           uint64 size) override;
  void cmdBindIndexBuffer(BufferHandle index_buffer, EIndexFormat index_format,
                          uint64 offset, uint64 size) override;
  void cmdPushConstants(const void* data, size_t size, size_t offset) override;
  // TODO: most open-source engine leave the buffer barrier out of copy function, but
  // this function do it inner, may be removed later.
  void cmdCopyBuffer(BufferHandle src, BufferHandle dst, size_t src_offset,
                     size_t dst_offset, size_t size) override;
  void cmdFillBuffer(BufferHandle buffer, size_t offset, size_t size,
                     uint32 data) override;
  void cmdUpdateBuffer(BufferHandle buffer, size_t offset, size_t size,
                       const void* data) override;
  void cmdDraw(uint32 vertex_count, uint32 instance_count = 1, uint32 first_vertex = 0,
               uint32 base_instance = 0) override;
  void cmdDrawIndexed(uint32 index_count, uint32 instance_count, uint32 first_index,
                      int32_t vertex_offset, uint32 base_instance) override;
  void cmdDrawIndirect(BufferHandle indirect_buffer, size_t indirect_buffer_offset,
                       uint32 draw_count, uint32 stride) override;
  void cmdDrawIndexedIndirect(BufferHandle indirect_buffer, size_t indirect_buffer_offset,
                              uint32 draw_count, uint32 stride) override;
  void cmdDrawIndexedIndirectCount(BufferHandle indirect_buffer,
                                   size_t indirect_buffer_offset,
                                   BufferHandle count_buffer, size_t count_buffer_Offset,
                                   uint32 max_draw_count, uint32 stride) override;
  void cmdDrawMeshTasks(const Dimensions& threadgroupCount) override;
  void cmdDrawMeshTasksIndirect(BufferHandle indirect_buffer,
                                size_t indirect_buffer_offset, uint32 draw_count,
                                uint32 stride) override;
  void cmdDrawMeshTasksIndirectCount(BufferHandle indirect_buffer,
                                     size_t indirectBufferOffset,
                                     BufferHandle countBuffer, size_t countBufferOffset,
                                     uint32 maxDrawCount, uint32 stride) override;
  void cmdTraceRays(uint32 width, uint32 height, uint32 depth,
                    const Dependencies& deps) override;
  void cmdSetBlendColor(const float color[4]) override;
  void cmdSetDepthBias(float constantFactor, float slopeFactor, float clamp) override;
  void cmdSetDepthBiasEnable(bool enable) override;
  void cmdSetFragmentShadingRate(
      const Dimensions& fragment_size,
      EShadingRateCombinerOp primitive_op = EShadingRateCombinerOp::Keep,
      EShadingRateCombinerOp attachment_op = EShadingRateCombinerOp::Keep) override;
  void cmdResetQueryPool(QueryPoolHandle pool, uint32 firstQuery,
                         uint32 queryCount) override;
  void cmdWriteTimestamp(QueryPoolHandle pool, uint32 query) override;
  void cmdClearColorImage(TextureHandle tex, const ClearColorValue& value,
                          const TextureLayers& layers) override;
  void cmdCopyImage(TextureHandle src, TextureHandle dst, const Dimensions& extent,
                    const Offset3D& srcOffset, const Offset3D& dstOffset,
                    const TextureLayers& srcLayers,
                    const TextureLayers& dstLayers) override;
  void cmdGenerateMipmap(TextureHandle handle) override;
  void cmdUpdateTLAS(AccelerationStructHandle handle,
                     BufferHandle instancesBuffer) override;

  explicit operator VkCommandBuffer() { return getVkCommandBuffer(); }
  VkCommandBuffer getVkCommandBuffer() const {
    return wrapper_ ? wrapper_->cmdBuf : VK_NULL_HANDLE;
  }

 public:
  CommandBuffer() = default;
  CommandBuffer(Context* ctx, VulkanImmediateCommands& immediate,
                uint32 queue_family_index);
  ~CommandBuffer() override;

  CommandBuffer& operator=(CommandBuffer&& other) = default;

 private:
  void bufferBarrier(BufferHandle handle, VkPipelineStageFlags2 src_stage,
                     VkPipelineStageFlags2 dst_stage, VkDeviceSize offset = 0,
                     VkDeviceSize size = VK_WHOLE_SIZE);

  void addCrossQueueDependencies(const Dependencies& deps);

  void trackResource(BufferHandle handle) const;
  void trackResource(TextureHandle handle) const;
  void trackResource(SamplerHandle handle) const;
  void trackResource(QueryPoolHandle handle) const;
  void trackResource(ComputePipelineHandle handle) const;
  void trackResource(RenderPipelineHandle handle) const;
  void trackResource(RayTracingPipelineHandle handle) const;
  void trackResource(AccelerationStructHandle handle) const;
  void trackResources(const Dependencies& deps) const;
  void trackResources(const Framebuffer& frame_buffer) const;

  bool acquireOwnershipIfPending(VulkanImage& image, StageAccess dst) const;

  bool isComputeOnlyQueue() const;

 private:
  friend struct Context;
  friend struct VulkanImage;

  Context* ctx_ = nullptr;
  VulkanImmediateCommands::CommandBufferWrapper* wrapper_{nullptr};
  VulkanImmediateCommands* immediate_{nullptr};

  // highest async-compute timeline value a graphics CB depends on (from
  // Dependencies::waitCompute); wait cross-queue at `submit()`
  uint64 cross_queue_compute_wait_value_{0u};
  // highest graphics timeline value an async-compute CB depends on (from
  // Dependencies::waitGraphics); wait cross-queue at `submit()`
  uint64 cross_queue_graphics_wait_value_{0u};
  uint32 queue_family_index_{0u};

  Framebuffer frame_buffer_;
  SubmitHandle last_submit_handle_;

  struct {
    VkDescriptorImageInfo imageInfos[MAX_COLOR_ATTACHMENTS];
    VkWriteDescriptorSet writeDstSet[MAX_COLOR_ATTACHMENTS];
    uint32 count{0u};
  } input_attachments_;

  VkPipeline last_pipeline_bound_{VK_NULL_HANDLE};

  bool is_rendering_{false};
  uint32 view_mask_{0u};

  RenderPipelineHandle current_graphics_pipeline_{nullptr};
  ComputePipelineHandle current_compute_pipeline_{nullptr};
  RayTracingPipelineHandle current_ray_tracing_pipeline_{nullptr};
};

// Vulkan Context
struct Context final : IContext {
  Context(const ContextConfig& config, void* window, void* display = nullptr,
          VkSurfaceKHR surface = VK_NULL_HANDLE);
  ~Context() override;

  // The implementation of abstract parent class.
  // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

  ICommandBuffer& acquireCommandBuffer(bool dedicated_compute = false) override;
  SubmitHandle submit(ICommandBuffer& commandBuffer, TextureHandle present,
                      const Span<TextureHandle>& release = {}) override;
  void wait(SubmitHandle handle) override;
  [[nodiscard]] bool isSubmitComplete(SubmitHandle handle) const override;
  [[nodiscard]] Holder<BufferHandle> createBuffer(const BufferDesc& desc,
                                                  const char* debug_name,
                                                  Result* result) override;
  [[nodiscard]] Holder<SamplerHandle> createSampler(const SamplerStateDesc& desc,
                                                    Result* result) override;
  [[nodiscard]] Holder<TextureHandle> createTexture(const TextureDesc& desc,
                                                    const char* debug_name,
                                                    Result* result) override;
  [[nodiscard]] Holder<TextureHandle> createTextureView(TextureHandle texture,
                                                        const TextureViewDesc& desc,
                                                        const char* debug_name,
                                                        Result* result) override;
  [[nodiscard]] TextureDescriptorIndex getTextureDescriptorIndex(
      TextureHandle handle) const override;
  [[nodiscard]] SamplerDescriptorIndex getSamplerDescriptorIndex(
      SamplerHandle handle) const override;
  [[nodiscard]] Holder<ComputePipelineHandle> createComputePipeline(
      const ComputePipelineDesc& desc, Result* result) override;
  [[nodiscard]] Holder<RenderPipelineHandle> createRenderPipeline(
      const RenderPipelineDesc& desc, Result* result) override;
  [[nodiscard]] Holder<RayTracingPipelineHandle> createRayTracingPipeline(
      const RayTracingPipelineDesc& desc, Result* result) override;
  [[nodiscard]] Holder<ShaderModuleHandle> createShaderModule(
      const ShaderModuleDesc& desc, Result* result = nullptr) override;
  [[nodiscard]] Holder<QueryPoolHandle> createQueryPool(uint32 num_queries,
                                                        const char* debug_name,
                                                        Result* result) override;

  [[nodiscard]] Holder<AccelerationStructHandle> createAccelerationStructure(
      const AccelStructDesc& desc, Result* outResult) override;

  void destroy(ComputePipelineHandle handle) override;
  void destroy(RenderPipelineHandle handle) override;
  void destroy(RayTracingPipelineHandle) override;
  void destroy(ShaderModuleHandle handle) override;
  void destroy(SamplerHandle handle) override;
  void destroy(BufferHandle handle) override;
  void destroy(TextureHandle handle) override;
  void destroy(QueryPoolHandle handle) override;
  void destroy(AccelerationStructHandle handle) override;
  void destroy(Framebuffer& fb) override;

  [[nodiscard]] uint64 gpuAddress(AccelerationStructHandle handle) const override;

  [[nodiscard]] AccelStructSizes getAccelerationStructSizes(
      const AccelStructDesc& desc, Result* result) const override;

  Result upload(BufferHandle handle, const void* data, size_t size,
                size_t offset) override;
  Result download(BufferHandle handle, void* data, size_t size, size_t offset) override;
  [[nodiscard]] uint8* getMappedPtr(BufferHandle handle) const override;
  [[nodiscard]] uint64 gpuAddress(BufferHandle handle, size_t offset) const override;
  void flushMappedMemory(BufferHandle handle, size_t offset, size_t size) const override;
  [[nodiscard]] uint32 getMaxStorageBufferRange() const override;
  Result upload(TextureHandle handle, const TextureRangeDesc& range, const void* data,
                uint32 buffer_row_length) override;
  Result download(TextureHandle handle, const TextureRangeDesc& range,
                  void* out_data) override;
  [[nodiscard]] Dimensions getDimensions(TextureHandle handle) const override;
  [[nodiscard]] float getAspectRatio(TextureHandle handle) const override;
  [[nodiscard]] EFormat getFormat(TextureHandle handle) const override;
  TextureHandle getCurrentSwapchainTexture() override;
  EFormat getSwapchainFormat() const override;
  EColorSpace getSwapchainColorSpace() const override;
  uint32 getSwapchainCurrentImageIndex() const override;
  uint32 getNumSwapchainImages() const override;
  void recreateSwapchain(int newWidth, int newHeight) override;
  [[nodiscard]] bool setCurrentPresentMode(EPresentMode mode) override;
  [[nodiscard]] EPresentMode getCurrentPresentMode() const override;
  uint32 getFramebufferMSAABitMask() const override;
  [[nodiscard]] Dimensions getShadingRateAttachmentMinTexelSize() const override;
  [[nodiscard]] Dimensions getShadingRateAttachmentMaxTexelSize() const override;
  bool isExtensionEnabled(const char* ext) const override;
  bool supportsAsyncCompute() const override;
  double getTimestampPeriodToMs() const override;
  bool getQueryPoolResults(QueryPoolHandle pool, uint32 firstQuery, uint32 queryCount,
                           size_t dataSize, void* outData, size_t stride) const override;

  // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

  VkPipeline getVkPipeline(ComputePipelineHandle handle);
  VkPipeline getVkPipeline(RenderPipelineHandle handle, uint32 view_mask);
  VkPipeline getVkPipeline(RayTracingPipelineHandle handle);

  //< @return the physical device detected.
  uint32 queryDevices(HWDeviceDesc* outDevices, uint32 max_out_devices = 1);

  Result initContext(const HWDeviceDesc& desc);
  Result initSwapchain(uint32 width, uint32 height);

  BufferHandle createBuffer(VkDeviceSize buffer_size, VkBufferUsageFlags usage_flags,
                            VkMemoryPropertyFlags memory_property_flags,
                            Result* out_result, const char* debug_name = nullptr);
  SamplerHandle createSampler(const VkSamplerCreateInfo& ci, Result* out_result,
                              // EFormat yuv_foramt = EFormat::NONE,
                              const char* debug_name = nullptr);
  AccelerationStructHandle createBLAS(const AccelStructDesc& desc, Result* result);
  AccelerationStructHandle createTLAS(const AccelStructDesc& desc, Result* result);

  bool hasSwapchain() const noexcept { return swapchain_ != nullptr; }

  const VkPhysicalDeviceProperties& getVkPhysicalDeviceProperties() const {
    return physical_device_properties2_.properties;
  }

  // OpenXR needs Vulkan instance to find physical device
  VkInstance getVkInstance() const { return instance_; }
  VkDevice getVkDevice() const { return device_; }
  VkPhysicalDevice getVkPhysicalDevice() const { return physical_device_; }
  std::vector<uint8> getPipelineCacheData() const;

  // Execute a task after all submissions in its retirement point have completed.
  void deferredTask(std::packaged_task<void()>&& task) const;
  void deferredTask(std::packaged_task<void()>&& task, RetirementPoint retirement) const;

  void* getVmaAllocator() const;

  void checkAndUpdateDescriptorSets();
  void bindDefaultDescriptorSets(VkCommandBuffer cmdBuf, VkPipelineBindPoint bindPoint,
                                 VkPipelineLayout layout, SubmitHandle submit,
                                 bool compute);

  const ContextConfig& getConfig() const { return config_; }

  VkBuffer getVkBuffer(BufferHandle handle) const;
  VkImage getVkImage(TextureHandle handle) const;
  VkImageView getVkImageView(TextureHandle handle) const;
  VkDeviceAddress getVkAccelerationStructureDeviceAddress(
      AccelerationStructHandle handle) const;
  VkAccelerationStructureKHR getVkAccelerationStructure(
      AccelerationStructHandle handle) const;
  VkBuffer getVkBuffer(AccelerationStructHandle handle) const;
  VkPipeline getVkPipeline(RayTracingPipelineHandle handle) const;
  VkPipelineLayout getVkPipelineLayout(RayTracingPipelineHandle handle) const;
  VkDeviceSize getBufferSize(BufferHandle handle) const;

 private:
  // The structure record the textures/samplers/accel structures,
  struct DescriptorSet {
    uint32 maxTextures{0u};
    uint32 maxSamplers{0u};
    uint32 maxAccelStructures{0u};
    VkDescriptorSetLayout dstSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool dstPool{VK_NULL_HANDLE};
    VkDescriptorSet dstSet{VK_NULL_HANDLE};

    RetirementPoint retirement;
  };

  Result createInstance();
  void createSurface(void* window, void* display);
  void createHeadlessSurface();
  void querySurfaceCapabilities();
  [[nodiscard]] RetirementPoint contextRetirementPoint() const;
  [[nodiscard]] bool isRetirementComplete(const RetirementPoint& retirement) const;
  [[nodiscard]] bool waitRetirement(const RetirementPoint& retirement);
  void retireTexture(TextureHandle handle, RetirementPoint retirement);
  void processDeferredTasks() const;
  void waitDeferredTasks(bool device_idle = false);
  void generateMipmap(TextureHandle handle);
  Result growDescriptorPool(DescriptorSet& dset, uint32 max_textures, uint32 max_samplers,
                            uint32 max_accel_structs);

  ShaderModuleState createShaderModuleFromSPIRV(const void* spirv, size_t num_bytes,
                                                const char* debug_name,
                                                Result* result) const;

  // TODO
  const VkSamplerYcbcrConversionInfo* getOrCreateYcbcrConversionInfo(EFormat format);
  // TODO
  VkSampler getOrCreateYcbcrSampler(EFormat format);

  void getBuildInfoBLAS(const AccelStructDesc& desc,
                        VkAccelerationStructureGeometryKHR& out_geom,
                        VkAccelerationStructureBuildSizesInfoKHR& out_info) const;
  void getBuildInfoTLAS(const AccelStructDesc& desc,
                        VkAccelerationStructureGeometryKHR& out_geom,
                        VkAccelerationStructureBuildSizesInfoKHR& out_info) const;

 private:
  friend struct VulkanSwapchain;
  friend struct StagingDevice;
  friend struct CommandBuffer;

  VkInstance instance_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkDevice device_{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT debug_utils_messenger_{VK_NULL_HANDLE};
  VkDebugReportCallbackEXT debug_report_callback_{VK_NULL_HANDLE};
  uint32 khronos_validation_version_{0};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};

  VkPhysicalDeviceVulkan14Features physical_device_features14_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
  VkPhysicalDeviceVulkan13Features physical_device_features13_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  VkPhysicalDeviceVulkan12Features physical_device_features12_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext = &physical_device_features13_};
  VkPhysicalDeviceVulkan11Features physical_device_features11_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext = &physical_device_features12_};
  VkPhysicalDeviceFeatures2 physical_device_features10_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext = &physical_device_features11_,
  };

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR ray_tracing_pipeline_properties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

  VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties_ =
      {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};

  VkPhysicalDeviceDriverProperties physical_device_driver_properties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES, nullptr};

  VkPhysicalDeviceMaintenance6Properties maintenance6_properties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES, nullptr};

  VkPhysicalDeviceFragmentDensityMapPropertiesEXT fragment_density_map_properties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_PROPERTIES_EXT};

  VkPhysicalDeviceMeshShaderPropertiesEXT mesh_shader_properties_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
  // queried (not chained by default) - only added to vkFeatures10_ when
  // VK_EXT_mesh_shader is supported
  VkPhysicalDeviceMeshShaderFeaturesEXT mesh_shader_features_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
  // queried (not chained by default) - only added to vkFeatures10_ when
  // VK_EXT_fragment_density_map is supported
  VkPhysicalDeviceFragmentDensityMapFeaturesEXT fragment_density_map_features_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_DENSITY_MAP_FEATURES_EXT};

  // provided by Vulkan 1.4
  VkPhysicalDeviceVulkan14Properties physical_device_props14_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_PROPERTIES,
      &physical_device_driver_properties_,
  };
  // provided by Vulkan 1.3
  VkPhysicalDeviceVulkan13Properties physical_device_props13_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES,
      &physical_device_driver_properties_,
  };
  // provided by Vulkan 1.2
  VkPhysicalDeviceVulkan12Properties physical_device_props12_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES,
      &physical_device_props13_,
  };
  // provided by Vulkan 1.1
  VkPhysicalDeviceVulkan11Properties physical_device_props11_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES,
      .pNext = &physical_device_props12_,
  };
  VkPhysicalDeviceProperties2 physical_device_properties2_ = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      &physical_device_props11_,
      VkPhysicalDeviceProperties{},
  };

  VkPhysicalDeviceFragmentShadingRatePropertiesKHR fragment_shading_rate_properties_ = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_PROPERTIES_KHR};

  std::vector<VkFormat> device_depth_formats_;
  std::vector<VkSurfaceFormat2KHR> device_surface_formats_;
  // vkGetPhysicalDeviceFragmentShadingRatesKHR()
  std::vector<VkPhysicalDeviceFragmentShadingRateKHR> device_fragment_shading_rates_;
  VkSurfaceCapabilities2KHR device_surface_capabilities_ = {
      .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR};
  std::vector<VkPresentModeKHR> device_present_modes_;

  DeviceQueues device_queues_;
  std::unique_ptr<vk::VulkanSwapchain> swapchain_;
  VkSemaphore timeline_semaphore_{VK_NULL_HANDLE};
  std::unique_ptr<VulkanImmediateCommands> immediate_;
  // dedicated async-compute queue (optional)
  std::unique_ptr<VulkanImmediateCommands> immediate_compute_;
  std::unique_ptr<StagingDevice> staging_device_;
  VkDescriptorSetLayout input_attachments_dst_set_layout_{VK_NULL_HANDLE};
  std::vector<DescriptorSet> dst_sets_;
  size_t last_updated_dst_set_{0};

  // don't use staging on devices with shared host-visible memory
  bool require_staging_for_device_buffers_{true};

  std::unique_ptr<struct ContextImpl> pimpl_;

  VkPipelineCache pipeline_cache_{VK_NULL_HANDLE};

  // a texture or sampler changed since the last descriptor set update
  mutable bool awaiting_descriptor_update_{false};
  mutable bool awaiting_new_immutable_samplers_{false};

  ContextConfig config_;
  // Adreno GPUs do not support unbounded arrays of acceleration structures (kTLAS[]) -
  // use a fixed-size array declaration in shaders
  bool workaround_fixed_size_accel_struct_array_{false};
  // Adreno GPUs do not support arrays (of any size) of combined image samplers with
  // YCbCr immutable samplers - use one non-array sampler
  bool workaround_noYcbcr_sampler_array_{false};

  bool has_KHR_acceleration_structure_{false};
  bool has_KHR_ray_query_{false};
  bool has_KHR_ray_tracing_pipeline_{false};
  bool has_EXT_ray_tracing_invocation_reorder{false};
  // VK_KHR_index_type_uint8 or VK_EXT_index_type_uint8
  bool has_8Bit_indices_{false};
  bool has_KHR_calibrated_timestamps_{false};
  bool has_EXT_swapchain_colorspace_{false};
  // VK_KHR_swapchain_maintenance1 or VK_EXT_swapchain_maintenance1
  bool has_KHR_swapchain_maintenance1_{false};
  bool has_EXT_hdr_metadata_{false};
  bool has_EXT_device_fault_{false};
  bool has_EXT_shader_tile_image{false};
  bool has_EXT_mesh_shader_{false};
  bool has_MVK_macos_surface_{false};
  bool has_KHR_shared_presentable_image_{false};
  bool has_KHR_present_mode_fifo_latest_ready_{false};
  bool has_KHR_maintenance6_{false};  // promoted to Vulkan 1.4
  bool has_KHR_fragment_shading_rate_{false};
  bool has_EXT_fragment_density_map_{false};
  bool has_EXT_fragment_density_map2_{false};
  bool has_EXT_host_image_copy_{false};  // promoted to Vulkan 1.4
  // SHADER_READ_ONLY_OPTIMAL is a usable copy destination
  bool host_image_copy_to_shader_readonly_{false};
  // GENERAL is a usable copy destination (for images which cannot be sampled)
  bool host_image_copy_to_general_{false};
  // HOST_TRANSFER preserves memory type requirements
  bool host_image_copy_identical_memory_type_requirements_{false};
  // bitmask of device-local memory type indices
  uint32 device_local_memory_type_mask_{0u};
  std::vector<const char*> enabled_instance_extension_names_;
  std::vector<const char*> enabled_device_extension_names_;

  TextureHandle dummy_texture_;

  ResourceTracker resource_tracker_;

  Pool<ShaderModuleTag, ShaderModuleState> shader_module_pool_;
  Pool<RenderPipelineTag, RenderPipelineState> render_pipeline_pool_;
  Pool<ComputePipelineTag, ComputePipelineState> compute_pipeline_pool_;
  Pool<RayTracingPipelineTag, RayTracingPipelineState> ray_tracing_pipeline_pool_;
  Pool<SamplerTag, VkSampler> sampler_pool_;
  Pool<BufferTag, vk::VulkanBuffer> buffer_pool_;
  Pool<TextureTag, vk::VulkanImage> texture_pool_;
  Pool<QueryPoolTag, VkQueryPool> query_pool_;
  Pool<AccelerationStructTag, AccelerationStructure> acceleration_structure_pool_;
};

void emitImageQueueFamilyOwnershipTransfer(
    VkCommandBuffer cb, const VulkanImage& image, VkImageLayout old_layout,
    VkImageLayout new_layout, StageAccess src_stage_access, StageAccess dst_stage_access,
    uint32 src_queue_family, uint32 dst_queue_family);

}  // namespace vk
}  // namespace amaryl

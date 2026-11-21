#pragma once

#include <memory>
#include <string>

#include "amaryl/nrhi/config.h"
#include "amaryl/nrhi/definitions.h"
#include "amaryl/nrhi/descriptor.shared.h"

struct SDL_Window;

namespace amaryl {

struct Result {
  enum class Code {
    Ok,
    ArgumentOutOfRange,
    RuntimeError,
  };

  Code code = Code::Ok;
  const char* message = "";
  explicit Result() = default;

  explicit Result(Code code, const char* message = "") : code(code), message(message) {}

  bool isOk() const { return code == Result::Code::Ok; }

  static void setResult(Result* out_result, Code code, const char* message = "") {
    if (out_result) {
      out_result->code = code;
      out_result->message = message;
    }
  }

  static void setResult(Result* out_result, const Result& sourceResult) {
    if (out_result) {
      *out_result = sourceResult;
    }
  }
};

struct IContext;
struct ICommandBuffer;

// forward declarations to access incomplete type IContext
#define DECLARE_CTX_DESTROY_HANDLE(HandleType) \
  NRHI_API void destroy(IContext* ctx, HandleType handle)

DECLARE_CTX_DESTROY_HANDLE(ComputePipelineHandle);
DECLARE_CTX_DESTROY_HANDLE(RenderPipelineHandle);
DECLARE_CTX_DESTROY_HANDLE(RayTracingPipelineHandle);
DECLARE_CTX_DESTROY_HANDLE(ShaderModuleHandle);
DECLARE_CTX_DESTROY_HANDLE(SamplerHandle);
DECLARE_CTX_DESTROY_HANDLE(BufferHandle);
DECLARE_CTX_DESTROY_HANDLE(TextureHandle);
DECLARE_CTX_DESTROY_HANDLE(QueryPoolHandle);
DECLARE_CTX_DESTROY_HANDLE(AccelerationStructHandle);

#undef DECLARE_CTX_DESTROY_HANDLE

template <typename HandleType>
struct Holder final {
  Holder() = default;
  Holder(IContext* ctx, HandleType handle) : ctx_(ctx), handle_(handle) {}
  ~Holder() { destroy(ctx_, handle_); }

  NO_COPYABLE(Holder);

  Holder(Holder&& other) : ctx_{other.ctx_}, handle_{other.handle_} {
    other.ctx_ = nullptr;
    other.handle_ = HandleType{};
  }
  Holder& operator=(Holder&& other) {
    std::swap(ctx_, other.ctx_);
    std::swap(handle_, other.handle_);
    return *this;
  }
  Holder& operator=(std::nullptr_t) {
    this->reset();
    return *this;
  }

  // NOTE: not add explicit here, the later pool.get(handle) will has a
  // imiplicit convertion.
  inline operator HandleType() const { return handle_; }  // NOLINT

  bool valid() const { return handle_.valid(); }

  bool empty() const { return handle_.empty(); }

  void reset() {
    destroy(ctx_, handle_);

    ctx_ = nullptr;
    handle_ = HandleType{};
  }

  HandleType release() {
    ctx_ = nullptr;
    return std::exchange(handle_, HandleType{});
  }

  uint32 gen() const { return handle_.gen(); }
  uint32 index() const { return handle_.index(); }
  void* indexAsVoid() const { return handle_.indexAsVoid(); }
  void* handleAsVoid() const { return handle_.handleAsVoid(); }

 private:
  IContext* ctx_{nullptr};
  HandleType handle_{};
};

struct RHIInitDesc {
  const std::string appName{"b2Engine"};
  SDL_Window* window{nullptr};
  // uint32 appVersion{VK_MAKE_VERSION(0, 1, 0)};
  bool enableValidation{false};
};

struct Dependencies {
  Span<TextureHandle> sampledImages;
  Span<TextureHandle> storageImages;
  Span<BufferHandle> buffers;
  Span<TextureHandle> inputAttachments;
  // Cross-queue waits only: dependencies on work submitted to the *other* queue.
  // Same-queue ordering is already guaranteed automatically (by barriers within a command
  // buffer and by the intra-queue chain between submits).
  // async-compute work a graphics submit must wait for
  Span<SubmitHandle> waitCompute;
  // graphics work an async-compute submit must wait for
  Span<SubmitHandle> waitGraphics;
  Span<SamplerHandle> samplers;
  Span<AccelerationStructHandle> accelerationStructures;
};

struct ICommandBuffer {
  virtual ~ICommandBuffer() = default;

  virtual void cmdTransitionToGeneral(const Span<TextureHandle>& textures,
                                      EShaderStage extra_dst_stage) const = 0;
  virtual void cmdTransitionToShaderReadOnly(const Span<TextureHandle>& textures,
                                             EShaderStage extra_dst_stage) const = 0;
  // no extra_dst_stage parameter: this is only used within a render pass
  virtual void cmdTransitionToRenderingLocalRead(
      const Span<TextureHandle>& textures) const = 0;

  virtual void cmdPushDebugGroupLabel(const char* label, uint32 color_rgba) const = 0;
  virtual void cmdInsertDebugEventLabel(const char* label, uint32 color_rgba) const = 0;
  virtual void cmdPopDebugGroupLabel() const = 0;

  virtual void cmdBindRayTracingPipeline(RayTracingPipelineHandle handle) = 0;

  virtual void cmdBindComputePipeline(ComputePipelineHandle handle) = 0;
  virtual void cmdDispatch(const Dimensions& group_count, const Dependencies& deps) = 0;
  virtual void cmdDispatchIndirect(BufferHandle indirect_buffer,
                                   size_t indirect_buffer_offset,
                                   const Dependencies& deps) = 0;

  virtual void cmdBeginRendering(const RenderPass& renderPass,
                                 const Framebuffer& frame_buffer,
                                 const Dependencies& deps = {}) = 0;
  virtual void cmdEndRendering() = 0;
  virtual void cmdNextSubpass() = 0;

  virtual void cmdBindViewport(const Viewport& viewport) = 0;
  virtual void cmdBindScissorRect(const ScissorRect& rect) = 0;

  virtual void cmdBindRenderPipeline(RenderPipelineHandle handle) = 0;
  virtual void cmdBindDepthState(const DepthState& state) = 0;

  virtual void cmdBindVertexBuffer(uint32 index, BufferHandle buffer, uint64 offset,
                                   uint64 size) = 0;
  virtual void cmdBindIndexBuffer(BufferHandle buffer, EIndexFormat format, uint64 offset,
                                  uint64 size) = 0;
  virtual void cmdPushConstants(const void* data, size_t size, size_t offset) = 0;

  template <typename Struct>
  void cmdPushConstants(const Struct& data, size_t offset) {
    this->cmdPushConstants(&data, sizeof(Struct), offset);
  }

  virtual void cmdCopyBuffer(BufferHandle src_buffer, BufferHandle dst_buffer,
                             size_t src_offset, size_t dst_offset, size_t size) = 0;
  virtual void cmdFillBuffer(BufferHandle buffer, size_t buffer_offset, size_t size,
                             uint32 data) = 0;
  virtual void cmdUpdateBuffer(BufferHandle buffer, size_t buffer_offset, size_t size,
                               const void* data) = 0;

  template <typename Struct>
  void cmdUpdateBuffer(BufferHandle buffer, const Struct& data,
                       size_t buffer_offset = 0) {
    this->cmdUpdateBuffer(buffer, buffer_offset, sizeof(Struct), &data);
  }

  virtual void cmdDraw(uint32 vertex_count, uint32 instance_count = 1,
                       uint32 first_vertex = 0, uint32 base_instance = 0) = 0;
  virtual void cmdDrawIndexed(uint32 index_count, uint32 instance_count,
                              uint32 first_index, int32 vertex_offset,
                              uint32 base_instance) = 0;
  virtual void cmdDrawIndirect(BufferHandle indirect_buffer,
                               size_t indirect_buffer_offset, uint32 draw_count,
                               uint32 stride) = 0;
  virtual void cmdDrawIndexedIndirect(BufferHandle indirect_buffer,
                                      size_t indirect_buffer_offset, uint32 draw_count,
                                      uint32 stride) = 0;
  virtual void cmdDrawIndexedIndirectCount(BufferHandle indirect_buffer,
                                           size_t indirect_buffer_offset,
                                           BufferHandle count_buffer,
                                           size_t count_buffer_offset,
                                           uint32 max_draw_count, uint32 stride) = 0;
  virtual void cmdDrawMeshTasks(const Dimensions& group_count) = 0;
  virtual void cmdDrawMeshTasksIndirect(BufferHandle indirect_buffer,
                                        size_t indirect_buffer_offset, uint32 draw_count,
                                        uint32 stride) = 0;
  virtual void cmdDrawMeshTasksIndirectCount(BufferHandle indirect_buffer,
                                             size_t indirect_buffer_offset,
                                             BufferHandle count_buffer,
                                             size_t count_buffer_offset,
                                             uint32 max_draw_count, uint32 stride) = 0;
  virtual void cmdTraceRays(uint32 width, uint32 height, uint32 depth,
                            const Dependencies& deps) = 0;

  virtual void cmdSetBlendColor(const float color[4]) = 0;
  // the argument order is correct, so the `clamp` parameter can have a default value
  virtual void cmdSetDepthBias(float constant_factor, float slope_factor,
                               float clamp) = 0;
  virtual void cmdSetDepthBiasEnable(bool enable) = 0;
  virtual void cmdSetFragmentShadingRate(
      const Dimensions& fragment_size,
      EShadingRateCombinerOp primitive_op = EShadingRateCombinerOp::Keep,
      EShadingRateCombinerOp attachment_op = EShadingRateCombinerOp::Keep) = 0;

  virtual void cmdResetQueryPool(QueryPoolHandle pool, uint32 first_query,
                                 uint32 query_count) = 0;
  virtual void cmdWriteTimestamp(QueryPoolHandle pool, uint32 query) = 0;

  virtual void cmdClearColorImage(TextureHandle tex, const ClearColorValue& value,
                                  const TextureLayers& layers) = 0;
  virtual void cmdCopyImage(TextureHandle src, TextureHandle dst,
                            const Dimensions& extent, const Offset3D& src_offset,
                            const Offset3D& dst_offst, const TextureLayers& src_layers,
                            const TextureLayers& dst_layers) = 0;
  virtual void cmdGenerateMipmap(TextureHandle handle) = 0;
  virtual void cmdUpdateTLAS(AccelerationStructHandle handle,
                             BufferHandle instances_buffer) = 0;

#if defined(LVK_WITH_RAW_VULKAN)
  virtual operator VkCommandBuffer() const = 0;
#endif  // defined(LVK_WITH_RAW_VULKAN)
};

struct IContext {
  virtual ~IContext() = default;

  virtual ICommandBuffer& acquireCommandBuffer(bool dedicated_compute = false) = 0;

  // hand these images to the other queue (destination implied by the CB's queue); the
  // acquire is automatic
  virtual SubmitHandle submit(ICommandBuffer& commandBuffer, TextureHandle present,
                              const Span<TextureHandle>& release = {}) = 0;

  // waiting on an empty handle results in vkDeviceWaitIdle()
  virtual void wait(SubmitHandle handle) = 0;
  [[nodiscard]] virtual bool isSubmitComplete(SubmitHandle handle) const = 0;

  [[nodiscard]] virtual Holder<BufferHandle> createBuffer(const BufferDesc& desc,
                                                          const char* debugName,
                                                          Result* out_result) = 0;

  [[nodiscard]] virtual Holder<SamplerHandle> createSampler(const SamplerStateDesc& desc,
                                                            Result* out_result) = 0;
  [[nodiscard]] virtual Holder<TextureHandle> createTexture(const TextureDesc& desc,
                                                            const char* debugName,
                                                            Result* out_result) = 0;
  [[nodiscard]] virtual Holder<TextureHandle> createTextureView(
      TextureHandle texture, const TextureViewDesc& desc, const char* debugName,
      Result* out_result) = 0;

  [[nodiscard]] virtual TextureDescriptorIndex getTextureDescriptorIndex(
      TextureHandle handle) const = 0;
  [[nodiscard]] virtual SamplerDescriptorIndex getSamplerDescriptorIndex(
      SamplerHandle handle) const = 0;

  [[nodiscard]] virtual Holder<ComputePipelineHandle> createComputePipeline(
      const ComputePipelineDesc& desc, Result* out_result) = 0;
  [[nodiscard]] virtual Holder<RenderPipelineHandle> createRenderPipeline(
      const RenderPipelineDesc& desc, Result* out_result) = 0;
  [[nodiscard]] virtual Holder<RayTracingPipelineHandle> createRayTracingPipeline(
      const RayTracingPipelineDesc& desc, Result* out_result) = 0;

  [[nodiscard]] virtual Holder<ShaderModuleHandle> createShaderModule(
      const ShaderModuleDesc& desc, Result* out_result = nullptr) = 0;

  [[nodiscard]] virtual Holder<QueryPoolHandle> createQueryPool(uint32 numQueries,
                                                                const char* debugName,
                                                                Result* out_result) = 0;

  [[nodiscard]] virtual Holder<AccelerationStructHandle> createAccelerationStructure(
      const AccelStructDesc& desc, Result* out_result) = 0;

  virtual void destroy(ComputePipelineHandle handle) = 0;
  virtual void destroy(RenderPipelineHandle handle) = 0;
  virtual void destroy(RayTracingPipelineHandle) = 0;
  virtual void destroy(ShaderModuleHandle handle) = 0;
  virtual void destroy(SamplerHandle handle) = 0;
  virtual void destroy(BufferHandle handle) = 0;
  virtual void destroy(TextureHandle handle) = 0;
  virtual void destroy(QueryPoolHandle handle) = 0;
  virtual void destroy(AccelerationStructHandle handle) = 0;
  virtual void destroy(Framebuffer& fb) = 0;

  [[nodiscard]] virtual uint64 gpuAddress(AccelerationStructHandle handle) const = 0;

  [[nodiscard]] virtual AccelStructSizes getAccelerationStructSizes(
      const AccelStructDesc& desc, Result* out_result) const = 0;

  virtual Result upload(BufferHandle handle, const void* data, size_t size,
                        size_t offset) = 0;
  virtual Result download(BufferHandle handle, void* data, size_t size,
                          size_t offset) = 0;
  [[nodiscard]] virtual uint8_t* getMappedPtr(BufferHandle handle) const = 0;
  [[nodiscard]] virtual uint64 gpuAddress(BufferHandle handle, size_t offset) const = 0;
  virtual void flushMappedMemory(BufferHandle handle, size_t offset,
                                 size_t size) const = 0;
  [[nodiscard]] virtual uint32 getMaxStorageBufferRange() const = 0;

  // `data` contains mip-levels and layers as in
  // https://registry.khronos.org/KTX/specs/1.0/ktxspec.v1.html
  virtual Result upload(TextureHandle handle, const TextureRangeDesc& range,
                        const void* data, uint32 buffer_row_length) = 0;
  virtual Result download(TextureHandle handle, const TextureRangeDesc& range,
                          void* out_data) = 0;
  [[nodiscard]] virtual Dimensions getDimensions(TextureHandle handle) const = 0;
  [[nodiscard]] virtual float getAspectRatio(TextureHandle handle) const = 0;
  [[nodiscard]] virtual EFormat getFormat(TextureHandle handle) const = 0;

  virtual TextureHandle getCurrentSwapchainTexture() = 0;
  virtual EFormat getSwapchainFormat() const = 0;
  virtual EColorSpace getSwapchainColorSpace() const = 0;
  virtual uint32 getSwapchainCurrentImageIndex() const = 0;
  virtual uint32 getNumSwapchainImages() const = 0;
  virtual void recreateSwapchain(int newWidth, int newHeight) = 0;
  // VK_KHR_swapchain_maintenance1
  [[nodiscard]] virtual bool setCurrentPresentMode(EPresentMode mode) = 0;
  [[nodiscard]] virtual EPresentMode getCurrentPresentMode() const = 0;

  // MSAA level is supported if ((samples & bitmask) != 0), where samples must be power of
  // two.
  virtual uint32 getFramebufferMSAABitMask() const = 0;

  // valid range for `Framebuffer::shadingRateAttachmentTexelSize` (requires
  // VK_KHR_fragment_shading_rate)
  [[nodiscard]] virtual Dimensions getShadingRateAttachmentMinTexelSize() const = 0;
  [[nodiscard]] virtual Dimensions getShadingRateAttachmentMaxTexelSize() const = 0;

  virtual bool isExtensionEnabled(const char* ext) const = 0;
  virtual bool supportsAsyncCompute() const = 0;

  virtual double getTimestampPeriodToMs() const = 0;
  virtual bool getQueryPoolResults(QueryPoolHandle pool, uint32 firstQuery,
                                   uint32 queryCount, size_t dataSize, void* outData,
                                   size_t stride) const = 0;
};

namespace vk {
struct ContextConfig;
}

NRHI_API std::unique_ptr<IContext> createVulkanContextWithSwapchain(
    SDL_Window* window, uint32 width, uint32 height, vk::ContextConfig& cfg,
    EHWDeviceType perferred_device_type = EHWDeviceType::Discrete,
    int selected_device = -1);

}  // namespace amaryl

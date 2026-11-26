#pragma once

#include <vector>

#include "amaryl/engine/render/gpu_scene.h"

namespace amaryl {

// GPU side of GpuSceneUploads. Owns the scene buffers read through the GpuSceneRoot
// device addresses, grows them to the uploaded capacities and records the scatter and
// transform passes of a frame.
//
// Per frame, between DeferredRenderer::beginFrame() and endFrame():
//   upload(cmd, uploads);   // before every pass reading the scene
//   ...                     // passes reading getRootAddress()
//   submit;  onSubmitted(handle);
struct ENGINE_API GpuSceneBuffers final {
  GpuSceneBuffers() = default;
  ~GpuSceneBuffers();

  NO_COPYABLE(GpuSceneBuffers);

  // Loads the compute pipelines, engine:// must be mounted.
  void init(IContext* rhi);
  // Releases the GPU resources, the GPU must be idle.
  void shutdown();

  // Records the growth copies, the scatter and transform passes and the root update.
  void upload(ICommandBuffer& cmd, const GpuSceneUploads& uploads);
  // The command buffer passed to upload() was submitted as `submit`.
  void onSubmitted(SubmitHandle submit);

  // Device address of the GpuSceneRoot written by the last upload().
  uint64 getRootAddress() const;
  // Every scene buffer and the root, for the Dependencies of the passes reading them.
  Span<const BufferHandle> getBuffers() const { return handles_; }
  BufferHandle getBuffer(EGpuSceneBuffer buffer) const {
    return buffers_[static_cast<uint32>(buffer)].buffer;
  }
  const GpuSceneRoot& getRoot() const { return root_data_; }

 private:
  struct DeviceBuffer {
    Holder<BufferHandle> buffer;
    size_t size{0};
  };

  // A buffer the GPU may still read: replaced buffers and staging buffers.
  struct InFlightBuffer {
    Holder<BufferHandle> buffer;
    size_t size{0};
    SubmitHandle submit{};
    // False until the frame using the buffer is submitted.
    bool submitted{false};
  };

  bool reserve(ICommandBuffer& cmd, DeviceBuffer& target, size_t bytes, const char* name);
  BufferHandle acquireStaging(size_t bytes);
  bool isComplete(const InFlightBuffer& buffer) const;
  uint64 getAddress(const DeviceBuffer& buffer) const;
  void refreshHandles();

  IContext* rhi_{nullptr};

  DeviceBuffer buffers_[kGpuSceneBufferCount];
  DeviceBuffer world_curr_;
  DeviceBuffer world_prev_;
  Holder<BufferHandle> root_;
  GpuSceneRoot root_data_{};

  std::vector<InFlightBuffer> retired_;
  std::vector<InFlightBuffer> staging_pool_;
  std::vector<uint8> staging_bytes_;
  std::vector<GpuScatterCopy> copies_;
  std::vector<BufferHandle> handles_;

  Holder<ShaderModuleHandle> scatter_module_;
  Holder<ShaderModuleHandle> transform_module_;
  Holder<ComputePipelineHandle> scatter_pipeline_;
  Holder<ComputePipelineHandle> transform_pipeline_;
  uint32 frame_index_{0};
};

}  // namespace amaryl

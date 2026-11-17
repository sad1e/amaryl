#pragma once

#include <cassert>
#include <cstdint>
#include <unordered_map>

#include "amaryl/nrhi/config.h"

namespace amaryl {

struct RetirementPoint final {
  SubmitHandle graphics;
  SubmitHandle compute;

  [[nodiscard]] bool empty() const { return graphics.empty() && compute.empty(); }
};

struct RetirementRequest final {
  RetirementPoint point;
  bool accepted{false};
};

// Records the submission that most recently referenced each resource on each queue. A
// resource remains in the tracker after retirement is requested so a stale handle cannot
// be recorded again.
class ResourceTracker final {
 public:
#define DECLARE_RESOURCE_TRACKING(HandleType)                        \
  void record(HandleType handle, SubmitHandle submit, bool compute); \
  [[nodiscard]] RetirementRequest retire(HandleType handle);         \
  void release(HandleType handle)

  DECLARE_RESOURCE_TRACKING(ComputePipelineHandle);
  DECLARE_RESOURCE_TRACKING(RenderPipelineHandle);
  DECLARE_RESOURCE_TRACKING(RayTracingPipelineHandle);
  DECLARE_RESOURCE_TRACKING(ShaderModuleHandle);
  DECLARE_RESOURCE_TRACKING(SamplerHandle);
  DECLARE_RESOURCE_TRACKING(BufferHandle);
  DECLARE_RESOURCE_TRACKING(TextureHandle);
  DECLARE_RESOURCE_TRACKING(QueryPoolHandle);
  DECLARE_RESOURCE_TRACKING(AccelerationStructHandle);

#undef DECLARE_RESOURCE_TRACKING

 private:
  struct UseState final {
    RetirementPoint point;
    bool retiring{false};
  };

  using UseMap = std::unordered_map<uint64, UseState>;

  template <typename HandleType>
  [[nodiscard]] static uint64 key(HandleType handle) {
    return static_cast<uint64>(reinterpret_cast<uintptr_t>(handle.handleAsVoid()));
  }

  static void record(UseMap& uses, uint64 key, SubmitHandle submit, bool compute);
  [[nodiscard]] static RetirementRequest retire(UseMap& uses, uint64 key);
  static void release(UseMap& uses, uint64 key);

  UseMap compute_pipelines_;
  UseMap render_pipelines_;
  UseMap ray_tracing_pipelines_;
  UseMap shader_modules_;
  UseMap samplers_;
  UseMap buffers_;
  UseMap textures_;
  UseMap query_pools_;
  UseMap acceleration_structures_;
};

}  // namespace amaryl

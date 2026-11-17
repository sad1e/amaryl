#include "amaryl/nrhi/resource_tracker.h"

namespace amaryl {

void ResourceTracker::record(UseMap& uses, uint64 resource_key, SubmitHandle submit,
                             bool compute) {
  if (!resource_key || submit.empty()) {
    return;
  }

  UseState& state = uses[resource_key];
  assert(!state.retiring && "Cannot record a resource after destruction was requested");
  if (state.retiring) {
    return;
  }

  (compute ? state.point.compute : state.point.graphics) = submit;
}

RetirementRequest ResourceTracker::retire(UseMap& uses, uint64 resource_key) {
  if (!resource_key) {
    return {};
  }

  UseState& state = uses[resource_key];
  assert(!state.retiring && "Resource destruction was requested more than once");
  if (state.retiring) {
    return {};
  }
  state.retiring = true;
  return {.point = state.point, .accepted = true};
}

void ResourceTracker::release(UseMap& uses, uint64 resource_key) {
  if (!resource_key) {
    return;
  }

  const auto iter = uses.find(resource_key);
  assert(iter != uses.end() && iter->second.retiring);
  if (iter != uses.end()) {
    uses.erase(iter);
  }
}

#define DEFINE_RESOURCE_TRACKING(HandleType, Member)                                   \
  void ResourceTracker::record(HandleType handle, SubmitHandle submit, bool compute) { \
    if (!handle.empty()) {                                                             \
      record(Member, key(handle), submit, compute);                                    \
    }                                                                                  \
  }                                                                                    \
  RetirementRequest ResourceTracker::retire(HandleType handle) {                       \
    return handle.empty() ? RetirementRequest{} : retire(Member, key(handle));         \
  }                                                                                    \
  void ResourceTracker::release(HandleType handle) {                                   \
    if (!handle.empty()) {                                                             \
      release(Member, key(handle));                                                    \
    }                                                                                  \
  }

DEFINE_RESOURCE_TRACKING(ComputePipelineHandle, compute_pipelines_)
DEFINE_RESOURCE_TRACKING(RenderPipelineHandle, render_pipelines_)
DEFINE_RESOURCE_TRACKING(RayTracingPipelineHandle, ray_tracing_pipelines_)
DEFINE_RESOURCE_TRACKING(ShaderModuleHandle, shader_modules_)
DEFINE_RESOURCE_TRACKING(SamplerHandle, samplers_)
DEFINE_RESOURCE_TRACKING(BufferHandle, buffers_)
DEFINE_RESOURCE_TRACKING(TextureHandle, textures_)
DEFINE_RESOURCE_TRACKING(QueryPoolHandle, query_pools_)
DEFINE_RESOURCE_TRACKING(AccelerationStructHandle, acceleration_structures_)

#undef DEFINE_RESOURCE_TRACKING

}  // namespace amaryl

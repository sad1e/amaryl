// gpu-scene-transforms.compute.slang
//
// Applies the GpuTransformUpdate list of a frame to worldCurr / worldPrev (at most one
// update per instance):
//   regular:  prev = curr,  curr = world
//   teleport: prev = world, curr = world   (no motion vectors across the jump)
//   settle:   prev = curr                  (moved last frame, not this frame)
//
// Layout note: the push constant block matches `TransformPushConstants` in
// src/engine/render/gpu_scene_buffers.cpp.

#include "amaryl/engine/render/gpu_scene.shared.h"

struct TransformPushConstants
{
    GpuTransformUpdate* updates;
    GpuTransform* worldCurr;
    GpuTransform* worldPrev;
    uint updateCount;
    uint pad0;
};

[[vk::push_constant]] TransformPushConstants g_push;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchId : SV_DispatchThreadID)
{
    if (dispatchId.x >= g_push.updateCount)
        return;

    const GpuTransformUpdate update = g_push.updates[dispatchId.x];
    const uint id = update.instanceId;
    if ((update.flags & GPU_TRANSFORM_UPDATE_SETTLE) != 0)
    {
        g_push.worldPrev[id] = g_push.worldCurr[id];
        return;
    }

    g_push.worldPrev[id] =
        (update.flags & GPU_TRANSFORM_UPDATE_TELEPORT) != 0 ? update.world : g_push.worldCurr[id];
    g_push.worldCurr[id] = update.world;
}

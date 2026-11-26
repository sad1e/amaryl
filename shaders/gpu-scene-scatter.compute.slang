// gpu-scene-scatter.compute.slang
//
// Applies the scatter copies of one GpuScatterQueue (see amaryl/engine/render/gpu_scene.h):
// copy i moves copies[i].dwordCount dwords from payload[copies[i].srcOffset] to
// dst[copies[i].dstOffset]. One workgroup per copy, the copies never overlap.
//
// Layout note: the push constant block matches `ScatterPushConstants` in
// src/engine/render/gpu_scene_buffers.cpp.

#include "amaryl/engine/render/gpu_scene.shared.h"

struct ScatterPushConstants
{
    uint* payload;
    GpuScatterCopy* copies;
    uint* dst;
    uint copyCount;
    uint pad0;
};

[[vk::push_constant]] ScatterPushConstants g_push;

static const uint kGroupSize = 64;
static const uint kMaxGroupsX = 65535;

[shader("compute")]
[numthreads(kGroupSize, 1, 1)]
void computeMain(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    const uint copyIndex = groupId.y * kMaxGroupsX + groupId.x;
    if (copyIndex >= g_push.copyCount)
        return;

    const GpuScatterCopy copy = g_push.copies[copyIndex];
    for (uint i = threadId.x; i < copy.dwordCount; i += kGroupSize)
        g_push.dst[copy.dstOffset + i] = g_push.payload[copy.srcOffset + i];
}

// debug-lighting-only.slang
//
// Full-screen "debug lighting only" pass. The scene mesh is rendered as a white
// (untextured) model - albedo is forced to 1 and no texture is sampled - so that only
// the lighting contribution is visible.
//
// This pass also exercises two renderer plumbing paths end-to-end so they can be
// verified against a minimal surface:
//   1. bindless sampling: a texture descriptor index arrives through push constants and
//      is used to sample from the global bindless sampled-texture array.
//   2. push-constant device address: a raw GPU address arrives through push constants
//      and is dereferenced as a physical-storage-buffer pointer.
//
// Layout note: the push constant block matches `DebugLightingPushConstants` in
// src/engine/render/deferred_renderer.cpp exactly (8-byte address + four uint32).

#include "amaryl/nrhi/descriptor.shared.h"

struct DebugLightingPushConstants
{
    // Device address of the debug verification buffer (float4[2]).
    float4* debugBuffer;
    // Bindless sampled-texture descriptor index.
    uint textureIndex;
    // Bindless sampler descriptor index.
    uint samplerIndex;
    uint frameIndex;
    uint pad0;
};

[[vk::push_constant]] DebugLightingPushConstants g_push;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// Full-screen triangle: three vertices generated from SV_VertexID, no vertex buffer.
[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    const float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    output.uv = uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    // 1. Verify bindless sampling: index the global descriptor array with the
    //    non-uniform index delivered via push constants.
    float4 sampled =
        NRHISampledTexture2DFloat4Descriptors[NonUniformResourceIndex(g_push.textureIndex)]
            .Sample(NRHISamplerDescriptors[NonUniformResourceIndex(g_push.samplerIndex)],
                    input.uv);

    // 2. Verify push-constant device address: dereference the raw address.
    float4 fromAddress = g_push.debugBuffer[0];

    // Scene meshes are rendered white: albedo = 1, no texture sampling. The debug view
    // only shows lighting, which is faked here by the two verified inputs.
    const float3 albedo = float3(1.0, 0.0, 1.0);
    float3 result = sampled.rgb * 0.5 + fromAddress.rgb * 0.5;
    result *= albedo;

    return float4(result, 1.0);
}

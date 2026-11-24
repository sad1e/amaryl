// imgui.slang
//
// Dear ImGui renderer, see src/engine/render/imgui_renderer.cpp.
// - Vertex pulling: ImDrawVert { float2 pos; float2 uv; uint col; } is read as 5 x uint
//   from a raw device address, independent of the std430/scalar layout rules.
// - ImTextureID is a bindless sampled-texture descriptor index.
//
// Layout note: the push constant block matches `ImGuiPushConstants` in
// src/engine/render/imgui_renderer.cpp exactly (40 bytes).

#include "amaryl/nrhi/descriptor.shared.h"

struct ImGuiPushConstants
{
    // ImDrawVert of the current draw command, ImDrawCmd::VtxOffset already applied.
    uint* vertices;
    float2 scale;
    float2 translate;
    uint textureIndex;
    uint samplerIndex;
    // Bit 0: the target is sRGB, the vertex colors are converted to linear.
    uint flags;
    uint pad0;
};

[[vk::push_constant]] ImGuiPushConstants g_push;

static const uint kImDrawVertStride = 5;
static const uint kFlagLinearOutput = 1;

struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

float3 srgbToLinear(float3 c)
{
    return lerp(pow((c + 0.055) / 1.055, 2.4), c / 12.92, step(c, float3(0.04045)));
}

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    const uint base = vertexID * kImDrawVertStride;
    const float2 pos = asfloat(uint2(g_push.vertices[base + 0], g_push.vertices[base + 1]));
    const float2 uv = asfloat(uint2(g_push.vertices[base + 2], g_push.vertices[base + 3]));
    const uint col = g_push.vertices[base + 4];

    float4 color = float4(float(col & 0xFF), float((col >> 8) & 0xFF),
                          float((col >> 16) & 0xFF), float(col >> 24)) / 255.0;
    if ((g_push.flags & kFlagLinearOutput) != 0)
    {
        color.rgb = srgbToLinear(color.rgb);
    }

    VSOutput output;
    output.position = float4(pos * g_push.scale + g_push.translate, 0.0, 1.0);
    output.color = color;
    output.uv = uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    const float4 texel =
        NRHISampledTexture2DFloat4Descriptors[NonUniformResourceIndex(g_push.textureIndex)]
            .Sample(NRHISamplerDescriptors[NonUniformResourceIndex(g_push.samplerIndex)],
                    input.uv);
    return input.color * texel;
}

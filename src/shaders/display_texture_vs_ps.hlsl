#include "common.hlsli"

#define ROOT_SIGNATURE \
    "DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_POINT, visibility = SHADER_VISIBILITY_PIXEL)"

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out FLOAT4 out_position_ndc : SV_Position,
    out FLOAT2 out_uv : _Uv
) {
    const FLOAT2 positions[4] = {
        FLOAT2(-1.0f, -1.0f),
        FLOAT2(-1.0f, 1.0f),
        FLOAT2(1.0f, -1.0f),
        FLOAT2(1.0f, 1.0f),
    };
    out_position_ndc = FLOAT4(0.25f * positions[vertex_id] + FLOAT2(0.7f, 0.7f), 0.0f, 1.0f);
    out_uv = 0.5f * positions[vertex_id] + 0.5f;
}

Texture2D srv_t0 : register(t0);
SamplerState sam_s0 : register(s0);

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    FLOAT4 position_ndc : SV_Position,
    FLOAT2 uv : _Uv,
    out FLOAT4 out_color : SV_Target0
) {
    out_color = srv_t0.Sample(sam_s0, uv);
}

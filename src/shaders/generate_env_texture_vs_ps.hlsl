#include "common.hlsli"
#include "../cpp_hlsl_common.h"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3, visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0, numDescriptors = 2), visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t2), visibility = SHADER_VISIBILITY_PIXEL), " \
    "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), " \
    "StaticSampler(" \
    "   s0, " \
    "   filter = FILTER_MIN_MAG_LINEAR_MIP_POINT, " \
    "   visibility = SHADER_VISIBILITY_PIXEL, " \
    "   addressU = TEXTURE_ADDRESS_BORDER, " \
    "   addressV = TEXTURE_ADDRESS_BORDER" \
    ")"

struct _GLOBALS {
    XMFLOAT4X4 object_to_clip;
};
ConstantBuffer<DRAW_COMMAND> cbv_draw_cmd : register(b0);
ConstantBuffer<_GLOBALS> cbv_glob : register(b1);

StructuredBuffer<VERTEX> srv_vertices : register(t0);
Buffer<U32> srv_indices : register(t1);

Texture2D srv_equirectangular_texture : register(t2);
SamplerState sam_s0 : register(s0);

XMFLOAT2 Sample_Spherical_Map(XMFLOAT3 v) {
    XMFLOAT2 uv = XMFLOAT2(atan2(v.z, v.x), asin(v.y));
    uv *= XMFLOAT2(0.1591f, 0.3183f);
    uv += 0.5f;
    return uv;
}

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out XMFLOAT4 out_position_ndc : SV_Position,
    out XMFLOAT3 out_position : _Position
) {
    const U32 vertex_index = srv_indices[vertex_id + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    const XMFLOAT3 position = srv_vertices[vertex_index].position;

    out_position_ndc = mul(XMFLOAT4(position, 1.0f), cbv_glob.object_to_clip);
    out_position = position; // Position in object space.
}

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    float4 position_ndc : SV_Position,
    float3 position : _Position,
    out float4 out_color : SV_Target0
) {
    const XMFLOAT2 uv = Sample_Spherical_Map(normalize(position));
    out_color = srv_equirectangular_texture.SampleLevel(sam_s0, uv, 0);
}

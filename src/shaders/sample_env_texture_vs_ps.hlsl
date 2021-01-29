#include "common.hlsli"
#include "../cpp_hlsl_common.h"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3, visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0, numDescriptors = 2), visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t2), visibility = SHADER_VISIBILITY_PIXEL), " \
    "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), " \
    "StaticSampler(" \
    "   s0, " \
    "   filter = FILTER_MIN_MAG_MIP_LINEAR, " \
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

TextureCube srv_env_texture : register(t2);
SamplerState sam_s0 : register(s0);

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out XMFLOAT4 out_position_ndc : SV_Position,
    out XMFLOAT3 out_uvw : _Uvw
) {
    const U32 vertex_index = srv_indices[vertex_id + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    const XMFLOAT3 position = srv_vertices[vertex_index].position;

    out_position_ndc = mul(XMFLOAT4(position, 1.0f), cbv_glob.object_to_clip).xyww;
    out_uvw = position;
}

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    XMFLOAT4 position_ndc : SV_Position,
    XMFLOAT3 uvw : _Uvw,
    out XMFLOAT4 out_color : SV_Target0
) {
    XMFLOAT3 env_color = srv_env_texture.Sample(sam_s0, uvw).rgb;
    env_color = env_color / (env_color + 1.0f);
    out_color = XMFLOAT4(env_color, 1.0f);
}

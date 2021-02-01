#include "common.hlsli"
#include "common_pbr_math.hlsli"
#include "../cpp_hlsl_common.h"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3, visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0, numDescriptors = 2), visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t2), visibility = SHADER_VISIBILITY_PIXEL), " \
    "CBV(b1), " \
    "RootConstants(b2, num32BitConstants = 1, visibility = SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(" \
    "   s0, " \
    "   filter = FILTER_MIN_MAG_LINEAR_MIP_POINT, " \
    "   visibility = SHADER_VISIBILITY_PIXEL, " \
    "   addressU = TEXTURE_ADDRESS_CLAMP, " \
    "   addressV = TEXTURE_ADDRESS_CLAMP, " \
    "   addressW = TEXTURE_ADDRESS_CLAMP" \
    ")"

struct _GLOBALS {
    XMFLOAT4X4 object_to_clip;
};
struct ROOT_CONST {
    F32 roughness;
};
ConstantBuffer<DRAW_COMMAND> cbv_draw_cmd : register(b0);
ConstantBuffer<_GLOBALS> cbv_glob : register(b1);
ConstantBuffer<ROOT_CONST> cbv_const : register(b2);

StructuredBuffer<VERTEX> srv_vertices : register(t0);
Buffer<U32> srv_indices : register(t1);

TextureCube srv_env_texture : register(t2);
SamplerState sam_s0 : register(s0);

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
    out_position = position;
}

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    XMFLOAT4 position_ndc : SV_Position,
    XMFLOAT3 position : _Position,
    out XMFLOAT4 out_color : SV_Target0
) {
    const F32 roughness = cbv_const.roughness;
    const XMFLOAT3 n = normalize(position);
    const XMFLOAT3 r = n;
    const XMFLOAT3 v = r;

    XMFLOAT3 prefiltered_color = 0.0f;
    F32 total_weight = 0.0f;
    const U32 num_samples = 4 * 1024;

    for (U32 sample_idx = 0; sample_idx < num_samples; ++sample_idx) {
        const XMFLOAT2 xi = Hammersley(sample_idx, num_samples);
        const XMFLOAT3 h = Importance_Sample_GGX(xi, roughness, n);
        const XMFLOAT3 l = normalize(2.0f * dot(v, h) * h - v);
        const F32 n_dot_l = saturate(dot(n, l));
        if (n_dot_l > 0.0f) {
            prefiltered_color += srv_env_texture.SampleLevel(sam_s0, l, 0).rgb * n_dot_l;
            total_weight += n_dot_l;
        }
    }
    out_color = XMFLOAT4(prefiltered_color / max(total_weight, 0.001f), 1.0f);
}

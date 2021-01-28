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

#define PI 3.1415926f

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    XMFLOAT4 position_ndc : SV_Position,
    XMFLOAT3 position : _Position,
    out XMFLOAT4 out_color : SV_Target0
) {
    const XMFLOAT3 n = normalize(position);

    // This is Right-Handed coordinate system and works for upper-left UV coordinate systems.
    const XMFLOAT3 up_vector = abs(n.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
    const XMFLOAT3 tangent_x = normalize(cross(up_vector, n));
    const XMFLOAT3 tangent_y = normalize(cross(n, tangent_x));

    U32 num_samples = 0;
    XMFLOAT3 irradiance = 0.0f;

    for (float phi = 0.0f; phi < (2.0f * PI); phi += 0.025f) {
        for (float theta = 0.0f; theta < (0.5f * PI); theta += 0.025f) {
            // Point on a hemisphere.
            const XMFLOAT3 h = XMFLOAT3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

            // Transform from tangent space to world space.
            const XMFLOAT3 sample_vector = tangent_x * h.x + tangent_y * h.y + n * h.z;

            irradiance += srv_env_texture.SampleLevel(sam_s0, sample_vector, 0).rgb *
                cos(theta) * sin(theta);

            num_samples++;
        }
    }

    irradiance = PI * irradiance * (1.0f / num_samples);
    out_color = XMFLOAT4(irradiance, 1.0f);
}

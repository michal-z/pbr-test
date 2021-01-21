#include "test_common.hlsli"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3, visibility = SHADER_VISIBILITY_VERTEX), " \
    "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0, numDescriptors = 2), visibility = SHADER_VISIBILITY_VERTEX)"

struct DRAW_COMMAND {
    UINT index_offset;
    UINT vertex_offset;
    UINT renderable_id;
};

ConstantBuffer<DRAW_COMMAND> cbv_draw_cmd : register(b0);

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    UINT vertex_id : SV_VertexID,
    out OUTPUT_VERTEX output
) {
    const UINT vertex_index = srv_indices[vertex_id + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    FLOAT4 position = FLOAT4(srv_vertices[vertex_index].position, 1.0f);

    const FLOAT4X4 object_to_world = cbv_const.objects[cbv_draw_cmd.renderable_id].object_to_world;
    const FLOAT4X4 world_to_clip = cbv_const.world_to_clip;

    FLOAT3 color;
    {
        const UINT hash0 = Hash(cbv_draw_cmd.renderable_id);
        color = FLOAT3(hash0 & 0xFF, (hash0 >> 8) & 0xFF, (hash0 >> 16) & 0xFF) / 255.0;

        if (cbv_const.enable_debug_draw) {
            const UINT hash1 = Hash(vertex_index / 3);
            color *= FLOAT3(hash1 & 0xFF, (hash1 >> 8) & 0xFF, (hash1 >> 16) & 0xFF) / 255.0;
        }
    }

    position = mul(position, object_to_world);
    output.position = position.xyz;

    position = mul(position, world_to_clip);
    output.position_ndc = position;

    output.normal = mul(srv_vertices[vertex_index].normal, (FLOAT3X3)object_to_world);
    output.color = color;
}

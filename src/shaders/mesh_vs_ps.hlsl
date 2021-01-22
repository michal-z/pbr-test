#include "common.hlsli"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3, visibility = SHADER_VISIBILITY_VERTEX), " \
    "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0, numDescriptors = 2), visibility = SHADER_VISIBILITY_VERTEX)"

struct DRAW_COMMAND {
    UINT index_offset;
    UINT vertex_offset;
    UINT renderable_id;
};

struct INPUT_VERTEX {
    FLOAT3 position;
    FLOAT3 normal;
};

struct OUTPUT_VERTEX {
    FLOAT4 position_ndc : SV_Position;
    FLOAT3 position : _Position;
    FLOAT3 normal : _Normal;
    FLOAT3 color : _Color;
};

struct CONSTANTS {
    FLOAT4X4 world_to_clip;
    struct {
        FLOAT4X4 object_to_world;
    } objects[1024];
};

ConstantBuffer<CONSTANTS> cbv_const : register(b1);

StructuredBuffer<INPUT_VERTEX> srv_vertices : register(t0);
Buffer<UINT> srv_indices : register(t1);

UINT Hash(UINT a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

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
    }

    position = mul(position, object_to_world);
    output.position = position.xyz;

    position = mul(position, world_to_clip);
    output.position_ndc = position;

    output.normal = mul(srv_vertices[vertex_index].normal, (FLOAT3X3)object_to_world);
    output.color = color;
}

void Pixel_Shader(
    OUTPUT_VERTEX input,
    out FLOAT4 out_color : SV_Target0
) {
    const FLOAT3 normal = normalize(input.normal);

    const FLOAT3 light_positions[4] = {
        FLOAT3(50.0f, 25.0f, 0.0f),
        FLOAT3(-50.0f, 25.0f, 0.0f),
        FLOAT3(0.0f, 25.0f, -50.0f),
        FLOAT3(0.0f, 25.0f, 50.0f),
    };
    const FLOAT3 light_colors[4] = {
        FLOAT3(0.4f, 0.3f, 0.1f),
        FLOAT3(0.4f, 0.3f, 0.1f),
        FLOAT3(1.0f, 0.8f, 0.2f),
        FLOAT3(1.0f, 0.8f, 0.2f),
    };

    FLOAT3 color = 0.0f;
    for (UINT i = 0; i < 4; ++i) {
        const FLOAT3 l = normalize(light_positions[i] - input.position);
        const FLOAT n_dot_l = saturate(dot(normal, l));
        color += n_dot_l * light_colors[i];
    }
    color *= input.color;

    out_color = FLOAT4(0.05f + color, 1.0f);
}

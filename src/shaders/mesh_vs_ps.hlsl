#include "common.hlsli"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3, visibility = SHADER_VISIBILITY_VERTEX), " \
    "CBV(b1, visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0, numDescriptors = 2), visibility = SHADER_VISIBILITY_VERTEX)"

struct DRAW_COMMAND {
    U32 index_offset;
    U32 vertex_offset;
    U32 renderable_id;
};

struct INPUT_VERTEX {
    FLOAT3 position;
    FLOAT3 normal;
    FLOAT2 uv;
};

struct OUTPUT_VERTEX {
    FLOAT4 position_ndc : SV_Position;
    FLOAT3 position : _Position;
    FLOAT3 normal : _Normal;
    FLOAT2 uv : _Uv;
};

struct CONSTANTS {
    FLOAT4X4 world_to_clip;
    struct {
        FLOAT4X4 object_to_world;
    } objects[1024];
};

ConstantBuffer<DRAW_COMMAND> cbv_draw_cmd : register(b0);
ConstantBuffer<CONSTANTS> cbv_const : register(b1);
StructuredBuffer<INPUT_VERTEX> srv_vertices : register(t0);
Buffer<U32> srv_indices : register(t1);

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out OUTPUT_VERTEX output
) {
    const U32 vertex_index = srv_indices[vertex_id + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    FLOAT4 position = FLOAT4(srv_vertices[vertex_index].position, 1.0f);

    const FLOAT4X4 object_to_world = cbv_const.objects[cbv_draw_cmd.renderable_id].object_to_world;
    const FLOAT4X4 world_to_clip = cbv_const.world_to_clip;

    position = mul(position, object_to_world);
    output.position = position.xyz;

    position = mul(position, world_to_clip);
    output.position_ndc = position;

    output.normal = mul(srv_vertices[vertex_index].normal, (FLOAT3X3)object_to_world);
    output.uv = srv_vertices[vertex_index].uv;
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
    for (U32 i = 0; i < 4; ++i) {
        const FLOAT3 l = normalize(light_positions[i] - input.position);
        const F32 n_dot_l = saturate(dot(normal, l));
        color += n_dot_l * light_colors[i];
    }

    //out_color = FLOAT4(0.05f + color, 1.0f);
    out_color = FLOAT4(input.uv, 0.0f, 1.0f);
}

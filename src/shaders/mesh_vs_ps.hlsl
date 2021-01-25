#include "mesh_common.hlsli"

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out FLOAT4 out_position_ndc : SV_Position,
    out FLOAT3 out_position : _Position,
    out FLOAT3 out_normal : _Normal,
    out FLOAT2 out_uv : _Uv
) {
    const U32 vertex_index = srv_indices[vertex_id + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    FLOAT4 position = FLOAT4(srv_vertices[vertex_index].position, 1.0f);

    const FLOAT4X4 object_to_world = cbv_const.objects[cbv_draw_cmd.renderable_id].object_to_world;
    const FLOAT4X4 world_to_clip = cbv_const.world_to_clip;

    position = mul(position, object_to_world);
    out_position = position.xyz;

    position = mul(position, world_to_clip);
    out_position_ndc = position;

    out_normal = mul(srv_vertices[vertex_index].normal, (FLOAT3X3)object_to_world);
    out_uv = srv_vertices[vertex_index].uv;
}

void Pixel_Shader(
    FLOAT4 position_ndc : SV_Position,
    FLOAT3 position : _Position,
    FLOAT3 normal : _Normal,
    FLOAT2 uv : _Uv,
    out FLOAT4 out_color : SV_Target0
) {

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

    const FLOAT3 n = normalize(normal);
    FLOAT3 color = 0.0f;
    for (U32 i = 0; i < 4; ++i) {
        const FLOAT3 l = normalize(light_positions[i] - position);
        const F32 n_dot_l = saturate(dot(n, l));
        color += n_dot_l * light_colors[i];
    }

    out_color = FLOAT4(uv, 0.0f, 1.0f);
}

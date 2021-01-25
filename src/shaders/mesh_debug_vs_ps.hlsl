#include "mesh_common.hlsli"

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out FLOAT4 out_position_ndc : SV_Position,
    out U32 out_mode : _Mode
) {
    const U32 vertex_index = srv_indices[vertex_id / 4 + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    const FLOAT4X4 object_to_world = cbv_const.objects[cbv_draw_cmd.renderable_id].object_to_world;
    const FLOAT4X4 world_to_clip = cbv_const.world_to_clip;

    const FLOAT3 normal = srv_vertices[vertex_index].normal;
    const FLOAT4 tangent = srv_vertices[vertex_index].tangent;

    const U32 mode = vertex_id & 0x3;

    FLOAT4 position = 0.0f;
    if (mode == 0 || mode == 2) {
        position = FLOAT4(srv_vertices[vertex_index].position, 1.0f);
    } else if (mode == 1) {
        position = FLOAT4(srv_vertices[vertex_index].position + 0.05f * normalize(normal), 1.0f);
    } else if (mode == 3) {
        position = FLOAT4(srv_vertices[vertex_index].position + 0.05f * normalize(tangent.xyz), 1.0f);
    }

    out_position_ndc = mul(position, mul(object_to_world, world_to_clip));
    out_mode = mode;
}

void Pixel_Shader(
    FLOAT4 position_ndc : SV_Position,
    U32 mode : _Mode,
    out FLOAT4 out_color : SV_Target0
) {
    if (mode == 0 || mode == 1) { // Normal
        out_color = FLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
    } else { // Tangent
        out_color = FLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
    }
}

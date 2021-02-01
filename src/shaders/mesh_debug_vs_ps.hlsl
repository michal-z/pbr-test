#include "mesh_common.hlsli"

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out XMFLOAT4 out_position_ndc : SV_Position,
    out U32 out_mode : _Mode
) {
    const U32 vertex_index = srv_indices[vertex_id / 6 + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    const XMFLOAT4X4 object_to_world = srv_const_renderables[cbv_draw_cmd.renderable_id].object_to_world;
    const XMFLOAT4X4 world_to_clip = cbv_glob.world_to_clip;

    const XMFLOAT3 normal = srv_vertices[vertex_index].normal;
    const XMFLOAT4 tangent = srv_vertices[vertex_index].tangent;

    const U32 mode = vertex_id & 0x7;

    XMFLOAT4 position = 0.0f;
    if (mode == 0 || mode == 2 || mode == 4) {
        position = XMFLOAT4(srv_vertices[vertex_index].position, 1.0f);
    } else if (mode == 1) {
        position = XMFLOAT4(srv_vertices[vertex_index].position + 0.05f * normalize(normal), 1.0f);
    } else if (mode == 3) {
        position = XMFLOAT4(srv_vertices[vertex_index].position + 0.05f * normalize(tangent.xyz), 1.0f);
    } else if (mode == 5) {
        position = XMFLOAT4(
            srv_vertices[vertex_index].position + 0.05f * normalize(cross(normal, tangent.xyz)) *
            tangent.w,
            1.0f
        );
    }

    out_position_ndc = mul(position, mul(object_to_world, world_to_clip));
    out_mode = mode;
}

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    XMFLOAT4 position_ndc : SV_Position,
    U32 mode : _Mode,
    out XMFLOAT4 out_color : SV_Target0
) {
    if (mode == 0 || mode == 1) { // Normal
        out_color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    } else if (mode == 2 || mode == 3) { // Tangent 
        out_color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
    } else { // Bitangent
        out_color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
    }
}

#include "mesh_common.hlsli"

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out XMFLOAT4 out_position_ndc : SV_Position,
    out XMFLOAT3 out_position : _Position,
    out XMFLOAT3 out_normal : _Normal,
    out XMFLOAT2 out_uv : _Uv
) {
    const U32 vertex_index = srv_indices[vertex_id + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    XMFLOAT4 position = XMFLOAT4(srv_vertices[vertex_index].position, 1.0f);

    const XMFLOAT4X4 object_to_world = srv_const_renderables[cbv_draw_cmd.renderable_id].object_to_world;
    const XMFLOAT4X4 world_to_clip = cbv_glob.world_to_clip;

    position = mul(position, object_to_world);
    out_position = position.xyz;

    position = mul(position, world_to_clip);
    out_position_ndc = position;

    out_normal = mul(srv_vertices[vertex_index].normal, (XMFLOAT3X3)object_to_world);
    out_uv = srv_vertices[vertex_index].uv;
}

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    XMFLOAT4 position_ndc : SV_Position,
    XMFLOAT3 position : _Position,
    XMFLOAT3 normal : _Normal,
    XMFLOAT2 uv : _Uv,
    out XMFLOAT4 out_color : SV_Target0
) {
    if (cbv_glob.draw_mode == 0) {
        const XMFLOAT3 light_positions[4] = {
            XMFLOAT3(50.0f, 25.0f, 0.0f),
            XMFLOAT3(-50.0f, 25.0f, 0.0f),
            XMFLOAT3(0.0f, 25.0f, -50.0f),
            XMFLOAT3(0.0f, 25.0f, 50.0f),
        };
        const XMFLOAT3 light_colors[4] = {
            XMFLOAT3(0.4f, 0.3f, 0.1f),
            XMFLOAT3(0.4f, 0.3f, 0.1f),
            XMFLOAT3(1.0f, 0.8f, 0.2f),
            XMFLOAT3(1.0f, 0.8f, 0.2f),
        };

        const XMFLOAT3 n = normalize(normal);
        XMFLOAT3 color = 0.0f;
        for (U32 i = 0; i < 4; ++i) {
            const XMFLOAT3 l = normalize(light_positions[i] - position);
            const F32 n_dot_l = saturate(dot(n, l));
            color += n_dot_l * light_colors[i];
        }

        out_color = XMFLOAT4(uv, 0.0f, 1.0f);
    } else {
        out_color = srv_mesh_textures[cbv_glob.draw_mode - 1].Sample(sam_linear, uv);
    }
}

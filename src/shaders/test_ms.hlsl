#include "test_common.hlsli"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3, visibility = SHADER_VISIBILITY_MESH), " \
    "CBV(b1, visibility = SHADER_VISIBILITY_MESH), " \
    "DescriptorTable(SRV(t0, numDescriptors = 4), visibility = SHADER_VISIBILITY_MESH)"

#define NUM_THREADS 32
#define MAX_NUM_VERTICES 64
#define MAX_NUM_PRIMITIVES 126

struct DRAW_COMMAND {
    UINT meshlet_offset;
    UINT vertex_offset;
    UINT renderable_id;
};

struct MESHLET {
    UINT data_offset;
    UINT num_vertices_triangles;
};

ConstantBuffer<DRAW_COMMAND> cbv_draw_cmd : register(b0);

Buffer<UINT> srv_meshlets : register(t2);
Buffer<UINT> srv_meshlets_data : register(t3);

[RootSignature(ROOT_SIGNATURE)]
[outputtopology("triangle")]
[numthreads(NUM_THREADS, 1, 1)]
void Mesh_Shader(
    UINT group_index : SV_GroupIndex,
    UINT3 group_id : SV_GroupID,
    out vertices OUTPUT_VERTEX out_vertices[MAX_NUM_VERTICES],
    out indices UINT3 out_triangles[MAX_NUM_PRIMITIVES]
) {
    const UINT thread_idx = group_index;
    const UINT meshlet_idx = group_id.x + cbv_draw_cmd.meshlet_offset;

    const UINT offset_vertices_triangles = srv_meshlets[meshlet_idx];
    const UINT data_offset = offset_vertices_triangles & 0x3FFFF;
    const UINT num_vertices = (offset_vertices_triangles >> 18) & 0x7F;
    const UINT num_triangles = (offset_vertices_triangles >> 25) & 0x7F;

    const UINT vertex_offset = data_offset;
    const UINT index_offset = data_offset + num_vertices;

    const FLOAT4X4 object_to_world = cbv_const.objects[cbv_draw_cmd.renderable_id].object_to_world;
    const FLOAT4X4 world_to_clip = cbv_const.world_to_clip;

    SetMeshOutputCounts(num_vertices, num_triangles);

    FLOAT3 color;
    {
        const UINT hash0 = Hash(cbv_draw_cmd.renderable_id);
        color = FLOAT3(hash0 & 0xFF, (hash0 >> 8) & 0xFF, (hash0 >> 16) & 0xFF) / 255.0;

        if (cbv_const.enable_debug_draw) {
            const UINT hash1 = Hash(meshlet_idx);
            color *= FLOAT3(hash1 & 0xFF, (hash1 >> 8) & 0xFF, (hash1 >> 16) & 0xFF) / 255.0;
        }
    }

    UINT i;
    for (i = thread_idx; i < num_vertices; i += NUM_THREADS) {
        const UINT vertex_idx = srv_meshlets_data[vertex_offset + i] + cbv_draw_cmd.vertex_offset;

        FLOAT4 position = FLOAT4(srv_vertices[vertex_idx].position, 1.0f);

        position = mul(position, object_to_world);
        out_vertices[i].position = position.xyz;

        position = mul(position, world_to_clip);
        out_vertices[i].position_ndc = position;

        out_vertices[i].normal = mul(srv_vertices[vertex_idx].normal, (FLOAT3X3)object_to_world);
        out_vertices[i].color = color;
    }

    for (i = thread_idx; i < num_triangles; i += NUM_THREADS) {
        const UINT prim = srv_meshlets_data[index_offset + i];
        out_triangles[i] = UINT3(prim & 0x3FF, (prim >> 10) & 0x3FF, (prim >> 20) & 0x3FF);
    }
}

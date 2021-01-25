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
    FLOAT4 tangent;
    FLOAT2 uv;
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

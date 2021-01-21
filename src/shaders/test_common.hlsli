#pragma once
#include "common.hlsli"

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
    UINT enable_debug_draw;
    UINT3 padding0;
    UINT4 padding1[3];
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

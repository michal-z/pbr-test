#pragma once
#include "common.hlsli"
#include "../cpp_hlsl_common.h"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3, visibility = SHADER_VISIBILITY_VERTEX), " \
    "CBV(b1), " \
    "DescriptorTable(SRV(t0, numDescriptors = 3), visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t4, numDescriptors = 4), visibility = SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, visibility = SHADER_VISIBILITY_PIXEL)"

ConstantBuffer<DRAW_COMMAND> cbv_draw_cmd : register(b0);
ConstantBuffer<GLOBALS> cbv_glob : register(b1);

StructuredBuffer<VERTEX> srv_vertices : register(t0);
Buffer<U32> srv_indices : register(t1);
StructuredBuffer<RENDERABLE_CONSTANTS> srv_const_renderables : register(t2);

Texture2D srv_mesh_textures[4] : register(t4);

SamplerState sam_linear : register(s0);

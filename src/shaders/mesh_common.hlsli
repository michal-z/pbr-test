#pragma once
#include "common.hlsli"
#include "../cpp_hlsl_common.h"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 3), " \
    "CBV(b1), " \
    "DescriptorTable(SRV(t0, numDescriptors = 3)), " \
    "DescriptorTable(SRV(t3, numDescriptors = 7), visibility = SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(" \
    "   s0, " \
    "   filter = FILTER_ANISOTROPIC, " \
    "   maxAnisotropy = 16, " \
    "   visibility = SHADER_VISIBILITY_PIXEL" \
    "), " \
    "StaticSampler(" \
    "   s1, " \
    "   filter = FILTER_MIN_MAG_LINEAR_MIP_POINT, " \
    "   addressU = TEXTURE_ADDRESS_CLAMP, " \
    "   addressW = TEXTURE_ADDRESS_CLAMP, " \
    "   addressV = TEXTURE_ADDRESS_CLAMP, " \
    "   visibility = SHADER_VISIBILITY_PIXEL" \
    ")"

ConstantBuffer<DRAW_COMMAND> cbv_draw_cmd : register(b0);
ConstantBuffer<GLOBALS> cbv_glob : register(b1);

StructuredBuffer<VERTEX> srv_vertices : register(t0);
Buffer<U32> srv_indices : register(t1);
StructuredBuffer<RENDERABLE_CONSTANTS> srv_const_renderables : register(t2);

Texture2D srv_ao_texture : register(t3);
Texture2D srv_base_color_texture : register(t4);
Texture2D srv_metallic_roughness_texture : register(t5);
Texture2D srv_normal_texture : register(t6);

TextureCube srv_irradiance_texture : register(t7);
TextureCube srv_prefiltered_env_texture : register(t8);
Texture2D srv_brdf_integration_texture : register(t9);

SamplerState sam_linear : register(s0);
SamplerState sam_prefiltered_env : register(s1);

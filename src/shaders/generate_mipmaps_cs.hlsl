#include "common.hlsli"

#define ROOT_SIGNATURE \
    "RootConstants(b0, num32BitConstants = 2), " \
    "DescriptorTable(SRV(t0), UAV(u0, numDescriptors = 4))"

Texture2D<XMFLOAT4> srv_src_mipmap : register(t0);
RWTexture2D<XMFLOAT4> uav_mipmap1 : register(u0);
RWTexture2D<XMFLOAT4> uav_mipmap2 : register(u1);
RWTexture2D<XMFLOAT4> uav_mipmap3 : register(u2);
RWTexture2D<XMFLOAT4> uav_mipmap4 : register(u3);

struct CONSTANTS {
    U32 src_mip_level;
    U32 num_mip_levels;
};
ConstantBuffer<CONSTANTS> cbv_const : register(b0);

groupshared F32 gs_red[64];
groupshared F32 gs_green[64];
groupshared F32 gs_blue[64];
groupshared F32 gs_alpha[64];

void Store_Color(U32 idx, XMFLOAT4 color) {
    gs_red[idx] = color.r;
    gs_green[idx] = color.g;
    gs_blue[idx] = color.b;
    gs_alpha[idx] = color.a;
}

XMFLOAT4 Load_Color(U32 idx) {
    return XMFLOAT4(gs_red[idx], gs_green[idx], gs_blue[idx], gs_alpha[idx]);
}

[RootSignature(ROOT_SIGNATURE)]
[numthreads(8, 8, 1)]
void Compute_Shader(
    XMUINT3 dispatch_id : SV_DispatchThreadID,
    U32 group_idx : SV_GroupIndex
) {
    const U32 x = dispatch_id.x * 2;
    const U32 y = dispatch_id.y * 2;

    XMFLOAT4 s00 = srv_src_mipmap.mips[cbv_const.src_mip_level][XMUINT2(x, y)];
    XMFLOAT4 s10 = srv_src_mipmap.mips[cbv_const.src_mip_level][XMUINT2(x + 1, y)];
    XMFLOAT4 s01 = srv_src_mipmap.mips[cbv_const.src_mip_level][XMUINT2(x, y + 1)];
    XMFLOAT4 s11 = srv_src_mipmap.mips[cbv_const.src_mip_level][XMUINT2(x + 1, y + 1)];
    s00 = 0.25f * (s00 + s01 + s10 + s11);

    uav_mipmap1[dispatch_id.xy] = s00;
    Store_Color(group_idx, s00);
    if (cbv_const.num_mip_levels == 1) {
        return;
    }
    GroupMemoryBarrierWithGroupSync();

    if ((group_idx & 0x9) == 0) {
        s10 = Load_Color(group_idx + 1);
        s01 = Load_Color(group_idx + 8);
        s11 = Load_Color(group_idx + 9);
        s00 = 0.25f * (s00 + s01 + s10 + s11);
        uav_mipmap2[dispatch_id.xy / 2] = s00;
        Store_Color(group_idx, s00);
    }
    if (cbv_const.num_mip_levels == 2) {
        return;
    }
    GroupMemoryBarrierWithGroupSync();

    if ((group_idx & 0x1B) == 0) {
        s10 = Load_Color(group_idx + 2);
        s01 = Load_Color(group_idx + 16);
        s11 = Load_Color(group_idx + 18);
        s00 = 0.25f * (s00 + s01 + s10 + s11);
        uav_mipmap3[dispatch_id.xy / 4] = s00;
        Store_Color(group_idx, s00);
    }
    if (cbv_const.num_mip_levels == 3) {
        return;
    }
    GroupMemoryBarrierWithGroupSync();

    if (group_idx == 0) {
        s10 = Load_Color(group_idx + 4);
        s01 = Load_Color(group_idx + 32);
        s11 = Load_Color(group_idx + 36);
        s00 = 0.25f * (s00 + s01 + s10 + s11);
        uav_mipmap4[dispatch_id.xy / 8] = s00;
        Store_Color(group_idx, s00);
    }
}

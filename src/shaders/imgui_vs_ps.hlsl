#include "common.hlsli"

#define ROOT_SIGNATURE \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
    "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \
    "DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL), " \
    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, visibility = SHADER_VISIBILITY_PIXEL)"

struct CONSTANTS {
    XMFLOAT4X4 screen_to_clip;
};
ConstantBuffer<CONSTANTS> cbv_b0 : register(b0);
Texture2D srv_t0 : register(t0);
SamplerState sam_s0 : register(s0);

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    XMFLOAT2 position : POSITION,
    XMFLOAT2 uv : _Uv,
    XMFLOAT4 color : _Color,
    out XMFLOAT4 out_position : SV_Position,
    out XMFLOAT2 out_uv : _Uv,
    out XMFLOAT4 out_color : _Color
) {
    out_position = mul(XMFLOAT4(position, 0.0f, 1.0f), cbv_b0.screen_to_clip);
    out_uv = uv;
    out_color = color;
}

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    XMFLOAT4 position : SV_Position,
    XMFLOAT2 uv : _Uv,
    XMFLOAT4 color : _Color,
    out XMFLOAT4 out_color : SV_Target0
) {
    color = pow(color, 2.2f);
    out_color = color * srv_t0.Sample(sam_s0, uv);
}

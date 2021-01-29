#include "common.hlsli"
#include "common_pbr_math.hlsli"

#define ROOT_SIGNATURE \
    "DescriptorTable(UAV(u0))"

RWTexture2D<XMFLOAT4> uav_brdf_integration_texture : register(u0);

XMFLOAT2 Integrate(F32 roughness, F32 n_dot_v) {
    XMFLOAT3 v;
    v.x = 0.0f;
    v.y = n_dot_v; // cos
    v.z = sqrt(1.0f - n_dot_v * n_dot_v); // sin

    const XMFLOAT3 n = XMFLOAT3(0.0f, 1.0f, 0.0f);

    F32 a = 0.0f;
    F32 b = 0.0f;
    const U32 num_samples = 4 * 1024;

    for (U32 sample_idx = 0; sample_idx < num_samples; ++sample_idx) {
        const XMFLOAT2 xi = Hammersley(sample_idx, num_samples);
        const XMFLOAT3 h = Importance_Sample_GGX(xi, roughness, n);
        const XMFLOAT3 l = normalize(2.0f * dot(v, h) * h - v);

        const F32 n_dot_l = saturate(l.y);
        const F32 n_dot_h = saturate(h.y);
        const F32 v_dot_h = saturate(dot(v, h));

        if (n_dot_l > 0.0f) {
            const F32 g = Geometry_Smith(n_dot_l, n_dot_v, roughness);
            const F32 g_vis = g * v_dot_h / (n_dot_h * n_dot_v);
            const F32 fc = pow(1.0f - v_dot_h, 5.0f);
            a += (1.0f - fc) * g_vis;
            b += fc * g_vis;
        }
    }
    return XMFLOAT2(a, b) / num_samples;
}

[RootSignature(ROOT_SIGNATURE)]
[numthreads(8, 8, 1)]
void Compute_Shader(XMUINT3 dispatch_id : SV_DispatchThreadID) {
    F32 width, height;
    uav_brdf_integration_texture.GetDimensions(width, height);

    const F32 roughness = (dispatch_id.y + 1) / height;
    const F32 n_dot_v = (dispatch_id.x + 1) / width;
    const XMFLOAT2 result = Integrate(roughness, n_dot_v);

    uav_brdf_integration_texture[dispatch_id.xy] = XMFLOAT4(result, 0.0f, 1.0f);
}

#include "common.hlsli"
#include "mesh_common.hlsli"
#include "../cpp_hlsl_common.h"

#define PI 3.1415926f

// Trowbridge-Reitz GGX normal distribution function.
F32 Distribution_GGX(XMFLOAT3 n, XMFLOAT3 h, F32 roughness) {
    const F32 alpha = roughness * roughness;
    const F32 alpha_sq = alpha * alpha;
    const F32 n_dot_h = dot(n, h);
    const F32 n_dot_h_sq = n_dot_h * n_dot_h;
    const F32 k = n_dot_h_sq * alpha_sq + (1.0f - n_dot_h_sq);
    return alpha_sq / (PI * k * k);
}

XMFLOAT3 Fresnel_Schlick(F32 cos_theta, XMFLOAT3 f0) {
    return f0 + (1.0f - f0) * pow(1.0f - cos_theta, 5.0f);
}

XMFLOAT3 Fresnel_Schlick_Roughness(F32 cos_theta, XMFLOAT3 f0, F32 roughness) {
    return f0 + (max(1.0f - roughness, f0) - f0) * pow(1.0f - cos_theta, 5.0f);
}

[RootSignature(ROOT_SIGNATURE)]
void Vertex_Shader(
    U32 vertex_id : SV_VertexID,
    out XMFLOAT4 out_position_ndc : SV_Position,
    out XMFLOAT3 out_position : _Position,
    out XMFLOAT3 out_normal : _Normal,
    out XMFLOAT4 out_tangent : _Tangent,
    out XMFLOAT2 out_uv : _Uv
) {
    const U32 vertex_index = srv_indices[vertex_id + cbv_draw_cmd.index_offset] +
        cbv_draw_cmd.vertex_offset;

    const VERTEX v = srv_vertices[vertex_index];

    const XMFLOAT4X4 object_to_world = srv_const_renderables[cbv_draw_cmd.renderable_id].object_to_world;
    const XMFLOAT4X4 object_to_clip = mul(object_to_world, cbv_glob.world_to_clip);

    out_position_ndc = mul(XMFLOAT4(v.position, 1.0f), object_to_clip);
    out_position = mul(v.position, (XMFLOAT3X3)object_to_world);
    out_normal = v.normal;
    out_tangent = v.tangent;
    out_uv = v.uv;
}

[RootSignature(ROOT_SIGNATURE)]
void Pixel_Shader(
    XMFLOAT4 position_ndc : SV_Position,
    XMFLOAT3 position : _Position,
    XMFLOAT3 normal : _Normal,
    XMFLOAT4 tangent : _Tangent,
    XMFLOAT2 uv : _Uv,
    out XMFLOAT4 out_color : SV_Target0
) {
    XMFLOAT3 n = normalize(srv_normal_texture.Sample(sam_linear, uv).rgb * 2.0f - 1.0f);

    //XMFLOAT3 nn = normalize(normal);
    //XMFLOAT3 tt = normalize(tangent.xyz - nn * dot(tangent.xyz, nn));
    //XMFLOAT3 bb = cross(normal, tangent.xyz) * tangent.w;
    //n = tt * n.x + bb * n.y + nn * n.z;

    normal = normalize(normal);
    tangent.xyz = normalize(tangent.xyz);
    const XMFLOAT3 bitangent = normalize(cross(normal, tangent.xyz)) * tangent.w;

    const XMFLOAT3X3 object_to_world =
        (XMFLOAT3X3)srv_const_renderables[cbv_draw_cmd.renderable_id].object_to_world;

    n = mul(n, XMFLOAT3X3(tangent.xyz, bitangent, normal));
    n = normalize(mul(n, object_to_world));

    //n = mul(n, XMFLOAT3X3(tangent.xyz, binormal, normal));
    //n = normalize(mul(n, object_to_world));
    //n = normalize(mul(normal, object_to_world));

    const XMFLOAT3 base_color = pow(srv_base_color_texture.Sample(sam_linear, uv).rgb, 2.2f);
    const XMFLOAT2 metallic_roughness = srv_metallic_roughness_texture.Sample(sam_linear, uv).rg;
    const F32 ao = srv_ao_texture.Sample(sam_linear, uv).r;

    const XMFLOAT3 v = normalize(cbv_glob.camera_position - position);
    const F32 n_dot_v = saturate(dot(n, v));

    out_color = XMFLOAT4(base_color, 1.0f);

    /*
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, Albedo, Metallic);

    float3 Lo = 0.0f;
    for (int LightIdx = 0; LightIdx < 4; ++LightIdx)
    {
        float3 LightVector = GPerFrameCB.LightPositions[LightIdx].xyz - InPositionWS;

        float3 L = normalize(LightVector);
        float3 H = normalize(L + V);
        float NoL = saturate(dot(N, L));
        float HoV = saturate(dot(H, V));

        float Attenuation = 1.0f / dot(LightVector, LightVector);
        float3 Radiance = GPerFrameCB.LightColors[LightIdx].rgb * Attenuation;

        float3 F = FresnelSchlick(HoV, F0);

        float NDF = DistributionGGX(N, H, Roughness);
        float G = GeometrySmith(NoL, NoV, (Roughness + 1.0f) * 0.5f);

        float3 Specular = (NDF * G * F) / max(4.0f * NoV * NoL, 0.001f);

        float3 KS = F;
        float3 KD = 1.0f - KS;
        KD *= 1.0f - Metallic;

        Lo += (KD * (Albedo / PI) + Specular) * Radiance * NoL;
    }

    float3 R = reflect(-V, N);

    float3 F = FresnelSchlickRoughness(NoV, F0, Roughness);

    float3 KD = 1.0f - F;
    KD *= 1.0f - Metallic;

    float3 Irradiance = GIrradianceMap.SampleLevel(GSampler, N, 0.0f).rgb;
    float3 Diffuse = Irradiance * Albedo;
    float3 PrefilteredColor = GPrefilteredEnvMap.SampleLevel(GSampler, R, Roughness * 5.0f).rgb;

    float2 EnvBRDF = GBRDFIntegrationMap.SampleLevel(GSampler, float2(min(NoV, 0.999f), Roughness), 0.0f).rg;

    float3 Specular = PrefilteredColor * (F * EnvBRDF.x + EnvBRDF.y);

    float3 Ambient = (KD * Diffuse + Specular) * AO;

    float3 Color = Ambient + Lo;
    Color = Color / (Color + 1.0f);
    Color = pow(Color, 1.0f / 2.2f);

    OutColor = float4(Color, 1.0f);
    */
}

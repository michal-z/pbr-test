#include "common.hlsli"
#include "mesh_common.hlsli"
#include "common_pbr_math.hlsli"
#include "../cpp_hlsl_common.h"

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
    if (cbv_glob.draw_mode == 1) {
        out_color = pow(srv_ao_texture.Sample(sam_linear, uv), 2.2f);
        return;
    } else if (cbv_glob.draw_mode == 2) {
        out_color = pow(srv_base_color_texture.Sample(sam_linear, uv), 2.2f);
        return;
    } else if (cbv_glob.draw_mode == 3) {
        out_color = pow(srv_metallic_roughness_texture.Sample(sam_linear, uv).b, 2.2f);
        return;
    } else if (cbv_glob.draw_mode == 4) {
        out_color = pow(srv_metallic_roughness_texture.Sample(sam_linear, uv).g, 2.2f);
        return;
    } else if (cbv_glob.draw_mode == 5) {
        out_color = pow(srv_normal_texture.Sample(sam_linear, uv), 2.2f);
        return;
    }

    XMFLOAT3 n = normalize(srv_normal_texture.Sample(sam_linear, uv).rgb * 2.0f - 1.0f);

    normal = normalize(normal);
    tangent.xyz = normalize(tangent.xyz);
    const XMFLOAT3 bitangent = normalize(cross(normal, tangent.xyz)) * tangent.w;

    const XMFLOAT3X3 object_to_world =
        (XMFLOAT3X3)srv_const_renderables[cbv_draw_cmd.renderable_id].object_to_world;

    n = mul(n, XMFLOAT3X3(tangent.xyz, bitangent, normal));
    n = normalize(mul(n, object_to_world));

    F32 metallic;
    F32 roughness;
    {
        const XMFLOAT2 mr = srv_metallic_roughness_texture.Sample(sam_linear, uv).bg;
        metallic = mr.r;
        roughness = mr.g;
    }
    const XMFLOAT3 base_color = pow(srv_base_color_texture.Sample(sam_linear, uv).rgb, 2.2f);
    const F32 ao = srv_ao_texture.Sample(sam_linear, uv).r;

    const XMFLOAT3 v = normalize(cbv_glob.camera_position - position);
    const F32 n_dot_v = saturate(dot(n, v));

    XMFLOAT3 f0 = XMFLOAT3(0.04f, 0.04f, 0.04f);
    f0 = lerp(f0, base_color, metallic);

    /*
    XMFLOAT3 radiance = 0.0f;
    for (U32 light_idx = 0; light_idx < 4; ++light_idx) {
        const XMFLOAT3 light_vec = cbv_glob.light_positions[light_idx].xyz - position;
        const F32 light_attenuation = 1.0f / dot(light_vec, light_vec);
        const XMFLOAT3 light_radiance = cbv_glob.light_colors[light_idx].rgb * light_attenuation;
        const XMFLOAT3 l = normalize(light_vec);
        const XMFLOAT3 h = normalize(l + v);
        const F32 n_dot_l = saturate(dot(n, l));
        const F32 h_dot_v = saturate(dot(h, v));
        const XMFLOAT3 f = Fresnel_Schlick(h_dot_v, f0);
        const F32 ndf = Distribution_GGX(n, h, roughness);
        const F32 g = Geometry_Smith(n_dot_l, n_dot_v, (roughness + 1.0f) * 0.5f);
        const XMFLOAT3 specular = (ndf * g * f) / max(4.0f * n_dot_v * n_dot_l, 0.001f);

        const XMFLOAT3 ks = f;
        XMFLOAT3 kd = 1.0f - ks;
        kd *= 1.0f - metallic;

        radiance += (kd * (base_color / PI) + specular) * light_radiance * n_dot_l;
    }
    */
    const XMFLOAT3 r = reflect(-v, n);
    const XMFLOAT3 f = Fresnel_Schlick_Roughness(n_dot_v, f0, roughness);

    const XMFLOAT3 kd = (1.0f - f) * (1.0f - metallic);

    const XMFLOAT3 irradiance = srv_irradiance_texture.SampleLevel(sam_linear, n, 0.0f).rgb;
    const XMFLOAT3 prefiltered_color = srv_prefiltered_env_texture.SampleLevel(
        sam_prefiltered_env,
        r,
        roughness * 5.0f // roughness * (num_mip_levels - 1.0f)
    ).rgb;
    const XMFLOAT2 env_brdf = srv_brdf_integration_texture.SampleLevel(
        sam_linear,
        XMFLOAT2(min(n_dot_v, 0.999f), roughness),
        0.0f
    ).rg;

    const XMFLOAT3 diffuse = irradiance * base_color;
    const XMFLOAT3 specular = prefiltered_color * (f * env_brdf.x + env_brdf.y);
    const XMFLOAT3 ambient = (kd * diffuse + specular) * ao;

    XMFLOAT3 color = ambient;// + radiance;
    color *= 3.0f;
    color = color / (color + 1.0f);

    out_color = XMFLOAT4(color, 1.0f);
}

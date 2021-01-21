#include "test_common.hlsli"

void Pixel_Shader(
    OUTPUT_VERTEX input,
    out FLOAT4 out_color : SV_Target0
) {
    const FLOAT3 normal = normalize(input.normal);

    const FLOAT3 light_positions[4] = {
        FLOAT3(50.0f, 25.0f, 0.0f),
        FLOAT3(-50.0f, 25.0f, 0.0f),
        FLOAT3(0.0f, 25.0f, -50.0f),
        FLOAT3(0.0f, 25.0f, 50.0f),
    };
    const FLOAT3 light_colors[4] = {
        FLOAT3(0.4f, 0.3f, 0.1f),
        FLOAT3(0.4f, 0.3f, 0.1f),
        FLOAT3(1.0f, 0.8f, 0.2f),
        FLOAT3(1.0f, 0.8f, 0.2f),
    };

    FLOAT3 color = 0.0f;
    for (UINT i = 0; i < 4; ++i) {
        const FLOAT3 l = normalize(light_positions[i] - input.position);
        const FLOAT n_dot_l = saturate(dot(normal, l));
        color += n_dot_l * light_colors[i];
    }
    color *= input.color;

    out_color = FLOAT4(0.05f + color, 1.0f);
}

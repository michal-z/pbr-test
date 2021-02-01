#pragma once

#define PI 3.14159265359f

F32 Radical_Inverse_VdC(U32 bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return (F32)bits * 2.3283064365386963e-10; // / 0x100000000
}

XMFLOAT2 Hammersley(U32 idx, U32 n) {
    return XMFLOAT2(idx / (F32)n, Radical_Inverse_VdC(idx));
}

XMFLOAT3 Importance_Sample_GGX(XMFLOAT2 xi, F32 roughness, XMFLOAT3 n) {
    const F32 alpha = roughness * roughness;
    const F32 phi = 2.0f * PI * xi.x;
    const F32 cos_theta = sqrt((1.0f - xi.y) / (1.0f + (alpha * alpha - 1.0f) * xi.y));
    const F32 sin_theta = sqrt(1.0f - cos_theta * cos_theta);

    XMFLOAT3 h;
    h.x = sin_theta * cos(phi);
    h.y = sin_theta * sin(phi);
    h.z = cos_theta;

    const XMFLOAT3 up_vector = abs(n.y) < 0.999f ? XMFLOAT3(0.0f, 1.0f, 0.0f) :
        XMFLOAT3(0.0f, 0.0f, 1.0f);
    const XMFLOAT3 tangent_x = normalize(cross(up_vector, n));
    const XMFLOAT3 tangent_y = cross(n, tangent_x);

    // Tangent to world space.
    return normalize(tangent_x * h.x + tangent_y * h.y + n * h.z);
}

F32 Geometry_Schlick_GGX(F32 cos_theta, F32 roughness) {
    const F32 k = (roughness * roughness) * 0.5f;
    return cos_theta / (cos_theta * (1.0f - k) + k);
}

F32 Geometry_Smith(F32 n_dot_l, F32 n_dot_v, F32 roughness) {
    return Geometry_Schlick_GGX(n_dot_v, roughness) * Geometry_Schlick_GGX(n_dot_l, roughness);
}

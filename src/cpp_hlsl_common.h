#pragma once

struct VERTEX {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 tangent;
    XMFLOAT2 uv;
};

struct GLOBALS {
    XMFLOAT4X4 world_to_clip;
};

struct RENDERABLE_CONSTANTS {
    XMFLOAT4X4 object_to_world;
};

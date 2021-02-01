#pragma once

struct VERTEX {
    XMFLOAT3 position;
    XMFLOAT3 normal;
    XMFLOAT4 tangent;
    XMFLOAT2 uv;
};

struct DRAW_COMMAND {
    U32 index_offset;
    U32 vertex_offset;
    U32 renderable_id;
};

struct GLOBALS {
    XMFLOAT4X4 world_to_clip;
    U32 draw_mode;
    XMFLOAT3 camera_position;
    XMFLOAT4 light_positions[4];
    XMFLOAT4 light_colors[4];
};

struct RENDERABLE_CONSTANTS {
    XMFLOAT4X4 object_to_world;
};

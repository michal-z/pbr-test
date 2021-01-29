module;
#include "pch.h"
export module demo;
import graphics;
import library;
namespace demo {

#include "cpp_hlsl_common.h"

constexpr U32 num_msaa_samples = 8;
constexpr U32 env_texture_resolution = 512;
constexpr U32 irradiance_texture_resolution = 64;
constexpr U32 prefiltered_env_texture_resolution = 256;
constexpr U32 prefiltered_env_texture_num_mip_levels = 6;

struct MESH {
    U32 index_offset;
    U32 vertex_offset;
    U32 num_indices;
};

struct RENDERABLE {
    MESH mesh;
    XMFLOAT3 position;
};

struct DEMO_STATE {
    graphics::CONTEXT graphics;
    library::FRAME_STATS frame_stats;
    library::IMGUI_CONTEXT gui;
    VECTOR<MESH> meshes;
    VECTOR<RENDERABLE> renderables;
    bool enable_draw_vectors;
    bool enable_dynamic_texture;
    S32 draw_mode;
    graphics::PIPELINE_HANDLE display_texture_pso;
    graphics::PIPELINE_HANDLE mesh_pso;
    graphics::PIPELINE_HANDLE mesh_debug_pso;
    graphics::PIPELINE_HANDLE sample_env_texture_pso;
    graphics::RESOURCE_HANDLE vertex_buffer;
    graphics::RESOURCE_HANDLE index_buffer;
    graphics::RESOURCE_HANDLE renderable_const_buffer;
    graphics::RESOURCE_HANDLE srgb_texture;
    graphics::RESOURCE_HANDLE depth_texture;
    graphics::RESOURCE_HANDLE dynamic_texture;
    graphics::RESOURCE_HANDLE mesh_textures[4];
    graphics::RESOURCE_HANDLE env_texture;
    graphics::RESOURCE_HANDLE irradiance_texture;
    graphics::RESOURCE_HANDLE prefiltered_env_texture;
    D3D12_CPU_DESCRIPTOR_HANDLE vertex_buffer_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE index_buffer_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE renderable_const_buffer_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE srgb_texture_rtv;
    D3D12_CPU_DESCRIPTOR_HANDLE depth_texture_dsv;
    D3D12_CPU_DESCRIPTOR_HANDLE dynamic_texture_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE mesh_textures_base_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE env_texture_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE irradiance_texture_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE prefiltered_env_texture_srv;
    struct {
        ID2D1_SOLID_COLOR_BRUSH* brush;
        IDWRITE_TEXT_FORMAT* text_format;
    } hud;
    struct {
        XMFLOAT3 position;
        XMFLOAT3 forward;
        F32 pitch;
        F32 yaw;
    } camera;
    struct {
        S32 cursor_prev_x;
        S32 cursor_prev_y;
    } mouse;
};

void Add_Mesh(
    LPCSTR filename,
    VECTOR<MESH>* meshes,
    VECTOR<VERTEX>* all_vertices,
    VECTOR<U32>* all_indices
) {
    assert(filename && meshes && all_vertices && all_indices);

    VECTOR<XMFLOAT3> positions;
    VECTOR<XMFLOAT3> normals;
    VECTOR<XMFLOAT4> tangents;
    VECTOR<XMFLOAT2> uvs;
    VECTOR<U32> indices;
    library::Load_Mesh(filename, &indices, &positions, &normals, &uvs, &tangents);
    assert(!normals.empty() && !uvs.empty() && !tangents.empty());

    VECTOR<VERTEX> vertices(positions.size());
    for (U32 i = 0; i < vertices.size(); ++i) {
        vertices[i] = { positions[i], normals[i], tangents[i], uvs[i] };
    }
    meshes->push_back({
        .index_offset = (U32)all_indices->size(),
        .vertex_offset = (U32)all_vertices->size(),
        .num_indices = (U32)indices.size(),
    });
    all_vertices->insert(all_vertices->end(), vertices.begin(), vertices.end());
    all_indices->insert(all_indices->end(), indices.begin(), indices.end());
}

void Create_And_Upload_Texture(
    LPCWSTR filename,
    graphics::CONTEXT* gr,
    library::MIPMAP_GENERATOR* mipgen,
    graphics::RESOURCE_HANDLE* texture,
    D3D12_CPU_DESCRIPTOR_HANDLE* texture_srv
) {
    assert(filename && gr && mipgen && texture && texture_srv);
    const auto [tex, srv] = graphics::Create_Texture_From_File(gr, filename);
    library::Generate_Mipmaps(mipgen, gr, tex);
    graphics::Add_Transition_Barrier(gr, tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    *texture = tex;
    *texture_srv = srv;
}

void Draw_To_Cube_Texture(
    graphics::CONTEXT* gr,
    graphics::RESOURCE_HANDLE texture,
    U32 target_mip_level
) {
    const D3D12_RESOURCE_DESC desc = graphics::Get_Resource_Desc(gr, texture);
    assert(target_mip_level < desc.MipLevels);
    const U32 texture_width = (U32)desc.Width >> target_mip_level;
    const U32 texture_height = desc.Height >> target_mip_level;
    assert(texture_width == texture_height);

    gr->cmdlist->RSSetViewports(
        1,
        Get_Const_Ptr(CD3DX12_VIEWPORT(0.0f, 0.0f, (F32)texture_width, (F32)texture_height))
    );
    gr->cmdlist->RSSetScissorRects(1, Get_Const_Ptr(CD3DX12_RECT(0, 0, texture_width, texture_height)));
    gr->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const auto zero = XMVectorZero();
    const XMMATRIX object_to_view[6] = {
        XMMatrixLookToLH(zero, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
        XMMatrixLookToLH(zero, XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
        XMMatrixLookToLH(zero, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)),
        XMMatrixLookToLH(zero, XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)),
        XMMatrixLookToLH(zero, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
        XMMatrixLookToLH(zero, XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)),
    };
    const XMMATRIX view_to_clip = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);

    for (U32 cube_face_idx = 0; cube_face_idx < 6; ++cube_face_idx) {
        const D3D12_CPU_DESCRIPTOR_HANDLE cube_face_rtv = graphics::Allocate_Temp_Cpu_Descriptors(
            gr,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            1
        );
        gr->device->CreateRenderTargetView(
            graphics::Get_Resource(gr, texture),
            Get_Const_Ptr<D3D12_RENDER_TARGET_VIEW_DESC>({
                .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY,
                .Texture2DArray = {
                    .MipSlice = target_mip_level,
                    .FirstArraySlice = cube_face_idx,
                    .ArraySize = 1,
                },
            }),
            cube_face_rtv
        );

        graphics::Add_Transition_Barrier(gr, texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
        graphics::Flush_Resource_Barriers(gr);
        gr->cmdlist->OMSetRenderTargets(1, &cube_face_rtv, TRUE, NULL);

        const auto [cpu_addr, gpu_addr] = graphics::Allocate_Upload_Memory(gr, sizeof XMFLOAT4X4A);
        XMStoreFloat4x4A(
            (XMFLOAT4X4A*)cpu_addr,
            XMMatrixTranspose(object_to_view[cube_face_idx] * view_to_clip)
        );
        gr->cmdlist->SetGraphicsRootConstantBufferView(3, gpu_addr);
        gr->cmdlist->DrawInstanced(36, 1, 0, 0); // 36 indices for cube mesh
    }
    graphics::Add_Transition_Barrier(gr, texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    graphics::Flush_Resource_Barriers(gr);
}

template<typename T> void Upload_To_Gpu(
    graphics::CONTEXT* gr,
    graphics::RESOURCE_HANDLE resource,
    const VECTOR<T>* data,
    D3D12_RESOURCE_STATES state
) {
    const auto [span, buffer, buffer_offset] = graphics::Allocate_Upload_Buffer_Region<T>(
        gr,
        (U32)data->size()
    );
    for (U32 i = 0; i < data->size(); ++i) {
        span[i] = (*data)[i];
    }
    gr->cmdlist->CopyBufferRegion(
        graphics::Get_Resource(gr, resource),
        0,
        buffer,
        buffer_offset,
        span.size_bytes()
    );
    graphics::Add_Transition_Barrier(gr, resource, state);
}

bool Init_Demo_State(DEMO_STATE* demo) {
    assert(demo);

    const HWND window = library::Create_Window("demo", 1920, 1080, /* init_imgui */ true);
    if (!graphics::Init_Context(&demo->graphics, window)) {
        return false;
    }
    graphics::CONTEXT* gr = &demo->graphics;

    {
        const VECTOR<U8> vs = library::Load_File("data/shaders/display_texture_vs_ps.vs.cso");
        const VECTOR<U8> ps = library::Load_File("data/shaders/display_texture_vs_ps.ps.cso");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
            .VS = { vs.data(), vs.size() },
            .PS = { ps.data(), ps.size() },
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT32_MAX,
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
            .SampleDesc = { .Count = num_msaa_samples, .Quality = 0 },
        };
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.DepthStencilState.DepthEnable = FALSE;
        desc.RasterizerState.MultisampleEnable = num_msaa_samples > 1 ? TRUE : FALSE;
        demo->display_texture_pso = graphics::Create_Graphics_Shader_Pipeline(gr, &desc);
    }
    {
        const VECTOR<U8> vs = library::Load_File("data/shaders/mesh_vs_ps.vs.cso");
        const VECTOR<U8> ps = library::Load_File("data/shaders/mesh_vs_ps.ps.cso");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
            .VS = { vs.data(), vs.size() },
            .PS = { ps.data(), ps.size() },
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT32_MAX,
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
            .DSVFormat = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = { .Count = num_msaa_samples, .Quality = 0 },
        };
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        desc.RasterizerState.MultisampleEnable = num_msaa_samples > 1 ? TRUE : FALSE;
        demo->mesh_pso = graphics::Create_Graphics_Shader_Pipeline(gr, &desc);
    }
    {
        const VECTOR<U8> vs = library::Load_File("data/shaders/mesh_debug_vs_ps.vs.cso");
        const VECTOR<U8> ps = library::Load_File("data/shaders/mesh_debug_vs_ps.ps.cso");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
            .VS = { vs.data(), vs.size() },
            .PS = { ps.data(), ps.size() },
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT32_MAX,
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
            .NumRenderTargets = 1,
            .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
            .DSVFormat = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = { .Count = num_msaa_samples, .Quality = 0 },
        };
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.MultisampleEnable = num_msaa_samples > 1 ? TRUE : FALSE;
        demo->mesh_debug_pso = graphics::Create_Graphics_Shader_Pipeline(gr, &desc);
    }
    {
        const VECTOR<U8> vs = library::Load_File("data/shaders/sample_env_texture_vs_ps.vs.cso");
        const VECTOR<U8> ps = library::Load_File("data/shaders/sample_env_texture_vs_ps.ps.cso");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
            .VS = { vs.data(), vs.size() },
            .PS = { ps.data(), ps.size() },
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT32_MAX,
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
            .DSVFormat = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = { .Count = num_msaa_samples, .Quality = 0 },
        };
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
        desc.RasterizerState.MultisampleEnable = num_msaa_samples > 1 ? TRUE : FALSE;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        demo->sample_env_texture_pso = graphics::Create_Graphics_Shader_Pipeline(gr, &desc);
    }

    VHR(gr->d2d.context->CreateSolidColorBrush({ 0.0f }, &demo->hud.brush));
    demo->hud.brush->SetColor({ .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f });

    VHR(gr->d2d.factory_dwrite->CreateTextFormat(
        L"Verdana",
        NULL,
        DWRITE_FONT_WEIGHT_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        32.0f,
        L"en-us",
        &demo->hud.text_format
    ));
    VHR(demo->hud.text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
    VHR(demo->hud.text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));

    VECTOR<VERTEX> all_vertices;
    VECTOR<U32> all_indices;
    {
        const LPCSTR mesh_paths[] = {
            "data/SciFiHelmet/SciFiHelmet.gltf",
            "data/cube.gltf",
        };
        for (U32 i = 0; i < eastl::size(mesh_paths); ++i) {
            Add_Mesh(mesh_paths[i], &demo->meshes, &all_vertices, &all_indices);
        }
    }
    demo->renderables.push_back({ .mesh = demo->meshes[0], .position = { 0.0f, 0.0f, 0.0f } });

    // Create one global vertex buffer for all static geometry.
    demo->vertex_buffer = graphics::Create_Committed_Resource(
        gr,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_HEAP_FLAG_NONE,
        Get_Const_Ptr(CD3DX12_RESOURCE_DESC::Buffer(all_vertices.size() * sizeof VERTEX)),
        D3D12_RESOURCE_STATE_COPY_DEST,
        NULL
    );
    demo->vertex_buffer_srv = graphics::Allocate_Cpu_Descriptors(
        gr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        1
    );
    gr->device->CreateShaderResourceView(
        graphics::Get_Resource(gr, demo->vertex_buffer),
        Get_Const_Ptr<D3D12_SHADER_RESOURCE_VIEW_DESC>({
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer = {
                .FirstElement = 0,
                .NumElements = (U32)all_vertices.size(),
                .StructureByteStride = sizeof VERTEX,
            }
        }),
        demo->vertex_buffer_srv
    );
    // Create one global index buffer for all static geometry.
    demo->index_buffer = graphics::Create_Committed_Resource(
        gr,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_HEAP_FLAG_NONE,
        Get_Const_Ptr(CD3DX12_RESOURCE_DESC::Buffer(all_indices.size() * sizeof U32)),
        D3D12_RESOURCE_STATE_COPY_DEST,
        NULL
    );
    demo->index_buffer_srv = graphics::Allocate_Cpu_Descriptors(
        gr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        1
    );
    gr->device->CreateShaderResourceView(
        graphics::Get_Resource(gr, demo->index_buffer),
        Get_Const_Ptr<D3D12_SHADER_RESOURCE_VIEW_DESC>({
            .Format = DXGI_FORMAT_R32_UINT,
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer = { .NumElements = (U32)all_indices.size() },
        }),
        demo->index_buffer_srv
    );
    // Create structured buffer containing constants for each renderable object.
    demo->renderable_const_buffer = graphics::Create_Committed_Resource(
        gr,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_HEAP_FLAG_NONE,
        Get_Const_Ptr(
            CD3DX12_RESOURCE_DESC::Buffer(demo->renderables.size() * sizeof RENDERABLE_CONSTANTS)
        ),
        D3D12_RESOURCE_STATE_COPY_DEST,
        NULL
    );
    demo->renderable_const_buffer_srv = graphics::Allocate_Cpu_Descriptors(
        gr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        1
    );
    gr->device->CreateShaderResourceView(
        graphics::Get_Resource(gr, demo->renderable_const_buffer),
        Get_Const_Ptr<D3D12_SHADER_RESOURCE_VIEW_DESC>({
            .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Buffer = {
                .FirstElement = 0,
                .NumElements = (U32)demo->renderables.size(),
                .StructureByteStride = sizeof RENDERABLE_CONSTANTS,
            }
        }),
        demo->renderable_const_buffer_srv
    );

    // Create srgb color texture.
    {
        auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            gr->viewport_width,
            gr->viewport_height
        );
        desc.MipLevels = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        desc.SampleDesc.Count = num_msaa_samples;
        demo->srgb_texture = graphics::Create_Committed_Resource(
            gr,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            Get_Const_Ptr(CD3DX12_CLEAR_VALUE(desc.Format, XMVECTORF32{ 0.0f, 0.0f, 0.0f, 1.0f }))
        );
        demo->srgb_texture_rtv = graphics::Allocate_Cpu_Descriptors(
            gr,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            1
        );
        gr->device->CreateRenderTargetView(
            graphics::Get_Resource(gr, demo->srgb_texture),
            NULL,
            demo->srgb_texture_rtv
        );
    }
    // Create depth texture.
    {
        auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_D32_FLOAT,
            gr->viewport_width,
            gr->viewport_height
        );
        desc.MipLevels = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        desc.SampleDesc.Count = num_msaa_samples;
        demo->depth_texture = graphics::Create_Committed_Resource(
            gr,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            Get_Const_Ptr(CD3DX12_CLEAR_VALUE(desc.Format, 1.0f, 0))
        );
        demo->depth_texture_dsv = graphics::Allocate_Cpu_Descriptors(
            gr,
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            1
        );
        gr->device->CreateDepthStencilView(
            graphics::Get_Resource(gr, demo->depth_texture),
            NULL,
            demo->depth_texture_dsv
        );
    }
    // Create dynamic texture.
    {
        demo->dynamic_texture = graphics::Create_Committed_Resource(
            gr,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_HEAP_FLAG_NONE,
            Get_Const_Ptr(CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 16, 16, 1, 1)),
            D3D12_RESOURCE_STATE_COPY_DEST,
            NULL
        );
        demo->dynamic_texture_srv = graphics::Allocate_Cpu_Descriptors(
            gr,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            1
        );
        gr->device->CreateShaderResourceView(
            graphics::Get_Resource(gr, demo->dynamic_texture),
            NULL,
            demo->dynamic_texture_srv
        );
    }
    // Create env. cube texture.
    demo->env_texture = graphics::Create_Committed_Resource(
        gr,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_HEAP_FLAG_NONE,
        Get_Const_Ptr<D3D12_RESOURCE_DESC>({
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = env_texture_resolution,
            .Height = env_texture_resolution,
            .DepthOrArraySize = 6,
            .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .SampleDesc = { .Count = 1 },
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        }),
        D3D12_RESOURCE_STATE_COPY_DEST,
        NULL
    );
    demo->env_texture_srv = graphics::Allocate_Cpu_Descriptors(
        gr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        1
    );
    gr->device->CreateShaderResourceView(
        graphics::Get_Resource(gr, demo->env_texture),
        Get_Const_Ptr<D3D12_SHADER_RESOURCE_VIEW_DESC>({
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .TextureCube = { .MipLevels = (U32)-1 },
        }),
        demo->env_texture_srv
    );
    // Create irradiance cube texture.
    demo->irradiance_texture = graphics::Create_Committed_Resource(
        gr,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_HEAP_FLAG_NONE,
        Get_Const_Ptr<D3D12_RESOURCE_DESC>({
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = irradiance_texture_resolution,
            .Height = irradiance_texture_resolution,
            .DepthOrArraySize = 6,
            .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .SampleDesc = { .Count = 1 },
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        }),
        D3D12_RESOURCE_STATE_COPY_DEST,
        NULL
    );
    demo->irradiance_texture_srv = graphics::Allocate_Cpu_Descriptors(
        gr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        1
    );
    gr->device->CreateShaderResourceView(
        graphics::Get_Resource(gr, demo->irradiance_texture),
        Get_Const_Ptr<D3D12_SHADER_RESOURCE_VIEW_DESC>({
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .TextureCube = { .MipLevels = (U32)-1 },
        }),
        demo->irradiance_texture_srv
    );
    // Create prefiltered env. cube texture.
    demo->prefiltered_env_texture = graphics::Create_Committed_Resource(
        gr,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_HEAP_FLAG_NONE,
        Get_Const_Ptr<D3D12_RESOURCE_DESC>({
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Width = prefiltered_env_texture_resolution,
            .Height = prefiltered_env_texture_resolution,
            .DepthOrArraySize = 6,
            .MipLevels = prefiltered_env_texture_num_mip_levels,
            .Format = DXGI_FORMAT_R16G16B16A16_FLOAT,
            .SampleDesc = { .Count = 1 },
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
        }),
        D3D12_RESOURCE_STATE_COPY_DEST,
        NULL
    );
    demo->prefiltered_env_texture_srv = graphics::Allocate_Cpu_Descriptors(
        gr,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        1
    );
    gr->device->CreateShaderResourceView(
        graphics::Get_Resource(gr, demo->prefiltered_env_texture),
        Get_Const_Ptr<D3D12_SHADER_RESOURCE_VIEW_DESC>({
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .TextureCube = { .MipLevels = prefiltered_env_texture_num_mip_levels },
        }),
        demo->prefiltered_env_texture_srv
    );

    // Create temporary pipelines for generating cube textures content (env., irradiance, etc.).
    graphics::PIPELINE_HANDLE generate_env_texture_pso = {};
    graphics::PIPELINE_HANDLE generate_irradiance_texture_pso = {};
    graphics::PIPELINE_HANDLE generate_prefiltered_env_texture_pso = {};
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT32_MAX,
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats = { DXGI_FORMAT_R16G16B16A16_FLOAT },
            .SampleDesc = { .Count = 1 },
        };
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
        desc.DepthStencilState.DepthEnable = FALSE;

        VECTOR<U8> vs = library::Load_File("data/shaders/generate_env_texture_vs_ps.vs.cso");
        VECTOR<U8> ps = library::Load_File("data/shaders/generate_env_texture_vs_ps.ps.cso");
        desc.VS = { vs.data(), vs.size() };
        desc.PS = { ps.data(), ps.size() };
        generate_env_texture_pso = graphics::Create_Graphics_Shader_Pipeline(gr, &desc);

        vs = library::Load_File("data/shaders/generate_irradiance_texture_vs_ps.vs.cso");
        ps = library::Load_File("data/shaders/generate_irradiance_texture_vs_ps.ps.cso");
        desc.VS = { vs.data(), vs.size() };
        desc.PS = { ps.data(), ps.size() };
        generate_irradiance_texture_pso = graphics::Create_Graphics_Shader_Pipeline(gr, &desc);

        vs = library::Load_File("data/shaders/generate_prefiltered_env_texture_vs_ps.vs.cso");
        ps = library::Load_File("data/shaders/generate_prefiltered_env_texture_vs_ps.ps.cso");
        desc.VS = { vs.data(), vs.size() };
        desc.PS = { ps.data(), ps.size() };
        generate_prefiltered_env_texture_pso = graphics::Create_Graphics_Shader_Pipeline(gr, &desc);
    }

    library::MIPMAP_GENERATOR mipgen_rgba8 = {};
    library::MIPMAP_GENERATOR mipgen_rgba16f = {};
    library::Init_Mipmap_Generator(&mipgen_rgba8, gr, DXGI_FORMAT_R8G8B8A8_UNORM);
    library::Init_Mipmap_Generator(&mipgen_rgba16f, gr, DXGI_FORMAT_R16G16B16A16_FLOAT);

    graphics::Begin_Frame(gr);

    library::Init_Gui_Context(&demo->gui, gr, num_msaa_samples);

    // Create and upload textures.
    {
        LPCWSTR names[] = {
            L"data/SciFiHelmet/SciFiHelmet_AmbientOcclusion.png",
            L"data/SciFiHelmet/SciFiHelmet_BaseColor.png",
            L"data/SciFiHelmet/SciFiHelmet_MetallicRoughness.png",
            L"data/SciFiHelmet/SciFiHelmet_Normal.png",
        };
        for (U32 i = 0; i < eastl::size(names); ++i) {
            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            Create_And_Upload_Texture(
                names[i],
                gr,
                &mipgen_rgba8,
                &demo->mesh_textures[i],
                &handle
            );
            if (i == 0) {
                demo->mesh_textures_base_srv = handle;
            }
        }
    }

    // Upload vertices.
    Upload_To_Gpu(gr, demo->vertex_buffer, &all_vertices, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // Upload indices.
    Upload_To_Gpu(gr, demo->index_buffer, &all_indices, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // Upload equirectangular HDR image to temporary 2D texture.
    graphics::RESOURCE_HANDLE equirectangular_texture = {};
    D3D12_CPU_DESCRIPTOR_HANDLE equirectangular_texture_srv = {};
    {
        S32 width, height;
        stbi_set_flip_vertically_on_load(1);
        void* image_data = stbi_loadf("data/Newport_Loft.hdr", &width, &height, NULL, 3);

        equirectangular_texture = graphics::Create_Committed_Resource(
            gr,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_HEAP_FLAG_NONE,
            Get_Const_Ptr(CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32_FLOAT, width, height)),
            D3D12_RESOURCE_STATE_COPY_DEST,
            NULL
        );
        equirectangular_texture_srv = graphics::Allocate_Cpu_Descriptors(
            gr,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            1
        );
        gr->device->CreateShaderResourceView(
            graphics::Get_Resource(gr, equirectangular_texture),
            NULL,
            equirectangular_texture_srv
        );
        graphics::Update_Tex2D_Subresource(
            gr,
            equirectangular_texture,
            0,
            image_data,
            width * sizeof XMFLOAT3
        );
        stbi_image_free(image_data);

        graphics::Add_Transition_Barrier(
            gr,
            equirectangular_texture,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        graphics::Flush_Resource_Barriers(gr);
    }

    // Generate env. cube texture from equirectangular HDR image.
    graphics::Set_Pipeline_State(gr, generate_env_texture_pso);
    {
        const MESH cube = demo->meshes[1];
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, cube.index_offset, 0);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, cube.vertex_offset, 1);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, 0, 2); // Set 'renderable_id' to 0.

        const auto base = graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->vertex_buffer_srv);
        graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->index_buffer_srv);
        gr->cmdlist->SetGraphicsRootDescriptorTable(1, base);
    }
    gr->cmdlist->SetGraphicsRootDescriptorTable(
        2,
        graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, equirectangular_texture_srv)
    );
    Draw_To_Cube_Texture(gr, demo->env_texture, 0);
    library::Generate_Mipmaps(&mipgen_rgba16f, gr, demo->env_texture);

    // Generate irradiance cube texture.
    graphics::Set_Pipeline_State(gr, generate_irradiance_texture_pso);
    {
        const MESH cube = demo->meshes[1];
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, cube.index_offset, 0);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, cube.vertex_offset, 1);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, 0, 2); // Set 'renderable_id' to 0.

        const auto base = graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->vertex_buffer_srv);
        graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->index_buffer_srv);
        gr->cmdlist->SetGraphicsRootDescriptorTable(1, base);
    }
    gr->cmdlist->SetGraphicsRootDescriptorTable(
        2,
        graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->env_texture_srv)
    );
    Draw_To_Cube_Texture(gr, demo->irradiance_texture, 0);
    library::Generate_Mipmaps(&mipgen_rgba16f, gr, demo->irradiance_texture);

    // Generate prefiltered env. cube texture.
    graphics::Set_Pipeline_State(gr, generate_prefiltered_env_texture_pso);
    {
        const MESH cube = demo->meshes[1];
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, cube.index_offset, 0);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, cube.vertex_offset, 1);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, 0, 2); // Set 'renderable_id' to 0.

        const auto base = graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->vertex_buffer_srv);
        graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->index_buffer_srv);
        gr->cmdlist->SetGraphicsRootDescriptorTable(1, base);
    }
    gr->cmdlist->SetGraphicsRootDescriptorTable(
        2,
        graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->env_texture_srv)
    );
    for (U32 mip_level = 0; mip_level < prefiltered_env_texture_num_mip_levels; ++mip_level) {
        const F32 roughness = (F32)mip_level / (prefiltered_env_texture_num_mip_levels - 1);
        union { F32 f; U32 u; } fu = { .f = roughness };
        gr->cmdlist->SetGraphicsRoot32BitConstant(4, fu.u, 0);
        Draw_To_Cube_Texture(gr, demo->prefiltered_env_texture, mip_level);
    }

    // Flush commands and wait for GPU to complete them.
    graphics::Flush_Gpu_Commands(gr);
    graphics::Finish_Gpu_Commands(gr);

    // Release temporary resources.
    library::Deinit_Mipmap_Generator(&mipgen_rgba8, gr);
    library::Deinit_Mipmap_Generator(&mipgen_rgba16f, gr);
    graphics::Release_Pipeline(gr, generate_env_texture_pso);
    graphics::Release_Pipeline(gr, generate_irradiance_texture_pso);
    graphics::Release_Pipeline(gr, generate_prefiltered_env_texture_pso);
    graphics::Release_Resource(gr, equirectangular_texture);
    graphics::Deallocate_Temp_Cpu_Descriptors(gr, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    library::Init_Frame_Stats(&demo->frame_stats);

    demo->camera = { .position = XMFLOAT3(0.0f, 3.0f, -3.0f), .pitch = XM_PIDIV4, .yaw = 0.0f };
    demo->mouse = { .cursor_prev_x = 0, .cursor_prev_y = 0 };
    return true;
}

void Deinit_Demo_State(DEMO_STATE* demo) {
    assert(demo);
    graphics::CONTEXT* gr = &demo->graphics;

    graphics::Finish_Gpu_Commands(gr);
    library::Deinit_Gui_Context(&demo->gui, gr);
    ImGui::DestroyContext();
    MZ_SAFE_RELEASE(demo->hud.brush);
    MZ_SAFE_RELEASE(demo->hud.text_format);
    graphics::Release_Resource(gr, demo->vertex_buffer);
    graphics::Release_Resource(gr, demo->index_buffer);
    graphics::Release_Resource(gr, demo->renderable_const_buffer);
    graphics::Release_Resource(gr, demo->srgb_texture);
    graphics::Release_Resource(gr, demo->depth_texture);
    graphics::Release_Resource(gr, demo->dynamic_texture);
    graphics::Release_Resource(gr, demo->env_texture);
    graphics::Release_Resource(gr, demo->irradiance_texture);
    graphics::Release_Resource(gr, demo->prefiltered_env_texture);
    for (U32 i = 0; i < eastl::size(demo->mesh_textures); ++i) {
        graphics::Release_Resource(gr, demo->mesh_textures[i]);
    }
    graphics::Release_Pipeline(gr, demo->display_texture_pso);
    graphics::Release_Pipeline(gr, demo->mesh_pso);
    graphics::Release_Pipeline(gr, demo->mesh_debug_pso);
    graphics::Release_Pipeline(gr, demo->sample_env_texture_pso);
    graphics::Deinit_Context(gr);
}

void Update_Demo_State(DEMO_STATE* demo) {
    assert(demo);

    library::Update_Frame_Stats(&demo->frame_stats);
    library::Update_Gui(demo->frame_stats.delta_time);

    ImGui::SetNextWindowPos(
        ImVec2(demo->graphics.viewport_width - 600.0f - 20.0f, 20.0f),
        ImGuiCond_FirstUseEver
    );
    ImGui::SetNextWindowSize(ImVec2(600.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(
        "Demo Settings",
        NULL,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
    );
    ImGui::Checkbox("Draw tangent, normal and bi-normal vectors", &demo->enable_draw_vectors);
    ImGui::Checkbox("Draw dynamic texture", &demo->enable_dynamic_texture);
    ImGui::Separator();
    ImGui::Text("Draw mode:");
    ImGui::Indent();
    ImGui::RadioButton("Draw PBR effect", &demo->draw_mode, 0);
    ImGui::RadioButton("Draw Ambient Occlusion texture", &demo->draw_mode, 1);
    ImGui::RadioButton("Draw Base Color texture", &demo->draw_mode, 2);
    ImGui::RadioButton("Draw Metallic Roughness texture", &demo->draw_mode, 3);
    ImGui::RadioButton("Draw Normal texture", &demo->draw_mode, 4);
    ImGui::Unindent();
    ImGui::End();

    // Handle camera rotation with mouse.
    {
        POINT pos = {};
        GetCursorPos(&pos);
        const F32 delta_x = (F32)pos.x - demo->mouse.cursor_prev_x;
        const F32 delta_y = (F32)pos.y - demo->mouse.cursor_prev_y;
        demo->mouse.cursor_prev_x = pos.x;
        demo->mouse.cursor_prev_y = pos.y;

        if (GetAsyncKeyState(VK_RBUTTON) < 0) {
            demo->camera.pitch += 0.0025f * delta_y;
            demo->camera.yaw += 0.0025f * delta_x;
            demo->camera.pitch = XMMin(demo->camera.pitch, 0.48f * XM_PI);
            demo->camera.pitch = XMMax(demo->camera.pitch, -0.48f * XM_PI);
            demo->camera.yaw = XMScalarModAngle(demo->camera.yaw);
        }
    }
    // Handle camera movement with 'WASD' keys.
    {
        const F32 speed = 5.0f;
        const F32 delta_time = demo->frame_stats.delta_time;
        const XMMATRIX transform = XMMatrixRotationX(demo->camera.pitch) *
            XMMatrixRotationY(demo->camera.yaw);
        XMVECTOR forward = XMVector3Normalize(
            XMVector3Transform(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), transform)
        );
        XMStoreFloat3(&demo->camera.forward, forward);
        const XMVECTOR right = speed * delta_time * XMVector3Normalize(
            XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward)
        );
        forward = speed * delta_time * forward;

        if (GetAsyncKeyState('W') < 0) {
            const XMVECTOR newpos = XMLoadFloat3(&demo->camera.position) + forward;
            XMStoreFloat3(&demo->camera.position, newpos);
        } else if (GetAsyncKeyState('S') < 0) {
            const XMVECTOR newpos = XMLoadFloat3(&demo->camera.position) - forward;
            XMStoreFloat3(&demo->camera.position, newpos);
        }
        if (GetAsyncKeyState('D') < 0) {
            const XMVECTOR newpos = XMLoadFloat3(&demo->camera.position) + right;
            XMStoreFloat3(&demo->camera.position, newpos);
        } else if (GetAsyncKeyState('A') < 0) {
            const XMVECTOR newpos = XMLoadFloat3(&demo->camera.position) - right;
            XMStoreFloat3(&demo->camera.position, newpos);
        }
    }

    graphics::CONTEXT* gr = &demo->graphics;

    graphics::Begin_Frame(gr);

    graphics::Add_Transition_Barrier(gr, demo->srgb_texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
    graphics::Flush_Resource_Barriers(gr);

    gr->cmdlist->OMSetRenderTargets(1, &demo->srgb_texture_rtv, TRUE, &demo->depth_texture_dsv);
    gr->cmdlist->ClearDepthStencilView(demo->depth_texture_dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
    gr->cmdlist->ClearRenderTargetView(
        demo->srgb_texture_rtv,
        XMVECTORF32{ 0.0f, 0.0f, 0.0f, 1.0f },
        0,
        NULL
    );

    D3D12_GPU_VIRTUAL_ADDRESS glob_buffer_addr = {};

    const XMMATRIX camera_world_to_view = XMMatrixLookToLH(
        XMLoadFloat3(&demo->camera.position),
        XMLoadFloat3(&demo->camera.forward),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );
    const XMMATRIX camera_view_to_clip = XMMatrixPerspectiveFovLH(
        XM_PI / 3.0f,
        (F32)gr->viewport_width / gr->viewport_height,
        0.1f,
        100.0f
    );

    // Upload 'GLOBALS' data.
    {
        const auto [cpu_addr, gpu_addr] = graphics::Allocate_Upload_Memory(gr, sizeof GLOBALS);
        glob_buffer_addr = gpu_addr;

        const XMMATRIX world_to_clip = camera_world_to_view * camera_view_to_clip;
        GLOBALS* globals = (GLOBALS*)cpu_addr;
        XMStoreFloat4x4(&globals->world_to_clip, XMMatrixTranspose(world_to_clip));
        globals->draw_mode = demo->draw_mode;
    }
    // Upload 'RENDERABLE_CONSTANTS' data.
    {
        graphics::Add_Transition_Barrier(
            gr,
            demo->renderable_const_buffer,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        graphics::Flush_Resource_Barriers(gr);
        const auto [span, buffer, buffer_offset] =
            graphics::Allocate_Upload_Buffer_Region<RENDERABLE_CONSTANTS>(
                gr,
                (U32)demo->renderables.size()
            );
        for (U32 i = 0; i < demo->renderables.size(); ++i) {
            const XMVECTOR pos = XMLoadFloat3(&demo->renderables[i].position);
            XMStoreFloat4x4(
                &span[i].object_to_world,
                XMMatrixTranspose(XMMatrixTranslationFromVector(pos))
            );
        }
        gr->cmdlist->CopyBufferRegion(
            graphics::Get_Resource(gr, demo->renderable_const_buffer),
            0,
            buffer,
            buffer_offset,
            span.size_bytes()
        );
        graphics::Add_Transition_Barrier(
            gr,
            demo->renderable_const_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );
        graphics::Flush_Resource_Barriers(gr);
    }

    const auto buffer_table_base = graphics::Copy_Descriptors_To_Gpu_Heap(
        gr,
        1,
        demo->vertex_buffer_srv
    );
    graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->index_buffer_srv);
    graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->renderable_const_buffer_srv);

    const auto texture_table_base = graphics::Copy_Descriptors_To_Gpu_Heap(
        gr,
        (U32)eastl::size(demo->mesh_textures),
        demo->mesh_textures_base_srv
    );

    graphics::Set_Pipeline_State(gr, demo->mesh_pso);
    gr->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    gr->cmdlist->SetGraphicsRootConstantBufferView(1, glob_buffer_addr);
    gr->cmdlist->SetGraphicsRootDescriptorTable(2, buffer_table_base);
    gr->cmdlist->SetGraphicsRootDescriptorTable(3, texture_table_base);
    for (U32 i = 0; i < demo->renderables.size(); ++i) {
        const RENDERABLE* renderable = &demo->renderables[i];
        gr->cmdlist->SetGraphicsRoot32BitConstants(
            0,
            3,
            Get_Const_Ptr<XMUINT3>({
                renderable->mesh.index_offset,
                renderable->mesh.vertex_offset,
                i, // renderable_id
            }),
            0
        );
        gr->cmdlist->DrawInstanced(renderable->mesh.num_indices, 1, 0, 0);
    }

    if (demo->enable_draw_vectors) {
        graphics::Set_Pipeline_State(gr, demo->mesh_debug_pso);
        gr->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        gr->cmdlist->SetGraphicsRootConstantBufferView(1, glob_buffer_addr);
        gr->cmdlist->SetGraphicsRootDescriptorTable(2, buffer_table_base);
        gr->cmdlist->SetGraphicsRootDescriptorTable(3, texture_table_base);
        for (U32 i = 0; i < demo->renderables.size(); ++i) {
            const RENDERABLE* renderable = &demo->renderables[i];
            gr->cmdlist->SetGraphicsRoot32BitConstants(
                0,
                3,
                Get_Const_Ptr<XMUINT3>({
                    renderable->mesh.index_offset,
                    renderable->mesh.vertex_offset,
                    i, // renderable_id
                }),
                0
            );
            gr->cmdlist->DrawInstanced(renderable->mesh.num_indices * 6, 1, 0, 0);
        }
    }

    // Draw env. cube texture.
    {
        const MESH cube = demo->meshes[1];
        graphics::Set_Pipeline_State(gr, demo->sample_env_texture_pso);
        gr->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, cube.index_offset, 0);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, cube.vertex_offset, 1);
        gr->cmdlist->SetGraphicsRoot32BitConstant(0, 0, 2); // Set 'renderable_id' to 0.
        {
            // Bind vertex and index buffers.
            const auto base = graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->vertex_buffer_srv);
            graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->index_buffer_srv);
            gr->cmdlist->SetGraphicsRootDescriptorTable(1, base);
        }
        gr->cmdlist->SetGraphicsRootDescriptorTable(
            2,
            graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->env_texture_srv)
        );
        XMMATRIX world_to_view_origin = camera_world_to_view;
        world_to_view_origin.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        const XMMATRIX object_to_clip = world_to_view_origin * camera_view_to_clip;

        const auto [cpu_addr, gpu_addr] = graphics::Allocate_Upload_Memory(gr, sizeof XMFLOAT4X4A);
        XMStoreFloat4x4A((XMFLOAT4X4A*)cpu_addr, XMMatrixTranspose(object_to_clip));
        gr->cmdlist->SetGraphicsRootConstantBufferView(3, gpu_addr);
        gr->cmdlist->DrawInstanced(cube.num_indices, 1, 0, 0);
    }

    if (demo->enable_dynamic_texture) {
        graphics::Add_Transition_Barrier(gr, demo->dynamic_texture, D3D12_RESOURCE_STATE_COPY_DEST);
        graphics::Flush_Resource_Barriers(gr);

        XMUBYTE4 data[256] = {};
        for (U32 i = 0; i < 256; ++i) {
            data[i].x = (U8)(255.0f * (rand() / (F32)RAND_MAX));
            data[i].y = (U8)(255.0f * (rand() / (F32)RAND_MAX));
            data[i].z = (U8)(255.0f * (rand() / (F32)RAND_MAX));
            data[i].w = 255;
        }
        Update_Tex2D_Subresource(gr, demo->dynamic_texture, 0, data, 64);

        graphics::Add_Transition_Barrier(
            gr,
            demo->dynamic_texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        graphics::Flush_Resource_Barriers(gr);

        graphics::Set_Pipeline_State(gr, demo->display_texture_pso);
        gr->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        gr->cmdlist->SetGraphicsRootDescriptorTable(
            0,
            graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->dynamic_texture_srv)
        );
        gr->cmdlist->DrawInstanced(4, 1, 0, 0);
    }

    library::Draw_Gui(&demo->gui, gr);

    const auto [back_buffer, back_buffer_rtv] = graphics::Get_Back_Buffer(gr);
    graphics::Add_Transition_Barrier(gr, back_buffer, D3D12_RESOURCE_STATE_RESOLVE_DEST);
    graphics::Add_Transition_Barrier(gr, demo->srgb_texture, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
    graphics::Flush_Resource_Barriers(gr);

    gr->cmdlist->ResolveSubresource(
        graphics::Get_Resource(gr, back_buffer),
        0,
        graphics::Get_Resource(gr, demo->srgb_texture),
        0,
        DXGI_FORMAT_R8G8B8A8_UNORM
    );
    graphics::Add_Transition_Barrier(gr, back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
    graphics::Flush_Resource_Barriers(gr);

    graphics::Begin_Draw_2D(gr);
    {
        WCHAR text[128];
        const S32 len = swprintf(
            text,
            eastl::size(text),
            L"FPS: %.1f\nFrame time: %.3f ms",
            demo->frame_stats.fps,
            demo->frame_stats.average_cpu_time
        );
        assert(len > 0);

        gr->d2d.context->DrawText(
            text,
            len,
            demo->hud.text_format,
            D2D1_RECT_F{
                .left = 4.0f,
                .top = 4.0f,
                .right = (F32)gr->viewport_width,
                .bottom = (F32)gr->viewport_height,
            },
            demo->hud.brush
        );
    }
    graphics::End_Draw_2D(gr);

    graphics::End_Frame(gr);
}

export bool Run() {
    SetProcessDPIAware();

    DEMO_STATE demo = {};
    if (!Init_Demo_State(&demo)) {
        return false;
    }

    for (;;) {
        MSG message = {};
        if (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            DispatchMessage(&message);
            if (message.message == WM_QUIT) {
                break;
            }
        } else {
            Update_Demo_State(&demo);
        }
    }

    Deinit_Demo_State(&demo);
    return true;
}

} // namespace demo

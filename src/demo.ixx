module;
#include "pch.h"
export module demo;
import graphics;
import library;
namespace demo {

struct VERTEX {
    XMFLOAT3 position;
    XMFLOAT3 normal;
};

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
    graphics::PIPELINE_HANDLE graphics_pso;
    graphics::RESOURCE_HANDLE vertex_buffer;
    graphics::RESOURCE_HANDLE index_buffer;
    graphics::RESOURCE_HANDLE const_buffer;
    graphics::RESOURCE_HANDLE depth_texture;
    D3D12_CPU_DESCRIPTOR_HANDLE vertex_buffer_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE index_buffer_srv;
    D3D12_CPU_DESCRIPTOR_HANDLE depth_texture_dsv;
    library::FRAME_STATS frame_stats;
    library::IMGUI_CONTEXT gui;
    eastl::vector<MESH> meshes;
    eastl::vector<RENDERABLE> renderables;
    U32 num_renderables_to_draw;
    bool enable_debug_draw;
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
    const char* filename,
    eastl::vector<MESH>* meshes,
    eastl::vector<VERTEX>* all_vertices,
    eastl::vector<U32>* all_indices
) {
    assert(filename && meshes && all_vertices && all_indices);

    eastl::vector<XMFLOAT3> positions;
    eastl::vector<XMFLOAT3> normals;
    eastl::vector<U32> indices;
    library::Load_Mesh(filename, &positions, &normals, &indices);

    eastl::vector<VERTEX> vertices(positions.size());
    for (U32 i = 0; i < vertices.size(); ++i) {
        vertices[i] = { positions[i], normals[i] };
    }
    meshes->push_back({
        .index_offset = (U32)all_indices->size(),
        .vertex_offset = (U32)all_vertices->size(),
        .num_indices = (U32)indices.size(),
    });
    all_vertices->insert(all_vertices->end(), vertices.begin(), vertices.end());
    all_indices->insert(all_indices->end(), indices.begin(), indices.end());
}

template<typename T> void Upload_To_Gpu(
    graphics::CONTEXT* gr,
    graphics::RESOURCE_HANDLE resource,
    const eastl::vector<T>* data,
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

    // Traditional, graphics shader pso.
    {
        const eastl::vector<U8> vs = library::Load_File("data/shaders/test_vs.vs.cso");
        const eastl::vector<U8> ps = library::Load_File("data/shaders/test_ps.ps.cso");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {
            .VS = { vs.data(), vs.size() },
            .PS = { ps.data(), ps.size() },
            .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
            .SampleMask = UINT32_MAX,
            .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
            .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
            .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
            .NumRenderTargets = 1,
            .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
            .DSVFormat = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = { .Count = 1, .Quality = 0 },
        };
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        demo->graphics_pso = graphics::Create_Graphics_Shader_Pipeline(gr, &desc);
    }

    VHR(gr->d2d.context->CreateSolidColorBrush(
        { .r = 1.0f, .g = 0.0f, .b = 0.0f, .a = 0.5f },
        &demo->hud.brush
    ));
    demo->hud.brush->SetColor({ .r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f });

    VHR(gr->d2d.factory_dwrite->CreateTextFormat(
        L"Verdana",
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        32.0f,
        L"en-us",
        &demo->hud.text_format
    ));
    VHR(demo->hud.text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
    VHR(demo->hud.text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));

    eastl::vector<VERTEX> all_vertices;
    eastl::vector<U32> all_indices;
    {
        const char* mesh_paths[] = {
            "data/meshes/SciFiHelmet.gltf",
            "data/meshes/sphere.gltf",
            "data/meshes/cube.gltf",
        };
        for (U32 i = 0; i < eastl::size(mesh_paths); ++i) {
            Add_Mesh(mesh_paths[i], &demo->meshes, &all_vertices, &all_indices);
        }
    }
    for (float z = -25.0f; z <= 26.0f; z += 3.0f) {
        for (float x = -25.0f; x <= 26.0f; x += 3.0f) {
            demo->renderables.push_back({ .mesh = demo->meshes[0], .position = { x, 0.0f, z } });
        }
    }

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
            .Buffer = { .NumElements = (U32)all_indices.size() }
        }),
        demo->index_buffer_srv
    );

    demo->const_buffer = graphics::Create_Committed_Resource(
        gr,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_HEAP_FLAG_NONE,
        Get_Const_Ptr(CD3DX12_RESOURCE_DESC::Buffer(256 * 1024)),
        D3D12_RESOURCE_STATE_COPY_DEST,
        NULL
    );

    // Create depth texture.
    {
        auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_D32_FLOAT,
            gr->viewport_width,
            gr->viewport_height
        );
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
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

    graphics::Begin_Frame(gr);

    library::Init_Gui_Context(&demo->gui, gr);

    // Upload vertices.
    Upload_To_Gpu(gr, demo->vertex_buffer, &all_vertices, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // Upload indices.
    Upload_To_Gpu(gr, demo->index_buffer, &all_indices, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    graphics::Flush_Gpu_Commands(gr);
    graphics::Finish_Gpu_Commands(gr);

    library::Init_Frame_Stats(&demo->frame_stats);

    demo->camera = {
        .position = XMFLOAT3(0.0f, 24.0f, -36.0f),
        .pitch = XM_PIDIV4,
        .yaw = 0.0f,
    };
    demo->mouse = {
        .cursor_prev_x = 0,
        .cursor_prev_y = 0,
    };
    demo->num_renderables_to_draw = (U32)demo->renderables.size();
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
    graphics::Release_Resource(gr, demo->const_buffer);
    graphics::Release_Resource(gr, demo->depth_texture);
    graphics::Release_Pipeline(gr, demo->graphics_pso);
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
    ImGui::SetNextWindowSize(ImVec2(600.0f, 250.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(
        "Demo Settings",
        NULL,
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
    );
    ImGui::SliderInt(
        "Number of objects",
        (S32*)&demo->num_renderables_to_draw,
        1,
        (S32)demo->renderables.size()
    );
    ImGui::Checkbox("Enable debug draw", &demo->enable_debug_draw);

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
        const F32 delta_time = demo->frame_stats.delta_time;
        const XMMATRIX transform = XMMatrixRotationX(demo->camera.pitch) *
            XMMatrixRotationY(demo->camera.yaw);
        XMVECTOR forward = XMVector3Normalize(
            XMVector3Transform(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), transform)
        );
        XMStoreFloat3(&demo->camera.forward, forward);
        const XMVECTOR right = 20.0f * delta_time * XMVector3Normalize(
            XMVector3Cross(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward)
        );
        forward = 20.0f * delta_time * forward;

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

    const auto [back_buffer, back_buffer_rtv] = graphics::Get_Back_Buffer(gr);

    graphics::Add_Transition_Barrier(gr, back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
    graphics::Flush_Resource_Barriers(gr);

    gr->cmdlist->OMSetRenderTargets(1, &back_buffer_rtv, TRUE, &demo->depth_texture_dsv);
    gr->cmdlist->ClearDepthStencilView(demo->depth_texture_dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
    gr->cmdlist->ClearRenderTargetView(back_buffer_rtv, XMVECTORF32{ 0.2f, 0.4f, 0.8f, 1.0f }, 0, NULL);
    gr->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Upload constant data.
    {
        graphics::Add_Transition_Barrier(
            gr,
            demo->const_buffer,
            D3D12_RESOURCE_STATE_COPY_DEST
        );
        graphics::Flush_Resource_Barriers(gr);

        const auto [span, buffer, buffer_offset] = graphics::Allocate_Upload_Buffer_Region<XMFLOAT4X4A>(
            gr,
            (U32)demo->renderables.size() + 2
        );
        XMMATRIX world_to_clip = XMMatrixLookToLH(
            XMLoadFloat3(&demo->camera.position),
            XMLoadFloat3(&demo->camera.forward),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
        );
        world_to_clip *= XMMatrixPerspectiveFovLH(
            XM_PI / 3.0f,
            (F32)gr->viewport_width / gr->viewport_height,
            0.1f,
            100.0f
        );
        XMStoreFloat4x4A(&span[0], XMMatrixTranspose(world_to_clip));
        *(U32*)&span[1] = demo->enable_debug_draw ? 1 : 0;

        for (U32 i = 0; i < demo->renderables.size(); ++i) {
            const XMVECTOR pos = XMLoadFloat3(&demo->renderables[i].position);
            XMStoreFloat4x4A(&span[i + 2], XMMatrixTranspose(XMMatrixTranslationFromVector(pos)));
        }
        gr->cmdlist->CopyBufferRegion(
            graphics::Get_Resource(gr, demo->const_buffer),
            0,
            buffer,
            buffer_offset,
            span.size_bytes()
        );
        graphics::Add_Transition_Barrier(
            gr,
            demo->const_buffer,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
        );
        graphics::Flush_Resource_Barriers(gr);
    }

    graphics::Set_Pipeline_State(gr, demo->graphics_pso);
    gr->cmdlist->SetGraphicsRootConstantBufferView(
        1,
        graphics::Get_Resource(gr, demo->const_buffer)->GetGPUVirtualAddress()
    );
    {
        const auto table_base = graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->vertex_buffer_srv);
        graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, demo->index_buffer_srv);
        gr->cmdlist->SetGraphicsRootDescriptorTable(2, table_base);
    }

    U32 num_triangles = 0;
    for (U32 i = 0; i < demo->num_renderables_to_draw; ++i) {
        const RENDERABLE* renderable = &demo->renderables[i];
        num_triangles += renderable->mesh.num_indices / 3;
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

    ImGui::Text("Number of triangles: %d", num_triangles);
    ImGui::End();

    library::Draw_Gui(&demo->gui, gr);

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
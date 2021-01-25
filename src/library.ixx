module;
#include "pch.h"
export module library;
import graphics;
namespace library {

export F64 Get_Time() {
    static LARGE_INTEGER start_counter = {};
    static LARGE_INTEGER frequency = {};
    if (start_counter.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start_counter);
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (counter.QuadPart - start_counter.QuadPart) / (F64)frequency.QuadPart;
}

export struct FRAME_STATS {
    F64 time;
    F32 delta_time;
    F64 fps;
    F64 average_cpu_time;
    F64 previous_time;
    F64 fps_refresh_time;
    U64 frame_counter;
};

export void Init_Frame_Stats(FRAME_STATS* stats) {
    assert(stats);
    stats->time = Get_Time();
    stats->delta_time = 0.0f;
    stats->fps = 0.0;
    stats->average_cpu_time = 0.0;
    stats->previous_time = 0.0;
    stats->fps_refresh_time = 0.0;
    stats->frame_counter = 0;
}

export void Update_Frame_Stats(FRAME_STATS* stats) {
    assert(stats);
    stats->time = Get_Time();
    stats->delta_time = (F32)(stats->time - stats->previous_time);
    stats->previous_time = stats->time;

    if ((stats->time - stats->fps_refresh_time) >= 1.0) {
        stats->fps = stats->frame_counter / (stats->time - stats->fps_refresh_time);
        stats->average_cpu_time = 1000.0 * (1.0 / stats->fps);
        stats->fps_refresh_time = stats->time;
        stats->frame_counter = 0;
    }
    stats->frame_counter += 1;
}

LRESULT CALLBACK Process_Window_Message(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    ImGuiIO* ui = NULL;
    if (ImGui::GetCurrentContext()) {
        ui = &ImGui::GetIO();
    }
    switch (message) {
    case WM_LBUTTONDOWN:
        if (ui) { ui->MouseDown[0] = true; }
        break;
    case WM_LBUTTONUP:
        if (ui) { ui->MouseDown[0] = false; }
        break;
    case WM_RBUTTONDOWN:
        if (ui) { ui->MouseDown[1] = true; }
        break;
    case WM_RBUTTONUP:
        if (ui) { ui->MouseDown[1] = false; }
        break;
    case WM_MBUTTONDOWN:
        if (ui) { ui->MouseDown[2] = true; }
        break;
    case WM_MBUTTONUP:
        if (ui) { ui->MouseDown[2] = false; }
        break;
    case WM_MOUSEWHEEL:
        if (ui) { ui->MouseWheel += GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1.0f : -1.0f; }
        break;
    case WM_MOUSEMOVE:
        if (ui) {
            ui->MousePos.x = (S16)lparam;
            ui->MousePos.y = (S16)(lparam >> 16);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        break;
    }
    return DefWindowProc(window, message, wparam, lparam);
}

export HWND Create_Window(LPCSTR name, U32 width, U32 height, bool init_imgui) {
    if (init_imgui) {
        ImGui::CreateContext();
    }
    RegisterClass(
        Get_Const_Ptr<WNDCLASS>({
            .lpfnWndProc = Process_Window_Message,
            .hInstance = GetModuleHandle(NULL),
            .hCursor = LoadCursor(NULL, IDC_ARROW),
            .lpszClassName = name,
        })
    );
    RECT rect = { 0, 0, (LONG)width, (LONG)height };
    if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, 0)) {
        assert(0);
    }
    const HWND window = CreateWindowEx(
        0,
        name,
        name,
        WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        NULL,
        NULL,
        NULL,
        0
    );
    assert(window);
    return window;
}

export VECTOR<U8> Load_File(LPCSTR filename) {
    assert(filename);
    FILE* file = fopen(filename, "rb");
    if (!file) {
        assert(0);
        return VECTOR<U8>();
    }
    MZ_DEFER(fclose(file));
    fseek(file, 0, SEEK_END);
    const LONG size = ftell(file);
    if (size <= 0) {
        assert(0);
        return VECTOR<U8>();
    }
    VECTOR<U8> content(size);
    fseek(file, 0, SEEK_SET);
    fread(content.data(), 1, content.size(), file);
    return content;
}

export void Load_Mesh(
    LPCSTR filename,
    VECTOR<U32>* indices,
    VECTOR<XMFLOAT3>* positions,
    VECTOR<XMFLOAT3>* normals,
    VECTOR<XMFLOAT2>* texcoords,
    VECTOR<XMFLOAT3>* tangents
) {
    assert(filename && positions && indices);
    assert(positions->empty() && indices->empty());
    if (normals) {
        assert(normals->empty());
    }
    if (tangents) {
        assert(tangents->empty());
    }
    if (texcoords) {
        assert(texcoords->empty());
    }

    cgltf_options options = {};
    cgltf_data* data = NULL;
    {
        cgltf_result result = cgltf_parse_file(&options, filename, &data);
        assert(result == cgltf_result_success);
        result = cgltf_load_buffers(&options, data, filename);
        assert(result == cgltf_result_success);
    }
    const U32 num_vertices = (U32)data->meshes[0].primitives[0].attributes[0].data->count;
    const U32 num_indices = (U32)data->meshes[0].primitives[0].indices->count;

    positions->resize(num_vertices);
    indices->resize(num_indices);

    // Indices.
    {
        const cgltf_accessor* accessor = data->meshes[0].primitives[0].indices;

        assert(accessor->buffer_view);
        assert(accessor->stride == accessor->buffer_view->stride ||
            accessor->buffer_view->stride == 0);
        assert((accessor->stride * accessor->count) == accessor->buffer_view->size);

        const auto data_addr = (const U8*)accessor->buffer_view->buffer->data + accessor->offset +
            accessor->buffer_view->offset;

        U32* dst_addr = indices->data();

        if (accessor->stride == 1) {
            assert(accessor->component_type == cgltf_component_type_r_8u);
            const U8* src = (const U8*)data_addr;
            for (U32 idx = 0; idx < accessor->count; ++idx) {
                dst_addr[idx] = (U32)src[idx];
            }
        } else if (accessor->stride == 2) {
            assert(accessor->component_type == cgltf_component_type_r_16u);
            const U16* src = (const U16*)data_addr;
            for (U32 idx = 0; idx < accessor->count; ++idx) {
                dst_addr[idx] = (U32)src[idx];
            }
        } else if (accessor->stride == 4) {
            assert(accessor->component_type == cgltf_component_type_r_32u);
            memcpy(dst_addr, data_addr, accessor->count * accessor->stride);
        } else {
            assert(0);
        }
    }
    // Attributes.
    {
        const U32 num_attribs = (U32)data->meshes[0].primitives[0].attributes_count;

        for (U32 attrib_idx = 0; attrib_idx < num_attribs; ++attrib_idx) {
            const cgltf_attribute* attrib =
                &data->meshes[0].primitives[0].attributes[attrib_idx];
            const cgltf_accessor* accessor = attrib->data;

            assert(accessor->buffer_view);
            assert(accessor->stride == accessor->buffer_view->stride ||
                accessor->buffer_view->stride == 0);
            assert((accessor->stride * accessor->count) == accessor->buffer_view->size);

            const auto data_addr = (const U8*)accessor->buffer_view->buffer->data +
                accessor->offset + accessor->buffer_view->offset;

            if (attrib->type == cgltf_attribute_type_position) {
                // POSITION
                assert(accessor->type == cgltf_type_vec3);
                assert(accessor->component_type == cgltf_component_type_r_32f);
                memcpy(positions->data(), data_addr, accessor->count * accessor->stride);
            } else if (normals && attrib->type == cgltf_attribute_type_normal) {
                // NORMAL
                assert(accessor->type == cgltf_type_vec3);
                assert(accessor->component_type == cgltf_component_type_r_32f);
                normals->resize(num_vertices);
                memcpy(normals->data(), data_addr, accessor->count * accessor->stride);
            } else if (tangents && attrib->type == cgltf_attribute_type_tangent) {
                // TANGENT
                assert(accessor->type == cgltf_type_vec3);
                assert(accessor->component_type == cgltf_component_type_r_32f);
                tangents->resize(num_vertices);
                memcpy(tangents->data(), data_addr, accessor->count * accessor->stride);
            } else if (texcoords && attrib->type == cgltf_attribute_type_texcoord) {
                // TEXCOORD
                assert(accessor->type == cgltf_type_vec2);
                assert(accessor->component_type == cgltf_component_type_r_32f);
                texcoords->resize(num_vertices);
                memcpy(texcoords->data(), data_addr, accessor->count * accessor->stride);
            }
        }
        assert(!positions->empty() && positions->size() == normals->size());
    }

    cgltf_free(data);
}

export struct IMGUI_CONTEXT {
    graphics::PIPELINE_HANDLE pso;
    graphics::RESOURCE_HANDLE font;
    graphics::RESOURCE_HANDLE vb[graphics::max_num_frames_in_flight];
    graphics::RESOURCE_HANDLE ib[graphics::max_num_frames_in_flight];
    void* vb_cpu_addr[graphics::max_num_frames_in_flight];
    void* ib_cpu_addr[graphics::max_num_frames_in_flight];
    D3D12_CPU_DESCRIPTOR_HANDLE font_srv;
};

export void Init_Gui_Context(IMGUI_CONTEXT* gui, graphics::CONTEXT* gr) {
    assert(gui && gr);

    ImGuiIO* io = &ImGui::GetIO();
    io->KeyMap[ImGuiKey_Tab] = VK_TAB;
    io->KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io->KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io->KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io->KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io->KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io->KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io->KeyMap[ImGuiKey_Home] = VK_HOME;
    io->KeyMap[ImGuiKey_End] = VK_END;
    io->KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io->KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io->KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io->KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io->KeyMap[ImGuiKey_A] = 'A';
    io->KeyMap[ImGuiKey_C] = 'C';
    io->KeyMap[ImGuiKey_V] = 'V';
    io->KeyMap[ImGuiKey_X] = 'X';
    io->KeyMap[ImGuiKey_Y] = 'Y';
    io->KeyMap[ImGuiKey_Z] = 'Z';
    io->ImeWindowHandle = gr->window;
    io->DisplaySize = ImVec2((F32)gr->viewport_width, (F32)gr->viewport_height);
    ImGui::GetStyle().WindowRounding = 0.0f;

    U8* pixels;
    S32 width, height;
    ImGui::GetIO().Fonts->AddFontFromFileTTF("data/Roboto-Medium.ttf", 24.0f);
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
    gui->font = graphics::Create_Committed_Resource(
        gr,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        NULL
    );
    graphics::Update_Tex2D_Subresource(gr, gui->font, 0, pixels, width * 4);
    graphics::Add_Transition_Barrier(gr, gui->font, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    gui->font_srv = graphics::Allocate_Cpu_Descriptors(gr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
    gr->device->CreateShaderResourceView(graphics::Get_Resource(gr, gui->font), NULL, gui->font_srv);

    const D3D12_INPUT_ELEMENT_DESC inputs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "_Uv", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "_Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    const VECTOR<U8> vs = library::Load_File("data/shaders/imgui_vs_ps.vs.cso");
    const VECTOR<U8> ps = library::Load_File("data/shaders/imgui_vs_ps.ps.cso");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {
        .VS = { vs.data(), vs.size() },
        .PS = { ps.data(), ps.size() },
        .BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT),
        .SampleMask = UINT32_MAX,
        .RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT),
        .DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT),
        .InputLayout = { inputs, (U32)eastl::size(inputs) },
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .SampleDesc = { .Count = 1, .Quality = 0 },
    };
    pso_desc.DepthStencilState.DepthEnable = FALSE;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    pso_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    pso_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    pso_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    pso_desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    pso_desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    pso_desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    gui->pso = graphics::Create_Graphics_Shader_Pipeline(gr, &pso_desc);
}

export void Deinit_Gui_Context(IMGUI_CONTEXT* gui, graphics::CONTEXT* gr) {
    assert(gui && gr);
    graphics::Release_Resource(gr, gui->font);
    graphics::Release_Pipeline(gr, gui->pso);
    for (U32 i = 0; i < graphics::max_num_frames_in_flight; ++i) {
        graphics::Release_Resource(gr, gui->vb[i]);
        graphics::Release_Resource(gr, gui->ib[i]);
    }
}

export void Update_Gui(F32 delta_time) {
    ImGuiIO* io = &ImGui::GetIO();
    io->KeyCtrl = GetAsyncKeyState(VK_CONTROL) < 0;
    io->KeyShift = GetAsyncKeyState(VK_SHIFT) < 0;
    io->KeyAlt = GetAsyncKeyState(VK_MENU) < 0;
    io->DeltaTime = delta_time;
    ImGui::NewFrame();
}

export void Draw_Gui(IMGUI_CONTEXT* gui, graphics::CONTEXT* gr) {
    assert(gui && gr);

    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    if (!draw_data || draw_data->TotalVtxCount == 0) {
        return;
    }
    ImGuiIO* io = &ImGui::GetIO();

    const auto vp_width = (S32)(io->DisplaySize.x * io->DisplayFramebufferScale.x);
    const auto vp_height = (S32)(io->DisplaySize.y * io->DisplayFramebufferScale.y);
    draw_data->ScaleClipRects(io->DisplayFramebufferScale);

    graphics::RESOURCE_HANDLE vb = gui->vb[gr->frame_index];
    graphics::RESOURCE_HANDLE ib = gui->ib[gr->frame_index];

    if (graphics::Get_Resource_Size(gr, vb) < (draw_data->TotalVtxCount * sizeof ImDrawVert)) {
        graphics::Release_Resource(gr, vb);
        gui->vb[gr->frame_index] = vb = graphics::Create_Committed_Resource(
            gr,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_HEAP_FLAG_NONE,
            Get_Const_Ptr(CD3DX12_RESOURCE_DESC::Buffer(draw_data->TotalVtxCount * sizeof ImDrawVert)),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL
        );
        VHR(graphics::Get_Resource(gr, vb)->Map(
            0,
            Get_Const_Ptr<D3D12_RANGE>({ .Begin = 0, .End = 0 }),
            &gui->vb_cpu_addr[gr->frame_index]
        ));
    }
    if (graphics::Get_Resource_Size(gr, ib) < (draw_data->TotalIdxCount * sizeof ImDrawIdx)) {
        graphics::Release_Resource(gr, ib);
        gui->ib[gr->frame_index] = ib = graphics::Create_Committed_Resource(
            gr,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_HEAP_FLAG_NONE,
            Get_Const_Ptr(CD3DX12_RESOURCE_DESC::Buffer(draw_data->TotalIdxCount * sizeof ImDrawIdx)),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL
        );
        VHR(graphics::Get_Resource(gr, ib)->Map(
            0,
            Get_Const_Ptr<D3D12_RANGE>({ .Begin = 0, .End = 0 }),
            &gui->ib_cpu_addr[gr->frame_index]
        ));
    }
    // Update vertex and index buffers.
    {
        ImDrawVert* vb_addr = (ImDrawVert*)gui->vb_cpu_addr[gr->frame_index];
        ImDrawIdx* ib_addr = (ImDrawIdx*)gui->ib_cpu_addr[gr->frame_index];

        for (U32 cmdlist_idx = 0; cmdlist_idx < (U32)draw_data->CmdListsCount; ++cmdlist_idx) {
            ImDrawList* list = draw_data->CmdLists[cmdlist_idx];
            memcpy(vb_addr, &list->VtxBuffer[0], list->VtxBuffer.size() * sizeof ImDrawVert);
            memcpy(ib_addr, &list->IdxBuffer[0], list->IdxBuffer.size() * sizeof ImDrawIdx);
            vb_addr += list->VtxBuffer.size();
            ib_addr += list->IdxBuffer.size();
        }
    }
    gr->cmdlist->RSSetViewports(
        1,
        Get_Const_Ptr<D3D12_VIEWPORT>({
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = (F32)vp_width,
            .Height = (F32)vp_height,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        })
    );
    gr->cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    graphics::Set_Pipeline_State(gr, gui->pso);
    // Update constant buffer.
    {
        const auto [cpu_addr, gpu_addr] = graphics::Allocate_Upload_Memory(gr, sizeof XMFLOAT4X4A);
        XMStoreFloat4x4A(
            (XMFLOAT4X4A*)cpu_addr,
            XMMatrixTranspose(
                XMMatrixOrthographicOffCenterLH(0.0f, (F32)vp_width, (F32)vp_height, 0.0f, 0.0f, 1.0f)
            )
        );
        gr->cmdlist->SetGraphicsRootConstantBufferView(0, gpu_addr);
    }
    gr->cmdlist->SetGraphicsRootDescriptorTable(
        1,
        graphics::Copy_Descriptors_To_Gpu_Heap(gr, 1, gui->font_srv)
    );
    gr->cmdlist->IASetVertexBuffers(
        0,
        1,
        Get_Const_Ptr<D3D12_VERTEX_BUFFER_VIEW>({
            .BufferLocation = graphics::Get_Resource(gr, vb)->GetGPUVirtualAddress(),
            .SizeInBytes = draw_data->TotalVtxCount * sizeof ImDrawVert,
            .StrideInBytes = sizeof ImDrawVert,
        })
    );
    gr->cmdlist->IASetIndexBuffer(
        Get_Const_Ptr<D3D12_INDEX_BUFFER_VIEW>({
            .BufferLocation = graphics::Get_Resource(gr, ib)->GetGPUVirtualAddress(),
            .SizeInBytes = draw_data->TotalIdxCount * sizeof ImDrawIdx,
            .Format = sizeof ImDrawIdx == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
        })
    );

    S32 vertex_offset = 0;
    U32 index_offset = 0;
    for (U32 cmdlist_idx = 0; cmdlist_idx < (U32)draw_data->CmdListsCount; ++cmdlist_idx) {
        const ImDrawList* list = draw_data->CmdLists[cmdlist_idx];

        for (U32 cmd_idx = 0; cmd_idx < (U32)list->CmdBuffer.size(); ++cmd_idx) {
            const ImDrawCmd* cmd = &list->CmdBuffer[cmd_idx];

            if (cmd->UserCallback) {
                cmd->UserCallback(list, cmd);
            } else {
                gr->cmdlist->RSSetScissorRects(
                    1,
                    Get_Const_Ptr<D3D12_RECT>({
                        .left = (LONG)cmd->ClipRect.x,
                        .top = (LONG)cmd->ClipRect.y,
                        .right = (LONG)cmd->ClipRect.z,
                        .bottom = (LONG)cmd->ClipRect.w,
                    })
                );
                gr->cmdlist->DrawIndexedInstanced(cmd->ElemCount, 1, index_offset, vertex_offset, 0);
            }
            index_offset += cmd->ElemCount;
        }
        vertex_offset += list->VtxBuffer.size();
    }
}

export struct MIPMAP_GENERATOR {
    graphics::PIPELINE_HANDLE pso;
    graphics::RESOURCE_HANDLE scratch_textures[4];
    D3D12_CPU_DESCRIPTOR_HANDLE base_uav;
    DXGI_FORMAT format;
};

export void Init_Mipmap_Generator(MIPMAP_GENERATOR* mipgen, graphics::CONTEXT* gr, DXGI_FORMAT format) {
    assert(mipgen && gr);

    U32 width = 2048 / 2;
    U32 height = 2048 / 2;
    for (U32 i = 0; i < eastl::size(mipgen->scratch_textures); ++i) {
        auto desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        mipgen->scratch_textures[i] = Create_Committed_Resource(
            gr,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            NULL 
        );
        width /= 2;
        height /= 2;
    }
    mipgen->base_uav = graphics::Allocate_Cpu_Descriptors(gr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = mipgen->base_uav;
    for (U32 i = 0; i < eastl::size(mipgen->scratch_textures); ++i) {
        gr->device->CreateUnorderedAccessView(
            Get_Resource(gr, mipgen->scratch_textures[i]),
            NULL,
            NULL,
            cpu_handle
        );
        cpu_handle.ptr += gr->cbv_srv_uav_cpu_heap.descriptor_size;
    }
    {
        const VECTOR<U8> cs = Load_File("data/shaders/generate_mipmaps_cs.cs.cso");
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {
            .CS = { cs.data(), cs.size() },
        };
        mipgen->pso = graphics::Create_Compute_Shader_Pipeline(gr, &desc);
    }
    mipgen->format = format;
}

export void Deinit_Mipmap_Generator(MIPMAP_GENERATOR* mipgen, graphics::CONTEXT* gr) {
    assert(mipgen && gr);
    graphics::Release_Pipeline(gr, mipgen->pso);
    for (U32 i = 0; i < eastl::size(mipgen->scratch_textures); ++i) {
        graphics::Release_Resource(gr, mipgen->scratch_textures[i]);
    }
}

export void Generate_Mipmaps(
    MIPMAP_GENERATOR* mipgen,
    graphics::CONTEXT* gr,
    graphics::RESOURCE_HANDLE texture
) {
    assert(mipgen && gr);
    const D3D12_RESOURCE_DESC texture_desc = graphics::Get_Resource_Desc(gr, texture);
    assert(mipgen->format == texture_desc.Format);
    assert(texture_desc.Width <= 2048 && texture_desc.Height <= 2048);
    assert(texture_desc.Width == texture_desc.Height);
    assert(texture_desc.MipLevels > 1);

    for (U32 array_slice = 0; array_slice < texture_desc.DepthOrArraySize; ++array_slice) {
        // Save heap size for temporary descriptor allocations.
        const U32 cbv_srv_uav_cpu_heap_size = gr->cbv_srv_uav_cpu_heap.size;
        const D3D12_CPU_DESCRIPTOR_HANDLE texture_srv = graphics::Allocate_Cpu_Descriptors(
            gr,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            1
        );
        gr->device->CreateShaderResourceView(
            graphics::Get_Resource(gr, texture),
            Get_Const_Ptr<D3D12_SHADER_RESOURCE_VIEW_DESC>({
                .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY,
                .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
                .Texture2DArray = {
                    .MipLevels = texture_desc.MipLevels,
                    .FirstArraySlice = array_slice,
                    .ArraySize = 1,
                }
            }),
            texture_srv
        );
        const D3D12_GPU_DESCRIPTOR_HANDLE table_base = graphics::Copy_Descriptors_To_Gpu_Heap(
            gr,
            1,
            texture_srv
        );
        graphics::Copy_Descriptors_To_Gpu_Heap(
            gr,
            (U32)eastl::size(mipgen->scratch_textures),
            mipgen->base_uav
        );
        // Restore heap size - we're done with temporary descriptor allocations.
        gr->cbv_srv_uav_cpu_heap.size = cbv_srv_uav_cpu_heap_size;

        graphics::Set_Pipeline_State(gr, mipgen->pso);
        U32 total_num_mips = (U32)(texture_desc.MipLevels - 1);
        U32 current_src_mip_level = 0;

        for (;;) {
            for (U32 i = 0; i < eastl::size(mipgen->scratch_textures); ++i) {
                graphics::Add_Transition_Barrier(
                    gr,
                    mipgen->scratch_textures[i],
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS
                );
            }
            graphics::Add_Transition_Barrier(gr, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            graphics::Flush_Resource_Barriers(gr);

            const U32 dispatch_num_mips = total_num_mips >= 4 ? 4 : total_num_mips;
            gr->cmdlist->SetComputeRoot32BitConstant(0, current_src_mip_level, 0);
            gr->cmdlist->SetComputeRoot32BitConstant(0, dispatch_num_mips, 1);
            gr->cmdlist->SetComputeRootDescriptorTable(1, table_base);
            gr->cmdlist->Dispatch(
                (U32)(texture_desc.Width >> (3 + current_src_mip_level)),
                texture_desc.Height >> (3 + current_src_mip_level),
                1
            );

            for (U32 i = 0; i < eastl::size(mipgen->scratch_textures); ++i) {
                graphics::Add_Transition_Barrier(
                    gr,
                    mipgen->scratch_textures[i],
                    D3D12_RESOURCE_STATE_COPY_SOURCE
                );
            }
            graphics::Add_Transition_Barrier(gr, texture, D3D12_RESOURCE_STATE_COPY_DEST);
            graphics::Flush_Resource_Barriers(gr);

            for (U32 mip_idx = 0; mip_idx < dispatch_num_mips; ++mip_idx) {
                const auto dst = CD3DX12_TEXTURE_COPY_LOCATION(
                    graphics::Get_Resource(gr, texture),
                    mip_idx + 1 + current_src_mip_level + array_slice * texture_desc.MipLevels
                );
                const auto src = CD3DX12_TEXTURE_COPY_LOCATION(
                    graphics::Get_Resource(gr, mipgen->scratch_textures[mip_idx]),
                    0
                );
                const auto box = CD3DX12_BOX(
                    0,
                    0,
                    0,
                    (U32)(texture_desc.Width >> (mip_idx + 1 + current_src_mip_level)),
                    texture_desc.Height >> (mip_idx + 1 + current_src_mip_level),
                    1
                );
                gr->cmdlist->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
            }

            if ((total_num_mips -= dispatch_num_mips) == 0) {
                break;
            }
            current_src_mip_level += dispatch_num_mips;
        }
    }
}

} // namespace library

module;
#include "pch.h"
export module graphics;
namespace graphics {

export constexpr U32 max_num_frames_in_flight = 3;

constexpr U32 max_num_resources = 256;
constexpr U32 max_num_pipelines = 128;

constexpr U32 num_swapchain_buffers = 3;

constexpr U32 num_rtv_descriptors = 128;
constexpr U32 num_dsv_descriptors = 128;
constexpr U32 num_cbv_srv_uav_cpu_descriptors = 16 * 1024;
constexpr U32 num_cbv_srv_uav_gpu_descriptors = 4 * 1024;

constexpr U32 upload_alloc_alignment = 512;
constexpr U32 upload_heap_capacity = 8 * 1024 * 1024;

struct GPU_MEMORY_HEAP {
    ID3D12_RESOURCE* heap;
    U8* cpu_addr;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_addr;
    U32 size;
    U32 capacity;
};

void Init_Gpu_Memory_Heap(
    GPU_MEMORY_HEAP* heap,
    ID3D12_DEVICE* device,
    U32 capacity,
    D3D12_HEAP_TYPE type
) {
    assert(heap && device && capacity > 0);

    VHR(device->CreateCommittedResource(
        Get_Const_Ptr<D3D12_HEAP_PROPERTIES>({ .Type = type }),
        D3D12_HEAP_FLAG_NONE,
        Get_Const_Ptr(CD3DX12_RESOURCE_DESC::Buffer(capacity)),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        NULL,
        IID_PPV_ARGS(&heap->heap)
    ));
    VHR(heap->heap->Map(
        0,
        Get_Const_Ptr<D3D12_RANGE>({ .Begin = 0, .End = 0 }),
        (void**)&heap->cpu_addr
    ));

    heap->gpu_addr = heap->heap->GetGPUVirtualAddress();
    heap->size = 0;
    heap->capacity = capacity;
}

TUPLE<U8*, D3D12_GPU_VIRTUAL_ADDRESS> Allocate_Gpu_Memory(
    GPU_MEMORY_HEAP* heap,
    U32 mem_size
) {
    assert(heap && mem_size > 0);

    const U32 aligned_size = (mem_size + (upload_alloc_alignment - 1)) & ~(upload_alloc_alignment - 1);
    if ((heap->size + aligned_size) >= heap->capacity) {
        return { {}, 0 };
    }
    U8* cpu_addr = heap->cpu_addr + heap->size;
    D3D12_GPU_VIRTUAL_ADDRESS gpu_addr = heap->gpu_addr + heap->size;

    heap->size += aligned_size;
    return { cpu_addr, gpu_addr };
}

export struct DESCRIPTOR {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
};

struct DESCRIPTOR_HEAP {
    ID3D12_DESCRIPTOR_HEAP* heap;
    DESCRIPTOR base;
    U32 size;
    U32 capacity;
    U32 descriptor_size;
};

void Init_Descriptor_Heap(
    DESCRIPTOR_HEAP* heap,
    ID3D12_DEVICE* device,
    U32 capacity,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags
) {
    assert(heap && heap->heap == NULL && device && capacity > 0);

    const D3D12_DESCRIPTOR_HEAP_DESC desc = {
        .Type = type,
        .NumDescriptors = capacity,
        .Flags = flags,
    };
    VHR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap->heap)));

    heap->base.cpu_handle = heap->heap->GetCPUDescriptorHandleForHeapStart();
    if (flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
        heap->base.gpu_handle = heap->heap->GetGPUDescriptorHandleForHeapStart();
    } else {
        heap->base.gpu_handle = {};
    }
    heap->size = 0;
    heap->capacity = capacity;
    heap->descriptor_size = device->GetDescriptorHandleIncrementSize(type);
}

DESCRIPTOR Allocate_Descriptors(DESCRIPTOR_HEAP* heap, U32 num_descriptors) {
    assert(heap && num_descriptors > 0);
    assert((heap->size + num_descriptors) < heap->capacity);

    const D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = {
        .ptr = heap->base.cpu_handle.ptr + heap->size * heap->descriptor_size,
    };
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = {};
    if (heap->base.gpu_handle.ptr != 0) {
        gpu_handle = { .ptr = heap->base.gpu_handle.ptr + heap->size * heap->descriptor_size };
    }
    heap->size += num_descriptors;
    return { cpu_handle, gpu_handle };
}

export struct RESOURCE_HANDLE {
    U16 index;
    U16 generation;
};

struct RESOURCE {
    ID3D12_RESOURCE* raw;
    D3D12_RESOURCE_STATES state;
    D3D12_RESOURCE_DESC desc;
};

struct RESOURCE_POOL {
    eastl::vector<RESOURCE> resources;
    eastl::vector<U16> generations;
};

void Init_Resource_Pool(RESOURCE_POOL* pool) {
    assert(pool);

    pool->resources.resize(max_num_resources + 1);
    pool->generations.resize(max_num_resources + 1);

    for (U32 i = 0; i < (max_num_resources + 1); ++i) {
        pool->resources[i] = {};
        pool->generations[i] = 0;
    }
}

void Deinit_Resource_Pool(RESOURCE_POOL* pool) {
    assert(pool);

    for (U32 i = 0; i < (max_num_resources + 1); ++i) {
        if (i > 0 && i <= num_swapchain_buffers) {
            // Release internally created swapbuffers.
            MZ_SAFE_RELEASE(pool->resources[i].raw);
        } else {
            // Verify that all resources has been released by a user.
            assert(pool->resources[i].raw == NULL);
        }
    }
}

RESOURCE_HANDLE Add_Resource(
    RESOURCE_POOL* pool,
    ID3D12_RESOURCE* raw,
    D3D12_RESOURCE_STATES state
) {
    assert(pool && raw);

    U32 slot_idx = 1;
    for (; slot_idx <= max_num_resources; ++slot_idx) {
        if (pool->resources[slot_idx].raw == NULL) {
            break;
        }
    }
    assert(slot_idx <= max_num_resources);

    pool->resources[slot_idx] = { .raw = raw, .state = state, .desc = raw->GetDesc() };
    return { .index = (U16)slot_idx, .generation = (pool->generations[slot_idx] += 1) };
}

RESOURCE* Get_Resource_Info(RESOURCE_POOL* pool, RESOURCE_HANDLE handle) {
    assert(pool);
    assert(handle.index > 0 && handle.index <= max_num_resources);
    assert(handle.generation > 0);
    assert(handle.generation == pool->generations[handle.index]);

    RESOURCE* resource = &pool->resources[handle.index];
    assert(resource->raw);
    return resource;
}

export struct PIPELINE_HANDLE {
    U16 index;
    U16 generation;
};

struct PIPELINE {
    ID3D12_PIPELINE_STATE* pso;
    ID3D12_ROOT_SIGNATURE* root_signature;
    bool is_compute;
};

struct PIPELINE_POOL {
    eastl::vector<PIPELINE> pipelines;
    eastl::vector<U16> generations;
};

void Init_Pipeline_Pool(PIPELINE_POOL* pool) {
    assert(pool);

    pool->pipelines.resize(max_num_pipelines + 1);
    pool->generations.resize(max_num_pipelines + 1);

    for (U32 i = 0; i < (max_num_pipelines + 1); ++i) {
        pool->pipelines[i] = {};
        pool->generations[i] = 0;
    }
}

void Deinit_Pipeline_Pool(PIPELINE_POOL* pool) {
    assert(pool);

    for (U32 i = 0; i < (max_num_pipelines + 1); ++i) {
        // Verify that all pipelines has been released by a user.
        assert(pool->pipelines[i].pso == NULL);
        assert(pool->pipelines[i].root_signature == NULL);
        pool->pipelines[i] = {};
    }
}

PIPELINE_HANDLE Add_Pipeline(
    PIPELINE_POOL* pool,
    ID3D12_PIPELINE_STATE* pso,
    ID3D12_ROOT_SIGNATURE* root_signature,
    bool is_compute
) {
    assert(pool && pso && root_signature);

    U32 slot_idx = 1;
    for (; slot_idx <= max_num_pipelines; ++slot_idx) {
        if (pool->pipelines[slot_idx].pso == NULL) {
            break;
        }
    }
    assert(slot_idx <= max_num_pipelines);

    pool->pipelines[slot_idx] = {
        .pso = pso,
        .root_signature = root_signature,
        .is_compute = is_compute
    };
    return { .index = (U16)slot_idx, .generation = (pool->generations[slot_idx] += 1) };
}

PIPELINE* Get_Pipeline_Info(PIPELINE_POOL* pool, PIPELINE_HANDLE handle) {
    assert(pool);
    assert(handle.index > 0 && handle.index <= max_num_pipelines);
    assert(handle.generation > 0);
    assert(handle.generation == pool->generations[handle.index]);

    PIPELINE* pipeline = &pool->pipelines[handle.index];
    assert(pipeline->pso && pipeline->root_signature);
    return pipeline;
}

export struct CONTEXT {
    ID3D12_DEVICE* device;
    ID3D12_GRAPHICS_COMMAND_LIST* cmdlist;
    ID3D12_COMMAND_QUEUE* cmdqueue;
    ID3D12_COMMAND_ALLOCATOR* cmdallocs[max_num_frames_in_flight];
    IDXGI_SWAP_CHAIN* swapchain;
    RESOURCE_HANDLE swapchain_buffers[num_swapchain_buffers];
    ID3D12_FENCE* frame_fence;
    HANDLE frame_fence_event;
    HWND window;
    U64 frame_fence_counter;
    U32 frame_index;
    U32 back_buffer_index;
    U32 viewport_width;
    U32 viewport_height;
    RESOURCE_POOL resource_pool;
    struct {
        PIPELINE_POOL pool;
        PIPELINE_HANDLE current;
        eastl::hash_map<U64, PIPELINE_HANDLE> map = {{}};
    } pipeline;
    DESCRIPTOR_HEAP rtv_heap;
    DESCRIPTOR_HEAP dsv_heap;
    DESCRIPTOR_HEAP cbv_srv_uav_cpu_heap;
    DESCRIPTOR_HEAP cbv_srv_uav_gpu_heaps[max_num_frames_in_flight];
    GPU_MEMORY_HEAP upload_heaps[max_num_frames_in_flight];
    eastl::vector<D3D12_RESOURCE_BARRIER> buffered_resource_barriers;
    struct {
        ID2D1_FACTORY* factory;
        ID2D1_DEVICE* device;
        ID2D1_DEVICE_CONTEXT* context;
        ID3D11_DEVICE* device11;
        ID3D11_DEVICE_CONTEXT* context11;
        ID3D11ON12_DEVICE* device11on12;
        ID3D11_RESOURCE* wrapped_swapbuffers[num_swapchain_buffers];
        ID2D1_BITMAP* targets[num_swapchain_buffers];
        IDWRITE_FACTORY* factory_dwrite;
    } d2d;
};

export bool Init_Context(CONTEXT* gr, HWND window) {
    assert(gr && window);

    CoInitialize(NULL);

    RECT rect;
    GetClientRect(window, &rect);
    gr->viewport_width = (U32)(rect.right - rect.left);
    gr->viewport_height = (U32)(rect.bottom - rect.top);

    IDXGI_FACTORY* factory = NULL;
    VHR(CreateDXGIFactory2(
#ifdef _DEBUG
        DXGI_CREATE_FACTORY_DEBUG,
#else
        0,
#endif
        IID_PPV_ARGS(&factory)
    ));

#ifdef _DEBUG
    {
        ID3D12Debug1* debug = NULL;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            //debug->SetEnableGPUBasedValidation(TRUE);
            MZ_SAFE_RELEASE(debug);
        }
    }
#endif
    VHR(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&gr->device)));

    const D3D12_COMMAND_QUEUE_DESC cmdqueue_desc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
    };
    VHR(gr->device->CreateCommandQueue(&cmdqueue_desc, IID_PPV_ARGS(&gr->cmdqueue)));

    DXGI_SWAP_CHAIN_DESC swapchain_desc = {
        .BufferDesc = {
            .Width = gr->viewport_width,
            .Height = gr->viewport_height,
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        },
        .SampleDesc = {
            .Count = 1,
            .Quality = 0,
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = num_swapchain_buffers,
        .OutputWindow = window,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
    };
    IDXGISwapChain* temp_swapchain = NULL;
    VHR(factory->CreateSwapChain((IUnknown*)gr->cmdqueue, &swapchain_desc, &temp_swapchain));
    VHR(temp_swapchain->QueryInterface(IID_PPV_ARGS(&gr->swapchain)));
    MZ_SAFE_RELEASE(temp_swapchain);
    MZ_SAFE_RELEASE(factory);

    gr->window = window;

    VHR(D3D11On12CreateDevice(
        (IUnknown*)gr->device,
#ifdef _DEBUG
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
#else
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
#endif
        NULL,
        0,
        (IUnknown**)&gr->cmdqueue,
        1,
        0,
        &gr->d2d.device11,
        &gr->d2d.context11,
        NULL
    ));
    VHR(gr->d2d.device11->QueryInterface(IID_PPV_ARGS(&gr->d2d.device11on12)));

    IDXGI_DEVICE* device_dxgi = NULL;
    VHR(gr->d2d.device11on12->QueryInterface(IID_PPV_ARGS(&device_dxgi)));
    MZ_DEFER(device_dxgi->Release());

    VHR(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
#ifdef _DEBUG
        { .debugLevel = D2D1_DEBUG_LEVEL_INFORMATION },
#else
        { .debugLevel = D2D1_DEBUG_LEVEL_NONE },
#endif
        &gr->d2d.factory
    ));

    VHR(gr->d2d.factory->CreateDevice(device_dxgi, &gr->d2d.device));
    VHR(gr->d2d.device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &gr->d2d.context));

    VHR(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(gr->d2d.factory_dwrite),
        (IUnknown**)&gr->d2d.factory_dwrite
    ));

    VHR(gr->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gr->frame_fence)));
    gr->frame_fence_event = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
    assert(gr->frame_fence_event);

    for (U32 frame_idx = 0; frame_idx < max_num_frames_in_flight; ++frame_idx) {
        VHR(gr->device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&gr->cmdallocs[frame_idx])
        ));
    }

    Init_Descriptor_Heap(
        &gr->rtv_heap,
        gr->device,
        num_rtv_descriptors,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    );
    Init_Descriptor_Heap(
        &gr->dsv_heap,
        gr->device,
        num_dsv_descriptors,
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    );
    Init_Descriptor_Heap(
        &gr->cbv_srv_uav_cpu_heap,
        gr->device,
        num_cbv_srv_uav_cpu_descriptors,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE
    );

    for (U32 frame_idx = 0; frame_idx < max_num_frames_in_flight; ++frame_idx) {
        Init_Descriptor_Heap(
            &gr->cbv_srv_uav_gpu_heaps[frame_idx],
            gr->device,
            num_cbv_srv_uav_gpu_descriptors,
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        );
        Init_Gpu_Memory_Heap(
            &gr->upload_heaps[frame_idx],
            gr->device,
            upload_heap_capacity,
            D3D12_HEAP_TYPE_UPLOAD
        );
    }

    Init_Resource_Pool(&gr->resource_pool);
    Init_Pipeline_Pool(&gr->pipeline.pool);

    for (U32 buffer_idx = 0; buffer_idx < num_swapchain_buffers; ++buffer_idx) {
        ID3D12_RESOURCE* buffer = NULL;
        VHR(gr->swapchain->GetBuffer(buffer_idx, IID_PPV_ARGS(&buffer)));

        gr->swapchain_buffers[buffer_idx] = Add_Resource(
            &gr->resource_pool,
            buffer,
            D3D12_RESOURCE_STATE_PRESENT
        );
        gr->device->CreateRenderTargetView(
            buffer,
            NULL,
            Allocate_Descriptors(&gr->rtv_heap, 1).cpu_handle
        );

        VHR(gr->d2d.device11on12->CreateWrappedResource(
            (IUnknown*)buffer,
            Get_Const_Ptr<D3D11_RESOURCE_FLAGS>({ .BindFlags = D3D11_BIND_RENDER_TARGET }),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT,
            IID_PPV_ARGS(&gr->d2d.wrapped_swapbuffers[buffer_idx])
        ));

        IDXGI_SURFACE* surface = NULL;
        VHR(gr->d2d.wrapped_swapbuffers[buffer_idx]->QueryInterface(IID_PPV_ARGS(&surface)));
        MZ_DEFER(surface->Release());

        VHR(gr->d2d.context->CreateBitmapFromDxgiSurface(
            surface,
            D2D1_BITMAP_PROPERTIES1{
                .pixelFormat = {
                    .format = DXGI_FORMAT_R8G8B8A8_UNORM,
                    .alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED,
                },
                .dpiX = 96.0f,
                .dpiY = 96.0f,
                .bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            },
            &gr->d2d.targets[buffer_idx]
        ));
    }

    VHR(gr->device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        gr->cmdallocs[0],
        NULL,
        IID_PPV_ARGS(&gr->cmdlist)
    ));
    VHR(gr->cmdlist->Close());

    gr->buffered_resource_barriers.reserve(32);
    gr->pipeline.map.clear();

    return true;
}

export void Deinit_Context(CONTEXT* gr) {
    assert(gr);
    for (U32 i = 0; i < max_num_frames_in_flight; ++i) {
        MZ_SAFE_RELEASE(gr->cmdallocs[i]);
        MZ_SAFE_RELEASE(gr->cbv_srv_uav_gpu_heaps[i].heap);
        MZ_SAFE_RELEASE(gr->upload_heaps[i].heap);
    }
    for (U32 i = 0; i < num_swapchain_buffers; ++i) {
        MZ_SAFE_RELEASE(gr->d2d.wrapped_swapbuffers[i]);
        MZ_SAFE_RELEASE(gr->d2d.targets[i]);
    }
    Deinit_Resource_Pool(&gr->resource_pool);
    Deinit_Pipeline_Pool(&gr->pipeline.pool);
    MZ_SAFE_RELEASE(gr->d2d.device);
    MZ_SAFE_RELEASE(gr->d2d.device11);
    MZ_SAFE_RELEASE(gr->d2d.device11on12);
    MZ_SAFE_RELEASE(gr->d2d.context);
    MZ_SAFE_RELEASE(gr->d2d.context11);
    MZ_SAFE_RELEASE(gr->d2d.factory);
    MZ_SAFE_RELEASE(gr->frame_fence);
    MZ_SAFE_RELEASE(gr->rtv_heap.heap);
    MZ_SAFE_RELEASE(gr->dsv_heap.heap);
    MZ_SAFE_RELEASE(gr->cbv_srv_uav_cpu_heap.heap);
    MZ_SAFE_RELEASE(gr->cmdqueue);
    MZ_SAFE_RELEASE(gr->cmdlist);
    MZ_SAFE_RELEASE(gr->swapchain);
    MZ_SAFE_RELEASE(gr->device);
}

export void Begin_Frame(CONTEXT* gr) {
    assert(gr);

    ID3D12_COMMAND_ALLOCATOR* cmdalloc = gr->cmdallocs[gr->frame_index];
    VHR(cmdalloc->Reset());

    ID3D12_GRAPHICS_COMMAND_LIST* cmdlist = gr->cmdlist;

    VHR(cmdlist->Reset(cmdalloc, NULL));
    cmdlist->SetDescriptorHeaps(1, &gr->cbv_srv_uav_gpu_heaps[gr->frame_index].heap);
    cmdlist->RSSetViewports(
        1,
        Get_Const_Ptr<D3D12_VIEWPORT>({
            .TopLeftX = 0.0f,
            .TopLeftY = 0.0f,
            .Width = (F32)gr->viewport_width,
            .Height = (F32)gr->viewport_height,
            .MinDepth = 0.0f,
            .MaxDepth = 1.0f,
        })
    );
    cmdlist->RSSetScissorRects(
        1,
        Get_Const_Ptr<D3D12_RECT>({
            .left = 0,
            .top = 0,
            .right = (LONG)gr->viewport_width,
            .bottom = (LONG)gr->viewport_height
        })
    );

    gr->pipeline.current = {};
}

export void End_Frame(CONTEXT* gr) {
    assert(gr);

    gr->frame_fence_counter += 1;

    VHR(gr->swapchain->Present(0, 0));
    VHR(gr->cmdqueue->Signal(gr->frame_fence, gr->frame_fence_counter));

    const U64 gpu_frame_counter = gr->frame_fence->GetCompletedValue();
    if ((gr->frame_fence_counter - gpu_frame_counter) >= max_num_frames_in_flight) {
        VHR(gr->frame_fence->SetEventOnCompletion(gpu_frame_counter + 1, gr->frame_fence_event));
        WaitForSingleObject(gr->frame_fence_event, INFINITE);
    }

    gr->frame_index = (gr->frame_index + 1) % max_num_frames_in_flight;
    gr->back_buffer_index = gr->swapchain->GetCurrentBackBufferIndex();

    gr->cbv_srv_uav_gpu_heaps[gr->frame_index].size = 0;
    gr->upload_heaps[gr->frame_index].size = 0;
}

export void Flush_Resource_Barriers(CONTEXT* gr) {
    assert(gr);

    if (!gr->buffered_resource_barriers.empty()) {
        gr->cmdlist->ResourceBarrier(
            (U32)gr->buffered_resource_barriers.size(),
            gr->buffered_resource_barriers.data()
        );
        gr->buffered_resource_barriers.clear();
    }
}

export void Flush_Gpu_Commands(CONTEXT* gr) {
    assert(gr);
    Flush_Resource_Barriers(gr);
    VHR(gr->cmdlist->Close());
    ID3D12_COMMAND_LIST* cmdlist = (ID3D12_COMMAND_LIST*)gr->cmdlist;
    gr->cmdqueue->ExecuteCommandLists(1, &cmdlist);
}

export void Begin_Draw_2D(CONTEXT* gr) {
    assert(gr);

    Flush_Gpu_Commands(gr);

    gr->d2d.device11on12->AcquireWrappedResources(
        &gr->d2d.wrapped_swapbuffers[gr->back_buffer_index],
        1
    );
    gr->d2d.context->SetTarget(gr->d2d.targets[gr->back_buffer_index]);
    gr->d2d.context->BeginDraw();
}

export void End_Draw_2D(CONTEXT* gr) {
    assert(gr);

    VHR(gr->d2d.context->EndDraw(NULL, NULL));

    gr->d2d.device11on12->ReleaseWrappedResources(
        &gr->d2d.wrapped_swapbuffers[gr->back_buffer_index],
        1
    );
    gr->d2d.context11->Flush();

    // Above calls will set back buffer state to PRESENT. We need to reflect this change
    // in 'resource_pool' by manually setting state.
    RESOURCE* back_buffer = Get_Resource_Info(
        &gr->resource_pool,
        gr->swapchain_buffers[gr->back_buffer_index]
    );
    back_buffer->state = D3D12_RESOURCE_STATE_PRESENT;
}

export void Finish_Gpu_Commands(CONTEXT* gr) {
    assert(gr);

    gr->frame_fence_counter += 1;

    VHR(gr->cmdqueue->Signal(gr->frame_fence, gr->frame_fence_counter));
    VHR(gr->frame_fence->SetEventOnCompletion(gr->frame_fence_counter, gr->frame_fence_event));
    WaitForSingleObject(gr->frame_fence_event, INFINITE);

    gr->cbv_srv_uav_gpu_heaps[gr->frame_index].size = 0;
    gr->upload_heaps[gr->frame_index].size = 0;
}

export inline TUPLE<RESOURCE_HANDLE, D3D12_CPU_DESCRIPTOR_HANDLE> Get_Back_Buffer(CONTEXT* gr) {
    return {
        gr->swapchain_buffers[gr->back_buffer_index],
        { .ptr = gr->rtv_heap.base.cpu_handle.ptr + gr->back_buffer_index * gr->rtv_heap.descriptor_size }
    };
}

export void Add_Transition_Barrier(
    CONTEXT* gr,
    RESOURCE_HANDLE handle,
    D3D12_RESOURCE_STATES state_after
) {
    assert(gr);

    RESOURCE* resource = Get_Resource_Info(&gr->resource_pool, handle);
    assert(resource);

    if (state_after != resource->state) {
        gr->buffered_resource_barriers.push_back(
            CD3DX12_RESOURCE_BARRIER::Transition(resource->raw, resource->state, state_after)
        );
        resource->state = state_after;

        if (gr->buffered_resource_barriers.size() > gr->buffered_resource_barriers.capacity() / 2) {
            Flush_Resource_Barriers(gr);
        }
    }
}

export RESOURCE_HANDLE Create_Committed_Resource(
    CONTEXT* gr,
    D3D12_HEAP_TYPE heap_type,
    D3D12_HEAP_FLAGS heap_flags,
    const D3D12_RESOURCE_DESC* desc,
    D3D12_RESOURCE_STATES initial_state,
    const D3D12_CLEAR_VALUE* clear_value
) {
    assert(gr && desc);

    ID3D12_RESOURCE* raw = NULL;
    VHR(gr->device->CreateCommittedResource(
        Get_Const_Ptr<D3D12_HEAP_PROPERTIES>({ .Type = heap_type }),
        heap_flags,
        desc,
        initial_state,
        clear_value,
        IID_PPV_ARGS(&raw)
    ));
    return Add_Resource(&gr->resource_pool, raw, initial_state);
}

export D3D12_CPU_DESCRIPTOR_HANDLE Allocate_Cpu_Descriptors(
    CONTEXT* gr,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    U32 num
) {
    assert(gr && num > 0);
    switch (type) {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
        return Allocate_Descriptors(&gr->cbv_srv_uav_cpu_heap, num).cpu_handle;
    case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
        return Allocate_Descriptors(&gr->rtv_heap, num).cpu_handle;
    case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
        return Allocate_Descriptors(&gr->dsv_heap, num).cpu_handle;
    }
    assert(0);
    return D3D12_CPU_DESCRIPTOR_HANDLE{};
}

export DESCRIPTOR Allocate_Gpu_Descriptors(CONTEXT* gr, U32 num) {
    assert(gr && num > 0);
    return Allocate_Descriptors(&gr->cbv_srv_uav_gpu_heaps[gr->frame_index], num);
}

export U32 Increment_Resource_Refcount(CONTEXT* gr, RESOURCE_HANDLE handle) {
    assert(gr);
    RESOURCE* resource = Get_Resource_Info(&gr->resource_pool, handle);
    return resource->raw->AddRef();
}

export U32 Release_Resource(CONTEXT* gr, RESOURCE_HANDLE handle) {
    assert(gr);
    RESOURCE* resource = Get_Resource_Info(&gr->resource_pool, handle);
    const U32 refcount = resource->raw->Release();
    if (refcount == 0) {
        *resource = {};
    }
    return refcount;
}

export inline ID3D12_RESOURCE* Get_Resource(CONTEXT* gr, RESOURCE_HANDLE handle) {
    return Get_Resource_Info(&gr->resource_pool, handle)->raw;
}

export inline U64 Get_Resource_Size(CONTEXT* gr, RESOURCE_HANDLE handle) {
    RESOURCE* resource = Get_Resource_Info(&gr->resource_pool, handle);
    assert(resource->desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
    return resource->desc.Width;
}

export U32 Increment_Pipeline_Refcount(CONTEXT* gr, PIPELINE_HANDLE handle) {
    assert(gr);

    PIPELINE* pipeline = Get_Pipeline_Info(&gr->pipeline.pool, handle);

    const U32 refcount = pipeline->pso->AddRef();
    if (pipeline->root_signature->AddRef() != refcount) {
        assert(0);
    }
    return refcount;
}

export U32 Release_Pipeline(CONTEXT* gr, PIPELINE_HANDLE handle) {
    PIPELINE* pipeline = Get_Pipeline_Info(&gr->pipeline.pool, handle);

    const U32 refcount = pipeline->pso->Release();
    if (pipeline->root_signature->Release() != refcount) {
        assert(0);
    }

    if (refcount == 0) {
        U64 hash_to_remove = 0;
        for (auto iter = gr->pipeline.map.begin(); iter != gr->pipeline.map.end(); ++iter) {
            if (iter->second.index == handle.index && iter->second.generation == handle.generation) {
                hash_to_remove = iter->first;
                break;
            }
        }
        assert(hash_to_remove != 0);
        gr->pipeline.map.erase(hash_to_remove);
        *pipeline = {};
    }

    return refcount;
}

export void Set_Pipeline_State(CONTEXT* gr, PIPELINE_HANDLE pipeline_handle) {
    // TODO: Do we need to unset pipeline state (0, 0)?
    PIPELINE* pipeline = Get_Pipeline_Info(&gr->pipeline.pool, pipeline_handle);

    if (pipeline_handle.index == gr->pipeline.current.index &&
        pipeline_handle.generation == gr->pipeline.current.generation) {
        return;
    }

    gr->cmdlist->SetPipelineState(pipeline->pso);
    if (pipeline->is_compute) {
        gr->cmdlist->SetComputeRootSignature(pipeline->root_signature);
    } else {
        gr->cmdlist->SetGraphicsRootSignature(pipeline->root_signature);
    }

    gr->pipeline.current = pipeline_handle;
}

export PIPELINE_HANDLE Create_Compute_Shader_Pipeline(
    CONTEXT* gr,
    D3D12_COMPUTE_PIPELINE_STATE_DESC* pso_desc
) {
    assert(gr && pso_desc && pso_desc->CS.pShaderBytecode);

    meow_state hasher = {};
    MeowBegin(&hasher, MeowDefaultSeed);

    MeowAbsorb(&hasher, pso_desc->CS.BytecodeLength, (void*)pso_desc->CS.pShaderBytecode);

    const U64 hash = MeowU64From(MeowEnd(&hasher, NULL), 0);

    if (auto found = gr->pipeline.map.find(hash); found != gr->pipeline.map.end()) {
        OutputDebugStringA("[graphics] Compute shader pipeline hit detected.\n");
        const PIPELINE_HANDLE handle = gr->pipeline.map[hash];
        Increment_Pipeline_Refcount(gr, handle);
        return handle;
    }

    ID3D12_ROOT_SIGNATURE* root_signature = NULL;
    VHR(gr->device->CreateRootSignature(
        0,
        pso_desc->CS.pShaderBytecode,
        pso_desc->CS.BytecodeLength,
        IID_PPV_ARGS(&root_signature)
    ));
    pso_desc->pRootSignature = root_signature;

    CD3DX12_PIPELINE_STATE_STREAM ps_stream(*pso_desc);

    ID3D12_PIPELINE_STATE* pso = NULL;
    VHR(gr->device->CreatePipelineState(
        Get_Const_Ptr<D3D12_PIPELINE_STATE_STREAM_DESC>({
            .SizeInBytes = sizeof ps_stream,
            .pPipelineStateSubobjectStream = &ps_stream,
        }),
        IID_PPV_ARGS(&pso)
    ));

    const PIPELINE_HANDLE handle = Add_Pipeline(&gr->pipeline.pool, pso, root_signature, true);
    gr->pipeline.map.insert({ hash, handle });
    return handle;
}

export PIPELINE_HANDLE Create_Graphics_Shader_Pipeline(
    CONTEXT* gr,
    D3D12_GRAPHICS_PIPELINE_STATE_DESC* pso_desc
) {
    assert(gr && pso_desc && pso_desc->VS.pShaderBytecode && pso_desc->PS.pShaderBytecode);

    meow_state hasher = {};
    MeowBegin(&hasher, MeowDefaultSeed);

    MeowAbsorb(&hasher, pso_desc->VS.BytecodeLength, (void*)pso_desc->VS.pShaderBytecode);
    MeowAbsorb(&hasher, pso_desc->PS.BytecodeLength, (void*)pso_desc->PS.pShaderBytecode);
    MeowAbsorb(&hasher, sizeof pso_desc->BlendState, (void*)&pso_desc->BlendState);
    MeowAbsorb(&hasher, sizeof pso_desc->SampleMask, (void*)&pso_desc->SampleMask);
    MeowAbsorb(&hasher, sizeof pso_desc->RasterizerState, (void*)&pso_desc->RasterizerState);
    MeowAbsorb(&hasher, sizeof pso_desc->DepthStencilState, (void*)&pso_desc->DepthStencilState);
    MeowAbsorb(&hasher, sizeof pso_desc->IBStripCutValue, (void*)&pso_desc->IBStripCutValue);
    MeowAbsorb(&hasher, sizeof pso_desc->PrimitiveTopologyType, (void*)&pso_desc->PrimitiveTopologyType);
    MeowAbsorb(&hasher, sizeof pso_desc->NumRenderTargets, (void*)&pso_desc->NumRenderTargets);
    MeowAbsorb(&hasher, sizeof pso_desc->RTVFormats, (void*)&pso_desc->RTVFormats[0]);
    MeowAbsorb(&hasher, sizeof pso_desc->DSVFormat, (void*)&pso_desc->DSVFormat);
    MeowAbsorb(&hasher, sizeof pso_desc->SampleDesc, (void*)&pso_desc->SampleDesc);
    MeowAbsorb(
        &hasher,
        sizeof pso_desc->InputLayout.NumElements,
        (void*)&pso_desc->InputLayout.NumElements
    );
    for (U32 i = 0; i < pso_desc->InputLayout.NumElements; ++i) {
        MeowAbsorb(
            &hasher,
            sizeof pso_desc->InputLayout.pInputElementDescs[i].SemanticIndex,
            (void*)&pso_desc->InputLayout.pInputElementDescs[i].SemanticIndex
        );
        MeowAbsorb(
            &hasher,
            sizeof pso_desc->InputLayout.pInputElementDescs[i].Format,
            (void*)&pso_desc->InputLayout.pInputElementDescs[i].Format
        );
        MeowAbsorb(
            &hasher,
            sizeof pso_desc->InputLayout.pInputElementDescs[i].InputSlot,
            (void*)&pso_desc->InputLayout.pInputElementDescs[i].InputSlot
        );
        MeowAbsorb(
            &hasher,
            sizeof pso_desc->InputLayout.pInputElementDescs[i].AlignedByteOffset,
            (void*)&pso_desc->InputLayout.pInputElementDescs[i].AlignedByteOffset
        );
        MeowAbsorb(
            &hasher,
            sizeof pso_desc->InputLayout.pInputElementDescs[i].InputSlotClass,
            (void*)&pso_desc->InputLayout.pInputElementDescs[i].InputSlotClass
        );
        MeowAbsorb(
            &hasher,
            sizeof pso_desc->InputLayout.pInputElementDescs[i].InstanceDataStepRate,
            (void*)&pso_desc->InputLayout.pInputElementDescs[i].InstanceDataStepRate
        );
    }
    // We don't support Stream Output.
    assert(pso_desc->StreamOutput.pSODeclaration == NULL);

    const U64 hash = MeowU64From(MeowEnd(&hasher, NULL), 0);

    if (auto found = gr->pipeline.map.find(hash); found != gr->pipeline.map.end()) {
        OutputDebugStringA("[graphics] Graphics shader pipeline hit detected.\n");
        const PIPELINE_HANDLE handle = gr->pipeline.map[hash];
        Increment_Pipeline_Refcount(gr, handle);
        return handle;
    }

    ID3D12_ROOT_SIGNATURE* root_signature = NULL;
    VHR(gr->device->CreateRootSignature(
        0,
        pso_desc->VS.pShaderBytecode,
        pso_desc->VS.BytecodeLength,
        IID_PPV_ARGS(&root_signature)
    ));
    pso_desc->pRootSignature = root_signature;

    CD3DX12_PIPELINE_STATE_STREAM ps_stream(*pso_desc);

    ID3D12_PIPELINE_STATE* pso = NULL;
    VHR(gr->device->CreatePipelineState(
        Get_Const_Ptr<D3D12_PIPELINE_STATE_STREAM_DESC>({
            .SizeInBytes = sizeof ps_stream,
            .pPipelineStateSubobjectStream = &ps_stream,
        }),
        IID_PPV_ARGS(&pso)
    ));

    const PIPELINE_HANDLE handle = Add_Pipeline(&gr->pipeline.pool, pso, root_signature, false);
    gr->pipeline.map.insert({ hash, handle });
    return handle;
}

export PIPELINE_HANDLE Create_Mesh_Shader_Pipeline(
    CONTEXT* gr,
    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC* pso_desc
) {
    assert(gr && pso_desc && pso_desc->MS.pShaderBytecode);

    meow_state hasher = {};
    MeowBegin(&hasher, MeowDefaultSeed);

    if (pso_desc->AS.pShaderBytecode) {
        MeowAbsorb(&hasher, pso_desc->AS.BytecodeLength, (void*)pso_desc->AS.pShaderBytecode);
    }
    if (pso_desc->MS.pShaderBytecode) {
        MeowAbsorb(&hasher, pso_desc->MS.BytecodeLength, (void*)pso_desc->MS.pShaderBytecode);
    }
    if (pso_desc->PS.pShaderBytecode) {
        MeowAbsorb(&hasher, pso_desc->PS.BytecodeLength, (void*)pso_desc->PS.pShaderBytecode);
    }
    MeowAbsorb(&hasher, sizeof pso_desc->BlendState, (void*)&pso_desc->BlendState);
    MeowAbsorb(&hasher, sizeof pso_desc->SampleMask, (void*)&pso_desc->SampleMask);
    MeowAbsorb(&hasher, sizeof pso_desc->RasterizerState, (void*)&pso_desc->RasterizerState);
    MeowAbsorb(&hasher, sizeof pso_desc->DepthStencilState, (void*)&pso_desc->DepthStencilState);
    MeowAbsorb(&hasher, sizeof pso_desc->PrimitiveTopologyType, (void*)&pso_desc->PrimitiveTopologyType);
    MeowAbsorb(&hasher, sizeof pso_desc->NumRenderTargets, (void*)&pso_desc->NumRenderTargets);
    MeowAbsorb(&hasher, sizeof pso_desc->RTVFormats, (void*)&pso_desc->RTVFormats[0]);
    MeowAbsorb(&hasher, sizeof pso_desc->DSVFormat, (void*)&pso_desc->DSVFormat);
    MeowAbsorb(&hasher, sizeof pso_desc->SampleDesc, (void*)&pso_desc->SampleDesc);

    const U64 hash = MeowU64From(MeowEnd(&hasher, NULL), 0);

    if (auto found = gr->pipeline.map.find(hash); found != gr->pipeline.map.end()) {
        OutputDebugStringA("[graphics] Mesh shader pipeline hit detected.\n");
        const PIPELINE_HANDLE handle = gr->pipeline.map[hash];
        Increment_Pipeline_Refcount(gr, handle);
        return handle;
    }

    ID3D12_ROOT_SIGNATURE* root_signature = NULL;
    VHR(gr->device->CreateRootSignature(
        0,
        pso_desc->MS.pShaderBytecode,
        pso_desc->MS.BytecodeLength,
        IID_PPV_ARGS(&root_signature)
    ));
    pso_desc->pRootSignature = root_signature;

    CD3DX12_PIPELINE_MESH_STATE_STREAM ps_stream(*pso_desc);

    ID3D12_PIPELINE_STATE* pso = NULL;
    VHR(gr->device->CreatePipelineState(
        Get_Const_Ptr<D3D12_PIPELINE_STATE_STREAM_DESC>({
            .SizeInBytes = sizeof ps_stream,
            .pPipelineStateSubobjectStream = &ps_stream,
        }),
        IID_PPV_ARGS(&pso)
    ));

    const PIPELINE_HANDLE handle = Add_Pipeline(&gr->pipeline.pool, pso, root_signature, false);
    gr->pipeline.map.insert({ hash, handle });
    return handle;
}

export TUPLE<U8*, D3D12_GPU_VIRTUAL_ADDRESS> Allocate_Upload_Memory(
    CONTEXT* gr,
    U32 mem_size
) {
    assert(gr && mem_size > 0);
    auto [cpu_addr, gpu_addr] = Allocate_Gpu_Memory(&gr->upload_heaps[gr->frame_index], mem_size);

    if (cpu_addr == NULL && gpu_addr == 0) {
        OutputDebugStringA("[graphics] Upload memory heap exhausted - draining a GPU...\n");

        Flush_Gpu_Commands(gr);
        Finish_Gpu_Commands(gr);
        Begin_Frame(gr);

        auto t = Allocate_Gpu_Memory(&gr->upload_heaps[gr->frame_index], mem_size);
        cpu_addr = eastl::get<0>(t);
        gpu_addr = eastl::get<1>(t);
    }
    assert(cpu_addr != NULL && gpu_addr != 0);
    return { cpu_addr, gpu_addr };
}

export template<typename T> inline TUPLE<SPAN<T>, ID3D12_RESOURCE*, U64> Allocate_Upload_Buffer_Region(
    CONTEXT* gr,
    U32 num
) {
    assert(gr && num > 0);
    const U32 mem_size = num * sizeof T;
    const auto [cpu_addr, gpu_addr] = Allocate_Upload_Memory(gr, mem_size);
    const U32 aligned_size = (mem_size + (upload_alloc_alignment - 1)) & ~(upload_alloc_alignment - 1);
    return {
        { (T*)cpu_addr, num },
        gr->upload_heaps[gr->frame_index].heap,
        gr->upload_heaps[gr->frame_index].size - aligned_size,
    };
}

export inline D3D12_GPU_DESCRIPTOR_HANDLE Copy_Descriptors_To_Gpu_Heap(
    CONTEXT* gr,
    U32 num,
    D3D12_CPU_DESCRIPTOR_HANDLE src_base
) {
    assert(gr && num > 0);
    const DESCRIPTOR base = Allocate_Gpu_Descriptors(gr, num);
    gr->device->CopyDescriptorsSimple(
        num,
        base.cpu_handle,
        src_base,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    );
    return base.gpu_handle;
}

export template<typename T> inline bool Is_Valid(const T handle) {
    return handle.index != 0 && handle.generation != 0;
}

} // namespace graphics
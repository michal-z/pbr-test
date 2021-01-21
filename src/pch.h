#pragma once
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define D3DX12_NO_STATE_OBJECT_HELPERS
#include "d3d12.h"
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d2d1_3.h>
#include <dwrite_3.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define EASTL_RTTI_ENABLED 0
#define EASTL_EXCEPTIONS_ENABLED 0
#include "EASTL/vector.h"
#include "EASTL/hash_map.h"
#include "EASTL/algorithm.h"
#include "EASTL/sort.h"
#include "EASTL/finally.h"
#include "EASTL/span.h"
#include "EASTL/tuple.h"

#include "cgltf.h"
#include "imgui/imgui.h"
#include "DirectXMath/DirectXMath.h"
using namespace DirectX;

#pragma warning(push)
#pragma warning(disable : 4324 4505)
#include "meow_hash_x64_aesni.h"
#pragma warning(pop)

#ifdef _DEBUG
inline void VHR(HRESULT hr) {
    if (FAILED(hr)) {
        assert(0);
    }
}
#else
inline void VHR(HRESULT) {}
#endif

template<typename T> const T* Get_Const_Ptr(const T& obj) {
    return &obj;
}

#define MZ_SAFE_RELEASE(obj) if ((obj)) { (obj)->Release(); (obj) = nullptr; }
#define MZ_CONCAT_(x, y) x##y
#define MZ_CONCAT(x, y) MZ_CONCAT_(x, y)
#define MZ_UNIQUE_NAME(prefix) MZ_CONCAT(prefix, __COUNTER__)
#define MZ_DEFER(...) auto MZ_UNIQUE_NAME(_mz_defer__) = eastl::make_finally([&]{ __VA_ARGS__; })

template <typename T, typename... Ts>
using TUPLE = eastl::tuple<T, Ts...>;

template <typename T, size_t Extent = eastl::dynamic_extent>
using SPAN = eastl::span<T, Extent>;

template <typename T, typename Allocator = EASTLAllocatorType>
using VECTOR = eastl::vector<T, Allocator>;

typedef char S8;
typedef short S16;
typedef int S32;
typedef long long S64;
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;
typedef float F32;
typedef double F64;

typedef ID3D12Device8 ID3D12_DEVICE;
typedef ID3D12CommandList ID3D12_COMMAND_LIST;
typedef ID3D12GraphicsCommandList6 ID3D12_GRAPHICS_COMMAND_LIST;
typedef ID3D12Resource ID3D12_RESOURCE;
typedef ID3D12CommandQueue ID3D12_COMMAND_QUEUE;
typedef ID3D12CommandAllocator ID3D12_COMMAND_ALLOCATOR;
typedef ID3D12Fence ID3D12_FENCE;
typedef ID3D12DescriptorHeap ID3D12_DESCRIPTOR_HEAP;
typedef ID3D12PipelineState ID3D12_PIPELINE_STATE;
typedef ID3D12RootSignature ID3D12_ROOT_SIGNATURE;

typedef IDXGIFactory4 IDXGI_FACTORY;
typedef IDXGIDevice IDXGI_DEVICE;
typedef IDXGISwapChain3 IDXGI_SWAP_CHAIN;
typedef IDXGISurface IDXGI_SURFACE;

typedef ID2D1Factory7 ID2D1_FACTORY;
typedef ID2D1Device6 ID2D1_DEVICE;
typedef ID2D1DeviceContext6 ID2D1_DEVICE_CONTEXT;
typedef ID2D1Bitmap1 ID2D1_BITMAP;
typedef ID2D1SolidColorBrush ID2D1_SOLID_COLOR_BRUSH;

typedef ID3D11Device ID3D11_DEVICE;
typedef ID3D11DeviceContext ID3D11_DEVICE_CONTEXT;
typedef ID3D11Resource ID3D11_RESOURCE;
typedef ID3D11On12Device2 ID3D11ON12_DEVICE;

typedef IDWriteFactory7 IDWRITE_FACTORY;
typedef IDWriteTextFormat IDWRITE_TEXT_FORMAT;

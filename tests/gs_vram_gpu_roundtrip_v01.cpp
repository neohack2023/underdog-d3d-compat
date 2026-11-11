// Underdog / PS2 GS VRAM bridge feasibility fixture.
// Independent synthetic 4 MiB CPU -> GPU -> CPU copy with explicit synchronization.
// Does NOT render a PS2 frame, implement GSRasterBackend, or run any game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;
constexpr UINT64 kSize = 4ull * 1024ull * 1024ull;
static int bad(const char* step, HRESULT hr) {
    std::fprintf(stderr, "{\"status\":\"FAIL\",\"step\":\"%s\",\"hr\":\"0x%08lx\"}\n",
                 step, static_cast<unsigned long>(hr));
    return 2;
}
static D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES p{};
    p.Type=type;
    p.CPUPageProperty=D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    p.MemoryPoolPreference=D3D12_MEMORY_POOL_UNKNOWN;
    p.CreationNodeMask=1;
    p.VisibleNodeMask=1;
    return p;
}
static D3D12_RESOURCE_DESC buffer(UINT64 size) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width=size;
    d.Height=1;
    d.DepthOrArraySize=1;
    d.MipLevels=1;
    d.Format=DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count=1;
    d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}
static uint64_t fnv64(const uint8_t* data, size_t bytes) {
    uint64_t h=14695981039346656037ull;
    for(size_t i=0;i<bytes;++i) { h^=data[i]; h*=1099511628211ull; }
    return h;
}
int main(int argc,char** argv) {
    const bool warp=argc>1 && std::strcmp(argv[1],"warp")==0;
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr=CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if(FAILED(hr)) return bad("dxgi_factory",hr);
    ComPtr<IDXGIAdapter1> adapter;
    if(warp) {
        hr=factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        if(FAILED(hr)) return bad("warp_adapter",hr);
    } else {
        for(UINT i=0;;++i) {
            ComPtr<IDXGIAdapter1> a;
            hr=factory->EnumAdapters1(i,&a);
            if(hr==DXGI_ERROR_NOT_FOUND) break;
            if(FAILED(hr)) return bad("enumerate",hr);
            DXGI_ADAPTER_DESC1 d{};
            if(SUCCEEDED(a->GetDesc1(&d)) && d.VendorId==0x8086u && d.DeviceId==0x1916u) {
                adapter=a;
                break;
            }
        }
        if(!adapter) return bad("intel_hd520_not_found",E_FAIL);
    }
    DXGI_ADAPTER_DESC1 info{};
    adapter->GetDesc1(&info);
    ComPtr<ID3D12Device> dev;
    hr=D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev));
    if(FAILED(hr)) return bad("create_device",hr);
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    hr=dev->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue));
    if(FAILED(hr)) return bad("queue",hr);
    auto desc=buffer(kSize);
    auto uploadHeap=heap(D3D12_HEAP_TYPE_UPLOAD);
    auto defaultHeap=heap(D3D12_HEAP_TYPE_DEFAULT);
    auto readbackHeap=heap(D3D12_HEAP_TYPE_READBACK);
    ComPtr<ID3D12Resource> upload,gpu,readback;
    hr=dev->CreateCommittedResource(&uploadHeap,D3D12_HEAP_FLAG_NONE,&desc,
             D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload));
    if(FAILED(hr)) return bad("upload_alloc",hr);
    hr=dev->CreateCommittedResource(&defaultHeap,D3D12_HEAP_FLAG_NONE,&desc,
             D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&gpu));
    if(FAILED(hr)) return bad("default_alloc",hr);
    hr=dev->CreateCommittedResource(&readbackHeap,D3D12_HEAP_FLAG_NONE,&desc,
             D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback));
    if(FAILED(hr)) return bad("readback_alloc",hr);
    std::vector<uint8_t> original(static_cast<size_t>(kSize));
    uint32_t state=0x13579BDFu;
    for(auto& byte:original) {
        state=state*1664525u+1013904223u;
        byte=static_cast<uint8_t>((state>>17u)&255u);
    }
    void* dst=nullptr;
    D3D12_RANGE noRead{0,0};
    hr=upload->Map(0,&noRead,&dst);
    if(FAILED(hr) || !dst) return bad("map_upload",hr);
    std::memcpy(dst,original.data(),original.size());
    D3D12_RANGE written{0,static_cast<SIZE_T>(kSize)};
    upload->Unmap(0,&written);

    ComPtr<ID3D12CommandAllocator> allocator;
    hr=dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator));
    if(FAILED(hr)) return bad("allocator",hr);
    ComPtr<ID3D12GraphicsCommandList> list;
    hr=dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,
                              IID_PPV_ARGS(&list));
    if(FAILED(hr)) return bad("list",hr);
    list->CopyBufferRegion(gpu.Get(),0,upload.Get(),0,kSize);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=gpu.Get();
    barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1,&barrier);
    list->CopyBufferRegion(readback.Get(),0,gpu.Get(),0,kSize);
    hr=list->Close();
    if(FAILED(hr)) return bad("close",hr);
    ComPtr<ID3D12Fence> fence;
    hr=dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence));
    if(FAILED(hr)) return bad("fence",hr);
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event) return bad("event",HRESULT_FROM_WIN32(GetLastError()));
    const ULONGLONG start=GetTickCount64();
    ID3D12CommandList* lists[]={list.Get()};
    queue->ExecuteCommandLists(1,lists);
    hr=queue->Signal(fence.Get(),1);
    if(FAILED(hr)) {CloseHandle(event);return bad("signal",hr);}
    hr=fence->SetEventOnCompletion(1,event);
    if(FAILED(hr)) {CloseHandle(event);return bad("set_fence_event",hr);}
    DWORD wait=WaitForSingleObject(event,10000);
    CloseHandle(event);
    if(wait!=WAIT_OBJECT_0) return bad("gpu_fence_timeout",E_FAIL);
    const ULONGLONG elapsed=GetTickCount64()-start;
    void* source=nullptr;
    D3D12_RANGE readable{0,static_cast<SIZE_T>(kSize)};
    hr=readback->Map(0,&readable,&source);
    if(FAILED(hr) || !source) return bad("map_readback",hr);
    const auto* actual=static_cast<const uint8_t*>(source);
    size_t different=0;
    for(size_t i=0;i<original.size();++i) if(actual[i]!=original[i]) ++different;
    const uint64_t expectedHash=fnv64(original.data(),original.size());
    const uint64_t actualHash=fnv64(actual,original.size());
    readback->Unmap(0,&noRead);
    const HRESULT removed=dev->GetDeviceRemovedReason();
    std::printf("{\"status\":\"%s\",\"adapter\":\"%s\",\"vendor_id\":%u,"
                "\"device_id\":%u,\"bytes\":%llu,\"different_bytes\":%llu,"
                "\"hash_expected\":\"%016llx\",\"hash_actual\":\"%016llx\","
                "\"fence_wait_ms\":%llu,\"device_removed_hr\":\"0x%08lx\","
                "\"actual_gpu_copy\":true,\"actual_gs_render\":false}\n",
                different==0 && SUCCEEDED(removed) ? "PASS":"FAIL",
                warp?"WARP":"Intel HD 520",info.VendorId,info.DeviceId,
                static_cast<unsigned long long>(kSize),
                static_cast<unsigned long long>(different),
                static_cast<unsigned long long>(expectedHash),
                static_cast<unsigned long long>(actualHash),
                static_cast<unsigned long long>(elapsed),static_cast<unsigned long>(removed));
    return different==0 && SUCCEEDED(removed) ? 0:3;
}

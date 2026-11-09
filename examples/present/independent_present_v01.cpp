// AIOS Underdog standalone D3D12 Win32 swap-chain experiment V01.
// Deliberately separate from the mesh/BVH executable: independent graphics application.
// Uses application-owned compatible texture budget; no injected/protected-process code.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <thread>
#include <cstdint>
#include "AIOS_UNDERDOG_FENCED_TEXTURE_BUDGET_V02.h"
using Microsoft::WRL::ComPtr;
#define CK(X) {HRESULT h=(X);if(FAILED(h)){printf(#X "_FAILED=0x%08lX\n",(unsigned long)h);fflush(stdout);return 10;}}
static LRESULT CALLBACK WndProc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_CLOSE){DestroyWindow(w);return 0;}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
 return DefWindowProcW(w,m,a,b);
}
static double percentile(std::vector<double> vals,double k){
 if(vals.empty())return -1;
 std::sort(vals.begin(),vals.end());
 return vals[std::min(vals.size()-1,(size_t)((vals.size()-1)*k))];
}
int main(){
 puts("AIOS_UNDERDOG_D3D12_SWAPCHAIN_PRESENT_V01");fflush(stdout);
 HINSTANCE instance=GetModuleHandleW(nullptr);
 WNDCLASSW cls={};cls.lpfnWndProc=WndProc;cls.hInstance=instance;
 cls.lpszClassName=L"AIOS_Underdog_D3D12_Win";
 if(!RegisterClassW(&cls)){puts("WINDOW_CLASS_FAIL");return 2;}
 HWND hwnd=CreateWindowExW(0,cls.lpszClassName,L"AIOS Underdog D3D12 Native Present | Independent App",
   WS_OVERLAPPEDWINDOW|WS_VISIBLE,100,80,840,520,nullptr,nullptr,instance,nullptr);
 if(!hwnd){puts("WINDOW_CREATE_FAIL");return 3;}
 ComPtr<IDXGIFactory4> factory;
 CK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;
 for(UINT i=0;factory->EnumAdapters1(i,&adapter)==S_OK;i++){
  DXGI_ADAPTER_DESC1 d={};adapter->GetDesc1(&d);
  if(!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){printf("GPU_VENDOR=0x%04X GPU_DEVICE=0x%04X\n",d.VendorId,d.DeviceId);break;}
  adapter.Reset();
 }
 if(!adapter)return 4;
 ComPtr<ID3D12Device> device;
 CK(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 AIOSUnderdog::FencedTextureBudget guard(device.Get(),2*1024*1024);
 AIOSUnderdog::TextureRef t1,t2,t3;
 auto h1=guard.create2D(800,800,DXGI_FORMAT_R16_UNORM,t1);
 auto h2=guard.create2D(800,800,DXGI_FORMAT_R16_UNORM,t2);
 printf("INDEPENDENT_APP_BUDGET_FIRST=0x%08lX SECOND=0x%08lX\n",(unsigned long)h1,(unsigned long)h2);
 if(FAILED(h1)||h2!=E_OUTOFMEMORY)return 5;
 // No GPU command ever uses the budgeted texture; free it after we have submitted no work that references it.
 if(!guard.retireAfterFence(t1.id,1))return 6;
 if(guard.collectCompleted(0)!=0)return 7;
 // Dedicated fence signal before freeing, so allocator lifecycle is gated by actual GPU completion.
 D3D12_COMMAND_QUEUE_DESC q={};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> queue;
 CK(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12Fence> fence;
 CK(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 HANDLE ev=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ev)return 8;
 CK(queue->Signal(fence.Get(),1));
 CK(fence->SetEventOnCompletion(1,ev));
 if(WaitForSingleObject(ev,1500)!=WAIT_OBJECT_0)return 9;
 if(guard.collectCompleted(fence->GetCompletedValue())!=1)return 11;
 auto h3=guard.create2D(800,800,DXGI_FORMAT_R16_UNORM,t3);
 printf("INDEPENDENT_APP_BUDGET_AFTER_FENCE=0x%08lX\n",(unsigned long)h3);
 if(FAILED(h3))return 12;
 DXGI_SWAP_CHAIN_DESC1 sc={};
 sc.Width=800;sc.Height=450;sc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 sc.SampleDesc.Count=1;sc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sc.BufferCount=2;
 sc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 ComPtr<IDXGISwapChain1> temp;
 CK(factory->CreateSwapChainForHwnd(queue.Get(),hwnd,&sc,nullptr,nullptr,&temp));
 CK(factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER));
 ComPtr<IDXGISwapChain3> swap;
 CK(temp.As(&swap));
 ComPtr<ID3D12Resource> buffers[2];
 for(unsigned i=0;i<2;i++)CK(swap->GetBuffer(i,IID_PPV_ARGS(&buffers[i])));
 D3D12_DESCRIPTOR_HEAP_DESC dh={};dh.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;dh.NumDescriptors=2;
 ComPtr<ID3D12DescriptorHeap> rtv;
 CK(device->CreateDescriptorHeap(&dh,IID_PPV_ARGS(&rtv)));
 UINT step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
 for(unsigned i=0;i<2;i++){
  auto cpu=rtv->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=i*step;
  device->CreateRenderTargetView(buffers[i].Get(),nullptr,cpu);
 }
 ComPtr<ID3D12CommandAllocator> alloc;
 CK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
 ComPtr<ID3D12GraphicsCommandList> list;
 CK(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list)));
 CK(list->Close());
 D3D12_QUERY_HEAP_DESC hd={};hd.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;hd.Count=120;
 ComPtr<ID3D12QueryHeap> query;
 CK(device->CreateQueryHeap(&hd,IID_PPV_ARGS(&query)));
 D3D12_RESOURCE_DESC timing={};timing.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
 timing.Width=120*8;timing.Height=1;timing.DepthOrArraySize=1;
 timing.MipLevels=1;timing.SampleDesc.Count=1;timing.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 D3D12_HEAP_PROPERTIES rb={};rb.Type=D3D12_HEAP_TYPE_READBACK;
 rb.CreationNodeMask=1;rb.VisibleNodeMask=1;
 ComPtr<ID3D12Resource> readback;
 CK(device->CreateCommittedResource(&rb,D3D12_HEAP_FLAG_NONE,&timing,
  D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
 UINT64 frequency=0;CK(queue->GetTimestampFrequency(&frequency));
 ComPtr<IDXGIAdapter3> adapter3;adapter.As(&adapter3);
 auto start=std::chrono::steady_clock::now(),previous=start;
 std::vector<double> presents,intervals,fences;
 unsigned frames=0,completed=0;bool running=true;
 for(unsigned i=0;i<60 && running;i++){
  MSG msg;
  while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
   if(msg.message==WM_QUIT){running=false;break;}
   TranslateMessage(&msg);DispatchMessageW(&msg);
  }
  if(!running)break;
  const auto frameStart=std::chrono::steady_clock::now();
  if(i)intervals.push_back(std::chrono::duration<double,std::milli>(frameStart-previous).count());
  previous=frameStart;
  CK(alloc->Reset());CK(list->Reset(alloc.Get(),nullptr));
  unsigned back=swap->GetCurrentBackBufferIndex();
  D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource=buffers[back].Get();
  barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;
  list->ResourceBarrier(1,&barrier);
  list->EndQuery(query.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2);
  auto cpu=rtv->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=back*step;
  const float color[4]={0.11f+(i%12)*.018f,0.25f,0.44f-(i%10)*.022f,1.0f};
  list->ClearRenderTargetView(cpu,color,0,nullptr);
  list->EndQuery(query.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2+1);
  barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_PRESENT;
  list->ResourceBarrier(1,&barrier);
  list->ResolveQueryData(query.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2,2,readback.Get(),i*16);
  CK(list->Close());
  ID3D12CommandList* commands[]={list.Get()};
  queue->ExecuteCommandLists(1,commands);
  const auto presentBegin=std::chrono::steady_clock::now();
  HRESULT ph=swap->Present(1,0);
  if(FAILED(ph)){printf("PRESENT_FAILURE_FRAME=%u HRESULT=0x%08lX\n",i,(unsigned long)ph);return 13;}
  presents.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-presentBegin).count());
  CK(queue->Signal(fence.Get(),i+2));
  CK(fence->SetEventOnCompletion(i+2,ev));
  auto fenceBegin=std::chrono::steady_clock::now();
  if(WaitForSingleObject(ev,2000)!=WAIT_OBJECT_0){printf("PRESENT_FENCE_TIMEOUT_FRAME=%u\n",i);return 14;}
  fences.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-fenceBegin).count());
  frames++;completed++;
 }
 printf("SWAPCHAIN_FRAMES=%u PRESENT_SUCCESS=%u\n",frames,completed);
 if(completed<30)return 15;
 D3D12_RANGE range={0,completed*16};void* mapped=nullptr;
 CK(readback->Map(0,&range,&mapped));
 const auto* times=(const uint64_t*)mapped;
 std::vector<double> gpu;
 for(unsigned i=0;i<completed;i++)gpu.push_back(double(times[i*2+1]-times[i*2])*1000.0/frequency);
 D3D12_RANGE zero={0,0};readback->Unmap(0,&zero);
 printf("GPU_CLEAR_P50_MS=%.4f GPU_CLEAR_P95_MS=%.4f\n",percentile(gpu,.5),percentile(gpu,.95));
 printf("PRESENT_CALL_P50_MS=%.4f PRESENT_CALL_P95_MS=%.4f\n",percentile(presents,.5),percentile(presents,.95));
 printf("WINDOW_FRAME_INTERVAL_P50_MS=%.4f WINDOW_FRAME_INTERVAL_P95_MS=%.4f\n",percentile(intervals,.5),percentile(intervals,.95));
 printf("QUEUE_FENCE_P95_MS=%.4f\n",percentile(fences,.95));
 DXGI_QUERY_VIDEO_MEMORY_INFO mem={};
 if(adapter3 && SUCCEEDED(adapter3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&mem)))
  printf("DXGI_USAGE_END_BYTES=%llu DXGI_BUDGET_BYTES=%llu\n",
   (unsigned long long)mem.CurrentUsage,(unsigned long long)mem.Budget);
 printf("DEVICE_REMOVED_REASON=0x%08lX\n",(unsigned long)device->GetDeviceRemovedReason());
 printf("TEST_WALL_SECONDS=%.3f\n",std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());
 CloseHandle(ev);
 puts("END_D3D12_HWND_PRESENT_TEST");
 return 0;
}

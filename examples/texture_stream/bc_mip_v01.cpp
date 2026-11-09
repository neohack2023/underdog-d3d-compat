// AIOS Underdog: bounded D3D12 BCn + mip upload/readback integrity V01.
// Self-owned diagnostic process, exact byte comparison per mip after GPU copy.
// No Fortnite process access, driver replacements, deliberate TDR, or injection.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <chrono>
using Microsoft::WRL::ComPtr;
#define CK(X) {HRESULT h=(X);if(FAILED(h)){printf("FAILED_%s=0x%08lX\n",#X,(unsigned long)h);fflush(stdout);return 10;}}
constexpr UINT MIPS=4;
static D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE type){
 D3D12_HEAP_PROPERTIES p={};p.Type=type;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;
}
static D3D12_RESOURCE_DESC buf(UINT64 size){
 D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;
 d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;
}
static uint8_t pattern(UINT f,UINT mip,UINT row,UINT byte){
 return uint8_t((f*17+mip*71+row*13+byte*7+3)&255);
}
int main(){
 puts("AIOS_UNDERDOG_BC_MIP_STREAM_V01");fflush(stdout);
 ComPtr<IDXGIFactory4> factory;CK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;
 for(UINT i=0;factory->EnumAdapters1(i,&adapter)==S_OK;i++){
  DXGI_ADAPTER_DESC1 d={};adapter->GetDesc1(&d);
  if(!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){
   printf("ADAPTER_VENDOR=0x%04X DEVICE=0x%04X\n",d.VendorId,d.DeviceId);break;
  }
  adapter.Reset();
 }
 if(!adapter)return 2;
 ComPtr<ID3D12Device> dev;CK(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)));
 const DXGI_FORMAT formats[]={DXGI_FORMAT_BC1_UNORM,DXGI_FORMAT_BC3_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM};
 const char* names[]={"BC1","BC3","RGBA8"};
 ComPtr<ID3D12CommandQueue> queue;
 D3D12_COMMAND_QUEUE_DESC q={};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 CK(dev->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12Fence> fence;CK(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 HANDLE ev=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ev)return 3;
 uint64_t frameSignal=0;uint64_t bytesCopied=0;
 unsigned subresourcePass=0,formatPass=0,frames=0;
 ComPtr<IDXGIAdapter3> adapter3;adapter.As(&adapter3);
 UINT64 memoryPeak=0,budget=0;
 for(UINT f=0;f<3;f++){
  D3D12_FEATURE_DATA_FORMAT_SUPPORT support={};support.Format=formats[f];
  auto sh=dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support));
  printf("FORMAT=%s FEATURE_QUERY=0x%08lX SUPPORT1=0x%X SUPPORT2=0x%X\n",
    names[f],(unsigned long)sh,(unsigned)support.Support1,(unsigned)support.Support2);
  if(FAILED(sh)||(support.Support1&D3D12_FORMAT_SUPPORT1_TEXTURE2D)==0)return 4;
  D3D12_RESOURCE_DESC tex={};
  tex.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;tex.Width=128;tex.Height=128;
  tex.DepthOrArraySize=1;tex.MipLevels=MIPS;tex.Format=formats[f];tex.SampleDesc.Count=1;
  ComPtr<ID3D12Resource> gpu;
  const auto def=hp(D3D12_HEAP_TYPE_DEFAULT);
  CK(dev->CreateCommittedResource(&def,D3D12_HEAP_FLAG_NONE,&tex,D3D12_RESOURCE_STATE_COPY_DEST,
    nullptr,IID_PPV_ARGS(&gpu)));
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[MIPS]={};
  UINT numRows[MIPS]={};UINT64 rowSizes[MIPS]={},total=0;
  dev->GetCopyableFootprints(&tex,0,MIPS,0,footprints,numRows,rowSizes,&total);
  if(!total||total>1024*1024)return 5;
  auto buffer=buf(total);
  const auto up=hp(D3D12_HEAP_TYPE_UPLOAD),rb=hp(D3D12_HEAP_TYPE_READBACK);
  ComPtr<ID3D12Resource> upload,readback;
  CK(dev->CreateCommittedResource(&up,D3D12_HEAP_FLAG_NONE,&buffer,
    D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)));
  CK(dev->CreateCommittedResource(&rb,D3D12_HEAP_FLAG_NONE,&buffer,
    D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
  ComPtr<ID3D12CommandAllocator> alloc;
  CK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
  ComPtr<ID3D12GraphicsCommandList> list;
  CK(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list)));
  const auto tick=std::chrono::steady_clock::now();
  for(UINT iteration=0;iteration<8;iteration++){
   if(iteration){CK(alloc->Reset());CK(list->Reset(alloc.Get(),nullptr));}
   void* mapped=nullptr;D3D12_RANGE empty={0,0};
   CK(upload->Map(0,&empty,&mapped));
   memset(mapped,0,(size_t)total);
   for(UINT mip=0;mip<MIPS;mip++){
    const auto& fp=footprints[mip];
    auto* base=(uint8_t*)mapped+fp.Offset;
    for(UINT row=0;row<numRows[mip];row++){
     uint8_t* dest=base+row*fp.Footprint.RowPitch;
     for(UINT byte=0;byte<rowSizes[mip];byte++)dest[byte]=pattern(f,iteration+mip,row,byte);
    }
   }
   upload->Unmap(0,nullptr);
   if(iteration){
    D3D12_RESOURCE_BARRIER bar={};bar.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    bar.Transition.pResource=gpu.Get();bar.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    bar.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
    bar.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
    list->ResourceBarrier(1,&bar);
   }
   for(UINT mip=0;mip<MIPS;mip++){
    D3D12_TEXTURE_COPY_LOCATION from={};from.pResource=upload.Get();
    from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;from.PlacedFootprint=footprints[mip];
    D3D12_TEXTURE_COPY_LOCATION to={};to.pResource=gpu.Get();
    to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.SubresourceIndex=mip;
    list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
   }
   D3D12_RESOURCE_BARRIER trans={};trans.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   trans.Transition.pResource=gpu.Get();trans.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   trans.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;trans.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
   list->ResourceBarrier(1,&trans);
   for(UINT mip=0;mip<MIPS;mip++){
    D3D12_TEXTURE_COPY_LOCATION from={};from.pResource=gpu.Get();
    from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;from.SubresourceIndex=mip;
    D3D12_TEXTURE_COPY_LOCATION to={};to.pResource=readback.Get();
    to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprints[mip];
    list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
   }
   CK(list->Close());
   ID3D12CommandList* cmds[]={list.Get()};
   queue->ExecuteCommandLists(1,cmds);
   ++frameSignal;CK(queue->Signal(fence.Get(),frameSignal));
   CK(fence->SetEventOnCompletion(frameSignal,ev));
   if(WaitForSingleObject(ev,2000)!=WAIT_OBJECT_0){
    printf("FENCE_TIMEOUT=%s ITER=%u DEVICE_REASON=0x%08lX\n",names[f],iteration,(unsigned long)dev->GetDeviceRemovedReason());
    CloseHandle(ev);return 6;
   }
   void* result=nullptr;D3D12_RANGE readable={0,(SIZE_T)total};
   CK(readback->Map(0,&readable,&result));
   unsigned errors=0;
   for(UINT mip=0;mip<MIPS;mip++){
    const auto& fp=footprints[mip];
    auto* base=(const uint8_t*)result+fp.Offset;
    for(UINT row=0;row<numRows[mip];row++){
     const uint8_t* from=base+row*fp.Footprint.RowPitch;
     for(UINT byte=0;byte<rowSizes[mip];byte++){
      if(from[byte]!=pattern(f,iteration+mip,row,byte))errors++;
     }
    }
    if(errors==0)subresourcePass++;
   }
   readback->Unmap(0,&empty);
   if(errors){printf("MISMATCH_FORMAT=%s ITER=%u ERRORS=%u\n",names[f],iteration,errors);return 7;}
   bytesCopied+=total;frames++;
   if(adapter3){
    DXGI_QUERY_VIDEO_MEMORY_INFO vm={};
    if(SUCCEEDED(adapter3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&vm))){
     budget=vm.Budget;memoryPeak=(std::max)(memoryPeak,vm.CurrentUsage);
    }
   }
  }
  const auto end=std::chrono::steady_clock::now();
  printf("FORMAT_PASS=%s MIPS=%u FRAMES=8 ROWS_0=%u PITCH_0=%u BYTES_TOTAL=%llu WALL_MS=%.3f\n",
    names[f],MIPS,numRows[0],footprints[0].Footprint.RowPitch,(unsigned long long)total,
    std::chrono::duration<double,std::milli>(end-tick).count());
  formatPass++;
 }
 CloseHandle(ev);
 printf("FORMATS_PASS=%u MIPS_COPIED_AND_CHECKED=%u FRAMES_COMPLETED=%u BYTES_TRANSFERRED=%llu\n",
   formatPass,subresourcePass,frames,(unsigned long long)bytesCopied);
 printf("DXGI_MEM_PEAK=%llu DXGI_MEM_BUDGET=%llu\n",(unsigned long long)memoryPeak,(unsigned long long)budget);
 printf("DEVICE_REMOVED_REASON=0x%08lX\n",(unsigned long)dev->GetDeviceRemovedReason());
 puts("END_BC_MIP_STREAM_V01");
 return (frames==24 && subresourcePass==96 && formatPass==3)?0:8;
}

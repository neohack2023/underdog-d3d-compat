// AIOS D3D12 standalone graphics PSO, rasterization, texture readback V01.
// Offscreen graphics workload, no Fortnite process or protected runtime interaction.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <vector>
#include <algorithm>
using Microsoft::WRL::ComPtr;
#define CK(X) {HRESULT z=(X);printf(#X "=0x%08lX\n",(unsigned long)z);fflush(stdout);if(FAILED(z))return 10;}
D3D12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE type){
 D3D12_HEAP_PROPERTIES h={};h.Type=type;h.CreationNodeMask=1;h.VisibleNodeMask=1;return h;
}
int main(){
 puts("AIOS_D3D12_GRAPHICS_PSO_TEXTURE_MIX_V01");fflush(stdout);
 ComPtr<IDXGIFactory4> factory;
 CK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;
 for(UINT i=0;factory->EnumAdapters1(i,&adapter)==S_OK;i++){
  DXGI_ADAPTER_DESC1 d={};adapter->GetDesc1(&d);
  if(!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))break;
  adapter.Reset();
 }
 if(!adapter)return 12;
 ComPtr<ID3D12Device> dev;
 CK(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)));
 const char* src=
 "struct VOut {float4 p:SV_POSITION;float3 c:COLOR;};"
 "VOut VS(uint id:SV_VertexID) {VOut o;"
 "float2 p[3]={float2(-0.8,-0.8),float2(0,0.8),float2(0.8,-0.8)};"
 "float3 c[3]={float3(1,0,0),float3(0,1,0),float3(0,0,1)};"
 "o.p=float4(p[id],0,1);o.c=c[id];return o;}"
 "float4 PS(VOut v):SV_TARGET {return float4(v.c,1);}";
 ComPtr<ID3DBlob> vs,ps,errs;
 auto ts=std::chrono::steady_clock::now();
 CK(D3DCompile(src,strlen(src),"AIOSGraphics",nullptr,nullptr,"VS","vs_5_1",
    D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&vs,&errs));
 errs.Reset();
 CK(D3DCompile(src,strlen(src),"AIOSGraphics",nullptr,nullptr,"PS","ps_5_1",
    D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&ps,&errs));
 D3D12_ROOT_SIGNATURE_DESC sig={};
 sig.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
 ComPtr<ID3DBlob> serialized;
 CK(D3D12SerializeRootSignature(&sig,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errs));
 ComPtr<ID3D12RootSignature> root;
 CK(dev->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
 D3D12_GRAPHICS_PIPELINE_STATE_DESC psd={};
 psd.pRootSignature=root.Get();
 psd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};
 psd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
 psd.SampleMask=0xffffffff;
 psd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
 psd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
 psd.RasterizerState.DepthClipEnable=TRUE;
 psd.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
 psd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
 psd.NumRenderTargets=1;
 psd.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;
 psd.SampleDesc.Count=1;
 const UINT VARIANTS=32;
 std::vector<ComPtr<ID3D12PipelineState>> psos(VARIANTS);
 std::vector<double> compilationMilliseconds;
 for(UINT i=0;i<VARIANTS;i++){
  psd.RasterizerState.CullMode=(i%2)?D3D12_CULL_MODE_BACK:D3D12_CULL_MODE_NONE;
  psd.RasterizerState.FillMode=(i%4==0)?D3D12_FILL_MODE_WIREFRAME:D3D12_FILL_MODE_SOLID;
  psd.RasterizerState.FrontCounterClockwise=(i%8>=4)?TRUE:FALSE;
  psd.RasterizerState.DepthClipEnable=TRUE;
  psd.BlendState.RenderTarget[0].BlendEnable=(i%4==1)?TRUE:FALSE;
  psd.BlendState.RenderTarget[0].SrcBlend=D3D12_BLEND_ONE;
  psd.BlendState.RenderTarget[0].DestBlend=D3D12_BLEND_ZERO;
  psd.BlendState.RenderTarget[0].BlendOp=D3D12_BLEND_OP_ADD;
  psd.BlendState.RenderTarget[0].SrcBlendAlpha=D3D12_BLEND_ONE;
  psd.BlendState.RenderTarget[0].DestBlendAlpha=D3D12_BLEND_ZERO;
  psd.BlendState.RenderTarget[0].BlendOpAlpha=D3D12_BLEND_OP_ADD;
  psd.BlendState.AlphaToCoverageEnable=FALSE;
  auto pt=std::chrono::steady_clock::now();
  HRESULT hr=dev->CreateGraphicsPipelineState(&psd,IID_PPV_ARGS(&psos[i]));
  printf("PSO_VARIANT=%u HRESULT=0x%08lX\n",i,(unsigned long)hr);
  if(FAILED(hr)){printf("FIRST_FAILED_PSO=%u DEVICE_REASON=0x%08lX\n",i,(unsigned long)dev->GetDeviceRemovedReason());return 31;}
  compilationMilliseconds.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-pt).count());
 }
 printf("pso_variants_compiled=%u\n",VARIANTS);
 auto tc=std::chrono::steady_clock::now();
 printf("graphics_shader_pso_compile_ms=%.3f\n",std::chrono::duration<double,std::milli>(tc-ts).count());
 constexpr UINT W=256,H=256;
 D3D12_RESOURCE_DESC texture={};
 texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;texture.Width=W;texture.Height=H;
 texture.DepthOrArraySize=1;texture.MipLevels=1;texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 texture.SampleDesc.Count=1;texture.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 auto def=heapProp(D3D12_HEAP_TYPE_DEFAULT);
 ComPtr<ID3D12Resource> render;
 CK(dev->CreateCommittedResource(&def,D3D12_HEAP_FLAG_NONE,&texture,
  D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&render)));
 D3D12_DESCRIPTOR_HEAP_DESC hdesc={};
 hdesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hdesc.NumDescriptors=1;
 ComPtr<ID3D12DescriptorHeap> rtv;
 CK(dev->CreateDescriptorHeap(&hdesc,IID_PPV_ARGS(&rtv)));
 dev->CreateRenderTargetView(render.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
 D3D12_RESOURCE_DESC buffer={};
 buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=W*H*4;
 buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;
 buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 auto rbheap=heapProp(D3D12_HEAP_TYPE_READBACK);
 ComPtr<ID3D12Resource> readback;
 CK(dev->CreateCommittedResource(&rbheap,D3D12_HEAP_FLAG_NONE,&buffer,
  D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));

 // Allocate a bounded live set while graphics PSOs and render target remain live.
 // This is residency/heap pressure without intentional over-budget conditions.
 std::vector<ComPtr<ID3D12Heap>> transientHeaps;
 std::vector<ComPtr<ID3D12Resource>> transientTextures;
 uint64_t allocated=0;
 ComPtr<IDXGIAdapter3> adapter3;adapter.As(&adapter3);
 DXGI_QUERY_VIDEO_MEMORY_INFO pre={};
 if(adapter3)adapter3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&pre);
 printf("LOCAL_USAGE_BEFORE=%llu LOCAL_BUDGET=%llu\n",
   (unsigned long long)pre.CurrentUsage,(unsigned long long)pre.Budget);
 for(unsigned i=0;i<8;i++){
  D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  d.Width=(i%2)?512:800;d.Height=(i%2)?512:800;
  d.DepthOrArraySize=1;d.MipLevels=1;
  d.Format=(i%2)?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_R16_UNORM;
  d.SampleDesc.Count=1;
  auto info=dev->GetResourceAllocationInfo(0,1,&d);
  if(info.SizeInBytes==0 || info.SizeInBytes>4ull*1024*1024||
    allocated+info.SizeInBytes>16ull*1024*1024)return 41;
  D3D12_HEAP_DESC hd={};hd.SizeInBytes=info.SizeInBytes;
  hd.Properties=heapProp(D3D12_HEAP_TYPE_DEFAULT);
  hd.Flags=(i%2)?D3D12_HEAP_FLAG_NONE:(D3D12_HEAP_FLAGS)68;
  ComPtr<ID3D12Heap> hh;HRESULT hr=dev->CreateHeap(&hd,IID_PPV_ARGS(&hh));
  if(FAILED(hr)){printf("HEAP_FAIL_AT=%u HRESULT=0x%08lX\n",i,(unsigned long)hr);return 42;}
  ComPtr<ID3D12Resource> t;
  hr=dev->CreatePlacedResource(hh.Get(),0,&d,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&t));
  if(FAILED(hr)){printf("PLACED_FAIL_AT=%u HRESULT=0x%08lX\n",i,(unsigned long)hr);return 43;}
  allocated+=info.SizeInBytes;
  transientHeaps.push_back(hh);transientTextures.push_back(t);
 }
 printf("TEXTURES_LIVE=%zu ALLOCATED_BYTES=%llu\n",transientTextures.size(),(unsigned long long)allocated);
 DXGI_QUERY_VIDEO_MEMORY_INFO memAtPeak={};
 if(adapter3)adapter3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memAtPeak);
 printf("LOCAL_USAGE_WITH_TEXTURES=%llu\n",(unsigned long long)memAtPeak.CurrentUsage);
 D3D12_COMMAND_QUEUE_DESC q={};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> queue;
 CK(dev->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> alloc;
 CK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
 ComPtr<ID3D12GraphicsCommandList> list;
 CK(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list)));
 list->SetGraphicsRootSignature(root.Get());
 list->SetPipelineState(psos[0].Get());
 D3D12_VIEWPORT vp={0,0,(float)W,(float)H,0,1};
 D3D12_RECT sc={0,0,(LONG)W,(LONG)H};
 list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&sc);
 D3D12_CPU_DESCRIPTOR_HANDLE handle=rtv->GetCPUDescriptorHandleForHeapStart();
 list->OMSetRenderTargets(1,&handle,FALSE,nullptr);
 const float clear[4]={0.02f,0.03f,0.08f,1.0f};
 list->ClearRenderTargetView(handle,clear,0,nullptr);
 list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 for(UINT i=0;i<VARIANTS;i++){
  list->SetPipelineState(psos[i].Get());
  list->DrawInstanced(3,1,0,0);
 }
 printf("draw_variants_recorded=%u\n",VARIANTS);
 D3D12_RESOURCE_BARRIER barrier={};
 barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
 barrier.Transition.pResource=render.Get();
 barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
 barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;
 barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
 list->ResourceBarrier(1,&barrier);
 D3D12_TEXTURE_COPY_LOCATION srcLoc={};srcLoc.pResource=render.Get();
 srcLoc.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;srcLoc.SubresourceIndex=0;
 D3D12_TEXTURE_COPY_LOCATION dstLoc={};dstLoc.pResource=readback.Get();
 dstLoc.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
 dstLoc.PlacedFootprint.Footprint.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 dstLoc.PlacedFootprint.Footprint.Width=W;dstLoc.PlacedFootprint.Footprint.Height=H;
 dstLoc.PlacedFootprint.Footprint.Depth=1;dstLoc.PlacedFootprint.Footprint.RowPitch=W*4;
 list->CopyTextureRegion(&dstLoc,0,0,0,&srcLoc,nullptr);
 CK(list->Close());
 ID3D12CommandList* lists[]={list.Get()};
 auto start=std::chrono::steady_clock::now();
 queue->ExecuteCommandLists(1,lists);
 ComPtr<ID3D12Fence> fence;
 CK(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 CK(queue->Signal(fence.Get(),1));
 HANDLE ev=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 if(!ev)return 13;
 CK(fence->SetEventOnCompletion(1,ev));
 auto wait=WaitForSingleObject(ev,1500);CloseHandle(ev);
 printf("graphics_fence_wait=%s\n",wait==WAIT_OBJECT_0?"PASS":"FAIL");
 printf("device_removed_reason=0x%08lX\n",(unsigned long)dev->GetDeviceRemovedReason());
 if(wait!=WAIT_OBJECT_0)return 14;
 printf("graphics_submit_to_fence_ms=%.3f\n",
 std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
 void* data=nullptr;D3D12_RANGE range={0,W*H*4};
 CK(readback->Map(0,&range,&data));
 const auto bytes=(const unsigned char*)data;
 unsigned nonClear=0;unsigned center=0;unsigned samples=0;
 const unsigned clearR=5,clearG=8,clearB=20;
 for(unsigned y=8;y<H;y+=8){
  for(unsigned x=8;x<W;x+=8){
   const unsigned char* c=bytes+(y*W+x)*4;
   if(c[0]!=clearR || c[1]!=clearG || c[2]!=clearB)nonClear++;
   samples++;
  }
 }
 const unsigned char* mid=bytes+((H/2)*W+W/2)*4;
 center=(unsigned)mid[0]+(unsigned)mid[1]+(unsigned)mid[2];
 printf("graphics_sample_non_clear=%u/%u\n",nonClear,samples);
 printf("graphics_center_rgb=%u,%u,%u\n",mid[0],mid[1],mid[2]);
 printf("graphics_pixel_validation=%s\n",nonClear>100 && center>40?"PASS":"FAIL");
 FILE* frame=fopen("measurements\\D3D12_GRAPHICS_PSO_TEXTURE_MIX_V01.ppm","wb");
 if(frame){
  fprintf(frame,"P6\n%u %u\n255\n",W,H);
  for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++)fwrite(bytes+(y*W+x)*4,1,3,frame);
  fclose(frame);
  puts("GPUFramePPM=measurements/D3D12_GRAPHICS_PSO_TEXTURE_MIX_V01.ppm");
 }else puts("GPUFramePPM=FAILED_TO_OPEN");
 D3D12_RANGE empty={0,0};readback->Unmap(0,&empty);
 puts("END_D3D12_GRAPHICS_TEST");
 return nonClear>100 && center>40?0:15;
}

// AIOS UnderDog SHADER+STREAM bounded D3D12 test V01.
// 3 ALU levels, per-frame 64KiB buffer upload, GPU timestamp pairs, CPU exact validation.
// Standalone owned process. No game/anti-cheat/device driver injection or modifications.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <cstdint>
using Microsoft::WRL::ComPtr;
#define CHECK(EXPR) {HRESULT hr=(EXPR);if(FAILED(hr)){printf("FAILED_%s=0x%08lX\n",#EXPR,(unsigned long)hr);fflush(stdout);return 10;}}
constexpr UINT N=16384,BYTES=N*4,FRAMES_PER_TIER=12,TIERS=3,FRAMES=FRAMES_PER_TIER*TIERS;
const UINT iterationTiers[TIERS]={4,32,128};
static D3D12_HEAP_PROPERTIES prop(D3D12_HEAP_TYPE t){
 D3D12_HEAP_PROPERTIES p={};p.Type=t;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;
}
static D3D12_RESOURCE_DESC buf(uint64_t bytes,bool uav){
 D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;
 d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;
 d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 d.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;return d;
}
static uint32_t input(UINT i,UINT frame){
 return uint32_t((i*1664525u) ^ (frame*2246822519u) ^ 0x9e3779b9u);
}
static uint32_t cpu(UINT index,UINT frame,UINT rounds){
 uint32_t v=input(index,frame)^(index*747796405u);
 for(UINT j=0;j<rounds;j++){
  v=(v^(v>>16))*2246822519u;
  v=(v^(v>>13))*3266489917u;
  v^=v>>16;
 }
 return v;
}
static double per(const std::vector<double>& v,double fraction){
 if(v.empty())return -1;
 auto sorted=v;std::sort(sorted.begin(),sorted.end());
 return sorted[std::min(sorted.size()-1,(size_t)((sorted.size()-1)*fraction))];
}
int main(){
 puts("AIOS_UNDERDOG_D3D12_ALU_UPLOAD_STREAM_V01");fflush(stdout);
 ComPtr<IDXGIFactory4> factory;CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;
 for(UINT i=0;factory->EnumAdapters1(i,&adapter)==S_OK;i++){
  DXGI_ADAPTER_DESC1 d={};adapter->GetDesc1(&d);
  if(!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)){printf("GPU_VENDOR=0x%04X GPU_DEVICE=0x%04X\n",d.VendorId,d.DeviceId);break;}
  adapter.Reset();
 }
 if(!adapter)return 3;
 ComPtr<ID3D12Device> dev;CHECK(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)));
 const char* shader=R"(
StructuredBuffer<uint> inputData:register(t0);
RWStructuredBuffer<uint> outputData:register(u0);
cbuffer Job:register(b0){uint rounds;};
[numthreads(8,8,1)]
void main(uint3 id:SV_DispatchThreadID) {
 uint i=id.y*128+id.x;
 uint v=inputData[i]^(i*747796405u);
 [loop]for(uint j=0;j<rounds;j++){
  v=(v^(v>>16))*2246822519u;
  v=(v^(v>>13))*3266489917u;
  v^=v>>16;
 }
 outputData[i]=v;
}
)";
 ComPtr<ID3DBlob> cs,err;
 HRESULT compiled=D3DCompile(shader,strlen(shader),"AIOSAluStreaming",nullptr,nullptr,"main","cs_5_1",
  D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&cs,&err);
 if(FAILED(compiled)){
  printf("SHADER_COMPILE_FAILED=0x%08lX\n",(unsigned long)compiled);
  if(err)printf("HLSL=%.*s\n",(int)err->GetBufferSize(),(char*)err->GetBufferPointer());
  return 4;
 }
 D3D12_ROOT_PARAMETER params[3]={};
 params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
 params[0].Descriptor.ShaderRegister=0;
 params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
 params[1].Descriptor.ShaderRegister=0;
 params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
 params[2].Constants.ShaderRegister=0;
 params[2].Constants.Num32BitValues=1;
 for(auto& p:params)p.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
 D3D12_ROOT_SIGNATURE_DESC rs={};rs.NumParameters=3;rs.pParameters=params;
 ComPtr<ID3DBlob> ser;
 CHECK(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&ser,&err));
 ComPtr<ID3D12RootSignature> root;
 CHECK(dev->CreateRootSignature(0,ser->GetBufferPointer(),ser->GetBufferSize(),IID_PPV_ARGS(&root)));
 D3D12_COMPUTE_PIPELINE_STATE_DESC pdesc={};
 pdesc.pRootSignature=root.Get();
 pdesc.CS={cs->GetBufferPointer(),cs->GetBufferSize()};
 ComPtr<ID3D12PipelineState> pso;CHECK(dev->CreateComputePipelineState(&pdesc,IID_PPV_ARGS(&pso)));
 auto defaultProp=prop(D3D12_HEAP_TYPE_DEFAULT),uploadProp=prop(D3D12_HEAP_TYPE_UPLOAD),readProp=prop(D3D12_HEAP_TYPE_READBACK);
 ComPtr<ID3D12Resource> source,output,upload,readback,timestampBuffer;
 auto standard=buf(BYTES,false),unordered=buf(BYTES,true);
 CHECK(dev->CreateCommittedResource(&defaultProp,D3D12_HEAP_FLAG_NONE,&standard,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&source)));
 CHECK(dev->CreateCommittedResource(&defaultProp,D3D12_HEAP_FLAG_NONE,&unordered,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output)));
 CHECK(dev->CreateCommittedResource(&uploadProp,D3D12_HEAP_FLAG_NONE,&standard,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)));
 CHECK(dev->CreateCommittedResource(&readProp,D3D12_HEAP_FLAG_NONE,&standard,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
 auto stampDesc=buf(FRAMES*3*8,false);
 CHECK(dev->CreateCommittedResource(&readProp,D3D12_HEAP_FLAG_NONE,&stampDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&timestampBuffer)));
 D3D12_COMMAND_QUEUE_DESC queueDesc={};queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> queue;CHECK(dev->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)));
 UINT64 frequency=0;CHECK(queue->GetTimestampFrequency(&frequency));
 printf("GPU_TIMESTAMP_FREQUENCY_HZ=%llu\n",(unsigned long long)frequency);
 D3D12_QUERY_HEAP_DESC qhd={};qhd.Count=FRAMES*3;qhd.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
 ComPtr<ID3D12QueryHeap> queries;CHECK(dev->CreateQueryHeap(&qhd,IID_PPV_ARGS(&queries)));
 ComPtr<ID3D12CommandAllocator> allocator;CHECK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
 ComPtr<ID3D12GraphicsCommandList> list;
 CHECK(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
 ComPtr<ID3D12Fence> fence;CHECK(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 HANDLE ev=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ev)return 5;
 ComPtr<IDXGIAdapter3> adapter3;adapter.As(&adapter3);
 uint64_t peakUsage=0,budget=0;
 uint32_t priorFrameMarker=0;unsigned samplesChecked=0;
 D3D12_RANGE empty={0,0};
 void* mappedInput=nullptr;CHECK(upload->Map(0,&empty,&mappedInput));
 auto wallStart=std::chrono::steady_clock::now();
 for(UINT frame=0;frame<FRAMES;frame++){
  UINT tier=frame/FRAMES_PER_TIER,rounds=iterationTiers[tier];
  if(frame){
   CHECK(allocator->Reset());CHECK(list->Reset(allocator.Get(),nullptr));
  }
  uint32_t* words=(uint32_t*)mappedInput;
  for(UINT i=0;i<N;i++)words[i]=input(i,frame);
  list->SetComputeRootSignature(root.Get());list->SetPipelineState(pso.Get());
  D3D12_RESOURCE_BARRIER b={};
  b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  if(frame){
   b.Transition.pResource=source.Get();
   b.Transition.StateBefore=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
   list->ResourceBarrier(1,&b);
   b.Transition.pResource=output.Get();
   b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
   b.Transition.StateAfter=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   list->ResourceBarrier(1,&b);
  }
  list->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frame*3);
  list->CopyBufferRegion(source.Get(),0,upload.Get(),0,BYTES);
  b.Transition.pResource=source.Get();
  b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;
  b.Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  list->ResourceBarrier(1,&b);
  list->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frame*3+1);
  list->SetComputeRootUnorderedAccessView(0,output->GetGPUVirtualAddress());
  list->SetComputeRootShaderResourceView(1,source->GetGPUVirtualAddress());
  list->SetComputeRoot32BitConstant(2,rounds,0);
  list->Dispatch(16,16,1);
  D3D12_RESOURCE_BARRIER u={};u.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;u.UAV.pResource=output.Get();
  list->ResourceBarrier(1,&u);
  list->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frame*3+2);
  b.Transition.pResource=output.Get();
  b.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
  list->ResourceBarrier(1,&b);
  list->CopyResource(readback.Get(),output.Get());
  list->ResolveQueryData(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frame*3,3,timestampBuffer.Get(),frame*24);
  CHECK(list->Close());
  ID3D12CommandList* commands[]={list.Get()};
  queue->ExecuteCommandLists(1,commands);
  CHECK(queue->Signal(fence.Get(),frame+1));
  CHECK(fence->SetEventOnCompletion(frame+1,ev));
  DWORD wait=WaitForSingleObject(ev,1500);
  if(wait!=WAIT_OBJECT_0){
   printf("FENCE_TIMEOUT_FRAME=%u ROUNDS=%u\n",frame,rounds);
   printf("DEVICE_REASON=0x%08lX\n",(unsigned long)dev->GetDeviceRemovedReason());
   CloseHandle(ev);return 6;
  }
  D3D12_RANGE readRange={0,BYTES};void* got=nullptr;CHECK(readback->Map(0,&readRange,&got));
  const auto* gpu=(const uint32_t*)got;
  constexpr UINT indices[]={0,1,127,128,4096,8191,12345,16383};
  unsigned mismatches=0;
  for(auto i:indices)if(gpu[i]!=cpu(i,frame,rounds))mismatches++;
  samplesChecked+=sizeof(indices)/sizeof(indices[0]);
  const uint32_t center=gpu[8191];
  readback->Unmap(0,&empty);
  if(mismatches){
   printf("CPU_GPU_MISMATCH_FRAME=%u COUNT=%u\n",frame,mismatches);return 7;
  }
  if(frame>0 && center==priorFrameMarker){
   printf("WARNING_SAME_MARKER_FRAME=%u\n",frame);
  }
  priorFrameMarker=center;
  if(adapter3){
   DXGI_QUERY_VIDEO_MEMORY_INFO vm={};
   if(SUCCEEDED(adapter3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&vm))){
    budget=vm.Budget;peakUsage=std::max<uint64_t>(peakUsage,vm.CurrentUsage);
   }
  }
 }
 upload->Unmap(0,nullptr);CloseHandle(ev);
 D3D12_RANGE stamps={0,FRAMES*3*8};void* stampPtr=nullptr;CHECK(timestampBuffer->Map(0,&stamps,&stampPtr));
 const auto* gpuStamps=(const uint64_t*)stampPtr;
 std::vector<double> uploadMs[TIERS],aluMs[TIERS];
 for(UINT frame=0;frame<FRAMES;frame++){
  const auto begin=gpuStamps[frame*3],mid=gpuStamps[frame*3+1],end=gpuStamps[frame*3+2];
  if(mid<begin||end<mid){printf("BAD_TIMESTAMP_FRAME=%u\n",frame);return 8;}
  uploadMs[frame/FRAMES_PER_TIER].push_back(1000.0*double(mid-begin)/double(frequency));
  aluMs[frame/FRAMES_PER_TIER].push_back(1000.0*double(end-mid)/double(frequency));
 }
 timestampBuffer->Unmap(0,&empty);
 for(UINT i=0;i<TIERS;i++){
  auto& up=uploadMs[i];auto& al=aluMs[i];
  const double aAvg=std::accumulate(al.begin(),al.end(),0.0)/al.size();
  printf("TIER=%u FRAMES=%u GPU_UPLOAD_P50_MS=%.4f GPU_UPLOAD_P95_MS=%.4f GPU_ALU_MEAN_MS=%.4f GPU_ALU_P50_MS=%.4f GPU_ALU_P95_MS=%.4f GPU_ALU_MAX_MS=%.4f\n",
    iterationTiers[i],FRAMES_PER_TIER,per(up,.5),per(up,.95),aAvg,per(al,.5),per(al,.95),per(al,1));
 }
 printf("FRAMES_COMPLETED=%u SAMPLES_CPU_VERIFIED=%u BYTES_UPLOADED=%u\n",FRAMES,samplesChecked,FRAMES*BYTES);
 printf("PEAK_DXGI_USAGE_BYTES=%llu DXGI_BUDGET_BYTES=%llu\n",(unsigned long long)peakUsage,(unsigned long long)budget);
 printf("WALL_SECONDS=%.3f\n",std::chrono::duration<double>(std::chrono::steady_clock::now()-wallStart).count());
 printf("DEVICE_REMOVED_REASON=0x%08lX\n",(unsigned long)dev->GetDeviceRemovedReason());
 puts("END_ALU_STREAM_TEST");
 return 0;
}

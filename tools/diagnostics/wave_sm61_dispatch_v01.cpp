// SPDX-License-Identifier: MIT
// Native Intel hardware D3D12 SM6.1/WaveOps 32-lane bounded dispatch + CPU readback.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <fstream>
#include <vector>
#include <cstdint>
using Microsoft::WRL::ComPtr;
#define CHECK(x) do {HRESULT _h=(x);if(FAILED(_h)){printf("FAIL:%s HR=0x%08X\n",#x,(unsigned)_h);return 10;}}while(0)
static D3D12_HEAP_PROPERTIES props(D3D12_HEAP_TYPE t){D3D12_HEAP_PROPERTIES p={};p.Type=t;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;}
static D3D12_RESOURCE_DESC buf(UINT64 size,bool uav){D3D12_RESOURCE_DESC r={};r.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;r.Width=size;r.Height=1;r.DepthOrArraySize=1;r.MipLevels=1;r.SampleDesc.Count=1;r.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;r.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;return r;}
int main(int argc,char**argv){
 if(argc!=2){puts("USAGE: wave_sm61_dispatch.exe shader.cso");return 2;}
 std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
 if(!f){puts("CSO_MISSING");return 3;}
 auto size=f.tellg();if(size<=0||size>1024*1024){puts("INVALID_CSO_SIZE");return 4;}
 std::vector<char> shader((size_t)size);f.seekg(0);f.read(shader.data(),size);
 if(!f){puts("CSO_READ_FAILED");return 5;}
 ComPtr<IDXGIFactory4> factory;CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;DXGI_ADAPTER_DESC1 desc={};bool found=false;
 for(UINT i=0;factory->EnumAdapters1(i,&adapter)==S_OK;i++){
  adapter->GetDesc1(&desc);if(desc.VendorId==0x8086 && desc.DeviceId==0x1916 && !(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){found=true;break;}adapter.Reset();
 }
 if(!found){puts("EXPECTED_INTEL_HD520_MISSING_WARP_FORBIDDEN");return 6;}
 ComPtr<ID3D12Device> device;CHECK(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));
 D3D12_FEATURE_DATA_SHADER_MODEL model={D3D_SHADER_MODEL_6_1};
 CHECK(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&model,sizeof(model)));
 if(model.HighestShaderModel<D3D_SHADER_MODEL_6_1){puts("SHADER_MODEL_6_1_NOT_SUPPORTED");return 7;}
 D3D12_FEATURE_DATA_D3D12_OPTIONS1 options={};
 CHECK(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1,&options,sizeof(options)));
 if(!options.WaveOps){puts("WAVEOPS_NOT_SUPPORTED");return 8;}
 D3D12_ROOT_PARAMETER rootParam={};rootParam.ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;rootParam.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;rootParam.Descriptor.ShaderRegister=0;
 D3D12_ROOT_SIGNATURE_DESC rootDesc={};rootDesc.NumParameters=1;rootDesc.pParameters=&rootParam;
 ComPtr<ID3DBlob> rootsigBlob,errors;CHECK(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&rootsigBlob,&errors));
 ComPtr<ID3D12RootSignature> rootSig;CHECK(device->CreateRootSignature(0,rootsigBlob->GetBufferPointer(),rootsigBlob->GetBufferSize(),IID_PPV_ARGS(&rootSig)));
 D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline={};pipeline.pRootSignature=rootSig.Get();pipeline.CS.pShaderBytecode=shader.data();pipeline.CS.BytecodeLength=shader.size();
 ComPtr<ID3D12PipelineState> pso;HRESULT psohr=device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&pso));
 printf("SM61_CREATE_COMPUTE_PSO_HR=0x%08X\n",(unsigned)psohr);if(FAILED(psohr))return 11;
 constexpr UINT N=32;constexpr UINT BYTES=N*8;
 ComPtr<ID3D12Resource> gpu,readback;
 auto defaultHeap=props(D3D12_HEAP_TYPE_DEFAULT);auto readbackHeap=props(D3D12_HEAP_TYPE_READBACK);auto gpuDesc=buf(BYTES,true);auto readDesc=buf(BYTES,false);
 CHECK(device->CreateCommittedResource(&defaultHeap,D3D12_HEAP_FLAG_NONE,&gpuDesc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&gpu)));
 CHECK(device->CreateCommittedResource(&readbackHeap,D3D12_HEAP_FLAG_NONE,&readDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
 D3D12_COMMAND_QUEUE_DESC queueDesc={};queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> queue;CHECK(device->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> alloc;CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc)));
 ComPtr<ID3D12GraphicsCommandList> cl;CHECK(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),pso.Get(),IID_PPV_ARGS(&cl)));
 cl->SetComputeRootSignature(rootSig.Get());
 cl->SetComputeRootUnorderedAccessView(0,gpu->GetGPUVirtualAddress());
 cl->Dispatch(1,1,1);
 D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
 barrier.Transition.pResource=gpu.Get();barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
 cl->ResourceBarrier(1,&barrier);cl->CopyBufferRegion(readback.Get(),0,gpu.Get(),0,BYTES);
 CHECK(cl->Close());
 ID3D12CommandList* lists[]={cl.Get()};queue->ExecuteCommandLists(1,lists);
 ComPtr<ID3D12Fence> fence;CHECK(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 CHECK(queue->Signal(fence.Get(),1));
 HANDLE event=CreateEventA(nullptr,FALSE,FALSE,nullptr);if(!event){puts("EVENT_FAILED");return 12;}
 if(fence->GetCompletedValue()<1){CHECK(fence->SetEventOnCompletion(1,event));}
 DWORD wait=WaitForSingleObject(event,5000);CloseHandle(event);
 if(wait!=WAIT_OBJECT_0){puts("GPU_TIMEOUT_OR_FAILURE");return 13;}
 D3D12_RANGE readRange={0,BYTES};uint32_t* ptr=nullptr;CHECK(readback->Map(0,&readRange,(void**)&ptr));
 unsigned passed=0;unsigned waveMin=~0u,waveMax=0;
 for(unsigned i=0;i<N;i++){unsigned lanes=ptr[2*i],sum=ptr[2*i+1];if(lanes>=4&&lanes<=32&&sum==lanes)passed++;if(lanes<waveMin)waveMin=lanes;if(lanes>waveMax)waveMax=lanes;}
 D3D12_RANGE writeRange={0,0};readback->Unmap(0,&writeRange);
 auto removed=device->GetDeviceRemovedReason();
 printf("GPU_VENDOR=0x%04X GPU_DEVICE=0x%04X\n",desc.VendorId,desc.DeviceId);
 printf("SM61_WAVE_RESULTS=%u/%u MIN_LANES=%u MAX_LANES=%u\n",passed,N,waveMin,waveMax);
 printf("DEVICE_REMOVED_REASON=0x%08X\n",(unsigned)removed);
 if(passed!=N||FAILED(removed))return 14;
 puts("SM61_WAVE_EXECUTION_PASS");return 0;
}

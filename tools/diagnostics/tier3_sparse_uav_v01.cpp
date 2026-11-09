// SPDX-License-Identifier: MIT
// Tier-3-specific 65-UAV table with one sparse, live descriptor at slot 64.
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
#define CK(x) do {HRESULT h=(x);if(FAILED(h)){printf("FAIL_%s=0x%08X\n",#x,(unsigned)h);return 10;}}while(0)
static D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE t){D3D12_HEAP_PROPERTIES p={};p.Type=t;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;}
static D3D12_RESOURCE_DESC buffer(UINT64 size,bool uav){D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;return d;}
int main(int argc,char**argv){
 if(argc!=2){puts("USAGE: tier3_sparse_uav.exe shader.cso");return 2;}
 std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
 if(!f||f.tellg()<=0||f.tellg()>1024*1024){puts("INVALID_CSO");return 3;}
 std::vector<char> code((size_t)f.tellg());f.seekg(0);f.read(code.data(),code.size());if(!f)return 4;
 ComPtr<IDXGIFactory4> factory;CK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> gpu;DXGI_ADAPTER_DESC1 info={};bool found=false;
 for(UINT i=0;factory->EnumAdapters1(i,&gpu)==S_OK;i++){gpu->GetDesc1(&info);if(info.VendorId==0x8086 && info.DeviceId==0x1916 && !(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){found=true;break;}gpu.Reset();}
 if(!found){puts("NO_INTEL_HD520_HARDWARE");return 5;}
 ComPtr<ID3D12Device> dev;CK(D3D12CreateDevice(gpu.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)));
 D3D12_FEATURE_DATA_D3D12_OPTIONS opts={};CK(dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&opts,sizeof(opts)));
 printf("RESOURCE_BINDING_TIER=%u\n",(unsigned)opts.ResourceBindingTier);
 if(opts.ResourceBindingTier<D3D12_RESOURCE_BINDING_TIER_3){puts("TIER3_ABSENT_SKIP");return 6;}
 D3D12_DESCRIPTOR_RANGE range={};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;range.NumDescriptors=65;range.BaseShaderRegister=0;range.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
 D3D12_ROOT_PARAMETER params[2]={};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable.NumDescriptorRanges=1;params[0].DescriptorTable.pDescriptorRanges=&range;
 params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[1].Constants.ShaderRegister=0;params[1].Constants.Num32BitValues=2;
 D3D12_ROOT_SIGNATURE_DESC rootDesc={};rootDesc.NumParameters=2;rootDesc.pParameters=params;
 ComPtr<ID3DBlob> blob,errs;CK(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errs));
 ComPtr<ID3D12RootSignature> root;CK(dev->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
 D3D12_COMPUTE_PIPELINE_STATE_DESC pipe={};pipe.pRootSignature=root.Get();pipe.CS.pShaderBytecode=code.data();pipe.CS.BytecodeLength=code.size();
 ComPtr<ID3D12PipelineState> pso;HRESULT phr=dev->CreateComputePipelineState(&pipe,IID_PPV_ARGS(&pso));
 printf("TIER3_PSO_HR=0x%08X\n",(unsigned)phr);if(FAILED(phr))return 11;
 D3D12_DESCRIPTOR_HEAP_DESC hd={};hd.NumDescriptors=65;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
 ComPtr<ID3D12DescriptorHeap> heap;CK(dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)));
 const auto def=hp(D3D12_HEAP_TYPE_DEFAULT),read=hp(D3D12_HEAP_TYPE_READBACK);const auto bd=buffer(256,true),rd=buffer(256,false);
 ComPtr<ID3D12Resource> output,readback;
 CK(dev->CreateCommittedResource(&def,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output)));
 CK(dev->CreateCommittedResource(&read,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
 D3D12_UNORDERED_ACCESS_VIEW_DESC uav={};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Format=DXGI_FORMAT_UNKNOWN;uav.Buffer.NumElements=64;uav.Buffer.StructureByteStride=sizeof(uint32_t);
 auto cpu=heap->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=64ull*dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
 dev->CreateUnorderedAccessView(output.Get(),nullptr,&uav,cpu);
 D3D12_COMMAND_QUEUE_DESC qd={};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> queue;CK(dev->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> allocator;CK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
 ComPtr<ID3D12GraphicsCommandList> list;CK(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),pso.Get(),IID_PPV_ARGS(&list)));
 list->SetComputeRootSignature(root.Get());ID3D12DescriptorHeap* heaps[]={heap.Get()};list->SetDescriptorHeaps(1,heaps);
 list->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());
 constexpr uint32_t marker=0x13BEEF42;uint32_t constants[]={64,marker};list->SetComputeRoot32BitConstants(1,2,constants,0);
 list->Dispatch(1,1,1);
 D3D12_RESOURCE_BARRIER barrier={};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=output.Get();barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;list->ResourceBarrier(1,&barrier);
 list->CopyBufferRegion(readback.Get(),0,output.Get(),0,256);
 CK(list->Close());ID3D12CommandList* lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);
 ComPtr<ID3D12Fence> fence;CK(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));CK(queue->Signal(fence.Get(),1));
 HANDLE ev=CreateEventA(nullptr,FALSE,FALSE,nullptr);if(!ev)return 12; if(fence->GetCompletedValue()<1)CK(fence->SetEventOnCompletion(1,ev));
 DWORD wait=WaitForSingleObject(ev,5000);CloseHandle(ev);if(wait!=WAIT_OBJECT_0){puts("TIER3_WAIT_TIMEOUT");return 13;}
 D3D12_RANGE rangeRead={0,4};uint32_t* data=nullptr;CK(readback->Map(0,&rangeRead,(void**)&data));uint32_t got=data[0];D3D12_RANGE zero={0,0};readback->Unmap(0,&zero);
 printf("TIER3_LIVE_INDEX=64 OF 65 POPULATED_DESCRIPTOR_COUNT=1\n");
 printf("TIER3_READBACK=0x%08X EXPECT=0x%08X\n",got,marker);
 auto removed=dev->GetDeviceRemovedReason();printf("DEVICE_REMOVED_REASON=0x%08X\n",(unsigned)removed);
 if(got!=marker||FAILED(removed))return 14;
 puts("TIER3_SPARSE_UAV_EXECUTION_PASS");return 0;
}

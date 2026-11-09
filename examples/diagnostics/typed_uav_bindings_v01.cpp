// Native HD 520 D3D12 UAV binding matrix, owned fixture, 1 pixel per dispatch.
// No external game, driver modification, injection, or deliberate GPU stress.
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include "underdog/UavBindingPolicy.h"
using Microsoft::WRL::ComPtr;
static bool g_rgba8=false;
static DXGI_FORMAT outputFormat(){return g_rgba8?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_R32G32B32A32_FLOAT;}
static void loghr(const char* stage,HRESULT h){printf("%s=0x%08lX\n",stage,(unsigned long)h);}
static bool ok(const char* stage,HRESULT h){loghr(stage,h);return SUCCEEDED(h);}
static D3D12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE type){
 D3D12_HEAP_PROPERTIES p={};p.Type=type;p.CreationNodeMask=p.VisibleNodeMask=1;return p;
}
static D3D12_RESOURCE_DESC buffer(UINT64 size,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){
 D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
 d.Width=size;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;
 d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;return d;
}
static D3D12_RESOURCE_DESC tex(){
 D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
 d.Width=1;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;
 d.Format=outputFormat();d.SampleDesc.Count=1;
 d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;return d;
}
static bool test(ID3D12Device* dev,bool typed,bool table){
 const char* label=typed?(table?"TYPED_TABLE":"TYPED_ROOT"):(table?"STRUCTURED_TABLE":"STRUCTURED_ROOT");
 printf("CASE_BEGIN=%s\n",label);
 auto binding=underdog::chooseUavBinding(
    typed?underdog::UavResourceKind::TypedTexture2D:underdog::UavResourceKind::StructuredBuffer,
    !table);
 printf("POLICY_BINDING=%s POLICY_FALLBACK=%u\n",
    binding.method==underdog::UavBindingMethod::RootDescriptor?"ROOT":
    binding.method==underdog::UavBindingMethod::DescriptorTable?"TABLE":"UNSUPPORTED",
    (unsigned int)binding.fallbackApplied);
 if(typed&&!table)puts("CONTROL_INVALID_TYPED_ROOT_BYPASS_POLICY=1");
 const char* shader=typed?
 "RWTexture2D<float4> outp:register(u0);[numthreads(1,1,1)] "
 "void main(uint3 id:SV_DispatchThreadID){outp[id.xy]=float4(0.25,0.5,0.75,1.0);}" :
 "RWStructuredBuffer<float4> outp:register(u0);[numthreads(1,1,1)] "
 "void main(uint3 id:SV_DispatchThreadID){outp[id.x]=float4(0.25,0.5,0.75,1.0);}";
 ComPtr<ID3DBlob> cs,errors;
 HRESULT hr=D3DCompile(shader,strlen(shader),label,nullptr,nullptr,"main","cs_5_1",
 D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&cs,&errors);
 loghr("HLSL_COMPILE",hr);
 if(errors)printf("HLSL_MESSAGE=%.*s\n",(int)errors->GetBufferSize(),(const char*)errors->GetBufferPointer());
 if(FAILED(hr)){printf("CASE_END=%s COMPILE_FAIL\n",label);return false;}
 D3D12_DESCRIPTOR_RANGE range={};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
 range.NumDescriptors=1;range.BaseShaderRegister=0;
 range.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
 D3D12_ROOT_PARAMETER param={};param.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
 if(table){param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable.NumDescriptorRanges=1;param.DescriptorTable.pDescriptorRanges=&range;
 }else{param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;param.Descriptor.ShaderRegister=0;}
 D3D12_ROOT_SIGNATURE_DESC rd={};rd.NumParameters=1;rd.pParameters=&param;
 ComPtr<ID3DBlob> sig;errors.Reset();
 hr=D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&errors);
 loghr("ROOT_SERIALIZE",hr);
 if(errors)printf("ROOT_MESSAGE=%.*s\n",(int)errors->GetBufferSize(),(const char*)errors->GetBufferPointer());
 if(FAILED(hr)){printf("CASE_END=%s SERIALIZE_FAIL\n",label);return false;}
 ComPtr<ID3D12RootSignature> root;
 if(!ok("ROOT_CREATE",dev->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&root))))
   return false;
 D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc={};psoDesc.pRootSignature=root.Get();
 psoDesc.CS={cs->GetBufferPointer(),cs->GetBufferSize()};
 ComPtr<ID3D12PipelineState> pso;
 hr=dev->CreateComputePipelineState(&psoDesc,IID_PPV_ARGS(&pso));
 loghr("COMPUTE_PSO",hr);
 if(FAILED(hr)){printf("CASE_END=%s PSO_REJECTED\n",label);return false;}
 // Root UAV + typed texture is never submitted to GPU, even if PSO erroneously succeeds.
 if(typed&&!table){puts("CASE_END=TYPED_ROOT INVALID_ROOT_TYPED_TEXTURE_SKIP_GPU");return true;}
 D3D12_COMMAND_QUEUE_DESC qd={};qd.Type=D3D12_COMMAND_LIST_TYPE_COMPUTE;
 ComPtr<ID3D12CommandQueue> queue;
 if(!ok("QUEUE",dev->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue))))return false;
 ComPtr<ID3D12CommandAllocator> allocator;
 if(!ok("ALLOCATOR",dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,IID_PPV_ARGS(&allocator))))return false;
 ComPtr<ID3D12GraphicsCommandList> list;
 if(!ok("LIST",dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COMPUTE,allocator.Get(),pso.Get(),IID_PPV_ARGS(&list))))return false;
 auto dHeap=hp(D3D12_HEAP_TYPE_DEFAULT);
 D3D12_RESOURCE_DESC desc=typed?tex():buffer(16,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 ComPtr<ID3D12Resource> target;
 if(!ok("RESOURCE",dev->CreateCommittedResource(&dHeap,D3D12_HEAP_FLAG_NONE,&desc,
 D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&target))))return false;
 ComPtr<ID3D12DescriptorHeap> descriptors;
 if(table){
  D3D12_DESCRIPTOR_HEAP_DESC hd={};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if(!ok("DESCRIPTOR_HEAP",dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&descriptors))))return false;
  D3D12_UNORDERED_ACCESS_VIEW_DESC u={};
  if(typed){u.Format=outputFormat();u.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;}
  else {u.Format=DXGI_FORMAT_UNKNOWN;u.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
   u.Buffer.NumElements=1;u.Buffer.StructureByteStride=16;}
  dev->CreateUnorderedAccessView(target.Get(),nullptr,&u,descriptors->GetCPUDescriptorHandleForHeapStart());
 }
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint={};
 UINT64 bytes=16;
 if(typed){UINT rows=0;UINT64 rowsize=0;dev->GetCopyableFootprints(&desc,0,1,0,&footprint,&rows,&rowsize,&bytes);
 if(bytes<256)bytes=256; // Ensure mapped readback range is within committed buffer.
 }
 auto rbHeap=hp(D3D12_HEAP_TYPE_READBACK);auto rbDesc=buffer(bytes);
 ComPtr<ID3D12Resource> readback;
 if(!ok("READBACK",dev->CreateCommittedResource(&rbHeap,D3D12_HEAP_FLAG_NONE,&rbDesc,
 D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))))return false;
 list->SetComputeRootSignature(root.Get());list->SetPipelineState(pso.Get());
 if(table){ID3D12DescriptorHeap* heaps[]={descriptors.Get()};list->SetDescriptorHeaps(1,heaps);
  list->SetComputeRootDescriptorTable(0,descriptors->GetGPUDescriptorHandleForHeapStart());
 }else list->SetComputeRootUnorderedAccessView(0,target->GetGPUVirtualAddress());
 list->Dispatch(1,1,1);
 D3D12_RESOURCE_BARRIER ubar={};ubar.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;
 ubar.UAV.pResource=target.Get();list->ResourceBarrier(1,&ubar);
 D3D12_RESOURCE_BARRIER trans={};trans.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
 trans.Transition.pResource=target.Get();trans.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
 trans.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
 trans.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
 list->ResourceBarrier(1,&trans);
 if(typed){
  D3D12_TEXTURE_COPY_LOCATION out={};out.pResource=readback.Get();
  out.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;out.PlacedFootprint=footprint;
  D3D12_TEXTURE_COPY_LOCATION in={};in.pResource=target.Get();
  in.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  list->CopyTextureRegion(&out,0,0,0,&in,nullptr);
 }else list->CopyBufferRegion(readback.Get(),0,target.Get(),0,16);
 if(!ok("LIST_CLOSE",list->Close()))return false;
 ID3D12CommandList* lists[]={list.Get()};queue->ExecuteCommandLists(1,lists);
 ComPtr<ID3D12Fence> fence;
 if(!ok("FENCE",dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))return false;
 if(!ok("SIGNAL",queue->Signal(fence.Get(),1)))return false;
 HANDLE evt=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!evt)return false;
 hr=fence->SetEventOnCompletion(1,evt);
 DWORD wait=SUCCEEDED(hr)?WaitForSingleObject(evt,3000):WAIT_FAILED;
 CloseHandle(evt);printf("GPU_WAIT=%lu\n",(unsigned long)wait);
 if(wait!=WAIT_OBJECT_0)return false;
 void* mapped=nullptr;D3D12_RANGE accessed={0,16};
 hr=readback->Map(0,&accessed,&mapped);loghr("MAP",hr);if(FAILED(hr))return false;
 float sample[4]={};
 if(typed&&g_rgba8){const auto* bytes=(const unsigned char*)mapped;
  for(int i=0;i<4;i++)sample[i]=bytes[i]/255.0f;
  printf("RGBA8_BYTES=%u,%u,%u,%u\n",bytes[0],bytes[1],bytes[2],bytes[3]);
 }else memcpy(sample,mapped,sizeof(sample));
 D3D12_RANGE wrote={0,0};readback->Unmap(0,&wrote);
 bool pixel=fabsf(sample[0]-0.25f)<0.006f&&fabsf(sample[1]-0.50f)<0.006f
 &&fabsf(sample[2]-0.75f)<0.006f&&fabsf(sample[3]-1.00f)<0.006f;
 printf("FLOAT4=%.4f,%.4f,%.4f,%.4f\n",sample[0],sample[1],sample[2],sample[3]);
 printf("PIXEL_VALIDATION=%s\n",pixel?"PASS":"FAIL");
 printf("DEVICE_REMOVED_REASON=0x%08lX\n",(unsigned long)dev->GetDeviceRemovedReason());
 printf("CASE_END=%s %s\n",label,pixel?"PASS":"PIXEL_FAIL");
 return pixel;
}
int main(int argc,char** argv){
 g_rgba8=argc>1&&strcmp(argv[1],"--rgba8")==0;
 puts("AIOS_UNDERDOG_ROOT_VS_TABLE_TYPED_UAV_V01");
 printf("FORMAT_MODE=%s\n",g_rgba8?"ORIGINAL_RGBA8_UNORM":"R32G32B32A32_FLOAT");
 ComPtr<IDXGIFactory4> factory;
 if(!ok("FACTORY",CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return 1;
 ComPtr<IDXGIAdapter1> gpu;
 for(UINT i=0;;i++){ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapters1(i,&a)!=S_OK)break;
 DXGI_ADAPTER_DESC1 d={};a->GetDesc1(&d);
 if(d.VendorId==0x8086&&d.DeviceId==0x1916){gpu=a;break;}}
 if(!gpu){puts("HD520_NOT_PRESENT");return 2;}
 ComPtr<ID3D12Device> dev;
 if(!ok("DEVICE",D3D12CreateDevice(gpu.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev))))return 3;
 ComPtr<ID3D12Debug> debug;
 loghr("DEBUG_LAYER_QUERY_ONLY",D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
 D3D12_FEATURE_DATA_FORMAT_SUPPORT fs={};fs.Format=outputFormat();
 HRESULT hr=dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&fs,sizeof(fs));
 loghr("FORMAT_QUERY",hr);
 if(FAILED(hr))return 4;
 printf("OUTPUT_FORMAT_SUPPORT1=0x%08X\n",(unsigned int)fs.Support1);
 printf("OUTPUT_FORMAT_SUPPORT2=0x%08X\n",(unsigned int)fs.Support2);
 printf("TYPED_STORE_SUPPORT=%d\n",(int)!!(fs.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE));
 bool a=test(dev.Get(),false,false),b=test(dev.Get(),false,true);
 bool c=test(dev.Get(),true,false),d=test(dev.Get(),true,true);
 printf("FINAL_STRUCT_ROOT=%d FINAL_STRUCT_TABLE=%d FINAL_TYPED_ROOT_PSO=%d FINAL_TYPED_TABLE=%d\n",(int)a,(int)b,(int)c,(int)d);
 printf("FINAL_DEVICE_REMOVED_REASON=0x%08lX\n",(unsigned long)dev->GetDeviceRemovedReason());
 bool pass=a&&b&&!c&&d;
 printf("MATRIX_PASS=%d\n",(int)pass);
 return pass?0:5;
}

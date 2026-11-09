// AIOS Underdog V02: BC1/BC3/BC7 mip shader sampling witness, Intel HD 520.
// Owned executable only; explicit compressed-block patterns, readback CPU reference.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <array>
using Microsoft::WRL::ComPtr;
#define CK(X) {HRESULT h=(X);if(FAILED(h)){printf(#X "_FAILED=0x%08lX\n",(unsigned long)h);return 10;}}
static D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES p={};p.Type=type;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;}
static D3D12_RESOURCE_DESC buffer(uint64_t bytes,bool uav=false){
 D3D12_RESOURCE_DESC d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=1;
 d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 if(uav)d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;return d;
}
int main(){
 puts("AIOS_UNDERDOG_BCN_SHADER_SAMPLE_V02");fflush(stdout);
 ComPtr<IDXGIFactory4> factory;CK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;
 for(UINT i=0;factory->EnumAdapters1(i,&adapter)==S_OK;i++){
  DXGI_ADAPTER_DESC1 d={};adapter->GetDesc1(&d);
  if(!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))break;adapter.Reset();
 }
 if(!adapter)return 2;
 ComPtr<ID3D12Device> dev;CK(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)));
 const char* hlsl=R"(
Texture2D<float4> compressed:register(t0);
RWStructuredBuffer<uint> result:register(u0);
[numthreads(4,1,1)]
void main(uint3 id:SV_DispatchThreadID){
 uint mip=id.x;
 float4 color=compressed.Load(int3(0,0,mip));
 uint4 c=(uint4)round(saturate(color)*255.0);
 result[id.x]=c.x|(c.y<<8)|(c.z<<16)|(c.w<<24);
}
)";
 ComPtr<ID3DBlob> blob,errors;
 HRESULT compile=D3DCompile(hlsl,strlen(hlsl),"UnderdogBC",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&blob,&errors);
 printf("SHADER_COMPILE=0x%08lX\n",(unsigned long)compile);
 if(FAILED(compile)){if(errors)printf("ERROR:%.*s\n",(int)errors->GetBufferSize(),(char*)errors->GetBufferPointer());return 3;}
 D3D12_DESCRIPTOR_RANGE range={};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=1;
 D3D12_ROOT_PARAMETER roots[2]={};
 roots[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
 roots[0].DescriptorTable.NumDescriptorRanges=1;roots[0].DescriptorTable.pDescriptorRanges=&range;
 roots[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;roots[1].Descriptor.ShaderRegister=0;
 for(auto& x:roots)x.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
 D3D12_ROOT_SIGNATURE_DESC rsd={};rsd.NumParameters=2;rsd.pParameters=roots;
 ComPtr<ID3DBlob> serialized;CK(D3D12SerializeRootSignature(&rsd,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errors));
 ComPtr<ID3D12RootSignature> root;
 CK(dev->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
 D3D12_COMPUTE_PIPELINE_STATE_DESC psd={};psd.pRootSignature=root.Get();psd.CS={blob->GetBufferPointer(),blob->GetBufferSize()};
 ComPtr<ID3D12PipelineState> pso;CK(dev->CreateComputePipelineState(&psd,IID_PPV_ARGS(&pso)));
 D3D12_DESCRIPTOR_HEAP_DESC hd={};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
 ComPtr<ID3D12DescriptorHeap> descriptors;CK(dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&descriptors)));
 D3D12_COMMAND_QUEUE_DESC q={};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> queue;CK(dev->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> allocator;CK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
 ComPtr<ID3D12GraphicsCommandList> commands;CK(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)));
 ComPtr<ID3D12Fence> fence;CK(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 HANDLE ev=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ev)return 4;
 const DXGI_FORMAT formats[]={DXGI_FORMAT_BC1_UNORM,DXGI_FORMAT_BC3_UNORM,DXGI_FORMAT_BC7_UNORM};
 const char* names[]={"BC1","BC3","BC7"};
 const uint16_t colors[]={0xF800,0x07E0,0x001F,0xFFE0};
 const uint32_t expected[]={0xFF0000FF,0xFF00FF00,0xFFFF0000,0xFF00FFFF};
 unsigned completed=0;
 for(UINT format=0;format<3;format++){
  if(format){CK(allocator->Reset());CK(commands->Reset(allocator.Get(),nullptr));}
  D3D12_RESOURCE_DESC texture={};texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture.Width=16;texture.Height=16;texture.DepthOrArraySize=1;texture.MipLevels=4;
  texture.Format=formats[format];texture.SampleDesc.Count=1;
  auto def=heap(D3D12_HEAP_TYPE_DEFAULT),up=heap(D3D12_HEAP_TYPE_UPLOAD),rb=heap(D3D12_HEAP_TYPE_READBACK);
  ComPtr<ID3D12Resource> tex;
  CK(dev->CreateCommittedResource(&def,D3D12_HEAP_FLAG_NONE,&texture,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&tex)));
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[4]={};
  UINT rows[4]={};UINT64 rowSizes[4]={},total=0;
  dev->GetCopyableFootprints(&texture,0,4,0,footprints,rows,rowSizes,&total);
  auto uploadDesc=buffer(total),outputDesc=buffer(16,true),readbackDesc=buffer(16);
  ComPtr<ID3D12Resource> upload,out,readback;
  CK(dev->CreateCommittedResource(&up,D3D12_HEAP_FLAG_NONE,&uploadDesc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload)));
  CK(dev->CreateCommittedResource(&def,D3D12_HEAP_FLAG_NONE,&outputDesc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&out)));
  CK(dev->CreateCommittedResource(&rb,D3D12_HEAP_FLAG_NONE,&readbackDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
  void* ptr=nullptr;D3D12_RANGE empty={0,0};
  CK(upload->Map(0,&empty,&ptr));memset(ptr,0,(size_t)total);
  for(UINT mip=0;mip<4;mip++){
   auto& foot=footprints[mip];
   for(UINT row=0;row<rows[mip];row++){
    uint8_t* line=(uint8_t*)ptr+foot.Offset+row*foot.Footprint.RowPitch;
    for(UINT block=0;block<rowSizes[mip]/(format?16:8);block++){
     uint8_t* bx=line+block*(format?16:8);
     if(format==2){
       // BC7 mode 6: prefix bit6 set; alpha endpoints 7-bit 127.
       // Both p-bits zero, all color and index bits zero:
       // expected RGBA=(0,0,0,254) independently of mip.
       bx[0]|=uint8_t(1u<<6);
       for(unsigned bit=49;bit<63;bit++)bx[bit/8]|=uint8_t(1u<<(bit%8));
       continue;
      }
      if(format==1){
       bx[0]=255;bx[1]=0; // alpha endpoint 255 and zero, all indices 0 => opaque
       bx+=8;
      }
     const auto color=colors[mip];
     bx[0]=uint8_t(color&255);bx[1]=uint8_t(color>>8);
     bx[2]=0;bx[3]=0; // second 565 endpoint black; all indices 0
   }
  }
  }
  upload->Unmap(0,nullptr);
  for(UINT mip=0;mip<4;mip++){
   D3D12_TEXTURE_COPY_LOCATION from={};from.pResource=upload.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   from.PlacedFootprint=footprints[mip];
   D3D12_TEXTURE_COPY_LOCATION to={};to.pResource=tex.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.SubresourceIndex=mip;
   commands->CopyTextureRegion(&to,0,0,0,&from,nullptr);
  }
  D3D12_RESOURCE_BARRIER transition={};transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  transition.Transition.pResource=tex.Get();transition.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  transition.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;
  transition.Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  commands->ResourceBarrier(1,&transition);
  D3D12_SHADER_RESOURCE_VIEW_DESC srv={};
  srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv.Format=formats[format];srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=4;
  dev->CreateShaderResourceView(tex.Get(),&srv,descriptors->GetCPUDescriptorHandleForHeapStart());
  commands->SetPipelineState(pso.Get());commands->SetComputeRootSignature(root.Get());
  ID3D12DescriptorHeap* heaps[]={descriptors.Get()};commands->SetDescriptorHeaps(1,heaps);
  commands->SetComputeRootDescriptorTable(0,descriptors->GetGPUDescriptorHandleForHeapStart());
  commands->SetComputeRootUnorderedAccessView(1,out->GetGPUVirtualAddress());
  commands->Dispatch(1,1,1);
  D3D12_RESOURCE_BARRIER uav={};uav.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;uav.UAV.pResource=out.Get();
  commands->ResourceBarrier(1,&uav);
  D3D12_RESOURCE_BARRIER copy={};copy.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  copy.Transition.pResource=out.Get();copy.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  copy.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;copy.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
  commands->ResourceBarrier(1,&copy);
  commands->CopyResource(readback.Get(),out.Get());
  CK(commands->Close());
  ID3D12CommandList* lists[]={commands.Get()};queue->ExecuteCommandLists(1,lists);
  CK(queue->Signal(fence.Get(),format+1));CK(fence->SetEventOnCompletion(format+1,ev));
  if(WaitForSingleObject(ev,2000)!=WAIT_OBJECT_0)return 5;
  D3D12_RANGE readable={0,16};void* values=nullptr;CK(readback->Map(0,&readable,&values));
  for(UINT mip=0;mip<4;mip++){
   uint32_t got=((const uint32_t*)values)[mip];
   printf("FORMAT=%s MIP=%u GPU_RGBA=0x%08X EXPECT=0x%08X\n",names[format],mip,got,(format==2?0xFE000000u:expected[mip]));
   if(got!=(format==2?0xFE000000u:expected[mip])){printf("DECODE_MISMATCH=%s_MIP_%u\n",names[format],mip);return 6;}
   completed++;
  }
  readback->Unmap(0,&empty);
  printf("FORMAT_SHADER_SAMPLE_PASS=%s\n",names[format]);
 }
 CloseHandle(ev);
 printf("MIP_SHADER_SAMPLES_PASS=%u DEVICE_REMOVED_REASON=0x%08lX\n",completed,(unsigned long)dev->GetDeviceRemovedReason());
 puts("END_BCN_SHADER_SAMPLE_V03");
 return completed==12?0:7;
}

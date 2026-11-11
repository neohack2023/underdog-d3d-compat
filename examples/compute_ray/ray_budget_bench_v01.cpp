// AIOS Underdog Lab: compute-based ray-sphere fallback, standalone native D3D12.
// This is an app-owned alternative shader path, NOT driver-level DXR emulation.
// No Fortnite process, anti-cheat, game files, system drivers, or proxy DLLs.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
using Microsoft::WRL::ComPtr;
#define CHECK(EXPR) {HRESULT r=(EXPR);printf(#EXPR " = 0x%08lX\n",(unsigned long)r);fflush(stdout);if(FAILED(r))return 10;}
static unsigned WIDTH=128,HEIGHT=128,PIXELS=128*128;
static D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE t){
 D3D12_HEAP_PROPERTIES p={};p.Type=t;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;
}

struct Vertex {float x,y,z,w;};
struct Triangle {Vertex a,b,c;};
struct Node {Vertex minv,maxv;uint32_t left,right,start,count;};
static float coord(const Vertex& v,int axis){return axis==0?v.x:axis==1?v.y:v.z;}
static Vertex vtx(float x,float y,float z){return {x,y,z,0};}
static Vertex mn(Vertex a,Vertex b){return vtx(std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z));}
static Vertex mx(Vertex a,Vertex b){return vtx(std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z));}
static std::vector<Triangle> makeMesh(){
 std::vector<Triangle> tris;
 const Vertex verts[]={
 vtx(-0.8,-0.8,0.2),vtx(0.8,-0.8,0.2),vtx(0.8,0.8,0.2),vtx(-0.8,0.8,0.2),
 vtx(-0.8,-0.8,1.8),vtx(0.8,-0.8,1.8),vtx(0.8,0.8,1.8),vtx(-0.8,0.8,1.8)};
 const int faces[][3]={{0,2,1},{0,3,2},{4,5,6},{4,6,7},
 {0,1,5},{0,5,4},{1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
 for(const auto& f:faces)tris.push_back({verts[f[0]],verts[f[1]],verts[f[2]]});
 return tris;
}
static std::vector<Node> nodes;
static std::vector<Triangle> ordered;
static uint32_t buildBVH(std::vector<Triangle> list,int depth){
 uint32_t id=(uint32_t)nodes.size();nodes.push_back({});
 Vertex low=vtx(1e20f,1e20f,1e20f),high=vtx(-1e20f,-1e20f,-1e20f);
 Vertex clow=low,chigh=high;
 for(const auto& t:list){
  low=mn(low,mn(t.a,mn(t.b,t.c)));high=mx(high,mx(t.a,mx(t.b,t.c)));
  Vertex centroid=vtx((t.a.x+t.b.x+t.c.x)/3,(t.a.y+t.b.y+t.c.y)/3,(t.a.z+t.b.z+t.c.z)/3);
  clow=mn(clow,centroid);chigh=mx(chigh,centroid);
 }
 nodes[id].minv=low;nodes[id].maxv=high;
 if(list.size()<=2||depth>=12){
  nodes[id].start=(uint32_t)ordered.size();nodes[id].count=(uint32_t)list.size();
  for(auto& t:list)ordered.push_back(t);
  return id;
 }
 Vertex delta=vtx(chigh.x-clow.x,chigh.y-clow.y,chigh.z-clow.z);
 int axis=(delta.x>=delta.y&&delta.x>=delta.z)?0:((delta.y>=delta.z)?1:2);
 std::sort(list.begin(),list.end(),[&](const Triangle&a,const Triangle&b){
  auto avg=[&](const Triangle& t){return(coord(t.a,axis)+coord(t.b,axis)+coord(t.c,axis))/3;};
  return avg(a)<avg(b);
 });
 const auto mid=list.begin()+list.size()/2;
 std::vector<Triangle> a(list.begin(),mid),b(mid,list.end());
 auto l=buildBVH(std::move(a),depth+1),r=buildBVH(std::move(b),depth+1);
 nodes[id].left=l;nodes[id].right=r;
 return id;
}
int main(int argc,char** argv){
 if(argc<2) {puts("USAGE ray_budget_bench_v01.exe 32|64|128 [warp]");return 18;}
 const unsigned n=static_cast<unsigned>(atoi(argv[1]));
 if(n!=32 && n!=64 && n!=128)return 18;
 WIDTH=HEIGHT=n;PIXELS=n*n;
 const bool warp=argc>2 && strcmp(argv[2],"warp")==0;
 const unsigned rate=argc>3?static_cast<unsigned>(atoi(argv[3])):1u;
 if(rate!=1 && rate!=2 && rate!=4)return 23;
 puts("AIOS_D3D12_RAY_GPU_TIMESTAMP_BUDGET_V01");fflush(stdout);
 ComPtr<IDXGIFactory4> factory;
 CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;
 if(warp){CHECK(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));}
 else for(unsigned i=0;factory->EnumAdapters1(i,&adapter)==S_OK;i++){
  DXGI_ADAPTER_DESC1 desc={};adapter->GetDesc1(&desc);
  if(!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){
   printf("adapter_vendor=0x%04X adapter_device=0x%04X\n",desc.VendorId,desc.DeviceId);
   break;
  }
  adapter.Reset();
 }
 if(!adapter)return 11;
 ComPtr<ID3D12Device> dev;
 CHECK(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)));
 const char* shader=R"(
struct Tri {float4 a; float4 b; float4 c;};
struct Node {float4 lower;float4 upper;uint left;uint right;uint start;uint count;};
StructuredBuffer<Tri> triangles:register(t0);
StructuredBuffer<Node> nodes:register(t1);
RWStructuredBuffer<uint> rgba:register(u0);
bool boxHit(float3 o,float3 d,float3 lo,float3 hi,float maxT){
 float3 inv=1.0/(sign(d)*max(abs(d),0.000001));
 float3 t0=(lo-o)*inv,t1=(hi-o)*inv;
 float3 a=min(t0,t1),b=max(t0,t1);
 float nearT=max(max(a.x,a.y),max(a.z,0));
 float farT=min(min(b.x,b.y),b.z);
 return farT>=nearT && nearT<maxT;
}
bool triHit(float3 o,float3 d,Tri tri,out float dist,out float3 normal){
 float3 a=tri.a.xyz,b=tri.b.xyz,c=tri.c.xyz;
 float3 e1=b-a,e2=c-a,p=cross(d,e2);
 float det=dot(e1,p);dist=0;normal=float3(0,0,1);
 if(abs(det)<0.000001)return false;
 float inv=1.0/det;
 float3 s=o-a;float u=dot(s,p)*inv;
 if(u<0 || u>1)return false;
 float3 q=cross(s,e1),vData=q;
 float v=dot(d,q)*inv;
 if(v<0 || u+v>1)return false;
 dist=dot(e2,q)*inv;
 normal=normalize(cross(e1,e2));
 return dist>0.0001;
}
[numthreads(8,8,1)]
void main(uint3 tid:SV_DispatchThreadID){
 if(tid.x>=TRACE_DIM||tid.y>=TRACE_DIM)return;
 uint index=tid.y*TRACE_DIM+tid.x;
 // Sparse schedule proof only. Missing pixels are not reconstructed here.
 // Real integration must preserve raster scene and apply depth/normal aware reconstruction.
 if(RAY_DIVISOR>1 && (index % RAY_DIVISOR)!=0) {
   rgba[index]=0xFF101010;
   return;
 }
 float2 uv=(float2(tid.xy)+0.5)/float(TRACE_DIM);
 float2 screen=(uv*2-1)*0.95;
 float3 origin=float3(0,0,-3),dir=normalize(float3(screen.x,-screen.y,1.5));
 float hit=100000;float3 normal=float3(0,0,1);
 uint hitId=0xffffffff;
 uint stack[32];uint sp=0;stack[sp++]=0;
 [loop] while(sp>0){
  uint id=stack[--sp];
  Node n=nodes[id];
  if(!boxHit(origin,dir,n.lower.xyz,n.upper.xyz,hit))continue;
  if(n.count>0){
   [loop] for(uint j=0;j<n.count;j++){
    float t;float3 trialNormal;
    if(triHit(origin,dir,triangles[n.start+j],t,trialNormal)&&t<hit){
     hit=t;normal=trialNormal;hitId=n.start+j;
    }
   }
  } else {
   if(sp+2<=32){stack[sp++]=n.left;stack[sp++]=n.right;}
  }
 }
 float3 col=lerp(float3(.04,.06,.12),float3(.15,.25,.40),uv.y);
 if(hitId!=0xffffffff){
  float light=saturate(abs(dot(normal,normalize(float3(-.4,.7,-1.0)))));
  float3 base=lerp(float3(.15,.7,.82),float3(.95,.38,.18),float(hitId%5)/4.0);
  col=base*(.18+.82*light);
 }
 uint3 rgb=(uint3)round(saturate(col)*255);
 rgba[tid.y*TRACE_DIM+tid.x]=rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xFF000000;
}
)";
 ComPtr<ID3DBlob> blob,errors;
 auto started=std::chrono::steady_clock::now();
 const std::string hlsl=std::string("#define TRACE_DIM ")+std::to_string(WIDTH)+
     "\n#define RAY_DIVISOR "+std::to_string(rate)+"\n"+shader;
 HRESULT hr=D3DCompile(hlsl.data(),hlsl.size(),"AIOSRayFallback",nullptr,nullptr,"main","cs_5_1",
    D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&blob,&errors);
 printf("ray_compute_compile=0x%08lX\n",(unsigned long)hr);
 if(FAILED(hr)){
  if(errors)printf("HLSL_error=%.*s\n",(int)errors->GetBufferSize(),(char*)errors->GetBufferPointer());
  return 12;
 }
 auto mesh=makeMesh();nodes.clear();ordered.clear();buildBVH(mesh,0);
 printf("mesh_triangles=%u bvh_nodes=%u depth_guard=12\n",(unsigned)ordered.size(),(unsigned)nodes.size());
 D3D12_ROOT_PARAMETER params[3]={};
 params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
 params[0].Descriptor.ShaderRegister=0;
 params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
 params[1].Descriptor.ShaderRegister=0;
 params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
 params[2].Descriptor.ShaderRegister=1;
 for(auto& p:params)p.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
 D3D12_ROOT_SIGNATURE_DESC rootDesc={};rootDesc.NumParameters=3;rootDesc.pParameters=params;
 ComPtr<ID3DBlob> serialized;
 CHECK(D3D12SerializeRootSignature(&rootDesc,D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&errors));
 ComPtr<ID3D12RootSignature> root;
 CHECK(dev->CreateRootSignature(0,serialized->GetBufferPointer(),serialized->GetBufferSize(),IID_PPV_ARGS(&root)));
 D3D12_COMPUTE_PIPELINE_STATE_DESC desc={};desc.pRootSignature=root.Get();
 desc.CS.pShaderBytecode=blob->GetBufferPointer();
 desc.CS.BytecodeLength=blob->GetBufferSize();
 ComPtr<ID3D12PipelineState> pso;
 CHECK(dev->CreateComputePipelineState(&desc,IID_PPV_ARGS(&pso)));
 auto compiled=std::chrono::steady_clock::now();
 printf("compile_and_pso_ms=%.3f\n",std::chrono::duration<double,std::milli>(compiled-started).count());
 D3D12_RESOURCE_DESC buffer={};
 buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
 buffer.Width=PIXELS*4;buffer.Height=1;buffer.DepthOrArraySize=1;buffer.MipLevels=1;
 buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 buffer.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
 const auto d=heap(D3D12_HEAP_TYPE_DEFAULT);
 ComPtr<ID3D12Resource> gpu;
 CHECK(dev->CreateCommittedResource(&d,D3D12_HEAP_FLAG_NONE,&buffer,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&gpu)));
 buffer.Flags=D3D12_RESOURCE_FLAG_NONE;
 const auto readbackHeapProps=heap(D3D12_HEAP_TYPE_READBACK);
 ComPtr<ID3D12Resource> rb;
 CHECK(dev->CreateCommittedResource(&readbackHeapProps,D3D12_HEAP_FLAG_NONE,&buffer,
    D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&rb)));

 // GPU-accessible, persistently owned, upload memory for small deterministic mesh fixture.
 auto uploadHeapProps=heap(D3D12_HEAP_TYPE_UPLOAD);
 auto makeUpload=[&](const void* bytes,uint64_t size,ComPtr<ID3D12Resource>& resource){
  D3D12_RESOURCE_DESC bd={};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=size;
  bd.Height=1;bd.DepthOrArraySize=1;bd.MipLevels=1;bd.SampleDesc.Count=1;
  bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  HRESULT h=dev->CreateCommittedResource(&uploadHeapProps,D3D12_HEAP_FLAG_NONE,&bd,
    D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&resource));
  if(FAILED(h))return h;
  void* destination=nullptr;D3D12_RANGE range={0,0};
  h=resource->Map(0,&range,&destination);
  if(SUCCEEDED(h)){memcpy(destination,bytes,(size_t)size);resource->Unmap(0,nullptr);}
  return h;
 };
 ComPtr<ID3D12Resource> triBuffer,nodeBuffer;
 CHECK(makeUpload(ordered.data(),ordered.size()*sizeof(Triangle),triBuffer));
 CHECK(makeUpload(nodes.data(),nodes.size()*sizeof(Node),nodeBuffer));
 D3D12_COMMAND_QUEUE_DESC queueDesc={};queueDesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> queue;
 CHECK(dev->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)));
 ComPtr<ID3D12CommandAllocator> allocator;
 CHECK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
 ComPtr<ID3D12GraphicsCommandList> list;
 CHECK(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
 list->SetComputeRootSignature(root.Get());
 list->SetPipelineState(pso.Get());
 list->SetComputeRootUnorderedAccessView(0,gpu->GetGPUVirtualAddress());
 list->SetComputeRootShaderResourceView(1,triBuffer->GetGPUVirtualAddress());
 list->SetComputeRootShaderResourceView(2,nodeBuffer->GetGPUVirtualAddress());
 D3D12_QUERY_HEAP_DESC queryDesc={};
 queryDesc.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;queryDesc.Count=2;
 ComPtr<ID3D12QueryHeap> queryHeap;
 CHECK(dev->CreateQueryHeap(&queryDesc,IID_PPV_ARGS(&queryHeap)));
 D3D12_RESOURCE_DESC timestampsDesc={};
 timestampsDesc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;timestampsDesc.Width=16;
 timestampsDesc.Height=1;timestampsDesc.DepthOrArraySize=1;timestampsDesc.MipLevels=1;
 timestampsDesc.SampleDesc.Count=1;timestampsDesc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 ComPtr<ID3D12Resource> timestampReadback;
 CHECK(dev->CreateCommittedResource(&readbackHeapProps,D3D12_HEAP_FLAG_NONE,
      &timestampsDesc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&timestampReadback)));
 UINT64 timestampFreq=0;
 CHECK(queue->GetTimestampFrequency(&timestampFreq));
 if(timestampFreq==0)return 21;
 list->EndQuery(queryHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
 list->Dispatch(WIDTH/8,HEIGHT/8,1);
 list->EndQuery(queryHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);
 list->ResolveQueryData(queryHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,
                        timestampReadback.Get(),0);
 D3D12_RESOURCE_BARRIER uav={};uav.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;uav.UAV.pResource=gpu.Get();
 list->ResourceBarrier(1,&uav);
 D3D12_RESOURCE_BARRIER transition={};
 transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
 transition.Transition.pResource=gpu.Get();
 transition.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
 transition.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
 transition.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
 list->ResourceBarrier(1,&transition);
 list->CopyResource(rb.Get(),gpu.Get());
 CHECK(list->Close());
 ID3D12CommandList* lists[]={list.Get()};
 const auto begin=std::chrono::steady_clock::now();
 queue->ExecuteCommandLists(1,lists);
 ComPtr<ID3D12Fence> fence;
 CHECK(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 CHECK(queue->Signal(fence.Get(),1));
 HANDLE ev=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!ev)return 13;
 CHECK(fence->SetEventOnCompletion(1,ev));
 auto wait=WaitForSingleObject(ev,1500);CloseHandle(ev);
 printf("ray_fence_wait=%s\n",wait==WAIT_OBJECT_0?"PASS":"FAILED");
 printf("device_removed_reason=0x%08lX\n",(unsigned long)dev->GetDeviceRemovedReason());
 if(wait!=WAIT_OBJECT_0)return 14;
 uint64_t ticks[2]={};
 void* stampPointer=nullptr;D3D12_RANGE stampRange={0,16};
 CHECK(timestampReadback->Map(0,&stampRange,&stampPointer));
 memcpy(ticks,stampPointer,16);
 D3D12_RANGE emptyStamp={0,0};timestampReadback->Unmap(0,&emptyStamp);
 if(ticks[1]<ticks[0])return 22;
 const double rayGpuMs=double(ticks[1]-ticks[0])*1000.0/double(timestampFreq);
 printf("ray_gpu_timestamp_ms=%.6f\n",rayGpuMs);
 const auto done=std::chrono::steady_clock::now();
 printf("dispatch_to_readback_fence_ms=%.3f\n",std::chrono::duration<double,std::milli>(done-begin).count());
 D3D12_RANGE range={0,PIXELS*4};void* pointer=nullptr;
 CHECK(rb->Map(0,&range,&pointer));
 const auto rgba=(const uint32_t*)pointer;
 const auto mid=rgba[(HEIGHT/2)*WIDTH+(WIDTH/2)],edge=rgba[3*WIDTH+3];
 printf("center_rgba_packed=0x%08X corner_rgba_packed=0x%08X\n",mid,edge);
 const char* path="ray_budget_bench.rgba";
 FILE* out=nullptr;
 if(fopen_s(&out,path,"wb")!=0||!out){printf("raw_output_write=FAILED\n");rb->Unmap(0,nullptr);return 15;}
 size_t written=fwrite(pointer,1,PIXELS*4,out);fclose(out);
 D3D12_RANGE empty={0,0};rb->Unmap(0,&empty);
 printf("raw_output_bytes=%zu\n",written);
 printf("ray_output=%s\n",path);
 printf("RAY_BENCH_SUMMARY adapter=%s dimension=%u rays=%u gpu_ms=%.6f\n",
        warp?"WARP":"HARDWARE",WIDTH,PIXELS/rate,rayGpuMs);
 puts("END_D3D12_COMPUTE_RAY_BUDGET");fflush(stdout);
 return written==PIXELS*4 && mid!=edge?0:16;
}

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
#include <thread>
#include <numeric>
#include <fstream>
#include <sstream>
#include <string>
using Microsoft::WRL::ComPtr;
#define CHECK(EXPR) {HRESULT r=(EXPR);printf(#EXPR " = 0x%08lX\n",(unsigned long)r);fflush(stdout);if(FAILED(r))return 10;}
const unsigned WIDTH=128,HEIGHT=128, PIXELS=WIDTH*HEIGHT;
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
static std::vector<Triangle> makeMesh(const char* filename){
 // Own test fixture only. Supports v + triangular f with 1-based positive vertex indices.
 // Reject invalid indices, malformed geometry, and unbounded input instead of guessing.
 std::ifstream file(filename);
 if(!file){puts("obj_file_open=FAILED");return {};}
 std::vector<Vertex> vertices;
 std::vector<Triangle> faces;
 std::string line;
 while(std::getline(file,line)){
  std::istringstream stream(line);
  std::string kind;stream>>kind;
  if(kind=="v"){
   float x,y,z;
   if(!(stream>>x>>y>>z)){puts("obj_invalid_vertex");return {};}
   if(vertices.size()>=4096){puts("obj_vertex_limit");return {};}
   vertices.push_back(vtx(x,y,z));
  }else if(kind=="f"){
   int a,b,c;
   if(!(stream>>a>>b>>c)){puts("obj_only_plain_triangle_faces_supported");return {};}
   if(a<=0||b<=0||c<=0||a>(int)vertices.size()||b>(int)vertices.size()||c>(int)vertices.size()){
    puts("obj_invalid_index");return {};
   }
   if(faces.size()>=4096){puts("obj_face_limit");return {};}
   faces.push_back({vertices[a-1],vertices[b-1],vertices[c-1]});
  }
 }
 printf("obj_source_vertices=%zu obj_triangles=%zu\n",vertices.size(),faces.size());
 return faces;
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
 const char* fixture=argc>1?argv[1]:"fixtures\\UNDERDOG_CUBE_12TRI_V01.obj";
 const unsigned requestedFrames=argc>2?(unsigned)atoi(argv[2]):60;
 if(requestedFrames<10||requestedFrames>120){puts("frame_count_outside_10_120");return 19;}
 puts("AIOS_D3D12_OBJ_BVH_MULTIFRAME_V02");fflush(stdout);
 ComPtr<IDXGIFactory4> factory;
 CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
 ComPtr<IDXGIAdapter1> adapter;
 for(unsigned i=0;factory->EnumAdapters1(i,&adapter)==S_OK;i++){
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
 if(tid.x>=128||tid.y>=128)return;
 float2 uv=(float2(tid.xy)+0.5)/128.0;
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
 rgba[tid.y*128+tid.x]=rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xFF000000;
}
)";
 ComPtr<ID3DBlob> blob,errors;
 auto started=std::chrono::steady_clock::now();
 HRESULT hr=D3DCompile(shader,strlen(shader),"AIOSRayFallback",nullptr,nullptr,"main","cs_5_1",
    D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&blob,&errors);
 printf("ray_compute_compile=0x%08lX\n",(unsigned long)hr);
 if(FAILED(hr)){
  if(errors)printf("HLSL_error=%.*s\n",(int)errors->GetBufferSize(),(char*)errors->GetBufferPointer());
  return 12;
 }
 auto mesh=makeMesh(fixture);if(mesh.empty())return 17;nodes.clear();ordered.clear();buildBVH(mesh,0);
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
 // Continuous offscreen D3D12 test: 60 sequential frames with genuine GPU timestamp pairs.
 // Commands reuse the same fixed mesh/BVH/output buffer, without GPU allocations in the loop.
 const unsigned FRAMES=requestedFrames;
 D3D12_COMMAND_QUEUE_DESC qdesc={};qdesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> queue;CHECK(dev->CreateCommandQueue(&qdesc,IID_PPV_ARGS(&queue)));
 UINT64 frequency=0;CHECK(queue->GetTimestampFrequency(&frequency));
 printf("gpu_timestamp_frequency_hz=%llu\n",(unsigned long long)frequency);
 D3D12_QUERY_HEAP_DESC queryDesc={};queryDesc.Count=FRAMES*2;
 queryDesc.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
 ComPtr<ID3D12QueryHeap> queryHeap;
 CHECK(dev->CreateQueryHeap(&queryDesc,IID_PPV_ARGS(&queryHeap)));
 D3D12_RESOURCE_DESC timingDesc={};
 timingDesc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
 timingDesc.Width=FRAMES*2*sizeof(uint64_t);
 timingDesc.Height=1;timingDesc.DepthOrArraySize=1;timingDesc.MipLevels=1;
 timingDesc.SampleDesc.Count=1;timingDesc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 ComPtr<ID3D12Resource> timings;
 CHECK(dev->CreateCommittedResource(&readbackHeapProps,D3D12_HEAP_FLAG_NONE,&timingDesc,
   D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&timings)));
 ComPtr<ID3D12CommandAllocator> allocator;
 CHECK(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
 ComPtr<ID3D12GraphicsCommandList> list;
 CHECK(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
 ComPtr<ID3D12Fence> fence;CHECK(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
 HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event)return 13;
 ComPtr<IDXGIAdapter3> adapter3;adapter.As(&adapter3);
 uint64_t budget=0,firstUsage=0,maxUsage=0,lastUsage=0;unsigned samples=0;
 auto sampleBudget=[&](){
  if(adapter3){
   DXGI_QUERY_VIDEO_MEMORY_INFO m={};
   if(SUCCEEDED(adapter3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&m))){
    budget=m.Budget;lastUsage=m.CurrentUsage;
    if(samples==0)firstUsage=lastUsage;
    maxUsage=std::max(maxUsage,lastUsage);samples++;
   }
  }
 };
 sampleBudget();
 std::vector<double> frameIntervals,submitToFence;
 auto start=std::chrono::steady_clock::now(),lastTick=start;
 const auto step=std::chrono::duration<double,std::milli>(1000.0/30.0);
 unsigned completed=0;
 for(unsigned i=0;i<FRAMES;i++){
  const auto target=start+std::chrono::duration_cast<std::chrono::steady_clock::duration>(step*i);
  std::this_thread::sleep_until(target);
  const auto tick=std::chrono::steady_clock::now();
  if(i>0)frameIntervals.push_back(std::chrono::duration<double,std::milli>(tick-lastTick).count());
  lastTick=tick;
  if(i>0){CHECK(allocator->Reset());CHECK(list->Reset(allocator.Get(),nullptr));}
  list->SetComputeRootSignature(root.Get());
  list->SetPipelineState(pso.Get());
  list->SetComputeRootUnorderedAccessView(0,gpu->GetGPUVirtualAddress());
  list->SetComputeRootShaderResourceView(1,triBuffer->GetGPUVirtualAddress());
  list->SetComputeRootShaderResourceView(2,nodeBuffer->GetGPUVirtualAddress());
  list->EndQuery(queryHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2);
  list->Dispatch(16,16,1);
  D3D12_RESOURCE_BARRIER uav={};uav.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;uav.UAV.pResource=gpu.Get();
  list->ResourceBarrier(1,&uav);
  list->EndQuery(queryHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2+1);
  list->ResolveQueryData(queryHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2,2,timings.Get(),i*16);
  if(i==FRAMES-1){
   D3D12_RESOURCE_BARRIER transition={};transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   transition.Transition.pResource=gpu.Get();transition.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   transition.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   transition.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
   list->ResourceBarrier(1,&transition);
   list->CopyResource(rb.Get(),gpu.Get());
  }
  CHECK(list->Close());
  ID3D12CommandList* cmds[]={list.Get()};
  const auto submitted=std::chrono::steady_clock::now();
  queue->ExecuteCommandLists(1,cmds);
  CHECK(queue->Signal(fence.Get(),i+1));
  CHECK(fence->SetEventOnCompletion(i+1,event));
  const DWORD waited=WaitForSingleObject(event,2000);
  if(waited!=WAIT_OBJECT_0){
   printf("FRAME_FAILURE=%u FENCE_TIMEOUT\n",i);
   CloseHandle(event);return 14;
  }
  submitToFence.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-submitted).count());
  completed++;sampleBudget();
 }
 CloseHandle(event);
 printf("frames_completed=%u\n",completed);
 printf("obj_fixture=%s triangle_count=%u bvh_count=%u\n",fixture,(unsigned)ordered.size(),(unsigned)nodes.size());
 printf("device_removed_reason=0x%08lX\n",(unsigned long)dev->GetDeviceRemovedReason());
 D3D12_RANGE readable={0,FRAMES*16};void* ptr=nullptr;CHECK(timings->Map(0,&readable,&ptr));
 const auto* stamp=(const uint64_t*)ptr;
 std::vector<double> gpuDurations;
 for(unsigned i=0;i<FRAMES;i++){
  if(stamp[i*2+1]<stamp[i*2]){printf("INVALID_TIMESTAMP_PAIR=%u\n",i);return 15;}
  const double ms=1000.0*double(stamp[i*2+1]-stamp[i*2])/double(frequency);
  gpuDurations.push_back(ms);
 }
 D3D12_RANGE zero={0,0};timings->Unmap(0,&zero);
 auto stats=[&](const char* label,std::vector<double> v){
  if(v.empty())return;
  std::sort(v.begin(),v.end());
  const double mean=std::accumulate(v.begin(),v.end(),0.0)/v.size();
  printf("%s_mean_ms=%.4f %s_p50_ms=%.4f %s_p95_ms=%.4f %s_p99_ms=%.4f %s_max_ms=%.4f\n",
    label,mean,label,v[v.size()/2],label,v[(v.size()-1)*95/100],label,v[(v.size()-1)*99/100],label,v.back());
 };
 stats("gpu_shader",gpuDurations);
 stats("cpu_frame_interval",frameIntervals);
 stats("cpu_submit_fence",submitToFence);
 printf("memory_budget_bytes=%llu mem_usage_start_bytes=%llu mem_usage_end_bytes=%llu mem_usage_peak_bytes=%llu mem_samples=%u\n",
    (unsigned long long)budget,(unsigned long long)firstUsage,(unsigned long long)lastUsage,
    (unsigned long long)maxUsage,samples);
 D3D12_RANGE dataRange={0,PIXELS*4};void* pixels=nullptr;CHECK(rb->Map(0,&dataRange,&pixels));
 const uint32_t* rgb=(const uint32_t*)pixels;
 printf("final_frame_center_pixel=0x%08X corner_pixel=0x%08X\n",rgb[64*WIDTH+64],rgb[3*WIDTH+3]);
 if(rgb[64*WIDTH+64]==rgb[3*WIDTH+3])return 16;
 FILE* file=nullptr;
 const char* outpath=argc>3?argv[3]:"measurements\\UNDERDOG_OBJ_BVH_MULTIFRAME_V02.raw";
 if(fopen_s(&file,outpath,"wb")==0 && file){
  fwrite(pixels,1,PIXELS*4,file);fclose(file);
 }
 rb->Unmap(0,&zero);
 const auto stop=std::chrono::steady_clock::now();
 printf("total_wall_seconds=%.4f\n",std::chrono::duration<double>(stop-start).count());
 puts("END_UNDERDOG_MULTIFRAME_GPU_TIMESTAMPS");
 return 0;
}

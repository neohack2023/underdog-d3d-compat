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
#include "underdog/Runtime.h"
#include <d3dcompiler.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <memory>
using Microsoft::WRL::ComPtr;
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
int main(int argc,char** argv){
 const unsigned targetFrames=argc>1?(unsigned)atoi(argv[1]):120;
 const bool typedUav=argc>4 && strcmp(argv[4],"--typed-uav")==0;
 if(argc>4 && !typedUav){puts("UNKNOWN_RENDER_MODE");return 25;}
 printf("OUTPUT_BACKEND=%s\n",typedUav?"TYPED_TEXTURE_TABLE":"STRUCTURED_BUFFER_ROOT");
 if(targetFrames<10||targetFrames>600)return 19;
 unsigned cameraActions=0;float camera=0.0f;
 puts("AIOS_UNDERDOG_ARENA_BVH_SWAPCHAIN_V03");fflush(stdout);
 HINSTANCE instance=GetModuleHandleW(nullptr);
 WNDCLASSW cls={};cls.lpfnWndProc=WndProc;cls.hInstance=instance;
 cls.lpszClassName=L"AIOS_Underdog_D3D12_Win";
 if(!RegisterClassW(&cls)){puts("WINDOW_CLASS_FAIL");return 2;}
 HWND hwnd=CreateWindowExW(0,cls.lpszClassName,L"Underdog Arena | Intel HD 520 | LEFT / RIGHT to move camera",
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
 underdog::Runtime runtime(device.Get());
 printf("UNDERDOG_RAY_BACKEND=%u\n",(unsigned)runtime.chooseRayBackend().path);
 AIOSUnderdog::TextureRef t1,t2,t3;
 auto h1=runtime.createTexture(800,800,DXGI_FORMAT_R16_UNORM,t1);
 auto h2=runtime.createTexture(800,800,DXGI_FORMAT_R16_UNORM,t2);
 printf("INDEPENDENT_APP_BUDGET_FIRST=0x%08lX SECOND=0x%08lX\n",(unsigned long)h1,(unsigned long)h2);
 if(FAILED(h1)||h2!=E_OUTOFMEMORY)return 5;
 // No GPU command ever uses the budgeted texture; free it after we have submitted no work that references it.
 if(!runtime.retireTexture(t1.id,1))return 6;
 if(runtime.collectCompleted(0)!=0)return 7;
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
 if(runtime.collectCompleted(fence->GetCompletedValue())!=1)return 11;
 auto h3=runtime.createTexture(800,800,DXGI_FORMAT_R16_UNORM,t3);
 printf("INDEPENDENT_APP_BUDGET_AFTER_FENCE=0x%08lX\n",(unsigned long)h3);
 if(FAILED(h3))return 12;

 const char* meshPath=argc>3?argv[3]:"fixtures/cube_12tri.obj";
 auto mesh=makeMesh(meshPath);
 if(mesh.empty())return 20;
 nodes.clear();ordered.clear();buildBVH(mesh,0);
 printf("SCENE_TRIANGLES=%zu BVH_NODES=%zu\n",ordered.size(),nodes.size());
 const char* shader=R"SHADER(

struct Tri {float4 a; float4 b; float4 c;};
struct Node {float4 lower;float4 upper;uint left;uint right;uint start;uint count;};
StructuredBuffer<Tri> triangles:register(t0);
StructuredBuffer<Node> nodes:register(t1);
RWStructuredBuffer<uint> rgba:register(u0);
cbuffer Camera:register(b0){float horizontal;};
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
 float3 origin=float3(horizontal,0,-3),dir=normalize(float3(screen.x,-screen.y,1.5));
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

)SHADER";
 std::string shaderText(shader);
 if(typedUav){
  const std::string oldDeclaration="RWStructuredBuffer<uint> rgba:register(u0);";
  const std::string oldStore="rgba[tid.y*128+tid.x]=rgb.x|(rgb.y<<8)|(rgb.z<<16)|0xFF000000;";
  if(shaderText.find(oldDeclaration)==std::string::npos || shaderText.find(oldStore)==std::string::npos)
   return 26;
  shaderText.replace(shaderText.find(oldDeclaration),oldDeclaration.size(),
    "RWTexture2D<float4> rgba:register(u0);");
  shaderText.replace(shaderText.find(oldStore),oldStore.size(),
    "rgba[tid.xy]=float4(float3(rgb)/255.0,1.0);");
 }
 ComPtr<ID3DBlob> cs,err;
 HRESULT hr=D3DCompile(shaderText.c_str(),shaderText.size(),"UnderdogArena",nullptr,nullptr,"main","cs_5_1",
 D3DCOMPILE_OPTIMIZATION_LEVEL1,0,&cs,&err);
 if(FAILED(hr)){if(err)printf("HLSL_ERROR=%.*s\n",(int)err->GetBufferSize(),(char*)err->GetBufferPointer());return 21;}
 D3D12_ROOT_PARAMETER params[4]={};
 D3D12_DESCRIPTOR_RANGE typedRange={};
 typedRange.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;typedRange.NumDescriptors=1;
 typedRange.BaseShaderRegister=0;
 typedRange.OffsetInDescriptorsFromTableStart=D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
 if(typedUav){
  params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  params[0].DescriptorTable.NumDescriptorRanges=1;
  params[0].DescriptorTable.pDescriptorRanges=&typedRange;
 }else{
  params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
  params[0].Descriptor.ShaderRegister=0;
 }
 params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;params[1].Descriptor.ShaderRegister=0;
 params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;params[2].Descriptor.ShaderRegister=1;
 params[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[3].Constants.Num32BitValues=1;
 params[3].Constants.ShaderRegister=0;
 for(auto& p:params)p.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
 D3D12_ROOT_SIGNATURE_DESC rd={};rd.NumParameters=4;rd.pParameters=params;
 ComPtr<ID3DBlob> packed;
 CK(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&packed,&err));
 ComPtr<ID3D12RootSignature> root;
 CK(device->CreateRootSignature(0,packed->GetBufferPointer(),packed->GetBufferSize(),IID_PPV_ARGS(&root)));
 D3D12_COMPUTE_PIPELINE_STATE_DESC pd={};pd.pRootSignature=root.Get();pd.CS={cs->GetBufferPointer(),cs->GetBufferSize()};
 ComPtr<ID3D12PipelineState> compute;
 HRESULT psoResult=device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&compute));
 if(FAILED(psoResult)){
  printf("COMPUTE_PSO_FAILED=0x%08lX\n",(unsigned long)psoResult);
  return 24;
 }
 auto up= D3D12_HEAP_PROPERTIES{};up.Type=D3D12_HEAP_TYPE_UPLOAD;up.CreationNodeMask=1;up.VisibleNodeMask=1;
 auto def=D3D12_HEAP_PROPERTIES{};def.Type=D3D12_HEAP_TYPE_DEFAULT;def.CreationNodeMask=1;def.VisibleNodeMask=1;
 auto makeSceneBuffer=[&](const void* bytes,UINT64 size,ComPtr<ID3D12Resource>& resource)->HRESULT{
  D3D12_RESOURCE_DESC desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width=size;desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;
  desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  HRESULT status=device->CreateCommittedResource(&up,D3D12_HEAP_FLAG_NONE,&desc,
   D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&resource));
  if(FAILED(status))return status;
  D3D12_RANGE empty={0,0};void* dst=nullptr;status=resource->Map(0,&empty,&dst);
  if(SUCCEEDED(status)){memcpy(dst,bytes,(size_t)size);resource->Unmap(0,nullptr);}
  return status;
 };
 ComPtr<ID3D12Resource> triangles,tree;
 CK(makeSceneBuffer(ordered.data(),ordered.size()*sizeof(Triangle),triangles));
 CK(makeSceneBuffer(nodes.data(),nodes.size()*sizeof(Node),tree));
 D3D12_RESOURCE_DESC td={};
 td.Dimension=typedUav?D3D12_RESOURCE_DIMENSION_TEXTURE2D:D3D12_RESOURCE_DIMENSION_BUFFER;
 td.Width=typedUav?128:(128*128*4);td.Height=typedUav?128:1;td.DepthOrArraySize=1;td.MipLevels=1;
 td.SampleDesc.Count=1;td.Format=typedUav?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_UNKNOWN;
 td.Layout=typedUav?D3D12_TEXTURE_LAYOUT_UNKNOWN:D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 td.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
 const auto resourceKind=typedUav?underdog::ComputeOutputKind::TypedUavTexture2D:
   underdog::ComputeOutputKind::PackedRgba8Buffer;
 const auto preflight=underdog::validateComputeOutputDesc(td,128,128,resourceKind);
 printf("OUTPUT_DESC_PREFLIGHT=%s REASON=%s\n",preflight.valid?"PASS":"FAIL",preflight.reason);
 if(!preflight.valid)return 27;
 ComPtr<ID3D12Resource> scene;
 CK(device->CreateCommittedResource(&def,D3D12_HEAP_FLAG_NONE,&td,
   D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&scene)));
 const auto actual=underdog::validateComputeOutputDesc(scene->GetDesc(),128,128,resourceKind);
 if(!actual.valid){printf("OUTPUT_ACTUAL_DESC_FAIL=%s\n",actual.reason);return 28;}
 ComPtr<ID3D12DescriptorHeap> typedHeap;
 if(typedUav){
  D3D12_DESCRIPTOR_HEAP_DESC typedDesc={};
  typedDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  typedDesc.NumDescriptors=1;typedDesc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  CK(device->CreateDescriptorHeap(&typedDesc,IID_PPV_ARGS(&typedHeap)));
  D3D12_UNORDERED_ACCESS_VIEW_DESC uav={};
  uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
  uav.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
  device->CreateUnorderedAccessView(scene.Get(),nullptr,&uav,
    typedHeap->GetCPUDescriptorHandleForHeapStart());
 }
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT captureFoot={};
 captureFoot.Offset=0;captureFoot.Footprint.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 captureFoot.Footprint.Width=128;captureFoot.Footprint.Height=128;
 captureFoot.Footprint.Depth=1;captureFoot.Footprint.RowPitch=128*4;
 UINT64 totalBytes=128ull*128*4;
 auto rbHeap=D3D12_HEAP_PROPERTIES{};rbHeap.Type=D3D12_HEAP_TYPE_READBACK;
 rbHeap.CreationNodeMask=1;rbHeap.VisibleNodeMask=1;
 D3D12_RESOURCE_DESC captureDesc={};
 captureDesc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;captureDesc.Width=totalBytes;
 captureDesc.Height=1;captureDesc.DepthOrArraySize=1;captureDesc.MipLevels=1;
 captureDesc.SampleDesc.Count=1;captureDesc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 ComPtr<ID3D12Resource> capture;
 CK(device->CreateCommittedResource(&rbHeap,D3D12_HEAP_FLAG_NONE,&captureDesc,
  D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&capture)));

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
 D3D12_QUERY_HEAP_DESC hd={};hd.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;hd.Count=targetFrames*2;
 ComPtr<ID3D12QueryHeap> query;
 CK(device->CreateQueryHeap(&hd,IID_PPV_ARGS(&query)));
 D3D12_RESOURCE_DESC timing={};timing.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
 timing.Width=(UINT64)targetFrames*16;timing.Height=1;timing.DepthOrArraySize=1;
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
 for(unsigned i=0;i<targetFrames && running;i++){
  MSG msg;
  while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
   if(msg.message==WM_QUIT){running=false;break;}
   TranslateMessage(&msg);DispatchMessageW(&msg);
  }
  if(!running)break;
  if(GetAsyncKeyState(VK_LEFT)&0x8000){camera-=.015f;cameraActions++;}
  if(GetAsyncKeyState(VK_RIGHT)&0x8000){camera+=.015f;cameraActions++;}
  if(argc>2 && strcmp(argv[2],"--autoplay")==0){camera=.7f*sinf(float(i)*.034f);cameraActions++;}
  camera=(std::max)(-2.0f,(std::min)(2.0f,camera));
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
  if(i){
   D3D12_RESOURCE_BARRIER tx={};tx.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   tx.Transition.pResource=scene.Get();tx.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   tx.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;
   tx.Transition.StateAfter=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   list->ResourceBarrier(1,&tx);
  }
  list->SetComputeRootSignature(root.Get());
  list->SetPipelineState(compute.Get());
  if(typedUav){
   ID3D12DescriptorHeap* heaps[]={typedHeap.Get()};
   list->SetDescriptorHeaps(1,heaps);
   list->SetComputeRootDescriptorTable(0,typedHeap->GetGPUDescriptorHandleForHeapStart());
  }else list->SetComputeRootUnorderedAccessView(0,scene->GetGPUVirtualAddress());
  list->SetComputeRootShaderResourceView(1,triangles->GetGPUVirtualAddress());
  list->SetComputeRootShaderResourceView(2,tree->GetGPUVirtualAddress());
  uint32_t shiftBits=0;memcpy(&shiftBits,&camera,sizeof(float));
  list->SetComputeRoot32BitConstant(3,shiftBits,0);
  list->Dispatch(16,16,1);
  D3D12_RESOURCE_BARRIER u={};u.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;u.UAV.pResource=scene.Get();
  list->ResourceBarrier(1,&u);
  D3D12_RESOURCE_BARRIER tx={};tx.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  tx.Transition.pResource=scene.Get();tx.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  tx.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  tx.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
  list->ResourceBarrier(1,&tx);

  auto cpu=rtv->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=back*step;
  const float color[4]={0.11f+(i%12)*.018f,0.25f,0.44f-(i%10)*.022f,1.0f};
  list->ClearRenderTargetView(cpu,color,0,nullptr);
  // The swapchain's RTV is cleared first, then the compute raytraced texture
  // is copied directly into the backbuffer. No CPU image upload is involved.
  barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
  list->ResourceBarrier(1,&barrier);
  D3D12_TEXTURE_COPY_LOCATION dst={};dst.pResource=buffers[back].Get();
  dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=0;
  D3D12_TEXTURE_COPY_LOCATION src={};src.pResource=scene.Get();
  if(typedUav){
   src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.SubresourceIndex=0;
  }else{
   src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   src.PlacedFootprint=captureFoot;
  }
  list->CopyTextureRegion(&dst,336,161,0,&src,nullptr);
  if(i==targetFrames-1){
   if(typedUav){
    D3D12_TEXTURE_COPY_LOCATION dstCapture={};dstCapture.pResource=capture.Get();
    dstCapture.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstCapture.PlacedFootprint=captureFoot;
    list->CopyTextureRegion(&dstCapture,0,0,0,&src,nullptr);
   }else list->CopyBufferRegion(capture.Get(),0,scene.Get(),0,totalBytes);
  }
  list->EndQuery(query.Get(),D3D12_QUERY_TYPE_TIMESTAMP,i*2+1);
  barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;
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
 printf("SWAPCHAIN_FRAMES=%u PRESENT_SUCCESS=%u CAMERA_ACTIONS=%u CAMERA_FINAL=%.3f\n",frames,completed,cameraActions,camera);
 if(completed<30)return 15;
 D3D12_RANGE range={0,completed*16};void* mapped=nullptr;
 CK(readback->Map(0,&range,&mapped));
 const auto* times=(const uint64_t*)mapped;
 std::vector<double> gpu;
 for(unsigned i=0;i<completed;i++)gpu.push_back(double(times[i*2+1]-times[i*2])*1000.0/frequency);
 D3D12_RANGE zero={0,0};readback->Unmap(0,&zero);
 printf("GPU_BVH_COPY_P50_MS=%.4f GPU_BVH_COPY_P95_MS=%.4f\n",percentile(gpu,.5),percentile(gpu,.95));
 printf("PRESENT_CALL_P50_MS=%.4f PRESENT_CALL_P95_MS=%.4f\n",percentile(presents,.5),percentile(presents,.95));
 printf("WINDOW_FRAME_INTERVAL_P50_MS=%.4f WINDOW_FRAME_INTERVAL_P95_MS=%.4f\n",percentile(intervals,.5),percentile(intervals,.95));
 printf("QUEUE_FENCE_P95_MS=%.4f\n",percentile(fences,.95));
 DXGI_QUERY_VIDEO_MEMORY_INFO mem={};
 if(adapter3 && SUCCEEDED(adapter3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&mem)))
  printf("DXGI_USAGE_END_BYTES=%llu DXGI_BUDGET_BYTES=%llu\n",
   (unsigned long long)mem.CurrentUsage,(unsigned long long)mem.Budget);
 printf("DEVICE_REMOVED_REASON=0x%08lX\n",(unsigned long)device->GetDeviceRemovedReason());
 printf("TEST_WALL_SECONDS=%.3f\n",std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());
 // Verify a non-uniform GPU image from the same texture copied to the screen.
 D3D12_RANGE readRange={0,(SIZE_T)totalBytes};void* captured=nullptr;
 CK(capture->Map(0,&readRange,&captured));
 const auto* base=(const uint8_t*)captured;
 uint32_t center=0,corner=0;
 memcpy(&center,base+64*captureFoot.Footprint.RowPitch+64*4,4);
 memcpy(&corner,base+3*captureFoot.Footprint.RowPitch+3*4,4);
 FILE* screenshot=nullptr;
 const std::string sceneName=std::string(meshPath).find("3200")!=std::string::npos?"grid3200":
   (std::string(meshPath).find("800")!=std::string::npos?"grid800":"cube12");
 const std::string witness="build/ARENA_BVH_GPU_WITNESS_V03_"+
   std::string(typedUav?"typed_":"structured_")+sceneName+".ppm";
 if(fopen_s(&screenshot,witness.c_str(),"wb")==0 && screenshot){
  fprintf(screenshot,"P6\n128 128\n255\n");
  for(unsigned y=0;y<128;y++)for(unsigned x=0;x<128;x++)
   fwrite(base+y*captureFoot.Footprint.RowPitch+x*4,1,3,screenshot);
  fclose(screenshot);
 }
 capture->Unmap(0,nullptr);
 printf("ARENA_GPU_CENTER=0x%08X CORNER=0x%08X WITNESS_VALID=%d\n",center,corner,center!=corner);
 printf("RENDER_MODE=%s GPU_WITNESS_PATH=%s\n",
   typedUav?"TYPED_TEXTURE_TABLE":"STRUCTURED_BUFFER_ROOT",witness.c_str());
 CloseHandle(ev);
 puts("END_UNDERDOG_ARENA_BVH_PRESENT_V03");
 if(center==corner)return 23;
 return 0;
}

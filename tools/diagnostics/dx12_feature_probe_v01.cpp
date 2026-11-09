// SPDX-License-Identifier: MIT
// Read-only D3D12 capability inspector. Intel hardware only, no WARP.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
using Microsoft::WRL::ComPtr;
static std::string hexhr(HRESULT h){std::ostringstream x;x<<"0x"<<std::uppercase<<std::hex<<std::setw(8)<<std::setfill('0')<<(unsigned)h;return x.str();}
static std::string narrow(const wchar_t* w){char a[512]={};WideCharToMultiByte(CP_UTF8,0,w,-1,a,sizeof(a),nullptr,nullptr);return a;}
int main(){
 ComPtr<IDXGIFactory6> fac;HRESULT h=CreateDXGIFactory2(0,IID_PPV_ARGS(&fac));if(FAILED(h)){std::cerr<<"DXGI factory "<<hexhr(h);return 2;}
 ComPtr<IDXGIAdapter1> gpu;DXGI_ADAPTER_DESC1 desc={};bool found=false;
 for(UINT i=0;fac->EnumAdapters1(i,&gpu)==S_OK;i++){gpu->GetDesc1(&desc);if(desc.VendorId==0x8086 && !(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){found=true;break;}gpu.Reset();}
 if(!found){std::cerr<<"No Intel hardware adapter; WARP prohibited";return 3;}
 ComPtr<ID3D12Device> dev;h=D3D12CreateDevice(gpu.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev));
 if(FAILED(h)){std::cerr<<"CreateDevice "<<hexhr(h);return 4;}
 std::cout<<"{\n\"schema\":\"underdog.dx12.feature_probe.v1\",\"adapter\":\""<<narrow(desc.Description)
 <<"\",\"vendor_id\":"<<desc.VendorId<<",\"device_id\":"<<desc.DeviceId
 <<",\"dedicated_memory\":"<<(unsigned long long)desc.DedicatedVideoMemory
 <<",\"shared_memory\":"<<(unsigned long long)desc.SharedSystemMemory<<",\n";
 const D3D_FEATURE_LEVEL req[]={D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0,D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
 D3D12_FEATURE_DATA_FEATURE_LEVELS fl={};fl.NumFeatureLevels=4;fl.pFeatureLevelsRequested=req;
 HRESULT fhr=dev->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,&fl,sizeof(fl));
 std::cout<<"\"feature_level\":{\"hr\":\""<<hexhr(fhr)<<"\",\"max\":"<<(SUCCEEDED(fhr)?(unsigned)fl.MaxSupportedFeatureLevel:0)<<"},\n";
 const D3D_SHADER_MODEL mods[]={D3D_SHADER_MODEL_6_6,D3D_SHADER_MODEL_6_5,D3D_SHADER_MODEL_6_4,D3D_SHADER_MODEL_6_3,D3D_SHADER_MODEL_6_2,D3D_SHADER_MODEL_6_1,D3D_SHADER_MODEL_6_0,D3D_SHADER_MODEL_5_1};
 std::cout<<"\"shader_models\":[";
 for(int i=0;i<8;i++){D3D12_FEATURE_DATA_SHADER_MODEL sm={};sm.HighestShaderModel=mods[i];HRESULT r=dev->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&sm,sizeof(sm));if(i)std::cout<<",";std::cout<<"{\"request\":"<<(unsigned)mods[i]<<",\"hr\":\""<<hexhr(r)<<"\",\"reported\":"<<(SUCCEEDED(r)?(unsigned)sm.HighestShaderModel:0)<<"}";}
 std::cout<<"],\n";
 D3D12_FEATURE_DATA_ROOT_SIGNATURE root={};root.HighestVersion=D3D_ROOT_SIGNATURE_VERSION_1_1;
 HRESULT rhr=dev->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE,&root,sizeof(root));
 std::cout<<"\"root_signature\":{\"hr\":\""<<hexhr(rhr)<<"\",\"reported\":"<<(SUCCEEDED(rhr)?(unsigned)root.HighestVersion:0)<<"},\n";
 D3D12_FEATURE_DATA_D3D12_OPTIONS opt={};HRESULT ohr=dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&opt,sizeof(opt));
 std::cout<<"\"options\":{\"hr\":\""<<hexhr(ohr)<<"\",\"binding_tier\":"<<(SUCCEEDED(ohr)?(unsigned)opt.ResourceBindingTier:0)
 <<",\"heap_tier\":"<<(SUCCEEDED(ohr)?(unsigned)opt.ResourceHeapTier:0)
 <<",\"tiled_tier\":"<<(SUCCEEDED(ohr)?(unsigned)opt.TiledResourcesTier:0)
 <<",\"conservative_raster_tier\":"<<(SUCCEEDED(ohr)?(unsigned)opt.ConservativeRasterizationTier:0)
 <<",\"typed_uav_additional\":"<<(SUCCEEDED(ohr)&&opt.TypedUAVLoadAdditionalFormats?"true":"false")
 <<",\"rov\":"<<(SUCCEEDED(ohr)&&opt.ROVsSupported?"true":"false")<<"},\n";
 D3D12_FEATURE_DATA_D3D12_OPTIONS1 opt1={};HRESULT h1=dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1,&opt1,sizeof(opt1));
 std::cout<<"\"options1\":{\"hr\":\""<<hexhr(h1)<<"\",\"wave_ops\":"<<(SUCCEEDED(h1)&&opt1.WaveOps?"true":"false")<<"},\n";
 D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5={};HRESULT h5=dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&opt5,sizeof(opt5));
 std::cout<<"\"options5\":{\"hr\":\""<<hexhr(h5)<<"\",\"raytracing_tier\":"<<(SUCCEEDED(h5)?(unsigned)opt5.RaytracingTier:0)<<"},\n";
 struct F{DXGI_FORMAT format;const char* name;};
 const F formats[]={{DXGI_FORMAT_R8G8B8A8_UNORM,"RGBA8"},{DXGI_FORMAT_B8G8R8A8_UNORM,"BGRA8"},{DXGI_FORMAT_R16G16B16A16_FLOAT,"RGBA16F"},{DXGI_FORMAT_BC1_UNORM,"BC1"},{DXGI_FORMAT_BC3_UNORM,"BC3"},{DXGI_FORMAT_BC7_UNORM,"BC7"},{DXGI_FORMAT_D32_FLOAT,"D32F"},{DXGI_FORMAT_R8G8B8A8_UINT,"RGBA8_UINT"}};
 std::cout<<"\"formats\":[";
 for(int i=0;i<8;i++){D3D12_FEATURE_DATA_FORMAT_SUPPORT f={};f.Format=formats[i].format;HRESULT r=dev->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&f,sizeof(f));if(i)std::cout<<",";std::cout<<"{\"name\":\""<<formats[i].name<<"\",\"dxgi_format\":"<<(unsigned)f.Format<<",\"hr\":\""<<hexhr(r)<<"\",\"support1\":"<<(SUCCEEDED(r)?(unsigned)f.Support1:0)<<",\"support2\":"<<(SUCCEEDED(r)?(unsigned)f.Support2:0)<<"}";}
 std::cout<<"]\n}\n";return 0;
}

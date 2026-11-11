// Independent, opt-in D3D11 GS-like textured sprite rendering fixture.
// MIT-compatible self-contained implementation; no PS2Recomp code included.
// Compare against a separately compiled, GPL-3.0 CPU GS oracle's RGBA witness.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
constexpr UINT kW=64, kH=64, kTW=8, kTH=8;
static const char* kShader=R"(
Texture2D<uint4> Tex : register(t0);
struct VOut { float4 pos : SV_Position; };
VOut VS(uint id : SV_VertexID) {
    float2 corners[6] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(0,1), float2(1,0), float2(1,1)
    };
    float2 pix = float2(8.0, 8.0) + corners[id]*32.0;
    VOut o;
    o.pos = float4(pix.x/32.0-1.0, 1.0-pix.y/32.0, 0.5, 1.0);
    return o;
}
uint4 PS(VOut i) : SV_Target0 {
    uint tx = min(7u, (uint)max(0.0, floor((i.pos.x-8.0)/4.0)));
    uint ty = min(7u, (uint)max(0.0, floor((i.pos.y-8.0)/4.0)));
    return Tex.Load(int3(tx,ty,0));
}
)";
static uint64_t fnv64(const uint8_t* p,size_t n) {
    uint64_t h=14695981039346656037ull;
    for(size_t i=0;i<n;++i) {h^=p[i]; h*=1099511628211ull;}
    return h;
}
static std::vector<uint8_t> load(const char* file,size_t size) {
    std::ifstream in(file,std::ios::binary);
    if(!in) return {};
    std::vector<uint8_t> out(size);
    in.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(size));
    if(!in || in.peek()!=EOF) return {};
    return out;
}
static int fail(const char* stage,HRESULT hr=E_FAIL) {
    std::fprintf(stderr,"{\"status\":\"FAIL\",\"stage\":\"%s\",\"hr\":\"0x%08lx\"}\n",
                stage,static_cast<unsigned long>(hr));
    return 2;
}
static bool compile(const char* entry,const char* target,ComPtr<ID3DBlob>& blob) {
    ComPtr<ID3DBlob> error;
    HRESULT hr=D3DCompile(kShader,strlen(kShader),nullptr,nullptr,nullptr,
            entry,target,D3DCOMPILE_ENABLE_STRICTNESS,0,blob.GetAddressOf(),error.GetAddressOf());
    if(FAILED(hr) && error)
        std::fprintf(stderr,"HLSL_ERROR %s\n",static_cast<const char*>(error->GetBufferPointer()));
    return SUCCEEDED(hr);
}
int main(int argc,char** argv) {
    const bool warp=argc>1 && std::strcmp(argv[1],"warp")==0;
    const std::vector<uint8_t> source=load("gs_texture_8x8.rgba",kTW*kTH*4);
    const std::vector<uint8_t> oracle=load("gs_cpu_64x64.rgba",kW*kH*4);
    if(source.size()!=kTW*kTH*4 || oracle.size()!=kW*kH*4)
        return fail("missing_cpu_oracle_or_texture");
    ComPtr<IDXGIAdapter1> intel;
    DXGI_ADAPTER_DESC1 desc{};
    if(!warp) {
        ComPtr<IDXGIFactory1> factory;
        HRESULT hr=CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        if(FAILED(hr)) return fail("dxgi_factory",hr);
        for(UINT n=0;;++n) {
            ComPtr<IDXGIAdapter1> candidate;
            hr=factory->EnumAdapters1(n,&candidate);
            if(hr==DXGI_ERROR_NOT_FOUND) break;
            if(FAILED(hr)) return fail("dxgi_enum",hr);
            DXGI_ADAPTER_DESC1 info{};
            if(SUCCEEDED(candidate->GetDesc1(&info)) && info.VendorId==0x8086 && info.DeviceId==0x1916) {
                intel=candidate; desc=info; break;
            }
        }
        if(!intel) return fail("intel_hd520_not_found");
    }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL obtained{};
    const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_0};
    HRESULT hr=D3D11CreateDevice(warp ? nullptr:intel.Get(),
             warp ? D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_UNKNOWN,
             nullptr,0,levels,1,D3D11_SDK_VERSION,&device,&obtained,&context);
    if(FAILED(hr)) return fail("d3d11_device",hr);
    ComPtr<ID3DBlob> vsBlob,psBlob;
    if(!compile("VS","vs_5_0",vsBlob)) return fail("compile_vs");
    if(!compile("PS","ps_5_0",psBlob)) return fail("compile_ps");
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> pixel;
    hr=device->CreateVertexShader(vsBlob->GetBufferPointer(),vsBlob->GetBufferSize(),
                                  nullptr,&vertex);
    if(FAILED(hr)) return fail("vertex_shader",hr);
    hr=device->CreatePixelShader(psBlob->GetBufferPointer(),psBlob->GetBufferSize(),
                                 nullptr,&pixel);
    if(FAILED(hr)) return fail("pixel_shader",hr);
    D3D11_TEXTURE2D_DESC tex{};
    tex.Width=kTW;tex.Height=kTH;tex.MipLevels=1;tex.ArraySize=1;
    tex.Format=DXGI_FORMAT_R8G8B8A8_UINT;tex.SampleDesc.Count=1;
    tex.Usage=D3D11_USAGE_IMMUTABLE;tex.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem=source.data();init.SysMemPitch=kTW*4;
    ComPtr<ID3D11Texture2D> inTex;
    hr=device->CreateTexture2D(&tex,&init,&inTex);
    if(FAILED(hr)) return fail("input_texture",hr);
    ComPtr<ID3D11ShaderResourceView> srv;
    hr=device->CreateShaderResourceView(inTex.Get(),nullptr,&srv);
    if(FAILED(hr)) return fail("texture_srv",hr);

    D3D11_TEXTURE2D_DESC outDesc{};
    outDesc.Width=kW;outDesc.Height=kH;outDesc.MipLevels=1;outDesc.ArraySize=1;
    outDesc.Format=DXGI_FORMAT_R8G8B8A8_UINT;
    outDesc.SampleDesc.Count=1;outDesc.Usage=D3D11_USAGE_DEFAULT;
    outDesc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> framebuffer;
    hr=device->CreateTexture2D(&outDesc,nullptr,&framebuffer);
    if(FAILED(hr)) return fail("framebuffer",hr);
    ComPtr<ID3D11RenderTargetView> rtv;
    hr=device->CreateRenderTargetView(framebuffer.Get(),nullptr,&rtv);
    if(FAILED(hr)) return fail("rtv",hr);
    D3D11_TEXTURE2D_DESC stageDesc=outDesc;
    stageDesc.Usage=D3D11_USAGE_STAGING;
    stageDesc.BindFlags=0;
    stageDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    hr=device->CreateTexture2D(&stageDesc,nullptr,&staging);
    if(FAILED(hr)) return fail("staging",hr);

    D3D11_RASTERIZER_DESC rast{};
    rast.FillMode=D3D11_FILL_SOLID;
    rast.CullMode=D3D11_CULL_NONE;
    rast.DepthClipEnable=TRUE;
    ComPtr<ID3D11RasterizerState> rasterizer;
    hr=device->CreateRasterizerState(&rast,&rasterizer);
    if(FAILED(hr)) return fail("rasterizer",hr);
    D3D11_VIEWPORT viewport{};
    viewport.Width=static_cast<FLOAT>(kW);
    viewport.Height=static_cast<FLOAT>(kH);
    viewport.MaxDepth=1.0f;
    context->RSSetViewports(1,&viewport);
    context->RSSetState(rasterizer.Get());
    const float clear[4]={0,0,0,0};
    context->ClearRenderTargetView(rtv.Get(),clear);
    context->OMSetRenderTargets(1,rtv.GetAddressOf(),nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(vertex.Get(),nullptr,0);
    context->PSSetShader(pixel.Get(),nullptr,0);
    context->PSSetShaderResources(0,1,srv.GetAddressOf());
    context->Draw(6,0);
    context->OMSetRenderTargets(0,nullptr,nullptr);
    context->CopyResource(staging.Get(),framebuffer.Get());
    context->Flush();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr=context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped);
    if(FAILED(hr)) return fail("map_staging",hr);
    std::vector<uint8_t> gpu(kW*kH*4);
    for(UINT y=0;y<kH;++y)
        memcpy(gpu.data()+y*kW*4,
               static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch,kW*4);
    context->Unmap(staging.Get(),0);
    std::vector<uint8_t> differences(gpu.size());
    size_t differentPixels=0,differentChannels=0,nonzero=0;
    for(size_t i=0;i<gpu.size();i+=4) {
        bool pixelDiff=false,pixelNonzero=false;
        for(int c=0;c<4;++c) {
            const uint8_t a=gpu[i+c],b=oracle[i+c];
            const bool diff=a!=b;
            differentChannels+=diff;pixelDiff|=diff;pixelNonzero|=(a!=0);
            differences[i+c]=diff?255u:0u;
        }
        differentPixels+=pixelDiff;nonzero+=pixelNonzero;
    }
    const std::string name=warp?"gs_gpu_warp_64x64.rgba":"gs_gpu_intel_64x64.rgba";
    std::ofstream output(name,std::ios::binary);
    output.write(reinterpret_cast<const char*>(gpu.data()),static_cast<std::streamsize>(gpu.size()));
    if(!output) return fail("write_capture");
    const auto status=(differentPixels==0 && nonzero==1024 && SUCCEEDED(device->GetDeviceRemovedReason()))
                        ? "PASS":"FAIL";
    std::printf("{\"status\":\"%s\",\"adapter\":\"%s\",\"backend\":\"D3D11_PS_5_0\","
                "\"frame_width\":64,\"frame_height\":64,\"source_texels\":64,"
                "\"drawn_pixels\":%zu,\"differing_pixels\":%zu,\"differing_channels\":%zu,"
                "\"oracle_fnv64\":\"%016llx\",\"gpu_fnv64\":\"%016llx\","
                "\"real_gpu_rasterization\":true,\"game_boot\":false,\"device_removed_hr\":\"0x%08lx\"}\n",
                status,warp?"WARP":"Intel_HD_520",nonzero,differentPixels,differentChannels,
                static_cast<unsigned long long>(fnv64(oracle.data(),oracle.size())),
                static_cast<unsigned long long>(fnv64(gpu.data(),gpu.size())),
                static_cast<unsigned long>(device->GetDeviceRemovedReason()));
    return differentPixels==0 && nonzero==1024 ? 0:3;
}

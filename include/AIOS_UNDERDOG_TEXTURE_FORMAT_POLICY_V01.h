// AIOS Underdog cooperative texture capability policy V01.
// Self-owned renderer integration, no D3D12 impersonation or proxy injection.
#pragma once
#include <d3d12.h>
#include <dxgiformat.h>
#include <cstdint>
namespace AIOSUnderdog {
struct TextureCapabilities {
 bool bc1=false,bc3=false,rgba8=false;
 bool tiledResources=false;
 unsigned tiledTier=0;
 bool allMipTransfersTested=false; // runtime device query cannot establish test fidelity
};
inline bool supportsSampled2D(ID3D12Device* device,DXGI_FORMAT format){
 if(!device)return false;
 D3D12_FEATURE_DATA_FORMAT_SUPPORT v={};v.Format=format;
 if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&v,sizeof(v))))return false;
 const auto bits=D3D12_FORMAT_SUPPORT1_TEXTURE2D|D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
 return (v.Support1&bits)==bits;
}
inline TextureCapabilities probeTextureCapabilities(ID3D12Device* device){
 TextureCapabilities out;
 out.bc1=supportsSampled2D(device,DXGI_FORMAT_BC1_UNORM);
 out.bc3=supportsSampled2D(device,DXGI_FORMAT_BC3_UNORM);
 out.rgba8=supportsSampled2D(device,DXGI_FORMAT_R8G8B8A8_UNORM);
 D3D12_FEATURE_DATA_D3D12_OPTIONS opt={};
 if(device&&SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&opt,sizeof(opt)))){
  out.tiledTier=unsigned(opt.TiledResourcesTier);
  out.tiledResources=opt.TiledResourcesTier!=D3D12_TILED_RESOURCES_TIER_NOT_SUPPORTED;
 }
 return out;
}
// Format decision for cooperating applications which can prepare compatible assets.
// Never silently reinterpret pixels or claim that a texture's encoded bytes have changed.
inline DXGI_FORMAT chooseSampledFormat(const TextureCapabilities& c,bool alphaRequired,bool allowCompression){
 if(allowCompression){
  if(alphaRequired&&c.bc3)return DXGI_FORMAT_BC3_UNORM;
  if(!alphaRequired&&c.bc1)return DXGI_FORMAT_BC1_UNORM;
 }
 return c.rgba8?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_UNKNOWN;
}
inline uint64_t encodedByteCount(uint32_t width,uint32_t height,unsigned mips,DXGI_FORMAT fmt){
 if(width==0||height==0||mips==0)return 0;
 if(fmt!=DXGI_FORMAT_BC1_UNORM&&fmt!=DXGI_FORMAT_BC3_UNORM&&fmt!=DXGI_FORMAT_R8G8B8A8_UNORM)return 0;
 uint64_t total=0;
 for(unsigned i=0;i<mips;i++){
  const uint64_t w=width>0?width:1,h=height>0?height:1;
  if(fmt==DXGI_FORMAT_BC1_UNORM)total+=((w+3)/4)*((h+3)/4)*8ull;
  else if(fmt==DXGI_FORMAT_BC3_UNORM)total+=((w+3)/4)*((h+3)/4)*16ull;
  else total+=w*h*4ull;
  width=(width>1)?width/2:1;
  height=(height>1)?height/2:1;
 }
 return total;
}
}

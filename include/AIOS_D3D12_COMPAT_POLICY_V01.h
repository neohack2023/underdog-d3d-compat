// AIOS D3D12 cooperative compatibility library, prototype V01
// Source-owned application integration only; no proxy DLL or process injection.
#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <cstddef>
namespace AIOSCompat {
using Microsoft::WRL::ComPtr;
struct Caps {
 D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
 D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL_5_1;
 UINT resourceBindingTier=0, tiledTier=0, heapTier=0;
 bool raytracing=false, vrs=false, standardSwizzle=false;
 bool graphicsPSOCandidate=false;
};
inline Caps query(ID3D12Device* dev) {
 Caps c; if(!dev)return c;
 D3D_FEATURE_LEVEL candidates[]={D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0,D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
 D3D12_FEATURE_DATA_FEATURE_LEVELS fl={};
 fl.NumFeatureLevels=4;fl.pFeatureLevelsRequested=candidates;
 if(SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,&fl,sizeof(fl))))c.featureLevel=fl.MaxSupportedFeatureLevel;
 for(auto sm : {D3D_SHADER_MODEL_6_5,D3D_SHADER_MODEL_6_4,D3D_SHADER_MODEL_6_3,D3D_SHADER_MODEL_6_2,D3D_SHADER_MODEL_6_1,D3D_SHADER_MODEL_6_0,D3D_SHADER_MODEL_5_1}){
  D3D12_FEATURE_DATA_SHADER_MODEL data={sm};
  if(SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&data,sizeof(data)))){
   c.shaderModel=data.HighestShaderModel;break;
  }
 }
 D3D12_FEATURE_DATA_D3D12_OPTIONS op={};
 if(SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&op,sizeof(op)))){
  c.resourceBindingTier=op.ResourceBindingTier;
  c.tiledTier=op.TiledResourcesTier;c.heapTier=op.ResourceHeapTier;
  c.standardSwizzle=op.StandardSwizzle64KBSupported!=0;
 }
 D3D12_FEATURE_DATA_D3D12_OPTIONS5 op5={};
 if(SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&op5,sizeof(op5))))c.raytracing=op5.RaytracingTier!=D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
 D3D12_FEATURE_DATA_D3D12_OPTIONS6 op6={};
 if(SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6,&op6,sizeof(op6))))c.vrs=op6.VariableShadingRateTier!=D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED;
 c.graphicsPSOCandidate=c.featureLevel>=D3D_FEATURE_LEVEL_11_0;
 return c;
}
class TextureBudget {
 ID3D12Device* dev;
 uint64_t capBytes;
 uint64_t usedBytes=0;
 struct Owned {
  uint64_t bytes;
  ComPtr<ID3D12Heap> heap;
  ComPtr<ID3D12Resource> resource;
 };
 std::vector<Owned> items;
public:
 TextureBudget(ID3D12Device* d,uint64_t cap):dev(d),capBytes(cap){}
 uint64_t used() const{return usedBytes;}
 uint64_t capacity() const{return capBytes;}
 size_t count() const{return items.size();}
 HRESULT create2D(UINT width,UINT height,DXGI_FORMAT fmt,D3D12_HEAP_FLAGS flags=D3D12_HEAP_FLAG_NONE){
  if(!dev||!width||!height)return E_INVALIDARG;
  D3D12_RESOURCE_DESC desc={};
  desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width;desc.Height=height;
  desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=fmt;desc.SampleDesc.Count=1;
  desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
  const auto allocation=dev->GetResourceAllocationInfo(0,1,&desc);
  if(!allocation.SizeInBytes||allocation.SizeInBytes==UINT64_MAX)return E_INVALIDARG;
  if(allocation.SizeInBytes>capBytes || usedBytes>capBytes-allocation.SizeInBytes)return E_OUTOFMEMORY;
  D3D12_HEAP_DESC hd={};
  hd.SizeInBytes=allocation.SizeInBytes;hd.Flags=flags;
  hd.Properties.Type=D3D12_HEAP_TYPE_DEFAULT;
  hd.Properties.CreationNodeMask=1;hd.Properties.VisibleNodeMask=1;
  Owned entry={};entry.bytes=allocation.SizeInBytes;
  HRESULT hr=dev->CreateHeap(&hd,IID_PPV_ARGS(&entry.heap));
  if(FAILED(hr))return hr;
  hr=dev->CreatePlacedResource(entry.heap.Get(),0,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&entry.resource));
  if(FAILED(hr))return hr;
  usedBytes+=entry.bytes;
  items.push_back(std::move(entry));
  return S_OK;
 }
 void clear(){items.clear();usedBytes=0;}
};
} // namespace AIOSCompat

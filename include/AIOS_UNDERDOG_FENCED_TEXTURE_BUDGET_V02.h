// AIOS Underdog DX12 cooperative resource-budget library V02
// Application-owned integration. Not a driver, API replacement, or game hook.
// Designed for single-threaded owner; call collect() only with a genuinely
// completed GPU fence value, never a predicted or requested signal value.
#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <utility>
namespace AIOSUnderdog {
using Microsoft::WRL::ComPtr;
struct TextureRef {
 uint64_t id=0;
 ID3D12Resource* resource=nullptr; // Borrowed until retired fence collected.
 uint64_t sizeBytes=0;
};
class FencedTextureBudget {
 struct Item {
  uint64_t id=0,sizeBytes=0,retireFence=0;
  bool retiring=false;
  ComPtr<ID3D12Heap> heap;
  ComPtr<ID3D12Resource> resource;
 };
 ID3D12Device* device=nullptr; // Borrowed; caller keeps device alive.
 uint64_t capBytes=0,usedBytes=0,nextId=1;
 std::vector<Item> items;
public:
 explicit FencedTextureBudget(ID3D12Device* dev,uint64_t cap):device(dev),capBytes(cap){}
 uint64_t used() const{return usedBytes;}
 uint64_t capacity() const{return capBytes;}
 size_t count() const{return items.size();}
 bool retiring(uint64_t id) const{
  auto p=std::find_if(items.begin(),items.end(),[&](const Item& a){return a.id==id;});
  return p!=items.end() && p->retiring;
 }
 HRESULT create2D(UINT width,UINT height,DXGI_FORMAT format,TextureRef& output,D3D12_HEAP_FLAGS flags=D3D12_HEAP_FLAG_NONE){
  output=TextureRef{};
  if(!device||width==0||height==0)return E_INVALIDARG;
  D3D12_RESOURCE_DESC desc={};
  desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Width=width;desc.Height=height;desc.DepthOrArraySize=1;desc.MipLevels=1;
  desc.Format=format;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
  D3D12_RESOURCE_ALLOCATION_INFO info=device->GetResourceAllocationInfo(0,1,&desc);
  if(!info.SizeInBytes||info.SizeInBytes==UINT64_MAX)return E_INVALIDARG;
  if(info.SizeInBytes>capBytes || usedBytes>capBytes-info.SizeInBytes)return E_OUTOFMEMORY;
  D3D12_HEAP_DESC heapDesc={};
  heapDesc.SizeInBytes=info.SizeInBytes;heapDesc.Flags=flags;
  heapDesc.Properties.Type=D3D12_HEAP_TYPE_DEFAULT;
  heapDesc.Properties.CreationNodeMask=1;heapDesc.Properties.VisibleNodeMask=1;
  Item item={};
  item.id=nextId;item.sizeBytes=info.SizeInBytes;
  HRESULT hr=device->CreateHeap(&heapDesc,IID_PPV_ARGS(&item.heap));
  if(FAILED(hr))return hr;
  hr=device->CreatePlacedResource(item.heap.Get(),0,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,
    IID_PPV_ARGS(&item.resource));
  if(FAILED(hr))return hr;
  output.id=nextId;output.resource=item.resource.Get();output.sizeBytes=info.SizeInBytes;
  items.push_back(std::move(item));
  usedBytes+=info.SizeInBytes;
  nextId++;
  return S_OK;
 }
 bool retireAfterFence(uint64_t id,uint64_t value){
  auto p=std::find_if(items.begin(),items.end(),[&](const Item& a){return a.id==id;});
  if(p==items.end()||p->retiring||value==0)return false;
  p->retireFence=value;p->retiring=true;return true;
 }
 size_t collectCompleted(uint64_t actualCompletedFence){
  size_t n=0;
  for(auto i=items.begin();i!=items.end();){
   if(i->retiring&&i->retireFence<=actualCompletedFence){
    usedBytes-=i->sizeBytes;
    i=items.erase(i);++n;
   }else ++i;
  }
  return n;
 }
 // Caller must verify all its GPU references have completed before
 // destroying this manager. The manager deliberately has no unsafe clear().
};
}

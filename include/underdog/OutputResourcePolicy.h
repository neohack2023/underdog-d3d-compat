// Cooperative preflight for owned compute output resources, not a D3D driver.
#pragma once
#include <d3d12.h>
#include <cstdint>
namespace underdog {
enum class ComputeOutputKind : std::uint8_t {
 PackedRgba8Buffer, TypedUavTexture2D
};
struct OutputResourceCheck {
 bool valid=false;
 const char* reason="Resource description not validated";
};
inline OutputResourceCheck validateComputeOutputDesc(const D3D12_RESOURCE_DESC& d,
 UINT expectedWidth, UINT expectedHeight, ComputeOutputKind kind) noexcept {
 if(!expectedWidth || !expectedHeight || expectedWidth>16384 || expectedHeight>16384)
  return {false,"Invalid expected compute output dimensions"};
 if(!(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
  return {false,"Missing unordered access output flag"};
 if(d.SampleDesc.Count!=1 || d.DepthOrArraySize!=1 || d.MipLevels!=1)
  return {false,"Output must be a single-sample, single-subresource resource"};
 if(kind==ComputeOutputKind::TypedUavTexture2D){
  if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D)
   return {false,"Typed output must use a Texture2D resource"};
  if(d.Width!=expectedWidth || d.Height!=expectedHeight)
   return {false,"Typed output dimensions do not match shader dispatch image"};
  if(d.Format==DXGI_FORMAT_UNKNOWN || d.Layout!=D3D12_TEXTURE_LAYOUT_UNKNOWN)
   return {false,"Typed output requires a concrete format and default texture layout"};
 }else{
  if(d.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER ||
   d.Height!=1 || d.Layout!=D3D12_TEXTURE_LAYOUT_ROW_MAJOR ||
   d.Format!=DXGI_FORMAT_UNKNOWN)
   return {false,"Packed output must be a row-major untyped buffer"};
  if(d.Width<static_cast<UINT64>(expectedWidth)*expectedHeight*4u)
   return {false,"Packed output buffer is smaller than the expected RGBA8 pixels"};
 }
 return {true,"Output resource matches the specified compute image layout"};
}
} // namespace underdog

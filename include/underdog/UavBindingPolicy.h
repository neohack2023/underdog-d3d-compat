// Cooperative D3D12 UAV binding choice. Does not alter graphics drivers.
#pragma once
#include <cstdint>
namespace underdog {
enum class UavResourceKind : uint8_t {
 Unknown, StructuredBuffer, RawBuffer, TypedBuffer, TypedTexture2D
};
enum class UavBindingMethod : uint8_t {
 Unsupported, RootDescriptor, DescriptorTable
};
struct UavBindingRecommendation {
 UavBindingMethod method=UavBindingMethod::Unsupported;
 bool fallbackApplied=false;
 const char* reason="Resource declaration unknown; shader binding must be verified";
};
// Root UAV descriptors address untyped buffers directly. Typed UAV resources
// require descriptor tables for format interpretation. The caller must classify
// the HLSL declaration correctly; no shader reflection is performed here.
constexpr bool canUseRootUav(UavResourceKind kind) noexcept {
 return kind==UavResourceKind::StructuredBuffer ||
        kind==UavResourceKind::RawBuffer;
}
constexpr UavBindingRecommendation chooseUavBinding(
 UavResourceKind kind,bool preferRoot) noexcept {
 if(kind==UavResourceKind::Unknown)
  return {UavBindingMethod::Unsupported,false,
   "Unknown UAV declaration: no implicit hardware binding decision"};
 if(preferRoot && canUseRootUav(kind))
  return {UavBindingMethod::RootDescriptor,false,
   "Untyped structured/raw buffer may use a D3D12 root UAV descriptor"};
 if(preferRoot)
  return {UavBindingMethod::DescriptorTable,true,
   "Typed resource cannot use a root UAV; select a descriptor table"};
 return {UavBindingMethod::DescriptorTable,false,
   "UAV descriptor table selected"};
}
} // namespace underdog

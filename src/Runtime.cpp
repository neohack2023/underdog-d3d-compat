#include "underdog/Runtime.h"
#include <utility>
namespace underdog {
struct Runtime::State {
 Microsoft::WRL::ComPtr<ID3D12Device> device;
 RuntimeOptions options{};
 FeatureReport features{};
 AIOSUnderdog::TextureCapabilities textures{};
 std::unique_ptr<AIOSUnderdog::FencedTextureBudget> budget;
 State(ID3D12Device* dev,RuntimeOptions config):device(dev),options(config) {
  features.deviceAvailable=(dev!=nullptr);
  if(!dev)return;
  textures=AIOSUnderdog::probeTextureCapabilities(dev);
  features.bc1=textures.bc1;features.bc3=textures.bc3;features.rgba8=textures.rgba8;
  features.tiledResourcesTier=textures.tiledTier;
  features.computeShaders=true;
  D3D12_FEATURE_DATA_D3D12_OPTIONS1 opt1={};
  if(SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1,&opt1,sizeof(opt1))))
   features.waveOps=opt1.WaveOps!=FALSE;
  D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5={};
  if(SUCCEEDED(dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&opt5,sizeof(opt5))))
   features.raytracingNative=opt5.RaytracingTier!=D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
  budget=std::make_unique<AIOSUnderdog::FencedTextureBudget>(dev,options.textureBudgetBytes);
 }
};
Runtime::Runtime(ID3D12Device* dev,RuntimeOptions options):state_(std::make_unique<State>(dev,options)){}
Runtime::~Runtime()=default;
Runtime::Runtime(Runtime&&) noexcept=default;
Runtime& Runtime::operator=(Runtime&&) noexcept=default;
const FeatureReport& Runtime::capabilities() const noexcept{return state_->features;}
FeatureChoice chooseRayBackend(const FeatureReport& f,const RuntimeOptions& options) noexcept {
 if(!f.deviceAvailable){
  if(options.enableCpuReference)return {Backend::CpuReference,"No D3D12 device: CPU reference candidate; caller implementation required"};
  return {Backend::Unavailable,"No Direct3D 12 device"};
 }
 if(f.raytracingNative)return {Backend::NativeD3D12,"Native raytracing tier reported"};
 if(f.computeShaders&&options.enableComputeRayFallback)
  return {Backend::ComputeFallback,"Compute fallback candidate; application must implement it"};
 if(options.enableCpuReference)
  return {Backend::CpuReference,"CPU reference candidate; application must implement it"};
 return {Backend::Unavailable,"Raytracing unavailable and fallbacks disabled"};
}
FeatureChoice Runtime::chooseRayBackend() const noexcept {
 return underdog::chooseRayBackend(state_->features,state_->options);
}
DXGI_FORMAT Runtime::chooseTextureFormat(bool alpha,bool compressed) const noexcept {
 return AIOSUnderdog::chooseSampledFormat(state_->textures,alpha,compressed);
}
HRESULT Runtime::createTexture(UINT w,UINT h,DXGI_FORMAT fmt,AIOSUnderdog::TextureRef& result){
 result={};if(!state_->budget)return E_POINTER;
 return state_->budget->create2D(w,h,fmt,result);
}
bool Runtime::retireTexture(uint64_t id,uint64_t fence) noexcept {
 return state_->budget&&state_->budget->retireAfterFence(id,fence);
}
size_t Runtime::collectCompleted(uint64_t completed) noexcept {
 return state_->budget?state_->budget->collectCompleted(completed):0;
}
uint64_t Runtime::textureBytesUsed() const noexcept {
 return state_->budget?state_->budget->used():0;
}
uint64_t Runtime::textureBudgetBytes() const noexcept {
 return state_->budget?state_->budget->capacity():0;
}
}

// UnderdogRuntime v0.1: cooperative D3D12 adapter, not a driver.
#pragma once
#include <d3d12.h>
#include <cstdint>
#include <memory>
#include "AIOS_UNDERDOG_FENCED_TEXTURE_BUDGET_V02.h"
#include "AIOS_UNDERDOG_TEXTURE_FORMAT_POLICY_V01.h"
namespace underdog {
enum class Backend : uint8_t { Unavailable, NativeD3D12, ComputeFallback, CpuReference };
struct FeatureReport {
 bool deviceAvailable=false;
 bool bc1=false,bc3=false,rgba8=false;
 bool raytracingNative=false,computeShaders=false,waveOps=false;
 unsigned tiledResourcesTier=0;
};
struct RuntimeOptions {
 uint64_t textureBudgetBytes=2ull*1024*1024;
 bool enableComputeRayFallback=true;
 bool enableCpuReference=false;
};
struct FeatureChoice {
 Backend path=Backend::Unavailable;
 const char* reason="No selected feature implementation";
};
class Runtime final {
public:
 Runtime(ID3D12Device* device,RuntimeOptions options={});
 ~Runtime();
 Runtime(const Runtime&)=delete;
 Runtime& operator=(const Runtime&)=delete;
 Runtime(Runtime&&) noexcept;
 Runtime& operator=(Runtime&&) noexcept;
 const FeatureReport& capabilities() const noexcept;
 FeatureChoice chooseRayBackend() const noexcept;
 DXGI_FORMAT chooseTextureFormat(bool alpha,bool compressed) const noexcept;
 HRESULT createTexture(UINT width,UINT height,DXGI_FORMAT format,AIOSUnderdog::TextureRef& output);
 bool retireTexture(uint64_t id,uint64_t fenceValue) noexcept;
 size_t collectCompleted(uint64_t completedFenceValue) noexcept;
 uint64_t textureBytesUsed() const noexcept;
 uint64_t textureBudgetBytes() const noexcept;
private:
 struct State;
 std::unique_ptr<State> state_;
};
FeatureChoice chooseRayBackend(const FeatureReport&,const RuntimeOptions&) noexcept;
} // namespace underdog

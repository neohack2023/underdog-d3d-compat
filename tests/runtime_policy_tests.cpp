// Hardware-independent unit tests.
#include "underdog/Runtime.h"
#include <cstdlib>
#include <iostream>
using namespace underdog;
#define EXPECT(COND) do { if(!(COND)) { std::cerr << "FAIL line " << __LINE__ << ": " #COND << "\n"; return EXIT_FAILURE; } } while(0)
int main(){
 FeatureReport f{};
 RuntimeOptions o{};
 EXPECT(chooseRayBackend(f,o).path==Backend::Unavailable);
 o.enableCpuReference=true;
 EXPECT(chooseRayBackend(f,o).path==Backend::CpuReference);
 f.deviceAvailable=true;f.computeShaders=true;
 EXPECT(chooseRayBackend(f,o).path==Backend::ComputeFallback);
 f.raytracingNative=true;
 EXPECT(chooseRayBackend(f,o).path==Backend::NativeD3D12);
 f.raytracingNative=false;o.enableComputeRayFallback=false;
 EXPECT(chooseRayBackend(f,o).path==Backend::CpuReference);
 o.enableCpuReference=false;
 EXPECT(chooseRayBackend(f,o).path==Backend::Unavailable);
 AIOSUnderdog::TextureCapabilities c{};
 c.bc1=true;c.bc3=true;c.rgba8=true;
 EXPECT(AIOSUnderdog::chooseSampledFormat(c,false,true)==DXGI_FORMAT_BC1_UNORM);
 EXPECT(AIOSUnderdog::chooseSampledFormat(c,true,true)==DXGI_FORMAT_BC3_UNORM);
 EXPECT(AIOSUnderdog::chooseSampledFormat(c,true,false)==DXGI_FORMAT_R8G8B8A8_UNORM);
 EXPECT(AIOSUnderdog::encodedByteCount(128,128,4,DXGI_FORMAT_BC1_UNORM)==10880);
 EXPECT(AIOSUnderdog::encodedByteCount(128,128,4,DXGI_FORMAT_BC3_UNORM)==21760);
 EXPECT(AIOSUnderdog::encodedByteCount(128,128,4,DXGI_FORMAT_R8G8B8A8_UNORM)==87040);
 EXPECT(chooseUavBinding(UavResourceKind::TypedTexture2D,true).method==UavBindingMethod::DescriptorTable);
 EXPECT(chooseUavBinding(UavResourceKind::TypedTexture2D,true).fallbackApplied);
 EXPECT(chooseUavBinding(UavResourceKind::TypedBuffer,true).method==UavBindingMethod::DescriptorTable);
 EXPECT(chooseUavBinding(UavResourceKind::StructuredBuffer,true).method==UavBindingMethod::RootDescriptor);
 EXPECT(chooseUavBinding(UavResourceKind::RawBuffer,true).method==UavBindingMethod::RootDescriptor);
 EXPECT(chooseUavBinding(UavResourceKind::StructuredBuffer,false).method==UavBindingMethod::DescriptorTable);
 EXPECT(chooseUavBinding(UavResourceKind::Unknown,true).method==UavBindingMethod::Unsupported);
 EXPECT(!canUseRootUav(UavResourceKind::TypedTexture2D));
 EXPECT(canUseRootUav(UavResourceKind::StructuredBuffer));
 Runtime noDevice(nullptr);
 AIOSUnderdog::TextureRef ref;
 EXPECT(!noDevice.capabilities().deviceAvailable);
 EXPECT(noDevice.createTexture(128,128,DXGI_FORMAT_R8G8B8A8_UNORM,ref)==E_POINTER);
 EXPECT(noDevice.textureBytesUsed()==0);
 std::cout<<"UnderdogRuntime policy PASS\n";
 return 0;
}

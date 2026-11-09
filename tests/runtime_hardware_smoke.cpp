// Hardware smoke is NOT a CI test: requires a real D3D12 device and uses a bounded GPU fence.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdio>
#include "underdog/Runtime.h"
using Microsoft::WRL::ComPtr;
int main() {
 ComPtr<ID3D12Device> device;
 HRESULT hr=D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
 if(FAILED(hr)){std::printf("DEVICE_FAILURE=0x%08lx\n",(unsigned long)hr);return 2;}
 underdog::RuntimeOptions options;options.textureBudgetBytes=2ull*1024*1024;
 underdog::Runtime runtime(device.Get(),options);
 if(!runtime.capabilities().deviceAvailable)return 3;
 const auto path=runtime.chooseRayBackend();
 std::printf("RAY_BACKEND=%u BC1=%d BC3=%d TILED_TIER=%u\n",
  unsigned(path.path),runtime.capabilities().bc1,runtime.capabilities().bc3,
  runtime.capabilities().tiledResourcesTier);
 AIOSUnderdog::TextureRef first,second,reused;
 hr=runtime.createTexture(800,800,DXGI_FORMAT_R16_UNORM,first);
 if(FAILED(hr)){std::printf("ALLOC_FAILURE=0x%08lx\n",(unsigned long)hr);return 4;}
 if(runtime.createTexture(800,800,DXGI_FORMAT_R16_UNORM,second)!=E_OUTOFMEMORY)return 5;
 if(!runtime.retireTexture(first.id,1))return 6;
 if(runtime.collectCompleted(0)!=0)return 7;
 D3D12_COMMAND_QUEUE_DESC queueDescription={};queueDescription.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 ComPtr<ID3D12CommandQueue> queue;hr=device->CreateCommandQueue(&queueDescription,IID_PPV_ARGS(&queue));
 if(FAILED(hr))return 8;
 ComPtr<ID3D12Fence> fence;hr=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence));
 if(FAILED(hr))return 9;
 HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 if(!event)return 10;
 hr=queue->Signal(fence.Get(),1);
 if(FAILED(hr)){CloseHandle(event);return 11;}
 hr=fence->SetEventOnCompletion(1,event);
 if(FAILED(hr)||WaitForSingleObject(event,2000)!=WAIT_OBJECT_0){CloseHandle(event);return 12;}
 CloseHandle(event);
 if(runtime.collectCompleted(fence->GetCompletedValue())!=1)return 13;
 hr=runtime.createTexture(800,800,DXGI_FORMAT_R16_UNORM,reused);
 if(FAILED(hr))return 14;
 std::printf("ALLOCATED_BYTES=%llu DEVICE_REASON=0x%08lx\n",
  (unsigned long long)runtime.textureBytesUsed(),(unsigned long)device->GetDeviceRemovedReason());
 puts("UNDERDOG_RUNTIME_HARDWARE_SMOKE_PASS");
 return 0;
}

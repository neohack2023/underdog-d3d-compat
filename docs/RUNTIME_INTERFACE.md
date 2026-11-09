# UnderdogRuntime 0.1

The C++ static library offers an opt-in front-end for cooperating application renderers.

It accepts an existing ID3D12Device and maintains a COM reference. It queries actual device capabilities, exposes a candidate ray backend decision and texture format choice, and delegates bounded texture allocation and fence-aware retirement to the previously validated controller.

A candidate ComputeFallback or CpuReference decision does not implement native DXR. The application must supply the renderer. A borrowed TextureRef remains valid until retirement and collection; only collect after an actual completed GPU fence. The controller is single-thread owned and does not budget staging resources.

Use CMake to link Underdog::Runtime. Unit tests validate policy without requiring a GPU. Windows CI compiles selected D3D12 examples but does not execute GPU rendering on hosted runners.

Next gate: present an actual BVH-rendered image through DXGI and test correctness, GPU timestamps and device recovery.

# UnderdogRuntime typed UAV root binding regression V05

## What failed

The original Arena shader used RWTexture2D<float4> at UAV u0, but its root signature used a root UAV descriptor. D3D12 root UAV descriptors only support untyped buffers such as RWStructuredBuffer / RWByteAddressBuffer; typed textures need a descriptor-table binding. The earlier CreateComputePipelineState returned E_INVALIDARG 0x80070057. The corrected Arena changed several parameters at once, so the prior successful run alone could not isolate the reason.

Microsoft reference: https://learn.microsoft.com/en-us/windows/desktop/direct3d12/using-descriptors-directly-in-the-root-signature

## Controlled Host 02 experiment

Intel HD Graphics 520 (8086:1916), driver 22.20.16.4836, native 1-pixel CS 5.1 test.

| Shader declaration | Root UAV descriptor | UAV descriptor table |
|---|---|---|
| RWStructuredBuffer<float4> | PSO OK, GPU pixel OK | PSO OK, GPU pixel OK |
| RWTexture2D<float4> | PSO E_INVALIDARG, no dispatch | PSO OK, GPU pixel OK |

Each matrix was run 3 times for R32G32B32A32_FLOAT and 3 times for the original R8G8B8A8_UNORM format. All 6 matrices passed. All 18 valid GPU pixel readbacks passed. The RGBA8 texture read back 64,128,191,255. No device removal was reported.

D3D12GetDebugInterface returned 0x887A002D; SDK debug layers were not installed or enabled. No driver state changed.

## Implemented Underdog policy

The header include/underdog/UavBindingPolicy.h supplies chooseUavBinding(kind, preferRoot). It selects root UAV for raw/structured untyped buffers; descriptor table for typed texture and typed buffer; unknown input is rejected. The caller is responsible for correctly classifying the shader UAV declaration, and for creating the selected descriptor/root signature.

The owned diagnostic examples/diagnostics/typed_uav_bindings_v01.cpp deliberately bypasses that recommendation for the invalid typed-root negative control. Hosted Windows CI compiles the executable, but does not run the GPU test.

Build target: underdog_typed_uav_ab.
Run locally on HD 520:
- build/Release/underdog_typed_uav_ab.exe
- build/Release/underdog_typed_uav_ab.exe --rgba8

Source-free metadata: driver_intelligence/intel_hd520_uav_binding_v05.json. Raw individual GPU witness logs live in the independent AIOS evidence store.

## Boundaries

The minimal mismatch reproduces the historical Arena HRESULT. It does not prove the entire original Arena shader had no other defects, that Intel's DDI is faulty, or that Fortnite's driver reset shares this mechanism. No protected game process was instrumented and no GPU driver modified.

Next practical test: offer a typed texture descriptor-table backend in our owned Arena engine, compare output parity, 60-frame pacing and GPU timestamps against the working structured-buffer path.

# Underdog D3D Compatibility

Experimental hybrid Direct3D graphics compatibility library for legacy GPUs.

**Status: self-owned application prototypes. Not a Windows driver, complete Direct3D wrapper, DXR driver, or Fortnite patch.**

The approach is hardware-first: retain real GPU acceleration wherever supported, substitute selected missing techniques with compatible compute shaders, and use explicit CPU reference/last-resort software rendering only where justified.

## Verified engineering slices on Intel HD 520

- Fenced texture allocation policy: capacity guard, deferred release after confirmed GPU fence.
- D3D12 compute raytracing: small OBJ meshes, CPU-built BVH, ray/triangle intersections.
- Texture streaming: 2D shader input, BC1/BC3 compressed mip copies, shader sampling.
- 12/800/3200 triangle GPU timestamp comparisons and independent D3D12 Win32 Present application.
- CPU reference validation and bounded GPU-device fault diagnostics.

These are fixtures, not a universal API implementation. Fortnite remains a downstream crash witness; our code has never been injected into it.

## Architecture

    Cooperating application
       -> Underdog feature probe / resource policy
          -> Native hardware API
          -> GPU compute fallback (limited, verified features)
          -> CPU reference/software fallback (experimental)
       -> Timestamped correctness and performance gates

## Repository structure

- include: reusable C++ headers
- examples: original self-contained Windows D3D12 experiments
- fixtures: tiny source-owned OBJ meshes
- tools: bounded build helper
- evidence: SHA-256 provenance pointers, no binary or database archive
- docs: architecture and limits

## Build

Use Visual Studio 2022 C++ Build Tools and Windows SDK, with a compatible physical D3D12 GPU.

Run tools/build_examples.cmd in a Visual Studio Developer Command Prompt.
The build helper builds selected standalone examples; a unified cross-GPU SDK is future work.

## Key limitations

- No third-party DX12 API interception or feature-tier misrepresentation.
- No safe way to claim arbitrary games can use our fallbacks.
- Compute ray intersections are not full DXR API emulation.
- No general shader translation or complete software rasterizer.
- No anti-cheat bypasses, injected DLLs, system driver replacement or modified Fortnite files.
- No proof of improvement to Fortnite performance; its Intel display driver reset remains unresolved.

## Open-source precedents

- Microsoft DirectX-Graphics-Samples / D3D12RaytracingFallback
- Mesa Gallium and llvmpipe
- DXVK and vkd3d-proton

These are research references; no third-party code is vendored here.

## Ownership and licensing

All staged files originated in the user's AIOS graphics lab and have SHA-256 provenance.
The source in this repository is now provided under the MIT License (see LICENSE). No third-party vendored code is included; external projects remain research references.

## Next steps

- CI/build portability, validate curated examples outside the original workspace
- Combine OBJ/BVH compute with windowed D3D12 Present
- Develop bounded residency and mapped tile streaming
- Formalize capability queries and well-defined fallback interface
- Test with additional self-owned applications and older GPUs

## UnderdogRuntime v0.1

The first C++ static library lives in src/Runtime.cpp and include/underdog/Runtime.h. It composes native D3D12 capability queries, ray-backend selection, compressed-texture policy, and fence-aware texture budgeting. See docs/RUNTIME_INTERFACE.md.

Build and test with Visual Studio 2022 and CMake:

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

The hardware-dependent integration smoke test is compiled but not run on GitHub-hosted CI. It can be run explicitly as build/Release/underdog_runtime_hardware_smoke.exe on a physical GPU.

GitHub Actions verifies Windows MSVC compilation and CPU-safe policies, not real gaming performance or Fortnite compatibility.
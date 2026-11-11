# Adaptive ray tracing for low-end GPUs: V01

**Status:** Host 02 proof of concept, branch-only. **Not integrated into a game or complete DXR emulation.**

## Objective

Provide *optional* compute-based ray effects for older GPUs without degrading a game's primary rasterization path. A graphics feature scheduler must treat native raytracing, bounded compute fallback, CPU/reference rendering, and 'effect unavailable' as distinct decisions. A feasible software implementation is not necessarily fast enough for gameplay.

The initial target is **Intel HD Graphics 520 (8086:1916)** using Direct3D 12 compute shader model 5.1 and a source-owned 12-triangle BVH test scene. Microsoft WARP is an independent software reference, **not** an automatic real-time fallback.

Research foundations:
- [Microsoft DXR spec](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html): native DXR and standalone compute emulation are distinct; historical fallback layer was not maintained as a general performance solution.
- [Microsoft DXR fallback precedent](https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Libraries/D3D12RaytracingFallback/readme.md): useful architecture, but not adopted or vendored here.
- [Microsoft GPU timing](https://learn.microsoft.com/en-us/windows/win32/direct3d12/timing): GPU queue timestamp deltas, not CPU-side fence wait time, provide a dispatch-duration measurement.
- [AMD FidelityFX checkerboard denoising](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/main/Kits/FidelityFX/docs/techniques/denoising.md): established approach to reducing ray counts when a compatible reconstruction and scene-motion pipeline exists.

## What was implemented

1. **Control-plane policy:** [`RayBudgetController.h`](../include/underdog/RayBudgetController.h) independently chooses Off, Quarter, Half or Full. It starts disabled unless explicitly opted in. It requires valid measured frame, baseline and GPU-ray duration inputs, rejects invalid timestamps, and disables itself on device loss, invalid measurements, insufficient main-rendering headroom, or ray-count overrun.
2. **Downshift and recover:** It reacts quickly to repeated overloads, recovers more slowly with hysteresis/cooldown, and requires external validation before reusing temporal history.
3. **Hard ray-count ceiling:** The planner calculates an upper bound on active ray pixels; it does **not** predict or guarantee completion under a particular millisecond budget.
4. **D3D12 GPU timestamp fixture:** [`ray_budget_bench_v01.cpp`](../examples/compute_ray/ray_budget_bench_v01.cpp) exercises the existing owned CPU-built BVH and ray/triangle compute shader at 32, 64, and 128 square work sizes. Optional rate divisor 2 or 4 skips traversal for a subset of pixels at 128 square and writes a visibly incomplete placeholder, expressly *not* denoised output.
5. **Hardware-free regression test:** [`ray_budget_policy_tests.cpp`](../tests/ray_budget_policy_tests.cpp) exercises warmup, disabled-by-default, up/down transitions, cooldown, scene-history invalidation, ray-count/dimension guards, lost timestamp, baseline crowding, device loss, NaN and infinity. MSVC on Host 02: PASS, failures=0.

## Measured Host 02 dispatch cost

Five independent executions per configuration. Times are **GPU timestamp intervals immediately around the compute Dispatch**. They exclude BVH construction, shader compilation, scene upload, GPU copying, present, game simulation, frame pacing, and CPU overhead. Shared GPU conditions were not controlled and no statistical performance claim is implied.

### Variable output sizes (one ray per output pixel)

| Render size | Rays | Intel HD 520 median | WARP median |
|---|---:|---:|---:|
| 32 × 32 | 1,024 | 0.092 ms | 8.603 ms |
| 64 × 64 | 4,096 | 0.113 ms | 9.999 ms |
| 128 × 128 | 16,384 | 0.158 ms | 14.370 ms |

Every group had five runs, no D3D12 device removals and internally stable image checksums.

### Sparse rays at unchanged 128 × 128 output size

| Traversal density | Active rays | Intel HD 520 median | WARP median |
|---|---:|---:|---:|
| Full | 16,384 | 0.164 ms | 14.257 ms |
| Half | 8,192 | 0.154 ms | 14.013 ms |
| Quarter | 4,096 | 0.141 ms | 13.838 ms |

Reducing active rays to a quarter reduced median HD 520 dispatch time by only about **14%** in this fixture, not 75%. The skipped pixels still execute a shader branch and write a placeholder. Active pixel outputs matched the equivalent full-trace output byte-for-byte for both tested adapters, but the complete sparse image is *not visually equivalent*.

An observed HD 520 half-rate run reached **0.331 ms** despite a roughly 0.154 ms median. That variance reinforces the need for hysteresis and reserved frame-time headroom. WARP may be valuable as a correctness reference, but software execution was much slower here.

## Suggested hardware-conservative scheduling policy

`RayBudgetConfig` defaults are *design parameters to tune in a real game*, not measured safe limits:

| Setting | Example default | Role |
|---|---:|---|
| Enabled | **false** | Only opt-in integration |
| Frame envelope | 33.333 ms | 30 FPS example, game-configurable |
| Marginal ray work budget | 0.75 ms | Upper advisory threshold, not a hardware guarantee |
| Reserved baseline headroom | 2.0 ms | Never prioritize optional ray work over the base frame |
| Maximum active rays | 4,096 | Prevent accidental full-frame high-resolution trace |
| Overload persistence | 2 samples | Rapid downgrade |
| Recovery persistence | 30 samples | Slow promotion |
| Cooldown | 12 samples | Avoid rapid quality oscillation |

Callers must collect real application frame times and GPU timestamp queries (with a completed fence/readback at a *safe* point), and track base-render timings without optional ray effects. The controller does not collect telemetry, dispatch rays, own a swapchain, or synchronize command queues; it only emits a constrained plan. `RayPlan.scheduledRays` is a bound, not an instruction to skip final-frame reconstruction.

**Important:** The current benchmark does not yet connect `RayBudgetController` to its shader. The full/half/quarter parameters are entered explicitly, and scheduling state-machine tests are separate. Never describe this as adaptive real-time game rendering yet.

## Engineering constraints and next gates

1. **Keep game rasterization authoritative.** Optional ray shadows/AO should be composited only after validity checks. When Off, the original game frame must remain intact.
2. **Build only narrow effects first:** contact shadows or low-resolution ray AO. Do not pursue full real-time multi-bounce GI or a general DXR shader table without engine, memory and frame-time feasibility.
3. **Integrate the controller with one opt-in original Arena effect** and separate baseline frame-time capture. Log GPU timestamps for BVH build (if dynamic), rays, temporal reconstruction, and present individually.
4. **Reconstruct sparse samples** via an explicit half/quarter-resolution or checkerboard pipeline, then test depth/normal/motion validity, disocclusion, camera changes, frame history, and golden-image differences.
5. **Quality acceptance:** A hardware test must show raster-only baseline versus raster-plus-rays on the *same scene*, frame-time p50/p95, visual correctness and graceful disable behavior. Single isolated GPU dispatch times cannot establish gameplay performance.
6. **Unsupported hardware:** Expose 'not feasible' cleanly; CPU software path only when its measured marginal cost and required fidelity justify it. WARP is not a transparent performance optimization.
7. **No system driver modification, injected game code, or protected-process operation.** Work is original opt-in application instrumentation. No third-party code vendored.

## Local evidence ownership

Host 02 workspace: `07_EXECUTION/RAY_SCHEDULER_V01/`. Test results: `gpu_ray_matrix_host02_v01.json`, `sparse_ray_gpu_host02_v01.json`. These are **local execution evidence**, not new AIOS canon or an assertion of Drive scope registration. A bounded evidence receipt should accompany further promotion.

## Running locally

Compile the CPU policy and GPU program with CMake on Windows (VS 2022 Build Tools and Windows SDK):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target underdog_ray_budget_policy_tests underdog_ray_budget_bench
ctest --test-dir build -C Release --output-on-failure
```

In an explicit hardware test directory run `underdog_ray_budget_bench.exe 64` (Intel by current default adapter selection), or `underdog_ray_budget_bench.exe 128 warp 4`. Args: output dimension 32/64/128, optional `warp`, optional trace divisor 1/2/4. The fixture writes `ray_budget_bench.rgba` in the process working directory and overwrites that file on repeated runs; use a dedicated directory. Hosted CI only builds the GPU program and runs CPU-safe policy tests.

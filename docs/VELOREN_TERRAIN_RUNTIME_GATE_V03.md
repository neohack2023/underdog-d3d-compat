# Veloren terrain void: Windows source-build and live-trace gate V03

**Status as of 2026-11-10, Host 02:** Native Veloren/D3D12 gameplay and movement verified earlier; source-level terrain lifecycle control flow mapped, but the user-reported missing-block void is **open and not independently reproduced**. No instrumented consumer executable yet. Underdog has not demonstrated an in-game performance benefit.

## Source, toolchain and boundaries

- Official Veloren weekly binary (revision `da6626e6`) runs on Intel HD 520 with wgpu/Dx12. Its original executable SHA-256 and its original user settings SHA-256 are unchanged.
- Pinned **full** upstream commit: `da6626e6ed3c09619ed3e46423e6609dffadad22`. The sparse partial GitLab checkout initially occupied about 17.4 MB, excluding bulk assets, far below the rejected 355.7 MB full archive.
- Rust nightly `2026-06-13` installed. Windows MSVC 2022 Build Tools, CMake, Ninja available; Cargo metadata parsed 25 workspace packages.
- Only the checked-out `voxygen/src/scene/terrain/mod.rs` is modified. The original game files remain untouched.
- The opt-in terrain patch `UNDERDOG_TERRAIN_TRACE=1` is GPL consumer source, kept under Host02 governed execution storage, **not copied into the MIT Underdog repository**. Patch application/reversal and Rust syntax parsing passed. This is not a full-game build validation.
- Separate source-proven blocker: pinned `voxygen/src/render/renderer/mod.rs` has `#[cfg(any())]` around `WGPU_TRACE_DIR` and unconditionally specifies `wgpu::Trace::Off`. Native wgpu API trace output was therefore not expected. That limitation does not disable our custom terrain lifecycle logger.

## Bounded compilation, not a completed instrumented runtime

`07_EXECUTION/VELOREN_TERRAIN_BUILD_EVAL_V01` includes a locked `cargo build -p veloren-voxygen --bin veloren-voxygen --no-default-features --features singleplayer -j 2` attempt with the specified nightly, bounded elapsed runtime, storage limits, MSVC and CMake/Ninja available.

The V02 bounded attempt started **95 Rust dependency compilations** and reported no compiler diagnostic errors in the captured output. The guard stopped the incomplete build after approximately 420 seconds. Its retained `target` cache is **463,887,427 bytes (442.4 MiB)**; free disk space remained approximately 40.5 GiB. The presence of no errors in this incomplete pass does **not** establish that the patched terrain module type-checks, that Veloren compiles, or that gameplay is reachable.

The test command intentionally disables many optional features to reduce compile cost. Before comparing its visual behavior to the weekly executable, confirm the necessary UI, world, and plugin features or rebuild with a matched feature set.

## Why the user-observed hole is still unclassified

The original native binary reached the saved local character and rendered a 3D world. Two bounded movement sequences and full-resolution screenshots did not capture the reported hole; no chunk coordinate or matching bad-frame log exists. The nine source facts isolate candidate boundaries:

1. Neighbor-availability gate for 3x3 chunks before meshing queue admission.
2. Scheduled meshing workers, missing neighbor samples and bounded worker concurrency.
3. Older worker responses admitted relative to a newer pending tick.
4. Atlas allocation reset/recreation and terrain-model insertion.
5. Frustum draw gating and missing `opaque_model`.

No single candidate is established as a root cause. In particular, a CPU-side `MODEL_STORED` event does not prove that the GPU drew or presented those vertices.

## What is already durable

- Native controls and screenshots: `07_EXECUTION/VELOREN_TERRAIN_NATIVE_WGPU_TRACE_V01`.
- Exact source checkout, GPL patch, compiler log, strict build guard and reusable dependency cache: `07_EXECUTION/VELOREN_TERRAIN_BUILD_EVAL_V01` and `07_EXECUTION/VELOREN_TERRAIN_TRACE_V01`.
- Verified negative control: `06_EVIDENCE/VELOREN_WGPU_TRACE_DISABLED_SOURCE_V01.json`.
- Build receipt: `06_EVIDENCE/VELOREN_TERRAIN_BOUNDED_BUILD_V03.json`.
- Dataset: `03_DATASET/VELOREN_TERRAIN_RUNTIME_GATE_V03.json`.
- SQLite: `02_DATABASE/UNDERDOG_D3D_COMPAT_V01.sqlite` (bounded source-build gate run).
- Knowledge ledger: `04_KNOWLEDGE/VELOREN_TERRAIN_RUNTIME_GATE_V03.md` and the experience JSONL.
- Scoped continuity: `08_HANDOFF/VELOREN_TERRAIN_TRACE_V03.json`.
- Cleanup: eight thumbnail JPEG derivatives removed, 404,118 bytes reclaimed. Full-resolution PNG sources, logs, compiler cache, executable, game settings and world fixtures retained.

## Next decisive action

Continue the exact pinned source build using the retained target and Cargo registry caches, or move only the GPL-licensed consumer build and patch into a separately governed Windows CI build job to avoid lengthy compilation on the HD 520 laptop. Validate source compilation and instrumented launch **before** claiming terrain events. Then enable `UNDERDOG_TERRAIN_TRACE=1`, enter the copied world on native Dx12, capture the first missing-block frame with coordinates and time, and parse its lifecycle events using `tools/diagnostics/analyze_veloren_terrain_trace_v01.py`. Compare the relevant chunk's arrival, neighbor gate, worker response, atlas/model storage, draw eligibility and visible frame. Preserve a native controls run for every proposed fix.

Official build instructions: https://book.veloren.net/contributors/compiling.html. Exact game source: https://gitlab.com/veloren/veloren/-/tree/da6626e6.

**Do not** classify user terrain loss as an Intel driver bug, a confirmed Veloren meshing bug, or an Underdog repair without a correlated hardware/runtime witness.

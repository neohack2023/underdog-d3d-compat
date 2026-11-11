# NativeSX2 / Underdog: GS backend boundary and 4 MiB VRAM transport V01

**Status:** Local Host 02 hardware PASS; synthetic transport only, no game boot or PS2 GS rendering.  
**Scope:** External NativeSX2 research candidate. No AIOS Drive-scope promotion.  
**Repository license isolation:** Underdog remains MIT. The PS2Recomp GPL-3.0 headers were used only in an external, local compilation fixture and are not copied into this repository.

## Exact source boundary

- NativeSX2: https://github.com/contatonewportgame-code/nativesx2 (external launcher, overrides and integration patches; its repository does not contain the full runnable PS2Recomp renderer).
- Upstream GS interface: https://github.com/ran-j/PS2Recomp/blob/e5636981a1c9de368f6b71e819c3dc188dfedbf4/ps2xRuntime/include/runtime/gs/gs_backend.h
- Upstream GS implementation boundary: https://github.com/ran-j/PS2Recomp/blob/e5636981a1c9de368f6b71e819c3dc188dfedbf4/ps2xRuntime/src/lib/gs/gs_frontend.cpp
- Pinned upstream commit: `e5636981a1c9de368f6b71e819c3dc188dfedbf4`.
- `GS::setRasterBackend(std::unique_ptr<GSRasterBackend>)` transfers backend state by flushing/synchronizing, snapshotting external GS VRAM, and initializing the replacement backend.
- `GSRasterBackend` has 16 pure virtual methods, including `Submit`, `TextureFlush`, `Sync`, `Present`, `SnapshotVram` and host transfer methods.
- Default upstream `GS` starts with `GSCpuBackend`. Do not equate this with an already operational OpenGL or Direct3D replacement.
- Recent upstream PR #431 adds a 43-line `GSOpenGLBackend` header, explicitly noting it **does not rasterize GS primitives on the GPU**: https://github.com/ran-j/PS2Recomp/pull/431

## Host 02 executed tests (2026-11-11)

Platform: `DaltonsLaptop574`, Intel HD Graphics 520 (8086:1916), Microsoft WARP (1414:008C).

1. **C++ GS interface compile/dispatch gate:** A locally staged `GSRasterBackend` synthetic implementation compiled with MSVC 2022 BuildTools and executed; 16 overridden methods, test `PASS`. The upstream GPL headers remain under `07_EXECUTION/NATIVESX2_GS_CONTRACT_V01/upstream/` and outside the MIT repository.
2. **D3D12 4 MiB transport:** `tests/gs_vram_gpu_roundtrip_v01.cpp` allocates an upload heap, default GPU heap and readback heap, transfers a deterministic 4,194,304-byte pattern, synchronizes a fence, and compares all bytes.
3. **Independent reference:** Intel HD 520 and software WARP both PASS, zero bytes different, identical FNV64 checksum `5eda06df082504c5`. Six additional repeats (three per adapter) also passed with zero differences.
4. Both devices reported `GetDeviceRemovedReason() = S_OK`. Recorded 0-ms waits use coarse wall-clock timing and **are not throughput or frame-time benchmarks**.

Execution-only receipt: `C:\AIOS\04_WORKSPACES\D3D_COMPAT_UNDERDOG_01\06_EVIDENCE\NATIVESX2_GS_CONTRACT_HOST02_V01.json`.

## Hardware implications

Intel lists HD 4000 as DirectX 11 / OpenGL 4.0 and HD 520 as DirectX 12 / OpenGL 4.6. A shared NativeSX2 optimization cannot mandate D3D12 on the Intel HD 4000. A D3D11 compatibility lane or appropriate OpenGL path is required for that hardware. See https://www.intel.com/content/www/us/en/support/articles/000005524/graphics.html

## Confidence and constraints

**Verified:** compile-level GS contract and independent D3D12 GPU buffer transport.

**Not verified:** an actual GPU-backed GS rasterizer, texture alias semantics, texture feedback, texture-clamp/PSM conversion, GPU shader performance, OpenGL-vs-D3D parity, Intel HD 4000 execution, God of War boot/playability, or the Reddit author's performance numbers.

**No driver replacement or third-party game modification** occurred. No ISO or proprietary game asset was used.

## Next acceptance gate

- Implement a **minimal, opt-in, source-separated GS backend adapter** behind `GSRasterBackend`; preserve `GSCpuBackend` as a golden oracle.
- Start with 4 MiB VRAM initialization/snapshot/restore, image upload, `TEXFLUSH`, host transfer, and present, then a textured sprite.
- Validate color values, PSM layouts, framebuffer feedback, ordering and sync against CPU reference on the same sequence; compare full captured images, not just success HRESULTs.
- Run hardware matrix: HD 520 D3D12; HD 4000 requires D3D11/OpenGL path and a real HD 4000 machine.
- Keep GPU test **compile-only** in generic hosted CI. Run it only by explicit hardware/WARP selection. Do not promote game compatibility or performance claims until a playable-game witness exists.

Build locally on Windows with CMake using target `underdog_gs_vram_roundtrip`. Run `underdog_gs_vram_roundtrip.exe` for Intel HD 520, or `underdog_gs_vram_roundtrip.exe warp` for Microsoft WARP.

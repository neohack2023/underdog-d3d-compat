# Intel HD 520 DirectX 12 feature probe V01

Status: **hardware capability inquiry complete; bounded SM5.1 compute execution complete; SM6.1 WaveOps 32-lane bounded execution verified.** Tested November 9, 2026 on Windows 10 18362, Intel HD 520 (8086:1916), driver 22.20.16.4836. No driver changes, DLL interception, WARP fallback, or game patches.

## Driver-reported API capabilities

| Probe | Observed result | Interpretation |
| --- | --- | --- |
| D3D12CreateDevice | S_OK on Intel hardware | Native device initialized |
| D3D_FEATURE_LEVEL | 12_1 | Reported feature level |
| D3D12_FEATURE_SHADER_MODEL | 6.1; SM6.6 request E_INVALIDARG | **Reported**, not SM6.1 execution |
| D3D12_OPTIONS ResourceBindingTier | 3 | Query successful |
| D3D12_OPTIONS ResourceHeapTier | 2 | Query successful |
| D3D12_OPTIONS TiledResourcesTier | 3 | Query successful |
| RootSignature | 1.1 | Query successful |
| ConservativeRasterizationTier | 3 | Query successful, no raster test |
| Additional typed UAV formats | true | Query successful, no per-format execution |
| Rasterizer ordered views | true | Query successful, no ordering test |
| OPTIONS1 WaveOps | true | Query successful, separate bounded WaveGetLaneCount/WaveActiveSum execution PASS |
| OPTIONS5 RaytracingTier | 0 | Native DXR unavailable |
| FORMAT_SUPPORT | RGBA8, BGRA8, RGBA16F, BC1, BC3, BC7, D32F, RGBA8_UINT all S_OK | Raw Support1/Support2 bits retained. **Not proof of every format operation** |

## Separate D3D12 execution fixture

Reused the existing examples/benchmark/alu_stream_v01.cpp, compiled via tools/diagnostics/build_alu_stream_reuse.cmd. It executed 36 Shader Model 5.1 compute frames and verified 288 output samples against CPU calculations, with no device removal. Three CPU-verified ALU tiers (4, 32, 128 rounds) measured median GPU ALU times of 0.0674, 0.1225, 0.3437 ms respectively. Those timings describe the fixture only, not game performance or SM6.1.

## Reproduction

- Build the inspector using tools/diagnostics/build_dx12_feature_probe_v01.cmd. Set UNDERDOG_FEATURE_PROBE_OUT to a chosen artifact directory; if unset, output goes to a temporary directory. Run the executable there and retain its stdout JSON.
- To execute the existing compute fixture, build via tools/diagnostics/build_alu_stream_reuse.cmd, then run alu_stream_v01.exe. Require its 36-frame/288-CPU-sample PASS and 0 device-removed code before admitting the result.
- In the registered Host 02 workspace, tools/diagnostics/ingest_dx12_feature_probe_v01.py validates raw results, updates the 03_DATASET matrix and 02_DATABASE SQLite with additive, idempotent tables, and writes evidence + a knowledge ledger. This ingestion helper expects the governed AIOS workspace layout; do not use it against arbitrary databases.
- Raw hardware captures, binary hashes and screenshots remain in Host 02 execution/evidence storage. The repository carries reusable tool source and this verified summary, not duplicate binaries or proprietary game assets.

## Real-game boundary

The **unchanged** Veloren official Windows weekly (revision da6626e6, 2026-10-27) selected Intel HD 520 DX12 when WGPU_BACKEND=dx12 and entered a rendered single-player world with character and HUD. This **does not** prove Underdog improved Veloren. Native/Underdog A/B frame timings and the safe wgpu adapter have not been built.

## Next evidence gates

1. Compile and **dispatch** SM6.1 wave compute on Intel hardware, then read back known results; distinguish compiler acceptance from device execution.
2. Decode per-format support masks; exercise BC1/BC3/BC7 sampling, UAV load/write, and RGBA16F target/resolve where supported.
3. Run binding-tier and placed-heap fixtures with fence/readback checks, capped allocations.
4. Instrument the Veloren wgpu device/resource/pipeline/presentation boundary behind an opt-in audit switch without substituting an external ID3D12Device or bypassing wgpu lifetime ownership.
5. Compare 120+ consecutive real-game presentations and memory behavior on the same build, driver, resolution, and scene. Only promote an Underdog compatibility improvement when repeatably observed.

## Storage and provenance policy

Keep source code, small machine-readable receipts, database facts and evidence hashes. Build objects and duplicate scratch may be removed after reproduction is established. Never treat cleanup as license to discard immutable game baselines, original source archives, failed-run evidence, or unresolved project material. GitHub owns live repository implementation; Host 02 owns runtime evidence; Drive is the authority for governed AIOS durable records, not Notion.

## SM6.1 WaveOps actual hardware execution V01

DXC successfully compiled a cs_6_1 shader, CreateComputePipelineState returned S_OK, and a 32-thread dispatch using WaveGetLaneCount + WaveActiveSum produced 32/32 valid uint2 GPU-readback pairs on Intel HD 520. Both values agreed; the actual wave width was 16 lanes. DeviceRemovedReason was 0x00000000. This is a bounded witness, not exhaustive Shader Model 6.1 conformance or a game compatibility improvement.

Rebuild/run: tools/diagnostics/build_run_wave_sm61_v01.cmd, source tools/diagnostics/wave_sm61_v01.hlsl and tools/diagnostics/wave_sm61_dispatch_v01.cpp. Ingest: tools/diagnostics/ingest_wave_sm61_v01.py. Host 02 evidence DX12_HD520_SM61_WAVE_V01.json and raw execution log.

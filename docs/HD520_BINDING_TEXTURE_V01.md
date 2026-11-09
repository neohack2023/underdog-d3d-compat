# HD 520 Tier 3 Resource Binding and Texture Fixtures V01

Tested: 2026-11-09. Host02 Intel HD Graphics 520 8086:1916, driver 22.20.16.4836, Windows 10 build 18362. Original game untouched, WARP not used.

## Reused owned fixtures, extended only where necessary

- `examples/texture_stream/bc_mip_v01.cpp`: original BC1, BC3, RGBA8 upload + exact-copy readback fixture.
- `examples/texture_stream/bc_mip_v02.cpp`: bounded extension to BC7 and RGBA16F. All five formats: **160/160 mip/subresource checks** over 40 GPU frames. This is byte transfer, not filtering, blending, or format conversion.
- `examples/texture_stream/bcn_sample_v02.cpp`: original BC1/BC3 texture shader Load/readback.
- `examples/texture_stream/bcn_sample_v03.cpp`: BC7 mode-6 encoded block, 4 GPU-read mip values, expected `0xFE000000`.
- `examples/texture_stream/bcn_sample_v04.cpp`: RGBA16F exact 0/1 binary16 values; shader Load to RGBA8 output. Four formats total **16/16 mip samples**. Not an HDR rendering precision test.
- `examples/diagnostics/typed_uav_bindings_v01.cpp`: reused unchanged. Valid structured root/table + typed-table shader readback for both R32G32B32A32_FLOAT and RGBA8_UNORM. Invalid typed root UAV PSO rejected `E_INVALIDARG`; never dispatched.
- `tools/diagnostics/tier3_sparse_uav_v01.cpp` and HLSL: opt-in standalone Tier3-specific UAV array of 65 descriptors, only slot 64 initialized, runtime shader selection of slot 64. Readback `0x13BEEF42`, no device removal. Tier2 limits 64 UAVs across stages; Tier3 supports full heap and sparse uninitialized unused descriptors per Microsoft spec. This validates a **single Tier3-specific configuration**, not the entire Tier3 conformance envelope.

## Results and negative controls

| Test | Result |
|---|---|
| BC1/BC3/BC7/RGBA16F shader sample mips | 16/16 |
| BC1/BC3/BC7/RGBA8/RGBA16F GPU transfer mips | 160/160 |
| Structured UAV root/table and typed texture table | Pass in two formats |
| Typed texture illegal root UAV | Rejected as expected, E_INVALIDARG |
| 65-UAV descriptor table with sparse slot64 | Readback exact |
| Native driver device removal | None observed |

Inputs/outputs are the native GPU on Host02, not software WARP. Raw logs, exact hashes, and machine-readable receipts are stored under the local Underdog workspace in 06_EVIDENCE and 07_EXECUTION. Hardware database and experience ledger are the authoritative execution record; this repository contains reproducible source/recipes.

## Reproduction and future gates

- Build reusable texture/UAV fixtures: `tools/diagnostics/build_binding_texture_v01.cmd`. It writes only to a governed execution output folder.
- Build and execute Tier3 sparse UAV: `tools/diagnostics/build_run_tier3_sparse_v01.cmd`.
- Ingest only validated, immutable log results: `tools/diagnostics/ingest_binding_texture_v01.py`.
- Further verification: filter interpolation, RGBA16F blending/precision, longer descriptor lifetime and nonuniform indexing, residency under bounded load.

A successful hardware fixture does not establish Underdog improves Veloren. Native-vs-audited A/B remains a separate acceptance gate.

Microsoft reference: https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support

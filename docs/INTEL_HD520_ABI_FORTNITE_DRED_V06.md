# Intel HD 520 D3D12 adapter ABI and Fortnite device-hang evidence, V06

**Host:** WINDOWS_EXECUTION_HOST_02, Intel HD Graphics 520 PCI 8086:1916, driver 22.20.16.4836. **Mode:** read-only driver interface and existing crash-log correlation. No injected runtime, anti-cheat bypass, modified driver, or new Fortnite launch.

## Resolved: eight D3D12 adapter callback slots

Two preserved Microsoft Windows SDK releases, `10.0.10240.0` and `10.0.14393.0`, contain identical `D3D12DDI_ADAPTERFUNCS` member declarations in `d3d12umddi.h`. Both source files were retrieved from a commit-pinned public SDK mirror, verified against their **Git blob SHA-1**, parsed independently and compared in order. On x64 the eight function pointers occupy 64 bytes total.

| x64 table offset | SDK member name | Evidence status |
| --- | --- | --- |
| `+0x00` | `pfnCalcPrivateDeviceSize` | ABI layout verified, Intel RVA mapped |
| `+0x08` | `pfnCreateDevice` | ABI verified and 5/5 installed variants have prior high-confidence static signature |
| `+0x10` | `pfnCloseAdapter` | ABI layout verified, Intel RVA mapped |
| `+0x18` | `pfnGetSupportedVersions` | ABI layout verified, Intel RVA mapped |
| `+0x20` | `pfnGetCaps` | ABI layout verified, Intel RVA mapped |
| `+0x28` | `pfnGetOptionalDDITables` | ABI layout verified, Intel RVA mapped |
| `+0x30` | `pfnFillDDITable` | ABI verified and Intel V04 wrapper/implementation traced statically |
| `+0x38` | `pfnDestroyDevice` | ABI layout verified, Intel RVA mapped |

The earlier `intel_hd520_openadapter_v02.json` retains its **48 immutable pointer-RVA facts** from five installed variant tables plus one 21.20.16.4534 older-driver table. V05 adds only the eight semantic definitions; on Host02, SQLite view `driver_adapter_slots_semantic_v05` joins them to the 48 prior observations.

The **CreateDevice** candidate was previously traced from slot `+0x08` across five variants into implementations that read `D3D12DDIARG_CREATEDEVICE_0003`'s `Interface`, `Version`, `p12UMCallbacks`, and `Flags` fields in the expected positions. It remains a high-confidence **static** identification, not an observed runtime callback.

The **FillDDITable** callback at `+0x30` leads through an Intel versioned device-core table. Prior V04 Ghidra evidence separately maps `CreatePipelineState` at device table `+0x110` and `CreateHeapAndResource` at `+0x1E0` in four observed table constructors. Exact negotiated DDI version names and live process invocation remain unproven. Do not mistake adapter table offsets for device-core table offsets.

### Pinned source references

- SDK 10.0.10240.0: https://github.com/tpn/winsdk-10/blob/9b69fd26ac0c7d0b83d378dba01080e93349c2ed/Include/10.0.10240.0/um/d3d12umddi.h
- SDK 10.0.14393.0: https://github.com/tpn/winsdk-10/blob/9b69fd26ac0c7d0b83d378dba01080e93349c2ed/Include/10.0.14393.0/um/d3d12umddi.h
- Microsoft D3D12DDIARG_OPENADAPTER declaration: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d12umddi/ns-d3d12umddi-d3d12ddiarg_openadapter

No SDK header content is vendored into this repository. Re-run `python tools/driver_intelligence/validate_intel_adapter_abi_v05.py --verify-upstream` on a network-connected Windows host to reproduce the two historical header version checks. The default command validates all pointer-RVA joins without network access or GPU hardware.

## Fortnite: confirmed device removal, driver culpability still unknown

Host 02's original Windows Error Reporting and Unreal logs provide **three correlated crashes**:

| Local time (EST) | Windows/Unreal evidence | API operation also observed failing |
| --- | --- | --- |
| Nov 8, 2026, 21:21 | `D3DDRED2` with `VEN_8086`, driver `22.20.16.4836`, `0x887A0006 DXGI_ERROR_DEVICE_HUNG` | `CreatePlacedResource` returned device removed |
| Nov 8, 2026, 21:36 | Second `D3DDRED2` with identical vendor, driver and device-hung code | `CreatePlacedResource` then three PSO creation failures (`0x887A0005`, already removed) |
| Nov 9, 2026, 07:40 | Game log `DXGI_ERROR_DRIVER_INTERNAL_ERROR`; corresponding Display/igfx 4101 recovery and WER LiveKernelEvent 141 | Resource `Map` failure and later PSO creation failures |

In the bounded November 8, 20:00 to November 9, 08:00 window there are **10 distinct System/Display Event ID 4101** recoveries, **two Fortnite-specific application WER `D3DDRED2` events**, and three source-matched game crash contexts. WER LiveKernelEvent 141 has repeat report rows; those are not counted as distinct crashes.

Fortnite's own three crash contexts show `RHI.DRED=false`. Its logs report Intel Breadcrumbs explicitly disabled, `D3D12_FEATURE_EXISTING_HEAPS` unsupported, `DRED: No breadcrumb head found` and `DRED: No PageFault data`. These are **missing GPU execution witnesses**. UE's subsequent resource and PSO failures are not necessarily the operation that first hung the GPU. Microsoft documents `DXGI_ERROR_DEVICE_HUNG` and DRED limitations at:
- https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-error
- https://microsoft.github.io/DirectX-Specs/d3d/DeviceRemovedExtendedData.html

Local budget and usage reported by Unreal were 5480.06 MB versus 631.32, 623.20 and 209.35 MB at each failure. This does **not** establish simple reported-budget exhaustion, but also cannot rule out fragmentation, faulty command sequences, residency errors, or earlier GPU faults.

The original Fortnite crash evidence remains in its existing `pc-game-re-modding / FORTNITE_UEFN` project branch and local crash-lab database. The Host02 Underdog new V06 record **links to that evidence** rather than copying a second archive of logs and dumps. The public repo contains only sanitized summaries.

## What remains unresolved

**Exact Intel callback initiating the Fortnite device hang:** **UNKNOWN.** The DDI slot layout and four PSO/resource table implementations are mapped, but no trace ties a particular GPU submission, shader, resource, or callback instruction to the TDR. We must not infer causality from a `CreatePlacedResource` or `CreatePipelineState` failure occurring after device removal.

Suggested next read-only discriminating tests:

1. Examine the **existing** Unreal and Windows WATCHDOG minidumps with a trusted debugger and pinned public symbols, recording only call-stack/module summaries and any actual device-removal context. Preserve the original dumps in the Fortnite scope. WinDbg/CDB was not installed in the checked Windows SDK Debuggers paths, so this remains a tool dependency.
2. Use existing Underdog **owned fixtures** to record D3D12 PSO creation, resource/heap operations and command-queue submission on HD 520, with reversible scoped GPU diagnostics where supported. Do not replace Fortnite's render path or touch its anti-cheat.
3. Compare original kernel/Unreal time-correlated evidence and, if needed, a consented new external-only ETW capture of graphics work. Only assign an Intel callback as causal after a correlated runtime witness identifies the failing operation.

## Evidence and regression

The source-free models are `driver_intelligence/intel_hd520_adapter_abi_v05.json` and `driver_intelligence/intel_hd520_fortnite_gpu_faults_v06.json`. Regression commands:
`python tools/driver_intelligence/validate_intel_adapter_abi_v05.py` and
`python tools/driver_intelligence/validate_intel_fortnite_gpu_faults_v06.py`.

Host02 governed source provenance, game-log hashes and WER record IDs remain in `05_RESEARCH`, normalized rows in `02_DATABASE`, machine dataset in `03_DATASET`, decision and experience records in `04_KNOWLEDGE`, independent receipts in `06_EVIDENCE`, and current handoff in `08_HANDOFF`. No raw Intel binary, Microsoft SDK header, game log, XML crash context or minidump is checked into GitHub.

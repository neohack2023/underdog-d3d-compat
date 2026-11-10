# Intel HD 520: D3D12 user-mode driver adapter-function map

**Status (V06):** Eight adapter callback identities resolved through two pinned historical Windows SDK declarations. Intel CreateDevice and FillDDITable have additional high-confidence static signatures. The Intel function responsible for Fortnite GPU hangs remains unknown.

**Hardware:** Intel HD Graphics 520, PCI 8086:1916. Versions: 21.20.16.4534 and installed 22.20.16.4836.

The installed driver exports OpenAdapter12 at RVA 0x57F50. After the runtime callback query, it dispatches through a 14-entry relative jump table at RVA 0x58040. Five different implementations populate adapter-function tables, and two dispatch codes lead to an invalid-argument result. All five paths read the pAdapterFuncs pointer at OpenAdapter argument offset 0x18 and assign eight callback addresses at offsets 0x00 through 0x38.

The older driver exports OpenAdapter12 at RVA 0x90AF0 and uses a single observed implementation at RVA 0x913D0. That implementation assigns the same eight callback offsets.

See the machine-readable map at driver_intelligence/intel_hd520_openadapter_v02.json. It uses RVAs, not fixed load addresses. The source binary bytes and raw disassembly remain only in the Host 02 AIOS research workspace.

## Verification levels

| Boundary | Evidence state |
|---|---|
| OpenAdapter12 export | Verified PE |
| pAdapterCallbacks at OpenAdapter argument +0x10 | Strong DDI structural match |
| pAdapterFuncs at OpenAdapter argument +0x18 | Verified by bounded Ghidra disassembly |
| Eight function-pointer writes per variant | Verified by bounded Ghidra disassembly |
| Five installed driver variants and one older driver variant | Verified static code and data |
| Meaning of each callback slot | Resolved SDK ABI semantics for 8 x64 slots (V05), correlated with 48 previously verified Intel pointer RVAs |
| CreateDevice callback identity | High-confidence Intel static signature across 5 installed variants; documented adapter slot +0x08 |
| Intel function causing Fortnite driver reset | **Unknown cause**; 2 Fortnite D3DDRED2 / DEVICE_HUNG reports and 3 matching game crash logs confirmed |

**Historical V02 next trace, now completed:** The eight callback slots have been matched to preserved Windows SDK 10.0.10240 and 10.0.14393 D3D12DDI_ADAPTERFUNCS declarations, with both source Git blob hashes verified. CreateDevice was traced at +0x08 (V03), FillDDITable at +0x30, and selected device-core PSO/resource interfaces mapped statically (V04). The remaining target is to correlate recorded GPU work to the Fortnite device hang without assuming downstream API errors initiated it.

No driver modification, game interception, or code from proprietary binaries is in this repository.

Reference: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d12umddi/ns-d3d12umddi-d3d12ddiarg_openadapter


## Update V03: high-confidence CreateDevice identification

The eight-slot adapter-function table was previously preserved with semantic names unresolved.
A new read-only Ghidra pass traced slot +0x08 in all five installed-driver variants through
the forwarding wrapper to actual code. All five implementations read UINT fields at
second argument offsets +0x08 (Interface) and +0x0C (Version), matching the official
D3D12DDIARG_CREATEDEVICE_0003 declaration. The variant covering query value 0x1B
also calls an initializer at RVA 0x124900 that copies +0x08 (Interface), +0x0C (Version),
+0x20 (p12UMCallbacks) and +0x28 (Flags).

The callback is now labeled PFND3D12DDI_CREATEDEVICE_0003 with HIGH-CONFIDENCE STATIC
signature matching. This does not claim dynamic driver execution. At the V03 checkpoint, the other seven
adapter callback slot *names* were still unresolved; V05 has since resolved them using the SDK ABI.
Read the V03 machine-readable mapping:
[CreateDevice RVA trace](../driver_intelligence/intel_hd520_createdevice_v03.json).

Next engineering trace: follow the device initialization helpers to locate actual
device-function table population and identify the graphics PSO and resource creation
DDI entrypoints. Keep Intel binaries and raw disassembly local.


## Device core V04: PSO and heap/resource boundaries (static)

Following the installed driver's versioned CreateDevice callback, the adapter's
FillDDITable entry at adapter table offset +0x30 (RVA 0x400100) forwards to
implementation RVA 0x1225b0. It dispatches based on the requested DDI table code,
table size, and output pointer. Four core-function table constructors were statically
recovered: 0x1234A0 (712 bytes), 0x122FB0, 0x121730 and 0x122020 (736 bytes each).

In their versioned D3D12 device-core table layouts, the documented
CreatePipelineState callback is offset +0x110 and CreateHeapAndResource is offset +0x1E0.
Ghidra verified all eight assigned callback wrappers and the first forwarded
implementation in the installed Intel binary.

| Constructor RVA | PSO wrapper -> implementation RVA | Resource wrapper -> implementation RVA |
|---|---|---|
| 0x1234A0 | 0x400C00 -> 0x12D3E0 | 0x401120 -> 0x137D90 |
| 0x122FB0 | 0x400BC0 -> 0x12D160 | 0x401300 -> 0x13BC90 |
| 0x121730 | 0x400C40 -> 0x12CED0 | 0x401300 -> 0x13BC90 |
| 0x122020 | 0x400CA0 -> 0x12CBE0 | 0x4014C0 -> 0x13BB50 |

Exact negotiated DDI *version names*, live invoked callbacks, and a causal
graphics-driver fault mechanism remain unproven. The API-level owned fixture replay
passed four tests: graphics PSO, mixed graphics+textures, bounded resource escalator
and 60-frame Underdog Arena. These demonstrate known-good Direct3D API use only.

Machine-readable source-free map:
[Core PSO/resource RVA map](../driver_intelligence/intel_hd520_pso_resource_v04.json).

Research remains read-only. Proprietary Intel binaries and raw disassembly live
exclusively in local AIOS evidence storage, not in the MIT repository.

## V05/V06, November 10 2026: adapter ABI resolved, Fortnite fault evidence correlated

The eight slots in the table above now have documented x64 callback names:

| Adapter offset | Windows SDK callback | Intel identification |
|---|---|---|
| +0x00 | pfnCalcPrivateDeviceSize | SDK ABI identity; Intel wrapper RVA mapped |
| +0x08 | pfnCreateDevice | SDK ABI identity and V03 high-confidence static signature |
| +0x10 | pfnCloseAdapter | SDK ABI identity; Intel wrapper RVA mapped |
| +0x18 | pfnGetSupportedVersions | SDK ABI identity; Intel wrapper RVA mapped |
| +0x20 | pfnGetCaps | SDK ABI identity; Intel wrapper RVA mapped |
| +0x28 | pfnGetOptionalDDITables | SDK ABI identity; Intel wrapper RVA mapped |
| +0x30 | pfnFillDDITable | SDK ABI identity and V04 static implementation trace |
| +0x38 | pfnDestroyDevice | SDK ABI identity; Intel wrapper RVA mapped |

This is a semantic projection of historical V02 Intel pointer observations. Five installed-driver adapter tables and one older-driver table contain 48 previously verified callback RVAs. On Host 02, one eight-row SQLite table plus a join view presents those same 48 RVAs with names, without cloning them into another pointer dataset. This does not prove runtime invocation of every callback.

Fortnite evidence is now stronger but **the specific Intel culprit remains unknown**. Host 02 Windows WER confirms two Fortnite D3DDRED2 / DXGI_ERROR_DEVICE_HUNG reports on driver 22.20.16.4836 from Nov 8, 2026 at 21:21:56 and 21:36:29 EST. Original Unreal game logs and crash contexts match three incidents, including a Nov 9 DXGI_ERROR_DRIVER_INTERNAL_ERROR. In the bounded window, ten distinct igfx Display Event 4101 recoveries occurred. CreatePlacedResource, CreatePipelineState and Resource::Map calls failed in these logs, possibly *after* device removal. UE crash contexts state RHI.DRED=false, and logs say Intel Breadcrumbs disabled; the last successfully executing GPU workload is unknown.

The original game evidence remains in the registered Fortnite RE lab; the sanitized findings are linked rather than copied. See [V05/V06 ABI and Fortnite GPU fault investigation](INTEL_HD520_ABI_FORTNITE_DRED_V06.md), [adapter semantics V05](../driver_intelligence/intel_hd520_adapter_abi_v05.json) and [Fortnite device fault summary V06](../driver_intelligence/intel_hd520_fortnite_gpu_faults_v06.json). No raw driver binaries, SDK headers, crash dumps or game logs are distributed here.

Next discriminating work: read-only stack/dump inspection and scoped GPU scheduling/PSO/resource telemetry from authorized owned fixtures or historical crash evidence. Do not label a post-removal callback as causal without time-correlated GPU execution evidence.
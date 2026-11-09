# Intel HD 520: D3D12 user-mode driver adapter-function map

**Status:** Statically verified entrypoint, dispatch and function-pointer assignments. Callback names unresolved.

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
| Meaning of each callback slot | Unresolved |
| CreateDevice callback identity | Unresolved |
| Intel function causing Fortnite driver reset | Unknown |

Next trace: obtain the exact version-appropriate D3D12DDI_ADAPTERFUNCS layout from a trusted Windows Driver Kit declaration and verify it against the eight callback-slot RVAs. Only then follow CreateDevice and device-specific graphics PSO/resource interfaces.

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
signature matching. This does not claim dynamic driver execution. The other seven
adapter callback slots are still unresolved. Read the new machine-readable mapping:
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

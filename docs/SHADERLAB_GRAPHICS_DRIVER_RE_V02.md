# ShaderLab V02 — D3D8/D3D9 game boundaries and Intel driver versions

Host: Host 02, Intel HD 520, currently installed driver 22.20.16.4836.
Authority: local Underdog research candidate; no Drive scope promotion.
Method: REA 6.9.0 offline imports/exports, Ghidra 12.1.4, independent Python pefile. No driver installs, game launches or shader replacements.

## Confirmed game import boundaries

| Game | REA source evidence | Interpretation | Unknown |
|---|---|---|---|
| MGRR | d3d9.dll!Direct3DCreate9, d3dx9_43.dll!D3DXGetShaderConstantTable | D3D9 device entry and shader-constant helper | Shader bytecode, device vtable call sites, live shader models |
| UT2004 | d3d8.dll!Direct3DCreate8 and DDRAW.dll!DirectDrawCreate | D3D8 and DirectDraw entry points | Selected renderer, fixed-function/programmable shader usage |

MGRR also statically imports D3DXCreateTextureFromFileInMemoryEx, D3DXCreateCubeTextureFromFileInMemoryEx and D3DXCreateTexture, plus matrix, vector and quaternion functions. These are API entry points, not observed runtime calls.
Direct3D device methods are called through COM vtables. Absence of CreatePixelShader in a PE import list does not prove absence of shader creation.

## Intel driver versions

Local 21.20.16.4534 DriverStore files compared with the hash-verified official Intel 15.45.34.5174 (Windows driver 21.20.16.5174) ZIP.

| DLL | Named exports older → newer | Interpretation |
|---|---|---|
| igdumdim32.dll | 8 → 8 | 32-bit legacy Direct3D user-mode driver; OpenAdapter present in both |
| igd12umd64.dll | 1 → 1 | D3D12 user-mode driver; OpenAdapter12 stable by name |
| igd10iumd64.dll | 2 → 2 | OpenAdapter10 and OpenAdapter10_2 stable by name |
| ig9icd64.dll | 20 → 20 | OpenGL ICD reference; do not use as substitute for D3D9 |
| igc64.dll | 95 → 83 | Twelve Vulkan-flavored compiler exports absent in newer library |

Removed symbol exports do not establish lost rendering capabilities; implementations can relocate or become private. Stable exports do not establish equivalent implementations.

REA's Ghidra overview completed on the 5174 igd12umd64.dll: base 0x180000000, eight regions, 5,833 procedures, 1,144 defined strings. Ghidra identities do not prove full discovery or function meaning.
The separate 4836 D3D12 evidence identifies OpenAdapter12 RVA 0x57F50 and core PSO/resource callback slots +0x110 and +0x1E0. Never project these RVAs onto the 4534 or 5174 modules.

## Source and provenance

- Intel 15.45.34.5174 and official ZIP SHA-256: https://www.intel.com/content/www/us/en/download/18799/intel-graphics-driver-for-windows-15-45.html
- Intel 31.0.101.2115, reference only, not downloaded or installed: https://www.intel.com/content/www/us/en/download/762755/intel-6th-gen-processor-graphics-windows.html
- Dell 22.20.16.4836 OEM driver reference: https://www.dell.com/support/home/en-us/drivers/driversdetails?driverid=dwxd9
- REA source and guidance: https://github.com/morluto/rea

## Reproducible V02 implementation

- tools/shaderlab_driver_v02.py: expected-hash Intel archive check; bounded extraction of five selected user-mode libraries; PE export/import deltas.
- tools/shaderlab_rea_graphics_v02.py: reconcile REA game imports and Intel exports with explicit uncertainty.
- tools/shaderlab_record_v02.py: receipt-keyed evidence in existing Underdog SQLite with integrity verification and prewrite backup.
- tests/test_shaderlab_v02.py: conservative import and ABI inference tests.
- Local governance manifest: 00_MANIFESTS/SHADERLAB_DRIVER_SOURCE_REGISTRY_V02.json.
- Local evidence: 07_EXECUTION/SHADERLAB_V02, including original REA JSON, PE comparison, Ghidra overview and DB receipts.

## Next engineering steps

1. MGRR: identify Direct3DCreate9 thunk xrefs, COM vtable calls to shader creation/selection and constant setters; locate actual shader payloads or pack format.
2. UT2004: identify Direct3DCreate8 and DirectDrawCreate thunk xrefs, actual renderer DLL and fixed-function render-state behavior.
3. Intel: follow the 32-bit igdumdim32.dll OpenAdapter table per driver version; avoid assigning semantics from name-only exports.
4. Replay shader work on a disposable authorized offline game environment with rendering and GPU-time witnesses before any compatibility claim.

No binary archive or proprietary code is included in GitHub. Neither Intel driver nor game executable was installed, patched or executed by this V02 research slice.

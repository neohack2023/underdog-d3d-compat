# ShaderLab Host 02 Game Admission V01
Date: 2026-11-10
Authority: Underdog local execution evidence; this is not promoted Drive canon.

## Current host findings
Read-only scan of 17 existing game project directories. A bounded PE import pass examined up to eight executable candidates and 250 directories per game. Findings are **direct imported DLL signatures only**; games may load render APIs dynamically or use launchers, source ports, custom engines and compatibility shims. No game was launched or modified by this scanner.

| Game project | Direct static graphics imports found | ShaderLab recommendation |
|---|---|---|
| ARCANUM | DirectDraw | Legacy/2D boundary research, low priority for HLSL |
| DAGGERFALL | None in bounded sample | Resolve source port/runtime renderer |
| DRAGON_AGE_ORIGINS | None in bounded sample | Resolve canonical executable and render backend |
| DUKE3D_ATOMIC | None in bounded sample | Prefer authorized source port instrumentation |
| ELDEN_RING | D3D12 and DXGI | High integration complexity; no game interception planned |
| FAR_CRY_1 | None in bounded sample | Inspect actual game renderer/late-loaded DLLs |
| FORTNITE_UEFN | None in bounded sample | Driver reset research only; no injection |
| INTERSTATE_82 | None in bounded sample | Trace existing DDraw/3D compatibility sequence first |
| MGRR | D3D9 | Candidate for optional offline shader inspection and replacement evaluation |
| MGS1_PC | D3D9 configuration utility; DirectDraw game executables | Do not classify game rendering from configuration utility alone |
| MINECRAFT_WINDOWS | D3D11 in current Faucet test executables | Candidate for shader provenance audit, separate from gameplay modifications |
| QUAKE_1 | OpenGL in GLQuake executables | Source-port shader integration, not native D3D shader path |
| TDR2000 | DirectDraw | Existing compatibility-wrapper boundary first |
| UT2004 | D3D8 and DirectDraw | Candidate for D3D8 translation/opt-in shader tests |
| UT_GOTY | None in bounded sample | Resolve renderer before assigning shader model |
| WRECKFEST | No exe sampled | Inventory only |
| WRECKFEST+ | No exe sampled | Inventory only |

## Admission queue
1. **Arena**: Completed owned-renderer control. Compiler and GPU variant testing proved.
2. **MGRR**: Direct D3D9 import observed in game EXE. Preserve game baseline; investigate shader model and an offline read-only inventory before opt-in adapter work.
3. **UT2004**: D3D8 import observed; investigate a D3D8-to-supported-backend path before applying shader inspection.
4. **TDR2000 / Interstate '82**: Legacy rendering compatibility remains prerequisite. No reason to write a D3D12 shader patch for an unresolved DDraw interface.
5. **Quake 1**: Use source-port renderer branch for a different API test once exact local runtime path is resolved.

Do not infer "compatible" from imports. The only ShaderLab runtime GPU acceptance here is the source-owned Arena 24/24 run. No third-party ShaderLab integrations exist yet. "Not feasible" requires an explicit hardware or API impossibility finding rather than a missing executable.

## Evidence
Host 02 receipt (local only):
C:\AIOS\04_WORKSPACES\D3D_COMPAT_UNDERDOG_01\07_EXECUTION\SHADERLAB_V01\game_graphics_audit_v01.json

Registered database:
C:\AIOS\04_WORKSPACES\D3D_COMPAT_UNDERDOG_01\02_DATABASE\UNDERDOG_D3D_COMPAT_V01.sqlite
Table: shaderlab_game_api_findings (17 records)

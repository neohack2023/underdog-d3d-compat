# Veloren on Intel HD 520: low FPS and terrain-void triage V01

Observed November 9, 2026. **User-reported defect, not yet reproduced or attributed to the driver.**

## Two different findings

1. Verified: official Veloren Windows weekly da6626e6 with WGPU_BACKEND=dx12 entered a rendered 3D single-player world on HD 520 driver 22.20.16.4836. PresentMon 2.6.0 native idle-forest 35s captured 209 presents: approximately 6.10 presented FPS; median 166.15ms inter-present; GPU frame time median 164.24ms. UI/menu capture had 354 presents, approximately 29.96 presented FPS. This demonstrates high render cost for the tested world scene, not a whole-game performance score.
2. Unverified cause: user later reported a temporary black hole of terrain blocks beneath the player while running around; no matching hole screenshot, timestamp, terrain-event trace or driver-removal log. Preserve this as a symptom, not a diagnosed GPU defect or verified world corruption.

## Native graphical load

Read-only settings snapshot SHA-256: 845A6D4F90276EB79E4F58E6A8C572EDE8893E19C462664686D0A8BDD9DF3A76. Recorded 1920x1080 borderless, internal scale 1.0, cloud High, reflection High, fluid Medium, mapped shadows, bloom On, point glow 0.35, rain enabled, terrain view distance 10, sprite distance 100, particle chance 1.0.

Veloren performance guide https://book.veloren.net/players/performance.html recommends lower internal resolution, clouds, fluid quality, shadows, bloom and view distances for older GPUs. These are hypotheses to test, not validated fixes. Do not change user settings in place while preserving the native baseline.

## Exact source inspection, pinned da6626e6

Upstream source and license remain separate from MIT Underdog. Inspect https://gitlab.com/veloren/veloren/-/blob/da6626e6/voxygen/src/scene/terrain/mod.rs

- Line ~124: ChunkMeshState (position, started tick, worker active and skip-mesh status)
- Line ~147: MeshWorkerResponse; ~247: mesh_worker
- Line ~831: Terrain::maintain
- Line ~856: removed_chunks received
- Line ~887: new_chunks received
- Line ~914: mesh_todo insertion
- Line ~1028: meshing worker concurrency budget
- Line ~1118: terrain worker launch
- Line ~1136: worker response and GPU upload
- Line ~1263: stale mesh discard

Suggested opt-in source auditor: bounded event counters per chunk coordinate for received, queued, worker started, worker finished, discarded, GPU-upload requested/completed, and drawn. Add 256-entry ring buffer around camera location, with timestamps; ensure OFF is unchanged and ON has no mesh or resource mutations. Never infer that a visually missing chunk is a driver fault without inspecting these transitions and comparing native DX11/12.

## New native settings-only controls (isolated VELOREN_USERDATA)

A verified native reference (1.0 scale, expensive effects, prior idle forest scene): 209 presents / 35s, 6.10 presented FPS, GPU time p50 164.24 ms.

Native scale-only control (0.65 scale, all effects originally high, same game executable, isolated saved world): 208 presents / 35s, 6.11 FPS, GPU p50 164.04 ms. A lower internal resolution alone made no observable improvement in these *unmatched conditions*.

Native low-effects bundle at 0.65 scale: 423 presents / 35s, 12.24 FPS, GPU p50 83.80 ms. Runtime settings verified: clouds Low, shadows r#None, bloom Off, point glow 0.0, reflection High, fluid Medium. The requested fluid Cheap setting was automatically returned to Medium; do not report it as an applied change. Output remained 1920x1080. In-game screenshot preserved. *Exploratory scenes were not frame-matched for camera/time/weather; no causal per-effect or Underdog speedup is claimed.*

The test world's 22.7MB copy was created solely under 07_EXECUTION/VELOREN_PERF_TRIAGE_V01 via upstream `VELOREN_USERDATA`, preserving the original settings SHA-256 845A6D4F90276EB79E4F58E6A8C572EDE8893E19C462664686D0A8BDD9DF3A76. Retain the isolated world until the user-reported terrain void has been reproduced or resolved; it is currently a necessary failure-investigation fixture. No hole reproduced in the short stationary runs.

Receipts: `06_EVIDENCE/VELOREN_NATIVE_DX12_RENDER_SCALE065_V01.json`, `06_EVIDENCE/VELOREN_NATIVE_DX12_LOWFX065_V01.json`. Dataset: `03_DATASET/VELOREN_GRAPHICS_AB_EXPERIMENT_V01.json`. SQLite: `game_performance_runs`, with native-only classifications. Further controlled tests should isolate cloud, shadow, bloom, reflection, sprite/terrain view distance and GPU mesh upload; do not patch Intel driver first.
## Controlled tests

Use the dataset VELOREN_GRAPHICS_AB_EXPERIMENT_V01.json under Host02 03_DATASET. A remains original, B changes only internal resolution factor, C tests lowered graphics cost, D keeps same settings but changes to native DX11. Record at least three paired 120+ present runs per condition with same world, location, weather/time, camera, route and logs.

Before calling missing terrain fixed, capture a reproducible void and verify whether waiting in place restores it and whether collision still exists. Keep the original game executable, world state, and original settings file unchanged; no unverified driver fixes.

## Constraints

The separate GPL-compatible source-level wgpu auditor patch has only passed static apply/reverse checks. Rust/Cargo was not available and the patch has not been compiled. PresentMon files are **native A-side only**. Do not claim any Underdog integration, bug repair, or FPS improvement yet. Evidence belongs to governed Host 02 storage; source to GitHub, Drive scope promotion only after registered authority review.

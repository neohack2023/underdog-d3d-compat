# Veloren real-game Underdog audit and performance protocol V01

Status as of 2026-11-09: **native D3D12 PLAYABLE, native PresentMon captures complete, opt-in source patch static-apply checked; audited game NOT BUILT and NO improvement demonstrated**.

## Authority / licensing boundary

- Upstream game: Veloren official GitLab revision `da6626e6`, tagged in game UI as 2026-10-27. Source: https://gitlab.com/veloren/veloren/-/tree/da6626e6.
- Veloren's gameplay source is GPL-3.0; the Underdog native diagnostics repository is MIT. **Do not copy or vendor Veloren source/derived patch into the MIT tree.** The isolated GPL consumer patch is stored on Host 02 in `07_EXECUTION/VELOREN_POLICY_AUDIT_V01`.
- No binary patch, driver injection, graphics-driver modification, or wgpu HAL ownership escape.

## Hardware and untouched baseline

Host02 Intel HD Graphics 520, Windows 10 build 18362, graphics driver 22.20.16.4836. Official Veloren executable hash pinned in its separate baseline receipt. Launch with `WGPU_BACKEND=dx12`.

PresentMon **v2.6.0** signed CLI tool placed in `C:\AIOS\01_TOOLS\PRESENTMON_V260`. Official tool source: https://github.com/GameTechDev/PresentMon/releases/tag/v2.6.0. No MSI or background service installed.

Two recorded reference captures:

| Scene | Native captured frames | Mean present FPS | Present p50/p95 ms | GPU time p50 ms |
| --- | ---: | ---: | ---: | ---: |
| Menu / character selection, 12 sec | 354 | 29.96 | 33.35 / 33.88 | 4.13 |
| In-world idle forest, 35 sec | 209 | 6.10 | 166.15 / 170.22 | 164.24 |

Scene screenshots, source log, raw PresentMon CSV, exact hashes and receipts stay on Host 02. Weather, game time, terrain view, camera and texture caching must be held constant for causal comparison. The forest sample shows a GPU-intensive workload; it is not evidence that Underdog already improved it.

## Proposed source-level audit, opt-in

An isolated 4.7 KB GPL-compatible unified diff against the pinned upstream `renderer/mod.rs`, `texture.rs`, and `renderer/binding.rs` was prepared. `git apply --check`, `git apply`, and reverse-check pass on hash-pinned source snapshots. **Rust/Cargo are now detected on Host 02 (rustc/cargo 1.99.0; rustfmt installed), but no pinned game source build has completed, so it has not compiled or run.**

When the separate consumer build exists, `UNDERDOG_WGPU_AUDIT=1` records device features/limits, texture-descriptor format/dimensions/usage, and figure/terrain/sprite atlas-binding events. A hard 256-observation cap per audit site limits log growth. The patch does not change resource descriptors or bindings. OFF mode is a build-matched comparison control, not proof of byte-identical binary to the official release.

## Scientific controls for the real-game A/B

1. **A: Unmodified official executable.** Freeze revision, saved world, character, scene, graphics settings, resolution and driver. PresentMon 120+ presents, screenshot, GPU timings, memory observation.
2. **B: Patched consumer build with audit disabled.** Controls compiler/source-build differences from the official binary.
3. **C: Same patched consumer build with audit enabled.** Compare C to B to measure auditor overhead and fault visibility, not graphics performance improvement.
4. **D: Future opt-in evidence-justified Underdog policy enforcement.** Only after verifying correct boundaries, add resource/binding policy adaptation; compare to B and C. A speedup or compatibility benefit requires correct visuals, 120+ consecutive presents, stable device, repeated controlled runs and statistical confidence. Do not equate policy logging with performance improvement.
5. Normalize scene/environment variability and collect at least three paired trials. Compare median, p95, GPU time, CPU busy, memory usage, errors and image parity.

## Pending technical gates

- Rust/Cargo toolchain and isolated Veloren source build (not present; avoid unbounded compilation on a disk-constrained host).
- Validate the pinned patch against the actual upstream Cargo/wgpu 27 build, then promote a **compiled** audit artifact.
- Build an explicit no-mutation policy auditor, then a separate opt-in enforcement fixture.
- Measure A/B/C (and D if justified) on the same in-world route.

No current record should be classified as `compatible` for Underdog+Veloren. Native Veloren D3D12 is compatible with this GPU/driver; Underdog integration remains `in_progress`.

## Native low-cost control and terrain void, November 9

The user reported extremely slow gameplay and a black hole of terrain blocks beneath the character. The visual hole is **user-observed, not instrumented or root-caused**. Latest native-only PresentMon data: original 1.0-scale forest approximately 6.10 FPS (209 frames), isolated 0.65-scale effects-high village approximately 6.11 FPS (208 frames), isolated 0.65-scale reduced-effects village approximately 12.24 FPS (423 frames). The latter two have matching world/character but no frozen camera, weather or time; no per-effect causal conclusion. The low-effects runtime profile retained fluid Medium and reflection High, with clouds Low, shadows None, bloom Off, glow 0.0. See `docs/VELOREN_TERRAIN_VOID_TRIAGE_V01.md` and governed Host02 receipts.

The isolated 22.7MB mutable userdata fixture is not a canonical game archive and must remain protected until the terrain-void investigation closes. It is separate from the original unchanged player save and will be evaluated for cleanup when reconstruction is proven. Audit/Underdog source patch is still not compiled, so no speedup attribution is allowed.
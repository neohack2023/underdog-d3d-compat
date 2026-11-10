# Veloren terrain lifecycle trace V01 | Intel HD 520

**Status: exact pinned source traced; 9 source findings ingested; optional GPL source instrumentation applies/reverses and passes rustfmt syntax parsing; game instrumentation is NOT compiled or run.**

## Why this matters

The user observed severe lag and a black void of terrain blocks under their character in Veloren. The visual defect is **OPEN / USER REPORTED / UNREPRODUCED**. A native D3D12 35-second scene captured approximately 6.1 FPS at High clouds/lighting and mapped shadows. A separate low-effects native scene was around 12.24 FPS. Both were unmodified official builds; all comparisons remain exploratory and are not Underdog performance claims.

## Pinned source and mechanics

Official game source revision: [Veloren da6626e6 terrain](https://gitlab.com/veloren/veloren/-/blob/da6626e6/voxygen/src/scene/terrain/mod.rs). Raw upstream source SHA256: `CF65FE0B8551C2C7433B8F8BBB44A524C16DB86A1496529DF7FC52098E1CEC90`.

| Stage | Source lines (pinned) | Actual behavior | Fault classification if missing |
| --- | ---: | --- | --- |
| Chunk removed | ~856-865 | Remove GPU chunk model and neighboring todo entries | missing from world stream |
| New/modified arrival | ~874-925 | Requires all 3×3 neighbor chunks before queue admission | neighbor arrival or world stream |
| Meshing queue | ~1028-1059 | Prioritize nearby chunks, limit active worker jobs by CPU count | backlog or worker scheduling |
| Neighbor sampling | ~1075-1084 | Missing neighbor leaves todo for later retry | stalled mesh sampling |
| Worker dispatch | ~1110-1133 | Asynchronous TERRAIN_MESHING slow job | pending worker |
| Result receipt | ~1136-1153 | Meters worker results at 240/s scaled by frame delta | upload processing backlog |
| Stale result path | ~1152-1266 | Accepts response tick **less than or equal to** pending todo tick, but clears todo only if equal; older accepted responses can temporarily be installed | **investigate**, not established defect |
| Atlas allocation | ~1177-1199 | On failure creates new atlas and clears old chunk allocation IDs | texture atlas churn or data association |
| CPU model and atlas writes | ~1206-1251 | Update atlas textures and store opaque/fluid models | GPU upload-request/model creation |
| Visibility | ~63-72, ~1281-1307 | Only `in_frustum` currently gates drawing, `in_range` is not used in `is_visible()` | frustum exclusion |
| Final draw submission | ~1605-1642 | Must exist in chunk render map, be visible and have opaque model | draw eligibility, **not actual GPU present** |

## New source trace patch (kept GPL-isolated)

Under the Host 02 governed execution folder `07_EXECUTION/VELOREN_TERRAIN_TRACE_V01`:

- `upstream_terrain_da6626e6.rs`: immutable official source used for SHA/check.
- `generate_terrain_trace_patch_v01.py`: deterministic exact-source generator.
- `veloren_da6626e6_terrain_trace_v01.patch`: reversible GPL consumer patch; never vendored into MIT Underdog.
- `terrain_trace_patch_receipt_v01.json`: source/patch hashes and test state.
- `unchanged_native_log_trace_parse_v01.json`: **zero** opt-in terrain events (expected negative control).

The opt-in `UNDERDOG_TERRAIN_TRACE=1` patch adds a bounded 8192-event trace for: REMOVED, NEIGHBOR_WAIT, QUEUED, SAMPLE_WAIT, WORKER_START, WORKER_RECV, OLDER_RESULT_ADMITTED, REJECTED_TICK, REJECTED_REMOVED, ATLAS_RESET, UPLOAD_BEGIN, MODEL_STORED, VISIBLE, CULLED, FOCUS_DRAW_GATE. It **does not mutate** the rendering rules, chunks, draw ordering, or graphics resource policy. The trace is capped to avoid runaway log growth, but the cap is not a ring buffer: a late fault may require a chunk-specific or higher-budget build.

Validated: patch dry-run apply PASS; actual apply PASS; reverse-check PASS; normalized patched source matches generated content; Rust 2024 syntax parser via rustfmt PASS. Rust/Cargo are installed on Host 02 (rustc/cargo 1.99.0), correcting the earlier toolchain-unavailable note. **Not** validated: entire-game Rust compile, runtime tracer logging, GPU execution/presentation of modified consumer, or reproduction of the player's missing-chunk visual defect.

## Generic parser (MIT Underdog)

`tools/diagnostics/analyze_veloren_terrain_trace_v01.py` works on actual opt-in source logs. Run `python ... --self-test` to verify six synthetic event cases; run with `--log PATH --out JSON [--chunk X,Y] [--radius N]` when a real instrumented game log exists. The parser emits per-chunk stage counts and prioritized diagnostic flags but **never** concludes a driver fault without a screenshot/time-correlated trace. The unmodified game log contains no trace events.

## Next execution gate

Acquire and build a pinned GPL consumer source worktree in a separately bounded Host 02 test workspace, apply the isolated patch, compile with Rust/Cargo and confirm trace target logging; run native Dx12 through the copied single-player world while capturing coordinates and video. On the first black-void frame, identify chunk position and compare all stages, including whether the player could collide with missing terrain. Only if FOCUS_DRAW_GATE says drawable and the actual frame still omits geometry should we pursue driver/GPU draw or resource-residency instrumentation.

Keep original executable SHA and original saved-world/settings untouched; do not assume the current source patch makes Veloren faster or fixes missing terrain. Prior performance A/B and the visual defect remain separately tracked.

Data state: `03_DATASET/VELOREN_TERRAIN_LIFECYCLE_V01.json`, `02_DATABASE/UNDERDOG_D3D_COMPAT_V01.sqlite`, `04_KNOWLEDGE/VELOREN_TERRAIN_TRACE_FINDINGS_V01.md`, `06_EVIDENCE/VELOREN_TERRAIN_SOURCE_TRACE_V01.json`; governed source handoff in `08_HANDOFF`. No Notion writes.

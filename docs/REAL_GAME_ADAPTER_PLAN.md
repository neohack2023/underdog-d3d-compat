# Real-game adapter plan: after Arena

The current HDMI/Intel HD 520 matrix has only one compatible entry: our own original Underdog Arena *graphics demo*. This document defines a path to a **real source-available game** without modifying protected game executables or introducing unmaintainable DLL interception.

## First target: Dethrace / Carmageddon 1997

Upstream: https://github.com/dethrace-labs/dethrace

This is **not** Carmageddon TDR 2000. Keep them distinct compatibility entries, evidence, source trees and legal provenance.

Dethrace uses SDL2 and CMake, and needs data files from a separately owned copy of Carmageddon or an authorized demo. Its source licensing and game-content rights are not inherited from the MIT Underdog repo. Do not vendor Dethrace source, copy game assets, or imply the upstream license has changed.

1. Resolve the actual AIOS scope and local Dethrace build path through Drive's current registry and live Host 02 execution inventory.
2. Verify an unchanged, native baseline: starting menu, in-race visual observation, controller, 120 consecutive frames, and device stability.
3. Inspect its real renderer boundary and platform abstractions. Write an opt-in adapter in a separate consumer project; no global DLL replacement.
4. If feasible, consume UnderdogRuntime for capability queries, texture policy, GPU allocations and fence-lifetime logic. At first, use a D3D12 backbuffer upload/present adapter that preserves gameplay state and visuals. Avoid claiming software raytracing improves a fundamentally rasterized legacy engine.
5. A compat promotion requires an in-game race with the adapter loaded, functioning UI, image comparison, stable rendering, and full hardware/driver-specific receipts.
6. Capture GPU timestamps, frame pacing, memory residency and CPU overhead against the unmodified baseline.

**Current status: IN PROGRESS. No Dethrace + Underdog integration demonstrated yet.**

## Second target: Quake 1 source-port adapter

Use a source-available Quake port with a properly separated renderer boundary, recording its original license and baseline. Existing OpenGL/Vulkan rendering is not automatically a Direct3D adapter. Do not claim compat simply because a source port already runs on HD 520.

## Status changes

- **Compatible:** repeatable game-specific integration and a playable visual witness at the named GPU/driver. Include tool, commit, screenshot/log and frame metrics.
- **In progress:** benchmark or adapter still missing, native-only gameplay, unresolved driver crashes, or a failed candidate that might be corrected.
- **Not feasible:** specific repeatable proof that the configuration cannot support the needed functionality or practical frame rate; not inferred from a lack of testing.

Scope of the status is always a particular hardware/OS/driver/renderer combination, not a global blanket verdict.

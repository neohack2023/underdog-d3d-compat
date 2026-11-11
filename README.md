# Underdog: Graphics Compatibility & Feature Extension

**Make more graphics capabilities available across generations of games, APIs, drivers, and hardware.**

Underdog is an experimental graphics engineering project (MIT-licensed source; repository visibility is currently private) investigating how to extend the useful life of older GPUs, make unsupported rendering features practical through compatible alternatives, and bring selected newer graphics techniques to older games.

We are exploring **software-backed graphics drivers and translation layers**, **feature scheduling across hardware and software execution paths**, **graphics API compatibility**, and **game-specific rendering upgrades**. The long-term goal is to let an application request a graphics capability and have a verified implementation satisfy it using the best available path, even when the original hardware or driver does not provide that feature natively.

> **Current reality:** Underdog is a collection of working native Windows graphics experiments, a small D3D12 compatibility runtime, repeatable tests, and driver research. It is **not yet** a replacement Windows GPU driver, universal Direct3D/Vulkan implementation, complete feature scheduler, DXR emulator, or automatic game compatibility solution.

## What we're building

### 1. Software graphics drivers, backends, and compatibility layers

Explore driver-facing interfaces, user-mode graphics backends, API translation, CPU rasterization, and GPU-compute implementations that can provide missing graphics behavior.

Possible implementation paths include:

- **Native GPU path:** use supported hardware functionality without unnecessary translation.
- **GPU compute fallback:** implement a selected missing operation with compute shaders when feasible.
- **Software backend:** implement or reference the operation on the CPU when the GPU lacks a suitable path.
- **API/backend translation:** adapt supported operations across graphics APIs or rendering backends.
- **Game-specific adapter:** integrate a reversible fix, renderer replacement, or feature upgrade where we control the code or have an authorized, compatible modification path.

**Status:** Hardware/compute and software-reference experiments exist. Low-level Intel driver interfaces are being mapped through read-only reverse engineering. A complete installable driver or general software rendering backend does not yet exist here. We do not modify system graphics drivers as part of the current test suite.

### 2. Graphics feature scheduling and workload routing

The proposed scheduler will match a requested graphics feature with a supported execution path, taking account of:

- Device capabilities, driver behavior, API and resource-format support.
- Correctness, ordering, resource residency, synchronization, and memory budgets.
- Estimated compute cost, transfer overhead, latency, and available CPU/GPU capacity.
- Verified fallbacks, per-game compatibility rules, and explicit opt-in settings.

The intended decision model is:

```text
Game / application / renderer
             |
    Feature and resource request
             |
      Capability + policy probe
             |
    [future: feature scheduler]
        /       |        \
 Native GPU  GPU compute  Software / CPU
        \       |        /
   Synchronize / validate / present
             |
   Correctness + performance evidence
```

The scheduler is **a design target**, not a currently implemented general-purpose dispatcher. Today's `UnderdogRuntime` has narrower feature probing, ray-backend selection, texture policy, and fence-aware resource budgeting.

### 3. Modern graphics features for older games and GPUs

We want to experiment with introducing newer rendering techniques into older game engines, while also adapting newer games to older hardware where realistically possible.

Candidate features include improved textures and streaming, lighting and shadows, post-processing, resolution scaling, compute-assisted rendering, alternate rasterization paths, and selective ray-query or raytracing-like effects. Each feature needs an actual integration seam, compatible assets/shaders, and visual/performance evidence.

This is **not** a claim that any legacy GPU can gain arbitrary hardware capabilities, or that an older game will automatically support a new feature through a driver shim. Hardware limits, API semantics, shader models, bandwidth, licensing, and game-engine design still apply.

Our approaches include source-level engine changes, authorized reversible binary/data patches, user-selectable renderer backends, and small standalone compatibility fixtures. We prioritize proven workarounds before attempting invasive driver changes.

## Current engineering status

| Capability or investigation | State | Evidence / boundary |
|---|---|---|
| Intel HD 520 D3D12 capability probes and `UnderdogRuntime` | **Verified prototype** | Device feature reporting, backend selection policy, texture budget and fence management |
| Compute-based ray intersection | **Verified experiment** | CPU-built BVH, GPU compute ray/triangle testing on bounded scenes; **not** full DXR |
| Texture formats, uploads, and streaming | **Verified experiments** | BC1/BC3, shader sampling, selected mip copies and resource policies; **not** universal texture paging |
| Underdog Arena integrated graphics test | **Verified prototype** | BVH rendering into a Win32 D3D12 swap chain |
| Typed UAV resource/binding validation | **Verified diagnostics** | Hardware/WARP comparison and repeatable Arena image parity |
| Intel HD 520 driver-interface investigation | **Research / partial mapping** | Read-only D3D12 adapter/device callback investigations; **not** a replacement driver |
| Windows build and CPU-safe regression tests | **Implemented** | GitHub Actions compilation and hardware-free tests; GPU behavior requires an actual execution host |
| D3D11 textured sprite vs. PS2 GS CPU reference | **Verified experimental branch, not merged** | Intel HD 520 and WARP: exact pixel parity on one synthetic sprite; [draft PR #2](https://github.com/neohack2023/underdog-d3d-compat/pull/2) |
| General graphics feature scheduler | **Planned** | Per-feature routing, synchronized fallback selection, workload measurement |
| General software graphics driver/backend | **Research objective** | Interface contracts, software rasterization and translation feasibility |
| Third-party game compatibility or modern-feature retrofits | **Per-game investigation** | No general compatibility guarantee or automatic injection mechanism |

**Terminology:** “Verified” means the named, bounded test was executed and supported by a recorded witness. It does not mean the capability is generally available in all games, GPUs, or drivers. “Research,” “planned,” and “experimental branch” are not supported-product claims.

## Architecture: hybrid rendering without pretending hardware limits disappear

Underdog's **hybrid** approach means selectively assigning supported graphics operations to native hardware, GPU-compute substitutes, or CPU/software implementations, then validating their results. It does **not** mean that all operations are automatically interchangeable or that a software path will outperform a native one.

A useful fallback needs to satisfy the required API behavior, resource formats, synchronization and lifetime rules, image correctness, and an acceptable performance budget. The system may decide that a feature is unavailable or not feasible instead of returning misleading capability information.

In particular, a DXR-like compute technique is not equivalent to implementing Microsoft's DXR API, and a software renderer is not inherently a system graphics driver.

## Hardware and game compatibility

**Primary development target:** Intel HD Graphics 520 on Windows, with Microsoft WARP as an independent software-device reference for supported tests.

The [Intel HD 520 game compatibility matrix](compatibility/README.md) tracks compatible, in-progress, and not-feasible cases, while separating:

- A game's native behavior on the tested hardware.
- An Underdog-owned fixture's behavior on that hardware.
- An actual Underdog integration into that game.
- Unknown, blocked, or unverified results.

We are also researching older Intel hardware, including HD 4000, through APIs such as D3D11 that may be supported where D3D12 is not. **HD 4000 execution has not been verified by the recent GS rendering tests.**

A working standalone experiment does not upgrade a third-party game's compatibility classification. That requires an opt-in integration, a repeatable launch/playable witness, and resource and rendering validation.

## Verified experiments and current investigations

### D3D12 runtime and Arena

`UnderdogRuntime` is an initial C++ library in [src/Runtime.cpp](src/Runtime.cpp) and [include/underdog/Runtime.h](include/underdog/Runtime.h). It provides bounded native D3D12 feature reporting, ray-backend choice, compressed texture policies, and fence-aware texture budgeting.

The [Arena V07 report](docs/ARENA_TYPED_TEXTURE_V07.md) documents optional descriptor-table typed UAV output, Intel HD 520 versus WARP comparisons, resource preflight, and byte-identical images across its bounded scene fixtures.

The [typed UAV regression](docs/TYPED_UAV_BINDING_V05.md) investigates a binding mismatch that produced pipeline-state errors in an earlier standalone fixture. It is not a Fortnite driver fix.

### Intel driver reverse engineering

The [driver interface map](docs/DRIVER_INTERFACE_MAP.md) and [machine-readable Intel adapter data](driver_intelligence/intel_hd520_openadapter_v02.json) document observed D3D12 driver-facing callback structures and function addresses.

This is interface research to inform targeted compatibility experiments. It does not provide a complete public specification of Intel's driver internals, vendor source code, or a drop-in driver replacement. Driver binaries are not bundled.

### PS2 GS and NativeSX2 renderer investigation

We are studying the [NativeSX2](https://github.com/contatonewportgame-code/nativesx2) and [PS2Recomp](https://github.com/ran-j/PS2Recomp) ecosystems as external reverse-engineering references.

A **draft, opt-in D3D11 research slice** has rendered a synthetic textured sprite on Intel HD 520 and WARP and matched the separately compiled PS2Recomp CPU GS reference pixel-for-pixel. The working experiment currently handles one controlled sprite, not arbitrary PS2 graphics commands or a playable game.

See [draft PR #2](https://github.com/neohack2023/underdog-d3d-compat/pull/2) for the source, independently generated test input, detailed limits, and validation plan. GPL-licensed upstream source used for local reference testing is kept outside Underdog's MIT-licensed repository.

## Building and testing

**Current Windows requirements:** Visual Studio 2022 C++ Build Tools, Windows SDK, CMake, and Python for selected validation tests.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For selected earlier examples, `tools/build_examples.cmd` can be run from a Visual Studio Developer Command Prompt.

**CI and physical GPU tests are different gates.** GitHub Actions builds Windows code and runs CPU-safe/static checks. Hardware-dependent D3D11/D3D12 tests require explicit execution on a suitable Intel GPU or a selected WARP device, with appropriate fixtures and recorded output. A successful CI build is not proof of game compatibility or frame-rate improvement.

## Project structure

| Directory | Purpose |
|---|---|
| `include/`, `src/` | Reusable Underdog C++ runtime and compatibility policy |
| `examples/` | Independent owned graphics and window/presentation experiments |
| `tests/` | Regression tests, hardware-smoke source, and controlled render fixtures |
| `tools/` | Build, validation and reproducibility utilities |
| `fixtures/` | Small owned or reproducibly generated inputs |
| `compatibility/` | Hardware-specific game compatibility states and policy |
| `driver_intelligence/` | Read-only vendor interface observations and validation scripts |
| `evidence/` | Bounded witnesses and provenance pointers |
| `docs/` | Reports, architecture, implementation limitations and handoffs |

## Engineering rules

1. **Correctness before speed:** check images, checksums, resources, device faults, and explicit synchronization.
2. **Baseline first:** keep unmodified/native behavior separately measured from compatibility intervention.
3. **Feature claims stay narrow:** do not report general driver support from a standalone shader, compute, or memory-copy test.
4. **Choose reversible integration:** no unreviewed system-driver modifications or silent edits to installed games.
5. **Avoid fake capability reports:** feature tiers and API support must reflect an actual supported implementation and semantics.
6. **Preserve provenance:** source identity, hashes, driver/GPU, fixtures, measurements, and reproducible failures remain distinguishable.
7. **Observe licenses and security boundaries:** third-party code is not silently copied into this MIT repository. No anti-cheat bypasses or unauthorized injection.

## Research precedents

- [Mesa / Gallium / llvmpipe](https://www.mesa3d.org/): software rasterization and driver architecture.
- [Microsoft D3D12 Raytracing Fallback Layer](https://github.com/microsoft/DirectX-Graphics-Samples): an important historical compute-fallback precedent.
- [DXVK](https://github.com/doitsujin/dxvk) and [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton): graphics API translation research.
- [PS2Recomp](https://github.com/ran-j/PS2Recomp): game/runtime reconstruction and graphics backend abstraction.

These are external research references. Their licenses and architectural assumptions vary; this project does not claim ownership of their implementations.

## Roadmap

**Near term**
- Maintain reliable Windows CI, GPU/WARP parity fixtures, and game compatibility evidence.
- Generalize the GS sprite experiment toward a guarded GPU backend, with CPU-reference checks for transfers, texture flushes, blending, feedback, and presentation.
- Expand the set of tested hardware and application-controlled integration points.

**Mid term**
- Define a versioned graphics-capability contract and a per-feature scheduling policy.
- Add more software/compute fallbacks, resource synchronization rules, and explicit cost measurement.
- Explore source-available renderer adapters and reversible enhancements for selected older games.

**Long term**
- Investigate whether modular software-backed driver components or user-mode compatibility layers can expose useful missing functionality without misrepresenting GPU capabilities.
- Support multiple APIs, generations of graphics hardware, and opt-in game integrations through tested, maintainable adapters.

## License and scope

Underdog source is licensed under [MIT](LICENSE), unless an individual file says otherwise. Third-party repositories, game code, binaries, drivers, and game assets retain their own licenses and rights.

Underdog is a graphics research and implementation effort, not a promise that every game will run on every GPU. Its value comes from narrow, reproducible engineering results that can gradually grow into reusable capabilities.

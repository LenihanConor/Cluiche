# Research: Explore — CMake Migration & Dia Architecture

**Session date:** 2026-05-25
**Folder:** docs/research/migrat_cmake/

## Problem Space Overview

Cluiche currently builds with 55 Visual Studio project files (50 Dia modules + 5 application projects) in a single `.sln`. `Directory.Build.props` centralises toolchain settings. The build system works but has two compounding problems:

**Problem 1 — Architecture enforcement is purely documentary.** Every module has a `dia.*.architecture.module.md` YAML file listing `dependencies.required` and `dependencies.forbidden`. But nothing enforces this at build time. A developer can add a `#include <DiaGraphics/...>` inside `DiaCore` and MSBuild will silently compile it. Violations only surface during code review — if caught at all. With 55 modules the graph is effectively ungoverned.

**Problem 2 — The module graph is logically flat.** All 50 Dia `.vcxproj` files sit at the same depth in the solution. The `dia.root.architecture.module.md` lists logical groupings (Core, Maths, Graphics, Physics, Application…) but these are not structural in the build system. There is no enforced concept of "this module is foundational — nothing it depends on may depend back on it."

CMake is the lever that turns both problems into build errors: `target_link_libraries` with explicit `PUBLIC/PRIVATE/INTERFACE` scoping means the dependency graph is declared once and enforced by the linker. You cannot accidentally consume a forbidden module unless you explicitly wire it in CMake — making violations visible in diffs.

The secondary payoffs (Clang-Tidy via `compile_commands.json`, TSan on Linux/WSL2, VS Open Folder IDE) become available as a consequence, not the primary goal.

## Current Module Landscape

55 `.vcxproj` files. Natural layers already visible in the YAML dependency declarations:

| Layer | Modules (current names) | Deps |
|-------|------------------------|------|
| **Foundation** | DiaCore, DiaMaths, DiaGeometry2D, DiaGeometry3D | None (or each other) |
| **Platform Primitives** | DiaWindow, DiaInput, DiaThreading, DiaMailbox | Foundation only |
| **Engine Services** | DiaApplicationFlow, DiaObservation, DiaEntity, DiaAssetCatalogue, DiaAssetRuntime, DiaStateMachine, DiaSerializer, DiaAsset | Foundation + Primitives |
| **Simulation** | DiaRigidBody2D, DiaSoftBody2D, DiaRig2D, DiaIK2D, DiaAnimation2D | Foundation + Services |
| **Rendering & Integration** | DiaGraphics, DiaBgfx, DiaSFML, DiaUI, DiaUICEF, DiaUIUltralight, DiaImGui, DiaProtobuf | Foundation + Primitives + (some Services) |
| **Tooling & Debug** | DiaVisualDebugger, DiaVisualDebuggerConsole, DiaEditor, DiaAPI, DiaAutomation, DiaWebSocket, DiaDebugServer, DiaDebugProtocol | All layers above |
| **Visual Debuggers** | DiaRigidBody2DVisualDebugger, DiaSoftBody2DVisualDebugger, DiaAnimation2DVisualDebugger, DiaRig2DVisualDebugger, DiaIK2DVisualDebugger, DiaGeometry2DVisualDebugger | Simulation + Rendering + Tooling |
| **Editor Plugins** | DiaAssetCatalogueEditor, DiaAssetRuntimeEditor, DiaApplicationEditor, DiaPipelineEditor | Services + Rendering + Tooling |

The flat MSBuild layout doesn't express this — it's all siblings. CMake subdirectories or `ALIAS` targets can make the layers structural.

## Existing CMake Approaches

- **Full migration — CMake generates VS solution** — `cmake -G "Visual Studio 17 2022"` produces `.sln` + per-project `.vcxproj`. Developers open the generated solution. CMake is the source of truth. Loses hand-authored `.vcxproj.filters` layout.
- **CMakePresets.json dual-track** — One preset generates VS solution for daily dev; a second uses Ninja for fast CLI/CI builds and produces `compile_commands.json`. Same `CMakeLists.txt` serves both targets.
- **Layered CMake with INTERFACE aggregates** — Each layer defined as a CMake `INTERFACE` library (e.g. `Dia::Foundation`) that bundles its modules. Upper layers link against the lower layer aggregate; individual cross-layer includes become impossible without explicit wiring.
- **CMake as analysis overlay only** — Parallel `CMakeLists.txt` tree used only for `compile_commands.json` generation (Ninja). MSBuild stays the primary. Requires keeping both trees in sync — doubles maintenance cost.
- **VS 2022 compile_commands shim** — VS 2022 can export `compile_commands.json` per-project without CMake. Fragile, per-project, incomplete — but zero migration cost for Clang-Tidy on one module.
- **Module boundary enforced via Python tool** — Read YAML `dependencies.forbidden` from module docs, parse `#include` graphs, fail CI on violations. Architecture enforcement without any build system change.
- **Phased migration — Foundation first** — Migrate DiaCore + DiaMaths + DiaGeometry2D only. Validate CMake parity, then work up the dependency tree one layer at a time.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Migration scope** | Full 55 modules · Layer by layer · Foundation only · Tooling only | Full = most value + most cost; foundation-first = lowest risk, proves pattern |
| **VS IDE experience** | CMake-generated `.sln` · Open Folder (no `.sln`) · Keep hand-authored `.sln` | Open Folder loses filter layout; generated `.sln` loses GUID stability |
| **Build backend** | Ninja (fast, `compile_commands.json`) · MSBuild · Both via presets | Ninja is key for Clang-Tidy; MSBuild for VS compat |
| **Architecture enforcement** | CMake `target_link_libraries` · Python include-graph tool · Both | CMake = compile-time enforcement; Python = CI enforcement; both = defense in depth |
| **Layer model** | 7-layer hierarchy · 3-tier (foundation/services/integration) · flat with forbidden-dep checker | Fewer layers = easier migration; more layers = finer-grained enforcement |
| **External deps** | `FetchContent` · `find_package` · `add_subdirectory` · keep in `External/` | bgfx/bx/bimg already in `.diaenv/`; googletest + protobuf have CMakeLists.txt |
| **DiaCLI impact** | Swap to `cmake --build` / `ninja` · Keep `msbuild` calls · Auto-detect | Ninja build faster for iterative; `dia run` must remain stable |

## Known Tradeoffs

- **CMake `target_link_libraries` is the key enforcer.** A module declaring `target_link_libraries(DiaCore PRIVATE DiaCore_Internal)` with `PRIVATE` makes those headers unavailable to consumers. This is exactly what's needed to enforce the current YAML `dependencies.forbidden` at build time.
- **Flat → layered requires breaking changes.** Some modules probably have silent forbidden-dep violations today (the YAML says forbidden but the `.vcxproj` compiles it). A CMake migration will surface these as build errors that must be fixed before the migration can pass.
- **VS solution generation from CMake loses `.vcxproj.filters`** — the custom folder layout in Visual Studio. The generated layout is flat/alphabetical. This is the biggest daily-workflow friction point.
- **`compile_commands.json` requires Ninja or Unix Makefiles** — the VS generator doesn't produce it. A second CMake preset for Ninja is needed alongside the VS preset.
- **55 CMakeLists.txt files is mechanical work.** Each needs: source file list, include dirs, compiler flags, `target_link_libraries`. The YAML module docs already contain most of this information — a code-gen tool could bootstrap it.
- **`Directory.Build.props` logic moves into CMake root.** `/std:c++20`, output dirs, toolset version all need replicating in `CMakeLists.txt` and `CMakePresets.json`. One-time, but necessary.
- **DiaCLI change is forced.** `dia run` calls `msbuild` today. Post-migration it must call `cmake --build` or `ninja`. If a parallel system is kept, DiaCLI must detect or be told which to use.

## Known Pitfalls (C++ / game engine context)

- **Include path ordering** — MSBuild's `AdditionalIncludeDirectories` differs from CMake's `target_include_directories(PUBLIC/PRIVATE/INTERFACE)`. Mis-mapping silently breaks include resolution.
- **Forbidden-dep violations surface as build errors** — good in the long run, painful during migration. Need a plan to audit current violations before starting.
- **Static library link order** — MSBuild's `.sln` drives this implicitly. CMake's `target_link_libraries` must replicate it explicitly.
- **bgfx Shader compilation** — handled separately in `.diaenv/build/`; CMake migration must not absorb or break this path.
- **Protobuf codegen (`DiaProtobuf`)** — custom build step. CMake has `protobuf_generate()` but requires per-module setup.
- **Binary SDK externals** (SFML, CEF, Ultralight, Python311) — need `find_package` wrappers or `ExternalProject_Add`. Not trivial.

## Cluiche-Specific Opportunities

### Existing CMake Footholds

- `.diaenv/build/` already runs CMake for bgfx/bx/bimg — the pattern is proven in-repo.
- `googletest`, `protobuf`, `pybind11`, `websocketpp` all ship `CMakeLists.txt` — can be `add_subdirectory`'d immediately.
- The `dia.*.architecture.module.md` YAML files already list `dependencies.required` and `dependencies.forbidden` for each module. A generator script could bootstrap `CMakeLists.txt` stubs from these.

### Platform Decision Constraints

| Decision | Implication for CMake migration |
|----------|---------------------------------|
| PD-005 — x64 Windows only | `CMAKE_GENERATOR_PLATFORM x64` is sufficient |
| PD-006 — VS project files are source of truth | **Must be updated** — CMake would become the new source of truth |
| PD-007 — C++20 required | `target_compile_features(… cxx_std_20)` replaces `stdcpp20` in `.props` |
| PD-008 — `Directory.Build.props` owns OutDir/IntDir | Replicate via `CMAKE_RUNTIME_OUTPUT_DIRECTORY` etc. in root |
| PD-004 — No STL in public APIs | No build system impact |

## Open Questions for Ideation

- Is the primary deliverable architecture enforcement, or tooling enablement (Clang-Tidy/TSan)?
- Can the migration be done layer by layer (Foundation first), or does it need to be atomic?
- Is the current YAML `dependencies.forbidden` accurate, or are there silent violations today?
- Should the VS IDE experience be preserved via a generated `.sln`, or is Open Folder acceptable?
- Is a code-gen tool (YAML → CMakeLists.txt stubs) worth building to accelerate the migration?
- Should `dia run` continue calling MSBuild during migration, or switch to `cmake --build` immediately?
- Should external deps (bgfx, SFML, CEF, protobuf) be folded into the CMake tree, or remain separate?

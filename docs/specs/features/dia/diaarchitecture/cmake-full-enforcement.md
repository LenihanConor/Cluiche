# Feature Spec: cmake-full-enforcement (C3) — SUPERSEDED

> **This spec has been split into two features:**
> - **C3a:** [cmake-dia-full.md](cmake-dia-full.md) — all 55 Dia modules, additive (`.vcxproj` kept)
> - **C3b:** [cmake-cluiche-apps.md](cmake-cluiche-apps.md) — Cluiche apps + atomic retirement
>
> This file is kept for reference only. Do not implement from it.

---

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaarchitecture.md | **cmake-full-enforcement** |

**Status:** `Superseded` (split into C3a + C3b)
**Research:** docs/research/migrat_cmake/summary.md

---

## Summary

Extend CMake coverage to all remaining Dia modules (Platform, Application, Entity, Assets, Tooling, all four domains) and wire the full layered INTERFACE aggregate targets (`Dia::Platform`, `Dia::Application`, etc.). Once all modules build cleanly under CMake, update `dia run` and `dia pipeline` in DiaCLI to call `cmake --build` instead of `msbuild`, and update PD-006 to reflect that CMake is now the source of truth.

`Cluiche.sln` and all `.vcxproj` files are **retired** as build drivers — they may be kept for reference or deleted by developer preference, but they are no longer maintained.

**Prerequisites:**
- C7 complete (all modules have `layer:`)
- C1 exits 0 for all modules (no violations outstanding)
- C2 complete and `Dia::Foundation` builds cleanly

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | `cmake --build --preset vs2022` builds the full Dia module graph cleanly | No errors |
| AC2 | `cmake --build --preset ninja-debug` builds the full graph and produces `compile_commands.json` with entries for all modules | File has entries for each module |
| AC3 | All CMake INTERFACE aggregate targets exist and resolve correctly: `Dia::Foundation`, `Dia::Platform`, `Dia::Application`, `Dia::Entity`, `Dia::Assets`, `Dia::Tooling`, `Dia::Physics`, `Dia::Physics.Tools`, `Dia::Animation`, `Dia::Animation.Tools`, `Dia::Rendering`, `Dia::Rendering.Backends` | `cmake --build` succeeds with a consumer using each target |
| AC4 | A module with a forbidden dep (e.g. `DiaCore` including `DiaEditor`) **fails to configure** with a clear CMake error | Introduce deliberate violation, confirm configure error |
| AC5 | `dia run cluichetest` uses `cmake --build` and produces a working executable | Manual run |
| AC6 | `dia run googletest` uses `cmake --build` and all tests pass | `dia run googletest` exits 0 |
| AC7 | `dia run cluicheeditor` uses `cmake --build` and launches the editor | Manual run |
| AC8 | PD-006 in `docs/specs/platform/Cluiche.md` is updated: "CMake is the source of truth for build structure" | Spec diff |
| AC9 | `Directory.Build.props` is retired (removed from repo root or scoped to empty no-op) | File deleted or contains `<!-- retired — see CMakePresets.json -->` |
| AC10 | `DiaProtobuf` protobuf codegen works via `protobuf_generate()` in its `CMakeLists.txt` | `dia run cluichetest` works end-to-end |
| AC11 | Binary SDK externals (SFML, CEF, Ultralight, Python311) are found via `find_package` wrappers in `cmake/Find*.cmake` | Configure succeeds on a clean checkout after `dia env setup` |

---

## CMake Structure (full)

### Root layout
```
CMakeLists.txt              ← root, includes all subdirs
CMakePresets.json           ← vs2022 + ninja-debug + release presets
cmake/
  FindSFML.cmake            ← wraps External/SFML
  FindCEF.cmake             ← wraps External/CEF
  FindUltralight.cmake      ← wraps External/Ultralight
  FindPython311.cmake       ← wraps External/Python311
  DiaLayers.cmake           ← defines INTERFACE aggregate targets
Dia/
  DiaCore/CMakeLists.txt
  DiaMaths/CMakeLists.txt
  ... (one per module)
Cluiche/
  CluicheTest/CMakeLists.txt
  CluicheEditor/CMakeLists.txt
  Tests/GoogleTests/CMakeLists.txt
  Stages/DummyStage/CMakeLists.txt
```

### `cmake/DiaLayers.cmake` (INTERFACE aggregates)
```cmake
# Core sub-layer aggregates
add_library(Dia_Foundation INTERFACE) ...  # defined in root (C2)
add_library(Dia_Platform INTERFACE)
target_link_libraries(Dia_Platform INTERFACE Dia::Foundation
    DiaWindow DiaInput DiaThreading DiaMailbox)
add_library(Dia::Platform ALIAS Dia_Platform)

add_library(Dia_Application INTERFACE)
target_link_libraries(Dia_Application INTERFACE Dia::Platform
    DiaApplicationFlow DiaStateMachine)
add_library(Dia::Application ALIAS Dia_Application)

# ... Entity, Assets, Tooling similarly ...

# Domain aggregates
add_library(Dia_Physics INTERFACE)
target_link_libraries(Dia_Physics INTERFACE Dia::Application
    DiaRigidBody2D DiaSoftBody2D)
add_library(Dia::Physics ALIAS Dia_Physics)

add_library(Dia_Physics_Tools INTERFACE)
target_link_libraries(Dia_Physics_Tools INTERFACE Dia::Physics Dia::Tooling
    DiaRigidBody2DVisualDebugger DiaSoftBody2DVisualDebugger)
add_library(Dia::Physics.Tools ALIAS Dia_Physics_Tools)
# ... Animation, Rendering similarly ...
```

### DiaCLI change
`dia run` and `dia pipeline --stage compile-code` currently call:
```python
subprocess.run(["msbuild", solution_path, f"/p:Configuration={config}", ...])
```
After C3, replaced with:
```python
subprocess.run(["cmake", "--build", "--preset", cmake_preset_for(config), ...])
```
`dia run googletest --config Asan` continues to work via the `asan` CMake preset added in DiaBugDetection.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Write `CMakeLists.txt` for all Platform modules (DiaWindow, DiaInput, DiaThreading, DiaMailbox) | 4 files |
| 2 | Write `CMakeLists.txt` for Application modules (DiaApplicationFlow, DiaStateMachine) | 2 files |
| 3 | Write `CMakeLists.txt` for Entity modules (DiaEntity) | 1 file |
| 4 | Write `CMakeLists.txt` for Assets modules (DiaAsset, DiaAssetCatalogue, DiaAssetRuntime, DiaReflect) | 4 files |
| 5 | Write `CMakeLists.txt` for Tooling modules (DiaEditor, DiaAPI, DiaAutomation, DiaWebSocket, DiaDebugServer, DiaDebugProtocol, DiaVisualDebugger, DiaVisualDebuggerConsole, DiaPython) | 9 files |
| 6 | Write `CMakeLists.txt` for DiaProtobuf — wire `protobuf_generate()` codegen | Special: custom codegen step |
| 7 | Write `CMakeLists.txt` for Physics domain (DiaRigidBody2D, DiaSoftBody2D, 2 visual debuggers) | 4 files |
| 8 | Write `CMakeLists.txt` for Animation domain (7 core + 3 tools) | 10 files |
| 9 | Write `CMakeLists.txt` for Rendering domain (4 core + 6 backends) | 10 files |
| 10 | Write `cmake/Find*.cmake` wrappers for SFML, CEF, Ultralight, Python311 | 4 files |
| 11 | Write `cmake/DiaLayers.cmake` with all INTERFACE aggregate targets | |
| 12 | Write `CMakeLists.txt` for application projects (CluicheTest, CluicheEditor, GoogleTests, DummyStage) | 4 files |
| 13 | Add `release` and `asan`/`ubsan` presets to `CMakePresets.json` | Aligns with DiaBugDetection sanitizer-configs spec |
| 14 | Update DiaCLI `run` command to call `cmake --build` | Replace `msbuild` subprocess call |
| 15 | Update DiaCLI `pipeline --stage compile-code` to call `cmake --build` | |
| 16 | Update PD-006 in `docs/specs/platform/Cluiche.md` | "CMake is the source of truth" |
| 17 | Retire `Directory.Build.props` (delete or stub) | |
| 18 | Verify full test suite passes: `dia run googletest` exits 0 | Regression gate |
| 19 | Verify `dia run cluichetest` and `dia run cluicheeditor` launch successfully | Manual verification |

---

## Binding Decisions Compliance

| Decision | How Complied |
|----------|-------------|
| PD-005 — x64 only | All presets set `"architecture": "x64"` or Ninja host (x64) |
| PD-006 — VS project files source of truth | **Updated by this feature** — CMake becomes source of truth |
| PD-007 — C++20 | `target_compile_features(… cxx_std_20)` on every module |
| PD-008 — `Directory.Build.props` owns paths | **Retired** — CMake root owns all output paths via `CMAKE_*_OUTPUT_DIRECTORY` |
| AD-001 — Module YAML frontmatter | YAML docs preserved; `CMakeLists.txt` added alongside |
| AD-003 — `Dia::<Module>::` namespaces | CMake ALIAS targets use `Dia::` prefix matching namespace convention |

---

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should `.vcxproj` files be deleted or kept? | Deleted — keeping them creates a maintenance trap where developers accidentally edit them. A single commit removes all 55. |
| 2 | `Cluiche.sln` — delete or keep? | Delete. Developers use VS Open Folder or the CMake-generated solution from `cmake --preset vs2022`. |
| 3 | What is the CMake minimum version? | 3.25 — required for `CMakePresets.json` v3 and `CONFIGURE_DEPENDS` reliability. `dia env verify` checks for this. |
| 4 | How are bgfx/bx/bimg handled — they are already built by `.diaenv/build/`? | They remain external. `DiaBgfx/CMakeLists.txt` uses a `find_package(bgfx)` wrapper that points to the `.diaenv/build/bgfx/` output — same as today, just described in CMake instead of a vcxproj additional lib dir. |
| 5 | Does retiring `Directory.Build.props` break anything for developers who haven't migrated their local setup? | `dia env setup` and `dia env verify` are updated to check for CMake + Ninja. The `.props` file is removed; any dev with a stale local VS that loads the old `.sln` will get a clean "project not found" error rather than a silent bad build. |

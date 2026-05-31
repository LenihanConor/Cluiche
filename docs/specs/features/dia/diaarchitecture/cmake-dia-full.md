# Feature Spec: cmake-dia-full (C3a)

**Parent:** @docs/specs/systems/dia/diaarchitecture.md
**Status:** `Draft`
**Research:** docs/research/migrat_cmake/summary.md

---

## Summary

Extend CMake coverage to all remaining Dia modules (Platform, Application, Entity, Assets, Tooling, all four domains) and wire the full layered INTERFACE aggregate targets (`Dia::Platform`, `Dia::Application`, etc.). This step is **additive** — all Dia `.vcxproj` files are kept intact, `Cluiche.sln` continues to work via MSBuild, and `dia run` continues to call `msbuild`. The payoff: Dia module architecture violations become CMake configure-time errors, and `compile_commands.json` covers all 55 Dia modules (unblocking Clang-Tidy on the full library).

Cluiche app projects (CluicheTest, GoogleTests, etc.) are **not touched** — they remain MSBuild-only. That migration happens in C3b.

**Prerequisites:**
- C7 complete (all modules have `layer:`)
- C1 exits 0 for all modules (no violations outstanding)
- C2 complete and `Dia::Foundation` builds cleanly

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | `cmake --build --preset vs2022` builds all 55 Dia modules cleanly | No errors |
| AC2 | `cmake --build --preset ninja-debug` builds all 55 Dia modules and produces `compile_commands.json` with entries for every Dia module | File has entries for each module |
| AC3 | All CMake INTERFACE aggregate targets exist and resolve correctly: `Dia::Foundation`, `Dia::Platform`, `Dia::Application`, `Dia::Entity`, `Dia::Assets`, `Dia::Tooling`, `Dia::Physics`, `Dia::Physics.Tools`, `Dia::Animation`, `Dia::Animation.Tools`, `Dia::Rendering`, `Dia::Rendering.Backends` | `cmake --build` succeeds with a consumer linking each target |
| AC4 | A module with a forbidden dep (e.g. `DiaCore` including `DiaEditor`) **fails to configure** with a clear CMake error | Introduce deliberate violation, confirm configure error |
| AC5 | All Dia `.vcxproj` files untouched — `msbuild Cluiche/Cluiche.sln` still builds successfully | MSBuild build exits 0 |
| AC6 | `dia run googletest` continues to work unchanged (MSBuild path) | `dia run googletest` exits 0 |

---

## CMake Structure

### Root `CMakeLists.txt` additions
```cmake
# After Foundation (C2):
add_subdirectory(Dia/DiaWindow)
add_subdirectory(Dia/DiaInput)
add_subdirectory(Dia/DiaThreading)
add_subdirectory(Dia/DiaMailbox)
add_subdirectory(Dia/DiaApplicationFlow)
add_subdirectory(Dia/DiaStateMachine)
add_subdirectory(Dia/DiaEntity)
# ... Assets, Tooling, Domains ...

include(cmake/DiaLayers.cmake)
```

### `cmake/DiaLayers.cmake` (INTERFACE aggregates)
```cmake
add_library(Dia_Foundation INTERFACE) ...  # defined in root (C2)
add_library(Dia_Platform INTERFACE)
target_link_libraries(Dia_Platform INTERFACE Dia::Foundation
    DiaWindow DiaInput DiaThreading DiaMailbox)
add_library(Dia::Platform ALIAS Dia_Platform)

add_library(Dia_Application INTERFACE)
target_link_libraries(Dia_Application INTERFACE Dia::Platform
    DiaApplicationFlow DiaStateMachine)
add_library(Dia::Application ALIAS Dia_Application)

# Entity, Assets, Tooling, Physics, Animation, Rendering similarly...
```

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Write `CMakeLists.txt` for Platform modules (DiaWindow, DiaInput, DiaThreading, DiaMailbox) | 4 files |
| 2 | Write `CMakeLists.txt` for Application modules (DiaApplicationFlow, DiaStateMachine) | 2 files |
| 3 | Write `CMakeLists.txt` for Entity + Assets modules (DiaEntity, DiaAsset, DiaAssetCatalogue, DiaAssetRuntime, DiaReflect) | 5 files |
| 4 | Write `CMakeLists.txt` for Tooling modules (DiaEditor, DiaAPI, DiaAutomation, DiaWebSocket, DiaDebugServer, DiaDebugProtocol, DiaVisualDebugger, DiaVisualDebuggerConsole, DiaPython) | 9 files |
| 5 | Write `CMakeLists.txt` for DiaProtobuf — wire `protobuf_generate()` codegen | Special: custom codegen step |
| 6 | Write `CMakeLists.txt` for Physics domain (DiaRigidBody2D, DiaSoftBody2D, 2 visual debuggers) | 4 files |
| 7 | Write `CMakeLists.txt` for Animation domain (7 core + 3 tools) | 10 files |
| 8 | Write `CMakeLists.txt` for Rendering domain (4 core + 6 backends) | 10 files |
| 9 | Write `cmake/DiaLayers.cmake` with all INTERFACE aggregate targets | |
| 10 | Update root `CMakeLists.txt` to `add_subdirectory` for all new modules | |
| 11 | Extend `CMakePresets.json` with `release` preset | Aligns with DiaBugDetection sanitizer-configs spec |
| 12 | Verify AC4: introduce deliberate violation, confirm configure error, revert | |

---

## Binding Decisions Compliance

| Decision | How Complied |
|----------|-------------|
| PD-005 — x64 only | All presets set `"architecture": "x64"` or Ninja host (x64) |
| PD-006 — VS project files source of truth | **Unchanged** — `.vcxproj` kept; PD-006 updated only at C3b |
| PD-007 — C++20 | `target_compile_features(… cxx_std_20)` on every module |
| AD-001 — Module YAML frontmatter | YAML docs preserved; `CMakeLists.txt` added alongside |
| AD-003 — `Dia::<Module>::` namespaces | CMake ALIAS targets use `Dia::` prefix matching namespace convention |

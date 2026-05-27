# Feature Spec: cmake-foundation-pilot (C2)

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaarchitecture.md | **cmake-foundation-pilot** |

**Status:** `Approved`
**Research:** docs/research/migrat_cmake/summary.md

---

## Summary

Write `CMakeLists.txt` files for the six Foundation sub-layer modules (DiaCore, DiaMaths, DiaGeometry2D, DiaGeometry3D, DiaSerializer, DiaObservation) and a repo-root `CMakeLists.txt` + `CMakePresets.json` with two presets: `vs2022` (Visual Studio 17 generator, daily dev) and `ninja-debug` (Ninja, produces `compile_commands.json` for Clang-Tidy and clangd).

The `.vcxproj` files are **not modified or deleted** — CMake is additive. This pilot proves the pattern, validates `CMakePresets.json` layout, and unlocks `compile_commands.json` for the most critical modules. It is the prerequisite for C3.

**Prerequisite:** C1 audit tool exits 0 for all Foundation modules (no violations to fix before wiring).

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | `cmake --preset vs2022` configures successfully with no errors | Run from repo root |
| AC2 | `cmake --build --preset vs2022` builds all Foundation modules cleanly (Debug + Release) | Build output clean |
| AC3 | `cmake --preset ninja-debug` configures and produces `compile_commands.json` in `Cluiche/out/cmake/ninja-debug/` | File exists and contains entries for DiaCore `.cpp` files |
| AC4 | `cmake --build --preset ninja-debug` builds all Foundation modules cleanly | Build output clean |
| AC5 | All existing GoogleTests that exercise Foundation modules still pass via `dia run googletest` (MSBuild path) | `dia run googletest` exits 0 |
| AC6 | `dia env verify` reports CMake 3.25+ and Ninja as available (or missing with guidance) | |
| AC7 | `CMakePresets.json` is committed at repo root; no generated files committed | `.gitignore` covers `Cluiche/out/cmake/` |
| AC8 | `Directory.Build.props` is untouched — MSBuild path unchanged | Diff shows no changes to `.props` or `.vcxproj` |

---

## CMake Structure

### Root `CMakeLists.txt`
```cmake
cmake_minimum_required(VERSION 3.25)
project(Cluiche CXX)

# Replicate Directory.Build.props settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Cluiche/bin/${PROJECT_NAME}/${CMAKE_BUILD_TYPE}/x64)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Cluiche/bin/sharedlibs/${CMAKE_BUILD_TYPE}/x64)

add_subdirectory(Dia/DiaCore)
add_subdirectory(Dia/DiaMaths)
add_subdirectory(Dia/DiaGeometry2D)
add_subdirectory(Dia/DiaGeometry3D)
add_subdirectory(Dia/DiaSerializer)
add_subdirectory(Dia/DiaObservation)

# Foundation aggregate INTERFACE target
add_library(Dia_Foundation INTERFACE)
target_link_libraries(Dia_Foundation INTERFACE
    DiaCore DiaMaths DiaGeometry2D DiaGeometry3D DiaSerializer DiaObservation)
add_library(Dia::Foundation ALIAS Dia_Foundation)
```

### `CMakePresets.json`
```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "vs2022",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/Cluiche/out/cmake/vs2022"
    },
    {
      "name": "ninja-debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/Cluiche/out/cmake/ninja-debug",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug",
                          "CMAKE_EXPORT_COMPILE_COMMANDS": "ON" }
    }
  ],
  "buildPresets": [
    { "name": "vs2022", "configurePreset": "vs2022" },
    { "name": "ninja-debug", "configurePreset": "ninja-debug" }
  ]
}
```

### Per-module pattern (DiaCore example)
```cmake
# Dia/DiaCore/CMakeLists.txt
file(GLOB_RECURSE DIACORE_SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB_RECURSE DIACORE_HEADERS CONFIGURE_DEPENDS "*.h")

add_library(DiaCore STATIC ${DIACORE_SOURCES} ${DIACORE_HEADERS})
add_library(Dia::Core ALIAS DiaCore)

target_include_directories(DiaCore
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/Dia>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Json/external)

target_compile_features(DiaCore PUBLIC cxx_std_20)
```

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Write root `CMakeLists.txt` with output path settings and Foundation `add_subdirectory` calls | |
| 2 | Write `CMakePresets.json` with `vs2022` and `ninja-debug` presets | `CMAKE_EXPORT_COMPILE_COMMANDS` on ninja-debug only |
| 3 | Write `Dia/DiaCore/CMakeLists.txt` | Include `Json/external` as private include dir |
| 4 | Write `Dia/DiaMaths/CMakeLists.txt` | Links `DiaCore` |
| 5 | Write `Dia/DiaGeometry2D/CMakeLists.txt` | Links `DiaCore`, `DiaMaths` |
| 6 | Write `Dia/DiaGeometry3D/CMakeLists.txt` | Links `DiaCore`, `DiaMaths` |
| 7 | Write `Dia/DiaSerializer/CMakeLists.txt` | Links `DiaCore` |
| 8 | Write `Dia/DiaObservation/CMakeLists.txt` | Links `DiaCore` |
| 9 | Add `Dia::Foundation` INTERFACE aggregate to root | |
| 10 | Add `Cluiche/out/cmake/` to `.gitignore` | |
| 11 | Extend `dia env verify` to check CMake 3.25+ and Ninja availability | |
| 12 | Verify `dia run googletest` (MSBuild) still passes after all changes | Regression gate |

---

## Binding Decisions Compliance

| Decision | How Complied |
|----------|-------------|
| PD-005 — x64 only | `"architecture": "x64"` in `vs2022` preset; Ninja builds default to host arch (x64) |
| PD-006 — VS project files source of truth | Not yet updated — `.vcxproj` kept intact; CMake is additive. PD-006 is updated at C3. |
| PD-007 — C++20 | `target_compile_features(… cxx_std_20)` on every module target |
| PD-008 — `Directory.Build.props` owns OutDir/IntDir | CMake output paths replicate the same layout; `.props` is untouched |

---

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | `GLOB_RECURSE` for source files is convenient but CMake won't auto-detect new files without re-running configure. Is that acceptable? | Yes for the pilot — `CONFIGURE_DEPENDS` flag re-runs configure when the glob changes on Ninja/Makefile generators. For VS generator it's less reliable; can switch to explicit file lists in C3. |
| 2 | Should the Dia::Foundation INTERFACE aggregate use PUBLIC or INTERFACE linkage? | `INTERFACE` — it's a pure aggregation target with no source files of its own. Consumers that link `Dia::Foundation` get all six modules' headers and libs transitively. |
| 3 | Does `compile_commands.json` from the ninja-debug preset include all headers, not just `.cpp` files? | `compile_commands.json` contains one entry per compiled translation unit (`.cpp`). Clang-Tidy follows `#include` chains to reach headers. This is the standard and expected behaviour. |

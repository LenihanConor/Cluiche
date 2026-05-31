# Feature Spec: cmake-cluiche-apps (C3b)

**Parent:** @docs/specs/systems/dia/diaarchitecture.md
**Status:** `Draft`
**Research:** docs/research/migrat_cmake/summary.md

---

## Summary

Complete the CMake migration by writing `CMakeLists.txt` for all four Cluiche app projects (CluicheTest, GoogleTests, CluicheGameBaseline, CluicheEditor), adding `Find*.cmake` wrappers for the binary SDK externals (CEF, Ultralight, bgfx, SDL3, Python311), and switching DiaCLI to call `cmake --build` instead of `msbuild`. Once CI is green through CMake for all targets, delete all `.vcxproj` files and `Cluiche.sln` atomically. Update PD-006.

This step cannot begin until C3a is complete and all Dia modules build cleanly via CMake — deleting Dia `.vcxproj` while Cluiche apps still use `ProjectReference` would break the solution.

**Prerequisites:**
- C3a complete (all 55 Dia modules have `CMakeLists.txt` and build cleanly)
- `cmake --build --preset vs2022` exits 0 for the full Dia graph

---

## Why Cluiche is the Hard Part

Cluiche apps reference Dia libs via MSBuild `ProjectReference` today. In CMake they link `Dia::Application` etc. instead. The complexity is in **external dependency wiring** — bgfx, CEF, Ultralight, SDL3, and Python311 each need a custom `Find*.cmake` wrapper that points to the `.diaenv/build/` or `External/` pre-built outputs. None of these are on the system CMake search path.

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | `cmake --build --preset vs2022` builds CluicheTest, CluicheEditor, GoogleTests, and CluicheGameBaseline cleanly | No errors |
| AC2 | `dia run cluichetest` uses `cmake --build` and produces a working executable | Manual run |
| AC3 | `dia run googletest` uses `cmake --build` and all tests pass | `dia run googletest` exits 0 |
| AC4 | `dia run cluicheeditor` uses `cmake --build` and launches the editor | Manual run |
| AC5 | All `.vcxproj` files deleted (55 Dia + 5 Cluiche app projects) | `find . -name "*.vcxproj"` returns empty |
| AC6 | `Cluiche.sln` deleted | File absent |
| AC7 | PD-006 in `docs/specs/platform/Cluiche.md` updated: "CMake is the source of truth for build structure" | Spec diff |
| AC8 | `Directory.Build.props` retired (removed or stubbed to no-op) | File deleted or contains `<!-- retired — see CMakePresets.json -->` |
| AC9 | Binary SDK externals (bgfx, CEF, Ultralight, Python311) found via `Find*.cmake` wrappers on a clean checkout after `dia env setup` | Configure succeeds |
| AC10 | `DiaProtobuf` codegen works end-to-end: `dia run cluichetest` launches without protobuf errors | Manual run |

---

## CMake Structure

### New files
```
cmake/
  FindSFML.cmake            ← wraps External/SFML
  FindCEF.cmake             ← wraps External/CEF
  FindUltralight.cmake      ← wraps External/Ultralight
  FindPython311.cmake       ← wraps External/Python311
  FindBgfx.cmake            ← wraps External/.diaenv/build/bgfx/
  FindSDL3.cmake            ← wraps External/SDL3/_build/
Cluiche/
  CluicheTest/CMakeLists.txt
  CluicheGameBaseline/CMakeLists.txt
  CluicheEditor/CMakeLists.txt
  Tests/GoogleTests/CMakeLists.txt
```

### DiaCLI change
`dia run` and `dia pipeline --stage compile-code` currently call:
```python
subprocess.run(["msbuild", solution_path, f"/p:Configuration={config}", ...])
```
After C3b, replaced with:
```python
subprocess.run(["cmake", "--build", "--preset", cmake_preset_for(config), ...])
```
`dia run googletest --config Asan` continues to work via the `asan` CMake preset added in DiaBugDetection.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Write `cmake/Find*.cmake` wrappers for bgfx, SDL3, SFML, CEF, Ultralight, Python311 | 6 files; each wraps a pre-built `.diaenv/build/` or `External/` artifact |
| 2 | Write `CMakeLists.txt` for GoogleTests | Links GoogleTest + all tested Dia modules |
| 3 | Write `CMakeLists.txt` for CluicheGameBaseline | |
| 4 | Write `CMakeLists.txt` for CluicheTest | Largest: links rendering, physics, animation, UI etc. |
| 5 | Write `CMakeLists.txt` for CluicheEditor | Links CEF + DiaEditor chain |
| 6 | Update root `CMakeLists.txt` to `add_subdirectory` for all Cluiche apps | |
| 7 | Add `asan` + `ubsan` presets to `CMakePresets.json` | Aligns with DiaBugDetection spec |
| 8 | Update DiaCLI `run` command to call `cmake --build` | Replace `msbuild` subprocess call |
| 9 | Update DiaCLI `pipeline --stage compile-code` to call `cmake --build` | |
| 10 | Verify CI green: `dia run googletest` exits 0 via CMake path | |
| 11 | Atomic deletion commit: remove all `.vcxproj`, `.vcxproj.filters`, `.vcxproj.user`, `Cluiche.sln` | Single commit |
| 12 | Retire `Directory.Build.props` | Delete or stub |
| 13 | Update PD-006 in `docs/specs/platform/Cluiche.md` | |

---

## Binding Decisions Compliance

| Decision | How Complied |
|----------|-------------|
| PD-005 — x64 only | All presets set `"architecture": "x64"` |
| PD-006 — VS project files source of truth | **Updated by this feature** — CMake becomes source of truth |
| PD-007 — C++20 | `target_compile_features(… cxx_std_20)` on every app target |
| PD-008 — `Directory.Build.props` owns OutDir/IntDir | **Retired** — CMake root owns all output paths via `CMAKE_*_OUTPUT_DIRECTORY` |
| AD-001 — Module YAML frontmatter | YAML docs preserved; `.vcxproj` retirement does not affect YAML |

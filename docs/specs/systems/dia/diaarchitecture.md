# System Spec: DiaArchitecture

**Application:** Dia
**Research:** docs/research/migrat_cmake/summary.md
**Status:** `Approved`

---

## Summary

DiaArchitecture formalises the domain-oriented module structure for the Dia engine and enforces it at build time via CMake. The current 55-module flat MSBuild layout has no compile-time enforcement of the dependency rules documented in `dia.*.architecture.module.md` YAML files — violations are invisible until code review. This system introduces a 6-sub-layer Core + 4-domain architecture, an audit tool that surfaces violations immediately, a CMake Foundation pilot that produces `compile_commands.json`, and a full layered CMake enforcement model that makes forbidden-dep violations impossible to compile.

---

## Goals

1. **Document** the target architecture by adding a `layer:` field to every module's YAML doc (C7)
2. **Audit** the current codebase for forbidden-dep violations via `dia check --tool=arch` (C1)
3. **Pilot** CMake for the Foundation sub-layer to prove the pattern and unlock `compile_commands.json` (C2)
4. **Enforce** the Dia module graph via CMake `target_link_libraries` across all 55 modules — additive, `.vcxproj` kept (C3a)
5. **Complete** the migration by adding CMake for Cluiche apps, retiring all `.vcxproj` files, and switching DiaCLI to `cmake --build` (C3b)

---

## Target Architecture

### Core (6 sub-layers — strict bottom-up, no upward reach)

| Sub-layer | Modules |
|-----------|---------|
| `foundation` | DiaCore, DiaMaths, DiaGeometry2D, DiaGeometry3D, DiaSerializer, DiaObservation |
| `platform` | DiaWindow, DiaInput, DiaThreading, DiaMailbox |
| `application` | DiaApplicationFlow, DiaStateMachine |
| `entity` | DiaEntity |
| `assets` | DiaAsset, DiaAssetCatalogue, DiaAssetRuntime |
| `tooling` | DiaEditor, DiaAPI, DiaAutomation, DiaWebSocket, DiaDebugServer, DiaDebugProtocol, DiaVisualDebugger (base/console) |

### Domains (vertical slices — `core` + `tools` tiers per domain)

| Domain | Core modules | Tools modules |
|--------|-------------|---------------|
| `physics` | DiaRigidBody2D, DiaSoftBody2D | DiaRigidBody2DVisualDebugger, DiaSoftBody2DVisualDebugger |
| `animation` | DiaRig2D, DiaIK2D, DiaAnimation2D, DiaMesh3D, DiaRig3D, DiaAnimation3D, DiaSkinning3D | DiaRig2DVisualDebugger, DiaIK2DVisualDebugger, DiaAnimation2DVisualDebugger |
| `rendering` | DiaGraphics, DiaGraphics3D, DiaUI, DiaScene3D | DiaBgfx, DiaBgfx3D, DiaSFML, DiaUICEF, DiaUIUltralight, DiaImGui |
| `ai` | DiaAI, DiaPathfinding *(future)* | DiaAIVisualDebugger *(future)* |

### Hard rules

- Core sub-layers depend only on layers below them — no upward reach
- Domain `core` tiers depend on Core only — never on other domain cores
- Domain `tools` tiers may depend on Core/Tooling + their own domain core
- These rules are enforced at compile time by CMake `target_link_libraries` (after C3a)

---

## Features

| # | Feature | Spec | Status | Size |
|---|---------|------|--------|------|
| C7 | YAML layer formalisation — add `layer:` to all module docs | [layer-formalisation.md](../../features/dia/diaarchitecture/layer-formalisation.md) | Draft | S |
| C1 | Architecture audit tool — `dia check --tool=arch` | [arch-audit-tool.md](../../features/dia/diaarchitecture/arch-audit-tool.md) | Draft | S |
| C2 | Foundation CMake pilot — CMakeLists.txt for Foundation sub-layer | [cmake-foundation-pilot.md](../../features/dia/diaarchitecture/cmake-foundation-pilot.md) | Draft | M |
| C3a | Dia CMake full — CMakeLists.txt for all 55 Dia modules (additive, `.vcxproj` kept) | [cmake-dia-full.md](../../features/dia/diaarchitecture/cmake-dia-full.md) | Draft | L |
| C3b | Cluiche CMake + retirement — apps, Find wrappers, DiaCLI switch, atomic `.vcxproj` deletion | [cmake-cluiche-apps.md](../../features/dia/diaarchitecture/cmake-cluiche-apps.md) | Draft | M |

---

## Responsibilities

- Define and document the canonical layer assignment for every Dia module
- Provide a CI-runnable tool that detects forbidden-dep violations
- Provide CMake build targets that make forbidden-dep violations compile-time errors
- Produce `compile_commands.json` (via Ninja preset) for Clang-Tidy and clangd
- Update PD-006 when CMake becomes the source of truth (C3b)

## Non-Responsibilities

- Code generation of new modules (architecture governance only)
- Refactoring violations found by C1 (those are separate tasks per affected module)
- Linux/WSL2 CI setup (TSan is unblocked by this system but is a separate spec)
- Clang-Tidy rule configuration (separate DiaBugDetection feature)

---

## Public Interfaces

### `dia check --tool=arch`  *(C1)*
```
dia check --tool=arch [--module <module-id>] [--fix-report]
```
- Reads all `dia.*.architecture.module.md` files
- Parses `#include` directives across all `.cpp`/`.h` files in each module
- Reports violations of `dependencies.forbidden` and cross-layer upward reaches
- Exit code 0 = clean, 1 = violations found
- Output: `Cluiche/out/check/arch-violations.txt` + console summary

### CMake targets  *(C2 / C3)*
```cmake
Dia::Foundation          # DiaCore + DiaMaths + DiaGeometry2D/3D + DiaSerializer + DiaObservation
Dia::Platform            # + DiaWindow + DiaInput + DiaThreading + DiaMailbox
Dia::Application         # + DiaApplicationFlow + DiaStateMachine
Dia::Entity              # + DiaEntity
Dia::Assets              # + DiaAsset + DiaAssetCatalogue + DiaAssetRuntime
Dia::Tooling             # + DiaEditor + DiaAPI + DiaAutomation + ...
Dia::Physics             # Physics domain core
Dia::Physics.Tools       # Physics domain tools (visual debuggers)
Dia::Animation           # Animation domain core
Dia::Animation.Tools     # Animation domain tools
Dia::Rendering           # Rendering domain core
Dia::Rendering.Backends  # DiaBgfx + DiaSFML + backend modules
Dia::AI                  # AI domain core (future)
```

### CMakePresets.json presets  *(C2 / C3)*
- `vs2022` — Visual Studio 17 generator, Debug + Release, for daily dev
- `ninja-debug` — Ninja generator, Debug, produces `compile_commands.json`

---

## Dependencies

| Dependency | Why |
|------------|-----|
| DiaCLI | `dia check --tool=arch` and `dia run` (post-C3) wired through DiaCLI |
| All Dia modules | C7 touches every module doc; C3a adds CMakeLists.txt alongside; C3b retires `.vcxproj` |
| CMake 3.25+ | `CMakePresets.json` v3 requires CMake 3.25 |
| Ninja | Required for `compile_commands.json` preset |

---

## Binding Decisions Compliance

| Decision | Source | How This System Complies |
|----------|--------|--------------------------|
| PD-001 — StringCRC for IDs | Platform | `layer:` field values are plain strings in YAML/CMake target names — no runtime StringCRC needed; enforcement is build-time only |
| PD-004 — No STL in public APIs | Platform | Build system and audit tool are Python/CMake — no C++ public API touches STL |
| PD-005 — x64 Windows only | Platform | CMakePresets use `"architecture": "x64"`; no cross-compile targets added |
| PD-006 — VS project files are source of truth | Platform | **Updated by C3b**: CMake becomes the new source of truth; PD-006 amended when C3b ships |
| PD-007 — C++20 required | Platform | `target_compile_features(… cxx_std_20)` replaces `stdcpp20` in `Directory.Build.props`; enforced in CMake root |
| PD-008 — `Directory.Build.props` owns OutDir/IntDir | Platform | C3 replicates all output path logic in CMake root (`CMAKE_RUNTIME_OUTPUT_DIRECTORY` etc.); `Directory.Build.props` is retired for CMake-managed projects |
| AD-001 — Module YAML frontmatter | Application | C7 extends YAML schema with `layer:` field; all existing fields preserved |
| AD-003 — `Dia::<Module>::` namespaces | Application | CMake `ALIAS` targets use `Dia::` prefix matching existing namespace convention |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | C1 audit tool | How should the tool handle `External/` includes — they are not Dia modules and should not be flagged | Exclude paths under `External/` and system headers from violation detection; only flag includes that resolve to another Dia module directory |
| 2 | C3 migration | What happens to `Directory.Build.props` when CMake takes over? | CMake root replicates all settings; `Directory.Build.props` is kept but scoped only to any remaining non-CMake projects; retired when all projects are migrated |
| 3 | C2 pilot | Should `.vcxproj` files for Foundation modules be deleted during the pilot or kept? | Kept — CMake is additive during C2; `.vcxproj` files are only retired in C3 |
| 4 | C3 VS experience | Will developers lose the `.vcxproj.filters` folder layout they use in VS? | Yes — CMake-generated `.vcxproj.filters` are flat/alphabetical. Mitigation: use VS Open Folder (CMake Tools) as the preferred IDE mode, or add a `source_group` pass to the CMake generator |
| 5 | C1 violations | What if C1 reveals widespread forbidden-dep violations that block C3? | C1 findings are tracked as a separate fix task before C3 starts; C3 spec has a prerequisite gate: `dia check --tool=arch` must exit 0 |
| 6 | DiaCLI switch | When does `dia run` stop calling `msbuild` and start calling `cmake --build`? | At C3 completion; until then `dia run` continues using `msbuild`; C3 feature spec gates on DiaCLI update |
| 7 | Layer assignment | Are the layer assignments in the summary locked, or can they shift during C7? | Locked for the modules in scope; C7 may surface a small number of contested assignments (e.g. DiaStateMachine in `application` vs `entity`) which are flagged as decisions in the C7 spec |
| 8 | Protobuf codegen | DiaProtobuf has a `.proto` codegen custom build step — how is this handled in CMake? | `protobuf_generate()` CMake helper replaces the MSBuild custom step; wired in DiaProtobuf's CMakeLists.txt during C3 |

# System Spec: DiaArchitecture

**Application:** Dia
**Research:** docs/research/migrat_cmake/summary.md
**Status:** `Approved`
**Plan:** [diaarchitecture.plan.md](diaarchitecture.plan.md)

---

## Summary

DiaArchitecture formalises the domain-oriented module structure for the Dia engine. The current 61-module flat MSBuild layout has no compile-time enforcement of the dependency rules documented in `dia.*.architecture.module.md` YAML files — violations are invisible until code review. This system introduces a numbered-layer architecture with strict dependency ordering, an audit tool that surfaces violations immediately, and module splits that create clean architectural boundaries.

---

## Goals

1. **Document** the target architecture by adding a `layer:` field to every module's YAML doc (C7)
2. **Audit** the current codebase for forbidden-dep violations via `dia check --tool=arch` (C1)

---

## Target Architecture

### Numbered Layers

```
Level 3 — Domains (cross-domain forbidden, within-domain OK)
  Visual    (3.0 core / 3.1 tools) — Rendering, UI, Scene
  Physics   (3.0 core / 3.1 tools)
  Animation (3.0 core / 3.1 tools)
  [Future: AI (3.0 core / 3.1 tools)]

Level 2 — Assets (2.0 core / 2.1 tools)

Level 1 — Foundation
  Application  (1.2) — DiaApplicationFlow, DiaAutomation, DiaGame
  Platform     (1.2) — DiaWindow, DiaInput, DiaSDL
  Maths        (1.1) — DiaMaths, DiaGeometry2D/3D, DiaGeometryBridge, DiaGeometry2DPicking
  Services     (1.1) — DiaObservation, DiaMetrics, DiaDebugProtocol, DiaWebSocket, DiaAPI,
                        DiaDebugServer, DiaEditor, DiaDebugDraw, DiaStreams, DiaPython, DiaImGui
  Core         (1.0) — DiaCore, DiaSerializer, DiaThreading, DiaMailbox, DiaStateMachine,
                        DiaProtobuf, DiaPicking, DiaFileIO, DiaJson
```

### Hard Rules

1. **No upward deps** — a module at level N cannot depend on anything at level N+1 or above
2. **Sub-levels are ordered** — 1.0 < 1.1 < 1.2; a module can depend on same or lower sub-level
3. **Same sub-level, different group → forbidden** — Maths (1.1) cannot use Services (1.1) and vice versa
4. **Same sub-level, same group → allowed** — DiaDebugServer → DiaWebSocket within Services is fine
5. **Level-3 domains are independent** — Visual, Physics, Animation cannot cross-depend at core tier
6. **Within-domain deps OK** — DiaBgfx → DiaUI → DiaGraphics all within Visual is fine
7. **No cross-domain exception** — DiaDebugDraw at Services (1.1) provides abstract debug submission; DiaVisualDebugRenderer in Visual (3.1) provides the concrete rendering. Domain VDs depend only on DiaDebugDraw.

### Level 3 — Domains

| Domain | Core (3.0) | Tools (3.1) |
|--------|-----------|-------------|
| **Visual** | DiaGraphics, DiaGraphics3D, DiaBgfx, DiaBgfx3D, DiaUI, DiaUICEF, DiaUIUltralight, DiaScene2D, DiaScene3D, DiaCamera2D, DiaLighting2D | DiaVisualDebugRenderer, DiaVisualDebuggerConsole, DiaScene2DVisualDebugger, DiaSceneEditor, DiaGeometry2DVisualDebugger |
| **Physics** | DiaRigidBody2D, DiaSoftBody2D | DiaRigidBody2DVisualDebugger, DiaSoftBody2DVisualDebugger |
| **Animation** | DiaRig2D, DiaIK2D, DiaAnimation2D, DiaRig3D, DiaAnimation3D, DiaSkinning3D | DiaRig2DVisualDebugger, DiaIK2DVisualDebugger, DiaAnimation2DVisualDebugger |
| **AI** *(future)* | DiaAI, DiaPathfinding | DiaAIVisualDebugger |

### Level 2 — Assets

| Tier | Modules |
|------|---------|
| Core (2.0) | DiaEntity, DiaAsset, DiaAssetCatalogue, DiaAssetRuntime, DiaMesh3D |
| Tools (2.1) | DiaAssetCatalogueEditor, DiaEntityInspector, DiaBlueprintEditor, DiaPipelineEditor, DiaApplicationEditor, DiaAssetRuntimeInspector, DiaEntityVisualDebugger, DiaAssetRuntimeVisualDebugger |

### Level 1 — Foundation

| Sub-level | Group | Modules |
|-----------|-------|---------|
| 1.2 | Application | DiaApplicationFlow, DiaAutomation, DiaGame |
| 1.2 | Platform | DiaWindow, DiaInput, DiaSDL |
| 1.1 | Maths | DiaMaths, DiaGeometry2D, DiaGeometry3D, DiaGeometryBridge, DiaGeometry2DPicking |
| 1.1 | Services | DiaObservation, DiaMetrics, DiaDebugProtocol, DiaWebSocket, DiaAPI, DiaDebugServer, DiaEditor, DiaDebugDraw, DiaStreams, DiaPython, DiaImGui |
| 1.0 | Core | DiaCore, DiaSerializer, DiaThreading, DiaMailbox, DiaStateMachine, DiaProtobuf, DiaPicking, DiaFileIO, DiaJson |

### VS Solution Folders

The `.sln` file uses numbered solution folders to reflect the architecture:
- `1.0-Core`, `1.1-Maths`, `1.1-Services`, `1.2-Platform`, `1.2-Application`
- `2.0-Assets`, `2.1-Assets-Tools`
- `3.0-Visual`, `3.1-Visual-Tools`, `3.0-Physics`, `3.1-Physics-Tools`, `3.0-Animation`, `3.1-Animation-Tools`

---

## Refactoring Actions

These are required to make the layered architecture valid. Each is a separate task within C7 (layer-formalisation).

| # | Action | Type | Detail |
|---|--------|------|--------|
| R1 | **DiaFileIO** | Split from DiaCore | Extract FilePath/, AsyncFileLoader, FileWatcher, StreamReader/Writer → new module at 1.0. Computation modules (Maths, Physics, Animation) stop depending on filesystem. |
| R2 | **DiaJson** | Split from DiaCore | Extract Json/ (jsoncpp wrapper) → new module at 1.0. Computation modules stop depending on JSON parsing. |
| R3 | **DiaStreams** | Split from DiaApplicationFlow | Extract Streams/ (ServiceStream, FrameStream, EventStream, StreamRegistry) → new module at 1.1 Services. Modules that just publish/subscribe stop depending on the full app lifecycle. |
| R4 | **TextureHandler → DiaBgfx** | Move | TextureHandler moves from DiaAssetRuntime to DiaBgfx. DiaAssetRuntime keeps abstract IAssetTypeHandler interface; app registers concrete handler at startup. DiaAssetRuntime drops DiaBgfx ProjectReference. |
| R5 | **DiaDebugServer dep inversion** | Refactor | Remove DiaApplicationFlow ProjectReference from DiaDebugServer. DiaApplicationFlow registers with DiaDebugServer via IDebugStateProvider at startup. |
| R6 | **DiaDebugDraw split** | Split from DiaVisualDebugger | New DiaDebugDraw at 1.1 Services — abstract shape/line/text submission API. DiaVisualDebugger becomes DiaVisualDebugRenderer at Visual (3.1) — consumes DiaDebugDraw, renders via DiaGraphics. Domain VDs depend only on DiaDebugDraw (1.1). |

---

## Features

| # | Feature | Spec | Status | Size |
|---|---------|------|--------|------|
| C7 | Layer formalisation — assign `layer:` to all modules + refactoring actions | [layer-formalisation.md](../../features/dia/diaarchitecture/layer-formalisation.md) | Approved | M |
| C1 | Architecture audit tool — `dia check --tool=arch` | [arch-audit-tool.md](../../features/dia/diaarchitecture/arch-audit-tool.md) | Approved | S |

---

## Responsibilities

- Define and document the canonical layer assignment for every Dia module
- Provide a CI-runnable tool that detects forbidden-dep violations

## Non-Responsibilities

- Code generation of new modules (architecture governance only)
- CMake migration (dropped — MSBuild/.vcxproj remains the build system)
- Linux/WSL2 CI setup (TSan remains a separate spec)
- Clang-Tidy rule configuration (separate DiaBugDetection feature)
- Physical directory reorganization (modules stay flat under `Dia/`; solution folders provide the visual hierarchy)

---

## Public Interfaces

### `dia check --tool=arch`  *(C1)*
```
dia check --tool=arch [--module <module-id>] [--summary]
```
- Reads all `dia.*.architecture.module.md` files
- Parses `#include` directives across all `.cpp`/`.h` files in each module
- Reports violations of `dependencies.forbidden` and cross-layer upward reaches
- Exit code 0 = clean, 1 = violations found
- Output: `Cluiche/out/check/arch-violations.txt` + console summary

---

## Dependencies

| Dependency | Why |
|------------|-----|
| DiaCLI | `dia check --tool=arch` wired through DiaCLI |
| All Dia modules | C7 touches every module doc |
| PyYAML | Already in DiaCLI venv; used by C1 to parse module YAML |

---

## Binding Decisions Compliance

| Decision | Source | How This System Complies |
|----------|--------|--------------------------|
| PD-006 — VS project files are source of truth | Platform | No change — MSBuild/.vcxproj remains the build system; this system is YAML + Python tooling only |
| AD-001 — Module YAML frontmatter | Application | C7 extends the existing schema with `layer:`; all existing fields preserved |

---

## Open Design Questions

| # | Question | Notes |
|---|----------|-------|
| 1 | Should DiaFileIO and DiaJson be split in a single pass or sequentially? | Sequential is safer (DiaJson may depend on DiaFileIO for loading); split DiaFileIO first. |
| 2 | Does DiaStreams need DiaObservation, or vice versa? | Need to verify — if circular, DiaStreams may need to sit at 1.0 instead of 1.1. |
| 3 | When DiaDebugDraw is extracted, does the submission buffer live in DiaDebugDraw or DiaVisualDebugRenderer? | Buffer should be in DiaDebugDraw so renderers are pure consumers. |

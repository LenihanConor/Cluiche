# Feature Spec: layer-formalisation (C7)

**Parent:** @docs/specs/systems/dia/diaarchitecture.md
**Status:** `Approved`
**Note:** CMake features (C2, C3a, C3b) dropped — MSBuild remains the build system. C7 + C1 are the full scope of DiaArchitecture.
**Research:** docs/research/migrat_cmake/summary.md

---

## Summary

Assign a `layer:` field to every `dia.*.architecture.module.md` YAML file using the numbered-layer architecture, and execute 6 refactoring actions to make the layering valid. This is the prerequisite for C1 (the audit tool reads `layer:` to enforce ordering rules).

---

## Layer Value Reference

Valid `layer:` values and their permitted dependencies:

| Value | Level | Permitted dependencies |
|-------|-------|----------------------|
| `foundation/core` | 1.0 | Nothing else in Dia |
| `foundation/maths` | 1.1 | `foundation/core` only |
| `foundation/services` | 1.1 | `foundation/core` only (not `foundation/maths`) |
| `foundation/platform` | 1.2 | `foundation/core`, `foundation/maths`, `foundation/services` |
| `foundation/application` | 1.2 | `foundation/core`, `foundation/maths`, `foundation/services` (not `foundation/platform`) |
| `assets/core` | 2.0 | All Foundation (1.x) |
| `assets/tools` | 2.1 | `assets/core` + all Foundation (1.x) |
| `domain/visual/core` | 3.0 | All Foundation (1.x) + `assets/core` |
| `domain/visual/tools` | 3.1 | `domain/visual/core` + all Foundation (1.x) + `assets/core` |
| `domain/physics/core` | 3.0 | All Foundation (1.x) + `assets/core` |
| `domain/physics/tools` | 3.1 | `domain/physics/core` + `foundation/services` (DiaDebugDraw) + all Foundation (1.x) + `assets/core` |
| `domain/animation/core` | 3.0 | All Foundation (1.x) + `assets/core` |
| `domain/animation/tools` | 3.1 | `domain/animation/core` + `foundation/services` (DiaDebugDraw) + all Foundation (1.x) + `assets/core` |
| `domain/ai/core` | 3.0 | All Foundation (1.x) + `assets/core` |
| `domain/ai/tools` | 3.1 | `domain/ai/core` + `foundation/services` (DiaDebugDraw) + all Foundation (1.x) + `assets/core` |

**Key constraint:** Domain tools tiers reach DiaDebugDraw (in `foundation/services`) for abstract debug submission — never directly into `domain/visual/*`.

---

## Canonical Layer Assignments

### Foundation — Core (1.0)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaCore | `foundation/core` | Base containers, CRC, types, memory, reflection |
| DiaSerializer | `foundation/core` | JSON/binary serialization |
| DiaThreading | `foundation/core` | Thread primitives + JobSystem |
| DiaMailbox | `foundation/core` | Message passing |
| DiaStateMachine | `foundation/core` | State machine framework |
| DiaProtobuf | `foundation/core` | Protocol buffers |
| DiaPicking | `foundation/core` | Pick event routing (deps: DiaCore, DiaMailbox) |
| DiaFileIO | `foundation/core` | **NEW** — file path, async loader, watcher, stream reader/writer (split from DiaCore) |
| DiaJson | `foundation/core` | **NEW** — jsoncpp wrapper (split from DiaCore) |

### Foundation — Maths (1.1)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaMaths | `foundation/maths` | Vectors, matrices, transforms |
| DiaGeometry2D | `foundation/maths` | 2D shapes, intersection |
| DiaGeometry3D | `foundation/maths` | 3D shapes |
| DiaGeometryBridge | `foundation/maths` | 2D↔3D conversion |
| DiaGeometry2DPicking | `foundation/maths` | Geometry intersection queries |

### Foundation — Services (1.1)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaObservation | `foundation/services` | Logging, tracing, metrics, health |
| DiaMetrics | `foundation/services` | Metrics collection |
| DiaDebugProtocol | `foundation/services` | Debug message protocol |
| DiaWebSocket | `foundation/services` | WebSocket server/client |
| DiaAPI | `foundation/services` | REST API framework |
| DiaDebugServer | `foundation/services` | Debug server (registration-based, no app dep) |
| DiaEditor | `foundation/services` | Editor plugin registration framework |
| DiaDebugDraw | `foundation/services` | **NEW** — abstract debug shape/line/text submission (split from DiaVisualDebugger) |
| DiaStreams | `foundation/services` | **NEW** — ServiceStream, FrameStream, EventStream (split from DiaApplicationFlow) |
| DiaPython | `foundation/services` | Python bindings |
| DiaImGui | `foundation/services` | ImGui integration |

### Foundation — Platform (1.2)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaWindow | `foundation/platform` | Window management |
| DiaInput | `foundation/platform` | Input handling |
| DiaSDL | `foundation/platform` | SDL3 platform layer |

### Foundation — Application (1.2)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaApplicationFlow | `foundation/application` | PU/Module/Phase framework (streams extracted) |
| DiaAutomation | `foundation/application` | Automation/scripting hooks |
| DiaGame | `foundation/application` | Game/app manifest serialization |

### Assets — Core (2.0)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaEntity | `assets/core` | ECS: domains, entities, components, hierarchy |
| DiaAsset | `assets/core` | Asset type definitions |
| DiaAssetCatalogue | `assets/core` | Asset manifest/registry |
| DiaAssetRuntime | `assets/core` | Asset loading/lifecycle (DiaBgfx dep removed) |
| DiaMesh3D | `assets/core` | 3D mesh asset type |

### Assets — Tools (2.1)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaAssetCatalogueEditor | `assets/tools` | Asset manifest editor plugin |
| DiaEntityInspector | `assets/tools` | Entity state inspector plugin |
| DiaBlueprintEditor | `assets/tools` | Blueprint/template editor plugin |
| DiaPipelineEditor | `assets/tools` | Build pipeline editor plugin |
| DiaApplicationEditor | `assets/tools` | App config editor plugin |
| DiaAssetRuntimeInspector | `assets/tools` | Asset loading state inspector |
| DiaEntityVisualDebugger | `assets/tools` | Entity debug viz (uses DiaDebugDraw) |
| DiaAssetRuntimeVisualDebugger | `assets/tools` | Asset state debug viz (uses DiaDebugDraw) |

### Domain — Visual Core (3.0)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaGraphics | `domain/visual/core` | Abstract rendering interfaces |
| DiaGraphics3D | `domain/visual/core` | 3D rendering interfaces |
| DiaBgfx | `domain/visual/core` | bgfx GPU backend (+ TextureHandler) |
| DiaBgfx3D | `domain/visual/core` | bgfx 3D backend |
| DiaUI | `domain/visual/core` | UI widget framework |
| DiaUICEF | `domain/visual/core` | CEF browser UI backend |
| DiaUIUltralight | `domain/visual/core` | Ultralight UI backend |
| DiaScene2D | `domain/visual/core` | 2D scene graph |
| DiaScene3D | `domain/visual/core` | 3D scene graph |
| DiaCamera2D | `domain/visual/core` | 2D camera system |
| DiaLighting2D | `domain/visual/core` | 2D lighting |

### Domain — Visual Tools (3.1)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaVisualDebugRenderer | `domain/visual/tools` | **RENAMED** from DiaVisualDebugger — concrete debug rendering |
| DiaVisualDebuggerConsole | `domain/visual/tools` | ImGui debug console |
| DiaScene2DVisualDebugger | `domain/visual/tools` | Scene debug viz |
| DiaSceneEditor | `domain/visual/tools` | Scene placement editor |
| DiaGeometry2DVisualDebugger | `domain/visual/tools` | Geometry debug viz |

### Domain — Physics Core (3.0)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaRigidBody2D | `domain/physics/core` | Rigid body simulation |
| DiaSoftBody2D | `domain/physics/core` | Soft body simulation |

### Domain — Physics Tools (3.1)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaRigidBody2DVisualDebugger | `domain/physics/tools` | Rigid body debug viz |
| DiaSoftBody2DVisualDebugger | `domain/physics/tools` | Soft body debug viz |

### Domain — Animation Core (3.0)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaRig2D | `domain/animation/core` | 2D skeletal rig |
| DiaIK2D | `domain/animation/core` | 2D inverse kinematics |
| DiaAnimation2D | `domain/animation/core` | 2D animation playback |
| DiaRig3D | `domain/animation/core` | 3D skeletal rig |
| DiaAnimation3D | `domain/animation/core` | 3D animation playback |
| DiaSkinning3D | `domain/animation/core` | 3D mesh skinning |

### Domain — Animation Tools (3.1)

| Module | `layer:` value | Notes |
|--------|---------------|-------|
| DiaRig2DVisualDebugger | `domain/animation/tools` | Rig debug viz |
| DiaIK2DVisualDebugger | `domain/animation/tools` | IK debug viz |
| DiaAnimation2DVisualDebugger | `domain/animation/tools` | Animation debug viz |

### Deprecated

| Module | Notes |
|--------|-------|
| DiaSFML | Being replaced by DiaSDL — no layer assignment |

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | Every `dia.*.architecture.module.md` file contains a `layer:` field with a value from the reference table above | `grep -rL "^layer:" Dia/**/*.md` returns empty |
| AC2 | No module has an unrecognised `layer:` value | `dia check --tool=arch --validate-layers` exits 0 (or manual scan) |
| AC3 | All existing YAML fields are preserved unchanged | Diff shows only `layer:` additions/corrections |
| AC4 | DiaAssetRuntime has no DiaBgfx ProjectReference | vcxproj diff |
| AC5 | DiaDebugServer has no DiaApplicationFlow ProjectReference | vcxproj diff |
| AC6 | DiaDebugDraw exists as a separate module with abstract submission API | New vcxproj + module.md |
| AC7 | DiaFileIO exists as a separate module | New vcxproj + module.md |
| AC8 | DiaJson exists as a separate module | New vcxproj + module.md |
| AC9 | DiaStreams exists as a separate module | New vcxproj + module.md |
| AC10 | `dia check --tool=arch` reports no upward-dep violations | Exit 0 |

---

## Tasks

### Layer field updates (documentation)

| # | Task | Notes |
|---|------|-------|
| 1 | Set `layer:` on Foundation/Core modules (DiaCore, DiaSerializer, DiaThreading, DiaMailbox, DiaStateMachine, DiaProtobuf, DiaPicking) | 7 files → `foundation/core` |
| 2 | Set `layer:` on Foundation/Maths modules (DiaMaths, DiaGeometry2D, DiaGeometry3D, DiaGeometryBridge, DiaGeometry2DPicking) | 5 files → `foundation/maths` |
| 3 | Set `layer:` on Foundation/Services modules (DiaObservation, DiaMetrics, DiaDebugProtocol, DiaWebSocket, DiaAPI, DiaDebugServer, DiaEditor, DiaPython, DiaImGui) | 9 files → `foundation/services` |
| 4 | Set `layer:` on Foundation/Platform modules (DiaWindow, DiaInput, DiaSDL) | 3 files → `foundation/platform` |
| 5 | Set `layer:` on Foundation/Application modules (DiaApplicationFlow, DiaAutomation, DiaGame) | 3 files → `foundation/application` |
| 6 | Set `layer:` on Assets/Core modules (DiaEntity, DiaAsset, DiaAssetCatalogue, DiaAssetRuntime, DiaMesh3D) | 5 files → `assets/core` |
| 7 | Set `layer:` on Assets/Tools modules (DiaAssetCatalogueEditor, DiaEntityInspector, DiaBlueprintEditor, DiaPipelineEditor, DiaApplicationEditor, DiaAssetRuntimeInspector, DiaEntityVisualDebugger, DiaAssetRuntimeVisualDebugger) | 8 files → `assets/tools` |
| 8 | Set `layer:` on Visual domain (DiaGraphics, DiaGraphics3D, DiaBgfx, DiaBgfx3D, DiaUI, DiaUICEF, DiaUIUltralight, DiaScene2D, DiaScene3D, DiaCamera2D, DiaLighting2D + tools) | ~16 files |
| 9 | Set `layer:` on Physics domain (DiaRigidBody2D, DiaSoftBody2D + visual debuggers) | 4 files |
| 10 | Set `layer:` on Animation domain (DiaRig2D, DiaIK2D, DiaAnimation2D, DiaRig3D, DiaAnimation3D, DiaSkinning3D + visual debuggers) | ~9 files |

### Refactoring actions (code changes)

| # | Task | Notes |
|---|------|-------|
| R1 | Split DiaFileIO from DiaCore | Extract FilePath/, AsyncFileLoader, FileWatcher, StreamReader/Writer. New vcxproj + module.md at `foundation/core`. Update DiaCore vcxproj to remove files + add ProjectReference to DiaFileIO. |
| R2 | Split DiaJson from DiaCore | Extract Json/ (jsoncpp wrapper). New vcxproj + module.md at `foundation/core`. Update DiaCore vcxproj. |
| R3 | Split DiaStreams from DiaApplicationFlow | Extract Streams/ (ServiceStream, FrameStream, EventStream, StreamRegistry). New vcxproj + module.md at `foundation/services`. Update DiaApplicationFlow vcxproj. |
| R4 | Move TextureHandler to DiaBgfx | Move TextureHandler class from DiaAssetRuntime to DiaBgfx. DiaAssetRuntime keeps IAssetTypeHandler interface. Remove DiaBgfx ProjectReference from DiaAssetRuntime.vcxproj. App registers handler at startup. |
| R5 | Invert DiaDebugServer → DiaApplicationFlow dep | Remove DiaApplicationFlow ProjectReference from DiaDebugServer.vcxproj. DiaApplicationFlow registers with DiaDebugServer via IDebugStateProvider at startup. |
| R6 | Split DiaDebugDraw from DiaVisualDebugger | New DiaDebugDraw at `foundation/services` — abstract shape/line/text submission API + buffer. Rename DiaVisualDebugger → DiaVisualDebugRenderer at `domain/visual/tools`. Update all domain VDs to depend on DiaDebugDraw instead of DiaVisualDebugger. |

---

## Binding Decisions

| Decision | How Complied |
|----------|-------------|
| PD-006 — VS project files source of truth | .vcxproj changes limited to ProjectReference updates and new module creation; no build-system migration |
| AD-001 — Module YAML frontmatter | `layer:` field uses new numbered scheme; all other existing fields preserved |

---

## Open Design Questions

| # | Question | Notes |
|---|----------|-------|
| 1 | DiaMesh3D is in `assets/core` but is consumed by animation (skinning). Should it be `domain/animation/core`? | Mesh data is an asset type (load from disk, reference counted). Animation consumes it from level 3 looking down at level 2 — this works. |
| 2 | Does DiaStreams need DiaObservation, or vice versa? | If circular, DiaStreams may need to sit at `foundation/core` (1.0) instead of `foundation/services` (1.1). Verify during R3 implementation. |
| 3 | DiaReflect stays in DiaCore — should it have its own layer tag within DiaCore's module.md? | No — it's a sub-directory, not a module boundary. DiaCore is `foundation/core` as a whole. |

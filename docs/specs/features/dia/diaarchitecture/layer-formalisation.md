# Feature Spec: layer-formalisation (C7)

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaarchitecture.md | **layer-formalisation** |

**Status:** `Approved`
**Research:** docs/research/migrat_cmake/summary.md

---

## Summary

Add a `layer:` field to every `dia.*.architecture.module.md` YAML file, recording each module's canonical position in the domain-oriented architecture. This is a documentation-only change — no code, no build system changes. It is the prerequisite for C1 (the audit tool reads `layer:` to enforce ordering rules) and C3 (the CMake generator uses `layer:` to assign modules to INTERFACE aggregates).

---

## Layer Value Reference

Valid `layer:` values and their meaning:

| Value | Sub-layer | Permitted dependencies |
|-------|-----------|----------------------|
| `core/foundation` | Foundation | Nothing in Dia |
| `core/platform` | Platform | `core/foundation` only |
| `core/application` | Application | `core/foundation`, `core/platform` |
| `core/entity` | Entity | `core/foundation`, `core/platform`, `core/application` |
| `core/assets` | Assets | `core/foundation`, `core/platform`, `core/application` |
| `core/tooling` | Tooling | All Core sub-layers |
| `domain/physics/core` | Physics core | Core only |
| `domain/physics/tools` | Physics tools | Core + `domain/physics/core` |
| `domain/animation/core` | Animation core | Core only |
| `domain/animation/tools` | Animation tools | Core + `domain/animation/core` |
| `domain/rendering/core` | Rendering core | Core only |
| `domain/rendering/backends` | Rendering backends | Core + `domain/rendering/core` |
| `domain/rendering/tools` | Rendering tools | Core + `domain/rendering/core` |
| `domain/ai/core` | AI core | Core only |
| `domain/ai/tools` | AI tools | Core + `domain/ai/core` |

---

## Canonical Layer Assignments

| Module | `layer:` value |
|--------|---------------|
| DiaCore | `core/foundation` |
| DiaMaths | `core/foundation` |
| DiaGeometry2D | `core/foundation` |
| DiaGeometry3D | `core/foundation` |
| DiaGeometryBridge | `core/foundation` |
| DiaSerializer | `core/foundation` |
| DiaObservation | `core/foundation` |
| DiaMetrics | `core/foundation` |
| DiaWindow | `core/platform` |
| DiaInput | `core/platform` |
| DiaThreading | `core/platform` |
| DiaMailbox | `core/platform` |
| DiaApplicationFlow | `core/application` |
| DiaStateMachine | `core/application` |
| DiaEntity | `core/entity` |
| DiaAsset | `core/assets` |
| DiaAssetCatalogue | `core/assets` |
| DiaAssetRuntime | `core/assets` |
| DiaReflect | `core/assets` |
| DiaEditor | `core/tooling` |
| DiaAPI | `core/tooling` |
| DiaAutomation | `core/tooling` |
| DiaWebSocket | `core/tooling` |
| DiaDebugServer | `core/tooling` |
| DiaDebugProtocol | `core/tooling` |
| DiaVisualDebugger | `core/tooling` |
| DiaVisualDebuggerConsole | `core/tooling` |
| DiaRigidBody2D | `domain/physics/core` |
| DiaSoftBody2D | `domain/physics/core` |
| DiaRigidBody2DVisualDebugger | `domain/physics/tools` |
| DiaSoftBody2DVisualDebugger | `domain/physics/tools` |
| DiaRig2D | `domain/animation/core` |
| DiaIK2D | `domain/animation/core` |
| DiaAnimation2D | `domain/animation/core` |
| DiaMesh3D | `domain/animation/core` |
| DiaRig3D | `domain/animation/core` |
| DiaAnimation3D | `domain/animation/core` |
| DiaSkinning3D | `domain/animation/core` |
| DiaRig2DVisualDebugger | `domain/animation/tools` |
| DiaIK2DVisualDebugger | `domain/animation/tools` |
| DiaAnimation2DVisualDebugger | `domain/animation/tools` |
| DiaGraphics | `domain/rendering/core` |
| DiaGraphics3D | `domain/rendering/core` |
| DiaUI | `domain/rendering/core` |
| DiaScene3D | `domain/rendering/core` |
| DiaBgfx | `domain/rendering/backends` |
| DiaBgfx3D | `domain/rendering/backends` |
| DiaSFML | `domain/rendering/backends` |
| DiaUICEF | `domain/rendering/backends` |
| DiaUIUltralight | `domain/rendering/backends` |
| DiaImGui | `domain/rendering/backends` |
| DiaProtobuf | `core/tooling` |
| DiaPython | `core/tooling` |

---

## Acceptance Criteria

| ID | Criterion | Verification |
|----|-----------|-------------|
| AC1 | Every `dia.*.architecture.module.md` file contains a `layer:` field with a value from the reference table above | `grep -rL "^layer:" Dia/**/*.md` returns empty |
| AC2 | No module has an unrecognised `layer:` value | `dia check --tool=arch --validate-layers` exits 0 (or manual scan) |
| AC3 | Contested assignments (if any) are recorded as decisions in this spec before marking Approved | Review section at bottom of spec |
| AC4 | All existing YAML fields are preserved unchanged | Diff shows only `layer:` additions |

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `layer:` to Foundation modules (DiaCore, DiaMaths, DiaGeometry2D/3D, DiaGeometryBridge, DiaSerializer, DiaObservation, DiaMetrics) | 7 files |
| 2 | Add `layer:` to Platform modules (DiaWindow, DiaInput, DiaThreading, DiaMailbox) | 4 files |
| 3 | Add `layer:` to Application modules (DiaApplicationFlow, DiaStateMachine) | 2 files |
| 4 | Add `layer:` to Entity + Assets modules (DiaEntity, DiaAsset, DiaAssetCatalogue, DiaAssetRuntime, DiaReflect) | 5 files |
| 5 | Add `layer:` to Tooling modules (DiaEditor, DiaAPI, DiaAutomation, DiaWebSocket, DiaDebugServer, DiaDebugProtocol, DiaVisualDebugger, DiaVisualDebuggerConsole, DiaProtobuf, DiaPython) | 10 files |
| 6 | Add `layer:` to Physics domain (DiaRigidBody2D, DiaSoftBody2D, both visual debuggers) | 4 files |
| 7 | Add `layer:` to Animation domain (DiaRig2D, DiaIK2D, DiaAnimation2D, DiaMesh3D, DiaRig3D, DiaAnimation3D, DiaSkinning3D, 3 visual debuggers) | 10 files |
| 8 | Add `layer:` to Rendering domain (DiaGraphics, DiaGraphics3D, DiaUI, DiaScene3D, DiaBgfx, DiaBgfx3D, DiaSFML, DiaUICEF, DiaUIUltralight, DiaImGui) | 10 files |

---

## Binding Decisions Compliance

| Decision | How Complied |
|----------|-------------|
| PD-006 — VS project files source of truth | No change to `.vcxproj` files — YAML-only edit |
| AD-001 — Module YAML frontmatter | `layer:` extends the existing schema; all existing fields preserved |

---

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | DiaReflect is in `core/assets` — is that right, or does it belong in `core/foundation`? | It serialises assets and depends on types above Foundation; `core/assets` is correct. If it later drops to Foundation-only deps it can be promoted. |
| 2 | DiaProtobuf is in `core/tooling` — it's also a serialization format used at runtime. Should it be `core/assets`? | Protobuf is used for network/editor protocol, not for game asset loading. `core/tooling` is correct. |
| 3 | DiaMesh3D is in `domain/animation/core` but it's also a rendering concern. | Mesh data (vertices, indices, bones) is consumed by animation (skinning) before it reaches the renderer. The renderer gets a skinned draw command, not a mesh. `domain/animation/core` is correct. |

# Feature Spec: drawer-boundary-consistency

**Parent:** @docs/specs/systems/dia/diavisualdebugger.md

---

## Summary

Enforces the engine/application layer boundary for all visual debugger drawers. Moves generic drawers from CluicheTest into their corresponding Dia visual debugger modules, refactors test-specific drawers to consume engine-level drawers instead of duplicating their logic, and normalises naming across all drawer files.

**Problem solved:** Several drawers in `CluicheTest/Modules/TestStages/Drawers/` implement generic debug visualization capability that any game could use (scene overview, AABB overlay, shape labels). Meanwhile, `Animation2DTestDrawer` hand-rolls skeleton rendering that already exists in `DiaRig2DVisualDebugger`. This violates the layer boundary rule and creates duplication.

---

## Goals

1. Every drawer that visualizes generic engine state lives in the corresponding `Dia*VisualDebugger` module
2. CluicheTest drawers only contain test-scenario-specific wiring + ImGui
3. No duplicated rendering logic between test drawers and engine drawers
4. Consistent naming across all drawer files and layer names

---

## Scope

### In scope
- Move `Scene2DTestDrawer` → new `DiaScene2DVisualDebugger` module
- Extract generic AABB overlay logic → `AABBOverlayDrawer` in `DiaGeometry2DVisualDebugger`
- Extract generic shape labels logic → `ShapeLabelsDrawer` in `DiaGeometry2DVisualDebugger`
- Refactor CluicheTest `Geometry2DAABBDrawer` + `Geometry2DLabelsDrawer` to thin wrappers
- Refactor `Animation2DTestDrawer` to delegate to `BoneLinesDrawer` + `JointCirclesDrawer`
- Naming consistency pass across all drawers

### Out of scope
- `EntityTestDrawer` — depends on test-only components; stays as-is until `DiaEntityVisualDebugger` exists
- `Geometry2DShapesDrawer` — already correctly delegates to engine `ShapeDrawer`
- `Geometry2DIntersectionsDrawer` — test-only scenario visualization

---

## Binding Decisions

- SD-DBG-001: Stack of focused draw classes — new drawers are single-purpose
- SD-DBG-014: Same-family classes share vcxproj — new geometry drawers go in existing `DiaGeometry2DVisualDebugger.vcxproj`; scene drawer gets new `DiaScene2DVisualDebugger.vcxproj`

---

## Open Design Questions

1. **Scene2DTestDrawer references `TransformComponent`** — This is a CluicheTest-only component. The generic `SceneOverviewDrawer` should not depend on it. Entity rendering should be removed from the engine drawer (entities are drawn by entity-specific drawers) or accept a position callback/lambda.

2. **AABBOverlayDrawer API** — Should it use the submit-per-frame pattern (like `ShapeDrawer`) or hold const references to shapes (like the current test drawer)? Submit-per-frame is more reusable.

---

## Status

`Approved`

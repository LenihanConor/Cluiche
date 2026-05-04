# Plan: DiaVisualDebugger

**Spec:** @docs/specs/systems/dia/diavisualdebugger.md  
**Status:** In Progress  
**Started:** 2026-05-04  
**Last Updated:** 2026-05-04

---

## Overview

11 feature specs, all Approved. Three foundation features must complete in order before any draw class stacks can begin. Features 4–8 can then parallelise. Console (9) and editor panel (10) require the manager. Animation stack (11) is last.

### Dependency diagram

```
1. debug-budget
2. debug-text-primitive      (requires debug-budget)
3. debug-layer-manager       (requires debug-budget, debug-text-primitive)
     │
     ├── 4. rig2d-visual-debugger-stack
     ├── 5. rigidbody2d-visual-debugger-stack
     ├── 6. softbody2d-visual-debugger-stack
     ├── 7. ik2d-visual-debugger-stack
     ├── 8. geometry2d-visual-debugger-stack
     ├── 9. debug-console
     ├── 10. debug-editor-panel
     └── 11. animation2d-visual-debugger-stack
```

Features 4–11 all require features 1–3. Features 4–11 have no dependencies on each other.

---

## Feature Tasks

| # | Feature | Status | Model | Notes |
|---|---------|--------|-------|-------|
| 1 | **debug-budget** | Done | sonnet | 19 tests passing. `kCapacity` on `DebugFrameData`, `entityId` on `DebugPrimitive`, all 7 `RequestDraw*` guarded. |
| 2 | **debug-text-primitive** | Done | sonnet | 15 tests passing. **SFML is v3** — `sf::Font::getDefaultFont()` removed; using static Arial font instead. |
| 3 | **debug-layer-manager** | Done | sonnet | 24 tests passing. `inline const` used for LayerNames (StringCRC not constexpr). `BroadcastLayerState` is a stub — wired in feature 10. |
| 4 | **rig2d-visual-debugger-stack** | Done | sonnet | 27 tests passing. Old VisualDebugger removed. No CluicheTest call sites needed updating. |
| 5 | **rigidbody2d-visual-debugger-stack** | Done | sonnet | 25 tests passing. Old class removed. `PhysicsAABBDrawer` uses `ComputeWorldAABB()` from `WorldShapeUtil.h` (no `GetAABB()` on Body2DBase). |
| 6 | **softbody2d-visual-debugger-stack** | Done | sonnet | 20 tests passing. Cloth uses flat-to-2D index conversion. `PrimitiveCapture` helper used in tests to avoid MSVC stack issues. |
| 7 | **ik2d-visual-debugger-stack** | Done | sonnet | 19 tests passing. 6 IKSolver accessors added. `DIA_ASSERT(mRootTransformSet)` in `GetWorldTransforms()`. |
| 8 | **geometry2d-visual-debugger-stack** | Done | sonnet | 16 tests passing. 4 concrete spatial drawers (template .inl pattern). Added traversal accessors to SpatialGrid, Quadtree, BVH, HexGrid. |
| 9 | **debug-console** | Not Started | opus | 12 tasks — new DiaImGui.vcxproj + DiaVisualDebuggerConsole.vcxproj; SFML imgui backend; ImGui overlay; external imgui source; requires #1–3 |
| 10 | **debug-editor-panel** | Not Started | sonnet | 9 tasks — DebugLayerPanelPlugin + proto extension + CEF UI; requires #3 |
| 11 | **animation2d-visual-debugger-stack** | Done | sonnet | 22 tests passing. 11 accessors added across AnimationEvaluator, PoseBlendStack, SpringChain. boneId field added to NodeState. |

### Model rationale

- **Sonnet** for all draw stack features: pattern is clear, the spec fully codes the draw logic, and the work is file-by-file C++ with known patterns.
- **Opus** for debug-console (#9): this is the architecturally complex feature — new external dependency (imgui), two new vcxprojs, backend abstraction design, SFML render loop integration, and several "must verify at implementation time" ambiguities in the spec that require judgment. Opus handles ambiguity better.
- All other features: Sonnet. The designs are fully resolved in the specs.

---

## Parallelisation notes

- **Phase 1 (serial):** #1 → #2 → #3 (strict dependency chain; ~3 sessions)
- **Phase 2 (parallel):** #4, #5, #6, #7, #8, #9, #10, #11 can all run in parallel once #3 is done
  - Practical limit: run #9 (console/imgui) separately as it touches DiaSFML and requires external source setup
  - #4–#8 and #11 are pure new-file work with no shared file edits — safe to parallelize
  - #10 (editor panel) touches the proto file and CluicheEditor startup — run isolated

---

## Key implementation notes

- **Audit before remove (features 4 and 5):** both spec task 0 requires grepping `CluicheTest/` for old class usage before deleting old files. Do not skip.
- **`inline constexpr` on LayerNames constants** (from debug-layer-manager AI review Q6): use `inline constexpr Dia::Core::StringCRC` — not `static const` — to avoid ODR violations.
- **`DebugFrameData::kCapacity`** (from debug-budget AI review Q5): move `kDebugPrimitiveCapacity` to `DebugFrameData::kCapacity`. Update the one reference in `TestRigidBody2DVisualDebugger.cpp`.
- **`GetDebugPrimitive(uint32_t index) const`** (from debug-budget AI review Q2): add test accessor pair to `DebugFrameData` — needed by EntityId round-trip test and by DebugLayerManager overflow logging.
- **SFML is version 3** (discovered during debug-text-primitive): `sf::Font::getDefaultFont()` was removed. All SFML font usage must load a font explicitly. imgui-sfml version must support SFML 3.
- **imgui version** (debug-console AI review Q6): SFML 3 changes the imgui-sfml version needed — use imgui-sfml that targets SFML 3, not 2.6+.
- **BroadcastLayerState call frequency** (debug-editor-panel AI review Q4): broadcast only when `mLayersDirty == true` (set by Register/Enable/Disable/Unregister). Clear after broadcast.
- **Leaf detection in JointCirclesDrawer** (rig2d AI review Q3): stack-allocate `bool isLeaf[kMaxBones] = {}` inside `Draw()`, mark parents in one pass, then draw pass.
- **BoneLabelsDrawer font clamp** (rig2d AI review Q5): `fontSize = std::min(12.0f * scale, 24.0f)` — prevents runaway text at extreme scales.
- **SpatialStructureDrawer** (geometry2d AI review Q2): use four concrete non-template classes (`SpatialGridDrawer<T>`, `QuadtreeDrawer<T>`, etc.) rather than one base template with runtime dispatch.
- **`DIA_ASSERT(mRootTransformSet)`** in `IKSolver::GetWorldTransforms()` (ik2d AI review Q5) — catches misconfigured call order.

---

## Session Notes

### 2026-05-04
- Plan created from all 11 Approved feature specs.
- Model column added: Sonnet for all draw stacks; Opus for debug-console.
- Implementation not yet started.

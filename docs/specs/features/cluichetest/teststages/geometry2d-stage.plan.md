# Implementation Plan: Geometry2DStage

## Spec
@docs/specs/features/cluichetest/teststages/geometry2d-stage.md

## Session Notes — Spec Decisions Summary

**Key constraints from the full spec chain (Platform → App → System → Feature):**
- All IDs use `StringCRC` — stage `"TestGeometry2D"`, module `"TestGeometry2DStageModule"`, checkpoint `"geometry2d.passed"`, metrics `"shape_count"` / `"intersection_pair_count"` (PD-001)
- No STL in public module APIs; Dia types throughout (PD-004)
- Every new `.h/.cpp` must be added to its `.vcxproj` and `.vcxproj.filters` (PD-006)
- C++20 required (PD-007)
- Stage scaffolded via `/new-cluichetest-stage TestGeometry2D` — **post-simplification** version (4 touch points, not 8)
- `TestGeometry2DStageModule` lives on **MainPU** — AutomationModule lives on MainPU; SimPU modules cannot depend on it
- ~~`VisualDebuggerModule` on SimPU — mandatory to prevent render starvation~~ → **Obsolete after simplification Task #4** (FrameStream auto-flush); still optional for debug draw support
- `TestResultsRegistry::SetRunning()` **mandatory** in DoStart — HUD reads `activeStageName` from it
- `TestResultsRegistry::SetActiveFrameCount()` **mandatory** each DoUpdate tick
- Transitions live in `cluiche_main.diaapp`'s `stages` array — **NOT** in `.diastage`; Boot transitions auto-derived after simplification Task #3
- ~~`pipeline.toml` must have stage in `asset_stages`~~ → **Obsolete after simplification Tasks #1 + #5** (auto-derived from catalogue + .diagame imports)
- ~~Force-copy `cluiche_main.diaapp` to bin~~ → **Obsolete after simplification Task #6** (pipeline detects staleness)
- Arc/Sector/OORect/Capsule rendered via ConvexPolygon tessellation helpers (8/8/4/12 verts)
- 5 draw layers, each a separate `IVisualDebugger` class: geo2d.shapes, geo2d.labels, geo2d.intersections, geo2d.spatial_overlay, geo2d.aabbs
- Drawers registered with `DebugLayerManager::Register()` on first DoUpdate; unregistered in DoStop
- 6 intersection pairs: Circle/Circle (HIT), Circle/AARect (MISS), AARect/Triangle (HIT), Ray/Circle (HIT), Line/AARect (MISS), Circle/Triangle (MISS)
- 4 spatial structures: BVH, Quadtree, SpatialGrid, HexGrid — existing drawers available (`BVHDrawer`, `QuadtreeDrawer`, `SpatialGridDrawer`, `HexGridDrawer`)
- Total shape budget: ~40 shapes/frame — well under ShapeDrawer's 128-shape cap
- Debug Console title is generic **"Debug Console"**; domain tab **"Geometry2D"** active; three collapsible sections: Draw Layers, Geometry Stats, Spatial Stats
- Checkpoints in a **separate** ImGui panel (shared infra from VisualDebugger module), not inside the console
- HUD bar matches RigidBody2D pattern: stage name | checkpoint badge | frame counter | PASS | exit button

---

## Implementation Patterns

### Task 2 — Scaffold (`/new-cluichetest-stage TestGeometry2D`)

> **BLOCKED ON:** `stage-scaffold-simplification.plan.md` (Tasks 1–7). Run this task **after** the simplification lands and the skill is updated. The post-simplification skill reduces touch points from 8→4 and eliminates pipeline.toml edits, force-copy, and mandatory VisualDebuggerModule on SimPU.

Post-simplification, the skill generates:
1. `Assets/Stages/TestGeometry2D/test_geometry2d_stage.diastage`
2. `Assets/Stages/Geometry2D/misc/ApplicationFlow/geometry2d_stage.diaapp` (no VisualDebuggerModule needed — FrameStream auto-flushes)
3. `CluicheTest/Modules/TestStages/TestGeometry2DStageModule.h/.cpp` (skeleton)
4. `CluicheTest.vcxproj` + `.vcxproj.filters` (add module)
5. `cluiche_main.diaapp` (stage entry + HUD — Boot transitions auto-derived, no force-copy needed)
6. `cluichetest.diagame` (import)
7. `assets.catalogue.json` (stage + manifest entries)

**No longer needed:** `pipeline.toml` edit, `asset_stages` entry, force-copy of `cluiche_main.diaapp` to bin.

After: `dia pipeline --target cluichetest --config Debug` must pass and stage visible in Boot menu.

### Task 3 — Tessellation Helpers

Free functions in `Dia::Geometry2DVisualDebugger` namespace:

```cpp
// Dia/DiaGeometry2DVisualDebugger/ArcDrawHelper.h
namespace Dia::Geometry2DVisualDebugger {
    Dia::Geometry2D::ConvexPolygon ToConvexPolygon(const Dia::Geometry2D::Arc& arc, int segments = 8);
}
// Same pattern: SectorDrawHelper (8+2 verts), OORectDrawHelper (4 verts), CapsuleDrawHelper (12 verts)
```

Each helper returns a `ConvexPolygon` — caller passes to `ShapeDrawer::SubmitConvexPoly()`. No vtable, no new IVisualDebugger subclass. Add all `.h/.cpp` files to `DiaGeometry2DVisualDebugger.vcxproj`.

### Task 4 — Drawers (5 classes)

Each drawer inherits `Dia::Debug::IVisualDebugger`, follows the RigidBody2D pattern:

```cpp
class Geometry2DShapesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    Geometry2DShapesDrawer(/* references to shape data */, const Dia::Debug::DebugLayerManager& mgr);
    Dia::Core::StringCRC GetLayerName() const override; // "geo2d.shapes"
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;  // slider: "Point size"
};
```

| Drawer | Layer name | Draw content | ImGui controls |
|--------|-----------|-------------|----------------|
| `Geometry2DShapesDrawer` | `geo2d.shapes` | All 10 gallery shapes via ShapeDrawer | Point size slider |
| `Geometry2DLabelsDrawer` | `geo2d.labels` | ImGui text at world→screen projected positions | Font scale slider |
| `Geometry2DIntersectionsDrawer` | `geo2d.intersections` | 6 pairs drawn red (HIT) or green (MISS) | Hit/miss colour pickers |
| `Geometry2DSpatialOverlayDrawer` | `geo2d.spatial_overlay` | Delegates to existing BVHDrawer/QuadtreeDrawer/SpatialGridDrawer/HexGridDrawer | Per-structure toggle checkboxes |
| `Geometry2DAABBDrawer` | `geo2d.aabbs` | AABBs around all shapes (off by default) | Draw mode radio: Outline/Fill |

Drawers live in `Cluiche/CluicheTest/Modules/TestStages/Drawers/` — CluicheTest-side, not Dia-side.

Registration in module (first DoUpdate):
```cpp
void TestGeometry2DStageModule::RegisterDrawers()
{
    auto* mgr = Cluiche::AppFlow::VisualDebuggerModule::GetStaticLayerManager();
    mShapesDrawer = std::make_unique<Geometry2DShapesDrawer>(/*...*/);
    mgr->Register(mShapesDrawer.get(), 20);
    // ... same for all 5 drawers, priorities 20-24
}
```

Unregister in DoStop:
```cpp
void TestGeometry2DStageModule::UnregisterDrawers()
{
    auto* mgr = Cluiche::AppFlow::VisualDebuggerModule::GetStaticLayerManager();
    if (mShapesDrawer) { mgr->Unregister(mShapesDrawer->GetLayerName()); mShapesDrawer.reset(); }
    // ... same for all 5
}
```

### Task 5 — Module Logic

Fill the skeleton `DoStart`/`DoUpdate`/`DoStop`:

**DoStart:**
1. `DIA_LOG_INFO` entry
2. `SetupGallery()` — position 10 shapes at fixed world coordinates matching mockup layout
3. `SetupIntersectionPairs()` — define 6 pairs, position them in middle band
4. `SetupSpatialStructures()` — create BVH<Circle>, Quadtree, SpatialGrid, HexGrid; insert ~8 shapes each
5. `EvaluateIntersections()` — run `IntersectionTests` on 6 pairs, cache bool results
6. `RegisterCheckpoints()` — register `"geometry2d.passed"` checkpoint
7. `TestResultsRegistry::SetRunning(...)` — mandatory for HUD
8. Return `StartResult::kReady`

**DoUpdate:**
1. `++mFrameCount; TestResultsRegistry::SetActiveFrameCount(mFrameCount)`
2. First frame only: `RegisterDrawers()`
3. First frame only: `mCheckpointPassed = true; TestResultsRegistry::SetPassed(...)`

**DoStop:**
1. `DIA_LOG_INFO` entry
2. `UnregisterCheckpoints()` via AutomationService
3. `UnregisterDrawers()`
4. Reset counters
5. Return `StopResult::kDone`

### Task 6 — REPL Commands

Register with `CommandRegistry` (same pattern as RigidBody2D):
```cpp
registry.Register("dia.geometry2d.shape_count", [this]() { return mShapeCount; });
registry.Register("dia.geometry2d.intersection_pairs", [this]() { return json with hits/misses; });
```

### Task 7 — Pytest Scenario

```python
def test_geometry2d_stage(cluichetest):
    cluichetest.navigate_to("TestGeometry2D")
    cluichetest.wait_for_checkpoint("geometry2d.passed", timeout_frames=10)
    assert cluichetest.get_metric("shape_count") > 0
    assert cluichetest.get_metric("intersection_pair_count") == 6
    cluichetest.navigate_to("Boot")
    # Determinism: second run
    cluichetest.navigate_to("TestGeometry2D")
    cluichetest.wait_for_checkpoint("geometry2d.passed", timeout_frames=10)
    cluichetest.navigate_to("Boot")
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `geometry2d-stage.mockup.html` | Visual sign-off | Done | sonnet | Approved 2026-05-27 |
| 2 | `/new-cluichetest-stage TestGeometry2D` — scaffold all 8 touch points | `dia pipeline` green; stage in Boot menu | Todo | sonnet | |
| 3 | Add tessellation helpers to `DiaGeometry2DVisualDebugger`; update vcxproj | Unit tests: vertex count + convexity | Todo | sonnet | Parallel with Task 2 |
| 4 | Create 5 drawers in `CluicheTest/Modules/TestStages/Drawers/`; update CluicheTest vcxproj | Clean build; drawers register/unregister | Todo | sonnet | After Task 3 |
| 5 | Implement `TestGeometry2DStageModule` logic (setup + drawers + checkpoint) | Stage loads; checkpoint fires; drawers visible | Todo | sonnet | After Tasks 2 + 4 |
| 6 | Wire REPL commands | Commands return correct values | Todo | sonnet | After Task 5 |
| 7 | Write pytest scenario | Orchestrator passes | Todo | sonnet | After Task 5 |
| 8 | `dia run cluichetest` — visual verify against mockup | All layers toggleable in Debug Console | Todo | sonnet | After Tasks 5 + 6 |
| 9 | Commit + update spec status → Done | — | Todo | haiku | After Tasks 7 + 8 |

## Dependencies

```
Task 1 (mockup)     → DONE
Task 2 (scaffold)   ─┐
Task 3 (helpers)    ─┤ parallel
                     │
Task 4 (drawers)   ← Task 3
Task 5 (module)    ← Tasks 2 + 4
Task 6 (REPL)      ← Task 5
Task 7 (pytest)    ← Task 5
Task 8 (verify)    ← Tasks 5 + 6
Task 9 (commit)    ← Tasks 7 + 8
```

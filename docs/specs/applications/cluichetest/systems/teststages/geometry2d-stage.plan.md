# Implementation Plan: Geometry2DStage

**Spec:** @docs/specs/applications/cluichetest/systems/teststages/geometry2d-stage.md
**Status:** In Progress

---

## Key Constraints

- All IDs use `StringCRC` — stage `"Geometry2DTestStage"`, module `"Geometry2DTestStageModule"`, checkpoint `"test.geometry2d.passed"`, metrics `"shape_count"` / `"intersection_pair_count"`
- No STL in public module APIs; Dia types throughout
- Every new `.h/.cpp` must be added to its `.vcxproj` and `.vcxproj.filters`
- Module inherits `TestStageModuleBase` (handles frame counting, timeout, checkpoint lifecycle, HUD integration)
- Module lives on **MainPU** — no SimPU modules needed (FrameStream auto-flush)
- Scaffold via `dia scaffold stage Geometry2D --budget 60` (script handles all 9 touch points)
- Drawers registered with `stageTag` → auto-deregister on stage exit, no manual cleanup
- Arc/Sector/OORect/Capsule rendered via ConvexPolygon tessellation helpers (8/8/4/12 verts)
- 6 intersection pairs: Circle/Circle (HIT), Circle/AARect (MISS), AARect/Triangle (HIT), Ray/Circle (HIT), Line/AARect (MISS), Circle/Triangle (MISS)
- 4 spatial structures: BVH, Quadtree, SpatialGrid, HexGrid
- Total shape budget: ~40 shapes/frame — well under ShapeDrawer's 128-shape cap

---

## Implementation Patterns

### Task 2 — Scaffold

```bash
dia scaffold stage Geometry2D --budget 60
```

Script creates all 9 touch points:
1. `.diastage` at `Assets/Stages/Geometry2DTestStage/`
2. `.diaapp` in stage's `Misc/ApplicationFlow/` (MainPU only, no SimPU modules)
3. `Geometry2DTestStageModule.h/.cpp` skeleton (inherits `TestStageModuleBase`)
4. `CluicheTest.vcxproj` + `.vcxproj.filters`
5. `cluiche_main.diaapp` (stage entry + HUD)
6. `cluichetest.diagame` (import)
7. `assets.catalogue.json` (stage + manifest entries)

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

Registration in `OnUpdate` (first frame, lazy init with stageTag):
```cpp
#ifdef DIA_DEBUG
if (!mShapesDrawer)
{
    if (auto* mgr = Cluiche::AppFlow::VisualDebuggerModule::GetStaticLayerManager())
    {
        const Dia::Core::StringCRC stageTag(GetStageName());

        mShapesDrawer = std::make_unique<Geometry2DShapesDrawer>(/*...*/);
        mgr->Register(mShapesDrawer.get(), 20, stageTag);
        // ... same for all 5 drawers, priorities 20-24
    }
}
#endif
```

No manual unregister in `OnStop` — `stageTag` auto-deactivates on stage exit.

### Task 5 — Module Logic

Module inherits `TestStageModuleBase`. Override the virtuals:

```cpp
class Geometry2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit Geometry2DTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;       // "Geometry2DTestStage"
    unsigned int GetBudgetFrames() const override;             // 60
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;

    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;
    // AreDependenciesReady() — not needed, no external module deps

private:
    void SetupGallery();
    void SetupIntersectionPairs();
    void SetupSpatialStructures();
    void EvaluateIntersections();

    // Gallery shapes (10 primitives) ...
    // Spatial structures ...
    // Drawers (DIA_DEBUG) ...
    int mShapeCount = 0;
    int mIntersectionPairCount = 0;
};
```

**OnStart(service):**
1. `SetupGallery()` — position 10 shapes at fixed world coordinates matching mockup layout
2. `SetupIntersectionPairs()` — define 6 pairs, position them in middle band
3. `SetupSpatialStructures()` — create BVH<Circle>, Quadtree, SpatialGrid, HexGrid; insert ~8 shapes each
4. `EvaluateIntersections()` — run `IntersectionTests` on 6 pairs, cache bool results
5. Register checkpoint via `service->RegisterCheckpoint(...)` with lambda returning cached results

**OnUpdate(deltaTime):**
1. First frame: register drawers (lazy init, `#ifdef DIA_DEBUG`)
2. First frame: `ReportPassed()` — static scene, nothing to wait for

**OnStop:**
1. Reset shape/spatial data (drawers auto-deactivate via stageTag)

### Task 6 — REPL Commands

Register with `CommandRegistry` (same pattern as RigidBody2D):
```cpp
registry.Register("dia.geometry2d.shape_count", [this]() { return mShapeCount; });
registry.Register("dia.geometry2d.intersection_pairs", [this]() { return json with hits/misses; });
```

### Task 7 — Pytest Scenario

```python
def test_geometry2d_stage(cluichetest):
    cluichetest.navigate_to("Geometry2DTestStage")
    cluichetest.wait_for_checkpoint("test.geometry2d.passed", timeout_frames=10)
    assert cluichetest.get_metric("shape_count") > 0
    assert cluichetest.get_metric("intersection_pair_count") == 6
    cluichetest.navigate_to("Boot")
    # Determinism: second run
    cluichetest.navigate_to("Geometry2DTestStage")
    cluichetest.wait_for_checkpoint("test.geometry2d.passed", timeout_frames=10)
    cluichetest.navigate_to("Boot")
```

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `geometry2d-stage.mockup.html` | Visual sign-off | Done | sonnet | Approved 2026-05-27 |
| 2 | `dia scaffold stage Geometry2D --budget 60` | `dia pipeline` green; stage in Boot menu | Done | haiku | Trivial — script handles all 9 files |
| 3 | Add tessellation helpers to `DiaGeometry2DVisualDebugger`; update vcxproj | Unit tests: vertex count + convexity | Done | sonnet | Arc/Sector via halfAngle math; OORect via corner pts; Capsule 12-vert loop |
| 4 | Create 4 drawers in `CluicheTest/Modules/TestStages/Drawers/`; update CluicheTest vcxproj + link | Clean build; drawers compile | Done | sonnet | SpatialOverlayDrawer dropped (spatial drawers are templates); added DiaGeometry2DVisualDebugger to link dependencies |
| 5 | Implement `Geometry2DTestStageModule` logic (setup + drawers + checkpoint) | Stage loads; checkpoint fires; drawers visible | Done | sonnet | ReportPassed() on first frame; Gauges registered for orchestrator; REPL commands registered |
| 6 | Wire REPL commands + Gauges | Commands return correct values | Done | sonnet | dia.geometry2d.shape_count + intersection_pairs as REPL; cluichetest.geometry2d.* as Gauges |
| 7 | Write pytest scenario | Orchestrator passes | Done | sonnet | smoke.py + get_metric() added to DiaClient |
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

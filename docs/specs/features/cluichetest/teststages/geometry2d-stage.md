# Feature Spec: Geometry2DStage

## Parent System
@docs/specs/systems/cluichetest/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007, PD-009, PD-010 |
| Application | @docs/specs/applications/cluichetest.md | AD-001, AD-002, AD-003, AD-005 |
| System | @docs/specs/systems/cluichetest/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Sibling | @docs/specs/features/cluichetest/teststages/test-stage-infrastructure.md | Inherits Shared ACs 1–9; depends on Boot menu + manifest loader |

---

## Problem Statement

No visual exercise path exists for `DiaGeometry2D` shapes, intersection tests, or spatial acceleration structures. Unit tests cover correctness but cannot catch rendering regressions or visually confirm that all shape types are wired to the draw path. A gallery stage closes that gap.

---

## Template Answers

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature under test | `DiaGeometry2D` — all primitive shapes, `IntersectionTests`, BVH/Quadtree/SpatialGrid; `DiaGeometry2DVisualDebugger` — `ShapeDrawer`, spatial renderers |
| T2 | Scene layout | Three sections: (1) gallery grid of one instance per shape type, (2) 6 intersection pairs with colour-coded hit/miss, (3) spatial structure overlay (BVH, Quadtree, SpatialGrid, HexGrid) drawn over a compact shape set |
| T3 | Acceptance criteria | Shared ACs 1–9 + stage-specific ACs AC-G1 through AC-G7 (see below) |
| T4 | Metrics | `shape_count` (total shapes submitted to ShapeDrawer per frame) and `intersection_pair_count` (pairs evaluated in DoStart) |
| T5 | PU assignment | `TestGeometry2DStageModule` on **MainPU** (AutomationModule lives on MainPU; SimPU modules cannot depend on it). `VisualDebuggerModule` on SimPU writing to `SimToRender` to prevent render starvation. |
| T6 | Assets | No textures. Stage scaffolded via `/new-cluichetest-stage TestGeometry2D`: `.diastage` at `Assets/Stages/TestGeometry2D/`, `.diaapp` in stage's `misc/ApplicationFlow/`, plus `cluiche_main.diaapp` + `cluichetest.diagame` + `assets.catalogue.json` + `pipeline.toml` updates |
| T7 | Unit test gap | `DiaGeometry2D` intersection tests have GoogleTest coverage; this stage covers the visual/runtime draw path only |
| T8 | Determinism | Fully deterministic — static scene, no simulation |
| T9 | Frame budget | All shapes static; negligible CPU cost per frame |
| T10 | Dependencies | `DiaGeometry2DVisualDebugger` linked to CluicheTest; `ArcDrawHelper` / `SectorDrawHelper` / `OORectDrawHelper` / `CapsuleDrawHelper` added to `DiaGeometry2DVisualDebugger` (Option A — ConvexPolygon tessellation, ShapeDrawer unchanged) |

---

## Acceptance Criteria

### Shared ACs (inherited from test-stage-infrastructure.md)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-S1 | Stage has a manifest entry with `transitions: ["Boot"]` | Manifest review |
| AC-S2 | Stage module registers at least one checkpoint in DoStart via AutomationService | Code review + checkpoint trigger returns success |
| AC-S3 | Stage module emits at least one metric to MetricRegistry | Metric query from orchestrator returns value |
| AC-S4 | Stage module logs at DoStart entry, DoStop entry (`DIA_LOG_INFO`) | Session log review |
| AC-S5 | Stage module returns `StartResult::kReady` only after scene setup is complete | Code review |
| AC-S6 | Stage module cleans up all resources in DoStop, returns `StopResult::kDone` | No leaks; asset handles released |
| AC-S7 | Navigating Boot → Stage → Boot → Stage produces identical checkpoint results | Orchestrator scenario runs twice in sequence |
| AC-S8 | Checkpoint names follow convention: `<feature>.<checkpoint_name>` | Code review |
| AC-S9 | Stage has a matching pytest scenario file | File exists |

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-G1 | Gallery grid renders one instance of each shape type: Circle, AARect, OORect, Line, Ray, Triangle, Capsule, ConvexPolygon, Arc, Sector | Visual inspection against mockup |
| AC-G2 | OORect, Capsule, Arc, and Sector are rendered via ConvexPolygon approximation helpers (no native ShapeDrawer calls) | Code review of helpers |
| AC-G3 | Intersection pair section shows exactly 6 pairs: Circle/Circle (HIT), Circle/AARect (MISS), AARect/Triangle (HIT), Ray/Circle (HIT), Line/AARect (MISS), Circle/Triangle (MISS) | Visual inspection against mockup |
| AC-G4 | `gallery.render_frame_complete` checkpoint passes on the first frame after DoStart completes | Orchestrator polls checkpoint → returns `passed: true` within 5 frames |
| AC-G5 | `shape_count` metric emitted each frame; orchestrator asserts value matches expected total | Metric query returns stable non-zero count |
| AC-G6 | `intersection_pair_count` metric emitted once in DoStart; orchestrator asserts value equals the number of evaluated pairs | Metric query returns expected constant |
| AC-G7 | Spatial structure section shows all four structures — BVH, Quadtree, SpatialGrid, HexGrid — each rendered as a cell overlay over a compact shape set | Visual inspection against mockup |

---

## Design

**Visual mockup:** [@docs/specs/features/cluichetest/teststages/geometry2d-stage.mockup.html](geometry2d-stage.mockup.html) — open in a browser for visual acceptance gate reference.

### Scene Layout

```
+----------------------------------------------+
|  [Debug Console - top-left ImGui panel]       |
|  Title: "Debug Console"                       |
|  Domain tabs: Geometry2D* | RigidBody2D | ... |
|    ▼ Draw Layers                              |
|      ☑ geo2d.shapes   ☑ geo2d.labels         |
|      ☑ geo2d.intersections (hit/miss colours) |
|      ☑ geo2d.spatial_overlay  ☐ geo2d.aabbs  |
|    ▼ Geometry Stats                           |
|      Shapes:10  Pairs:6  Hits:3  Misses:3    |
|      Total drawn:40  Budget:128               |
|    ▶ Spatial Stats (collapsed)                |
|  > [command input]                            |
|  Output | Warnings                            |
|                                               |
|  [Checkpoints panel - right of console]       |
|  ✓ gallery.render_frame_complete   PASS       |
|                                               |
|  WORLD SPACE (centre + right)                 |
|                                               |
|  — Shape Gallery (top band) ——————————————   |
|  Circle  AARect  OORect*  Line  Ray           |
|  Triangle  Capsule*  ConvexPoly  Arc*  Sector*|
|  (* = ConvexPolygon tessellation)             |
|                                               |
|  — Intersection Tests (middle band) ————————  |
|  Circle/Circle(HIT)  Circle/AARect(MISS)      |
|  AARect/Triangle(HIT)  Ray/Circle(HIT)        |
|  Line/AARect(MISS)  Circle/Triangle(MISS)     |
|                                               |
|  — Spatial Structures (bottom band) ————————  |
|  [BVH]  [Quadtree]  [SpatialGrid]  [HexGrid]  |
|                                               |
|  ——————————————————————————————————————————  |
|  HUD: geometry2d_stage | ✓ checkpoint | 042   PASS  ✕ |
+----------------------------------------------+
```

- **Debug Console** (ImGui, top-left, ~310px wide): generic title, domain tabs across the top (Geometry2D active), three collapsible sections inside the Geometry2D tab — Draw Layers, Geometry Stats, Spatial Stats. Matches the RigidBody2D console pattern exactly.
- **Checkpoints panel** (ImGui, right of console, ~200px wide): separate from the debug console; shows checkpoint name + PASS/PENDING/FAIL badge.
- **World space** (remainder of viewport): three horizontal bands — gallery top, intersection pairs middle, spatial structures bottom.
- **HUD bar** (28px, bottom): stage name | checkpoint badge | frame counter | PASS status | exit button. Identical pattern to RigidBody2D.

### Module Structure

```cpp
// CluicheTest/Modules/TestStages/TestGeometry2DStageModule.h
class TestGeometry2DStageModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;  // "TestGeometry2DStageModule"
    explicit TestGeometry2DStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    void SetupGallery();
    void SetupIntersectionPairs();
    void SetupSpatialStructures();
    void EvaluateIntersections();
    void RegisterCheckpoints();
    void RegisterDrawers();
    void UnregisterDrawers();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::AutomationModule> mAutomation{this};

    // Gallery shapes (10 primitives)
    Dia::Geometry2D::Circle         mCircle;
    Dia::Geometry2D::AARect         mAARect;
    Dia::Geometry2D::OORect         mOORect;
    Dia::Geometry2D::Line           mLine;
    Dia::Geometry2D::Ray            mRay;
    Dia::Geometry2D::Triangle       mTriangle;
    Dia::Geometry2D::Capsule        mCapsule;
    Dia::Geometry2D::ConvexPolygon  mConvexPoly;
    Dia::Geometry2D::Arc            mArc;
    Dia::Geometry2D::Sector         mSector;

    // Spatial structures
    // BVH, Quadtree, SpatialGrid, HexGrid — populated with ~8 shapes each

    // Visual debugger drawers (5 layers)
#ifdef DIA_DEBUG
    std::unique_ptr<Geometry2DShapesDrawer>         mShapesDrawer;
    std::unique_ptr<Geometry2DLabelsDrawer>         mLabelsDrawer;
    std::unique_ptr<Geometry2DIntersectionsDrawer>  mIntersectionsDrawer;
    std::unique_ptr<Geometry2DSpatialOverlayDrawer> mSpatialOverlayDrawer;
    std::unique_ptr<Geometry2DAABBDrawer>           mAABBDrawer;
#endif

    unsigned int mFrameCount = 0;
    int  mShapeCount            = 0;
    int  mIntersectionPairCount = 0;
    bool mCheckpointPassed      = false;
};
```

Module lives on **MainPU** (dependency on AutomationModule). Drawers are registered with `DebugLayerManager` on first `DoUpdate` (same pattern as RigidBody2DTestModule) and unregistered in `DoStop`.

### Checkpoint Logic

```cpp
// In DoStart():
auto* service = mAutomation.Get()->GetService();
service->RegisterCheckpoint(this, Dia::Core::StringCRC("geometry2d.passed"),
    [this]() -> Dia::Automation::CheckpointResult {
        return { mCheckpointPassed, mCheckpointPassed ? "passed" : "pending", 0.0f };
    });

const Dia::Core::StringCRC checkpoints[] = { Dia::Core::StringCRC("geometry2d.passed") };
TestResultsRegistry::GetInstance().SetRunning(
    Dia::Core::StringCRC("TestGeometry2D"), 300, checkpoints, 1);
```

### Update Loop

```cpp
void TestGeometry2DStageModule::DoUpdate(float /*deltaTime*/)
{
    ++mFrameCount;
    TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);

#ifdef DIA_DEBUG
    if (!mShapesDrawer)
        RegisterDrawers();  // First frame: register drawers with DebugLayerManager
#endif

    if (!mCheckpointPassed)
    {
        // Pass immediately after first frame — static scene, nothing to wait for
        mCheckpointPassed = true;
        TestResultsRegistry::GetInstance().SetPassed(
            Dia::Core::StringCRC("TestGeometry2D"), mFrameCount);
    }
}
```

### Arc/Sector Tessellation Helpers

New files in `Dia/DiaGeometry2DVisualDebugger/`:

- `ArcDrawHelper.h/.cpp` — converts `Arc` to `ConvexPolygon` with 8 vertices
- `SectorDrawHelper.h/.cpp` — converts `Sector` to `ConvexPolygon` with 8 + 2 vertices (fan with centre + closing point)
- `OORectDrawHelper.h/.cpp` — converts `OORect` to `ConvexPolygon` with 4 rotated vertices
- `CapsuleDrawHelper.h/.cpp` — converts `Capsule` to `ConvexPolygon` with 12 vertices

These are free functions / thin wrappers, not new classes on `IVisualDebugger`.

### Manifest Entries (generated by `/new-cluichetest-stage TestGeometry2D`)

`Assets/Stages/TestGeometry2D/test_geometry2d_stage.diastage`:
```json
{
  "name": "TestGeometry2D",
  "manifest": "stages/TestGeometry2D/misc/ApplicationFlow/test_geometry2d_stage.diaapp",
  "config": { "path_aliases": { "stage_root": "." } }
}
```

`Assets/Stages/TestGeometry2D/misc/ApplicationFlow/test_geometry2d_stage.diaapp`:
```json
{
  "version": 3,
  "processing_units": [
    { "instance_id": "MainPU", "frequency_hz": 30, "dedicated_thread": false,
      "modules": [{ "instance_id": "TestGeometry2DStageModule", "type_id": "TestGeometry2DStageModule",
                    "stages": ["TestGeometry2D"], "dependencies": ["AutomationModule"], "channels": [] }] },
    { "instance_id": "SimPU", "frequency_hz": 30, "dedicated_thread": true,
      "modules": [{ "instance_id": "VisualDebuggerModule", "type_id": "VisualDebuggerModule",
                    "stages": ["TestGeometry2D"], "dependencies": [],
                    "channels": [{"id": "SimToRender", "role": "writes"}] }] }
  ]
}
```

`cluiche_main.diaapp` additions:
- Stage entry: `{ "name": "TestGeometry2D", "transitions": ["Boot"], "auto_advance": false }`
- `"TestGeometry2D"` added to Boot's `transitions` array
- `"TestGeometry2D"` added to TestStageHUDModule's `stages` array

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/geometry2d_stage/smoke.py
def test_geometry2d_stage_loads(cluichetest):
    cluichetest.navigate_to("TestGeometry2D")
    cluichetest.wait_for_checkpoint("gallery.render_frame_complete", timeout_frames=10)
    shape_count = cluichetest.get_metric("shape_count")
    assert shape_count > 0
    pair_count = cluichetest.get_metric("intersection_pair_count")
    assert pair_count > 0
    cluichetest.navigate_to("Boot")
```

---

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/features/cluichetest/teststages/geometry2d-stage.md` | This spec |
| `docs/specs/features/cluichetest/teststages/geometry2d-stage.plan.md` | Implementation plan |
| `docs/specs/features/cluichetest/teststages/geometry2d-stage.mockup.html` | Visual acceptance gate |
| **Scaffold (via `/new-cluichetest-stage TestGeometry2D`)** | |
| `Cluiche/Assets/Stages/TestGeometry2D/test_geometry2d_stage.diastage` | New stage declaration |
| `Cluiche/Assets/Stages/TestGeometry2D/misc/ApplicationFlow/test_geometry2d_stage.diaapp` | New app manifest |
| `Cluiche/CluicheTest/Modules/TestStages/TestGeometry2DStageModule.h/.cpp` | New module skeleton |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` + `.vcxproj.filters` | Add module source |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Stage + Boot transition + HUD |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest entries |
| `pipeline.toml` | Add to asset_stages + deploy |
| **Tessellation helpers** | |
| `Dia/DiaGeometry2DVisualDebugger/ArcDrawHelper.h/.cpp` | New tessellation helper |
| `Dia/DiaGeometry2DVisualDebugger/SectorDrawHelper.h/.cpp` | New tessellation helper |
| `Dia/DiaGeometry2DVisualDebugger/OORectDrawHelper.h/.cpp` | New tessellation helper |
| `Dia/DiaGeometry2DVisualDebugger/CapsuleDrawHelper.h/.cpp` | New tessellation helper |
| `Dia/DiaGeometry2DVisualDebugger/DiaGeometry2DVisualDebugger.vcxproj` | Add helper source files |
| **Drawers** | |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/Geometry2DShapesDrawer.h/.cpp` | Gallery shapes draw layer |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/Geometry2DLabelsDrawer.h/.cpp` | ImGui label overlay layer |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/Geometry2DIntersectionsDrawer.h/.cpp` | Hit/miss pair draw layer |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/Geometry2DSpatialOverlayDrawer.h/.cpp` | BVH/Quadtree/SpatialGrid/HexGrid cell layer |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/Geometry2DAABBDrawer.h/.cpp` | AABB outline layer |
| **E2E** | |
| `Tools/orchestrator/scenarios/cluichetest/geometry2d_stage/smoke.py` | E2E scenario |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `geometry2d-stage.mockup.html` | Visual sign-off | Done | sonnet | Approved 2026-05-27 |
| 2 | Run `/new-cluichetest-stage TestGeometry2D` — scaffold all 8 touch points | `dia pipeline --target cluichetest` green; stage in Boot menu | Todo | sonnet | Generates skeleton module, manifests, vcxproj, pipeline.toml |
| 3 | Add `ArcDrawHelper`, `SectorDrawHelper`, `OORectDrawHelper`, `CapsuleDrawHelper` to `DiaGeometry2DVisualDebugger`; update vcxproj | Unit tests: vertex count + convex polygon validity | Todo | sonnet | Dia-side; escalation rule confirmed OK |
| 4 | Create 5 drawers: `Geometry2DShapesDrawer`, `Geometry2DLabelsDrawer`, `Geometry2DIntersectionsDrawer`, `Geometry2DSpatialOverlayDrawer`, `Geometry2DAABBDrawer`; update CluicheTest vcxproj | Clean build; drawers register/unregister correctly | Todo | sonnet | Follow RigidBody2D drawer pattern exactly |
| 5 | Implement `TestGeometry2DStageModule` logic — SetupGallery, SetupIntersectionPairs, SetupSpatialStructures, EvaluateIntersections, RegisterDrawers | Stage loads; checkpoint fires; drawers render shapes | Todo | sonnet | Replaces skeleton TODO; depends on Tasks 3+4 |
| 6 | Wire REPL commands: `dia.geometry2d.shape_count`, `dia.geometry2d.intersection_pairs` into CommandRegistry | Commands return correct values from REPL | Todo | sonnet | Follow RigidBody2D command pattern |
| 7 | Write pytest scenario `geometry2d_stage/smoke.py` | Orchestrator scenario passes | Todo | sonnet | |
| 8 | `dia run cluichetest` — visual verify against mockup | Manual visual gate | Todo | sonnet | All layers toggleable in Debug Console |
| 9 | Commit + update spec status → Done | — | Todo | haiku | |

---

## Dependencies

```
Task 1 (mockup)     → DONE
Task 2 (scaffold)   → independent; start now
Task 3 (helpers)    → independent; parallel with Task 2
Task 4 (drawers)    → after Task 3 (uses tessellation helpers)
Task 5 (module)     → after Tasks 2 + 4 (needs scaffold + drawers)
Task 6 (REPL)       → after Task 5 (needs module data)
Task 7 (pytest)     → after Task 5
Task 8 (verify)     → after Tasks 5 + 6
Task 9 (commit)     → after Tasks 7 + 8
```

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | `kUniqueId`, checkpoint name `"gallery.render_frame_complete"`, stage name `"geometry2d_stage"`, metric names `"shape_count"` / `"intersection_pair_count"` all `StringCRC`. |
| PD-004 | No STL containers in public APIs | Module interface uses Dia types. Tessellation helpers are internal free functions; no STL in their signatures. |
| PD-006 | VS project files are source of truth | Tasks 6 and 7 add all new source files to the appropriate `.vcxproj` files. |
| PD-007 | C++20 required | `constexpr StringCRC`, standard C++ features throughout. |
| PD-009 | Generated output under `Cluiche/out/` | Session logs and metrics in `Cluiche/out/CluicheTest/sessions/`. |
| PD-010 | `.diagame` root; `.diastage` for stage metadata | `geometry2d_stage.diastage` added as a typed stage import in `cluichetest.diagame`; loader resolves manifest via the `.diastage` pointer. |
| AD-001 | Three PUs | `TestGeometry2DStageModule` lives on MainPU (AutomationModule dependency); `VisualDebuggerModule` on SimPU writing to `SimToRender`. No structural changes to PU topology. |
| AD-002 | Levels are code-based | Scene setup (gallery positions, shape parameters, intersection pairs) is pure C++; no data-driven level loading. |
| AD-003 | Entry point in `Main.cpp` | No change; stage hooks into existing app lifecycle via manifest. |
| AD-005 | App is testbed, not shipped product | Stage exists purely for engine validation; no production constraints apply. |
| SD-TS-001 | One manifest stage per feature | Exactly one stage: `geometry2d_stage`. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | `RegisterCheckpoint` called in `DoStart`; auto-clear via module ownership on stop. |
| SD-TS-003 | Metrics for threshold assertions | `shape_count` and `intersection_pair_count` emitted; pytest asserts specific values. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in `geometry2d_stage.diastage`. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec for this stage. |

---

## Open Questions

None.

---

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| Q1 | Design — Tessellation | How many polygon vertices should ArcDrawHelper/SectorDrawHelper use for acceptable visual quality at typical screen scale? | 12 vertices for arc/sector; 16 for capsule | **8 vertices for Arc/Sector; 12 for Capsule.** Lower resolution is intentional — angular shapes are acceptable for a test gallery and keeps polygon budget low. |
| Q2 | Design — ShapeDrawer limit | Does the full scene (gallery + intersection pairs + spatial structure cells) stay under ShapeDrawer's 128-shape cap? | Cap spatial overlay to 8 shapes per structure (24 total); total ~40 shapes — safely under 128 | **Confirmed compliant.** ~10 gallery + ~6 intersection + ~24 spatial = ~40 total. |
| Q3 | Design — OORect/Capsule scope | The spec adds OORectDrawHelper and CapsuleDrawHelper in addition to Arc/Sector — is this scope confirmed? | Yes — all DiaGeometry2D shape types should be renderable from the gallery | **Confirmed.** AC-G1 requires all shape types; OORect and Capsule helpers are included in scope. |
| Q4 | Design — Spatial renderers | Do BVHRenderer/QuadtreeRenderer/SpatialGridRenderer integrate directly via ShapeDrawer, or do they need bridging? | They likely use ShapeDrawer internally; verify at implementation start | **Deferred to implementation start.** Verify renderer interface before Task 3. If bridging is needed it becomes a sub-task within Task 3. |
| Q5 | Tasks — PU for geometry module | SimPU is chosen — does anything break if SimPU is used for a non-physics module? | No — SimPU hosts any simulation/evaluation work; physics is one consumer, not the only one | **Confirmed.** SimPU is appropriate; no structural issue. |

---

## Status

`Approved` — 2026-05-27

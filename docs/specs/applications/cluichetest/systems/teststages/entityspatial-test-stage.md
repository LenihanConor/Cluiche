# Feature Spec: EntitySpatialTestStage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

---

## Problem Statement

`DiaEntitySpatial` is unit-tested in isolation (30 GoogleTests) but has never been exercised under real PU timing with live entities moving each frame. No E2E path validates: incremental dirty-flag re-indexing at 30 Hz, all five query shapes returning correct entities against a live domain, layer-mask category filtering being toggled at runtime, or the index correctly handling entity destruction mid-run. The stage also needs to be visually interesting — you should be able to look at the window and immediately understand what the spatial queries are doing.

---

## Template Answers

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features under test | `DiaEntitySpatial` — `EntitySpatialModule`, `SpatialComponent`, all five query shapes (`QueryCircle`, `QueryRegion`, `QueryKNearest`, `QueryRay`, `QuerySector`), dirty-flag incremental re-indexing, layer-mask filtering, entity destruction sweep |
| T2 | Scene layout | 40×40 world (400 × 400 world units), SquareGrid cellSize=10; 32 agents moving on sine-wave paths across the field; 1 "player" entity at centre that drives all 5 queries each frame; 4 layer categories (Friendly/Enemy/Obstacle/Neutral); 2 agents destroyed at frame 90 to verify stale-handle sweep |
| T3 | Acceptance criteria | Shared ACs 1–9 + stage-specific ACs AC-ES1 through AC-ES9 (see below) |
| T4 | Metrics | `cluichetest.entityspatial.circle_hit_count`, `cluichetest.entityspatial.region_hit_count`, `cluichetest.entityspatial.knearest_count`, `cluichetest.entityspatial.ray_hit_count`, `cluichetest.entityspatial.sector_hit_count`, `cluichetest.entityspatial.active_entity_count` |
| T5 | PU assignment | `EntitySpatialTestStageModule` on SimPU |
| T6 | Assets | `entityspatial_test_stage.diastage` + `entityspatial_test_stage.diaapp`; world layout defined inline in C++ |
| T7 | Unit test gap | Unit tests use fixed entity positions and one-shot updates. This stage validates: dirty-flag sweep running at 30 Hz with 32 continuously moving entities; correct hit counts across all 5 query shapes against a live domain; layer-mask toggle at frame 60 (enemy layer disabled → query hits drop); two entity destructions at frame 90 processed by the detach/destroy sweep without crash or stale results; `QueryKNearest(k=5)` returns exactly 5 even when 32 agents are present. |
| T8 | Determinism | Deterministic — entity paths are pure sine waves (frame-counter-driven, no random); destructions at fixed frames |
| T9 | Frame budget | 32 entities × 1 dirty update + 5 queries per frame — negligible at 30 Hz |
| T10 | Dependencies | `AutomationModule` (checkpoints + metrics), `VisualDebuggerModule` (query shape overlays + entity circle overlays) |

---

## Acceptance Criteria

### Shared ACs (inherited from test-stage-infrastructure.md)

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-S1 | Stage has a manifest entry with `transitions: ["Boot"]` | Manifest review |
| AC-S2 | Stage module registers at least one checkpoint in DoStart via AutomationService | Code review + checkpoint trigger returns success |
| AC-S3 | Stage module emits at least one metric to MetricRegistry | Metric query returns value |
| AC-S4 | Stage module logs at DoStart entry, DoStop entry (`DIA_LOG_INFO`) | Session log review |
| AC-S5 | Stage module returns `StartResult::kReady` only after scene setup is complete | Code review |
| AC-S6 | Stage module cleans up all resources in DoStop, returns `StopResult::kDone` | No leaks |
| AC-S7 | Navigating Boot → Stage → Boot → Stage produces identical checkpoint results | E2E scenario runs twice |
| AC-S8 | Checkpoint names follow convention: `entityspatial.<checkpoint_name>` | Code review |
| AC-S9 | Stage has a matching pytest scenario file | File exists at `Cluiche/Tests/E2E/scenarios/cluichetest/entityspatial_stage/smoke.py` |

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-ES1 | `entityspatial.index_initialized` passes: `EntitySpatialModule` constructed and first `Update()` complete without crash | Orchestrator polls within 2s |
| AC-ES2 | `entityspatial.circle_query_live` passes: `QueryCircle` returns ≥ 1 entity at frame 30+ (agents have moved into player's circle range) | Orchestrator polls within 5s |
| AC-ES3 | `entityspatial.sector_query_live` passes: `QuerySector` (player FOV cone, 90° half-angle, radius 120) returns ≥ 1 entity at frame 30+ | Orchestrator polls within 5s |
| AC-ES4 | `entityspatial.knearest_exact` passes: `QueryKNearest(k=5)` returns exactly 5 entities when ≥ 5 agents are alive and indexed | Orchestrator polls within 5s |
| AC-ES5 | `entityspatial.layer_mask_filter` passes: at frame 60, enemy layer (mask bit 1) is cleared from the query mask; circle-query hit count drops relative to pre-frame-60 baseline | Orchestrator polls within 3s after frame 60 |
| AC-ES6 | `entityspatial.destroy_sweep` passes: at frame 90, 2 agents are destroyed; by frame 91, neither appears in any query result | Orchestrator polls within 3s after frame 90 |
| AC-ES7 | `entityspatial.ray_query_live` passes: `QueryRay` (player origin, rotating direction, maxDist=200) returns ≥ 1 entity across any 30-frame window | Orchestrator polls within 10s |
| AC-ES8 | `entityspatial.all_queries_stable` passes: all 5 query shapes return stable non-crashing results for 120 consecutive frames (no assert fires, no crash) | Implicit — orchestrator survives to frame 210 |
| AC-ES9 | `cluichetest.entityspatial.active_entity_count` metric equals 30 at frame 91+ (32 spawned − 2 destroyed) | Metric assertion in pytest |

---

## Design

### Scene Layout

```
+------------------------------------------------------------------+
|  [Query Stats — top-left ImGui]                                  |
|  EntitySpatial Test Stage                                        |
|  Active entities : 32                                            |
|  Circle  hits    : 7                                             |
|  Region  hits    : 5                                             |
|  KNearest(5)     : 5                                             |
|  Ray     hits    : 2                                             |
|  Sector  hits    : 4                                             |
|  Layer mask      : [F][E][O][N]  (toggle buttons)               |
|                                                                  |
|  [Checkpoints — right of stats]                                  |
|  ✓ entityspatial.index_initialized    PASS                      |
|  ✓ entityspatial.circle_query_live    PASS                      |
|  ✓ entityspatial.sector_query_live    PASS                      |
|  ✓ entityspatial.knearest_exact       PASS                      |
|  ○ entityspatial.layer_mask_filter    PENDING                   |
|  ○ entityspatial.destroy_sweep        PENDING                   |
|  ○ entityspatial.ray_query_live       PENDING                   |
|  ○ entityspatial.all_queries_stable   PENDING                   |
|                                                                  |
|  WORLD SPACE  (400 × 400, SquareGrid 10-unit cells)             |
|                                                                  |
|  [Grid overlay — thin grey lines every 10 units]                |
|  [Agent circles — colour by layer category]                     |
|    Friendly  : teal   RGBA(40,200,160,200)                      |
|    Enemy     : red    RGBA(220,60,60,200)                        |
|    Obstacle  : grey   RGBA(160,160,160,200)                      |
|    Neutral   : yellow RGBA(200,200,40,200)                       |
|  [Agent trails — fading tail, 8 frames history]                 |
|                                                                  |
|  [Player entity — white diamond ◆ at scene centre]              |
|                                                                  |
|  QUERY SHAPE OVERLAYS (drawn by VisualDebuggerModule):           |
|  Circle query  — cyan circle outline at player, radius 80        |
|  Region query  — magenta AARect outline (player ±60 each axis)  |
|  Sector query  — two lines from player at ±90°, length 120       |
|    Entities INSIDE sector : bright white ring glow               |
|  Ray query     — rotating white line from player, maxDist 200    |
|    Entities HIT by ray    : orange flash on hit                  |
|  KNearest(5)   — connecting lines from player to 5 nearest      |
|    Line colour  lerp(green, red, normalisedDist)                 |
|                                                                  |
|  HUD: entityspatial_test_stage | ✓ 4/8 | f:127  ticking    ✕   |
+------------------------------------------------------------------+
```

### World Layout

```
World: 400 × 400 world units, origin at centre (−200 to +200 each axis)
SquareGrid: worldBounds = AARect(−200,−200, 200,200), cellSize = 10

Player entity: fixed at (0, 0), radius = 5, layerMask = 0 (never returned by queries)

Agents (32 total, spawned at DoStart):

  8 Friendly  (layer bit 0, mask 0x01) — move along horizontal sine waves:
    agents 0–7:  pos.x = −180 + i*50,  pos.y = 60 * sin(frame/40.0 + i*0.4)

  8 Enemy     (layer bit 1, mask 0x02) — move along vertical sine waves:
    agents 8–15: pos.y = −180 + (i−8)*50,  pos.x = 60 * sin(frame/35.0 + i*0.6)

  8 Obstacle  (layer bit 2, mask 0x04) — orbit player slowly:
    agents 16–23: pos = 140 * (cos(frame/80.0 + i*π/4), sin(frame/80.0 + i*π/4))

  8 Neutral   (layer bit 3, mask 0x08) — figure-8 Lissajous paths:
    agents 24–31: pos = (100*sin(frame/60.0 + i*0.5), 60*sin(frame/30.0*2 + i*0.3))

  All agents: radius = 8.0f; each frame pos changes → SpatialComponent dirty → module re-indexes

Destruction events:
  Frame 90: agents[2] and agents[10] QueueDestroy'd (one Friendly, one Enemy)
  → by frame 91 both are swept from the index

Query parameters (evaluated each frame from player position (0,0)):
  QueryCircle  : radius 80,  mask 0x0F (all layers; narrows to 0x0D at frame 60)
  QueryRegion  : AARect(−60,−60, 60,60),  mask 0x0F
  QueryKNearest: k=5, mask 0x0F
  QueryRay     : origin (0,0), dir rotates 2° per frame, maxDist 200, mask 0x0F
  QuerySector  : origin (0,0), dir=(1,0) fixed, radius 120, halfAngle=π/2, mask 0x0F
```

### Phase Timeline

| Frame range | Event |
|-------------|-------|
| 0 | DoStart: spawn 32 agents, construct `EntitySpatialModule`, first `Update()` — `entityspatial.index_initialized` fires |
| 0–30 | Agents move; dirty-flag sweep runs each frame; queries start returning results |
| 30 | `circle_query_live`, `sector_query_live`, `knearest_exact` checkpoints checked |
| 30–60 | All 5 queries active; metrics emitted each frame |
| 60 | Enemy layer (bit 1) dropped from circle query mask → `layer_mask_filter` checkpoint fires |
| 60–90 | Queries run with reduced mask; layer toggle visually visible (red agents no longer highlighted) |
| 90 | agents[2] and agents[10] destroyed — `destroy_sweep` checkpoint fires at frame 91 |
| 90–120 | Active count drops to 30; destruction sweep clears index; no stale handles in results |
| 90–210 | Ray query rotating; `ray_query_live` fires whenever ≥1 hit occurs |
| 210 | `all_queries_stable` fires (120 consecutive stable frames) — stage reports PASSED |

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/EntitySpatialTestStageModule.h
class EntitySpatialTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Entity spatial queries: 5 query shapes, layer masks, dirty-flag re-index, destroy sweep";
    static constexpr unsigned int kMinDisplayFrames = 210;

    explicit EntitySpatialTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~EntitySpatialTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 600; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    void SpawnAgents();
    void MoveAgents();   // updates SpatialComponent::position + MarkDirty each frame
    void RunQueries();
    void UpdateMetrics();
    void CheckCheckpoints();

    // Domain + spatial module
    Dia::Entity::Domain               mDomain;
    // EntitySpatialModule owns the index; constructed in OnStart
    std::unique_ptr<Dia::EntitySpatial::EntitySpatialModule> mSpatialModule;

    // Agent tracking
    static constexpr int kAgentCount = 32;
    Dia::Entity::Entity  mAgents[kAgentCount];

    // Per-frame query outputs (reused each frame)
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mCircleOut;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mRegionOut;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 8>  mKNearestOut;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mRayOut;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, 64> mSectorOut;

    // Metrics
    Dia::Observation::Metric::Gauge* mMetricCircleHits    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricRegionHits    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricKNearestCount = nullptr;
    Dia::Observation::Metric::Gauge* mMetricRayHits       = nullptr;
    Dia::Observation::Metric::Gauge* mMetricSectorHits    = nullptr;
    Dia::Observation::Metric::Gauge* mMetricActiveCount   = nullptr;

    // Checkpoint booleans
    bool mIndexInitialized  = false;
    bool mCircleQueryLive   = false;
    bool mSectorQueryLive   = false;
    bool mKNearestExact     = false;
    bool mLayerMaskFilter   = false;
    bool mDestroySweep      = false;
    bool mRayQueryLive      = false;
    bool mAllQueriesStable  = false;

    // State
    uint32_t mQueryMask           = 0x0Fu;
    int      mStableFrameCount    = 0;
    bool     mDestroyFired        = false;
    bool     mLayerMaskReduced    = false;
    int      mBaselineCircleHits  = 0;   // measured at frame 59 for layer_mask_filter check

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    // overlays registered in OnStart
#endif
};
```

### Visual Rendering

| Element | Primitive | Colour |
|---------|-----------|--------|
| Grid cell lines | Line | RGBA(60,60,60,80) — subtle |
| Friendly agent circles | Circle outline, r=8 | RGBA(40,200,160,200) |
| Enemy agent circles | Circle outline, r=8 | RGBA(220,60,60,200) |
| Obstacle agent circles | Circle outline, r=8 | RGBA(160,160,160,200) |
| Neutral agent circles | Circle outline, r=8 | RGBA(200,200,40,200) |
| Agent trail (8 frames) | Line, fading alpha | Same as agent colour, alpha × (age/8) |
| Player entity | Diamond ◆ (cross of 2 rects) | RGBA(255,255,255,255) |
| Circle query boundary | Circle outline | RGBA(0,220,220,180) — cyan |
| Region query boundary | AARect outline | RGBA(220,0,220,180) — magenta |
| Sector query cone | 2 lines from player | RGBA(180,255,180,180) — pale green |
| Ray query line | Line, maxDist=200 | RGBA(255,255,255,160) — white |
| Agent inside circle query | Filled tint over circle | RGBA(0,220,220,60) |
| Agent inside sector query | Bright ring glow | RGBA(200,255,200,200) |
| Agent hit by ray | Orange flash (5 frames) | RGBA(255,160,0,220) fading |
| KNearest connecting lines | Line player→agent | Lerp(RGBA(0,255,0,180), RGBA(255,0,0,180), normDist) |
| Destroyed agent (death flash) | Expanding circle, fading | RGBA(255,255,255,200) → transparent over 10 frames |
| HUD bar | ImGui text | Stage name, checkpoint count, frame, status |

### Pytest Scenario

```python
# Cluiche/Tests/E2E/scenarios/cluichetest/entityspatial_stage/smoke.py

_STAGE = "EntitySpatialTestStage"

_CHECKPOINTS = [
    ("entityspatial.index_initialized",  2.0),
    ("entityspatial.circle_query_live",  5.0),
    ("entityspatial.sector_query_live",  5.0),
    ("entityspatial.knearest_exact",     5.0),
    ("entityspatial.layer_mask_filter",  5.0),   # fires just after frame 60
    ("entityspatial.destroy_sweep",      5.0),   # fires at frame 91
    ("entityspatial.ray_query_live",    12.0),
    ("entityspatial.all_queries_stable", 8.0),   # fires at frame 210
]


def test_entityspatial_stage(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp}: {result['message']}"

        active = dia_client.get_metric("cluichetest.entityspatial.active_entity_count")
        assert active == 30, f"Expected 30 active entities after destruction, got {active}"

        knearest = dia_client.get_metric("cluichetest.entityspatial.knearest_count")
        assert knearest == 5, f"QueryKNearest(5) returned {knearest}, expected 5"

        circle_hits = dia_client.get_metric("cluichetest.entityspatial.circle_hit_count")
        assert circle_hits >= 0, "circle_hit_count metric missing"

        sector_hits = dia_client.get_metric("cluichetest.entityspatial.sector_hit_count")
        assert sector_hits >= 0, "sector_hit_count metric missing"

    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
```

---

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/applications/cluichetest/systems/teststages/entityspatial-test-stage.md` | This spec |
| `Cluiche/CluicheTest/Modules/TestStages/EntitySpatialTestStageModule.h` | New stage module header |
| `Cluiche/CluicheTest/Modules/TestStages/EntitySpatialTestStageModule.cpp` | New stage module implementation |
| `Cluiche/Assets/CluicheTest/Stages/EntitySpatialTestStage/entityspatial_test_stage.diastage` | Stage manifest |
| `Cluiche/Assets/CluicheTest/Stages/EntitySpatialTestStage/misc/ApplicationFlow/entityspatial_test_stage.diaapp` | Module wiring |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Register stage module type |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Import new stage |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Cluiche/Tests/E2E/scenarios/cluichetest/entityspatial_stage/smoke.py` | E2E scenario |
| `Cluiche/Tests/E2E/plans/cluichetest/default.json` | Register scenario |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `EntitySpatialTestStageModule.h/.cpp` skeleton: DoStart/DoUpdate/DoStop, 8 checkpoints registered, 6 metrics registered, Domain + EntitySpatialModule constructed | Build passes; `entityspatial.index_initialized` fires on first Update | Todo | sonnet | |
| 2 | Implement `SpawnAgents`: create 32 entities with SpatialComponent (4 layer categories, sine-wave initial positions), store handles in `mAgents[]` | Agents visible in scene at frame 0 | Todo | sonnet | Depends on Task 1 |
| 3 | Implement `MoveAgents`: per-frame sine-wave position update + `MarkDirty`; destruction of agents[2] and agents[10] at frame 90 | `entityspatial.destroy_sweep` passes | Todo | sonnet | Depends on Task 2 |
| 4 | Implement `RunQueries`: all 5 query shapes each frame; layer-mask reduction at frame 60; KNearest k=5; rotating ray; checkpoint logic | `circle_query_live`, `sector_query_live`, `knearest_exact`, `layer_mask_filter`, `ray_query_live`, `all_queries_stable` pass | Todo | sonnet | Depends on Task 3 |
| 5 | Implement `UpdateMetrics`: emit 6 metrics per frame | Metric assertions in pytest pass | Todo | haiku | Depends on Task 4 |
| 6 | Implement debug visuals: grid overlay, agent circles + trails (8-frame history), player diamond, all 5 query shape overlays, destroy flash, KNearest connecting lines | Visual inspection — all overlays render | Todo | sonnet | DIA_DEBUG only; depends on Task 4 |
| 7 | Create `.diastage` + `.diaapp` manifest files; add source files to `CluicheTest.vcxproj` | `dia validate manifest` passes; stage appears in Boot menu | Todo | haiku | Can run in parallel with Tasks 2–6 |
| 8 | Write pytest scenario `entityspatial_stage/smoke.py`; register in `default.json` | Scenario collected by `dia test e2e --list` | Todo | haiku | Can run in parallel with Tasks 1–7 |
| 9 | `dia run cluichetest` — navigate to EntitySpatialTestStage; visual verify: agents move, query shapes visible, layer toggle drops red hits, destroy flash at frame 90, all 8 checkpoints pass | Manual visual gate + `dia run cluichetest` passes | Todo | sonnet | Requires Tasks 1–7 complete |
| 10 | `dia run e2e --scenario entityspatial_stage` — automated pytest scenario passes | E2E green | Todo | sonnet | Requires Task 9 complete |
| 11 | Commit + `dia docs spec-done` | — | Todo | haiku | Requires Task 10 complete |

### Task Dependencies
- Task 1 and Task 7 can start in parallel.
- Task 2 depends on Task 1.
- Task 3 depends on Task 2.
- Task 4 depends on Task 3.
- Tasks 5 and 6 depend on Task 4.
- Task 8 can run in parallel with Tasks 1–7.
- Task 9 requires Tasks 1–7 complete.
- Task 10 requires Task 9.
- Task 11 requires Task 10.

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | `kTypeId`, checkpoint names (`entityspatial.*`), metric names — all `StringCRC` |
| PD-004 | No STL in public APIs | Module interface uses `DIA_MODULE`; `DynamicArrayC` for all query outputs |
| PD-006 | VS project files source of truth | Task 7 adds all new source files to `CluicheTest.vcxproj` |
| PD-010 | `.diagame` root; `.diastage` for stage metadata | `entityspatial_test_stage.diastage` imported in `cluichetest.diagame` |
| AD-001 | Three PUs | `EntitySpatialTestStageModule` on SimPU; no structural PU changes |
| SD-TS-001 | One manifest stage per test feature | Exactly one stage: `EntitySpatialTestStage` |
| SD-TS-002 | Checkpoints in DoStart, auto-cleared on stop | All 8 checkpoints registered in `OnStart` via `TestStageModuleBase` |
| SD-TS-003 | Metrics for threshold assertions | 6 metrics emitted each frame |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in `.diastage` |
| SD-TS-005 | Individual stages are feature specs | This IS the feature spec for this stage |

---

## Open Design Questions

| # | Question | Default if not revisited |
|---|----------|--------------------------|
| ODQ-1 | **Agent trail storage**: 8-frame position history per agent needs storage. Should it be a fixed ring buffer inside the module (32 × 8 Vector2Ds = 256 floats, stack-friendly) or a debug-only member? | Debug-only member, ring buffer per agent |
| ODQ-2 | **Domain per stage vs shared domain**: `EntitySpatialTestStageModule` owns its own `Dia::Entity::Domain`. Is there a reason to share the global CluicheTest domain instead? The stage doesn't need to interop with other game entities. | Own domain — isolated teardown is simpler |
| ODQ-3 | **Player entity in domain**: Should the player entity have a `SpatialComponent` with a layer mask that excludes it from all queries (mask = 0), or should it not be a domain entity at all (just a `Vector2D` position)? | Not a domain entity — just a position constant; avoids needing a "player" layer |

---

## Status

**Status:** `Done`

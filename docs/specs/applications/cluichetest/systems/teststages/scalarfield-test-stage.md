# Feature Spec: ScalarFieldTestStage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

---

## Problem Statement

DiaScalarField is fully unit-tested in isolation but has never been run under real PU timing in a visually observable stage. No E2E path exists to validate: field propagation reaching steady state, walls correctly blocking diffusion, write shapes (WriteRadial, WriteBox, WritePoint) working as expected, multi-field Combine producing a correct composite view, and spatial queries (FindLocalMaxima, FindCellsAboveThreshold, GetGradient) returning stable results. The visual overlays (heatmap, gradient arrows) also have no runtime exercise path.

---

## Template Answers

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features under test | `DiaScalarField` — UniformDecayPolicy, RulesPropagationPolicy, WriteRadial, WriteBox, WritePoint, SetBlocked, SetStaticModifier, SetClampRange, Combine, FindLocalMaxima, FindCellsAboveThreshold, GetGradient, ScalarFieldHeatmapOverlay, ScalarFieldGradientOverlay |
| T2 | Scene layout | 20×15 8-connected square grid; 2 Blue base cells (top-left zone), 2 Red base cells (bottom-right zone); L-shaped walls blocking centre corridors; 3-cell "swamp" strip with reduced static modifier; 3 separate fields: Blue influence, Red influence, Combined (front line) |
| T3 | Acceptance criteria | Shared ACs 1–9 + stage-specific ACs AC-SF1 through AC-SF11 (see below) |
| T4 | Metrics | `cluichetest.scalarfield.blue_peak`, `cluichetest.scalarfield.red_peak`, `cluichetest.scalarfield.contested_cell_count`, `cluichetest.scalarfield.blue_cells_above_half`, `cluichetest.scalarfield.red_cells_above_half`, `cluichetest.scalarfield.gradient_probe_magnitude` |
| T5 | PU assignment | `ScalarFieldTestStageModule` on SimPU |
| T6 | Assets | `scalarfield_test_stage.diastage` + `scalarfield_test_stage.diaapp`; grid layout defined inline in C++ |
| T7 | Unit test gap | Unit tests run Tick() on tiny grids with mocked policies. This stage validates: real dt field propagation on a 20×15 grid at 30Hz; RulesPropagationPolicy with terrain modifiers blocking propagation at wall cells; WriteBox burst events creating visible ripples; Combine over two live fields producing a stable contested front line; spatial queries returning reproducible results under SimPU timing. |
| T8 | Determinism | Fully deterministic — fixed grid, fixed base positions, burst events at fixed frames |
| T9 | Frame budget | 3 fields × 300 cells each Tick() — ~900 cell updates per frame, negligible at 30Hz |
| T10 | Dependencies | `AutomationModule` (checkpoints + metrics), `VisualDebuggerModule` (heatmap + gradient overlays) |

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
| AC-S8 | Checkpoint names follow convention: `scalarfield.<checkpoint_name>` | Code review |
| AC-S9 | Stage has a matching pytest scenario file | File exists at `Cluiche/Tests/E2E/scenarios/cluichetest/scalarfield_stage/smoke.py` |

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-SF1 | `scalarfield.fields_initialized` passes: all 3 fields have completed at least 1 Tick() | Orchestrator polls within 2s of DoStart |
| AC-SF2 | `scalarfield.blue_steady_state` passes: Blue field peak value > 0.8 (sustained WriteRadial sources each frame) | Orchestrator polls within 8s |
| AC-SF3 | `scalarfield.red_steady_state` passes: Red field peak value > 0.8 | Orchestrator polls within 8s |
| AC-SF4 | `scalarfield.walls_respected` passes: all 6 blocked wall cells read exactly 0.0 after steady state | Orchestrator polls within 8s |
| AC-SF5 | `scalarfield.box_write_burst` passes: at frame 60, WriteBox 3×3 at centre causes ≥4 cells in the box region to exceed 0.5 on the same tick | Orchestrator polls within 5s of frame 60 |
| AC-SF6 | `scalarfield.local_maxima_found` passes: FindLocalMaxima on Blue field returns exactly 2 peaks (one per source cluster) | Orchestrator polls within 12s |
| AC-SF7 | `scalarfield.contested_zone_stable` passes: FindCellsAboveThreshold(0.4) on the Combined field returns ≥ 10 cells (front line has minimum width) | Orchestrator polls within 15s |
| AC-SF8 | `scalarfield.gradient_non_zero_at_probe` passes: GetGradient at centre probe cell (10, 7) returns a vector with magnitude > 0.05 | Orchestrator polls within 15s |
| AC-SF9 | `cluichetest.scalarfield.blue_peak` and `cluichetest.scalarfield.red_peak` metrics are both > 0.7 at steady state | Metric assertion in pytest |
| AC-SF10 | `cluichetest.scalarfield.contested_cell_count` metric (cells where |combined| < 0.1) is > 0 at frame 180+ | Metric assertion in pytest |
| AC-SF11 | Heatmap and gradient overlays render visually — Blue base is bright on blue heatmap, Red base is bright on red heatmap, front line arrows point inward from both sides on combined gradient overlay | Visual inspection |

---

## Design

### Scene Layout

```
+------------------------------------------------------------------+
|  [Field Stats panel — top-left ImGui]                            |
|  Title: "Scalar Field Influence"                                 |
|  Blue  peak:0.94  cells>0.5: 87                                  |
|  Red   peak:0.91  cells>0.5: 83                                  |
|  Front contested: 24 cells                                       |
|  Gradient probe (10,7): 0.31                                     |
|                                                                  |
|  [Checkpoints panel — right of stats panel]                      |
|  ✓ scalarfield.fields_initialized      PASS                      |
|  ✓ scalarfield.blue_steady_state       PASS                      |
|  ✓ scalarfield.red_steady_state        PASS                      |
|  ✓ scalarfield.walls_respected         PASS                      |
|  ✓ scalarfield.box_write_burst         PASS                      |
|  ✓ scalarfield.local_maxima_found      PASS                      |
|  ○ scalarfield.contested_zone_stable   PENDING                   |
|  ○ scalarfield.gradient_non_zero...    PENDING                   |
|                                                                  |
|  WORLD SPACE (20×15 grid, each cell 32px)                        |
|                                                                  |
|  [Blue heatmap — blue→white gradient]                            |
|  ████ Blue bases glow white (top-left)                           |
|  ░░░░ Influence fades toward centre                              |
|  ████ Walls render as black cells (value 0)                      |
|  ░░░░ Swamp strip dims noticeably                                |
|                                                                  |
|  [Combined heatmap overlay — blue→black→red]                     |
|  Blue side solid blue, Red side solid red                        |
|  Contested centre is near-black                                  |
|  Frame 60+: burst ripple visible as bright patch, then fades     |
|                                                                  |
|  [Gradient arrows on combined field]                             |
|  White arrows point TOWARD each faction's territory              |
|  Arrows flip direction at the front line                         |
|  Swamp strip: arrows dimmer / shorter (lower magnitude)          |
|                                                                  |
|  HUD: scalarfield_test_stage | ✓ 6/8 | f:210  ticking    ✕      |
+------------------------------------------------------------------+
```

### Grid Layout

```
Grid: 20 cols × 15 rows, 8-connected SquareFieldTopology, cell size = 32.0f world units

Blue bases (WriteRadial each frame, radius=2.5, peak=1.0, kQuadratic):
  Source A: cell (2, 2)
  Source B: cell (2, 12)

Red bases (WriteRadial each frame, radius=2.5, peak=1.0, kQuadratic):
  Source C: cell (17, 2)
  Source D: cell (17, 12)

Wall cells (SetBlocked = true) — two L-shapes creating corridors:
  Left wall:  (5,3), (5,4), (5,5), (5,6), (5,7), (5,8), (5,9), (5,10), (5,11)
  Right wall: (14,3),(14,4),(14,5),(14,6),(14,7),(14,8),(14,9),(14,10),(14,11)

Swamp strip (SetStaticModifier = 0.35f) — horizontal band across centre:
  All cells where y == 7 and 6 <= x <= 13

Burst event (WriteBox 3×3, value=0.8) at frame 60, centre (10,7):
  Blue field box: cells (9,6)–(11,8)  [promotes Blue into contested zone]

Second burst (WriteBox 3×3, value=0.8) at frame 180, centre (10,7):
  Red field box: cells (9,6)–(11,8)   [Red responds — tests that front line shifts back]

Gradient probe cell: (10, 7)  [centre of contested zone, always has a gradient]
```

### Fields

```
mBlueField:    SquareScalarField — UniformDecayPolicy(diffusion=0.8, decay=0.05), clamp [0,1]
mRedField:     DiaScalarField<SquareFieldTopology, RulesPropagationPolicy>
                 — modifier fn: returns 0.0 for blocked cells, 0.35 for swamp, 1.0 otherwise
                 — clamp [0,1]
mCombinedField: SquareScalarField — UniformDecayPolicy(diffusion=0, decay=0), clamp [-1,1]
                 — written each frame via Combine({blue,+1.0}, {red,-1.0})
                 — no ticking; purely a combination result
```

Using two different policies (UniformDecayPolicy and RulesPropagationPolicy) tests both code paths in the same stage. The Combined field uses Combine() directly rather than ticking, testing that path independently.

### Phase Timeline

| Frame range | Event |
|-------------|-------|
| 0 | Setup: SetBlocked walls, SetStaticModifier swamp, WriteRadial sources; all 3 fields Tick() starts |
| 0–60 | Fields propagate to steady state; heatmaps fill in visually |
| 60 | Blue WriteBox burst at centre (9,6)–(11,8); front line shifts Blue |
| 60–120 | Burst absorbed; fields re-equilibrate |
| 120 | Combine() begins writing mCombinedField every frame |
| 120–180 | Front line stabilises; local maxima and threshold queries run |
| 180 | Red WriteBox burst at centre (9,6)–(11,8); Red pushes back |
| 180–300 | Front line shifts Red, then re-equilibrates; stage passes at frame 300 |

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/ScalarFieldTestStageModule.h
class ScalarFieldTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;

    explicit ScalarFieldTestStageModule(const Dia::Core::StringCRC& instanceId);

    Dia::Core::StringCRC GetStageName() const override;
    unsigned int         GetBudgetFrames() const override { return 600; }

protected:
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool                        AreDependenciesReady() override;
    void                        OnStart(Dia::Automation::AutomationService* service) override;
    void                        OnUpdate(float deltaTime) override;
    void                        OnStop() override;

private:
    void SetupGrid();
    void TickFields();
    void ApplyBurst(Dia::ScalarField::SquareScalarField& field, bool isBlueField);
    void UpdateCombined();
    void UpdateMetrics();
    void RunSpatialQueries();

    Dia::ScalarField::SquareScalarField mBlueField;
    Dia::ScalarField::DiaScalarField<
        Dia::ScalarField::SquareFieldTopology,
        Dia::ScalarField::Adaptors::RulesPropagationPolicy<...>> mRedField;
    Dia::ScalarField::SquareScalarField mCombinedField;

    // Overlays (registered with VisualDebuggerModule under DIA_DEBUG)
    // ScalarFieldHeatmapOverlay for mBlueField
    // ScalarFieldHeatmapOverlay for mCombinedField
    // ScalarFieldGradientOverlay for mCombinedField

    // Checkpoint state
    bool mFieldsInitialized     = false;
    bool mBlueSteadyState       = false;
    bool mRedSteadyState        = false;
    bool mWallsRespected        = false;
    bool mBoxBurstPassed        = false;
    bool mLocalMaximaFound      = false;
    bool mContestedZoneStable   = false;
    bool mGradientNonZero       = false;

    // Metrics
    Dia::Observation::Metric::GaugeHandle mMetricBluePeak;
    Dia::Observation::Metric::GaugeHandle mMetricRedPeak;
    Dia::Observation::Metric::GaugeHandle mMetricContestedCount;
    Dia::Observation::Metric::GaugeHandle mMetricBlueCellsAboveHalf;
    Dia::Observation::Metric::GaugeHandle mMetricRedCellsAboveHalf;
    Dia::Observation::Metric::GaugeHandle mMetricGradientProbe;

    static constexpr int   kGridW         = 20;
    static constexpr int   kGridH         = 15;
    static constexpr float kCellSize      = 32.0f;
    static constexpr float kSteadyThresh  = 0.8f;
    static constexpr float kBurstFrame1   = 60;
    static constexpr float kBurstFrame2   = 180;
    static constexpr int   kMinFrames     = 300;

    Dia::ApplicationFlow::ModuleRef<AutomationModule>        mAutomation{this};
    Dia::ApplicationFlow::ModuleRef<VisualDebuggerModule>    mVisuals{this};
};
```

### Visual Rendering

| Element | Primitive | Colour |
|---------|-----------|--------|
| Blue heatmap cells | Filled rect | Lerp(RGB(0,20,80,200), RGB(180,220,255,200), value) |
| Combined heatmap cells | Filled rect | value<0: Lerp(black, RGB(60,140,255), -value); value>0: Lerp(black, RGB(255,60,60), value) |
| Combined gradient arrows | RequestDrawRay, arrowScale=0.35 | RGBA(255,255,255,160) |
| Wall cells | Filled rect | RGB(15,15,20,255) — near-black outline |
| Burst flash (10 frames after burst) | Filled rect, fading alpha | RGB(255,255,100,180) fading to 0 |
| Blue source markers | Circle r=8 | RGB(80,160,255,255) pulsing |
| Red source markers | Circle r=8 | RGB(255,80,80,255) pulsing |
| Local maxima markers | X cross | RGB(255,255,0,255) |
| Swamp strip tint | Filled rect, low alpha | RGB(80,120,30,40) |
| HUD bar | ImGui text | Stage name, checkpoint count, frame count, status |

### Pytest Scenario

```python
# Cluiche/Tests/E2E/scenarios/cluichetest/scalarfield_stage/smoke.py

_STAGE = "ScalarFieldTestStage"

_CHECKPOINTS = [
    ("scalarfield.fields_initialized",       2.0),
    ("scalarfield.blue_steady_state",        8.0),
    ("scalarfield.red_steady_state",         8.0),
    ("scalarfield.walls_respected",          8.0),
    ("scalarfield.box_write_burst",          5.0),   # fires at frame 60
    ("scalarfield.local_maxima_found",      12.0),
    ("scalarfield.contested_zone_stable",   15.0),
    ("scalarfield.gradient_non_zero_at_probe", 15.0),
]


def test_scalarfield_stage(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout)
            assert result["passed"], f"{cp}: {result['message']}"

        blue_peak = dia_client.get_metric("cluichetest.scalarfield.blue_peak")
        assert blue_peak > 0.7, f"Blue peak {blue_peak:.2f} below threshold"

        red_peak = dia_client.get_metric("cluichetest.scalarfield.red_peak")
        assert red_peak > 0.7, f"Red peak {red_peak:.2f} below threshold"

        contested = dia_client.get_metric("cluichetest.scalarfield.contested_cell_count")
        assert contested > 0, "No contested cells found — front line not forming"

    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
```

---

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/applications/cluichetest/systems/teststages/scalarfield-test-stage.md` | This spec |
| `Cluiche/CluicheTest/Modules/TestStages/ScalarFieldTestStageModule.h` | New stage module header |
| `Cluiche/CluicheTest/Modules/TestStages/ScalarFieldTestStageModule.cpp` | New stage module implementation |
| `Cluiche/Assets/CluicheTest/Stages/ScalarFieldTestStage/scalarfield_test_stage.diastage` | Stage manifest |
| `Cluiche/Assets/CluicheTest/Stages/ScalarFieldTestStage/misc/ApplicationFlow/scalarfield_test_stage.diaapp` | Module wiring |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Register stage module (stages: all → remove; stage-scoped only) |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Import new stage |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Cluiche/Tests/E2E/scenarios/cluichetest/scalarfield_stage/smoke.py` | E2E scenario |
| `Cluiche/Tests/E2E/plans/cluichetest/default.json` | Register scenario |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `ScalarFieldTestStageModule.h/.cpp` — `SetupGrid` (20×15 topology, walls, swamp static modifiers, all 3 field instances), register 8 checkpoints, register 6 metrics | Build passes; `fields_initialized` checkpoint fires on first Tick | Todo | sonnet | Core skeleton |
| 2 | Implement `TickFields` — `WriteRadial` sources each frame, `Tick()` on Blue + Red, `Combine` into Combined; implement burst events at frame 60 and 180 | `blue_steady_state`, `red_steady_state`, `walls_respected`, `box_write_burst` pass | Todo | sonnet | Depends on Task 1 |
| 3 | Implement `RunSpatialQueries` — `FindLocalMaxima` on Blue, `FindCellsAboveThreshold` on Combined, `GetGradient` probe; set checkpoint flags | `local_maxima_found`, `contested_zone_stable`, `gradient_non_zero_at_probe` pass | Todo | sonnet | Depends on Task 2 |
| 4 | Implement visuals — register HeatmapOverlay (Blue + Combined) and GradientOverlay (Combined) with VisualDebuggerModule; burst flash effect; source pulse markers; local maxima X markers; HUD bar | Visual inspection — all overlays render, front line visible | Todo | sonnet | Depends on Task 1 |
| 5 | Create `.diastage` + `.diaapp` manifest files | `dia validate manifest` passes | Todo | haiku | |
| 6 | Import stage in `cluichetest.diagame`; add source files to `CluicheTest.vcxproj` | Stage appears in Boot menu; clean build | Todo | haiku | Depends on Tasks 1, 5 |
| 7 | Write pytest scenario `scalarfield_stage/smoke.py`; register in `default.json` | Scenario collected by `dia test e2e --list` | Todo | haiku | Can run in parallel with Tasks 2–6 |
| 8 | `dia run cluichetest` — navigate to ScalarFieldTestStage, visual verify: heatmaps fill correctly, gradient arrows flip at front line, burst ripple visible at frames 60 and 180, all 8 checkpoints pass | Manual visual gate | Todo | sonnet | Requires Tasks 1–6 complete |
| 9 | Commit + `dia docs spec-done` | — | Todo | haiku | Requires Tasks 7 and 8 |

### Task Dependencies
- Tasks 1 and 5 can run in parallel.
- Task 2 depends on Task 1.
- Task 3 depends on Task 2.
- Task 4 depends on Task 1.
- Task 6 depends on Tasks 1 and 5.
- Task 7 can run in parallel with Tasks 1–6.
- Task 8 requires Tasks 1–6 complete.
- Task 9 requires Tasks 7 and 8.

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | `kTypeId`, checkpoint names (`scalarfield.*`), metric names — all `StringCRC` |
| PD-004 | No STL in public APIs | Module interface uses `DIA_MODULE`; `DynamicArrayC` for spatial query output |
| PD-006 | VS project files source of truth | Task 6 adds all new source files to `CluicheTest.vcxproj` |
| PD-007 | C++20 required | `constexpr StringCRC`, deduction guides on `RulesPropagationPolicy` |
| PD-010 | `.diagame` root; `.diastage` for stage metadata | `scalarfield_test_stage.diastage` imported in `cluichetest.diagame` |
| AD-001 | Three PUs | `ScalarFieldTestStageModule` on SimPU; no structural PU changes |
| SD-TS-001 | One manifest stage per feature | Exactly one stage: `ScalarFieldTestStage` |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | All 8 checkpoints registered in `OnStart` via `TestStageModuleBase` |
| SD-TS-003 | Metrics for threshold assertions | 6 metrics emitted each frame |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in `.diastage` |
| SD-TS-005 | Individual stages are feature specs | This IS the feature spec for this stage |

---

## Open Design Questions

| # | Question | Default if not revisited |
|---|----------|--------------------------|
| ODQ-1 | **RulesPropagationPolicy terrain fn**: The modifier function needs to read blocked/swamp state. Should it close over a lambda capturing the topology and a blocked-cell bitfield, or should a named struct with `operator()` be used for clarity? Affects only implementation style. | Lambda capturing local arrays |
| ODQ-2 | **Combined field Combine() timing**: Combine() is called every frame from frame 120 onward. Should it be called every frame from frame 0 (simpler, slightly more CPU) or only after steady state is reached? | Every frame from frame 0 — simpler |
| ODQ-3 | **Swamp visual tint**: The swamp strip's reduced static modifier creates a dimming in the gradient arrows. Should it also receive an explicit green tint overlay to make it legible as terrain? | Yes — low-alpha green rect over swamp cells |

---

## Status

`Approved`

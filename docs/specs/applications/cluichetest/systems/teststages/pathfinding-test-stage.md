# Feature Spec: PathfindingTestStage

## Parent System
@docs/specs/applications/cluichetest/systems/teststages/teststages.md

---

## Problem Statement

DiaPathfinding, DiaFlowField, and DiaSteering are each unit-tested in isolation but have never been run together under real PU timing in a visually observable stage. No E2E path exists to validate: A* producing a navigable path, that path generating a correct flow field, and agents actually steering to goal via that field — with dynamic re-pathing when the environment changes mid-run.

---

## Template Answers

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine features under test | `DiaPathfinding` (sync A*), `DiaFlowField` (flow field compute + cache + invalidation), `DiaSteering` (Arrive behaviour via SteeringSystem + SteeringPipeline) — full navigation stack integration |
| T2 | Scene layout | 20×15 `SquareFlowAdapter` grid with scattered L-shaped and diagonal obstacle clusters; 3 agents at distinct start corners; 1 shared goal at centre-right; obstacle block added at frame 60 to force mid-run recompute |
| T3 | Acceptance criteria | Shared ACs 1–9 + stage-specific ACs AC-P1 through AC-P8 (see below) |
| T4 | Metrics | `pathfinding.agent_0_dist_to_goal`, `pathfinding.agent_1_dist_to_goal`, `pathfinding.agent_2_dist_to_goal`, `pathfinding.recompute_count` |
| T5 | PU assignment | `PathfindingTestStageModule` on SimPU |
| T6 | Assets | `pathfinding_test_stage.diastage` + `pathfinding_test_stage.diaapp`; grid layout defined inline in C++ (no external data file needed) |
| T7 | Unit test gap | Unit tests call `ComputeFlowField` and `SteeringSystem::Update` in isolation with mock graphs. This stage validates: real dt flow to `SteeringSystem::Update` under SimPU 30Hz; `FlowFieldCache::Invalidate` + recompute path at runtime; multi-agent Arrive convergence with no artificial advance to goal. |
| T8 | Determinism | Fully deterministic — fixed grid layout, fixed agent starting positions, dynamic obstacle added at a fixed frame number |
| T9 | Frame budget | 3 agents × Arrive each frame; ComputeFlowField on a 20×15 grid is O(cells) Dijkstra — negligible at 30Hz |
| T10 | Dependencies | `AutomationModule` (checkpoints + metrics), `VisualDebuggerModule` (ShapeDrawer for grid/arrows/trails) |

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
| AC-S6 | Stage module cleans up all resources in DoStop, returns `StopResult::kDone` | No leaks |
| AC-S7 | Navigating Boot → Stage → Boot → Stage produces identical checkpoint results | Orchestrator scenario runs twice |
| AC-S8 | Checkpoint names follow convention: `pathfinding.<checkpoint_name>` | Code review |
| AC-S9 | Stage has a matching pytest scenario file | File exists at `Cluiche/Tests/E2E/scenarios/cluichetest/pathfinding/test_pathfinding_stage.py` |

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-P1 | `pathfinding.path_computed` checkpoint passes: `FindPath` from agent-0's start cell to goal cell returns `PathResult::success == true` | Orchestrator polls within 5 frames of DoStart |
| AC-P2 | `pathfinding.flow_field_ready` checkpoint passes: `FlowField::IsComplete() == true` and all passable cells have `FlowCell::reachable == true` | Orchestrator polls within 10 frames of DoStart |
| AC-P3 | `pathfinding.recomputed_after_block` checkpoint passes: at frame 60 a new impassable cell is set, `FlowFieldCache::Invalidate` is called, and a new flow field is computed before the next update | Orchestrator polls within 5 frames of frame 60 |
| AC-P4 | `pathfinding.first_agent_arrived` checkpoint passes: at least one agent's world position is within `kArrivalRadius` of the goal world position | Orchestrator polls within 600 frames (20 s at 30 Hz) |
| AC-P5 | `pathfinding.all_agents_arrived` checkpoint passes: all 3 agents within `kArrivalRadius` of goal | Orchestrator polls within 900 frames (30 s at 30 Hz) |
| AC-P6 | Agents move at visible speed — journey from start to goal takes at least 3 seconds of real stage time | Visual inspection; stage enforces `kMinDisplayFrames = 300` |
| AC-P7 | Flow field arrows render on every passable cell, color-coded by distance-to-goal (green = near, red = far) | Visual inspection |
| AC-P8 | Each agent renders a fading trail (last 20 positions) and a velocity arrow; goal renders a pulsing circle | Visual inspection |

---

## Design

### Scene Layout

```
+------------------------------------------------------------------+
|  [Nav Stack panel — top-left ImGui]                              |
|  Title: "Navigation Stack"                                       |
|  ▼ Path   cells:47  cost:12.4                                    |
|  ▼ Flow   reachable:272  recomputes:1                            |
|  ▼ Agents                                                        |
|     Agent0  dist:0.0  ARRIVED                                    |
|     Agent1  dist:3.2  steering                                   |
|     Agent2  dist:8.7  steering                                   |
|                                                                  |
|  [Checkpoints panel — right of nav panel]                        |
|  ✓ pathfinding.path_computed          PASS                       |
|  ✓ pathfinding.flow_field_ready       PASS                       |
|  ✓ pathfinding.recomputed_after_block PASS                       |
|  ✓ pathfinding.first_agent_arrived    PASS                       |
|  ○ pathfinding.all_agents_arrived     PENDING                    |
|                                                                  |
|  WORLD SPACE (20×15 grid)                                        |
|                                                                  |
|  ░░░░░░░░░░░░░░░░░░░░   (faint grid lines, passable cells)      |
|  ███░░░░░░░░░░░░░░░░░░   (dark filled obstacle cells)           |
|  ░░░███░░↑↗→→↘↓↙←↙↓░  (flow arrows, green→red gradient)       |
|  ░░░░░░↑↗━━━━━━━━━━━░  (yellow highlighted A* path cells)      |
|  [A]░░░░↑░░░░░░░░░⊙░░   [A]=agent circle, ⊙=goal pulse         |
|  ░░░░[B]░░░░░░░░░░░░░                                           |
|     ░░░░░░░░░░░[C]░░░                                           |
|  (agent trails: fading dots behind each agent)                   |
|  (velocity arrows: short lines from agent center)                |
|                                                                  |
|  HUD: pathfinding_test_stage | ✓ 4/5 | f:482  steering   ✕      |
+------------------------------------------------------------------+
```

- **Nav Stack panel** (ImGui, top-left, ~260px): path stats, flow field stats, per-agent distance to goal + state (steering / arrived).
- **Checkpoints panel** (ImGui, right of nav panel, ~260px): 5 checkpoint rows with PASS/PENDING/FAIL badges.
- **World space grid**: 20×15 cells, each ~32px. Obstacle cells filled dark; passable cells faint grid lines.
- **Flow arrows**: one arrow per passable cell pointing in `FlowCell::direction`; color interpolated green→red by normalised Dijkstra distance.
- **Path cells**: A* result cells highlighted yellow.
- **Agents**: circles of radius 10px; three distinct colors (blue, orange, purple); velocity arrow (16px line); fading trail (last 20 positions, opacity proportional to age).
- **Goal**: white circle, radius oscillates 8–14px over a 1.5s period.
- **HUD bar** (28px, bottom): stage name | checkpoint badge | frame counter | current state | exit button.

### Grid and Obstacle Layout

```
Grid: 20 cols × 15 rows, 8-connected SquareFlowAdapter, cell size = 32.0f world units

Static obstacles (set at DoStart):
  L-wall:  (3,2)–(3,7), (3,7)–(7,7)
  Diagonal barrier: (10,3), (11,4), (12,5), (13,6)
  Central block: (8,6)–(10,8)

Dynamic obstacle (added at frame 60):
  Row block: (6,10)–(9,10)   ← forces agents travelling bottom path to re-route

Agent starts (world positions mapped from cells):
  Agent 0 (blue):   cell (1, 1)    top-left
  Agent 1 (orange): cell (1, 13)   bottom-left
  Agent 2 (purple): cell (18, 13)  bottom-right

Goal: cell (16, 7)   centre-right
```

### Module Structure

Single module on SimPU — no dependency on a separate domain module.

```cpp
// Cluiche/CluicheTest/Modules/TestStages/PathfindingTestStageModule.h
namespace CluicheTest {

class PathfindingTestStageModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"PathfindingTestStageModule"};
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs =
        Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char*  kDescription    = "Full navigation stack: A* + FlowField + Steering";
    static constexpr unsigned int kMinDisplayFrames = 300;  // 10 s at 30 Hz

    explicit PathfindingTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void        DoUpdate(float deltaTime) override;
    StopResult  DoStop() override;

private:
    void BuildGrid();
    void ComputeInitialPath();
    void ComputeInitialFlowField();
    void AddDynamicObstacle();
    void RecomputeFlowField();
    void UpdateAgents(float dt);
    void SubmitVisuals();
    void RegisterCheckpoints(Dia::Automation::AutomationService& service);
    void RegisterMetrics(Dia::Automation::AutomationService& service);

    // Navigation stack
    Dia::FlowField::SquareFlowAdapter               mGrid;
    Dia::FlowField::FlowFieldCache                  mFlowCache;
    Dia::Pathfinding::PathResult                    mPath;
    Dia::FlowField::FlowField                       mFlowField;

    // Steering
    Dia::Steering::SteeringSystem                   mSteeringSystem;
    Dia::Steering::SteeringAgent                    mAgents[3];

    // State
    Dia::Core::Vector2D  mGoalWorld;
    Dia::Core::Vector2D  mTrails[3][20];
    int                  mTrailHead[3]     = {};
    int                  mFrameCount       = 0;
    int                  mRecomputeCount   = 0;
    bool                 mPathComputed     = false;
    bool                 mFlowFieldReady   = false;
    bool                 mRecomputedAfterBlock = false;
    bool                 mAgentArrived[3]  = {};

    static constexpr float kArrivalRadius  = 24.0f;
    static constexpr float kAgentMaxSpeed  = 80.0f;   // world units/s — ~2.7 cells/s at 30Hz
    static constexpr float kAgentMaxForce  = 200.0f;
    static constexpr float kSlowingRadius  = 64.0f;
    static constexpr float kCellSize       = 32.0f;

    Dia::ApplicationFlow::ModuleRef<AutomationModule>   mAutomation{this};
    Dia::ApplicationFlow::ModuleRef<VisualDebugModule>  mVisuals{this};
};

} // namespace CluicheTest
DIA_MODULE(PathfindingTestStageModule);
```

### Checkpoint Logic

```cpp
// pathfinding.path_computed
[this]() { return mPathComputed; }

// pathfinding.flow_field_ready
[this]() { return mFlowFieldReady && mFlowField.IsComplete(); }

// pathfinding.recomputed_after_block
[this]() { return mRecomputedAfterBlock; }

// pathfinding.first_agent_arrived
[this]() {
    for (int i = 0; i < 3; ++i)
        if (mAgentArrived[i]) return true;
    return false;
}

// pathfinding.all_agents_arrived
[this]() {
    for (int i = 0; i < 3; ++i)
        if (!mAgentArrived[i]) return false;
    return true;
}
```

### Update Loop

```cpp
void PathfindingTestStageModule::DoUpdate(float deltaTime)
{
    if (mFrameCount == 60 && !mRecomputedAfterBlock)
    {
        AddDynamicObstacle();
        RecomputeFlowField();
        mRecomputedAfterBlock = true;
        ++mRecomputeCount;
    }

    UpdateAgents(deltaTime);   // SteeringSystem::Update + Arrive from FlowField sample
    SubmitVisuals();            // grid, arrows, trails, agent circles, goal pulse
    EmitMetrics();

    ++mFrameCount;
}

void PathfindingTestStageModule::UpdateAgents(float dt)
{
    for (int i = 0; i < 3; ++i)
    {
        if (mAgentArrived[i]) continue;

        // Sample flow field at agent world position
        Dia::FlowField::FlowCell cell =
            mFlowField.SampleWorld(mAgents[i].position, kCellSize);

        // Use Arrive to decelerate near goal; use Seek along flow direction otherwise
        Dia::Core::Vector2D desiredVel = cell.reachable
            ? Dia::Steering::Arrive(mAgents[i], mGoalWorld, kSlowingRadius)
            : Dia::Core::Vector2D{0,0};

        mSteeringSystem.UpdateAgentState(StringCRC(i), mAgents[i]);
        mAgents[i].velocity += desiredVel * dt;

        // Clamp to max speed
        float speed = mAgents[i].velocity.Length();
        if (speed > kAgentMaxSpeed)
            mAgents[i].velocity = mAgents[i].velocity * (kAgentMaxSpeed / speed);

        mAgents[i].position += mAgents[i].velocity * dt;

        // Record trail
        mTrails[i][mTrailHead[i] % 20] = mAgents[i].position;
        ++mTrailHead[i];

        // Arrival check
        if ((mAgents[i].position - mGoalWorld).Length() < kArrivalRadius)
            mAgentArrived[i] = true;
    }
}
```

### Visual Rendering Detail

| Element | Primitive | Color |
|---------|-----------|-------|
| Passable cell background | Thin quad outline | RGB(60,60,70) |
| Obstacle cell fill | Filled quad | RGB(30,30,40) |
| Dynamic obstacle (added frame 60) | Filled quad | RGB(80,20,20) — red tint to distinguish |
| Flow arrow | Line + arrowhead | Lerp(green, red, normalisedDijkstraDist) |
| A* path cell highlight | Filled quad, alpha 60 | RGB(240,220,40) |
| Agent 0 (blue) circle + trail | Circle + dots | RGB(60,140,255) |
| Agent 1 (orange) circle + trail | Circle + dots | RGB(255,140,40) |
| Agent 2 (purple) circle + trail | Circle + dots | RGB(180,80,255) |
| Velocity arrow | Line | Same as agent color, brighter |
| Goal pulse | Circle, radius = 8+6*sin(t*4) | RGB(255,255,255) |
| Start markers | X cross | RGB(120,120,120) |

### Manifest Entry

`pathfinding_test_stage.diastage`:
```json
{
  "name": "PathfindingTestStage",
  "manifest": "Stages/PathfindingTestStage/misc/ApplicationFlow/pathfinding_test_stage.diaapp",
  "config": { "path_aliases": { "stage_root": "." } }
}
```

`pathfinding_test_stage.diaapp` (v3):
```json
{
  "version": 3,
  "processing_units": [
    { "instance_id": "MainPU", "frequency_hz": 30, "dedicated_thread": false, "modules": [] },
    {
      "instance_id": "SimPU", "frequency_hz": 30, "dedicated_thread": true,
      "modules": [{
        "instance_id": "PathfindingTestStageModule",
        "type_id": "PathfindingTestStageModule",
        "stages": ["PathfindingTestStage"],
        "dependencies": [],
        "channels": [
          { "id": "AutomationService", "role": "consumes" },
          { "id": "VisualDebugService", "role": "consumes" },
          { "id": "RenderToSim",        "role": "reads" }
        ]
      }]
    }
  ]
}
```

### Pytest Scenario

```python
# Cluiche/Tests/E2E/scenarios/cluichetest/pathfinding/test_pathfinding_stage.py

_STAGE = "PathfindingTestStage"

_CHECKPOINTS = [
    ("pathfinding.path_computed",          10),
    ("pathfinding.flow_field_ready",       10),
    ("pathfinding.recomputed_after_block", 75),   # fires at frame 60 + margin
    ("pathfinding.first_agent_arrived",   700),   # up to ~23 s at 30 Hz
    ("pathfinding.all_agents_arrived",   1000),   # up to ~33 s at 30 Hz
]

def test_pathfinding_stage(dia_client):
    dia_client.navigate_to(_STAGE)
    try:
        for cp, timeout_frames in _CHECKPOINTS:
            result = dia_client.poll_checkpoint(cp, timeout_s=timeout_frames / 30.0)
            assert result["passed"], f"{cp}: {result['message']}"

        for i in range(3):
            dist = dia_client.get_metric(f"pathfinding.agent_{i}_dist_to_goal")
            assert dist < 24.0, f"Agent {i} dist {dist:.1f} exceeds arrival radius"

        recomputes = dia_client.get_metric("pathfinding.recompute_count")
        assert recomputes >= 1, "Flow field was never recomputed after dynamic obstacle"
    finally:
        dia_client.abort_stage()
        dia_client.navigate_to("Boot")
```

---

## Files Touched

| File | Change |
|------|--------|
| `docs/specs/applications/cluichetest/systems/teststages/pathfinding-test-stage.md` | This spec |
| `Cluiche/CluicheTest/Modules/TestStages/PathfindingTestStageModule.h/.cpp` | New stage module |
| `Cluiche/Assets/CluicheTest/Stages/PathfindingTestStage/pathfinding_test_stage.diastage` | Stage manifest |
| `Cluiche/Assets/CluicheTest/Stages/PathfindingTestStage/misc/ApplicationFlow/pathfinding_test_stage.diaapp` | Module wiring |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Import new stage |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Cluiche/Tests/E2E/scenarios/cluichetest/pathfinding/test_pathfinding_stage.py` | E2E scenario |
| `Cluiche/Tests/E2E/plans/cluichetest/default.json` | Register scenario |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `PathfindingTestStageModule.h/.cpp` — `BuildGrid` (20×15 SquareFlowAdapter + static obstacles), `ComputeInitialPath` (sync FindPath), `ComputeInitialFlowField` (FlowFieldCache), register 5 checkpoints, register 4 metrics | Build passes; `path_computed` + `flow_field_ready` checkpoints fire | Todo | sonnet | Core module skeleton |
| 2 | Implement `UpdateAgents` — SteeringSystem + Arrive per agent, trail recording, arrival detection; `AddDynamicObstacle` + `RecomputeFlowField` at frame 60 | `recomputed_after_block` fires; agents converge on goal | Todo | sonnet | Depends on Task 1 |
| 3 | Implement `SubmitVisuals` — grid cells (passable/obstacle quads), flow arrows with green→red gradient, A* path highlight, agent circles + velocity arrows + fading trails, goal pulse | Visual inspection — all elements present | Todo | sonnet | Depends on Task 1 |
| 4 | Create `.diastage` + `.diaapp` manifest files | `dia validate manifest` passes | Todo | haiku | |
| 5 | Import stage in `cluichetest.diagame`; add source files to `CluicheTest.vcxproj` | Stage appears in Boot menu; clean build | Todo | haiku | |
| 6 | Write pytest scenario `test_pathfinding_stage.py`; register in `default.json` | `dia run googletest` scenario passes | Todo | sonnet | |
| 7 | `dia run cluichetest` — navigate to PathfindingTestStage, visual verify: flow arrows colored correctly, agents move at visible speed, reroute visible at frame 60, all checkpoints pass | Manual visual gate | Todo | sonnet | |
| 8 | Commit + update spec status → Done | — | Todo | haiku | |

### Task Dependencies

- Tasks 1, 4 can run in parallel.
- Task 2 depends on Task 1.
- Task 3 depends on Task 1.
- Task 5 depends on Tasks 1, 4 (needs module + manifest both present for build).
- Task 6 can run in parallel with Tasks 2–5.
- Task 7 requires Tasks 1–5 complete.
- Task 8 requires Tasks 6 and 7.

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | `kTypeId`, checkpoint names (`"pathfinding.path_computed"` etc.), metric names, agent IDs — all `StringCRC`. |
| PD-004 | No STL in public APIs | Module interface uses `DIA_MODULE`; `mAgents` is a plain C array; containers from `DiaCore`. |
| PD-006 | VS project files source of truth | Task 5 adds all new source files to `CluicheTest.vcxproj`. |
| PD-007 | C++20 required | `constexpr StringCRC`, standard range-for, `[[nodiscard]]`. |
| PD-010 | `.diagame` root; `.diastage` for stage metadata | `pathfinding_test_stage.diastage` imported in `cluichetest.diagame`. |
| AD-001 | Three PUs | `PathfindingTestStageModule` on SimPU; no structural PU changes. |
| SD-TS-001 | One manifest stage per feature | Exactly one stage: `PathfindingTestStage`. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | All 5 checkpoints registered in `DoStart`. |
| SD-TS-003 | Metrics for threshold assertions | 4 metrics emitted each frame. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in `.diastage`. |
| SD-TS-005 | Individual stages are feature specs | This IS the feature spec for this stage. |

---

## Open Design Questions

| # | Question | Default if not revisited |
|---|----------|--------------------------|
| ODQ-1 | **Dragon theming**: Should the 3 agents be framed as named entities (e.g. "Scout", "Flanker", "Rear") — or themed as dragon hatchlings if CoW integration is planned later? Affects label strings, ImGui panel copy, and narrative descriptions only; no structural impact. | Generic "Agent 0/1/2" labels |
| ODQ-2 | **Reroute visibility**: At frame 60 the dynamic obstacle is added and the flow field silently recomputes. Should the new obstacle flash briefly (e.g. 10-frame white outline) to make the reroute visually legible? | Flash for 10 frames |
| ODQ-3 | **Multiple goals**: This spec uses one shared goal for all 3 agents. A future extension could assign a unique goal per agent to test independent flow fields via `FlowFieldCache`. Worth noting but not in scope here. | Single shared goal |

---

## Status

`Approved`
**Plan:** @docs/specs/applications/cluichetest/systems/teststages/pathfinding-test-stage.plan.md

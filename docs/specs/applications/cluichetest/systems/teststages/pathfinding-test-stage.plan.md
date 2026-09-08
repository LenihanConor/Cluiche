**Spec:** @docs/specs/applications/cluichetest/systems/teststages/pathfinding-test-stage.md
**Status:** In Progress

## Implementation Patterns

### Module Pattern
`PathfindingTestStageModule` extends `TestStageModuleBase` (not `Dia::ApplicationFlow::Module` directly). The base handles DoStart/DoUpdate/DoStop final dispatch; the subclass implements `OnStart`, `OnUpdate`, `OnStop`, `GetStageName`, `GetBudgetFrames`, `GetCheckpointNames`.

### Navigation Stack Objects
- `Dia::Pathfinding::SquarePathGrid` owns passability state (20×15, k8Connected)
- `Dia::FlowField::SquareFlowAdapter` wraps the grid (holds `const&` — grid must outlive adapter)
- `Dia::Pathfinding::FlatCostProvider` for uniform 1.0f edge costs
- `Dia::FlowField::FlowFieldCache<SquareFlowAdapter>` owns the named field cache
- `Dia::FlowField::FlowField` current field (reference into cache via `GetOrCompute`)
- `Dia::Pathfinding::PathResult` from `Dia::Pathfinding::FindPath<SquarePathGrid>`

### Steering Pattern
- `Dia::Steering::SteeringAgent mAgents[3]` — plain array, not registered with `SteeringSystem` for this stage (system is optional; direct `Arrive()` call is simpler for a test stage with known flow field)
- `Dia::Steering::Arrive(agent, goalWorld, kSlowingRadius)` returns desired velocity
- Apply as: `agent.velocity = Lerp(agent.velocity, desiredVel, dt * kSteeringGain)` — framerate-independent blend

### Visual Drawer Pattern
- `PathfindingTestDrawer : Dia::Debug::IVisualDebugger` inside `#ifdef DIA_DEBUG`
- Registered on `VisualDebuggerModule.GetLayerManager()` in `OnStart`, unregistered in `OnStop`
- `Draw(IDebugDraw&)` uses: `RequestDrawRect` (cells), `RequestDrawRay` (flow arrows), `RequestDraw(circle)` (agents, goal)
- `DrawImGui()` renders Nav Stack panel + Checkpoints panel

### Checkpoint Registration
Called inside `OnStart(AutomationService*)` with lambda closures capturing `this`:
```cpp
service->RegisterCheckpoint(this, StringCRC("pathfinding.path_computed"),
    [this]() -> CheckpointResult { return { mPathComputed, ..., 0.f }; });
```

### Stage Name Convention
`GetStageName()` returns `StringCRC("PathfindingTestStage")` — must match the `"name"` field in `.diastage`.

---

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `PathfindingTestStageModule.h/.cpp` — module skeleton: `TestStageModuleBase` subclass, `BuildGrid` (20×15 + static obstacles), `ComputeInitialPath` (sync FindPath), `ComputeInitialFlowField` (FlowFieldCache), `OnStart` registers 5 checkpoints + 4 metrics | Build passes; `path_computed` + `flow_field_ready` checkpoint flags set in OnStart | Done | sonnet | Core nav stack + checkpoint wiring |
| 2 | Implement `UpdateAgents` — `Arrive()` steering per agent from flow field sample, velocity integration, trail ring-buffer, arrival detection; `AddDynamicObstacle` + `RecomputeFlowField` triggered at frame 60 | `recomputed_after_block` fires; all 3 agents converge to goal (visual) | Done | sonnet | Depends on Task 1 |
| 3 | Implement `PathfindingTestDrawer` — `IVisualDebugger` subclass: grid quads, flow arrows (green→red gradient), path highlight, agent circles+trails+velocity arrows, goal pulse, ImGui panels | Visual inspection: all elements present, colored correctly | Done | sonnet | Depends on Task 1 |
| 4 | Create `.diastage` + `.diaapp` manifest files at correct paths | `dia validate manifest` passes | Done | haiku | Can run parallel with Task 1 |
| 5 | Import stage in `cluichetest.diagame`; add `.h/.cpp` to `CluicheTest.vcxproj` | Clean build via `dia pipeline --target cluichetest` | Done | haiku | Build passes; added DiaPathfinding/DiaFlowField/DiaSteering project refs; fixed RGBA accessor in drawer; undef min/max guards in FindPath.h |
| 6 | Write pytest scenario `test_pathfinding_stage.py`; register in `default.json` | Scenario file present; manual check of checkpoint names | Done | sonnet | Depends on Tasks 2–5 |
| 7 | `dia run cluichetest` — navigate to PathfindingTestStage, visual verify all elements, all checkpoints pass | `dia run` output: no crash; all 5 checkpoints pass | Pending | sonnet | Requires Tasks 1–5 complete |
| 8 | `dia docs spec-done` + commit plan Done | Spec status = Done; plan Status = Done | Pending | haiku | Requires Tasks 6+7 |

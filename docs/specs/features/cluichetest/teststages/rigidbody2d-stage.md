# Feature Spec: RigidBody2D Test Stage

## Parent System
@docs/specs/systems/cluichetest/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007 |
| Application | @docs/specs/applications/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/systems/cluichetest/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Sibling | @docs/specs/features/cluichetest/teststages/test-stage-infrastructure.md | Depends on Tasks 1-2 (manifest loader + transitions field) |

## Problem Statement

Unit tests for DiaRigidBody2D call `Step()` manually with explicit `dt` values, bypassing the fixed-timestep accumulator and real PU scheduling. This stage validates multi-body dynamics under real SimPU timing — catching integration bugs between TimeServer, the fixed-timestep loop, and the physics solver that unit tests cannot reach.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature exercised | DiaRigidBody2D: multi-body dynamics (gravity, collision, sleep detection) |
| T2 | Scene setup in DoStart | 10 circles at known positions (grid layout), gravity enabled, no input, ground plane (static body) |
| T3 | Checkpoint(s) and success conditions | `rigid_body.all_settled` → true when all 10 bodies report `IsAsleep()` |
| T4 | Metrics emitted | `cluichetest.rigidbody.settle_frame_count` (frames to reach settled), `cluichetest.rigidbody.step_count` (physics steps executed) |
| T5 | Processing Unit | SimPU (physics runs there) |
| T6 | Assets needed | None — bodies created programmatically |
| T7 | Gap vs unit tests | Unit tests call Step() with manual dt; this validates fixed-timestep accumulator under real PU timing with jitter |
| T8 | Determinism constraints | Fixed seed (N/A — no randomness), fixed timestep (SimPU 30Hz), deterministic initial positions, no external input |
| T9 | Expected frame budget | ~200 frames (6.6s at 30Hz) to settle 10 circles |
| T10 | Dependencies on other modules | TimeServer (for dt), AutomationModule (for checkpoints), DiaRigidBody2D world/bodies |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-R1 | DoStart creates exactly 10 circle rigid bodies + 1 static ground plane | Code review + metric `step_count` > 0 confirms sim is running |
| AC-R2 | All 10 bodies start awake and at known grid positions | Checkpoint returns false on first frame (not yet settled) |
| AC-R3 | `rigid_body.all_settled` returns true only when ALL bodies report `IsAsleep()` | Orchestrator polls checkpoint → eventually returns `passed: true` |
| AC-R4 | `settle_frame_count` metric emitted with value > 0 once settled | Metric query after checkpoint passes returns non-zero value |
| AC-R5 | Stage settles within 300 frames (10s at 30Hz) | Orchestrator timeout; if exceeded, test fails |
| AC-R6 | Repeated runs (Boot → Stage → Boot → Stage) produce identical `settle_frame_count` ±0 | Determinism: same frame count both runs |

## Design

### Scene Layout

```
Y
^
|  O O O O O     (5 circles, row 2, y=8.0)
|  O O O O O     (5 circles, row 1, y=5.0)
|  ___________   (static ground plane, y=0.0)
+-----------------> X
   x: 1,2,3,4,5
```

- Circle radius: 0.5 units
- Circle mass: 1.0 kg
- Gravity: (0, -9.81) m/s²
- Ground: static body, width covers x=[0, 6]
- Restitution: 0.3 (some bounce, but settles quickly)
- Friction: 0.5

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/RigidBody2DTestModule.h
namespace CluicheTest {

class RigidBody2DTestModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"RigidBody2DTestModule"};
    explicit RigidBody2DTestModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    void SetupScene();
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    void EmitMetricsIfSettled();
    bool AreAllBodiesAsleep() const;

    Dia::ApplicationFlow::ModuleRef<AutomationModule> mAutomation{this};

    Dia::RigidBody2D::World* mWorld = nullptr;
    Dia::Core::Containers::DynamicArrayC<Dia::RigidBody2D::BodyHandle, 10> mCircleBodies;
    Dia::RigidBody2D::BodyHandle mGroundBody;

    unsigned int mFrameCount = 0;
    unsigned int mSettleFrame = 0;
    bool mSettled = false;
};

} // namespace CluicheTest
DIA_MODULE(RigidBody2DTestModule);
```

### Checkpoint Logic

```cpp
StartResult RigidBody2DTestModule::DoStart()
{
    SetupScene();

    auto* automation = mAutomation->GetService();
    automation->RegisterCheckpoint(this, StringCRC("rigid_body.all_settled"),
        [this]() -> CheckpointResult {
            return { mSettled, mSettled ? "all 10 bodies at rest" : "bodies still moving", 0.0f };
        });

    DIA_LOG_INFO("CluicheTest", "RigidBody2DTestModule::DoStart — 10 circles + ground, checkpoint registered");
    return StartResult::kReady;
}
```

### Update Loop

```cpp
void RigidBody2DTestModule::DoUpdate(float deltaTime)
{
    ++mFrameCount;

    if (!mSettled && AreAllBodiesAsleep())
    {
        mSettled = true;
        mSettleFrame = mFrameCount;
        EmitMetricsIfSettled();
    }
}
```

Metrics are emitted once at settle time — not every frame.

### Manifest Entry

```json
{
    "name": "RigidBody2DStage",
    "manifest": "stages/RigidBody2DStage/misc/ApplicationFlow/rigidbody2d_stage.diaapp",
    "config": {
        "path_aliases": { "stage_root": "." }
    },
    "transitions": ["Boot"],
    "auto_advance": false
}
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/rigidbody2d/test_rigidbody2d_settle.py

def test_rigidbody2d_all_bodies_settle(dia_client):
    """Navigate to RigidBody2DStage, wait for settle checkpoint."""
    dia_client.navigate_to("RigidBody2DStage")

    result = dia_client.poll_checkpoint("rigid_body.all_settled", timeout_s=12.0)
    assert result["passed"], f"Bodies did not settle: {result['message']}"

    metrics = dia_client.query_metrics("cluichetest.rigidbody.*")
    assert metrics["cluichetest.rigidbody.settle_frame_count"] > 0
    assert metrics["cluichetest.rigidbody.settle_frame_count"] < 300

    dia_client.navigate_to("Boot")
```

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/RigidBody2DTestModule.h` | New — module header |
| `Cluiche/CluicheTest/Modules/TestStages/RigidBody2DTestModule.cpp` | New — module implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/Stages/RigidBody2DStage/misc/ApplicationFlow/rigidbody2d_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/Stages/RigidBody2DStage/rigidbody2d_stage.diastage` | New — stage declaration with transitions |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for RigidBody2D stage |
| `Tools/orchestrator/scenarios/cluichetest/rigidbody2d/test_rigidbody2d_settle.py` | New — pytest scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create RigidBody2DTestModule (.h/.cpp) | Compiles, module registered | Done | sonnet | Follow pattern from system spec |
| 2 | Implement DoStart: SetupScene + RegisterCheckpoints | Module starts, checkpoint queryable | Done | sonnet | 10 circles + ground + checkpoint registration |
| 3 | Implement DoUpdate: frame counting + settle detection | `all_settled` checkpoint returns true after settle | Done | sonnet | AreAllBodiesAsleep() check each frame |
| 4 | Implement DoStop: cleanup | No leaks, checkpoint auto-cleared | Done | haiku | Release world + bodies |
| 5 | Create stage manifest files (.diastage + .diaapp) | Manifest loads, stage appears in stages list | Done | haiku | transitions: ["Boot"] |
| 6 | Add stage import to cluichetest.diagame | Stage navigable from Boot | Done | haiku | |
| 7 | Add vcxproj + filters entries | Builds in VS | Done | haiku | |
| 8 | Write pytest scenario | `dia orchestrate` runs, checkpoint validates | Done | sonnet | poll_checkpoint + metric assertion |
| 9 | Add scenario to plan JSON | `--list` shows rigidbody2d scenario | Done | haiku | |
| 10 | Verify: full E2E pass (Boot → RigidBody2DStage → settle → Boot) | Orchestrator green | Done | sonnet | `TestStageModuleBase` extracted; pattern proven |

## Dependencies

- **Infrastructure spec Tasks 1-6** must be complete before Task 10 (E2E verification)
- Tasks 1-7 can proceed independently (module + manifest work)
- Task 8 requires DiaClient `poll_checkpoint` and `query_metrics` methods to exist

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint name `"rigid_body.all_settled"`, stage name `"RigidBody2DStage"` all StringCRC. |
| PD-004 | No STL in public APIs | Module interface uses Dia types. `std::function` used only in checkpoint callback (callable, not public API). |
| PD-006 | VS project files source of truth | Task 7 adds files to CluicheTest.vcxproj. |
| PD-007 | C++20 required | constexpr StringCRC, standard C++ features. |
| AD-001 (CT) | Three PUs | Module lives on SimPU (physics runs there). |
| AD-004 (CT) | Test levels included | This IS a test level — direct expression of the decision. |
| AD-005 (CT) | App is testbed not product | Stage exists purely for engine validation. |
| SD-TS-001 | One manifest stage per feature | One stage: RigidBody2DStage. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | `RegisterCheckpoint` called in DoStart. Auto-clear via DiaAutomation (module owner cleanup). |
| SD-TS-003 | Metrics for threshold assertions | `settle_frame_count` and `step_count` emitted at settle time. Pytest asserts `< 300`. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec for this stage. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Determinism | Can `settle_frame_count` vary between runs due to floating-point scheduling jitter on SimPU? | No — SimPU runs at fixed 30Hz timestep. The accumulator guarantees exactly one Step() per frame when dt <= fixed step. Initial conditions are identical. Float arithmetic is deterministic for same inputs on same platform (x64, no fast-math). AC-R6 enforces ±0 tolerance. |
| 2 | Scene | Why 10 circles instead of fewer? | Exercises broad-phase and multi-body solver paths. A single body wouldn't test island sleeping or inter-body contact resolution. 10 is enough to stress the solver without making the test slow. |
| 3 | Timeout | AC-R5 says 300 frames. What if physics params change and it takes longer? | 300 is generous (50% headroom over expected ~200). If physics params change, update this AC. The spec owns the expected budget — it's a correctness gate, not a performance test. |
| 4 | Metrics | Should metrics be emitted every frame or only at settle? | Only at settle. Per-frame emission would flood MetricRegistry. The orchestrator queries metrics after the checkpoint passes — by then the values are available. |
| 5 | World ownership | Who owns the RigidBody2D::World — the module or a shared physics system? | The module owns it. Test stages are self-contained: each creates its own World in DoStart and destroys it in DoStop. No shared physics state across stages (SD-TS-001 isolation). |
| 6 | poll_checkpoint | Does `DiaClient.poll_checkpoint` exist or need to be added? | It's a convenience wrapper: loop calling `validate(checkpoint)` with sleep until `passed=true` or timeout. If it doesn't exist in DiaClient yet, Task 8 adds it as a helper method. Thin wrapper — not a spec concern. |

## Status

`Done` — 2026-05-28. `TestStageModuleBase` extracted; all 10 tasks complete; pattern proven for future stages.

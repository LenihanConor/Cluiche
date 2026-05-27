# Feature Spec: StateMachineAnim Test Stage

## Parent System
@docs/specs/systems/cluichetest/teststages.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-006, PD-007, PD-010 |
| Application | @docs/specs/applications/cluichetest.md | AD-001, AD-004, AD-005 |
| System | @docs/specs/systems/cluichetest/teststages.md | SD-TS-001, SD-TS-002, SD-TS-003, SD-TS-004, SD-TS-005 |
| Sibling | @docs/specs/features/cluichetest/teststages/test-stage-infrastructure.md | Depends on Tasks 1-2 (manifest loader + transitions field) |

## Problem Statement

Unit tests for DiaStateMachine and DiaAnimation2D exercise each system in isolation — SM transitions are tested without animation, and animation playback is tested without SM-driven clip selection. This stage validates the cross-system coupling: a StateMachine driving animation clip selection on multiple entities simultaneously, under real SimPU fixed-timestep timing, with clips loaded from the asset pipeline.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature exercised | DiaStateMachine + DiaAnimation2D coupling: SM state → clip binding → playback verification |
| T2 | Scene setup in DoStart | 3 entities sharing the same SM definition (Idle/Walk/Jump states), each with a Rig2D + AnimController. Clips loaded from `.diaclip` assets. All start in Idle. |
| T3 | Checkpoint(s) and success conditions | `state_machine_anim.state_resolved` → true when all entities have transitioned through Idle→Walk→Jump sequence. `state_machine_anim.clip_correct` → true when each entity's active clip matches its current SM state. |
| T4 | Metrics emitted | `cluichetest.sm_anim.transitions_completed` (count of successful transitions across all entities), `cluichetest.sm_anim.clip_bind_frame_count` (frames from transition to correct clip playing) |
| T5 | Processing Unit | SimPU (fixed-timestep for deterministic animation evaluation) |
| T6 | Assets needed | 3x `.diaclip` files (idle_clip, walk_clip, jump_clip) — simple test clips with distinguishable pose data |
| T7 | Gap vs unit tests | Unit tests never exercise SM→Animation binding under real timing with asset-loaded clips on multiple entities simultaneously |
| T8 | Determinism constraints | Fixed timestep (SimPU 30Hz), deterministic transition triggers (frame-count based, not input), asset loading must complete before transitions begin |
| T9 | Expected frame budget | ~150 frames (5s at 30Hz) — 50 frames Idle, forced Walk at frame 50, forced Jump at frame 100, verify at frame 150 |
| T10 | Dependencies on other modules | TimeServer, AutomationModule, DiaStateMachine, DiaAnimation2D, DiaAssetRuntime (for clip loading) |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-SMA1 | DoStart creates 3 entities each with SM + AnimController + Rig2D | Code review + all 3 entities active after start |
| AC-SMA2 | All 3 entities share the same SM definition (Idle/Walk/Jump) | Code review — single SM template, 3 instances |
| AC-SMA3 | Animation clips loaded from `.diaclip` asset files (not programmatic) | Asset load calls in DoStart, StartResult::kReady only after load completes |
| AC-SMA4 | Transitions are forced at deterministic frame counts (frame 50: Walk, frame 100: Jump) | No input dependency; frame counter drives transitions |
| AC-SMA5 | `state_machine_anim.state_resolved` returns true only when all 3 entities have reached Jump state | Checkpoint polls → eventually true |
| AC-SMA6 | `state_machine_anim.clip_correct` returns true only when each entity's active clip matches expected clip for its current SM state | Validates the SM→clip binding table |
| AC-SMA7 | Repeated runs produce identical transition frame counts | Determinism: same frames both runs (AC-S7) |
| AC-SMA8 | All clips unloaded and entities destroyed in DoStop | No asset leaks, no dangling handles |

## Design

### Scene Layout

```
Entity 0     Entity 1     Entity 2
  [SM]         [SM]         [SM]        (same definition: Idle/Walk/Jump)
  [Rig2D]     [Rig2D]     [Rig2D]      (skeleton for pose evaluation)
  [AnimCtrl]  [AnimCtrl]  [AnimCtrl]   (clip playback, bound to SM state)
```

All entities start in Idle state with `idle_clip` playing. Transitions are forced by the module's update loop at fixed frame counts — no external input.

### State Machine Definition

```
States: Idle, Walk, Jump
Transitions:
  Idle → Walk  (trigger: "go_walk")
  Walk → Jump  (trigger: "go_jump")
```

The module fires triggers programmatically at known frame counts.

### Clip Binding Table

| SM State | Expected Clip Asset |
|----------|-------------------|
| Idle | `stages/StateMachineAnimStage/clips/idle_clip.diaclip` |
| Walk | `stages/StateMachineAnimStage/clips/walk_clip.diaclip` |
| Jump | `stages/StateMachineAnimStage/clips/jump_clip.diaclip` |

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/StateMachineAnimStageModule.h
namespace CluicheTest {

class StateMachineAnimStageModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"StateMachineAnimStageModule"};
    explicit StateMachineAnimStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    void SetupScene();
    void LoadClips();
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    void FireTransitionIfDue();
    bool AllEntitiesInState(Dia::Core::StringCRC targetState) const;
    bool AllClipsMatchState() const;

    Dia::ApplicationFlow::ModuleRef<AutomationModule> mAutomation{this};

    struct EntityData {
        Dia::StateMachine::Instance* stateMachine = nullptr;
        Dia::Animation2D::AnimController* animController = nullptr;
    };
    Dia::Core::Containers::DynamicArrayC<EntityData, 3> mEntities;

    Dia::Animation2D::ClipHandle mIdleClip;
    Dia::Animation2D::ClipHandle mWalkClip;
    Dia::Animation2D::ClipHandle mJumpClip;

    unsigned int mFrameCount = 0;
    bool mAllResolved = false;
    unsigned int mTransitionsCompleted = 0;

    static constexpr unsigned int kWalkFrame = 50;
    static constexpr unsigned int kJumpFrame = 100;
    static constexpr unsigned int kVerifyFrame = 150;
};

} // namespace CluicheTest
DIA_MODULE(StateMachineAnimStageModule);
```

### Checkpoint Logic

```cpp
// state_machine_anim.state_resolved — all entities reached Jump
automation->RegisterCheckpoint(this, StringCRC("state_machine_anim.state_resolved"),
    [this]() -> CheckpointResult {
        bool resolved = AllEntitiesInState(StringCRC("Jump"));
        return { resolved, resolved ? "all 3 in Jump" : "not all in Jump yet", 0.0f };
    });

// state_machine_anim.clip_correct — each entity's active clip matches SM state
automation->RegisterCheckpoint(this, StringCRC("state_machine_anim.clip_correct"),
    [this]() -> CheckpointResult {
        bool correct = AllClipsMatchState();
        return { correct, correct ? "all clips match state" : "clip mismatch detected", 0.0f };
    });
```

### Update Loop

```cpp
void StateMachineAnimStageModule::DoUpdate(float deltaTime)
{
    ++mFrameCount;
    FireTransitionIfDue();  // frame 50 → "go_walk", frame 100 → "go_jump"

    if (mFrameCount >= kVerifyFrame && !mAllResolved)
    {
        if (AllEntitiesInState(StringCRC("Jump")) && AllClipsMatchState())
        {
            mAllResolved = true;
            EmitMetrics();
        }
    }
}
```

### Asset Loading Strategy

DoStart loads all 3 clips asynchronously. Returns `StartResult::kLoading` until all clip handles report ready. Only then does it return `StartResult::kReady` and register checkpoints. This ensures no transitions fire before clips are available.

### Manifest Entry

```json
{
    "name": "StateMachineAnimStage",
    "manifest": "stages/StateMachineAnimStage/misc/ApplicationFlow/sm_anim_stage.diaapp",
    "config": {
        "path_aliases": { "stage_root": "." }
    },
    "transitions": ["Boot"],
    "auto_advance": false
}
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/statemachineanim/test_sm_anim_transitions.py

def test_sm_anim_all_states_resolve(dia_client):
    """Navigate to SM+Anim stage, wait for all entities to reach Jump with correct clips."""
    dia_client.navigate_to("StateMachineAnimStage")

    result = dia_client.poll_checkpoint("state_machine_anim.state_resolved", timeout_s=8.0)
    assert result["passed"], f"States did not resolve: {result['message']}"

    result = dia_client.poll_checkpoint("state_machine_anim.clip_correct", timeout_s=2.0)
    assert result["passed"], f"Clip mismatch: {result['message']}"

    metrics = dia_client.query_metrics("cluichetest.sm_anim.*")
    assert metrics["cluichetest.sm_anim.transitions_completed"] == 6  # 3 entities * 2 transitions each

    dia_client.navigate_to("Boot")
```

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/StateMachineAnimStageModule.h` | New — module header |
| `Cluiche/CluicheTest/Modules/TestStages/StateMachineAnimStageModule.cpp` | New — module implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/Stages/StateMachineAnimStage/misc/ApplicationFlow/sm_anim_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/Stages/StateMachineAnimStage/sm_anim_stage.diastage` | New — stage declaration |
| `Cluiche/Assets/Stages/StateMachineAnimStage/clips/idle_clip.diaclip` | New — test idle clip |
| `Cluiche/Assets/Stages/StateMachineAnimStage/clips/walk_clip.diaclip` | New — test walk clip |
| `Cluiche/Assets/Stages/StateMachineAnimStage/clips/jump_clip.diaclip` | New — test jump clip |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for SM+Anim stage |
| `Tools/orchestrator/scenarios/cluichetest/statemachineanim/test_sm_anim_transitions.py` | New — pytest scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create StateMachineAnimStageModule (.h/.cpp) | Compiles, module registered | Todo | sonnet | |
| 2 | Create test .diaclip asset files (idle, walk, jump) | Assets load without error | Todo | sonnet | Minimal clip data, distinguishable poses |
| 3 | Implement DoStart: LoadClips + SetupScene + RegisterCheckpoints | Module starts after clips ready, checkpoints queryable | Todo | sonnet | kLoading until clips ready |
| 4 | Implement DoUpdate: frame-driven transitions + verification | Checkpoints pass after frame 150 | Todo | sonnet | FireTransitionIfDue at frames 50/100 |
| 5 | Implement DoStop: cleanup entities + release clip handles | No leaks, checkpoints auto-cleared | Todo | haiku | |
| 6 | Create stage manifest files (.diastage + .diaapp) | Stage appears in manifest stages list | Todo | haiku | transitions: ["Boot"] |
| 7 | Add stage import to cluichetest.diagame | Stage navigable from Boot | Todo | haiku | |
| 8 | Add vcxproj + filters entries | Builds in VS | Todo | haiku | |
| 9 | Write pytest scenario | Orchestrator validates both checkpoints + metric count | Todo | sonnet | |
| 10 | Add scenario to plan JSON | `--list` shows sm_anim scenario | Todo | haiku | |
| 11 | Verify: full E2E pass (Boot → Stage → verify → Boot) | Orchestrator green | Todo | sonnet | Requires Infrastructure complete |

## Dependencies

- **Infrastructure spec Tasks 1-6** must be complete before Task 11
- **DiaAnimation2D clip loading API** must support async handle-based loading
- **DiaStateMachine trigger API** must support programmatic trigger firing
- Tasks 1-8 can proceed independently (module + assets + manifest)

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint names, state names, trigger names all StringCRC. |
| PD-004 | No STL in public APIs | Module interface uses Dia types. std::function in checkpoint callback only. |
| PD-006 | VS project files source of truth | Task 8 adds files to CluicheTest.vcxproj. |
| PD-007 | C++20 required | constexpr StringCRC, standard features. |
| PD-010 | .diastage for stages | Stage declared in `.diastage` with `transitions: ["Boot"]`. Clips stored under stage asset directory. |
| AD-001 (CT) | Three PUs | Module lives on SimPU (fixed-timestep for deterministic animation). |
| AD-004 (CT) | Test levels included | This IS a test level. |
| AD-005 (CT) | App is testbed not product | Stage exists purely for cross-system validation. |
| SD-TS-001 | One manifest stage per feature | One stage: StateMachineAnimStage. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | RegisterCheckpoint called after clips loaded. Auto-clear on module stop. |
| SD-TS-003 | Metrics for threshold assertions | `transitions_completed` and `clip_bind_frame_count` emitted. Pytest asserts expected values. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Asset Loading | What if clip loading takes longer than expected — does the frame counter start before clips are ready? | No. DoStart returns `kLoading` until all clips report ready. Frame counting begins only after `kReady`. Transitions fire at frame 50/100 relative to first DoUpdate call. |
| 2 | Multiple Entities | Do 3 entities sharing one SM definition exercise anything that 1 entity wouldn't? | Yes — validates that SM instances are independent (no shared mutable state), and that AnimController binding works per-entity. A single entity can't catch bugs where SM state leaks between instances. |
| 3 | Clip Distinguishability | How does `AllClipsMatchState()` verify the correct clip is playing without visual inspection? | Each clip has a unique ID (StringCRC of asset path). AnimController exposes `GetActiveClipId()`. The check compares active clip ID against the expected clip for the current SM state from the binding table. No pose comparison needed. |
| 4 | Transition Timing | Why frame-based triggers instead of time-based? | Frame-based is deterministic on SimPU (fixed timestep = fixed frame count). Time-based would depend on accumulator precision. Frame count is the ground truth for test repeatability. |
| 5 | Metric: transitions_completed | Expected value is 6 (3 entities × 2 transitions). What if one entity fails to transition? | The metric counts successful transitions. Pytest asserts exactly 6. If fewer, the assertion fails — surfacing which entity/transition was missed. The checkpoint `state_resolved` would also fail (not all in Jump). |
| 6 | Dependencies | Does DiaAnimation2D's clip loading support async handle-based loading today? | If not, this becomes a blocker. The spec assumes handle-based async load exists. Add to Dependencies section — verify before implementation starts. |

## Status

`Approved` — 2026-05-22

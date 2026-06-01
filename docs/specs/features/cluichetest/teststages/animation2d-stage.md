# Feature Spec: Animation2D Test Stage

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

Unit tests for DiaAnimation2D evaluate clips with manual `Advance(dt)` calls, bypassing real frame timing and the asset pipeline. This stage validates end-to-end animation playback: loading clips from disk, playing them on a Rig2D skeleton under real SimPU timing, transitioning between clips sequentially, and verifying final bone poses match golden values — catching timing drift, asset loading failures, and skeleton evaluation bugs that unit tests cannot reach.

## Template Answers (T1–T10)

| # | Question | Answer |
|---|----------|--------|
| T1 | Engine feature exercised | DiaAnimation2D: clip playback, Rig2D skeleton evaluation, asset-loaded clips, sequential clip transitions |
| T2 | Scene setup in DoStart | 1 entity with Rig2D (4-bone skeleton: root/spine/arm_l/arm_r). 3 clips loaded from disk (idle, walk, jump). Starts playing idle. |
| T3 | Checkpoint(s) and success conditions | `animation2d.clip_completed` → true when all 3 clips have played to their last frame in sequence. `animation2d.pose_correct` → true when final bone transforms match golden values within tolerance. |
| T4 | Metrics emitted | `cluichetest.anim2d.clips_played` (count: 3 when done), `cluichetest.anim2d.total_playback_frames` (total frames across all 3 clips) |
| T5 | Processing Unit | SimPU (fixed-timestep for deterministic animation evaluation) |
| T6 | Assets needed | 3x `.diaclip` files + 1x `.diarig` skeleton definition |
| T7 | Gap vs unit tests | Unit tests never load clips from disk or evaluate a full skeleton under real SimPU frame timing. Asset pipeline integration + timing accumulation tested together. |
| T8 | Determinism constraints | Fixed timestep (SimPU 30Hz), clips have integer frame counts (evenly divisible by timestep), no blending randomness |
| T9 | Expected frame budget | ~180 frames (6s at 30Hz) — idle: 60 frames, walk: 60 frames, jump: 60 frames |
| T10 | Dependencies on other modules | TimeServer, AutomationModule, DiaAnimation2D (Rig2D, AnimController, ClipPlayer), DiaAssetRuntime |

## Acceptance Criteria

### Shared ACs (inherited from Infrastructure spec)

This stage satisfies AC-S1 through AC-S9 as defined in the [Infrastructure spec](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-A1 | DoStart creates 1 entity with Rig2D (4 bones) + AnimController | Code review + module starts successfully |
| AC-A2 | 3 clips + 1 rig definition loaded from `.diaclip`/`.diarig` asset files | Asset load in DoStart, kReady only after load completes |
| AC-A3 | Clips play sequentially: idle → walk → jump (auto-advance on clip completion) | Frame counter confirms expected sequence timing |
| AC-A4 | `animation2d.clip_completed` returns true only after all 3 clips have finished | Checkpoint polls → eventually true at ~frame 180 |
| AC-A5 | `animation2d.pose_correct` returns true only when final bone transforms match golden values (tolerance: ±0.001 per component) | Golden pose comparison after last clip ends |
| AC-A6 | Stage completes within 240 frames (8s at 30Hz) | Orchestrator timeout; headroom over expected 180 |
| AC-A7 | Repeated runs produce identical `total_playback_frames` | Determinism (AC-S7) |
| AC-A8 | All clips unloaded and rig released in DoStop | No asset leaks |

## Design

### Skeleton Definition

```
4-bone hierarchy:
  Root (identity)
    └─ Spine (translate Y+1)
        ├─ Arm_L (translate X-0.5)
        └─ Arm_R (translate X+0.5)
```

Simple enough to have predictable golden poses, complex enough to validate hierarchical evaluation.

### Clip Definitions

| Clip | Duration (frames) | Final Pose (Spine bone) | Purpose |
|------|-------------------|------------------------|---------|
| idle_clip | 60 | Spine.Y = 1.0, Arms at rest | Baseline — verifies skeleton holds bind pose |
| walk_clip | 60 | Spine.Y = 1.2, Arm_L.X = -0.7 | Moderate motion — verifies interpolation |
| jump_clip | 60 | Spine.Y = 2.0, Arms raised | Large motion — verifies full range evaluation |

### Sequential Playback Logic

```
Frame 0:   Start idle_clip
Frame 60:  idle_clip ends → start walk_clip
Frame 120: walk_clip ends → start jump_clip
Frame 180: jump_clip ends → mark complete, verify pose
```

The module listens for clip completion events (AnimController callback) and chains the next clip. No crossfade/blending — clean sequential playback.

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/Animation2DStageModule.h
namespace CluicheTest {

class Animation2DStageModule : public Dia::ApplicationFlow::Module
{
public:
    static constexpr Dia::Core::StringCRC kTypeId{"Animation2DStageModule"};
    explicit Animation2DStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    void SetupScene();
    void LoadAssets();
    void RegisterCheckpoints(Dia::Automation::AutomationService& automation);
    void OnClipFinished();
    bool VerifyGoldenPose() const;

    Dia::ApplicationFlow::ModuleRef<AutomationModule> mAutomation{this};

    Dia::Animation2D::Rig2D* mRig = nullptr;
    Dia::Animation2D::AnimController* mAnimController = nullptr;

    Dia::Animation2D::ClipHandle mIdleClip;
    Dia::Animation2D::ClipHandle mWalkClip;
    Dia::Animation2D::ClipHandle mJumpClip;
    Dia::Animation2D::RigHandle mRigHandle;

    unsigned int mFrameCount = 0;
    unsigned int mClipsPlayed = 0;
    bool mAllCompleted = false;
    bool mPoseCorrect = false;

    static constexpr float kPoseTolerance = 0.001f;
};

} // namespace CluicheTest
DIA_MODULE(Animation2DStageModule);
```

### Checkpoint Logic

```cpp
// animation2d.clip_completed — all 3 clips played to end
automation->RegisterCheckpoint(this, StringCRC("animation2d.clip_completed"),
    [this]() -> CheckpointResult {
        return { mAllCompleted, mAllCompleted ? "all 3 clips finished" : "playback in progress", 0.0f };
    });

// animation2d.pose_correct — final pose matches golden values
automation->RegisterCheckpoint(this, StringCRC("animation2d.pose_correct"),
    [this]() -> CheckpointResult {
        return { mPoseCorrect, mPoseCorrect ? "pose within tolerance" : "pose mismatch or not yet verified", 0.0f };
    });
```

### Golden Pose Verification

After the last clip finishes, `VerifyGoldenPose()` reads current bone transforms from Rig2D and compares against expected values:

```cpp
bool Animation2DStageModule::VerifyGoldenPose() const
{
    // Expected final pose after jump_clip (frame 60 of jump)
    auto spine = mRig->GetBoneWorldTransform(StringCRC("Spine"));
    auto armL = mRig->GetBoneWorldTransform(StringCRC("Arm_L"));
    auto armR = mRig->GetBoneWorldTransform(StringCRC("Arm_R"));

    return Dia::Maths::ApproxEqual(spine.position.y, 2.0f, kPoseTolerance)
        && Dia::Maths::ApproxEqual(armL.position.x, -0.7f, kPoseTolerance)
        && Dia::Maths::ApproxEqual(armR.position.x, 0.7f, kPoseTolerance);
}
```

### Manifest Entry

```json
{
    "name": "Animation2DStage",
    "manifest": "stages/Animation2DStage/misc/ApplicationFlow/anim2d_stage.diaapp",
    "config": {
        "path_aliases": { "stage_root": "." }
    },
    "transitions": ["Boot"],
    "auto_advance": false
}
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/animation2d/test_anim2d_playback.py

def test_anim2d_sequential_playback_and_pose(dia_client):
    """Navigate to Animation2DStage, verify clip completion and golden pose."""
    dia_client.navigate_to("Animation2DStage")

    result = dia_client.poll_checkpoint("animation2d.clip_completed", timeout_s=10.0)
    assert result["passed"], f"Clips did not complete: {result['message']}"

    result = dia_client.poll_checkpoint("animation2d.pose_correct", timeout_s=2.0)
    assert result["passed"], f"Pose mismatch: {result['message']}"

    metrics = dia_client.query_metrics("cluichetest.anim2d.*")
    assert metrics["cluichetest.anim2d.clips_played"] == 3
    assert metrics["cluichetest.anim2d.total_playback_frames"] == 180

    dia_client.navigate_to("Boot")
```

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/Animation2DStageModule.h` | New — module header |
| `Cluiche/CluicheTest/Modules/TestStages/Animation2DStageModule.cpp` | New — module implementation |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add new source files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/Stages/Animation2DStage/misc/ApplicationFlow/anim2d_stage.diaapp` | New — stage app manifest |
| `Cluiche/Assets/Stages/Animation2DStage/anim2d_stage.diastage` | New — stage declaration |
| `Cluiche/Assets/Stages/Animation2DStage/skeleton/test_rig.diarig` | New — 4-bone test skeleton |
| `Cluiche/Assets/Stages/Animation2DStage/clips/idle_clip.diaclip` | New — 60-frame idle |
| `Cluiche/Assets/Stages/Animation2DStage/clips/walk_clip.diaclip` | New — 60-frame walk |
| `Cluiche/Assets/Stages/Animation2DStage/clips/jump_clip.diaclip` | New — 60-frame jump |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import for Animation2D stage |
| `Tools/orchestrator/scenarios/cluichetest/animation2d/test_anim2d_playback.py` | New — pytest scenario |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add scenario to plan |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create Animation2DStageModule (.h/.cpp) | Compiles, module registered | Todo | sonnet | |
| 2 | Create test .diarig skeleton (4 bones) | Asset loads without error | Todo | sonnet | Root/Spine/Arm_L/Arm_R |
| 3 | Create 3x .diaclip test clips (idle/walk/jump, 60 frames each) | Assets load, correct frame counts | Todo | sonnet | Known final poses for golden comparison |
| 4 | Implement DoStart: LoadAssets + SetupScene + RegisterCheckpoints | Module starts after assets ready | Todo | sonnet | kLoading until all handles ready |
| 5 | Implement DoUpdate + OnClipFinished: sequential playback chaining | Clips play in order, mClipsPlayed increments | Todo | sonnet | Completion callback triggers next clip |
| 6 | Implement VerifyGoldenPose | Returns true for expected final bone transforms | Todo | sonnet | Tolerance ±0.001 |
| 7 | Implement DoStop: cleanup | No leaks, assets released | Todo | haiku | |
| 8 | Create stage manifest files (.diastage + .diaapp) | Stage appears in stages list | Todo | haiku | transitions: ["Boot"] |
| 9 | Add stage import to cluichetest.diagame | Stage navigable from Boot | Todo | haiku | |
| 10 | Add vcxproj + filters entries | Builds in VS | Todo | haiku | |
| 11 | Write pytest scenario | Both checkpoints pass + metric assertions | Todo | sonnet | |
| 12 | Add scenario to plan JSON | `--list` shows anim2d scenario | Todo | haiku | |
| 13 | Verify: full E2E pass (Boot → Stage → complete → Boot) | Orchestrator green | Todo | sonnet | Requires Infrastructure complete |

## Dependencies

- **Infrastructure spec Tasks 1-6** must be complete before Task 13
- **DiaAnimation2D** must support: async clip loading, clip completion callbacks, Rig2D bone world-transform queries
- **Asset format** for `.diaclip` and `.diarig` must be defined (may need authoring tool or hand-crafted JSON)

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kTypeId`, checkpoint names, bone names, clip IDs all StringCRC. |
| PD-004 | No STL in public APIs | Module interface uses Dia types. std::function in checkpoint callback only. |
| PD-006 | VS project files source of truth | Task 10 adds files to CluicheTest.vcxproj. |
| PD-007 | C++20 required | constexpr StringCRC, standard features. |
| PD-010 | .diastage for stages | Stage declared in `.diastage`. Assets stored under stage directory. |
| AD-001 (CT) | Three PUs | Module lives on SimPU (fixed-timestep for deterministic evaluation). |
| AD-004 (CT) | Test levels included | This IS a test level. |
| AD-005 (CT) | App is testbed not product | Stage exists purely for animation pipeline validation. |
| SD-TS-001 | One manifest stage per feature | One stage: Animation2DStage. |
| SD-TS-002 | Checkpoints in DoStart, auto-clear on stop | RegisterCheckpoint after assets loaded. Auto-clear on stop. |
| SD-TS-003 | Metrics for threshold assertions | `clips_played` and `total_playback_frames` emitted. Pytest asserts exact values. |
| SD-TS-004 | All stages return to Boot | `transitions: ["Boot"]` in manifest. |
| SD-TS-005 | Individual stages are feature specs | This spec IS the feature spec. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Asset Format | Do `.diaclip` and `.diarig` formats exist today, or do they need to be defined? | If they don't exist, this is a dependency. The clips need minimal data: per-frame bone transforms (position/rotation/scale per bone per frame). Format definition may belong in a DiaAnimation2D spec, not here. Flag as dependency — verify before implementation. |
| 2 | Golden Pose | Won't golden values break if animation evaluation order changes? | The golden values are derived from the clip data + skeleton hierarchy. If the evaluation algorithm changes, values change — and the test SHOULD fail. That's the point: catching solver regressions. Update golden values only when the change is intentional. |
| 3 | Clip Completion | How does AnimController signal clip end — callback, event, or poll? | The spec assumes a completion callback (OnClipFinished). If AnimController uses a different mechanism (e.g., clip state query), adapt the implementation. The checkpoint logic doesn't change — just the trigger for advancing to the next clip. |
| 4 | Timing | 60 frames per clip at 30Hz = 2s each. Is that realistic for test clips? | Yes. Test clips don't need to look good — they need predictable math. 60 frames gives enough interpolation steps to validate the evaluator without making the test slow. |
| 5 | Tolerance | Is ±0.001 appropriate for bone positions? | For a test skeleton with small coordinate ranges (0-2 units), 0.001 is tight enough to catch solver bugs but loose enough to absorb float rounding. If positions were in world-space thousands of units, we'd need relative tolerance. Here absolute is fine. |
| 6 | Pose Checkpoint Timing | `pose_correct` is registered in DoStart but only becomes meaningful after clip_completed. Is that confusing for the orchestrator? | No — the orchestrator polls `clip_completed` first (timeout 10s), then polls `pose_correct` (timeout 2s). The scenario enforces ordering. `pose_correct` returning false before clips finish is expected and harmless. |

## Status

`Done` — 2026-06-01

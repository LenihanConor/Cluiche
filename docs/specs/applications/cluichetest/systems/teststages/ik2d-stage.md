# Feature Spec: IK2D Test Stage

**Parent:** @docs/specs/applications/cluichetest/systems/teststages/teststages.md
**Status:** Done
**Plan:** [ik2d-stage.plan.md](ik2d-stage.plan.md)

---

## Problem Statement

DiaIK2D has 33 unit tests that verify solver math in isolation, but no CluicheTest stage exercises the three solvers under real SimPU timing against the actual dragon skeleton asset, or surfaces solver state visually. This stage closes that gap: it runs all three solvers (two-bone, FABRIK, look-at) on the dragon skeleton across three sequential phases, registers convergence checkpoints, and wires the four `DiaIK2DVisualDebugger` drawers so solver chains can be inspected at runtime.

---

## Scene Design

**Skeleton:** `dragon.diarig` — 7 bones (Root, Head, Tail, Wing_L, Wing_R, WingTip_L, WingTip_R). Shared with Animation2D stage; IK2D stage carries its own copy under `IK2DTestStage/skeleton/`.

**Three phases, one stage:**

| Phase | Frames | Solver | Chain | Target motion | Dragon anatomy |
|-------|--------|--------|-------|---------------|----------------|
| 1 | 0–89 | Two-Bone | Root → Wing_R → WingTip_R | Arc sweep above body | Right wing flap |
| 2 | 90–179 | FABRIK | Root → Wing_L → WingTip_L | Sinusoidal side-to-side | Left wing wave |
| 3 | 180–239 | Look-At | Head bone | Circular orbit around dragon | Head tracking |

Each phase has one "golden frame" where the target is at a known position and the result is verified.

---

## Acceptance Criteria

### Shared ACs (inherited)
Satisfies AC-S1 through AC-S9 as defined in [test-stage-infrastructure.md](test-stage-infrastructure.md#a--shared-acs-every-test-stage-must-satisfy-these).

### Stage-Specific ACs

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-IK-1 | `DoStart` loads `dragon.diarig` from disk, creates `Skeleton`, `Pose`, and `IKSolver`; registers chains before first `DoUpdate` | Stage reaches `kReady` only after asset loaded |
| AC-IK-2 | `SetRootTransform` called once per frame before all `Solve*` calls (SD-012 compliance) | Code review |
| AC-IK-3 | Phase 1 golden frame (frame 60): `WingTip_R` world position within 0.01 units of target | `ik2d.two_bone_converged` checkpoint true |
| AC-IK-4 | Phase 2 golden frame (frame 150): `WingTip_L` world position within 0.01 units of target | `ik2d.fabrik_converged` checkpoint true |
| AC-IK-5 | Phase 3 golden frame (frame 210): `Head` bone facing target within 0.05 radians | `ik2d.look_at_accurate` checkpoint true |
| AC-IK-6 | All three `Solve*` calls return `true` on their golden frames | Verified in checkpoint lambdas |
| AC-IK-7 | Four `DiaIK2DVisualDebugger` drawers (bones, joints, arrows, reach circles) visible in console under `"IK2D"` stage tab | Visual inspection in debug build |
| AC-IK-8 | `IK2DTestDrawer::DrawImGui()` shows: active phase name, solver type, target position, convergence status, pass/fail per phase | Visual inspection |
| AC-IK-9 | Stage completes within 300 frames (10s at 30Hz) | Orchestrator timeout headroom |
| AC-IK-10 | Metrics emitted: `cluichetest.ik2d.two_bone_error`, `cluichetest.ik2d.fabrik_error`, `cluichetest.ik2d.look_at_error` (float distance/angle values) | Pytest metric assertions |
| AC-IK-11 | Repeated runs produce identical error metric values | Determinism (AC-S7) |

---

## Design

### IK Chains

```
Phase 1 — Two-Bone (right wing flap):
  chain id:   "dragon.wing_r"
  startBone:  "Root"        (index 0)
  endBone:    "WingTip_R"   (index 6)
  path:       Root → Wing_R → WingTip_R   (3 bones, 2 joints)
  poleVector: direction (0, 1), weight 1.0   // bend upward
  target arc: semicircle above body, radius 1.6 (chain reach = 1.0 + 0.8 = 1.8)
  golden:     frame 60, target = (1.2, 0.8) world, expected WingTip_R ≈ (1.2, 0.8)

Phase 2 — FABRIK (left wing wave):
  chain id:   "dragon.wing_l"
  startBone:  "Root"        (index 0)
  endBone:    "WingTip_L"   (index 5)
  path:       Root → Wing_L → WingTip_L   (3 bones, 2 joints)
  target:     sinusoidal x=-1.5+0.5*sin(t), y=0.5*cos(t)
  golden:     frame 150, target = (-1.4, 0.3) world, expected WingTip_L ≈ (-1.4, 0.3)

Phase 3 — Look-At (head tracking):
  boneId:     "Head"        (index 1)
  axisAngleOffset: 0.0      (Head +X = forward)
  target:     circular orbit, radius 2.0, center (0,0)
  golden:     frame 210, target = (0, 2.0) world, expected Head.rotation ≈ π/2
```

### Phase Sequencing

```
Frame 0:   DoStart completes, Phase 1 begins — two-bone right wing
Frame 60:  Golden frame — verify WingTip_R, capture two_bone_error metric
Frame 90:  Transition to Phase 2 — FABRIK left wing
Frame 150: Golden frame — verify WingTip_L, capture fabrik_error metric
Frame 180: Transition to Phase 3 — look-at head
Frame 210: Golden frame — verify Head rotation, capture look_at_error metric
Frame 240: All phases done — ReportPassed() / ReportFailed()
```

### Module Structure

```cpp
// Cluiche/CluicheTest/Modules/TestStages/IK2DTestStageModule.h
class IK2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr PUAffinity kAllowedPUs = PUAffinity::kSim;
    static constexpr const char* kDescription =
        "Validates DiaIK2D: two-bone right wing, FABRIK left wing, look-at head on dragon skeleton";

protected:
    Dia::Core::StringCRC GetStageName()  const override;
    unsigned int         GetBudgetFrames() const override { return 300; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;

    void OnStart(AutomationService*) override;
    void OnUpdate(float deltaTime)   override;
    void OnStop()                    override;

private:
    void LoadAssets();
    void RegisterChains();
    void RegisterCheckpoints(AutomationService*);
    void RunPhase1(int phaseFrame);
    void RunPhase2(int phaseFrame);
    void RunPhase3(int phaseFrame);
    bool VerifyTwoBone() const;
    bool VerifyFABRIK()  const;
    bool VerifyLookAt()  const;

    std::unique_ptr<Dia::Rig2D::Skeleton>  mSkeleton;
    std::unique_ptr<Dia::Rig2D::Pose>      mPose;
    std::unique_ptr<Dia::IK2D::IKSolver>   mSolver;

    int  mPhase              = 0;
    int  mPhaseFrame         = 0;
    bool mTwoBoneConverged   = false;
    bool mFABRIKConverged    = false;
    bool mLookAtAccurate     = false;
    float mTwoBoneError      = FLT_MAX;
    float mFABRIKError       = FLT_MAX;
    float mLookAtError       = FLT_MAX;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<IK2DTestDrawer> mDrawer;
#endif
};
```

### Checkpoint Logic

```cpp
service->RegisterCheckpoint(this, StringCRC("ik2d.two_bone_converged"),
    [this]() -> CheckpointResult {
        return { mTwoBoneConverged,
                 mTwoBoneConverged ? "WingTip_R within tolerance" : "two-bone phase not yet complete",
                 0.0f };
    });
service->RegisterCheckpoint(this, StringCRC("ik2d.fabrik_converged"),
    [this]() -> CheckpointResult {
        return { mFABRIKConverged,
                 mFABRIKConverged ? "WingTip_L within tolerance" : "FABRIK phase not yet complete",
                 0.0f };
    });
service->RegisterCheckpoint(this, StringCRC("ik2d.look_at_accurate"),
    [this]() -> CheckpointResult {
        return { mLookAtAccurate,
                 mLookAtAccurate ? "Head facing target within tolerance" : "look-at phase not yet complete",
                 0.0f };
    });
```

### Visual Debugger Wiring

`IK2DTestDrawer` holds const references to `IKSolver` and `Skeleton`. In `Draw()` it delegates to all four engine drawers in priority order, then calls `DrawImGui()` with phase/solver status:

```cpp
// In IK2DTestStageModule::OnUpdate (lazy, first frame only):
mDrawer = std::make_unique<IK2DTestDrawer>(*mSolver, *mSkeleton, ...);
const Dia::Core::StringCRC tag("IK2D");
auto& mgr = vd->GetLayerManager();
mgr.Register(mDrawer.get(), 10, tag);   // bones+joints+arrows+reach all via single drawer
```

`IK2DTestDrawer::Draw()` instantiates the four engine drawers inline (same pattern as `Scene2DTestDrawer`) and calls each in sequence:

```cpp
IKChainBonesDrawer   bones  (*mSolver, *mSkeleton, mgr); bones.Draw(fd);
IKChainJointsDrawer  joints (*mSolver, *mSkeleton, mgr); joints.Draw(fd);
IKChainArrowsDrawer  arrows (*mSolver, *mSkeleton, mgr); arrows.Draw(fd);
IKReachCirclesDrawer reach  (*mSolver, *mSkeleton, mgr); reach.Draw(fd);
```

### Metrics

```cpp
MetricRegistry::Instance().SetGauge(StringCRC("cluichetest.ik2d.two_bone_error"), mTwoBoneError);
MetricRegistry::Instance().SetGauge(StringCRC("cluichetest.ik2d.fabrik_error"),   mFABRIKError);
MetricRegistry::Instance().SetGauge(StringCRC("cluichetest.ik2d.look_at_error"),  mLookAtError);
```

### Pytest Scenario

```python
# Tools/orchestrator/scenarios/cluichetest/ik2d/test_ik2d_solvers.py
def test_ik2d_all_solvers(dia_client):
    dia_client.navigate_to("IK2DTestStage")

    result = dia_client.poll_checkpoint("ik2d.two_bone_converged", timeout_s=5.0)
    assert result["passed"], f"Two-bone: {result['message']}"

    result = dia_client.poll_checkpoint("ik2d.fabrik_converged", timeout_s=5.0)
    assert result["passed"], f"FABRIK: {result['message']}"

    result = dia_client.poll_checkpoint("ik2d.look_at_accurate", timeout_s=5.0)
    assert result["passed"], f"Look-at: {result['message']}"

    metrics = dia_client.query_metrics("cluichetest.ik2d.*")
    assert metrics["cluichetest.ik2d.two_bone_error"] < 0.01
    assert metrics["cluichetest.ik2d.fabrik_error"]   < 0.01
    assert metrics["cluichetest.ik2d.look_at_error"]  < 0.05

    dia_client.navigate_to("Boot")
```

---

## Files Touched

| File | Change |
|------|--------|
| `Cluiche/CluicheTest/Modules/TestStages/IK2DTestStageModule.h` | New |
| `Cluiche/CluicheTest/Modules/TestStages/IK2DTestStageModule.cpp` | New |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/IK2DTestDrawer.h` | New |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/IK2DTestDrawer.cpp` | New |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add 4 new source files |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add filter entries |
| `Cluiche/Assets/Stages/IK2DTestStage/skeleton/dragon.diarig` | Copy of dragon skeleton |
| `Cluiche/Assets/Stages/IK2DTestStage/misc/ApplicationFlow/ik2d_stage.diaapp` | New stage app manifest |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add IK2DTestStage import |
| `Tools/orchestrator/scenarios/cluichetest/ik2d/test_ik2d_solvers.py` | New pytest scenario |
| `docs/specs/systems/cluichetest/teststages.md` | Add row to Features table |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `IK2DTestStageModule` (.h/.cpp) — skeleton, skeleton, IKSolver, 3 chains, phase loop, 3 verify methods | Compiles, module registered via `DIA_MODULE` | Todo | sonnet | |
| 2 | Create `IK2DTestDrawer` (.h/.cpp) — delegates to 4 engine drawers inline; ImGui shows phase/solver/status | Builds, layer name `"ik2d.overview"` returned | Todo | sonnet | Follow Scene2DTestDrawer inline-delegate pattern |
| 3 | Copy `dragon.diarig` to `IK2DTestStage/skeleton/` | Asset present under stage directory | Todo | haiku | |
| 4 | Create stage manifest (`ik2d_stage.diaapp`), add to `cluichetest.diagame` | Stage appears in stage list at Boot | Todo | haiku | `transitions: ["Boot"]` |
| 5 | Add all 4 source files to `CluicheTest.vcxproj` + `.vcxproj.filters` | Builds in VS | Todo | haiku | |
| 6 | Wire lazy drawer registration in `OnUpdate`; unregister in `OnStop` | Debug overlay visible when stage is active | Todo | sonnet | |
| 7 | Write pytest scenario; add to `default.json` plan | Both checkpoint + metric assertions pass | Todo | sonnet | |
| 8 | Verify: full pipeline pass (`dia run cluichetest`, `dia run googletest`) | No regressions | Todo | sonnet | |

---

## Binding Decisions

**From `teststages.md`:**

| ID | Decision | Compliance |
|----|----------|------------|
| SD-TS-001 | One manifest stage per feature | Single `IK2DTestStage` entry in manifest. |
| SD-TS-002 | Checkpoints registered in `DoStart`, auto-cleared on stop | All three checkpoints registered in `OnStart(AutomationService*)`. |
| SD-TS-003 | Emit metrics | Three float gauges emitted at golden frames. |
| SD-TS-004 | Returns to Boot | `transitions: ["Boot"]` in manifest. |

**From `diaik2d.md` (as a consumer of DiaIK2D):**

| ID | Decision | Compliance |
|----|----------|------------|
| SD-012 | `SetRootTransform` called once per frame before all `Solve*` | `OnUpdate` calls `mSolver->SetRootTransform(identity)` at top of each frame before `RunPhase*`. |
| SD-013 | IK reads/writes `Pose` local rotations, not `Skeleton` | Stage owns and holds a `Pose` separate from the `Skeleton`; IKSolver modifies only `Pose`. |
| SD-008 | `IKSolver` non-owning ref to `Skeleton&` | `IKSolver` constructed with `*mSkeleton`, which outlives the solver (both are `unique_ptr` members). |
| SD-009 | Bone IDs resolved at `RegisterChain` time | Chains registered once in `OnStart`; bone name StringCRCs (`"Root"`, `"Wing_R"`, etc.) resolved there. |

**From platform/app:**

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | Chain IDs, bone IDs, checkpoint names, module `kTypeId` all StringCRC. |
| PD-006 | VS project files source of truth | Task 5 adds files to `CluicheTest.vcxproj`. |
| AD-001 (CT) | Three PUs | Module lives on SimPU (`kAllowedPUs = PUAffinity::kSim`); IK is a post-FK transform pass. |

---

## Open Design Questions

**ODQ-1 — FABRIK chain depth:** The dragon skeleton has only 2 joints in the left wing chain (Root → Wing_L → WingTip_L). FABRIK is exercised but trivially (1 pass converges). If deeper FABRIK coverage is needed, the spec would require a longer tail chain added to `dragon.diarig`. Decision: accept shallow FABRIK for now — the code path is exercised; unit tests cover the N-joint behavior. Revisit if a multi-segment tail is added to the dragon.

**ODQ-2 — Golden tolerance:** Two-bone and FABRIK use 0.01 world units; look-at uses 0.05 radians. These are loose enough to absorb float accumulation at 30Hz but tight enough to catch solver regressions. If the solver is later optimized (fewer FABRIK iterations), the error may shift slightly — update golden tolerance then, not now.

**ODQ-3 — Drawer registration priority:** All four IK2D drawers are wired through the single `IK2DTestDrawer` wrapper at priority 10 (stage tag `"IK2D"`). This puts IK overlays beneath Coord2D (50–54) but at the same level as physics (10–14). If IK and physics stages ever run simultaneously (not currently possible), priorities would collide. Not a concern under SD-TS-001 (one stage at a time).

---

# Implementation Plan: IK2D Test Stage

**Spec:** [ik2d-stage.md](ik2d-stage.md)
**Status:** Done

---

## Session Notes

**What already exists (verified 2026-06-02):**

1. **TestStageModuleBase** — provides frame counting, budget/timeout, checkpoint auto-registration, ReportPassed/ReportFailed, entry count, linger frames pattern.

2. **DiaIK2D** — complete (Done 2026-05-02):
   - `IKSolver(Skeleton&, Pose&)` — non-owning, manages named chains
   - `SetRootTransform(BoneTransform)` — must call once per frame before any Solve*
   - `RegisterChain(IKChainDef)` — resolves bone IDs to indices at registration
   - `SolveTwoBone(chainId, target, poleVector*)` — analytic 2-joint solve
   - `SolveFABRIK(chainId, target)` — iterative N-joint
   - `SolveLookAt(boneId, target, weight, axisAngleOffset)` — single-bone rotation
   - `GetWorldTransforms()` — const ref to internal world transform array (for drawers)

3. **DiaIK2DVisualDebugger** — 4 drawers exist on disk (all take `const IKSolver&, const Skeleton&, const DebugLayerManager&`):
   - `IKChainBonesDrawer` — cyan bone lines
   - `IKChainJointsDrawer` — colour-coded joint circles
   - `IKChainArrowsDrawer` — direction rays
   - `IKReachCirclesDrawer` — max-reach outline circles

4. **dragon.diarig** — 7 bones (Root/Head/Tail/Wing_L/Wing_R/WingTip_L/WingTip_R). Bone lengths: Head=0.5, Tail=1.0, Wing_L=1.0, Wing_R=1.0, WingTip_L=0.8, WingTip_R=0.8. Root length=0.

5. **CluicheTest.vcxproj** — does NOT reference DiaIK2D or DiaIK2DVisualDebugger. These must be added as ProjectReferences:
   - DiaIK2D: `{F4A5B6C7-D8E9-0123-FABC-445566778899}`
   - DiaIK2DVisualDebugger: `{C3D4E5F6-A7B8-9012-CDEF-123456789012}`

6. **Asset loading** — `SerializedFileLoad::LoadNow<T, bufSize>(resolved, outObj, maxSize)`. `FilePath("stage_root", "skeleton", "dragon.diarig")` resolves via stage's `path_aliases.stage_root`.

7. **pipeline.toml** — needs `[targets.cluichetest.deploy_files]` entries for IK2D stage assets + `cluichetest.diagame` imports line.

**Spec corrections / design refinements:**

- Spec says chains are Root→Wing_R→WingTip_R (two-bone) and Root→Wing_L→WingTip_L (FABRIK). Looking at `dragon.diarig` hierarchy: Wing_R has parentIndex=0 (Root), WingTip_R has parentIndex=4 (Wing_R). So the chain from Root to WingTip_R spans 3 bones (Root→Wing_R→WingTip_R), 2 joints — correct for two-bone solver.
- `SolveTwoBone` requires exactly 2 joints (3 bones in chain span). Root→Wing_R→WingTip_R gives bones[0,4,6], indices differ but chain logic uses sequential bone walk from start to end — need to verify IKChainDef uses bone NAME resolution, which it does (startBoneId/endBoneId → resolved at RegisterChain).
- **Important:** `IKChainDef.startBoneId` and `endBoneId` are StringCRCs resolved to indices. The chain span is [startIdx..endIdx]. For two-bone: startBone=Wing_R, endBone=WingTip_R (2 bones, 1 joint) — wait, that's only 1 joint. For two-bone we need exactly 2 joints (3 bones). So the correct chain is startBone=Root, endBone=WingTip_R → path is Root(0)→Wing_R(4)→WingTip_R(6) but IK walks parent indices, not sequential indices.
- Actually re-reading IKSolver: chain span is computed by walking from endBone up to startBone via parentIndex. WingTip_R(parent=4=Wing_R), Wing_R(parent=0=Root), Root. So Root→Wing_R→WingTip_R = 2 joints. Correct.
- For FABRIK: same logic. Root→Wing_L→WingTip_L = Root(parent=-1), Wing_L(parent=0), WingTip_L(parent=3). Chain walk: WingTip_L→Wing_L→Root = 2 joints. Technically this is the same depth as two-bone (ODQ-1 in spec). FABRIK will still be exercised, just trivially convergent.
- Look-at: `SolveLookAt(StringCRC("Head"), target)` — straightforward single-bone.

---

## Implementation Patterns

### Module structure

```cpp
class IK2DTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "Validates DiaIK2D: two-bone right wing, FABRIK left wing, look-at head on dragon skeleton";
    explicit IK2DTestStageModule(const Dia::Core::StringCRC& instanceId);
    ~IK2DTestStageModule() override;

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 300; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    bool LoadAssets();
    void RegisterChains();
    Dia::Maths::Vector2D ComputePhase1Target(int phaseFrame) const;
    Dia::Maths::Vector2D ComputePhase2Target(int phaseFrame) const;
    Dia::Maths::Vector2D ComputePhase3Target(int phaseFrame) const;
    void VerifyPhase1();
    void VerifyPhase2();
    void VerifyPhase3();
    void EmitMetrics();

    // Rig
    Dia::Rig2D::SkeletonDef mSkeletonDef;
    std::unique_ptr<Dia::Rig2D::Skeleton> mSkeleton;
    std::unique_ptr<Dia::Rig2D::Pose> mPose;
    std::unique_ptr<Dia::IK2D::IKSolver> mSolver;

    // Phase state
    int mPhase = 0;         // 0=two-bone, 1=FABRIK, 2=look-at
    int mPhaseFrame = 0;
    Dia::Maths::Vector2D mCurrentTarget;

    // Results
    bool mTwoBoneConverged = false;
    bool mFABRIKConverged  = false;
    bool mLookAtAccurate   = false;
    float mTwoBoneError = FLT_MAX;
    float mFABRIKError  = FLT_MAX;
    float mLookAtError  = FLT_MAX;

    int mLingerFrames = 0;
    static constexpr int kLingerFrames = 60;

    // Phase boundaries
    static constexpr int kPhase1Frames = 90;
    static constexpr int kPhase2Frames = 90;
    static constexpr int kPhase3Frames = 60;
    static constexpr int kPhase1GoldenFrame = 60;
    static constexpr int kPhase2GoldenFrame = 60;  // relative to phase start
    static constexpr int kPhase3GoldenFrame = 30;  // relative to phase start

    // Tolerances
    static constexpr float kPositionTolerance = 0.01f;
    static constexpr float kAngleTolerance    = 0.05f;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<IK2DTestDrawer> mDrawer;
#endif
};
```

### Target animation functions

```cpp
// Phase 1: Arc sweep above body for right wing (two-bone)
// Semicircle from (1.0, 0.0) → (0.0, 1.6) → (-1.0, 0.0) scaled to wing reach
Vector2D ComputePhase1Target(int phaseFrame) const
{
    float t = static_cast<float>(phaseFrame) / static_cast<float>(kPhase1Frames);
    float angle = Dia::Maths::PI * t;  // 0→π over 90 frames
    float r = 1.6f;  // within wing reach of 1.8
    return Dia::Maths::Vector2D(r * std::cos(angle), r * std::sin(angle));
}

// Phase 2: Sinusoidal wave for left wing (FABRIK)
Vector2D ComputePhase2Target(int phaseFrame) const
{
    float t = static_cast<float>(phaseFrame) / static_cast<float>(kPhase2Frames);
    float x = -1.5f + 0.5f * std::sin(t * Dia::Maths::PI * 2.0f);
    float y = 0.5f * std::cos(t * Dia::Maths::PI * 2.0f);
    return Dia::Maths::Vector2D(x, y);
}

// Phase 3: Circular orbit for head look-at
Vector2D ComputePhase3Target(int phaseFrame) const
{
    float t = static_cast<float>(phaseFrame) / static_cast<float>(kPhase3Frames);
    float angle = t * Dia::Maths::PI * 2.0f;
    float r = 2.0f;
    return Dia::Maths::Vector2D(r * std::cos(angle), r * std::sin(angle));
}
```

### Golden verification

```cpp
void VerifyPhase1()
{
    // At golden frame, target is at known position. Check WingTip_R world pos.
    const auto& wt = mSolver->GetWorldTransforms();
    int tipIdx = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("WingTip_R"));
    Vector2D tipPos = wt[tipIdx].position;
    Vector2D target = ComputePhase1Target(kPhase1GoldenFrame);
    mTwoBoneError = (tipPos - target).Magnitude();
    mTwoBoneConverged = (mTwoBoneError < kPositionTolerance);
}

void VerifyPhase2()
{
    const auto& wt = mSolver->GetWorldTransforms();
    int tipIdx = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("WingTip_L"));
    Vector2D tipPos = wt[tipIdx].position;
    Vector2D target = ComputePhase2Target(kPhase2GoldenFrame);
    mFABRIKError = (tipPos - target).Magnitude();
    mFABRIKConverged = (mFABRIKError < kPositionTolerance);
}

void VerifyPhase3()
{
    const auto& wt = mSolver->GetWorldTransforms();
    int headIdx = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("Head"));
    float headRot = wt[headIdx].rotation;
    Vector2D target = ComputePhase3Target(kPhase3GoldenFrame);
    float expectedAngle = std::atan2(target.Y() - wt[headIdx].position.Y(),
                                     target.X() - wt[headIdx].position.X());
    mLookAtError = std::fabsf(headRot - expectedAngle);
    if (mLookAtError > Dia::Maths::PI)
        mLookAtError = 2.0f * Dia::Maths::PI - mLookAtError;
    mLookAtAccurate = (mLookAtError < kAngleTolerance);
}
```

### Drawer pattern (inline delegation — mirrors Scene2DTestDrawer)

```cpp
class IK2DTestDrawer : public Dia::Debug::IVisualDebugger
{
public:
    IK2DTestDrawer(
        const Dia::IK2D::IKSolver&           solver,
        const Dia::Rig2D::Skeleton&          skeleton,
        const int&                           phase,
        const int&                           phaseFrame,
        const Dia::Maths::Vector2D&          currentTarget,
        const bool&                          twoBoneConverged,
        const bool&                          fabrikConverged,
        const bool&                          lookAtAccurate,
        const float&                         twoBoneError,
        const float&                         fabrikError,
        const float&                         lookAtError,
        const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override;
    void Draw(Dia::Graphics::FrameData& frameData) override;
    void DrawImGui() override;

private:
    // const refs to solver + module state
};

// Draw() delegates to engine drawers inline:
void IK2DTestDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    Dia::IK2D::IKChainBonesDrawer   bones(mSolver, mSkeleton, mManager);
    bones.Draw(frameData);
    Dia::IK2D::IKChainJointsDrawer  joints(mSolver, mSkeleton, mManager);
    joints.Draw(frameData);
    Dia::IK2D::IKChainArrowsDrawer  arrows(mSolver, mSkeleton, mManager);
    arrows.Draw(frameData);
    Dia::IK2D::IKReachCirclesDrawer reach(mSolver, mSkeleton, mManager);
    reach.Draw(frameData);
}
```

### Asset stage structure

```
Cluiche/Assets/Stages/IK2DTestStage/
  ik2d_test_stage.diastage
  misc/ApplicationFlow/ik2d_test_stage.diaapp
  skeleton/dragon.diarig              (copy from Animation2DTestStage)
```

### .diastage format

```json
{
    "name": "IK2DTestStage",
    "manifest": "stages/IK2DTestStage/misc/ApplicationFlow/ik2d_test_stage.diaapp",
    "config": {
        "path_aliases": {
            "stage_root": "."
        }
    }
}
```

### .diaapp format

```json
{
    "version": 3,
    "processing_units": [
        {
            "instance_id": "MainPU",
            "frequency_hz": 30,
            "dedicated_thread": false,
            "modules": []
        },
        {
            "instance_id": "SimPU",
            "frequency_hz": 30,
            "dedicated_thread": true,
            "modules": [
                {
                    "instance_id": "IK2DTestStageModule",
                    "type_id": "IK2DTestStageModule",
                    "stages": ["IK2DTestStage"],
                    "dependencies": [],
                    "channels": [
                        { "id": "AutomationService", "role": "consumes" },
                        { "id": "RenderToSim", "role": "reads" }
                    ]
                }
            ]
        }
    ]
}
```

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-01 | Add DiaIK2D + DiaIK2DVisualDebugger as ProjectReferences in `CluicheTest.vcxproj` | `msbuild CluicheTest.vcxproj` succeeds with IK2D includes | Done | haiku | GUIDs: `{F4A5B6C7-D8E9-0123-FABC-445566778899}`, `{C3D4E5F6-A7B8-9012-CDEF-123456789012}` |
| T-02 | Create stage assets: `IK2DTestStage/ik2d_test_stage.diastage`, `misc/ApplicationFlow/ik2d_test_stage.diaapp`, copy `skeleton/dragon.diarig` | Files exist; JSON valid | Done | haiku | Follow Animation2D pattern exactly |
| T-03 | Add IK2DTestStage import to `cluichetest.diagame` | Stage appears in Boot menu | Done | haiku | `"stages/IK2DTestStage/ik2d_test_stage.diastage"` |
| T-04 | Add IK2DTestStage deploy entries to `pipeline.toml` | `dia pipeline --target cluichetest` deploys IK2D stage assets | Done | haiku | 3 entries: diastage, diaapp, skeleton/* |
| T-05 | Create `IK2DTestStageModule.h/.cpp` — full implementation: LoadAssets, RegisterChains, phase loop, 3 verify methods, EmitMetrics, checkpoints | `dia pipeline --target cluichetest` compiles; stage loads and cycles phases | Done | sonnet | Core implementation task — follows Animation2DTestStageModule pattern exactly |
| T-06 | Create `IK2DTestDrawer.h/.cpp` — inline-delegates to 4 engine drawers; ImGui shows phase/solver/target/error/pass-fail | Compiles; debug overlay shows IK2D tab with drawers enabled | Done | sonnet | Follow Scene2DTestDrawer inline-delegate pattern |
| T-07 | Add 4 source files (module .h/.cpp + drawer .h/.cpp) to `CluicheTest.vcxproj` + `.vcxproj.filters` | IDE shows files under `ApplicationFlow\Modules\TestStages\` and `\Drawers\` | Done | haiku | |
| T-08 | Verify: `dia run googletest` (no regressions) + `dia pipeline --target cluichetest` pass | 5662 tests pass (1 pre-existing flaky); cluichetest pipeline green | Done | sonnet | Pre-existing flaky test confirmed unrelated (passes in isolation) |

---

## Dependencies

```
T-01 (vcxproj refs) ──┐
T-02 (stage assets) ──┼── T-05 (module impl) ──┐
T-03 (diagame)     ──┘                          ├── T-08 (verify)
T-04 (pipeline)  ─────────────────────────────┘  │
                                                   │
T-05 ──── T-06 (drawer impl) ── T-07 (vcxproj) ──┘

Parallelism:
- T-01, T-02, T-03, T-04 are independent mechanical tasks — all in parallel (haiku)
- T-05 depends on T-01..T-04 (needs refs for compile, assets for load, pipeline for deploy)
- T-06 depends on T-05 (needs module types to reference)
- T-07 depends on T-05 + T-06 (need the files to exist)
- T-08 depends on everything
```

---

## Files Touched

| File | Change | Task |
|------|--------|------|
| `Cluiche/CluicheTest/CluicheTest.vcxproj` | Add DiaIK2D + DiaIK2DVisualDebugger refs; add 4 source files | T-01, T-07 |
| `Cluiche/CluicheTest/CluicheTest.vcxproj.filters` | Add 4 filter entries | T-07 |
| `Cluiche/Assets/Stages/IK2DTestStage/ik2d_test_stage.diastage` | New | T-02 |
| `Cluiche/Assets/Stages/IK2DTestStage/misc/ApplicationFlow/ik2d_test_stage.diaapp` | New | T-02 |
| `Cluiche/Assets/Stages/IK2DTestStage/skeleton/dragon.diarig` | New (copy) | T-02 |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add IK2D stage import | T-03 |
| `pipeline.toml` | Add deploy entries | T-04 |
| `Cluiche/CluicheTest/Modules/TestStages/IK2DTestStageModule.h` | New | T-05 |
| `Cluiche/CluicheTest/Modules/TestStages/IK2DTestStageModule.cpp` | New | T-05 |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/IK2DTestDrawer.h` | New | T-06 |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/IK2DTestDrawer.cpp` | New | T-06 |
| `docs/specs/features/cluichetest/teststages/ik2d-stage.md` | Status → Done (after T-08) | T-08 |

---

## Risks

| Risk | Mitigation |
|------|------------|
| IKSolver chain walk uses parent indices from skeleton — chain must form a connected path from end to start | Dragon hierarchy verified: WingTip_R→Wing_R→Root and WingTip_L→Wing_L→Root are valid parent chains |
| Two-bone requires exactly 2 joints (3 bones). Root→Wing_R→WingTip_R has bones at indices [0,4,6] but chain joint count = 2 (number of segments) | This is correct: IKSolver counts joints as (bones in chain - 1). Chain has 3 bones → 2 joints. SolveTwoBone requires == 2 joints. |
| FABRIK on a 2-joint chain converges trivially (1 iteration) — doesn't deeply exercise the iterative solver | Accepted per ODQ-1. The code path is exercised. Unit tests cover deep chains (5+ joints). |
| `GetWorldTransforms()` may not be updated after `SolveLookAt` (which doesn't use chains) | SolveLookAt triggers FK propagation on the modified bone per SD-002. Verify by reading the world transform post-solve. |
| DiaIK2DVisualDebugger drawers reference `IKSolver::GetWorldTransforms()` which requires `SetRootTransform` to have been called | Module calls `SetRootTransform(identity)` at the top of every `OnUpdate` before any solver dispatch — drawers always see a fresh cache. |
| CluicheTest doesn't currently link DiaIK2D — linker errors if refs missing | T-01 adds ProjectReferences first; T-05 won't compile without them. Task ordering enforced. |

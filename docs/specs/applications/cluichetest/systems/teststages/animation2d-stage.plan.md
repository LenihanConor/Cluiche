# Implementation Plan: Animation2D Test Stage

**Spec:** [animation2d-stage.md](animation2d-stage.md)
**Status:** Done
**Created:** 2026-06-01

---

## Session Notes

**What already exists (verified 2026-06-01):**

1. **TestStageModuleBase** (`Cluiche/CluicheTest/Modules/TestStages/TestStageModuleBase.h/.cpp`) — provides:
   - Frame counting, budget/timeout, TestResultsRegistry integration
   - AutomationService stream resolution + polling (`AreDependenciesReady()` gate)
   - Checkpoint auto-registration + auto-unregister in DoStop
   - ReportPassed/ReportFailed with deferred capture (RenderFence wait)
   - Entry counting for determinism (`GetEntryCount()`)

2. **DiaAnimation2D** — key types:
   - `AnimClipDef` — data struct: `id` (StringCRC), `duration` (float), `tracks` (DynamicArrayC<KeyframeTrack, 32>)
   - `AnimClip` — immutable compiled clip: `AnimClip(def, skeleton)`, `Sample(time, skeleton, outPose)`
   - `AnimClipPlayer` — playback state: `Play(clip, mode)`, `Stop()`, `Update(dt)`, `Sample(skeleton, outPose)`, `IsPlaying()`, `GetNormalizedTime()`, `GetCurrentClip()`
   - `AnimClipLoader::LoadAnimClipDefFromJson(Json::Value&)` — manual jsoncpp parser (legacy path)
   - `DiaAnimation2DSerializers.h` — DiaReflect `DIA_SERIALIZE` for `AnimClipDef`, `Keyframe`, `KeyframeTrack`

3. **DiaRig2D** — key types:
   - `SkeletonDef` — data struct: `id` (StringCRC), `bones` (DynamicArrayC<Bone, 128>)
   - `Skeleton` — compiled: `Skeleton(def)`, `FindBoneIndex(name)`, `GetBone(i)`, `GetBoneCount()`
   - `Pose` — per-skeleton local transforms: `Pose(skeleton)`, `GetLocalTransform(i)`, `SetToBindPose(skeleton)`, `ComputeWorldTransforms(skeleton, rootTransform, outWorld)`
   - `DiaRig2DSerializers.h` — DiaReflect `DIA_SERIALIZE` for `SkeletonDef`, `Bone`, `BoneTransform`
   - `JsonSkeletonSerializer` — legacy `ISkeletonSerializer` impl (loads SkeletonDef from raw JSON)

4. **SerializedFileLoad** (`DiaCore/FilePath/SerializedFileLoad.h`) — `LoadNow<T, bufSize>(filePath, outObject, maxSize)` — reads JSON file → `JsonReadArchive` → `serialize(ar, obj, 0)`. Works with any DIA_SERIALIZE type.

5. **VisualDebuggerModule** — SimPU module; accessed via `ModuleRef<VisualDebuggerModule>`. Exposes `GetLayerManager()`. Drawers register with stage tag for automatic console tab.

**Spec corrections / design refinements:**

- Spec assumes `AnimController`, `ClipHandle`, `RigHandle` — **none exist**. The module will drive `AnimClipPlayer` directly, polling `GetNormalizedTime() >= 1.0` for completion and chaining the next clip manually.
- Spec says `Rig2D::GetBoneWorldTransform(name)` — **doesn't exist**. Actual pattern: `Pose::ComputeWorldTransforms(skeleton, rootTransform, outWorldArray)` then index via `skeleton.FindBoneIndex(name)`.
- Asset loading: `SerializedFileLoad::LoadNow<SkeletonDef>` for `.diarig`, `SerializedFileLoad::LoadNow<AnimClipDef>` for `.diaclip`. Both use DiaReflect JSON. No DiaAssetRuntime integration needed — synchronous load in DoStart is sufficient for test data.
- No AnimController component — this is a **module-level test**, not a per-entity test. The module owns the skeleton, pose, player, and clips directly.

**Critical PU constraint:**
SimPU for deterministic fixed-timestep evaluation. TestStageModuleBase handles cross-PU AutomationService via ServiceStreamReader.

---

## Implementation Patterns

### Module structure (T-03)

```cpp
class Animation2DStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Validates DiaAnimation2D: clip loading, sequential playback, golden pose";
    explicit Animation2DStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override { return 240; }
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    bool LoadAssets();
    void AdvanceToNextClip();
    bool VerifyGoldenPose() const;

    // Rig
    Dia::Rig2D::SkeletonDef mSkeletonDef;
    std::unique_ptr<Dia::Rig2D::Skeleton> mSkeleton;
    std::unique_ptr<Dia::Rig2D::Pose> mPose;

    // Clips
    Dia::Animation2D::AnimClipDef mClipDefs[3];
    std::unique_ptr<Dia::Animation2D::AnimClip> mClips[3];
    Dia::Animation2D::AnimClipPlayer mPlayer;

    // State
    int mCurrentClipIndex = 0;
    unsigned int mClipsPlayed = 0;
    unsigned int mTotalPlaybackFrames = 0;
    bool mAllCompleted = false;
    bool mPoseCorrect = false;

    static constexpr float kPoseTolerance = 0.001f;

#ifdef DIA_DEBUG
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::VisualDebuggerModule> mVisualDebuggerRef{this};
    std::unique_ptr<Animation2DTestDrawer> mDrawer;
#endif
};
```

### Sequential playback logic (T-03)

```cpp
void Animation2DStageModule::OnUpdate(float deltaTime)
{
    if (mAllCompleted) return;

    mPlayer.Update(deltaTime);
    mPlayer.Sample(*mSkeleton, *mPose);
    mTotalPlaybackFrames++;

    // Check clip completion via normalized time
    if (!mPlayer.IsPlaying() || mPlayer.GetNormalizedTime() >= 1.0f)
    {
        mClipsPlayed++;
        if (mCurrentClipIndex < 2)
        {
            mCurrentClipIndex++;
            mPlayer.Play(*mClips[mCurrentClipIndex], Dia::Animation2D::PlaybackMode::kOneShot);
        }
        else
        {
            mAllCompleted = true;
            mPoseCorrect = VerifyGoldenPose();
            if (mPoseCorrect) ReportPassed();
            else ReportFailed();
        }
    }
}
```

### Golden pose verification (T-03)

```cpp
bool Animation2DStageModule::VerifyGoldenPose() const
{
    Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones> worldTransforms;
    Dia::Rig2D::BoneTransform rootTransform; // identity
    mPose->ComputeWorldTransforms(*mSkeleton, rootTransform, worldTransforms);

    int spineIdx = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("Spine"));
    int armLIdx  = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("Arm_L"));
    int armRIdx  = mSkeleton->FindBoneIndex(Dia::Core::StringCRC("Arm_R"));

    return Dia::Maths::ApproxEqual(worldTransforms[spineIdx].position.y, 2.0f, kPoseTolerance)
        && Dia::Maths::ApproxEqual(worldTransforms[armLIdx].position.x, -0.7f, kPoseTolerance)
        && Dia::Maths::ApproxEqual(worldTransforms[armRIdx].position.x, 0.7f, kPoseTolerance);
}
```

### Asset file format — `.diarig` (T-01)

DiaReflect JSON for `SkeletonDef`. Example:
```json
{
    "id": "test_rig",
    "bones": [
        { "name": "Root", "parentIndex": -1, "localPosition": [0.0, 0.0], "localRotation": 0.0, "localScale": [1.0, 1.0], "length": 0.0 },
        { "name": "Spine", "parentIndex": 0, "localPosition": [0.0, 1.0], "localRotation": 0.0, "localScale": [1.0, 1.0], "length": 1.0 },
        { "name": "Arm_L", "parentIndex": 1, "localPosition": [-0.5, 0.0], "localRotation": 0.0, "localScale": [1.0, 1.0], "length": 0.5 },
        { "name": "Arm_R", "parentIndex": 1, "localPosition": [0.5, 0.0], "localRotation": 0.0, "localScale": [1.0, 1.0], "length": 0.5 }
    ]
}
```

### Asset file format — `.diaclip` (T-02)

DiaReflect JSON for `AnimClipDef`. Each clip has tracks per bone with keyframes at time 0.0 and 2.0 (60 frames at 30Hz). The final keyframe values become the golden pose targets.

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-01 | Create `.diarig` asset: `Cluiche/Assets/Stages/Animation2DStage/skeleton/test_rig.diarig` (4 bones: Root/Spine/Arm_L/Arm_R) | `SerializedFileLoad::LoadNow<SkeletonDef>` round-trips, `Skeleton` validates | Todo | haiku | DiaReflect JSON; bind pose matches spec hierarchy |
| T-02 | Create 3× `.diaclip` assets under `clips/` (idle 60f, walk 60f, jump 60f) with known golden final poses | `SerializedFileLoad::LoadNow<AnimClipDef>` round-trips; `AnimClip::GetDuration()` == 2.0f each | Todo | sonnet | Keyframes at t=0.0 and t=2.0; final poses: idle=bind, walk=Spine.Y+0.2/ArmL.X-0.2, jump=Spine.Y+1.0/Arms raised |
| T-03 | Run `/new-cluichetest-stage Animation2D` scaffold. Then: (a) replace skeleton with `Animation2DStageModule` inheriting `TestStageModuleBase`; (b) implement full module (LoadAssets, OnStart, OnUpdate, OnStop, VerifyGoldenPose); (c) add VisualDebuggerModule on SimPU | `dia pipeline --target cluichetest` passes; Animation2DStage in Boot menu; stage loads + all checkpoints PASS | Todo | sonnet | Skill handles: diastage, diaapp, vcxproj, cluiche_main, diagame, catalogue, pipeline.toml. Module drives AnimClipPlayer directly — no AnimController. |
| T-04 | Create `Animation2DTestDrawer` (`Drawers/Animation2DTestDrawer.h/.cpp`) — `IVisualDebugger` subclass; `Draw()` renders bone positions as circles + hierarchy lines; `DrawImGui()` shows clip index, frame count, playback state, pose values | Visual: skeleton renders, console shows Anim2D tab with counters | Todo | sonnet | Follows Geometry2DShapesDrawer pattern; stage tag `"Anim2D"` |
| T-05 | Write pytest scenario: `Tools/orchestrator/scenarios/cluichetest/animation2d/test_anim2d_playback.py` | Both checkpoints pass + metric assertions | Todo | sonnet | poll clip_completed (10s), poll pose_correct (2s), assert clips_played==3, total_playback_frames==180 |
| T-06 | Add scenario to `Tools/orchestrator/plans/cluichetest/default.json` | `--list` shows anim2d scenario | Todo | haiku | |
| T-07 | Verify: `dia run cluichetest` — full E2E pass (Boot → Animation2DStage → complete → Boot) | Both checkpoints PASS; drawer renders skeleton; metrics correct | Todo | sonnet | Requires stage scaffold complete |
| T-08 | Update spec status → Done; commit | Spec status = Done | Todo | haiku | |

---

## Dependencies

```
T-01 (diarig asset)  ──┐
T-02 (diaclip assets) ──┼── T-03 (scaffold + module impl) ── T-04 (drawer)
                                                             ├── T-05 (pytest)
                                                             └──┐
T-04 ──────────────────────────────────────────────────────────┬── T-07 (visual verify)
T-05 ──────────────────────────────────────────────────────────┤
T-06 ──────────────────────────────────────────────────────────┤
                                                                └── T-08 (commit)

T-01 and T-02 can run in parallel.
T-03 depends on T-01 and T-02 (needs asset files to load).
T-04, T-05, T-06 can run in parallel after T-03.
T-07 requires T-03, T-04 complete.
T-08 requires T-05, T-06, T-07 complete.
```

---

## Files Touched

| File | Change | Task |
|------|--------|------|
| `Cluiche/Assets/Stages/Animation2DStage/skeleton/test_rig.diarig` | New — DiaReflect JSON SkeletonDef (4 bones) | T-01 |
| `Cluiche/Assets/Stages/Animation2DStage/clips/idle_clip.diaclip` | New — DiaReflect JSON AnimClipDef (60 frames, bind pose) | T-02 |
| `Cluiche/Assets/Stages/Animation2DStage/clips/walk_clip.diaclip` | New — DiaReflect JSON AnimClipDef (60 frames, moderate motion) | T-02 |
| `Cluiche/Assets/Stages/Animation2DStage/clips/jump_clip.diaclip` | New — DiaReflect JSON AnimClipDef (60 frames, large motion) | T-02 |
| `Cluiche/Assets/Stages/Animation2DStage/animation2d_stage.diastage` | New — stage pointer | T-03 |
| `Cluiche/Assets/Stages/Animation2DStage/misc/ApplicationFlow/animation2d_stage.diaapp` | New — module wiring (SimPU: Animation2DStageModule + VisualDebuggerModule) | T-03 |
| `Cluiche/CluicheTest/Modules/TestStages/Animation2DStageModule.h` | New — module header | T-03 |
| `Cluiche/CluicheTest/Modules/TestStages/Animation2DStageModule.cpp` | New — module implementation | T-03 |
| `Cluiche/CluicheTest/CluicheTest.vcxproj` + `.vcxproj.filters` | Add all new .h/.cpp files | T-03 |
| `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Add stage to stages[]; Boot transitions | T-03 |
| `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add stage import | T-03 |
| `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest entries | T-03 |
| `pipeline.toml` | Add asset_stages entry + deploy files | T-03 |
| `Cluiche/bin/CluicheTest/Debug/x64/assets/global/misc/ApplicationFlow/cluiche_main.diaapp` | Force-copy after edit | T-03 |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/Animation2DTestDrawer.h` | New — IVisualDebugger drawer header | T-04 |
| `Cluiche/CluicheTest/Modules/TestStages/Drawers/Animation2DTestDrawer.cpp` | New — drawer implementation | T-04 |
| `Tools/orchestrator/scenarios/cluichetest/animation2d/test_anim2d_playback.py` | New — pytest E2E scenario | T-05 |
| `Tools/orchestrator/plans/cluichetest/default.json` | Add animation2d scenario entry | T-06 |
| `docs/specs/features/cluichetest/teststages/animation2d-stage.md` | Status → Done | T-08 |

---

## Risks

| Risk | Mitigation |
|------|------------|
| `AnimClipPlayer` may not stop cleanly at `GetNormalizedTime() == 1.0` — could overshoot on the frame it finishes | Check `!IsPlaying()` first (OneShot mode auto-stops), OR check `>= 1.0` as fallback |
| Golden pose values derived from clip keyframes + skeleton hierarchy — must compute expected world transforms manually | T-02 must specify keyframe values such that `ComputeWorldTransforms` produces the spec's golden values exactly |
| `SerializedFileLoad` uses `FilePath::ResoledFilePath` — need correct path alias resolution | Stage's `path_aliases.stage_root` + PathStore must resolve `skeleton/test_rig.diarig` relative to stage directory |
| DiaReflect JSON format for `Vector2D` fields — must match archive's array-vs-object convention | Verify with existing test: `TestDiaAnimation2DSerializers.cpp` shows the expected JSON shape |

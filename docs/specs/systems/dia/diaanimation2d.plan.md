# Plan: DiaAnimation2D

**Spec:** @docs/specs/systems/dia/diaanimation2d.md  
**Status:** Done  
**Started:** 2026-05-02  
**Last Updated:** 2026-05-02 (rev 2)

---

## Overview

DiaAnimation2D is a pure C++ static library sitting above DiaRig2D and below game code. It has no rendering or physics dependencies. Seven features are Approved and ready to build; the Procedural Locomotion Oscillator remains Deferred. The implementation order below sequences features from leaf-dependencies inward to the orchestrator, so each phase builds on a working foundation.

**Dependency chain:** `DiaAnimation2D → DiaRig2D → DiaMaths → DiaCore`  
**Also uses:** `DiaLogger` (Animation channel, debug builds only)

---

## Implementation Tasks

| # | Task | Feature | Status | Notes |
|---|------|---------|--------|-------|
| 1 | Create `Dia/DiaAnimation2D/` directory; add `DiaAnimation2D.vcxproj` (static lib, x64 only, depends DiaRig2D) and `DiaAnimation2D.vcxproj.filters` | Scaffold | Done | |
| 2 | Add DiaAnimation2D project to `Cluiche/Cluiche.sln` | Scaffold | Done | |
| 3 | Create `Dia/DiaAnimation2D/Docs/dia.animation2d.architecture.module.md` (YAML module doc) | Scaffold | Done | |
| 4 | Add `Animation2D/` test directory to `Cluiche/Tests/GoogleTests/` vcxproj | Scaffold | Done | |
| 5 | Implement `SpringParamUtils.h` / `SpringParamUtils.cpp` | Spring Param Utils | Done | |
| 6 | Tests: `Animation2D/SpringParamUtilsTests.cpp` — 8 test cases (see test plan) | Spring Param Utils | Done | |
| 7 | Create data structs: `SpringNodeDef.h`, `SpringChainDef.h` | Spring Chain | Done | |
| 8 | Implement `SpringChain.h` / `SpringChain.cpp` | Spring Chain | Done | |
| 9 | Tests: `Animation2D/SpringChainTests.cpp` — 23 test cases | Spring Chain | Done | +5 added: large-dt stability, single-node combo, reset cycle, zero-gravity |
| 10 | Create data structs: `Keyframe.h`, `KeyframeTrack.h`, `AnimClipDef.h` | Keyframe Clip Player | Done | |
| 11 | Implement `AnimClip.h` / `AnimClip.cpp` | Keyframe Clip Player | Done | |
| 12 | Implement `AnimClipPlayer.h` / `AnimClipPlayer.cpp` | Keyframe Clip Player | Done | |
| 13 | Tests: `Animation2D/AnimClipTests.cpp` — 19 test cases | Keyframe Clip Player | Done | +3 added: all-channels, scale lerp, rotation-only isolation |
| 14 | Tests: `Animation2D/AnimClipPlayerTests.cpp` — 17 test cases | Keyframe Clip Player | Done | +5 added: IsLooping, PlayWithMode, null-ptr play, pointer-overload mode |
| 15 | Implement `AnimClipLoader.h` / `AnimClipLoader.cpp` | Clip Loader | Done | `LoadAnimClipDefFromJson` / `LoadAnimClipDefFromSpine` |
| 16 | Tests: `Animation2D/AnimClipLoaderTests.cpp` — 18 test cases | Clip Loader | Done | +4 added: empty anim body, empty bone, multi-bone, scale keyframes |
| 17 | Create data structs: `BoneMask.h`, `PoseLayer.h` | Pose Blend Stack | Done | |
| 18 | Implement `PoseBlendStack.h` / `PoseBlendStack.cpp` | Pose Blend Stack | Done | |
| 19 | Tests: `Animation2D/PoseBlendStackTests.cpp` — 26 test cases | Pose Blend Stack | Done | +5 added: bone-mask copy-at-add, max-layers death test, position blend, scale blend, remove-nonexistent |
| 20 | Implement `AnimationEvaluator.h` / `AnimationEvaluator.cpp` | Animation Evaluator | Done | |
| 21 | Tests: `Animation2D/AnimationEvaluatorTests.cpp` — 17 test cases | Animation Evaluator | Done | +4 added: unregister-nonexistent, re-register, middle unregister, spring auto-mask |
| 22 | Create `Dia/DiaAnimation2D/Testing/` directory with header-only helpers | Test Utilities | Done | |
| 23 | Tests: `Animation2D/TestUtilitiesTests.cpp` — 8 test cases | Test Utilities | Done | |
| 24 | Integration tests: `Animation2D/IntegrationTests.cpp` — 5 full-pipeline tests | Integration | Done | |
| 25 | Build all configurations (Debug + Release x64), run tests, confirm all pass | Verification | Done | 120/120 passing (rev 2) |
| 26 | Update `docs/BACKLOG.md` — move DiaAnimation2D from Ready to Build → Done | Docs | Done | |
| 27 | Update `docs/specs/systems/dia/diaanimation2d.md` status to Done | Docs | Done | |

---

## Test Plan

All tests live in `Cluiche/Tests/GoogleTests/Animation2D/`. Test fixture skeletons are built using DiaRig2D public API directly (no DiaRig2D/Testing dependency).

**Total test cases: ~128** across 9 test files.

---

### SpringParamUtilsTests.cpp (8 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `StiffnessFormula_KnownValues` | Golden | f=1 Hz, mass=1 → k = 4π² ≈ 39.4784 (tolerance 1e-4) |
| 2 | `DampingFormula_KnownValues` | Golden | f=1 Hz, dampingRatio=0.5, mass=1 → d = 2π ≈ 6.2832 |
| 3 | `DefaultMassOne_MatchesExplicitMassOne` | Unit | `SpringParamsFromFrequency(2,0.5)` == `SpringParamsFromFrequency(2,0.5,1)` |
| 4 | `InvalidFrequency_Zero_Asserts` | Boundary | frequency=0 → DIA_ASSERT |
| 5 | `InvalidFrequency_Negative_Asserts` | Boundary | frequency=-1 → DIA_ASSERT |
| 6 | `InvalidDampingRatio_Negative_Asserts` | Boundary | dampingRatio=-0.1 → DIA_ASSERT |
| 7 | `InvalidMass_ZeroOrNegative_Asserts` | Boundary | mass=0 and mass=-1 → DIA_ASSERT |
| 8 | `CriticallyDamped_GoldenValues` | Golden | f=3, dampingRatio=1.0, mass=2 → k≈710.6118, d≈75.3982 |

---

### SpringChainTests.cpp (23 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `Construction_StoresNodeCount` | Unit | Construct chain with N boneIds, verify GetNodeCount()==N |
| 2 | `Construction_StoresId` | Unit | GetId() returns SpringChainDef.id |
| 3 | `Construction_ResolvesBoneNamesToIndices` | Unit | Build chain with named bones, verify construction succeeds |
| 4 | `Construction_NonContiguousBones_Asserts` | Boundary | Pass non-contiguous boneIds → DIA_ASSERT fires |
| 5 | `Construction_NodeOverrideCountMismatch_Asserts` | Boundary | nodeOverrides.Size() > 0 but != boneIds.Size() → DIA_ASSERT |
| 6 | `SemiImplicit_SingleNode_GoldenValue` | Golden | k=50, d=5, displacement 0.3 rad, dt=1/60 → exact expected angle after 1 step |
| 7 | `SubStepping_LargeDt_MatchesManualSteps` | Determinism | Update(12/120) == 12x Update(1/120) — identical output |
| 8 | `AngularVelocityClamp_ExtremeForce` | Invariant | Apply huge external torque, verify velocity never exceeds maxAngularVelocity |
| 9 | `GravityApplied_NodesDriftInGravityDirection` | Unit | gravityStrength > 0, multiple updates, verify rotation in gravity direction |
| 10 | `ExternalTorque_AffectsNextUpdate` | Unit | ApplyExternalTorque, call Update, verify angle changed from baseline |
| 11 | `ExternalTorque_ClearedAfterUpdate` | Unit | Apply torque, Update twice; second Update behaves as if no torque |
| 12 | `Reset_SnapsToCurrentPose_ZeroVelocity` | Unit | Displace chain 10 steps, call Reset, verify next Update produces no motion |
| 13 | `SetNodeStiffness_PreservesVelocity` | Unit | Run N steps, SetNodeStiffness, verify velocity unchanged but force changes on next step |
| 14 | `SetNodeDamping_PreservesVelocity` | Unit | Run N steps, SetNodeDamping, verify velocity unchanged |
| 15 | `SetNodeMaxAngularVelocity_ClampsTakesEffect` | Unit | Reduce maxAngularVelocity mid-simulation to value below current velocity, verify clamping on next Update |
| 16 | `SetGravity_ChangesDriftDirection` | Unit | Set gravity right, verify nodes drift right; flip to left, verify drift reverses |
| 17 | `Update_WritesOnlyLocalRotations` | Invariant | Record pose position/scale before Update, verify identical after (only rotation changed) |
| 18 | `DtZero_IsNoOp` | Boundary | Update(0) produces no change in pose |
| 19 | `SingleNodeChain_Works` | Boundary | 1-node chain (root + 1 bone), verify Update runs without error |
| 20 | `ZeroStiffness_NodeDrifts_NeverRestores` | Unit | stiffness=0, apply displacement, Update many steps — angle does not return to rest |
| 21 | `ZeroDamping_NodeOscillates` | Unit | damping=0, displace, verify node oscillates without energy loss |
| 22 | `GravityDirectionZeroLength_Warning` | Boundary | gravityDirection=(0,0), gravityStrength>0 → DIA_LOG_WARNING, no crash |
| 23 | `Determinism_IdenticalInputs_IdenticalOutput` | Determinism | Two chains with same def + same initial pose → identical outputs after N updates |

---

### AnimClipTests.cpp (16 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `DataStructs_FieldsCorrect` | Unit | Keyframe, KeyframeTrack, AnimClipDef store correct fields and types |
| 2 | `Construction_ZeroDuration_Asserts` | Boundary | AnimClip with duration=0 → DIA_ASSERT |
| 3 | `Construction_UnsortedKeyframes_Asserts` | Boundary | Track with keyframes out of time order → DIA_ASSERT |
| 4 | `Construction_DuplicateBoneTracks_Asserts` | Boundary | Two tracks with same boneId → DIA_ASSERT |
| 5 | `Construction_EmptyTrack_Skipped` | Unit | Track with 0 keyframes is silently skipped; GetTrackCount excludes it |
| 6 | `Construction_MissingBone_Warning` | Unit | Bone name not in skeleton → DIA_LOG_WARNING, no crash, track excluded from sampling |
| 7 | `Sample_AtExactKeyframeTimes_ExactValues` | Unit | Sample at t=0 and t=duration → exact keyframe transforms (no interpolation) |
| 8 | `Sample_BetweenKeyframes_LinearLerp_PositionScale` | Golden | Two keyframes at t=0, t=1; sample at t=0.5 → lerped position and scale |
| 9 | `Sample_BetweenKeyframes_ShortestArcRotation` | Unit | Rotation 170° and -170°; sample at midpoint → crosses 180° not 0° |
| 10 | `Sample_PartialKeyframes_UsesBindPose` | Unit | Rotation-only track; sample → position and scale from skeleton bind pose |
| 11 | `Sample_SingleKeyframeTrack_ConstantValue` | Unit | One keyframe at t=0; sample at t=0.3, t=0.7, t=1.0 → same value throughout |
| 12 | `Sample_TimeClampedToDuration` | Boundary | Sample with time > duration returns same as sample at duration |
| 13 | `Sample_TimeClampedToZero` | Boundary | Sample with time < 0 returns same as sample at 0 |
| 14 | `Sample_NonUniformTrackLengths` | Unit | Track A has 2 keyframes, Track B has 5; sample at various times → per-track interpolation correct |
| 15 | `GetId_GetDuration_GetTrackCount_Correct` | Unit | Verify accessor methods return values matching AnimClipDef |
| 16 | `Sample_BoneNotInSkeleton_Skipped_NoCrash` | Boundary | Clip has bone not in skeleton; Sample runs without crash; other bones still sampled |

---

### AnimClipPlayerTests.cpp (12 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `Play_SetsIsPlaying_True` | Unit | Play → IsPlaying() == true |
| 2 | `Stop_SetsIsPlaying_False` | Unit | Play, Stop → IsPlaying() == false |
| 3 | `Sample_WhenNotPlaying_IsNoOp` | Unit | Call Sample without Play → outPose unchanged |
| 4 | `PlayWhilePlaying_RestartsFromZero` | Unit | Play, Update to midpoint, Play again → GetCurrentTime() == 0 |
| 5 | `OneShot_ClampsAtDuration_StopsPlaying` | Unit | Play one-shot, Update past duration → IsPlaying()==false, GetCurrentTime()==duration |
| 6 | `Looping_WrapsViaFmod` | Unit | Play looping, Update past duration → IsPlaying()==true, time has wrapped |
| 7 | `NegativeSpeed_OneShot_ClampsToZero` | Unit | SetSpeed(-1), Update past start → clamps to 0 |
| 8 | `NegativeSpeed_Looping_WrapsBackward` | Unit | SetSpeed(-1), looping → time wraps backward correctly |
| 9 | `SpeedZero_PausesPlayback` | Boundary | SetSpeed(0), Update(1.0) → time unchanged, still playing |
| 10 | `GetNormalizedTime_RangeZeroToOne` | Invariant | At start: 0.0, at duration: 1.0, at midpoint: 0.5 |
| 11 | `GetCurrentClip_ReturnsActiveClip` | Unit | After Play, GetCurrentClip() == &clip |
| 12 | `NonOwning_PlayerDoesNotDeleteClip` | Unit | Clip outlives player; stop player before clip out of scope (lifetime test) |

---

### AnimClipLoaderTests.cpp (14 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `LoadFromJson_AllFields_Correct` | Unit | Load complete JSON clip; verify id, duration, tracks, keyframe times/values |
| 2 | `LoadFromJson_PartialKeyframes_DefaultValues` | Unit | Rotation-only keyframe → position={0,0}, scale={1,1} |
| 3 | `LoadFromSpine_DegreesToRadians` | Unit | 90-degree Spine rotation → π/2 radians in AnimClipDef |
| 4 | `LoadFromSpine_YUpConversion` | Unit | Known translate Y value → inverted in engine convention |
| 5 | `LoadFromSpine_IgnoresSlots_Events_DrawOrder` | Unit | Spine JSON with slot/event data → no crash, only bone tracks in result |
| 6 | `Duration_ComputedFromMaxKeyframeTime` | Unit | Last keyframe on non-last track → duration equals that max time |
| 7 | `LoadFromJson_InvalidStructure_Asserts` | Boundary | Missing "tracks" field → DIA_ASSERT |
| 8 | `LoadFromJson_BoneNamesAsStringCRC` | Unit | Bone name in JSON → correct StringCRC hash in track.boneId |
| 9 | `LoadFromSpine_MultipleAnimations_SelectableByName` | Unit | Spine JSON with two animations; select each; verify correct data |
| 10 | `LoadFromJson_KeyframeTimesPreservedExactly` | Unit | Load JSON; verify keyframe times match input (no unit conversion) |
| 11 | `LoadFromJson_EmptyTracksArray_ZeroTracks` | Boundary | JSON with empty "tracks" array → AnimClipDef.tracks.Size() == 0 |
| 12 | `LoadFromSpine_NoBoneTimelines_EmptyResult` | Boundary | Spine animation with only slot timelines → zero tracks, zero duration |
| 13 | `LoadFromJson_ExtraUnknownFields_Ignored` | Unit | JSON with extra "author" field → no crash, no assert |
| 14 | `LoadFromSpine_InvalidName_Asserts` | Boundary | animationName not in Spine JSON → DIA_ASSERT |

---

### PoseBlendStackTests.cpp (22 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `SingleLayer_WeightOne_ExactCopy` | Unit | One layer weight 1.0 → output == source pose bone-by-bone |
| 2 | `TwoLayers_CascadingLerp_CorrectOutput` | Golden | Known poses at priorities 0 and 1; verify manual lerp matches Evaluate output |
| 3 | `ZeroWeightLayer_Skipped` | Unit | Evaluate with zero-weight layer present == Evaluate without it |
| 4 | `PriorityOrder_Respected` | Unit | Swap layer priorities → output changes accordingly |
| 5 | `SamePriority_RegistrationOrder` | Unit | Two layers at same priority → first-registered is base |
| 6 | `BoneMask_RestrictsBones` | Unit | Mask layer to bone "A"; verify bone "B" unchanged by that layer |
| 7 | `BoneMask_InvalidBoneId_Warning` | Unit | Non-existent bone ID in mask → DIA_LOG_WARNING, excluded from resolved set |
| 8 | `NullBoneMask_AffectsAllBones` | Unit | nullptr mask → all bones affected |
| 9 | `ZeroLayers_EvaluateIsNoOp` | Boundary | Empty stack → outPose unchanged |
| 10 | `OutputAliasInputPose_Asserts` | Boundary | outPose == a layer's source pose → DIA_ASSERT in debug |
| 11 | `Rotation_ShortestArcAcrossPiBoundary` | Unit | Layer A at -3.0 rad, layer B at +3.0 rad → blend uses short arc |
| 12 | `AddLayer_GetLayerCount` | Unit | Add N layers → GetLayerCount() == N |
| 13 | `RemoveLayer_ByID` | Unit | Add layer, RemoveLayer(id) → HasLayer(id)==false, count decremented |
| 14 | `SetLayerWeight_UpdatesBlend` | Unit | Set weight to 0.5; evaluate → 50% blend (not full override) |
| 15 | `SetLayerPriority_ChangesEvalOrder` | Unit | Change priority of one layer → evaluation order changes, output changes |
| 16 | `HasLayer_TrueForExisting_FalseForMissing` | Unit | HasLayer(known)==true; HasLayer(unknown)==false |
| 17 | `Clear_EmptiesStack` | Unit | Add layers, Clear() → GetLayerCount()==0 |
| 18 | `DuplicateLayerId_Asserts` | Boundary | AddLayer twice with same id → DIA_ASSERT |
| 19 | `SetLayerWeight_OutOfRange_ClampsWithWarning` | Boundary | SetLayerWeight(id, 1.5) → clamped to 1.0, DIA_LOG_WARNING |
| 20 | `ThreeLayers_CascadingLerp_Correct` | Golden | Three layers at priorities 0,1,2; verify manual cascading lerp matches output |
| 21 | `LayerWeight_Transitions_SmoothlyFromZeroToOne` | Unit | Walk weight 0→1 in steps; verify monotonic change in output |
| 22 | `BoneMask_PartialSkeleton_OtherBonesUnaffected` | Integration | Mask covers bones 0-2 on a 5-bone skeleton; bones 3-4 unchanged |

---

### AnimationEvaluatorTests.cpp (18 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `Constructor_StoresNonOwningRefs` | Unit | Skeleton/Pose refs are stored (code review + lifetime test) |
| 2 | `RegisterClipPlayer_ReturnsNonNullPose` | Unit | Returned Pose pointer is non-null |
| 3 | `RegisterSpringChain_ReturnsNonNullPose` | Unit | Returned Pose pointer is non-null |
| 4 | `RegisterDuplicateSourceId_Asserts` | Boundary | Register same sourceId twice → DIA_ASSERT |
| 5 | `UnregisterSource_RemovesSource` | Unit | Register then unregister; source removed from pipeline |
| 6 | `Evaluate_RunsFK_BeforeClipSpring` | Unit | World transforms buffer populated correctly before clip/spring calls |
| 7 | `Evaluate_UpdatesAndSamplesClipPlayer` | Unit | Clip player Update+Sample called; evaluator-owned pose is modified |
| 8 | `Evaluate_UpdatesSpringChain` | Unit | Spring chain Update called with correct dt and world transforms |
| 9 | `Evaluate_BlendStackWritesTargetPose` | Unit | Target pose modified by blend stack after Evaluate |
| 10 | `PipelineOrder_Clips_Before_Springs_Before_Blend` | Unit | Verify order using test sources that record call order |
| 11 | `SetSourceWeight_PropagatesTo_BlendStack` | Unit | SetSourceWeight → GetBlendStack().GetLayer(id).weight matches |
| 12 | `SetSourcePriority_PropagatesTo_BlendStack` | Unit | SetSourcePriority → GetBlendStack().GetLayer(id).priority matches |
| 13 | `ZeroSources_EvaluateIsNoOp` | Boundary | No registered sources → target pose unchanged |
| 14 | `DtZero_Evaluate_IsSafe` | Boundary | Evaluate(0, rootTransform) → no crash, pose unchanged (clips don't advance) |
| 15 | `MaxSources_Exceeded_Asserts` | Boundary | Register kMaxSources+1 sources → DIA_ASSERT |
| 16 | `EvaluatorOwnedPoses_ResetToBindPose_EachFrame` | Unit | Evaluator-owned pose reset before each Evaluate call; stale data not carried forward |
| 17 | `GetBlendStack_ReturnsAccessibleStack` | Unit | GetBlendStack() non-const returns a working PoseBlendStack that can add layers |
| 18 | `FullPipeline_ClipPlusSpringChain_BothContribute` | Integration | Register 1 clip player and 1 spring chain; evaluate 10 frames; verify target pose shows contribution from both |

---

### TestUtilitiesTests.cpp (8 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `BuildTestClip_ValidAnimClipDef` | Unit | Construct AnimClip from BuildTestClip result; GetTrackCount and sampling work |
| 2 | `BuildSingleBoneClip_CorrectKeyframes` | Unit | Sample at t=0 → startRotation; at t=duration → endRotation |
| 3 | `BuildTestSpringChain_CorrectBoneRange` | Unit | Construct SpringChain; GetNodeCount() == (end-start+1) |
| 4 | `AssertPosesEqual_PassesForIdentical` | Unit | Identical poses → assertions pass |
| 5 | `AssertPosesEqual_FailsForDifferent` | Unit | One bone differing by > tolerance → assertion fails with useful output |
| 6 | `AssertBoneRotation_CorrectBoneIndex` | Unit | Set known rotation on bone 2; AssertBoneRotation(pose, 2, expected) passes; bone 3 fails |
| 7 | `TestEvaluatorFixture_Functional` | Integration | Register clip player; Evaluate 1 frame; target pose changed from bind |
| 8 | `AllHelpers_CompileAndLink` | Build | Include all four Testing headers in same TU; compile succeeds |

---

### IntegrationTests.cpp (5 tests)

| # | Test Name | Type | Description |
|---|-----------|------|-------------|
| 1 | `FullPipeline_ClipPlayer_DrivesAllBones` | Integration | Single AnimClipPlayer covers all bones; Evaluate; verify target pose matches expected interpolated values |
| 2 | `FullPipeline_SpringChain_SecondaryMotion` | Integration | No clip; spring chain on tail bones; Evaluate N frames from displaced pose; verify tail converges toward rest |
| 3 | `FullPipeline_ClipAndSpring_BoneMaskSeparation` | Integration | Clip on upper body (BoneMask), spring chain on tail (separate BoneMask); Evaluate; verify each affects only its bone group |
| 4 | `FullPipeline_LoadClipFromJson_PlayAndEvaluate` | Integration | Load clip via AnimClipLoader; create AnimClipPlayer; Evaluate 5 frames; verify output is non-trivial |
| 5 | `FullPipeline_MultipleClipPlayers_BlendStack` | Integration | Two AnimClipPlayers at different blend priorities; Evaluate; verify blend stack weight semantics hold end-to-end |

---

## Session Notes

### 2026-05-02 (rev 2)
- Architectural review identified: pointer stability bug (BoneMask pointer inside PoseBlendStack dangled after UnregisterSource shifted SourceEntry slots), missing IsLooping() accessor, magic number constants.
- Fixed: PoseBlendStack now owns a compact LayerBoneMask (32-bone cap) by value — no pointer lifetime dependency. autoMask removed from SourceEntry. AnimationEvaluator is non-copyable. Named constants replace magic numbers (kMaxSubstepDt, kGravityEpsilon, kTimeEpsilon, kPi, kDegToRad). IsLooping() added to AnimClipPlayer.
- Added 13 tests (107 → 120): position/scale channel blending, rotation-only isolation, large-dt spring stability, gravity+torque combo, reset cycle, re-register after unregister, middle-source unregister, spring auto-mask bone isolation, bone mask copy-at-add, max-layers death test, Spine loader edge cases.

### 2026-05-02
- Plan created from 7 approved feature specs + system spec. All specs Approved, no spec work needed before implementation starts.
- Implementation order: Scaffold → SpringParamUtils → Spring data types → Clip data types → AnimClip → AnimClipPlayer → AnimClipLoader → SpringChain → PoseBlendStack → AnimationEvaluator → TestUtilities → Integration tests.
- SpringChain depends on SpringNodeDef/SpringChainDef (task 7); AnimClip depends on Keyframe/KeyframeTrack/AnimClipDef (task 10); PoseBlendStack depends on BoneMask/PoseLayer (task 17); AnimationEvaluator depends on all prior features.
- Test fixture pattern: build skeleton from SkeletonDef directly (DiaRig2D public API), not DiaRig2D/Testing/.
- DIA_ASSERT tests use GoogleTest death tests (`EXPECT_DEATH`) consistent with DiaRig2D and DiaIK2D test patterns.
- JSON test fixtures for AnimClipLoader tests: create minimal inline JSON strings in test file (no external fixture files needed unless Spine JSON format requires it).

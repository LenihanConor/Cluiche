# Plan: DiaIK2D

**Spec:** @docs/specs/systems/dia/diaik2d.md  
**Status:** Done  
**Started:** 2026-05-02  
**Last Updated:** 2026-05-02

---

## Prerequisites

**DiaRig2D must be implemented before DiaIK2D can start.** DiaIK2D depends on `Dia::Rig2D::Skeleton`, `Dia::Rig2D::Pose`, `Dia::Rig2D::BoneTransform`, and `Pose::ComputeWorldTransforms()`. Check DiaRig2D plan status before beginning Task 1.

---

## Implementation Order Rationale

Data types first (IKChainDef.h is header-only, no dependencies), then the IKSolver scaffold (registration + SetRootTransform + FK cache), then the three solvers in order of increasing complexity (two-bone is self-contained analytic geometry; FABRIK adds an iterative loop; look-at is the simplest). Logging is woven throughout the solver implementations rather than a separate task. Test utilities come after all solvers so the helpers can exercise the full API. Unit tests and build verification are last.

---

## Implementation Patterns

### Project Scaffolding (Task 1)
- **vcxproj**: `StaticLibrary`, x64 only. Include dirs: `./;./../;`. `ProjectReference` entries for DiaCore, DiaMaths, DiaLogger, **DiaRig2D** — all with `ReferenceOutputAssembly=false`. NO OutDir/IntDir/PlatformToolset/LanguageStandard overrides (PD-008).
- **sln**: Register under `Library` solution folder. Follow DiaSoftBody2D GUID pattern. Generate new GUID (e.g. `{F4A5B6C7-D8E9-0123-FABC-445566778899}`).
- **Filters**: Top-level filters: `Docs`, `Testing`, source root.
- **Architecture doc**: `Docs/dia.ik2d.architecture.module.md`, `schema: dia.module.v1`, `module_id: dia.ik2d`, `parent_module_id: dia.root`, dependencies `dia.rig2d`, `dia.maths`, `dia.core`, `dia.logger`.

### IKChainDef.h (Task 2)
- Pure header, no `.cpp`. Three structs in `namespace Dia::IK2D`.
- `JointLimitDef`: `float minAngle`, `float maxAngle`, `bool enabled`. Default-init: `minAngle{-Dia::Maths::kPi}`, `maxAngle{Dia::Maths::kPi}`, `enabled{false}`.
- `IKChainDef`: `StringCRC id{}`, `StringCRC startBoneId{}`, `StringCRC endBoneId{}`, `float reachWeight{1.0f}`, `int maxIterations{20}`, `float tolerance{0.001f}`, `DynamicArrayC<JointLimitDef> jointLimits{}`.
- `PoleVector`: `Vector2D direction{1.0f, 0.0f}`, `float weight{1.0f}`.
- **StringCRC note**: Use `static const` (not `constexpr`) for any `kUniqueId` if StringCRC ctor is not constexpr — same pattern as DiaSoftBody2D.

### IKSolver Scaffold (Task 3)
Internal `ResolvedChain` struct (private, lives in `IKSolver.cpp`):
```cpp
struct ResolvedChain {
    IKChainDef def;           // Owned copy
    int        startIndex;    // Resolved from startBoneId
    int        endIndex;      // Resolved from endBoneId
    int        jointCount;    // endIndex - startIndex
};
```
`IKSolver` private members:
```cpp
Dia::Rig2D::Skeleton&                  mSkeleton;
Dia::Rig2D::Pose&                      mPose;
Dia::Rig2D::BoneTransform              mRootTransform;
bool                                 mRootTransformSet{false};    // debug guard
DynamicArrayC<Rig2D::BoneTransform>    mWorldTransforms;            // sized to boneCount
DynamicArrayC<Maths::Vector2D>       mWorkingPositions;           // FABRIK working array
DynamicArrayC<float>                 mSnapshotRotations;          // reach weight pre-solve
DynamicArrayC<ResolvedChain>         mChains;                     // registered chains
```
Pre-allocate all arrays in constructor to `skeleton.GetBoneCount()`. No allocation during solve.

`SetRootTransform`: store root, call `mPose.ComputeWorldTransforms(mSkeleton, mRootTransform, mWorldTransforms)`, set `mRootTransformSet = true`.

`RegisterChain`: call `mSkeleton.FindBoneIndex(startBoneId)` / `FindBoneIndex(endBoneId)`. `DIA_ASSERT` on -1 result or `startIndex >= endIndex`. Build `ResolvedChain`, `mChains.Add()`.

### Two-Bone Solver (Task 4)
Algorithm (all in `IKSolver.cpp`):
1. Snapshot `mPose.GetLocalTransform(i).rotation` for start, mid, end bones into `mSnapshotRotations`.
2. Read world positions of start, mid, end from `mWorldTransforms`.
3. Read `l1 = mSkeleton.GetBone(midIndex).length`, `l2 = mSkeleton.GetBone(endIndex).length`.
4. Compute `d = (target - startWorldPos).Length()`.
5. Unreachable: `d >= l1 + l2` — align all bones toward target, skip to step 8.
6. Law of cosines for angle at mid: `cosAngle = (l1*l1 + l2*l2 - d*d) / (2*l1*l2)`. Clamp to `[-1, 1]` before `acosf`. `midAngle = acosf(cosAngle)`.
7. Angle at start: `atan2f(target.y - startPos.y, target.x - startPos.x)` adjusted by triangle geometry + pole vector bias.
8. Clamp angles to joint limits from `chain.def.jointLimits`.
9. Apply reach weight: `float t = clamp(chain.def.reachWeight, 0, 1)`. Per bone: `newRot = ShortestArcLerp(snapshot, iKRot, t)`. Write to `mPose.GetLocalTransform(i).rotation`.
10. Re-run `mPose.ComputeWorldTransforms(mSkeleton, mRootTransform, mWorldTransforms)` to refresh cache.

`ShortestArcLerp` inline helper:
```cpp
inline float ShortestArcLerp(float a, float b, float t) {
    float diff = b - a;
    diff = atan2f(sinf(diff), cosf(diff));  // normalise to [-pi, pi]
    return a + t * diff;
}
```

### FABRIK Solver (Task 5)
FABRIK works on world-space positions, so uses `mWorkingPositions`:
1. Copy chain bone world positions from `mWorldTransforms` into `mWorkingPositions`.
2. Snapshot pose rotations for reach weight.
3. Reachability: `totalLength = sum of bone lengths in chain`. If `(target - startPos).Length() >= totalLength` — extend and return.
4. Iterate up to `chain.def.maxIterations`:
   - **Backward pass**: `mWorkingPositions[end] = target`. For i = end-1 → start: direction = normalize(`pos[i] - pos[i+1]`), `pos[i] = pos[i+1] + direction * boneLength[i+1]`.
   - **Forward pass**: `mWorkingPositions[0] = startWorldPos` (pin root). For i = 1 → end: direction = normalize(`pos[i] - pos[i-1]`), `pos[i] = pos[i-1] + direction * boneLength[i]`.
   - Apply joint limits per joint (position→local angle→clamp→recompute position).
   - Convergence: `(mWorkingPositions[end] - target).Length() < chain.def.tolerance` → break.
5. Non-convergence: `DIA_LOG_WARNING("Rig2D", ...)` in `#ifndef NDEBUG`.
6. Convert world positions back to local rotations: `localRot[i] = worldAngle[i] - parentWorldAngle[i]`.
7. Apply reach weight via `ShortestArcLerp`. Write to Pose.
8. Re-run full FK to refresh `mWorldTransforms`.

### Look-At Constraint (Task 6)
`SolveLookAt(int boneIndex, ...)` is the canonical implementation; `SolveLookAt(StringCRC, ...)` resolves and calls through.

1. `DIA_ASSERT(boneIndex >= 0 && boneIndex < mSkeleton.GetBoneCount())`.
2. Snapshot `mPose.GetLocalTransform(boneIndex).rotation`.
3. Read bone world pos from `mWorldTransforms[boneIndex].position`.
4. Coincident guard: `if ((target - boneWorldPos).Length() < 1e-5f)` → `DIA_LOG_WARNING` + return `true`.
5. `worldAngle = atan2f(target.y - bonePos.y, target.x - bonePos.x) - axisAngleOffset`.
6. `parentWorldAngle = (boneIndex > 0) ? mWorldTransforms[parentIndex].rotation : mRootTransform.rotation`.
7. `localAngle = worldAngle - parentWorldAngle`.
8. Apply reach weight. Write to Pose.
9. Re-run full FK.

### Test Utilities (Task 7)
- `Testing/IKTestHelpers.h` — header-only.
  - `BuildLimbSkeleton(int boneCount, float boneLength)`: uses `Dia::Rig2D::SkeletonDef`, adds root + boneCount-1 children along +Y axis. Returns `{Skeleton, Pose}` pair via a small `TestRig` struct.
  - `BuildTestIKSolver(Skeleton& sk, Pose& pose)`: constructs `IKSolver(sk, pose)`, calls `SetRootTransform({identity})`, returns it.
- `Testing/IKAssertions.h` — header-only. Uses GoogleTest `EXPECT_NEAR`.
  - `AssertBoneRotation(const Pose&, int boneIndex, float expectedAngle, float toleranceRad)`.
  - `AssertEndEffectorPosition(IKSolver&, const BoneTransform& root, int boneIndex, Vector2D expected, float toleranceUnits)` — calls `SetRootTransform`, reads world pos.
  - `AssertBoneUnchanged(const Pose&, int boneIndex, float snapshotRotation)`.
- Not compiled into `DiaIK2D.vcxproj`. GoogleTests.vcxproj adds `DiaIK2D/` to include dirs; test files `#include <DiaIK2D/Testing/IKTestHelpers.h>`.

### Unit Tests (Task 8)
Test files in `Cluiche/Tests/GoogleTests/IK/`:
- `TestIKChainDef.cpp` — struct defaults, DynamicArrayC jointLimits partial array.
- `TestIKSolverRegistration.cpp` — RegisterChain, UnregisterChain, HasChain, duplicate chain, bone not found assert.
- `TestTwoBoneSolver.cpp` — reachable target (end effector within 1e-4), unreachable (full extension), pole vector bend direction, joint limits clamp, reach weight 0/0.5/1.0, degenerate (d ≈ 0).
- `TestFABRIKSolver.cpp` — 3-joint chain reaches target, unreachable full extension, root pinned, non-convergence warning, reach weight, single-joint degenerate case.
- `TestLookAtConstraint.cpp` — target right/up/left/down, axisAngleOffset correction, reach weight, coincident target no-op, boneIndex overload.
- `TestIKChainedSolves.cpp` — two solve calls in one frame (spine then arm), verify second solve uses updated world transforms.
- `TestIKLogging.cpp` — unreachable target emits warning, FABRIK non-convergence emits warning, SetRootTransform not called emits warning. Debug-build only tests.

---

## Tasks

| # | Task | Spec | Status | Notes |
|---|------|------|--------|-------|
| 0 | **Pre-check** — Verify DiaRig2D is implemented: `Dia::Rig2D::Skeleton`, `Pose`, `BoneTransform`, `Pose::ComputeWorldTransforms()` all exist and build. | — | Done | DiaRig2D confirmed Done (2026-05-01). |
| 1 | **Project scaffolding** — Create `Dia/DiaIK2D/` directory, `DiaIK2D.vcxproj` (static lib, x64, refs DiaCore/DiaMaths/DiaLogger/DiaRig2D), `.vcxproj.filters`, register in `Cluiche.sln` under Library folder, create `Docs/dia.ik2d.architecture.module.md` | System | Done | GUID: {F4A5B6C7-D8E9-0123-FABC-445566778899}. |
| 2 | **IK Chain Definition** — `IKChainDef.h`: `JointLimitDef`, `IKChainDef`, `PoleVector` structs. Header-only. | [ik-chain-definition.md](../../features/dia/diaik2d/ik-chain-definition.md) | Done | |
| 3 | **IKSolver scaffold** — `IKSolver.h` / `IKSolver.cpp`: constructor (pre-alloc all arrays), `SetRootTransform` (run full FK into `mWorldTransforms`), `RegisterChain` / `UnregisterChain` / `HasChain`, `GetSkeleton` / `GetPose`. No solver logic yet. | [ik-solver.md](../../features/dia/diaik2d/ik-solver.md) | Done | Added `GetParentWorldRot()` private helper for bone rest angle math. |
| 4 | **Two-bone solver** — `SolveTwoBone()`: law of cosines, pole vector, joint limits clamp, reach weight (ShortestArcLerp), FK cache refresh. | [two-bone-solver.md](../../features/dia/diaik2d/two-bone-solver.md) | Done | `BoneWorldRotFromDir(dirAngle, boneRestAngle)` helper used throughout. |
| 5 | **FABRIK solver** — `SolveFABRIK()`: iterative backward/forward passes on `mWorkingPositions`, joint limit projection per iteration, convergence check, non-convergence warning, reach weight, FK cache refresh. | [fabrik-solver.md](../../features/dia/diaik2d/fabrik-solver.md) | Done | |
| 6 | **Look-at constraint** — `SolveLookAt(int)` + `SolveLookAt(StringCRC)`: atan2 world angle, axis offset, parent world rotation, reach weight, coincident guard, FK cache refresh. | [look-at-constraint.md](../../features/dia/diaik2d/look-at-constraint.md) | Done | |
| 7 | **Test utilities** — `Testing/IKTestHelpers.h`, `Testing/IKAssertions.h`: `BuildLimbSkeleton`, `BuildTestIKSolver`, `AssertBoneRotation`, `AssertEndEffectorPosition`, `AssertBoneUnchanged`. Header-only, not compiled into `DiaIK2D.vcxproj`. | [test-utilities.md](../../features/dia/diaik2d/test-utilities.md) | Done | |
| 8 | **Unit tests** — Create `Cluiche/Tests/GoogleTests/IK/` with 7 test files. Register in GoogleTests.vcxproj + filters. Add `DiaIK2D.lib` to linker deps. | All features | Done | 33/33 tests pass. |
| 9 | **Build verification** — Build Debug + Release x64. Run all IK tests. Fix compile/link errors. Verify all source files listed in vcxproj/filters. | All features | Done | Debug + Release build clean. All 33 tests green. |

---

## File Layout (Target)

```
Dia/DiaIK2D/
  IKChainDef.h
  IKSolver.h
  IKSolver.cpp
  Testing/
    IKTestHelpers.h
    IKAssertions.h
  Docs/
    dia.ik2d.architecture.module.md
  DiaIK2D.vcxproj
  DiaIK2D.vcxproj.filters

Cluiche/Tests/GoogleTests/IK/
  TestIKChainDef.cpp
  TestIKSolverRegistration.cpp
  TestTwoBoneSolver.cpp
  TestFABRIKSolver.cpp
  TestLookAtConstraint.cpp
  TestIKChainedSolves.cpp
  TestIKLogging.cpp
```

---

## Session Notes

### 2026-05-01
- Plan created. All 6 feature specs Approved.
- Key design resolutions from spec review: `IKSolver` takes `Skeleton&` + `Pose&`; `SetRootTransform()` must be called once per frame before any solve; IK writes `Pose` local rotations, never `Skeleton` data; full FK re-run after each solve refreshes `mWorldTransforms` for chained solve calls.
- `ShortestArcLerp` inline helper needed: `a + t * atan2f(sinf(b-a), cosf(b-a))`. Check DiaMaths for existing utility before implementing inline.
- All three solvers live in a single `IKSolver.h` / `IKSolver.cpp` — no separate translation units per solver.
- Note: DiaRig2D (dependency) is also Not Started. Implement DiaRig2D first.

### 2026-05-02
- All tasks 0–9 complete. 33/33 tests green. Debug + Release build clean.
- Key implementation note: bone rotation in `ComputeWorldTransforms` uses full affine matrix decomposition (not just angle add). `worldRot = BoneWorldRotFromDir(dirAngle, boneRestAngle)` where `boneRestAngle = atan2(localPos.y, localPos.x)` is the critical helper for back-converting FABRIK world positions to local rotations.
- `Skeleton::ComputeBoneLengths()` sets `length = localPosition.Magnitude()` if `length == 0.0f`; bone[0] (root) always gets `length = 0.0f`. The FABRIK solver uses `GetBone(i).length` for segments starting at i.
- Two-bone bend direction: `elbowDirAngle = alpha - startOffset` for CCW, `alpha + startOffset` for CW. Pole vector picks whichever option has higher dot product with pole direction.

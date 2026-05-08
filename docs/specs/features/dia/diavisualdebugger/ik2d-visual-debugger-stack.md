# Feature Spec: ik2d-visual-debugger-stack

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Creates a new `DiaIK2DVisualDebugger` module as a stack of four focused `IVisualDebugger` draw classes, and adds the minimum public accessor surface to `IKSolver` that the draw classes need. There is no existing IK debugger. The four classes cover: chain bone lines, chain joint circles, bone direction arrows, and chain reach circles (the radius within which the end-effector can be placed). All data is read from an `IKSolver` held by reference.

**Problem solved:** `DiaIK2D` has no visual debugging. Developers cannot distinguish which bones belong to IK chains, see the direction each bone is pointing, or understand the maximum reach of a chain. The four-class stack provides this visibility on independent, toggleable layers.

---

## Acceptance Criteria

1. Four focused draw classes: `IKChainBonesDrawer`, `IKChainJointsDrawer`, `IKChainArrowsDrawer`, `IKReachCirclesDrawer`
2. Each implements `IVisualDebugger` and lives in a new `DiaIK2DVisualDebugger.vcxproj` static library
3. Each stores a `const IKSolver&` and `const Dia::Debug::DebugLayerManager&` reference (set at construction)
4. `IKSolver` gains six new public read-only accessors (see below) — added to `DiaIK2D.vcxproj`, not to the debugger project
5. `Draw(FrameData&)` uses `DebugColourPalette` constants — no ad-hoc colour literals
6. All sizes multiplied by `DebugLayerManager::GetDebugScale()`
7. Canonical layer names from `DebugLayerNames.h` used at `GetLayerName()` return
8. New `DiaIK2DVisualDebugger.vcxproj` added to `Cluiche.sln`
9. Build with no warnings; all tests pass

---

## IKSolver Accessor Additions (DiaIK2D module)

The following public methods are added to `IKSolver`. They expose the minimal read surface the debugger needs without exposing private implementation details beyond what is necessary.

```cpp
// In IKSolver.h — public section

// Number of registered IK chains.
int GetChainCount() const;

// Chain identity and span — index is 0-based up to GetChainCount()-1.
Dia::Core::StringCRC GetChainId(int chainIndex) const;
int GetChainStartBoneIndex(int chainIndex) const;
int GetChainEndBoneIndex(int chainIndex) const;
int GetChainJointCount(int chainIndex) const;

// Cached world-space transforms — refreshed by SetRootTransform() each frame.
// Size matches skeleton.GetBoneCount().
const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform, Dia::Rig2D::kMaxBones>&
    GetWorldTransforms() const;
```

All six accessors are `const` and forward into the existing private members (`mChains`, `mWorldTransforms`) with bounds-checked index reads.

---

## Draw Classes

### 1. IKChainBonesDrawer (`LayerNames::kIKBones`, priority 10)

Draws a line for each bone in each IK chain, using chain-specific colouring to distinguish IK-driven bones from FK bones drawn by `BoneLinesDrawer`.

Draw logic:
- For each chain `c` in `solver.GetChainCount()`:
  - For each bone index `i` from `GetChainStartBoneIndex(c)` to `GetChainEndBoneIndex(c)`:
    - `parentIdx = skeleton.GetBone(i).parentIndex`
    - If `parentIdx < 0`: skip (root)
    - `RequestDraw(worldTransforms[parentIdx].position, worldTransforms[i].position, DebugColourPalette::kGoal)` — cyan
- Only draws bones inside registered chains; unregistered bones not drawn

### 2. IKChainJointsDrawer (`LayerNames::kIKJoints`, priority 10)

Draws a circle at each joint position inside IK chains, sized by a fixed visual radius.

Draw logic:
- For each chain `c`:
  - For each bone index `i` from `GetChainStartBoneIndex(c)` to `GetChainEndBoneIndex(c)`:
    - End bone (i == `GetChainEndBoneIndex(c)`): `RequestDraw(pos, 3.5f * scale, DebugColourPalette::kHealthy)` — green end-effector
    - Start bone (i == `GetChainStartBoneIndex(c)`): `RequestDraw(pos, 3.0f * scale, DebugColourPalette::kGoal)` — cyan chain root
    - Mid-chain bone: `RequestDraw(pos, 2.5f * scale, DebugColourPalette::kGoal)` — cyan mid

### 3. IKChainArrowsDrawer (`LayerNames::kIKArrows`, priority 20)

Draws a `Ray2D` on each bone in each IK chain showing local +X direction in world space.

Draw logic (identical pattern to `DirectionArrowsDrawer` but filtered to chain bones):
- For each chain `c`:
  - For each bone index `i` from start to end:
    - `direction = Vector2D(cos(wt.rotation), sin(wt.rotation))`
    - Arrow length = `bone.length * scale` (if `bone.length > 0`), else `4.0f * scale`
    - `RequestDrawRay(wt.position, direction, length, DebugColourPalette::kGoal)` — cyan

### 4. IKReachCirclesDrawer (`LayerNames::kIKReach`, priority 30)

Draws a circle at each chain's start bone position whose radius equals the total reach of the chain (sum of bone lengths from start to end).

Draw logic:
- For each chain `c`:
  - `reachRadius = 0.0f`
  - For each bone index `i` from `GetChainStartBoneIndex(c)` to `GetChainEndBoneIndex(c) - 1`:
    - `reachRadius += skeleton.GetBone(i).length`
  - If `reachRadius > 0.0f`:
    - `pos = worldTransforms[GetChainStartBoneIndex(c)].position`
    - `RequestDraw(pos, reachRadius * scale, DebugColourPalette::kInactive)` — grey outline circle
  - No fill — outline only shows the reachable sphere

---

## New Module: DiaIK2DVisualDebugger

**Project references required:**
- `DiaCore.vcxproj`
- `DiaMaths.vcxproj`
- `DiaRig2D.vcxproj`
- `DiaIK2D.vcxproj`
- `DiaGraphics.vcxproj`
- `DiaVisualDebugger.vcxproj`

---

## Registration Example (game code)

```cpp
static IKChainBonesDrawer   bonesDrawer  (solver, manager);
static IKChainJointsDrawer  jointsDrawer (solver, manager);
static IKChainArrowsDrawer  arrowsDrawer (solver, manager);
static IKReachCirclesDrawer reachDrawer  (solver, manager);

manager.Register(&bonesDrawer,  10);
manager.Register(&jointsDrawer, 10);
manager.Register(&arrowsDrawer, 20);
manager.Register(&reachDrawer,  30);

// Each frame, after IKSolver::SetRootTransform() and all Solve* calls:
manager.Draw(frameData);
```

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaIK2D/IKSolver.h` | Add 6 public accessor declarations |
| `Dia/DiaIK2D/IKSolver.cpp` | Implement 6 accessors |
| `Dia/DiaIK2DVisualDebugger/IKChainBonesDrawer.h` | New |
| `Dia/DiaIK2DVisualDebugger/IKChainBonesDrawer.cpp` | New |
| `Dia/DiaIK2DVisualDebugger/IKChainJointsDrawer.h` | New |
| `Dia/DiaIK2DVisualDebugger/IKChainJointsDrawer.cpp` | New |
| `Dia/DiaIK2DVisualDebugger/IKChainArrowsDrawer.h` | New |
| `Dia/DiaIK2DVisualDebugger/IKChainArrowsDrawer.cpp` | New |
| `Dia/DiaIK2DVisualDebugger/IKReachCirclesDrawer.h` | New |
| `Dia/DiaIK2DVisualDebugger/IKReachCirclesDrawer.cpp` | New |
| `Dia/DiaIK2DVisualDebugger/DiaIK2DVisualDebugger.vcxproj` | New project |
| `Dia/DiaIK2DVisualDebugger/DiaIK2DVisualDebugger.vcxproj.filters` | New filters |
| `Cluiche/Cluiche.sln` | Add new project |

**Prerequisites:** `debug-budget`, `debug-text-primitive`, `debug-layer-manager` all implemented.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add 6 accessor declarations to `IKSolver.h` and implement in `IKSolver.cpp` | `GetChainCount`, `GetChainId`, `GetChainStartBoneIndex`, `GetChainEndBoneIndex`, `GetChainJointCount`, `GetWorldTransforms` |
| 2 | Create `DiaIK2DVisualDebugger/` directory and new `DiaIK2DVisualDebugger.vcxproj` | Modelled on `DiaRigidBody2DVisualDebugger.vcxproj` |
| 3 | Write `IKChainBonesDrawer.h/.cpp` | Cyan lines; filtered to chain bones |
| 4 | Write `IKChainJointsDrawer.h/.cpp` | Green end-effector, cyan mid/start |
| 5 | Write `IKChainArrowsDrawer.h/.cpp` | Cyan direction rays; same pattern as Rig2D `DirectionArrowsDrawer` |
| 6 | Write `IKReachCirclesDrawer.h/.cpp` | Grey outline circle; sums bone lengths |
| 7 | Add project to `Cluiche.sln` | |
| 8 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 9 | Write tests | `TestIK2DVisualDebuggerStack.cpp` |
| 10 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/IK2D/TestIK2DVisualDebuggerStack.cpp`

Test setup: a `Skeleton` + `Pose` with a 3-bone chain, one IK chain registered (`startBone=0`, `endBone=2`). World transforms computed via `SetRootTransform` + FK.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| IKSolver accessors | `GetChainCount_AfterRegister_IsOne` | Register one chain → `GetChainCount() == 1` |
| IKSolver accessors | `GetChainId_ReturnsRegisteredId` | Matches `IKChainDef::id` |
| IKSolver accessors | `GetChainStartEndBoneIndex_MatchDef` | Start/end indices resolve correctly |
| IKSolver accessors | `GetWorldTransforms_SizeMatchesBoneCount` | `GetWorldTransforms().Size() == skeleton.GetBoneCount()` |
| IKChainBonesDrawer | `Draw_ThreeBoneChain_TwoLines` | 3 bones in chain → 2 lines (root has no parent) |
| IKChainBonesDrawer | `Draw_Colour_IsGoal` | Lines are `kGoal` |
| IKChainBonesDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| IKChainBonesDrawer | `LayerName_IsIKBones` | `GetLayerName() == LayerNames::kIKBones` |
| IKChainJointsDrawer | `Draw_ThreeBones_ThreeCircles` | 3 bones → 3 circles |
| IKChainJointsDrawer | `Draw_EndBone_IsHealthy` | End bone colour == `kHealthy` |
| IKChainJointsDrawer | `Draw_StartBone_IsGoal` | Start bone colour == `kGoal` |
| IKChainJointsDrawer | `LayerName_IsIKJoints` | `GetLayerName() == LayerNames::kIKJoints` |
| IKChainArrowsDrawer | `Draw_ThreeBones_ThreeRays` | 3 bones in chain → 3 rays |
| IKChainArrowsDrawer | `Draw_Colour_IsGoal` | Rays are `kGoal` |
| IKChainArrowsDrawer | `LayerName_IsIKArrows` | `GetLayerName() == LayerNames::kIKArrows` |
| IKReachCirclesDrawer | `Draw_Chain_DrawsOneCircle` | 1 chain → 1 circle |
| IKReachCirclesDrawer | `Draw_Radius_SumOfBoneLengths` | Circle radius == sum of bone lengths in chain |
| IKReachCirclesDrawer | `Draw_Colour_IsInactive` | Reach circle is `kInactive` |
| IKReachCirclesDrawer | `LayerName_IsIKReach` | `GetLayerName() == LayerNames::kIKReach` |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — layer names use `LayerNames::kIK*` constants (StringCRC) |
| PD-002 | ProcessingUnit/Phase/Module | Compliant — draw classes are plain C++ objects |
| PD-003 | Component-based entities | Compliant — no entity ID concerns |
| PD-004 | No STL in public APIs | Compliant — all public methods use `DynamicArrayC`, `StringCRC`, and primitives only |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — new `DiaIK2DVisualDebugger.vcxproj` created and added to solution |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant |
| PD-009 | Generated output under `Cluiche/out/` | Compliant |
| AD-001 | Module YAML frontmatter | Compliant — new `dia.ik2dvisualdebugger.architecture.module.md` created |
| AD-002 | No STL in public APIs | Compliant |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all classes in `Dia::IK2D::` namespace |
| SD-DBG-001 | Stack of focused draw classes | Compliant — four classes registered independently |
| SD-DBG-002 | `#ifdef DIA_DEBUG` | Compliant — `DiaIK2DVisualDebugger.vcxproj` not linked in Release |
| SD-DBG-003 | Priority-ordered draw | Compliant — geometry at 10, overlays at 20, reach at 30 |
| SD-DBG-005 | Global `debugScale` | Compliant — all sizes multiplied by `mManager.GetDebugScale()` |
| SD-DBG-006 | Assert on name collision | Compliant — each drawer returns a distinct `LayerNames::kIK*` constant |
| SD-DBG-010 | `DebugColourPalette` colours binding | Compliant — all colours from palette |
| SD-DBG-014 | Same-family classes share vcxproj | Compliant — all 4 IK draw classes in `DiaIK2DVisualDebugger.vcxproj` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | IK targets not stored | The draw classes read from `IKSolver` but solver targets (the `Vector2D` passed to `SolveTwoBone/FABRIK/LookAt`) are not stored — they're parameters. The debugger cannot show where the end-effector is trying to reach. Is this acceptable? | Yes for this spec. If target visualization is needed, it belongs in game code (the caller knows the target, draws it with a `RequestDrawPoint()` directly). This spec covers only solver-intrinsic data. A future spec could add a `SetLastTarget()` cache to `IKSolver` if needed. |
| 2 | IKSolver private ResolvedChain | The accessors expose `startIndex`, `endIndex`, `jointCount`, and `id` without making `ResolvedChain` public. Is this the right granularity? | Yes — exposing `ResolvedChain` directly would expose `IKChainDef` internals including joint limits which aren't needed by the debugger. Six flat accessors are sufficient and keep the public surface minimal. |
| 3 | Bone length zero in `IKReachCirclesDrawer` | If all bones in a chain have `length == 0.0f`, `reachRadius` remains 0 and no circle is drawn. Is this correct? | Yes — drawing a zero-radius circle provides no information. The guard `if (reachRadius > 0.0f)` is intentional. Bone length 0 typically means the bone is a point-bone (no physical extent). |
| 4 | Draw when no chains registered | All four draw classes iterate `GetChainCount()`. If 0 chains are registered (drawer constructed before any `RegisterChain()` call), `Draw()` emits 0 primitives safely. | Correct — no special empty-check needed. The loop body simply never executes. |
| 5 | `GetWorldTransforms()` freshness | `IKSolver::mWorldTransforms` is refreshed by `SetRootTransform()` (calls `RefreshWorldTransforms()`). If the draw classes are called before `SetRootTransform()` on the first frame, `mRootTransformSet == false`. What does `GetWorldTransforms()` return in that case? | All-zero transforms (default-constructed `BoneTransform`). Document this in the accessor: "Call `SetRootTransform()` before drawing." Add a `DIA_ASSERT(mRootTransformSet)` in `GetWorldTransforms()` to catch misconfigured call order at debug time. |

---

## Status

`Approved`

# Feature Spec: rig2d-visual-debugger-stack

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Decomposes the existing monolithic `DiaRig2DVisualDebugger::VisualDebugger` class into a stack of focused `IVisualDebugger` draw classes, each responsible for one concern. The existing single-class API is retired and replaced by five classes registered independently with `DebugLayerManager`. All draw logic is migrated and extended with the full `DebugColourPalette` and `debugScale` support. Bone names (previously a stub flag that drew nothing) are now wired up via `RequestDrawText()`.

**Problem solved:** The existing `VisualDebugger` is a monolithic on/off toggle — you cannot suppress bone lines while keeping joint circles, and you cannot selectively enable direction arrows. The stack model makes each concern independently toggleable via the manager.

---

## Acceptance Criteria

1. Five focused draw classes replace `VisualDebugger`: `BoneLinesDrawer`, `JointCirclesDrawer`, `DirectionArrowsDrawer`, `RestPoseDrawer`, `BoneLabelsDrawer`
2. Each implements `IVisualDebugger` and lives in `DiaRig2DVisualDebugger.vcxproj`
3. Each stores a `const Skeleton&`, `const DynamicArrayC<BoneTransform, kMaxBones>&` reference (set at construction, not per-call)
4. `Draw(FrameData&)` uses `DebugColourPalette` constants — no ad-hoc colour literals
5. All sizes/lengths multiplied by `DebugLayerManager::GetDebugScale()` — pass manager ref at construction
6. `BoneLabelsDrawer` uses `RequestDrawText()` (requires `debug-text-primitive`)
7. `DirectionArrowsDrawer` draws a `Ray2D` in the local +X direction of each bone using `BoneTransform::rotation`
8. `RestPoseDrawer` draws the bind pose from `Skeleton` local transforms (grey ghost); uses `Pose::ComputeWorldTransforms()` with identity root transform
9. Old `VisualDebugger` class is **removed** from `DiaRig2DVisualDebugger.vcxproj`
10. Canonical layer names from `DebugLayerNames.h` used at `GetLayerName()` return
11. Build with no warnings; all tests pass

---

## Draw Classes

### 1. BoneLinesDrawer (`LayerNames::kRigBones`, priority 10)

Draws a white line from each non-root bone's parent position to the bone's position.

```cpp
class BoneLinesDrawer : public Dia::Debug::IVisualDebugger
{
public:
    BoneLinesDrawer(const Skeleton& skeleton,
                    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& worldTransforms,
                    const Dia::Debug::DebugLayerManager& manager);

    Dia::Core::StringCRC GetLayerName() const override { return Dia::Debug::LayerNames::kRigBones; }
    void Draw(Dia::Graphics::FrameData& frameData) override;

private:
    const Skeleton& mSkeleton;
    const Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones>& mWorldTransforms;
    const Dia::Debug::DebugLayerManager& mManager;
};
```

Draw logic:
- For each bone where `parentIndex >= 0`: `RequestDraw(parentPos, bonePos, DebugColourPalette::kActive)`
- Root bone (parentIndex == -1): no line drawn (root has no parent to connect to)

### 2. JointCirclesDrawer (`LayerNames::kRigJoints`, priority 10)

Draws a circle at each joint position, coloured by role.

Draw logic:
- Root bone (parentIndex == -1): `RequestDraw(pos, 4.0f * scale, DebugColourPalette::kHealthy)` — green root indicator
- Leaf bones (no children): `RequestDraw(pos, 2.5f * scale, DebugColourPalette::kWarning)` — yellow leaf
- Mid-chain bones: `RequestDraw(pos, 2.5f * scale, DebugColourPalette::kActive)` — white mid-chain

Leaf detection: a bone is a leaf if no other bone has it as `parentIndex`. Build a `DynamicArrayC<bool, kMaxBones>` lookup once at the start of `Draw()`.

### 3. DirectionArrowsDrawer (`LayerNames::kRigArrows`, priority 20)

Draws a `Ray2D` on each bone showing its local +X axis direction in world space.

Draw logic:
- `direction = Vector2D(cos(wt.rotation), sin(wt.rotation))` — local +X in world space
- Arrow length = `bone.length * scale` (if `bone.length > 0.0f`), else `4.0f * scale`
- `RequestDrawRay(wt.position, direction, length, DebugColourPalette::kGoal)` — cyan

### 4. RestPoseDrawer (`LayerNames::kRigRestPose`, priority 10)

Draws the skeleton's bind/rest pose as a grey ghost using the skeleton's own local transforms.

Draw logic:
- Calls `Pose::ComputeWorldTransforms(skeleton, identityRoot, outRestTransforms)` once per `Draw()` using a locally-constructed bind `Pose` via `Pose::SetToBindPose(skeleton)`
- Identity root: `BoneTransform{ Vector2D(0,0), 0.0f, Vector2D(1,1) }`
- For each non-root bone: `RequestDraw(restParentPos, restBonePos, DebugColourPalette::kInactive)` — grey

Note: allocates a `DynamicArrayC<BoneTransform, kMaxBones>` on the stack each `Draw()` call for the rest-pose world transforms. Stack allocation, no heap.

### 5. BoneLabelsDrawer (`LayerNames::kRigLabels`, priority 40)

Draws the bone name (`Bone::name.AsChar()`) as a world-space text label at each joint position.

```cpp
void Draw(Dia::Graphics::FrameData& frameData) override
{
    float scale = mManager.GetDebugScale();
    for (int i = 0; i < mSkeleton.GetBoneCount(); ++i)
    {
        const Bone& bone          = mSkeleton.GetBone(i);
        const BoneTransform& wt   = mWorldTransforms[i];
        Dia::Maths::Vector2D labelPos = wt.position + Dia::Maths::Vector2D(4.0f * scale, -4.0f * scale);
        frameData.RequestDrawText(labelPos, bone.name.AsChar(), 12.0f * scale,
                                  Dia::Debug::DebugColourPalette::kActive);
    }
}
```

Label offset: `(+4, -4) * scale` from joint position so text doesn't overlap the circle.

---

## Registration Example (game code)

```cpp
// At application startup, after Skeleton and worldTransforms are available:
static BoneLinesDrawer      boneLinesDrawer  (skeleton, worldTransforms, manager);
static JointCirclesDrawer   jointDrawer      (skeleton, worldTransforms, manager);
static DirectionArrowsDrawer arrowDrawer     (skeleton, worldTransforms, manager);
static RestPoseDrawer       restPoseDrawer   (skeleton, worldTransforms, manager);
static BoneLabelsDrawer     labelsDrawer     (skeleton, worldTransforms, manager);

manager.Register(&boneLinesDrawer,   10);
manager.Register(&jointDrawer,       10);
manager.Register(&arrowDrawer,       20);
manager.Register(&restPoseDrawer,    10);
manager.Register(&labelsDrawer,      40);

// Each frame, after FK / IK:
manager.Draw(frameData);
```

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaRig2DVisualDebugger/VisualDebugger.h` | **Removed** |
| `Dia/DiaRig2DVisualDebugger/VisualDebugger.cpp` | **Removed** |
| `Dia/DiaRig2DVisualDebugger/BoneLinesDrawer.h` | New |
| `Dia/DiaRig2DVisualDebugger/BoneLinesDrawer.cpp` | New |
| `Dia/DiaRig2DVisualDebugger/JointCirclesDrawer.h` | New |
| `Dia/DiaRig2DVisualDebugger/JointCirclesDrawer.cpp` | New |
| `Dia/DiaRig2DVisualDebugger/DirectionArrowsDrawer.h` | New |
| `Dia/DiaRig2DVisualDebugger/DirectionArrowsDrawer.cpp` | New |
| `Dia/DiaRig2DVisualDebugger/RestPoseDrawer.h` | New |
| `Dia/DiaRig2DVisualDebugger/RestPoseDrawer.cpp` | New |
| `Dia/DiaRig2DVisualDebugger/BoneLabelsDrawer.h` | New |
| `Dia/DiaRig2DVisualDebugger/BoneLabelsDrawer.cpp` | New |
| `Dia/DiaRig2DVisualDebugger/DiaRig2DVisualDebugger.vcxproj` | Remove old files, add 10 new files |
| `Dia/DiaRig2DVisualDebugger/DiaRig2DVisualDebugger.vcxproj.filters` | Update filters |

**Dependencies added to vcxproj:** `DiaVisualDebugger.lib` (for `IVisualDebugger`, `DebugColourPalette`, `DebugLayerNames`, `DebugLayerManager`)

**Prerequisites:** `debug-budget`, `debug-text-primitive`, `debug-layer-manager` all implemented.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Write `BoneLinesDrawer.h/.cpp` | Migrates existing bone-line draw logic; uses `kActive` colour |
| 2 | Write `JointCirclesDrawer.h/.cpp` | Migrates circle draw logic; adds leaf detection; uses `kHealthy`/`kWarning`/`kActive` |
| 3 | Write `DirectionArrowsDrawer.h/.cpp` | New — uses `BoneTransform::rotation`; uses `kGoal` colour |
| 4 | Write `RestPoseDrawer.h/.cpp` | New — calls `Pose::SetToBindPose` + `ComputeWorldTransforms`; uses `kInactive` |
| 5 | Write `BoneLabelsDrawer.h/.cpp` | Wires up previously-stub bone names; uses `RequestDrawText()` |
| 6 | Update `DiaRig2DVisualDebugger.vcxproj` — remove old files, add 10 new, add DiaVisualDebugger reference | |
| 7 | Update `DiaRig2DVisualDebugger.vcxproj.filters` | |
| 8 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 9 | Write tests — see test plan | `TestRig2DVisualDebuggerStack.cpp` |
| 10 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/Rig2D/TestRig2DVisualDebuggerStack.cpp`

Test setup: a 3-bone chain (root → mid → tip) with known positions. World transforms computed in test setup.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| BoneLinesDrawer | `Draw_TwoBoneChain_DrawsTwoLines` | 3 bones → 2 lines (mid→root, tip→mid); root has no parent so no line |
| BoneLinesDrawer | `Draw_LineEndpoints_MatchWorldPositions` | Line endpoints match worldTransforms positions |
| BoneLinesDrawer | `Draw_Colour_IsActive` | All lines use `DebugColourPalette::kActive` |
| BoneLinesDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| BoneLinesDrawer | `LayerName_IsRigBones` | `GetLayerName() == LayerNames::kRigBones` |
| JointCirclesDrawer | `Draw_ThreeBones_ThreeCircles` | 3 bones → 3 circles |
| JointCirclesDrawer | `Draw_RootCircle_IsGreen` | Root joint circle colour == `kHealthy` |
| JointCirclesDrawer | `Draw_LeafCircle_IsYellow` | Tip joint (leaf) circle colour == `kWarning` |
| JointCirclesDrawer | `Draw_MidChainCircle_IsWhite` | Mid bone circle colour == `kActive` |
| JointCirclesDrawer | `Draw_RootRadius_LargerThanLeaf` | Root circle radius > leaf circle radius |
| JointCirclesDrawer | `LayerName_IsRigJoints` | `GetLayerName() == LayerNames::kRigJoints` |
| DirectionArrowsDrawer | `Draw_ThreeBones_ThreeRays` | 3 bones → 3 rays |
| DirectionArrowsDrawer | `Draw_ZeroRotation_PointsRight` | Bone at rot=0 → ray direction == (1, 0) |
| DirectionArrowsDrawer | `Draw_Colour_IsGoal` | All rays use `DebugColourPalette::kGoal` |
| DirectionArrowsDrawer | `Draw_Scale_AffectsLength` | `SetDebugScale(2.0f)` → ray length doubles |
| DirectionArrowsDrawer | `LayerName_IsRigArrows` | `GetLayerName() == LayerNames::kRigArrows` |
| RestPoseDrawer | `Draw_ProducesLines` | 3-bone skeleton → 2 lines (rest pose) |
| RestPoseDrawer | `Draw_Colour_IsInactive` | All rest-pose lines use `DebugColourPalette::kInactive` |
| RestPoseDrawer | `Draw_DoesNotModifyWorldTransforms` | worldTransforms unchanged after `Draw()` |
| RestPoseDrawer | `LayerName_IsRigRestPose` | `GetLayerName() == LayerNames::kRigRestPose` |
| BoneLabelsDrawer | `Draw_ThreeBones_ThreeTextPrimitives` | 3 bones → 3 text primitives |
| BoneLabelsDrawer | `Draw_LabelText_MatchesBoneName` | Each text primitive `text` == `bone.name.AsChar()` |
| BoneLabelsDrawer | `Draw_LabelOffset_NonZero` | Label position ≠ joint position (offset applied) |
| BoneLabelsDrawer | `Draw_Colour_IsActive` | All labels use `DebugColourPalette::kActive` |
| BoneLabelsDrawer | `LayerName_IsRigLabels` | `GetLayerName() == LayerNames::kRigLabels` |
| Migration | `AllFiveDrawers_Registered_DrawCorrectTotalPrimitives` | All 5 registered → `Draw()` → primitive count matches expected sum |
| Migration | `OldVisualDebugger_RemovedFromBuild` | Verify `VisualDebugger.h` does not exist (compile-time) |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — layer names use `LayerNames::kRig*` constants (StringCRC) |
| PD-002 | ProcessingUnit/Phase/Module | Compliant — draw classes are plain C++ objects; caller schedules Draw() |
| PD-003 | Component-based entities | Compliant — no entity ID concerns in rig drawing; picking seam is manager's concern |
| PD-004 | No STL in public APIs | Compliant — all public methods use `DynamicArrayC`, `StringCRC`, and primitives only |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — `DiaRig2DVisualDebugger.vcxproj` updated; old files removed, new files added |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant — no per-project overrides |
| PD-009 | Generated output under `Cluiche/out/` | Compliant — no generated output |
| AD-001 | Module YAML frontmatter | Compliant — `dia.rig2dvisualdebugger.architecture.module.md` updated to reflect new class list |
| AD-002 | No STL in public APIs | Compliant |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all classes in `Dia::Rig2D::` namespace |
| SD-DBG-001 | Stack of focused draw classes | Compliant — this feature implements the stack model for Rig2D |
| SD-DBG-002 | `#ifdef DIA_DEBUG` | Compliant — `DiaRig2DVisualDebugger.vcxproj` not linked in Release |
| SD-DBG-003 | Priority-ordered draw | Compliant — canonical priorities used: 10 for geometry, 20 for arrows, 40 for labels |
| SD-DBG-005 | Global `debugScale` | Compliant — all size/length values multiplied by `mManager.GetDebugScale()` |
| SD-DBG-006 | Assert on name collision | Compliant — each drawer returns a distinct `LayerNames::kRig*` constant; manager asserts on collision |
| SD-DBG-010 | `DebugColourPalette` colours binding | Compliant — all ad-hoc `RGBA::Green`, `RGBA::White`, `RGBA::Yellow` replaced with palette constants |
| SD-DBG-014 | Same-family classes share vcxproj | Compliant — all 5 Rig2D draw classes in `DiaRig2DVisualDebugger.vcxproj` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | DebugLayerManager reference | Each draw class stores a `const DebugLayerManager&` to read `GetDebugScale()`. This creates a circular dependency if DiaVisualDebugger depends on DiaRig2DVisualDebugger or vice versa. Is this safe? | Safe — dependency is one-directional: `DiaRig2DVisualDebugger` depends on `DiaVisualDebugger` (for `IVisualDebugger`, `DebugLayerManager`, `DebugColourPalette`). `DiaVisualDebugger` does not depend on `DiaRig2DVisualDebugger`. |
| 2 | RestPoseDrawer stack allocation | `RestPoseDrawer::Draw()` allocates a `DynamicArrayC<BoneTransform, 128>` on the stack every frame (128 × ~20 bytes = ~2.5 KB). Is this acceptable? | Yes — `DynamicArrayC` is a fixed-size stack array; 2.5 KB is within normal stack frame budget. `RestPoseDrawer` is typically disabled in performance-sensitive contexts. |
| 3 | Leaf detection in JointCirclesDrawer | Leaf detection requires a per-Draw scan to build a `bool[kMaxBones]` lookup. For 128 bones this is 128 comparisons per Draw call. Acceptable? | Yes — 128 comparisons is negligible. Stack-allocate `bool isLeaf[kMaxBones] = {}` at Draw() start; mark parents in one pass before the draw pass. |
| 4 | Old VisualDebugger removal | Removing `VisualDebugger.h/.cpp` is a breaking change for any game code using the old API. Is game code in CluicheTest using it? | Must audit `CluicheTest/` for `#include "VisualDebugger.h"` or `Dia::Rig2D::VisualDebugger` before removal. Update any call site to register the stack classes instead. Include as task 0 in the implementation. |
| 5 | BoneLabelsDrawer font size and scale | `12.0f * scale` for font size. If `scale` is very large (e.g. 10.0f), font size becomes 120px — very large. Should font size have an upper clamp? | Add a `static constexpr float kMaxFontSize = 24.0f` clamp: `fontSize = std::min(12.0f * scale, kMaxFontSize)`. Prevents runaway text at extreme scales. |

---

## Status

`Approved`

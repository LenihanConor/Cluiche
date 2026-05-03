# Feature Spec: animation2d-visual-debugger-stack

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diavisualdebugger.md |
| Feature | this file |

---

## Summary

Creates a new `DiaAnimation2DVisualDebugger` module as a stack of three focused `IVisualDebugger` draw classes, and adds the minimum public read accessor surface across `AnimationEvaluator`, `PoseBlendStack`, and `SpringChain` that the draw classes require. The three classes cover: clip playback cursors (active clip name + normalised time), blend layer weights, and spring chain physics state (per-node angular velocity coloured by activity). All data is read from an `AnimationEvaluator` held by const reference, alongside the world transforms maintained by the caller.

**Problem solved:** `DiaAnimation2D` has no visual debugging. Developers cannot see which clips are playing, how blend weights are distributed across layers, or whether spring chains are at rest or oscillating rapidly. The three-class stack provides this visibility on independent, toggleable layers.

---

## Acceptance Criteria

1. Three focused draw classes: `AnimClipCursorDrawer`, `AnimBlendWeightsDrawer`, `AnimSpringDrawer`
2. Each implements `IVisualDebugger` and lives in a new `DiaAnimation2DVisualDebugger.vcxproj` static library
3. Each stores a `const AnimationEvaluator&`, a `const Skeleton&`, a `const DynamicArrayC<BoneTransform, kMaxBones>&`, and a `const DebugLayerManager&` (set at construction)
4. Ten new public read-only accessors added across `AnimationEvaluator`, `PoseBlendStack`, and `SpringChain` in `DiaAnimation2D` — listed below
5. `Draw(FrameData&)` uses `DebugColourPalette` constants — no ad-hoc colour literals
6. `AnimClipCursorDrawer` and `AnimBlendWeightsDrawer` use `RequestDrawText()` (requires `debug-text-primitive`)
7. All sizes multiplied by `DebugLayerManager::GetDebugScale()`
8. Canonical layer names from `DebugLayerNames.h` used at `GetLayerName()` return
9. New `DiaAnimation2DVisualDebugger.vcxproj` added to `Cluiche.sln`
10. Build with no warnings; all tests pass

---

## DiaAnimation2D Accessor Additions

The following public methods are added to existing `DiaAnimation2D` classes. They expose the minimal read surface required by the three draw classes, without making internal simulation state mutable.

### AnimationEvaluator additions

```cpp
// Source iteration — index 0-based, up to GetSourceCount()-1
int GetSourceCount() const;
Dia::Core::StringCRC GetSourceId(int index) const;

// Typed source access — returns nullptr if source at that ID is not the requested type
const AnimClipPlayer* GetClipPlayer(Dia::Core::StringCRC sourceId) const;
const SpringChain*    GetSpringChain(Dia::Core::StringCRC sourceId) const;
```

### PoseBlendStack addition

```cpp
// Index-based layer ID access — enables iteration without knowing IDs in advance
Dia::Core::StringCRC GetLayerId(int index) const;
```

Combined with the existing `GetLayerCount()`, `GetLayerWeight(StringCRC)`, and `GetLayerPriority(StringCRC)`, this enables full iteration.

### SpringChain additions

```cpp
// Per-node bone identity — node index matches boneIds order in SpringChainDef
Dia::Core::StringCRC GetNodeBoneId(int nodeIndex) const;

// Per-node physics state — read-only; updated each Update() call
float GetNodeAngularVelocity(int nodeIndex) const;
float GetNodeStiffness(int nodeIndex) const;
float GetNodeDamping(int nodeIndex) const;

// Chain-level gravity
Dia::Maths::Vector2D GetGravityDirection() const;
float                GetGravityStrength() const;
```

---

## Draw Classes

### 1. AnimClipCursorDrawer (`LayerNames::kAnimClipCursor`, priority 40)

Draws a text label near the skeleton root for each registered `AnimClipPlayer`, showing clip name and normalised playback time.

Draw logic:
- For each source index `i` in `evaluator.GetSourceCount()`:
  - `id = evaluator.GetSourceId(i)`
  - `player = evaluator.GetClipPlayer(id)` — skip if `nullptr` (source is a spring chain)
  - `rootPos = worldTransforms[0].position` (bone 0 is root)
  - `labelPos = rootPos + Vector2D(-20.0f * scale, (-8.0f * i - 4.0f) * scale)` — stacked vertically left of root
  - If `player->IsPlaying()`:
    - Label text: `"[clipId] 0.45"` — `id.AsString()` + normalised time formatted to 2 decimal places
    - Colour: `DebugColourPalette::kActive` — white
  - If not playing:
    - Label text: `"[clipId] stopped"`
    - Colour: `DebugColourPalette::kInactive` — grey
  - `frameData.RequestDrawText(labelPos, labelText, 10.0f * scale, colour)`
  - Font size clamped to `kMaxFontSize = 20.0f`

### 2. AnimBlendWeightsDrawer (`LayerNames::kAnimBlendWeights`, priority 40)

Draws a text label near the skeleton root for each layer in the `PoseBlendStack`, showing layer ID, weight, and priority.

Draw logic:
- `stack = evaluator.GetBlendStack()`
- `rootPos = worldTransforms[0].position`
- For each layer index `i` in `stack.GetLayerCount()`:
  - `layerId = stack.GetLayerId(i)`
  - `weight = stack.GetLayerWeight(layerId)`
  - `priority = stack.GetLayerPriority(layerId)`
  - `labelPos = rootPos + Vector2D(4.0f * scale, (-8.0f * i - 4.0f) * scale)` — stacked vertically right of root
  - Label text: `"layerId w=0.75 p=1"` — id + weight (2dp) + priority
  - Colour: `weight > 0.05f` → `DebugColourPalette::kActive`; else `DebugColourPalette::kInactive`
  - `frameData.RequestDrawText(labelPos, labelText, 10.0f * scale, colour)`
  - Font size clamped to `kMaxFontSize = 20.0f`

### 3. AnimSpringDrawer (`LayerNames::kAnimSpring`, priority 20)

Draws per-node spring state circles coloured by angular velocity magnitude, and a gravity direction indicator from each spring chain root.

Draw logic:
- For each source index `i` in `evaluator.GetSourceCount()`:
  - `id = evaluator.GetSourceId(i)`
  - `chain = evaluator.GetSpringChain(id)` — skip if `nullptr` (source is a clip player)
  - For each node `n` in `chain->GetNodeCount()`:
    - `boneId = chain->GetNodeBoneId(n)` — resolve to bone index via `skeleton.FindBone(boneId)`
    - If bone index not found: skip
    - `pos = worldTransforms[boneIndex].position`
    - `angVel = abs(chain->GetNodeAngularVelocity(n))`
    - Colour by activity:
      - `angVel < 0.5f`: `DebugColourPalette::kHealthy` — green (near rest)
      - `0.5f ≤ angVel < 5.0f`: `DebugColourPalette::kWarning` — yellow (active)
      - `angVel ≥ 5.0f`: `DebugColourPalette::kError` — red (fast/potentially unstable)
    - `frameData.RequestDraw(pos, 3.0f * scale, colour)` — circle outline
  - Gravity indicator (if `chain->GetGravityStrength() > 0.0f`):
    - `chainRootBoneId = chain->GetNodeBoneId(0)`; resolve to index
    - `rootPos = worldTransforms[chainRootIndex].position`
    - `frameData.RequestDrawRay(rootPos, chain->GetGravityDirection(), 3.0f * scale, DebugColourPalette::kInactive)` — grey gravity arrow

---

## Angular Velocity Colour Reference

| Range | Colour | Semantic |
|-------|--------|---------|
| `< 0.5 rad/s` | `kHealthy` (green) | At rest / converged |
| `0.5–5.0 rad/s` | `kWarning` (yellow) | Active spring oscillation |
| `≥ 5.0 rad/s` | `kError` (red) | Fast / potentially unstable |

---

## New Module: DiaAnimation2DVisualDebugger

**Project references required:**
- `DiaCore.vcxproj`
- `DiaMaths.vcxproj`
- `DiaRig2D.vcxproj`
- `DiaAnimation2D.vcxproj`
- `DiaGraphics.vcxproj`
- `DiaVisualDebugger.vcxproj`

---

## Registration Example (game code)

```cpp
// worldTransforms maintained by caller, computed after AnimationEvaluator::Evaluate():
// Pose::ComputeWorldTransforms(skeleton, rootTransform, worldTransforms);

static AnimClipCursorDrawer   cursorDrawer (evaluator, skeleton, worldTransforms, manager);
static AnimBlendWeightsDrawer weightsDrawer(evaluator, skeleton, worldTransforms, manager);
static AnimSpringDrawer       springDrawer (evaluator, skeleton, worldTransforms, manager);

manager.Register(&cursorDrawer,  40);
manager.Register(&weightsDrawer, 40);
manager.Register(&springDrawer,  20);

// Each frame, after AnimationEvaluator::Evaluate() and ComputeWorldTransforms():
manager.Draw(frameData);
```

---

## Files Changed

| File | Change |
|------|--------|
| `Dia/DiaAnimation2D/AnimationEvaluator.h` | Add 4 accessor declarations |
| `Dia/DiaAnimation2D/AnimationEvaluator.cpp` | Implement 4 accessors |
| `Dia/DiaAnimation2D/PoseBlendStack.h` | Add `GetLayerId(int)` declaration |
| `Dia/DiaAnimation2D/PoseBlendStack.cpp` | Implement `GetLayerId(int)` |
| `Dia/DiaAnimation2D/SpringChain.h` | Add 6 accessor declarations |
| `Dia/DiaAnimation2D/SpringChain.cpp` | Implement 6 accessors |
| `Dia/DiaAnimation2DVisualDebugger/AnimClipCursorDrawer.h` | New |
| `Dia/DiaAnimation2DVisualDebugger/AnimClipCursorDrawer.cpp` | New |
| `Dia/DiaAnimation2DVisualDebugger/AnimBlendWeightsDrawer.h` | New |
| `Dia/DiaAnimation2DVisualDebugger/AnimBlendWeightsDrawer.cpp` | New |
| `Dia/DiaAnimation2DVisualDebugger/AnimSpringDrawer.h` | New |
| `Dia/DiaAnimation2DVisualDebugger/AnimSpringDrawer.cpp` | New |
| `Dia/DiaAnimation2DVisualDebugger/DiaAnimation2DVisualDebugger.vcxproj` | New project |
| `Dia/DiaAnimation2DVisualDebugger/DiaAnimation2DVisualDebugger.vcxproj.filters` | New filters |
| `Cluiche/Cluiche.sln` | Add new project |

**Prerequisites:** `debug-budget`, `debug-text-primitive`, `debug-layer-manager` all implemented.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add 4 accessor declarations + implementations to `AnimationEvaluator` | `GetSourceCount`, `GetSourceId`, `GetClipPlayer`, `GetSpringChain` |
| 2 | Add `GetLayerId(int)` to `PoseBlendStack` | |
| 3 | Add 6 accessor declarations + implementations to `SpringChain` | `GetNodeBoneId`, `GetNodeAngularVelocity`, `GetNodeStiffness`, `GetNodeDamping`, `GetGravityDirection`, `GetGravityStrength` |
| 4 | Create `DiaAnimation2DVisualDebugger/` directory and new vcxproj | Modelled on `DiaRigidBody2DVisualDebugger.vcxproj` |
| 5 | Write `AnimClipCursorDrawer.h/.cpp` | Text labels; normalised time; playing/stopped colour |
| 6 | Write `AnimBlendWeightsDrawer.h/.cpp` | Text labels; weight + priority per layer |
| 7 | Write `AnimSpringDrawer.h/.cpp` | Coloured circles by angular velocity; gravity indicator |
| 8 | Add project to `Cluiche.sln` | |
| 9 | Build solution — verify zero warnings | `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |
| 10 | Write tests | `TestAnimation2DVisualDebuggerStack.cpp` |
| 11 | Run tests | `Cluiche/bin/Debug/x64/UnitTests.exe` |

---

## Test Plan

**File:** `Cluiche/Tests/GoogleTests/Animation2D/TestAnimation2DVisualDebuggerStack.cpp`

Test setup: an `AnimationEvaluator` with a 3-bone skeleton, one `AnimClipPlayer` registered (playing), one `SpringChain` registered (3 nodes). Known world transforms.

| Suite | Test | What it verifies |
|-------|------|-----------------|
| AnimationEvaluator accessors | `GetSourceCount_AfterRegister_IsTwo` | Register clip + spring → `GetSourceCount() == 2` |
| AnimationEvaluator accessors | `GetClipPlayer_ReturnsNonNull` | Registered clip player → non-null |
| AnimationEvaluator accessors | `GetSpringChain_ReturnsNonNull` | Registered spring chain → non-null |
| AnimationEvaluator accessors | `GetClipPlayer_SpringId_ReturnsNull` | Spring chain ID passed to `GetClipPlayer` → nullptr |
| PoseBlendStack accessors | `GetLayerId_Index0_MatchesAddedId` | `AddLayer(id, ...)` → `GetLayerId(0) == id` |
| SpringChain accessors | `GetNodeBoneId_Index0_MatchesDef` | First bone ID matches `SpringChainDef::boneIds[0]` |
| SpringChain accessors | `GetGravityStrength_MatchesDef` | Def gravity strength round-trips |
| AnimClipCursorDrawer | `Draw_PlayingClip_DrawsTextPrimitive` | Playing clip → 1 text primitive |
| AnimClipCursorDrawer | `Draw_PlayingClip_IsActive` | Playing clip label colour == `kActive` |
| AnimClipCursorDrawer | `Draw_StoppedClip_IsInactive` | Stopped clip label colour == `kInactive` |
| AnimClipCursorDrawer | `Draw_Disabled_NoPrimitives` | `SetEnabled(false)` → 0 primitives |
| AnimClipCursorDrawer | `LayerName_IsAnimClipCursor` | `GetLayerName() == LayerNames::kAnimClipCursor` |
| AnimBlendWeightsDrawer | `Draw_TwoLayers_TwoTextPrimitives` | 2 blend layers → 2 text primitives |
| AnimBlendWeightsDrawer | `Draw_ZeroWeightLayer_IsInactive` | Weight < 0.05 → `kInactive` colour |
| AnimBlendWeightsDrawer | `LayerName_IsAnimBlendWeights` | `GetLayerName() == LayerNames::kAnimBlendWeights` |
| AnimSpringDrawer | `Draw_ThreeNodes_ThreeCircles` | 3 spring nodes → 3 circle primitives |
| AnimSpringDrawer | `Draw_NearRest_IsHealthy` | `angVel < 0.5` → `kHealthy` colour |
| AnimSpringDrawer | `Draw_Active_IsWarning` | `0.5 ≤ angVel < 5.0` → `kWarning` colour |
| AnimSpringDrawer | `Draw_Fast_IsError` | `angVel ≥ 5.0` → `kError` colour |
| AnimSpringDrawer | `Draw_GravityEnabled_DrawsRay` | `gravityStrength > 0` → 1 ray primitive per chain |
| AnimSpringDrawer | `Draw_GravityZero_NoRay` | `gravityStrength == 0` → no ray |
| AnimSpringDrawer | `LayerName_IsAnimSpring` | `GetLayerName() == LayerNames::kAnimSpring` |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|-----------|
| PD-001 | StringCRC for identifiers | Compliant — layer names use `LayerNames::kAnim*` constants (StringCRC) |
| PD-002 | ProcessingUnit/Phase/Module | Compliant — draw classes are plain C++ objects |
| PD-003 | Component-based entities | Compliant — no entity ID concerns |
| PD-004 | No STL in public APIs | Compliant — all public methods use `DynamicArrayC`, `StringCRC`, and primitives only |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — new `DiaAnimation2DVisualDebugger.vcxproj` created and added to solution |
| PD-007 | C++20 required | Compliant |
| PD-008 | `Directory.Build.props` owns build paths | Compliant |
| PD-009 | Generated output under `Cluiche/out/` | Compliant |
| AD-001 | Module YAML frontmatter | Compliant — new `dia.animation2dvisualdebugger.architecture.module.md` created |
| AD-002 | No STL in public APIs | Compliant |
| AD-003 | `Dia::<Module>::` namespace | Compliant — all classes in `Dia::Animation2D::` namespace |
| SD-DBG-001 | Stack of focused draw classes | Compliant — three classes registered independently |
| SD-DBG-002 | `#ifdef DIA_DEBUG` | Compliant — project not linked in Release |
| SD-DBG-003 | Priority-ordered draw | Compliant — springs at 20, text overlays at 40 |
| SD-DBG-005 | Global `debugScale` | Compliant — all sizes and font sizes multiplied by `mManager.GetDebugScale()` |
| SD-DBG-006 | Assert on name collision | Compliant — each drawer returns a distinct `LayerNames::kAnim*` constant |
| SD-DBG-010 | `DebugColourPalette` colours binding | Compliant — all colours from palette |
| SD-DBG-014 | Same-family classes share vcxproj | Compliant — all 3 animation draw classes in `DiaAnimation2DVisualDebugger.vcxproj` |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | World transforms ownership | The draw classes hold a `const DynamicArrayC<BoneTransform, kMaxBones>&`. This is the same world transform array the caller computes via `Pose::ComputeWorldTransforms()` after `Evaluate()`. Is it guaranteed to be up-to-date when `Draw()` is called? | Yes — caller must call `Draw()` in the same frame after `Evaluate()` and `ComputeWorldTransforms()`. Document in the registration example: "Call `manager.Draw(frameData)` after `Evaluate()` and `ComputeWorldTransforms()`." |
| 2 | `AnimationEvaluator` internal source type | `GetClipPlayer(id)` returns nullptr if the source is a spring chain. This requires the evaluator to track source type alongside each registered source. Does the current internal `Source` struct distinguish type? | Must be verified at Task 1. If not, add a `SourceType` enum (`kClipPlayer`, `kSpringChain`) to the internal `Source` struct. The accessor casts accordingly. |
| 3 | `Skeleton::FindBone(StringCRC)` — does it exist? | `AnimSpringDrawer` resolves `GetNodeBoneId(n)` to a bone index using the skeleton. Does `Skeleton` expose a `FindBone(StringCRC)` or `FindBoneIndex(StringCRC)` method? | Must be verified at Task 7. If not, add `int FindBoneIndex(Dia::Core::StringCRC boneId) const` to `Skeleton.h` — returns -1 if not found. Skip that node in the draw loop if -1. |
| 4 | Text label character budget | Clip names + normalised time formatted as `"[walk_cycle] 0.45"` may exceed 63 characters if IDs are long. Is truncation via `RequestDrawText` silent truncation sufficient? | Yes — `RequestDrawText` already truncates to 63 chars silently. Debug labels that are too long just get clipped. No special handling needed. |
| 5 | `AnimBlendWeightsDrawer` vs `AnimClipCursorDrawer` priority collision | Both are registered at priority 40. They render to different screen positions (left vs. right of root) so they don't visually overlap. Is sharing priority 40 intentional? | Yes — priority 40 is the state-indicator tier (SD-DBG-003). Both are state indicators. Draw order between same-priority layers is registration order, which is fine since they don't overlap. |

---

## Status

`Approved`

# Feature Spec: Damped Spring Chain

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaanimation2d.md | **damped-spring-chain** |

**Status:** `Approved`

---

## Problem Statement

Secondary motion (tail wobble, frill follow-through, neck sway) on animated characters requires a lightweight simulation that reacts to the primary animation without physics coupling. DiaSoftBody2D's PBD Rope is too heavyweight for visual-only follow-through — it solves constraints, handles collision, and couples to rigid bodies. A simpler angular spring-damper chain is needed that drives bone local rotations directly, is fully deterministic on fixed dt, and is trivially testable.

---

## Solution Overview

Provide three types — `SpringNodeDef`, `SpringChainDef`, and `SpringChain` — in the `Dia::Animation2D` namespace.

### SpringNodeDef

Per-node spring parameters:

```cpp
namespace Dia::Animation2D {
    struct SpringNodeDef {
        float stiffness          = 50.0f;   // Spring constant k
        float damping            = 5.0f;    // Damping coefficient d
        float maxAngularVelocity = 20.0f;   // Radians/sec clamp
    };
}
```

### SpringChainDef

Chain configuration:

```cpp
namespace Dia::Animation2D {
    struct SpringChainDef {
        Dia::Core::StringCRC                                        id;
        Dia::Core::StringCRC                                        rootBoneId;     // Pinned to this skeleton bone
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC>  boneIds;        // Must be contiguous in skeleton
        SpringNodeDef                                               defaultNode;
        Dia::Core::Containers::DynamicArrayC<SpringNodeDef>         nodeOverrides;  // empty = use default
        Dia::Maths::Vector2D                                        gravityDirection = {0.0f, -1.0f};
        float                                                       gravityStrength  = 0.0f; // 0 = no gravity
    };
}
```

### SpringChain

Runtime simulation:

```cpp
namespace Dia::Animation2D {
    class SpringChain {
    public:
        explicit SpringChain(const SpringChainDef& def, const Dia::Rig2D::Skeleton& skeleton);

        // Advance simulation by dt. Reads chain root world rotation from worldTransforms.
        // Internally sub-steps at max 1/120s. Writes local rotations back to pose.
        // Caller must have run ComputeWorldTransforms before calling this.
        void Update(float dt,
                    Dia::Rig2D::Pose& pose,
                    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform>& worldTransforms);

        // Apply an external torque to a specific node (e.g. wind, impact).
        // Torque is consumed on the next Update call and then cleared.
        void ApplyExternalTorque(int nodeIndex, float torque);

        // Snap all nodes to the current pose (zero velocity). Use after teleport.
        void Reset(const Dia::Rig2D::Pose& pose);

        // Runtime parameter tuning (e.g. genetics overrides). No destroy/recreate needed.
        void SetNodeStiffness(int nodeIndex, float stiffness);
        void SetNodeDamping(int nodeIndex, float damping);
        void SetNodeMaxAngularVelocity(int nodeIndex, float maxAngularVelocity);
        void SetGravity(const Dia::Maths::Vector2D& direction, float strength);

        const Dia::Core::StringCRC& GetId() const;
        int GetNodeCount() const;
    };
}
```

### Key Behaviours

1. **Semi-implicit Euler integration**: `v += a * dt`, `angle += v * dt` (AND-012).
2. **Internal sub-stepping** at max 1/120s per step (AND-013). At 60 Hz = 1 sub-step. 100ms hitch = ~12 sub-steps.
3. **Angular velocity clamped** per node per sub-step to `maxAngularVelocity` (AND-019).
4. **Contiguous bones**: Bones must be contiguous in skeleton's flat array — DIA_ASSERT at construction (AND-014).
5. **FK precondition**: Caller must have run FK before `Update` — reads world transforms from provided array (AND-015).
6. **Gravity as torque**: Gravity applied as torque per node internally during `Update` (AND-027).
7. **External torque**: Consumed on next `Update` then cleared.
8. **Reset**: `Reset()` snaps all nodes to current pose with zero velocity — required after teleport.
9. **Runtime setters**: Preserve simulation state (velocities) (AND-028).
10. **Pose contract**: Writes to Pose local rotations only, same contract as DiaIK2D (AND-005).
11. **Rotation-only**: No position bobbing in v1.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaAnimation2D/SpringNodeDef.h` | SpringNodeDef struct |
| `Dia/DiaAnimation2D/SpringChainDef.h` | SpringChainDef struct |
| `Dia/DiaAnimation2D/SpringChain.h` | SpringChain class declaration |
| `Dia/DiaAnimation2D/SpringChain.cpp` | SpringChain implementation |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `SpringNodeDef` struct stores stiffness (default 50.0f), damping (default 5.0f), and maxAngularVelocity (default 20.0f) | Code review |
| 2 | `SpringChainDef` stores id, rootBoneId, boneIds, defaultNode, nodeOverrides, gravityDirection, and gravityStrength | Code review |
| 3 | `SpringChain` constructor resolves bone names to indices via `Skeleton::FindBoneIndex` | Unit test: construct with valid bone names, verify GetNodeCount matches boneIds count |
| 4 | `SpringChain` constructor DIA_ASSERTs if boneIds do not resolve to contiguous ascending indices in the skeleton | Unit test: construct with non-contiguous bones, DIA_ASSERT fires |
| 5 | Semi-implicit Euler integration produces correct output for known inputs: a single node with known stiffness/damping displaced by a known angle converges toward rest | Unit test: step with fixed dt = 1/60, verify angular position within tolerance (1e-5) |
| 6 | Sub-stepping splits large dt correctly: `Update(0.1)` at default settings produces ~12 internal sub-steps at max 1/120s each | Unit test: compare `Update(0.1)` result with 12 sequential `Update(1/120)` calls — results must be identical |
| 7 | Angular velocity is clamped per node per sub-step to `maxAngularVelocity` | Unit test: apply extreme external torque, verify angular velocity never exceeds maxAngularVelocity after Update |
| 8 | Gravity is applied as torque per node when `gravityStrength > 0` | Unit test: construct chain with gravity, verify nodes drift in gravity direction over multiple updates |
| 9 | `ApplyExternalTorque` adds torque that affects the next Update, then the torque is cleared | Unit test: apply torque, call Update, verify effect; call Update again, verify no residual effect |
| 10 | `Reset()` snaps all nodes to the current pose with zero angular velocity | Unit test: displace chain, call Reset, verify next Update produces no motion |
| 11 | `SetNodeStiffness` changes stiffness without resetting velocity state | Unit test: set stiffness mid-simulation, verify velocity is preserved but spring force changes |
| 12 | `SetNodeDamping` changes damping without resetting velocity state | Unit test: set damping mid-simulation, verify velocity is preserved but damping force changes |
| 13 | `SetNodeMaxAngularVelocity` changes the velocity clamp without resetting state | Unit test: reduce maxAngularVelocity mid-simulation, verify clamping takes effect on next Update |
| 14 | `SetGravity` changes gravity direction and strength at runtime | Unit test: change gravity direction mid-simulation, verify drift direction changes |
| 15 | SpringChain writes only to `Pose` local rotations — position and scale components are unchanged after Update | Unit test: record pose position/scale before Update, verify identical after Update |
| 16 | All public API uses Dia containers (DynamicArrayC), no STL | Code review |
| 17 | All code in `Dia::Animation2D::` namespace | Code review |
| 18 | `GetId()` returns the id from the SpringChainDef | Unit test |
| 19 | `GetNodeCount()` returns the number of bone IDs in the chain | Unit test |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `Dia/DiaAnimation2D/SpringNodeDef.h` — struct with default field values | - | Plain data struct, no .cpp needed |
| 2 | Create `Dia/DiaAnimation2D/SpringChainDef.h` — struct with gravity fields, DynamicArrayC members | 1 | |
| 3 | Implement `Dia/DiaAnimation2D/SpringChain.h` / `SpringChain.cpp` — construction (bone index resolution via FindBoneIndex, contiguity DIA_ASSERT), Update (sub-stepping loop, semi-implicit Euler per node, velocity clamp, gravity-as-torque), ApplyExternalTorque, Reset, runtime setters, GetId, GetNodeCount | 1, 2 | Core implementation task |
| 4 | Unit tests in `Cluiche/Tests/GoogleTests/Animation2D/` — cover all 19 acceptance criteria | 3 | |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — SpringChainDef.id, SpringChainDef.rootBoneId, SpringChainDef.boneIds all use StringCRC |
| PD-004 | Platform | No STL containers in public APIs | Compliant — boneIds, nodeOverrides use DynamicArrayC; no std::vector, std::string, etc. |
| PD-005 | Platform | x64 only | Compliant — DiaAnimation2D.vcxproj targets x64 exclusively |
| PD-006 | Platform | VS project files are source of truth | Compliant — .vcxproj and .vcxproj.filters manually maintained with all files listed |
| PD-007 | Platform | C++20 required | Compliant — compiled under /std:c++20 |
| PD-008 | Platform | Directory.Build.props owns build paths/toolchain | Compliant — DiaAnimation2D.vcxproj will not override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Compliant — dia.animation2d.architecture.module.md will be created as part of the DiaAnimation2D system setup (separate from this feature) |
| AD-002 | Dia App | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — all code in Dia::Animation2D:: |
| AND-001 | DiaAnimation2D | Angular spring-damper integration, not PBD | Compliant — this feature implements the angular spring-damper model; no constraint solving or PBD |
| AND-005 | DiaAnimation2D | Writes to Pose local rotations, same contract as DiaIK2D | Compliant — SpringChain::Update writes only local rotations; position and scale unchanged |
| AND-010 | DiaAnimation2D | No STL in public APIs | Compliant — reinforces PD-004/AD-002 |
| AND-012 | DiaAnimation2D | Semi-implicit Euler integration | Compliant — Update uses v += a*dt then angle += v*dt |
| AND-013 | DiaAnimation2D | Internal sub-stepping at max 1/120s | Compliant — Update subdivides dt into steps of at most ~8.3ms |
| AND-014 | DiaAnimation2D | Bones must be contiguous in skeleton flat array | Compliant — DIA_ASSERT at construction verifying contiguous ascending index range |
| AND-015 | DiaAnimation2D | FK must be run before Update | Compliant — Update takes worldTransforms as parameter; caller responsible for providing fresh FK results |
| AND-019 | DiaAnimation2D | Angular velocity clamped to configurable maximum | Compliant — velocity clamped per node per sub-step to SpringNodeDef.maxAngularVelocity |
| AND-027 | DiaAnimation2D | Optional gravity (direction + strength) | Compliant — SpringChainDef includes gravityDirection and gravityStrength; applied internally as torque |
| AND-028 | DiaAnimation2D | Runtime setters preserve simulation state | Compliant — SetNodeStiffness, SetNodeDamping, SetNodeMaxAngularVelocity, SetGravity do not reset velocities |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Integration | What happens if dt is 0 or negative? | dt == 0: Update is a no-op (zero sub-steps). dt < 0: Not a valid input. DIA_ASSERT that dt >= 0 at the top of Update. Negative time stepping is not supported — reverse playback is a clip player concern, not a physics simulation concern. |
| 2 | Edge Cases | What if the chain has only 1 node (single bone)? | Valid — a 1-node chain is a single bone with spring behavior driven relative to its parent (the root bone). The chain resolves rootBoneId + 1 boneId. Update reads the root world rotation and applies spring torque to the single node. No special-casing needed; the sub-stepping and integration logic work identically for N=1. |
| 3 | Parameters | What if stiffness or damping are set to 0? | stiffness == 0: No restoring force — the node drifts freely under external torque and gravity only. This is a valid configuration (e.g., a completely limp chain). damping == 0: No energy dissipation — the node oscillates indefinitely. This is physically correct but may produce unwanted perpetual motion. Neither value should be rejected; both are valid simulation parameters. Negative values should DIA_ASSERT. |
| 4 | Gravity | Can gravityDirection be unnormalized? | Yes — the implementation should normalize gravityDirection internally (or document that it must be unit length). Recommended approach: normalize at construction and in SetGravity. If the direction is zero-length and strength > 0, that is a DIA_LOG_WARNING and gravity is effectively disabled. This prevents division-by-zero in normalization. |
| 5 | Thread Safety | What is the thread safety contract for SpringChain? | SpringChain is NOT thread-safe. A single SpringChain instance must not be accessed concurrently from multiple threads. This matches the convention of DiaRig2D (Pose, Skeleton) and DiaIK2D (IKSolver). Game code is responsible for synchronization if spring chains are updated from different processing units. Document this in the class header. |
| 6 | Integration | How does SpringChain interact with AnimationEvaluator? | AnimationEvaluator registers a SpringChain via RegisterSpringChain, allocates an intermediate Pose for it, and calls SpringChain::Update during its Evaluate pipeline (after FK, after clip sampling). The SpringChain writes to the evaluator-owned Pose, which is then fed into the PoseBlendStack. SpringChain has no knowledge of AnimationEvaluator — it operates on whatever Pose and worldTransforms are provided. |
| 7 | Construction | What if nodeOverrides is non-empty but has a different count than boneIds? | If nodeOverrides.Size() > 0 but != boneIds.Size(), the mapping is ambiguous. DIA_ASSERT that nodeOverrides is either empty (use defaultNode for all) or has exactly the same count as boneIds (one override per node in order). Partial overrides are not supported — use defaultNode values explicitly in the override array for nodes that should use defaults. |
| 8 | Reset | Should Reset be called after construction, or does the constructor initialize state from the skeleton bind pose? | The constructor initializes all node angles from the skeleton bind pose with zero velocity — equivalent to an implicit Reset. Explicit Reset is only needed after teleport or other discontinuous pose changes at runtime. No caller action required immediately after construction. |

---

## Open Questions

None — all resolved above.

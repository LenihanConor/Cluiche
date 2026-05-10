# System Spec: DiaIK2D

## Parent Application
@docs/specs/applications/dia.md

**Status:** `Done`

---

## Purpose

DiaIK2D is the inverse kinematics system for the Dia engine. It operates as a post-process pass on top of DiaRig2D: it reads the current bone transforms from a `Skeleton`, runs one or more IK solvers to position an end effector at a target, and writes the modified bone rotations back in-place. DiaRig2D owns all joint data; DiaIK2D is stateless between frames (it holds registered chain definitions, but no simulation state).

DiaIK2D sits immediately above DiaRig2D in the animation stack:

```
DiaAnimation2D (future) — clips, playback, blend trees
DiaIK2D                 — solvers: two-bone, FABRIK, look-at
DiaRig2D                — skeleton definition, forward kinematics, pose representation
```

Three solvers cover the standard game IK use cases:
- **Analytic Two-Bone** — closed-form solution for exactly 2-joint limb chains (arm, leg). Fast and exact; requires a pole vector for bend direction.
- **FABRIK** (Forward And Backward Reaching IK) — iterative solver for arbitrary-length chains (spine, tail, finger, rope). Converges in 10–20 iterations.
- **Look-At** — rotates a single bone to face a 2D world-space target (eyes, head aim, gun barrel, turret).

**Dependency chain:**
`DiaIK2D → DiaRig2D → DiaMaths → DiaCore`
`DiaIK2D → DiaLogger`

---

## Responsibilities

- Define `IKChainDef`: chain ID, start bone, end bone, reach weight, solver parameters, per-joint angle limits
- Define `JointLimitDef`: per-joint min/max rotation clamp (radians), enable flag
- Define `PoleVector`: world-space direction hint + influence weight for two-bone bend direction
- Provide `IKSolver`: wraps a `Skeleton&` and `Pose&`, manages named registered chains, dispatches solve calls, triggers FK propagation on the modified bone range after each solve
- Implement the **Analytic Two-Bone Solver**: closed-form geometric solve for exactly 2-joint chains; handles unreachable targets (full extension); applies pole vector and reach weight
- Implement the **FABRIK Solver**: iterative N-joint solver with configurable max iterations, convergence tolerance, and per-joint angle limit projection; root bone is pinned
- Implement the **Look-At Constraint**: rotates a single bone so its forward axis points at a 2D target; supports axis angle offset and reach weight
- Require `SetRootTransform(BoneTransform)` to be called once per frame before any solve — stores the root world transform and refreshes the internal world-transform cache via FK
- Apply **joint angle limits** during solver iterations (two-bone: clamp after analytic solve; FABRIK: project per iteration)
- Apply **reach weight** per chain: blend linearly between the pre-solve `Pose` local rotation and the IK result, writing back to `Pose`
- Emit structured debug logs to the `Rig2D` DiaLogger channel (debug builds only): unreachable targets, joint limit violations, FABRIK non-convergence
- Provide test utilities under `DiaIK2D/Testing/`: `AssertBoneRotation`, `AssertEndEffectorPosition`, `BuildTestSkeleton` helpers — shipped with the library, consumer opt-in via include
- Provide `dia.ik2d.architecture.module.md` YAML module documentation
- Provide `DiaIK2D.vcxproj` static library registered in `Cluiche.sln`

---

## Non-Responsibilities

- Skeleton definition, FK propagation, pose blending — DiaRig2D
- Animation clips, blend trees, playback — future DiaAnimation2D
- Physics-driven IK (ragdoll, spring chains) — game code bridges DiaRig2D + DiaRigidBody2D
- 3D IK — future DiaIK3D
- Visual debug rendering of chains and targets — future DiaIK2DVisualDebugger or DiaRig2DVisualDebugger
- Editor visualisation of IK chains — future DiaRig2DEditor / DiaIK2DEditor
- `IKComponent` entity integration — deferred to v2; use `IKSolver` directly in phases/modules for v1
- Multi-target IK (simultaneous foot + hand placement with body adjustment) — future feature spec
- Bone stretching (elongating bones to reach far targets) — deferred
- Cross-skeleton IK targets (pin hand to another skeleton's bone) — deferred

---

## Public Interfaces

### JointLimitDef

```cpp
namespace Dia::IK2D {
    struct JointLimitDef {
        float minAngle = -Dia::Maths::kPi;  // Radians; local rotation lower bound
        float maxAngle =  Dia::Maths::kPi;  // Radians; local rotation upper bound
        bool  enabled  = false;
    };
}
```

### IKChainDef

```cpp
namespace Dia::IK2D {
    struct IKChainDef {
        Dia::Core::StringCRC id;
        Dia::Core::StringCRC startBoneId;   // Resolved to index at registration
        Dia::Core::StringCRC endBoneId;     // Resolved to index at registration
        float reachWeight  = 1.0f;          // [0,1]: 0 = pure FK, 1 = full IK
        int   maxIterations = 20;           // FABRIK only; ignored by two-bone
        float tolerance    = 0.001f;        // FABRIK convergence distance (world units)
        // Per-joint angle limits indexed from startBone to endBone (inclusive).
        // Empty = no limits. Partial arrays apply limits to the first N bones.
        Dia::Core::DynamicArrayC<JointLimitDef> jointLimits;
    };
}
```

### PoleVector

```cpp
namespace Dia::IK2D {
    struct PoleVector {
        Dia::Maths::Vector2D direction;  // World-space unit vector; bend knee/elbow toward this
        float weight = 1.0f;            // [0,1] influence
    };
}
```

### IKSolver

```cpp
namespace Dia::IK2D {
    class IKSolver {
    public:
        explicit IKSolver(Dia::Rig2D::Skeleton& skeleton, Dia::Rig2D::Pose& pose);

        // Must be called once per frame before any Solve* calls.
        // Stores the root transform and re-runs FK to refresh world-space bone positions.
        void SetRootTransform(const Dia::Rig2D::BoneTransform& rootTransform);

        // Chain registration — resolves bone IDs to indices at registration time.
        // DIA_ASSERT if startBoneId or endBoneId not found in skeleton.
        void RegisterChain(const IKChainDef& def);
        void UnregisterChain(Dia::Core::StringCRC chainId);
        bool HasChain(Dia::Core::StringCRC chainId) const;

        // Analytic two-bone solver. Chain must have exactly 2 joints (3 bones).
        // poleVector may be nullptr (falls back to current bone orientation).
        // Returns false if chain not found or bone count != 2 joints.
        bool SolveTwoBone(Dia::Core::StringCRC chainId,
                          const Dia::Maths::Vector2D& target,
                          const PoleVector* poleVector = nullptr);

        // Iterative FABRIK solver. Chain may have any number of joints (>= 1).
        // Returns false if chain not found. Returns true even if not converged
        // (best-effort result with DIA_LOG_WARNING on non-convergence).
        bool SolveFABRIK(Dia::Core::StringCRC chainId,
                         const Dia::Maths::Vector2D& target);

        // Single-bone look-at. Rotates boneId so its forward axis points at target.
        // axisAngleOffset: correction if the bone's forward is not +X (radians).
        // Returns false if boneId not found in skeleton.
        bool SolveLookAt(Dia::Core::StringCRC boneId,
                         const Dia::Maths::Vector2D& target,
                         float weight           = 1.0f,
                         float axisAngleOffset  = 0.0f);
        bool SolveLookAt(int boneIndex,
                         const Dia::Maths::Vector2D& target,
                         float weight           = 1.0f,
                         float axisAngleOffset  = 0.0f);

        // Access the skeleton and pose this solver was created with.
        const Dia::Rig2D::Skeleton& GetSkeleton() const;
        const Dia::Rig2D::Pose&     GetPose() const;
    };
}
```

---

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| IK Chain Definition | `IKChainDef`, `JointLimitDef`, `PoleVector` data types. Chain registration and bone ID→index resolution in `IKSolver`. | [ik-chain-definition.md](../../features/dia/diaik2d/ik-chain-definition.md) | Approved |
| Two-Bone Solver | Analytic closed-form solver for exactly 2-joint chains. Pole vector bend control. Reach weight. Joint angle limits. FK propagation on modified range. | [two-bone-solver.md](../../features/dia/diaik2d/two-bone-solver.md) | Approved |
| FABRIK Solver | Iterative N-joint chain solver. Convergence tolerance, max iterations, per-joint angle limit projection per pass. Root bone pinned. Reach weight. FK propagation. | [fabrik-solver.md](../../features/dia/diaik2d/fabrik-solver.md) | Approved |
| Look-At Constraint | Single-bone rotation toward a 2D world-space target. Axis angle offset. Reach weight blend. FK propagation on modified bone. | [look-at-constraint.md](../../features/dia/diaik2d/look-at-constraint.md) | Approved |
| IK Solver | `IKSolver` coordinator: wraps `Skeleton&` + `Pose&`, manages named registered chains, dispatches to the three solvers, triggers FK propagation after each solve, `Rig2D`-channel logging. | [ik-solver.md](../../features/dia/diaik2d/ik-solver.md) | Approved |
| Test Utilities | `DiaIK2D/Testing/`: `AssertBoneRotation`, `AssertEndEffectorPosition`, `AssertBoneUnchanged`, `BuildLimbSkeleton`, `BuildTestIKSolver`. Ships with library; consumer opt-in via include. | [test-utilities.md](../../features/dia/diaik2d/test-utilities.md) | Approved |

---

## Dependencies on Other Systems

**Required:**
- **DiaRig2D** — `Skeleton` (bone array, FK propagation), `Bone` (local transform read/write), `SkeletonDef` (for test utilities)
- **DiaMaths** — `Vector2D`, `Angle`, trigonometric utilities for solver geometry
- **DiaCore** — `StringCRC` (chain/bone IDs), `DynamicArrayC` (chain storage, joint limits), `DIA_ASSERT`
- **DiaLogger** — `Rig2D` channel for debug warnings

**Explicitly excluded:**
- **DiaStateMachine** — no dependency; IK is a pure math/transform operation
- **DiaGraphics** — no rendering; visual debug belongs to a future debugger system
- **DiaApplicationFlow** — no dependency; callers invoke `IKSolver` from within their phases/modules

**Dependents:**
- Game code (CluicheTest and future games) — creates `IKSolver`, registers chains, calls solve each frame after FK
- Future DiaAnimation2D — will drive IK targets from animation clips and blend trees
- Future DiaRig2DEditor / DiaIK2DEditor — uses IKSolver to preview IK in the editor

---

## Out of Scope

- 3D IK — future DiaIK3D
- Ragdoll / physics-driven IK — game code bridges DiaRig2D + DiaRigidBody2D
- Multi-target full-body IK (FBIK) — future feature spec
- Bone stretching (elongating to reach far targets) — deferred
- Cross-skeleton targets (pin to another skeleton's bone) — deferred
- Visual debug rendering — future DiaIK2DVisualDebugger
- Deep history / PBD spring chains — out of scope
- CCD solver — FABRIK covers the same use cases with better stability

---

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Post-process architecture: IK reads/writes `Skeleton` bone transforms in-place | DiaRig2D owns all joint data; IK is stateless between frames. Clean ownership — no duplicated bone arrays. Consistent with how DiaRig2D's FK step works. | All features | Accepted | Yes |
| SD-002 | `IKSolver` triggers FK propagation on the modified bone range after each solve call | IK modifies local bone rotations; world-space transforms must be recomputed before the next system reads them. Propagating only the affected range (start → end bone) avoids full-skeleton re-FK. | IK Solver | Accepted | Yes |
| SD-003 | Reach weight per chain blends linearly between pre-solve FK pose and IK result | Allows gradual IK influence (0 = pure FK, 1 = full IK). Caller snapshots local rotations before solve; weight lerps back after. Cost is proportional to chain length — negligible. | All solvers | Accepted | Yes |
| SD-004 | Pole vector is an optional per-solve-call input, not persisted state | Pole vectors typically come from game code each frame (e.g. a world-space point). Making it a per-call argument avoids a redundant data copy and matches Unity/Unreal convention. | Two-Bone Solver | Accepted | Yes |
| SD-005 | Joint limits applied during solver iterations, not as a post-process clamp | Post-process clamping breaks the IK solution (clamping after convergence produces incorrect end-effector position). Per-iteration projection gives the solver a chance to compensate. | Two-Bone Solver, FABRIK Solver | Accepted | Yes |
| SD-006 | FABRIK over CCD for multi-joint chains | FABRIK converges in fewer iterations and produces more natural-looking results, especially for long chains. CCD can exhibit base-heavy twisting. FABRIK is now the standard in game engines. | FABRIK Solver | Accepted | Yes |
| SD-007 | Two-bone solver handles unreachable targets by fully extending the chain | When target distance > chain length, the solver aligns all bones toward the target (maximum reach). No error — this is correct and expected behavior for limbs reaching past their range. | Two-Bone Solver | Accepted | Yes |
| SD-008 | `IKSolver` wraps a single `Skeleton&`, non-owning | Lifetime managed by the caller. One `IKSolver` per skeleton instance. Multiple solvers per skeleton are possible but the caller is responsible for ordering. Consistent with `SoftBodyWorld` non-owning world pointer pattern. | IK Solver | Accepted | Yes |
| SD-009 | Bone IDs resolved to indices at `RegisterChain` time, not at solve time | Avoid per-frame string lookup / hash cost in hot path. Registration is infrequent (setup); solve is per-frame. DIA_ASSERT at registration if bone not found (programming error). | IK Chain Definition | Accepted | Yes |
| SD-010 | No STL in public APIs | Consistent with PD-004 / AD-002. | All features | Accepted | Yes |
| SD-011 | Test utilities ship inside `DiaIK2D/Testing/` | Platform-wide pattern (DiaStateMachine, DiaSoftBody2D). Consumers opt in via include. Not compiled into the main library. | Test Utilities | Accepted | Yes |
| SD-012 | `SetRootTransform()` called once per frame before all solve calls | IK targets are world-space; solvers need bone world positions. The root transform maps the skeleton from local to world space. Calling once per batch is cheaper than passing it per-call and avoids multiple redundant FK re-runs. Caller contract: always call `SetRootTransform` before the first `Solve*` each frame. | IK Solver | Accepted | Yes |
| SD-013 | `IKSolver` reads/writes `Pose` local rotations, not `Skeleton` bone definitions | `Skeleton` is the immutable definition (bind pose, hierarchy). `Pose` is the mutable per-frame snapshot. IK always modifies `Pose::GetLocalTransform(i).rotation`, never `Skeleton` data. | All solvers | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

---

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Chain IDs, bone IDs, and any component key in `IKChainDef` use `StringCRC`. |
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC` for `jointLimits` and any collection parameters. |
| PD-005 | Platform | x64 only | `DiaIK2D.vcxproj` targets x64 exclusively; no Win32 configurations. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaIK2D.vcxproj` and `.vcxproj.filters` created and manually maintained; all files explicitly listed. |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20`. |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir/toolchain | `DiaIK2D.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.ik2d.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::IK2D::` namespace. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Post-process | Should `IKSolver` re-run FK after each chain solve, or leave it to the caller? | After each `Solve*` call, `IKSolver` re-propagates FK on the modified bone range only (start bone to end bone). This ensures subsequent solve calls within the same frame see correct world-space transforms. Caller is not required to manually invoke FK between chained solve calls. |
| 2 | Two-Bone | How does the two-bone analytic solve handle an unreachable target? | When target distance ≥ chain length, all bones are aligned toward the target (full extension). This is SD-007. The solver still applies the pole vector influence and reach weight normally — the result is a fully extended limb pointing at the target. |
| 3 | FABRIK | Is the root bone pinned or allowed to move? | Root bone is pinned (its world-space position is fixed). FABRIK's backward pass starts from the end effector reaching toward the target; the forward pass starts from the fixed root. This is standard FABRIK — allowing the root to drift would break character grounding. |
| 4 | Joint Limits | Should limits be enforced during FABRIK iteration or as a post-process clamp? | During iteration (SD-005). Post-process clamping is incorrect — it displaces the end effector after convergence. Per-iteration projection lets the solver work within the constrained space. The accuracy penalty is minor for typical joint limit ranges. |
| 5 | Chain overlap | Can the same bone appear in multiple registered chains? | Yes — e.g. a spine chain (pelvis→chest) and an arm chain (shoulder→wrist) share the shoulder bone. Solve ordering is the caller's responsibility. `IKSolver` processes each chain independently and propagates FK after each. The last-solved chain wins for overlapping bones. |
| 6 | Reach weight | What exactly does reach weight blend between? | Pre-solve local bone rotations (captured immediately before the solve) and post-solve local bone rotations, per bone in the chain. A weight of 0 = original FK pose, 1 = full IK result. The blend is applied per bone using linear interpolation on the rotation angle. |
| 7 | Look-At | Should Look-At support an axis angle offset? | Yes (SD-011 in interface). The offset corrects for bones whose bind-pose forward direction is not +X. For example, a bone drawn pointing up (+Y) needs `axisAngleOffset = -π/2`. This is specified per-call; no stored convention. |
| 8 | Debug | Should `IKSolver` expose the last solve's actual end-effector position for debug visualization? | Not in v1. Debug visualization belongs to a future DiaIK2DVisualDebugger. Callers can query the end bone's world-space position directly from the Skeleton after a solve if needed. |
| 9 | Two-bone joint count | What if the caller registers a chain with != 2 joints and calls `SolveTwoBone`? | `SolveTwoBone` returns `false` and emits a `DIA_ASSERT` in debug. The error message names the chain ID and actual joint count. Two-bone is a hard requirement of the analytic formula — using it on the wrong chain is a programming error. |
| 10 | FABRIK reach weight | Does reach weight interact correctly with joint limits? | Yes — the pre-solve snapshot is taken before limits are applied. The blended result respects limits because the IK result (the blend source) was produced under limit constraints. The FK result (the other blend source) may or may not respect limits; that is the caller's concern. |

---

## Status

`Done` — Implemented 2026-05-02. All 6 features complete. 33/33 unit tests pass (Debug + Release x64).

**Plan:** [diaik2d.plan.md](diaik2d.plan.md)

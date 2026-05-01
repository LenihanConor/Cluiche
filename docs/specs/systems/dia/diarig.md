# System Spec: DiaRig

## Parent Application
@docs/specs/applications/dia.md

**Status:** `Draft`

---

## Purpose

DiaRig is the 2D skeletal rig system for the Dia engine. It provides the data layer for bone-based character animation: skeleton definitions (bones in a flat parent-child hierarchy), forward kinematics (computing world-space transforms from local bone transforms), and pose representation (snapshots of bone transforms with basic blending).

DiaRig sits at the bottom of the animation stack:

```
DiaAnimation (future) — clips, playback, blend trees, animation state machine
DiaIK                 — solvers that produce/modify poses on a skeleton
DiaRig                — skeleton definition, forward kinematics, pose representation
```

DiaRig is a pure data + transform system with no physics coupling. Systems that need physics-driven skeletons (ragdoll, soft body skinning) depend on both DiaRig and the relevant physics system — DiaRig itself has no knowledge of physics.

**Dependency chain:**
`DiaRig -> DiaMaths -> DiaCore`
`DiaRig -> DiaLogger` (structured logging)

## Responsibilities

- Define a `Bone` data type: local transform (position, rotation, scale), parent index, StringCRC name/ID
- Maintain a `Skeleton` as a flat array of bones in topological order (parent always precedes child)
- Compute forward kinematics: propagate local bone transforms to world-space transforms given a root transform
- Define a `Pose` as a snapshot of bone local transforms for a given skeleton, with basic operations (lerp between two poses)
- Provide a `SkeletonDef` for programmatic skeleton construction and a JSON loader for data-driven skeleton definitions
- Provide a `SkeletonComponent` (IComponent) so skeletons can attach to game entities via the component system
- Emit structured debug logs to the `Rig` DiaLogger channel (debug builds only): skeleton validation warnings, bone lookup misses
- Provide a `dia.rig.architecture.module.md` YAML module documentation file
- Provide a `DiaRig.vcxproj` static library project registered in `Cluiche.sln`

## Non-Responsibilities

- Animation clips, keyframe interpolation, playback timing, blend trees — future DiaAnimation
- Animation state machines — DiaStateMachine (consumed by future DiaAnimation)
- Inverse kinematics solvers — DiaIK
- Physics simulation, ragdoll — DiaRigidBody2D (game code bridges DiaRig + DiaRigidBody2D)
- Soft body skinning — DiaSoftBody2D (game code bridges DiaRig + DiaSoftBody2D)
- Mesh deformation / skinned mesh rendering — DiaGraphics concern
- 3D skeletons — future DiaRig3D
- Skeleton editing UI — future DiaRigEditor
- Application scheduling — game code calls rig APIs; ProcessingUnit/Phase integration is the caller's concern

## Public Interfaces

### Bone

```cpp
namespace Dia::Rig {
    struct Bone {
        Dia::Core::StringCRC    name;
        int                     parentIndex;    // -1 = root bone
        Dia::Maths::Vector2D    localPosition;
        float                   localRotation;  // Radians
        Dia::Maths::Vector2D    localScale;     // Default {1, 1}
    };
}
```

### Skeleton

```cpp
namespace Dia::Rig {
    struct SkeletonDef {
        Dia::Core::StringCRC                        id;
        Dia::Core::Containers::DynamicArrayC<Bone>  bones;  // Topological order: parent index < own index
    };

    class Skeleton {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"Skeleton"};

        explicit Skeleton(const SkeletonDef& def);

        int                     GetBoneCount() const;
        const Bone&             GetBone(int index) const;
        int                     FindBoneIndex(const Dia::Core::StringCRC& name) const;  // -1 if not found
        const Dia::Core::StringCRC& GetId() const;

        bool                    IsValid() const;  // Validates topological ordering + parent indices
    };
}
```

### Pose

```cpp
namespace Dia::Rig {
    struct BoneTransform {
        Dia::Maths::Vector2D    position;
        float                   rotation;       // Radians
        Dia::Maths::Vector2D    scale;          // Default {1, 1}
    };

    class Pose {
    public:
        explicit Pose(const Skeleton& skeleton);  // Initialised to skeleton's bind pose

        int                     GetBoneCount() const;
        BoneTransform&          GetLocalTransform(int boneIndex);
        const BoneTransform&    GetLocalTransform(int boneIndex) const;

        void SetToBindPose(const Skeleton& skeleton);

        // Forward kinematics: compute world-space transforms from local transforms
        void ComputeWorldTransforms(
            const Skeleton& skeleton,
            const BoneTransform& rootTransform,
            Dia::Core::Containers::DynamicArrayC<BoneTransform>& outWorldTransforms
        ) const;
    };

    // Blend: result = lerp(a, b, t) per bone
    void BlendPoses(const Pose& a, const Pose& b, float t, Pose& outResult);
}
```

### SkeletonComponent

```cpp
namespace Dia::Rig {
    class SkeletonComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"SkeletonComponent"};

        const Skeleton& GetSkeleton() const;
        Pose&           GetCurrentPose();
        const Pose&     GetCurrentPose() const;
    };
}
```

### JSON Skeleton Format

```json
{
    "id": "player_skeleton",
    "bones": [
        { "name": "root",       "parent": -1,     "position": [0, 0],   "rotation": 0, "scale": [1, 1] },
        { "name": "spine",      "parent": "root",  "position": [0, 1],   "rotation": 0, "scale": [1, 1] },
        { "name": "left_arm",   "parent": "spine",  "position": [-0.5, 0], "rotation": 0, "scale": [1, 1] },
        { "name": "right_arm",  "parent": "spine",  "position": [0.5, 0],  "rotation": 0, "scale": [1, 1] }
    ]
}
```

Parent references use bone names (resolved to indices on load) or integer indices.

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Flat Skeleton | Bone data type, Skeleton class with flat bone array in topological order, SkeletonDef for construction, validation (parent < child invariant). | [flat-skeleton.md](../../features/dia/diarig/flat-skeleton.md) | Draft |
| Pose & Pose Blending | Pose class (array of BoneTransforms), bind pose initialization, forward kinematics (local-to-world propagation), BlendPoses() lerp. | [pose-blending.md](../../features/dia/diarig/pose-blending.md) | Draft |
| JSON Skeleton Definitions | Load/save SkeletonDef from JSON using DiaCore/Json. Parent references by name or index. Validation on load. | [json-skeleton-definitions.md](../../features/dia/diarig/json-skeleton-definitions.md) | Draft |
| Skeleton Component | IComponent wrapper for Skeleton + Pose. Factory registration with ComponentFactoryRegistry. StringCRC kUniqueId. | [skeleton-component.md](../../features/dia/diarig/skeleton-component.md) | Draft |
| Skeleton Debug Renderer | Separate .vcxproj bridging DiaRig + DiaGraphics. Draws bones as lines, joints as circles, parent-child hierarchy, bone names. On/off toggle. | [skeleton-debug-renderer.md](../../features/dia/diarig/skeleton-debug-renderer.md) | Draft |
| Test Utilities | Test helpers in `DiaRig/Testing/`: skeleton builders, pose comparison helpers, JSON test fixtures. | [test-utilities.md](../../features/dia/diarig/test-utilities.md) | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaMaths** — Vector2D, affine transform math, angle utilities
- **DiaCore** — StringCRC, DynamicArrayC, DIA_ASSERT, IComponent, ComponentFactoryRegistry, Json
- **DiaLogger** — Structured logging to `Rig` channel

**Dependents (consume DiaRig):**
- **DiaIK** — operates on Skeleton + Pose to solve IK chains
- **DiaAnimation** (future) — plays animation clips that produce Pose snapshots
- **DiaRigDebugRenderer** (Skeleton Debug Renderer feature) — separate .vcxproj bridging DiaRig + DiaGraphics
- **Game code** — creates skeletons, evaluates poses, attaches SkeletonComponent to entities

## Out of Scope

- Animation clips, playback, blend trees — future DiaAnimation
- Inverse kinematics — DiaIK (separate system)
- 3D skeletons / quaternion rotations — future DiaRig3D
- Physics-driven skeleton (ragdoll) — game code bridges DiaRig + DiaRigidBody2D
- Skinned mesh deformation — DiaGraphics concern
- Bone constraints (twist limits, hinge limits) — DiaIK or future constraint system
- Skeleton editing UI — future DiaRigEditor
- Skeleton instancing / shared skeleton data optimization — deferred
- Additive pose blending, multi-layer pose blending — future DiaAnimation
- Skeleton serialization to binary formats — deferred

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| RD-001 | Flat bone array in topological order (parent index < child index) | Cache-friendly iteration; forward kinematics is a single forward pass; no tree traversal needed; same pattern used by industry (Ozz-animation, ACM GDC talks) | All features | Accepted | Yes |
| RD-002 | 2D only: scalar rotation (radians), Vector2D position/scale | Consistent with DiaGeometry2D, DiaRigidBody2D, DiaSoftBody2D; avoids quaternion complexity; 3D is a separate future system | All features | Accepted | Yes |
| RD-003 | Pose is a separate object from Skeleton | Skeleton is the immutable definition (bind pose, hierarchy); Pose is a mutable snapshot. Multiple poses can reference one skeleton (current pose, blend target, IK result). Matches industry standard separation. | Pose & Pose Blending | Accepted | Yes |
| RD-004 | Bone lookup by StringCRC name, not string | Consistent with PD-001; O(n) scan of flat array is fast for typical bone counts (< 100); no hash table needed | All features | Accepted | Yes |
| RD-005 | No STL in public API | Consistent with PD-004 / AD-002 | All features | Accepted | Yes |
| RD-006 | DiaRig has no physics dependencies | Rig is pure data + transforms. Ragdoll, soft body skinning, and physics coupling are bridge concerns owned by game code or future bridge systems. Keeps DiaRig's dependency footprint minimal. | All features | Accepted | Yes |
| RD-007 | Skeleton Debug Renderer is a separate .vcxproj | Same pattern as DiaRigidBody2DVisualDebugger and DiaSoftBody2D Visual Debugger — avoids coupling DiaRig to DiaGraphics | Skeleton Debug Renderer | Accepted | Yes |
| RD-008 | JSON skeleton definitions use bone names for parent references | Human-readable; name-to-index resolution happens once at load time; integer indices also accepted for tool-generated files | JSON Skeleton Definitions | Accepted | Yes |
| RD-009 | Forward kinematics outputs to a caller-provided array | No internal world-transform cache in Pose; caller controls when FK runs and owns the output buffer. Avoids stale-cache bugs and keeps Pose lightweight. | Pose & Pose Blending | Accepted | Yes |

**Status values:** `Proposed` . `Accepted` . `Rejected` . `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system . `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Bone names, Skeleton ID, SkeletonComponent kUniqueId all use StringCRC |
| PD-003 | Platform | Component-based entities (IComponent/IComponentObject) | SkeletonComponent implements IComponent; registered with ComponentFactoryRegistry |
| PD-004 | Platform | No STL containers in public APIs | DynamicArrayC for bone arrays, pose transforms, world transform output |
| PD-005 | Platform | x64 only | DiaRig.vcxproj targets x64 exclusively; no Win32 configurations |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaRig.vcxproj and .vcxproj.filters created and manually maintained; all files explicitly listed |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | DiaRig.vcxproj must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | Any generated rig output (if applicable) goes under `Cluiche/out/` |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.rig.architecture.module.md` with public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Rig::` namespace |
| AD-005 | Dia App | Component-based entities (IComponent/IComponentObject) | Reinforces PD-003; SkeletonComponent uses component system |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Bone data | Should bones store a full 2D affine matrix instead of decomposed position/rotation/scale? | No — decomposed form is better for blending (lerp position, slerp angle, lerp scale independently). Full matrix can be composed on demand during FK. Decomposed is also more intuitive for authoring and debugging. |
| 2 | FK output | `ComputeWorldTransforms` takes a caller-provided array. Should Pose cache world transforms internally instead? | No — caching creates stale-data bugs when local transforms change without re-running FK. Caller-owned output makes the cost explicit and avoids hidden state. DiaIK and DiaAnimation both need to run FK at specific points in their pipelines. |
| 3 | Bone count limits | Should SkeletonDef validate maximum bone count at construction? | Yes — DIA_ASSERT in Skeleton constructor. Max 256 bones (generous for 2D; typical 2D skeletons have 20-50 bones). Exceeding is a programming error, not a runtime condition. |
| 4 | Pose compatibility | What happens if a Pose is used with a different Skeleton than it was created from? | DIA_ASSERT that bone counts match. Pose stores bone count at construction. Cross-skeleton pose application (retargeting) is out of scope — that's a future DiaAnimation concern. |
| 5 | BlendPoses rotation wrapping | Lerping rotation angles naively wraps incorrectly across the -pi/+pi boundary. How is this handled? | Use shortest-arc angle lerp: normalize the angle difference to [-pi, pi] before interpolating. This is a single `atan2(sin(diff), cos(diff))` or equivalent DiaMaths utility. Document in the Pose & Pose Blending feature spec. |
| 6 | FindBoneIndex performance | O(n) linear scan for bone lookup by name — is this acceptable? | Yes for typical 2D skeleton sizes (< 100 bones). Callers that need repeated lookups (IK chains, animation channels) should cache the index at setup time. Document this guidance in the API. |
| 7 | JSON parent references | Allowing both name strings and integer indices for parent references adds complexity. Is this worth it? | Yes — names are human-readable for hand-authored files; integer indices are efficient for tool-generated files. Resolution happens once at load time so the runtime cost is zero. Validate that referenced names exist; DIA_ASSERT on missing parents. |
| 8 | Debug renderer separation | The debug renderer is a separate .vcxproj. Should it be a separate system spec instead of a feature? | No — it follows the same pattern as DiaSoftBody2D's visual debugger (a feature within the system spec). It's a thin rendering bridge, not an independent system with its own lifecycle. Separate .vcxproj avoids DiaRig depending on DiaGraphics. |
| 9 | Scale in 2D bones | 2D skeletons rarely use non-uniform scale. Should scale be omitted to simplify? | Keep it — scale costs 2 floats per bone and enables squash-and-stretch animation, which is a core 2D animation technique. Default to {1,1} so it's zero-cost when unused. |
| 10 | SkeletonComponent ownership | Does SkeletonComponent own the Skeleton, or reference a shared one? | SkeletonComponent owns its Skeleton instance and its current Pose. Skeleton data is small (< 256 bones x ~32 bytes = ~8KB). Shared skeletons (instancing) are a future optimization — premature for v1. |

# System Spec: DiaRig2D

## Parent Application
@docs/specs/applications/dia.md

**Status:** `Approved`

---

## Purpose

DiaRig2D is the 2D skeletal rig system for the Dia engine. It provides the data layer for bone-based character animation: skeleton definitions (bones in a flat parent-child hierarchy), forward kinematics (computing world-space transforms from local bone transforms), and pose representation (snapshots of bone transforms with basic blending).

DiaRig2D sits at the bottom of the animation stack:

```
DiaAnimation2D (future) — clips, playback, blend trees, animation state machine
DiaIK2D                 — solvers that produce/modify poses on a skeleton
DiaRig2D                — skeleton definition, forward kinematics, pose representation
```

DiaRig2D is a pure data + transform system with no physics coupling. Systems that need physics-driven skeletons (ragdoll, soft body skinning) depend on both DiaRig2D and the relevant physics system — DiaRig2D itself has no knowledge of physics.

**Dependency chain:**
`DiaRig2D -> DiaMaths -> DiaCore`
`DiaRig2D -> DiaLogger` (structured logging)

## Responsibilities

- Define a `Bone` data type: local transform (position, rotation, scale), parent index, StringCRC name/ID
- Maintain a `Skeleton` as a flat array of bones in topological order (parent always precedes child)
- Compute forward kinematics: propagate local bone transforms to world-space transforms given a root transform
- Define a `Pose` as a snapshot of bone local transforms for a given skeleton, with basic operations (lerp between two poses)
- Provide a `SkeletonDef` for programmatic skeleton construction and a JSON loader for data-driven skeleton definitions
- Provide a `SkeletonComponent` (IComponent) so skeletons can attach to game entities via the component system
- Emit structured debug logs to the `Rig2D` DiaLogger channel (debug builds only): skeleton validation warnings, bone lookup misses
- Provide a `dia.rig2d.architecture.module.md` YAML module documentation file
- Provide a `DiaRig2D.vcxproj` static library project registered in `Cluiche.sln`

## Non-Responsibilities

- Animation clips, keyframe interpolation, playback timing, blend trees — future DiaAnimation2D
- Animation state machines — DiaStateMachine (consumed by future DiaAnimation2D)
- Inverse kinematics solvers — DiaIK2D
- Physics simulation, ragdoll — DiaRigidBody2D (game code bridges DiaRig2D + DiaRigidBody2D)
- Soft body skinning — DiaSoftBody2D (game code bridges DiaRig2D + DiaSoftBody2D)
- Mesh deformation / skinned mesh rendering — DiaGraphics concern
- 3D skeletons — future DiaRig3D
- Skeleton editing UI — future DiaRig2DEditor
- Application scheduling — game code calls rig APIs; ProcessingUnit/Phase integration is the caller's concern

## Public Interfaces

### Bone

```cpp
namespace Dia::Rig2D {
    struct Bone {
        Dia::Core::StringCRC    name;
        int                     parentIndex;    // -1 = root bone
        Dia::Maths::Vector2D    localPosition;
        float                   localRotation;  // Radians
        Dia::Maths::Vector2D    localScale;     // Default {1, 1}
        float                   length;         // Distance to parent in bind pose; 0 for root bones
    };
}
```

### Skeleton

```cpp
namespace Dia::Rig2D {
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
namespace Dia::Rig2D {
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
namespace Dia::Rig2D {
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
| Flat Skeleton | Bone data type, Skeleton class with flat bone array in topological order, SkeletonDef for construction, validation (parent < child invariant). | [flat-skeleton.md](../../features/dia/diarig2d/flat-skeleton.md) | Approved |
| Pose & Pose Blending | Pose class (array of BoneTransforms), bind pose initialization, forward kinematics (local-to-world propagation), BlendPoses() lerp. | [pose-blending.md](../../features/dia/diarig2d/pose-blending.md) | Approved |
| JSON Skeleton Definitions | Load/save SkeletonDef from JSON using DiaCore/Json. Parent references by name or index. Validation on load. | [json-skeleton-definitions.md](../../features/dia/diarig2d/json-skeleton-definitions.md) | Approved |
| Skeleton Component | IComponent wrapper for Skeleton + Pose. Factory registration with ComponentFactoryRegistry. StringCRC kUniqueId. | [skeleton-component.md](../../features/dia/diarig2d/skeleton-component.md) | Approved |
| Skeleton Debug Renderer | Separate .vcxproj bridging DiaRig2D + DiaGraphics. Draws bones as lines, joints as circles, parent-child hierarchy, bone names. On/off toggle. | [skeleton-debug-renderer.md](../../features/dia/diarig2d/skeleton-debug-renderer.md) | Approved |
| Test Utilities | Test helpers in `DiaRig2D/Testing/`: skeleton builders, pose comparison helpers, JSON test fixtures. | [test-utilities.md](../../features/dia/diarig2d/test-utilities.md) | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaMaths** — Vector2D, affine transform math, angle utilities
- **DiaCore** — StringCRC, DynamicArrayC, DIA_ASSERT, IComponent, ComponentFactoryRegistry, Json
- **DiaLogger** — Structured logging to `Rig2D` channel

**Dependents (consume DiaRig2D):**
- **DiaIK2D** — operates on Skeleton + Pose to solve IK chains
- **DiaAnimation2D** (future) — plays animation clips that produce Pose snapshots
- **DiaRig2DDebugRenderer** (Skeleton Debug Renderer feature) — separate .vcxproj bridging DiaRig2D + DiaGraphics
- **Game code** — creates skeletons, evaluates poses, attaches SkeletonComponent to entities

## Out of Scope

- Animation clips, playback, blend trees — future DiaAnimation2D
- Inverse kinematics — DiaIK2D (separate system)
- 3D skeletons / quaternion rotations — future DiaRig3D
- Physics-driven skeleton (ragdoll) — game code bridges DiaRig2D + DiaRigidBody2D
- Skinned mesh deformation — DiaGraphics concern
- Bone constraints (twist limits, hinge limits) — DiaIK2D or future constraint system
- Skeleton editing UI — future DiaRig2DEditor
- Skeleton instancing / shared skeleton data optimization — deferred
- Additive pose blending, multi-layer pose blending — future DiaAnimation2D
- Skeleton serialization to binary formats — deferred
- Root motion extraction — DiaAnimation2D concern (requires animation clip context)
- Pose pool / scratch allocator — future optimization if profiling shows allocation pressure
- Bone metadata / tags (IK end effectors, attachment points, weapon slots) — consumers maintain side tables; future BoneTag feature if pattern proves cumbersome

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| RD-001 | Flat bone array in topological order (parent index < child index) | Cache-friendly iteration; forward kinematics is a single forward pass; no tree traversal needed; same pattern used by industry (Ozz-animation, ACM GDC talks) | All features | Accepted | Yes |
| RD-002 | 2D only: scalar rotation (radians), Vector2D position/scale | Consistent with DiaGeometry2D, DiaRigidBody2D, DiaSoftBody2D; avoids quaternion complexity; 3D is a separate future system | All features | Accepted | Yes |
| RD-003 | Pose is a separate object from Skeleton | Skeleton is the immutable definition (bind pose, hierarchy); Pose is a mutable snapshot. Multiple poses can reference one skeleton (current pose, blend target, IK result). Matches industry standard separation. | Pose & Pose Blending | Accepted | Yes |
| RD-004 | Bone lookup by StringCRC name, not string | Consistent with PD-001; O(n) scan of flat array is fast for typical bone counts (< 100); no hash table needed | All features | Accepted | Yes |
| RD-005 | No STL in public API | Consistent with PD-004 / AD-002 | All features | Accepted | Yes |
| RD-006 | DiaRig2D has no physics dependencies | Rig is pure data + transforms. Ragdoll, soft body skinning, and physics coupling are bridge concerns owned by game code or future bridge systems. Keeps DiaRig2D's dependency footprint minimal. | All features | Accepted | Yes |
| RD-007 | Skeleton Debug Renderer is a separate .vcxproj | Same pattern as DiaRigidBody2DVisualDebugger and DiaSoftBody2D Visual Debugger — avoids coupling DiaRig2D to DiaGraphics | Skeleton Debug Renderer | Accepted | Yes |
| RD-008 | JSON skeleton definitions use bone names for parent references | Human-readable; name-to-index resolution happens once at load time; integer indices also accepted for tool-generated files | JSON Skeleton Definitions | Accepted | Yes |
| RD-009 | Forward kinematics outputs to a caller-provided array | No internal world-transform cache in Pose; caller controls when FK runs and owns the output buffer. Avoids stale-cache bugs and keeps Pose lightweight. | Pose & Pose Blending | Accepted | Yes |
| RD-010 | SRT transform concatenation order (Scale, Rotate, Translate) | Standard 2D convention: scale in local space, rotate, then position relative to parent. `worldChild = worldParent * T * R * S`. Consistent with SFML and standard 2D engine conventions. | All features | Accepted | Yes |
| RD-011 | Mirroring via negative scale, no special mirror flag | Negative X scale on any bone flips rotation handedness of all children. FK must propagate the sign correctly through the hierarchy. Avoids a separate code path for mirrored skeletons. | All features | Accepted | Yes |
| RD-012 | Bone length stored explicitly, computed from bind pose at construction | IK solvers need bone lengths every frame; recomputing from bind pose is wasteful. Computed once during Skeleton construction from parent-child distance. Root bones have length 0. | Flat Skeleton | Accepted | Yes |

**Status values:** `Proposed` . `Accepted` . `Rejected` . `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system . `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Bone names, Skeleton ID, SkeletonComponent kUniqueId all use StringCRC |
| PD-003 | Platform | Component-based entities (IComponent/IComponentObject) | SkeletonComponent implements IComponent; registered with ComponentFactoryRegistry |
| PD-004 | Platform | No STL containers in public APIs | DynamicArrayC for bone arrays, pose transforms, world transform output |
| PD-005 | Platform | x64 only | DiaRig2D.vcxproj targets x64 exclusively; no Win32 configurations |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaRig2D.vcxproj and .vcxproj.filters created and manually maintained; all files explicitly listed |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | DiaRig2D.vcxproj must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | Any generated rig output (if applicable) goes under `Cluiche/out/` |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.rig2d.architecture.module.md` with public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Rig2D::` namespace |
| AD-005 | Dia App | Component-based entities (IComponent/IComponentObject) | Reinforces PD-003; SkeletonComponent uses component system |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Bone data | Should bones store a full 2D affine matrix instead of decomposed position/rotation/scale? | No — decomposed form is better for blending (lerp position, slerp angle, lerp scale independently). Full matrix can be composed on demand during FK. Decomposed is also more intuitive for authoring and debugging. |
| 2 | FK output | `ComputeWorldTransforms` takes a caller-provided array. Should Pose cache world transforms internally instead? | No — caching creates stale-data bugs when local transforms change without re-running FK. Caller-owned output makes the cost explicit and avoids hidden state. DiaIK2D and DiaAnimation2D both need to run FK at specific points in their pipelines. |
| 3 | Bone count limits | Should SkeletonDef validate maximum bone count at construction? | Yes — DIA_ASSERT in Skeleton constructor. Max 256 bones (generous for 2D; typical 2D skeletons have 20-50 bones). Exceeding is a programming error, not a runtime condition. |
| 4 | Pose compatibility | What happens if a Pose is used with a different Skeleton than it was created from? | DIA_ASSERT that bone counts match. Pose stores bone count at construction. Cross-skeleton pose application (retargeting) is out of scope — that's a future DiaAnimation2D concern. |
| 5 | BlendPoses rotation wrapping | Lerping rotation angles naively wraps incorrectly across the -pi/+pi boundary. How is this handled? | Use shortest-arc angle lerp: normalize the angle difference to [-pi, pi] before interpolating. This is a single `atan2(sin(diff), cos(diff))` or equivalent DiaMaths utility. Document in the Pose & Pose Blending feature spec. |
| 6 | FindBoneIndex performance | O(n) linear scan for bone lookup by name — is this acceptable? | Yes for typical 2D skeleton sizes (< 100 bones). Callers that need repeated lookups (IK chains, animation channels) should cache the index at setup time. Document this guidance in the API. |
| 7 | JSON parent references | Allowing both name strings and integer indices for parent references adds complexity. Is this worth it? | Yes — names are human-readable for hand-authored files; integer indices are efficient for tool-generated files. Resolution happens once at load time so the runtime cost is zero. Validate that referenced names exist; DIA_ASSERT on missing parents. |
| 8 | Debug renderer separation | The debug renderer is a separate .vcxproj. Should it be a separate system spec instead of a feature? | No — it follows the same pattern as DiaSoftBody2D's visual debugger (a feature within the system spec). It's a thin rendering bridge, not an independent system with its own lifecycle. Separate .vcxproj avoids DiaRig2D depending on DiaGraphics. |
| 9 | Scale in 2D bones | 2D skeletons rarely use non-uniform scale. Should scale be omitted to simplify? | Keep it — scale costs 2 floats per bone and enables squash-and-stretch animation, which is a core 2D animation technique. Default to {1,1} so it's zero-cost when unused. |
| 10 | SkeletonComponent ownership | Does SkeletonComponent own the Skeleton, or reference a shared one? | SkeletonComponent owns its Skeleton instance and its current Pose. Skeleton data is small (< 256 bones x ~32 bytes = ~8KB). Shared skeletons (instancing) are a future optimization — premature for v1. |
| 11 | Transform order | What is the concatenation order for bone local transforms — TRS or SRT? | SRT (Scale, then Rotate, then Translate). This matches the standard 2D convention: scale the bone in its own space, rotate it, then position it relative to the parent. `ComputeWorldTransforms` composes as `worldChild = worldParent * T * R * S` per bone. This must be documented in the Flat Skeleton and Pose & Pose Blending feature specs and tested explicitly. Getting this wrong silently breaks every downstream consumer. |
| 12 | Character mirroring | 2D games constantly flip characters horizontally. How does DiaRig2D handle mirroring? | Mirroring is achieved by setting negative X scale on the root bone (or any bone). DiaRig2D must handle negative scale correctly in FK: a negative X scale flips the rotation handedness of all children. `ComputeWorldTransforms` must compose the sign through the hierarchy — no special mirror flag needed. Document and test the negative-scale-with-rotation case explicitly in the Flat Skeleton feature spec. |
| 13 | Bone length | IK solvers need bone lengths. Is length stored on Bone, or derived from bind pose? | Store `length` explicitly on Bone, computed once from parent-child distance in the bind pose during Skeleton construction. This avoids DiaIK2D recomputing it every frame. For root bones (parentIndex == -1), length is 0. JSON loader computes length automatically; programmatic SkeletonDef can set it explicitly. Add `float length` to the Bone struct. |
| 14 | Root motion extraction | Should DiaRig2D support extracting root bone translation delta for entity movement? | No — root motion extraction is a DiaAnimation2D concern. It requires knowledge of animation clips (previous frame pose vs current frame pose), which DiaRig2D doesn't have. DiaRig2D exposes the root bone transform in the Pose; DiaAnimation2D can diff it across frames. Document this as out of scope. |
| 15 | Pose allocation patterns | BlendPoses, FK output, and IK all need temporary Pose/array storage. Should DiaRig2D provide a pose pool? | Not in v1. Document the allocation concern in the Pose & Pose Blending feature spec. Callers should pre-allocate Pose objects and reuse them across frames. A `PosePool` or scratch allocator is a future optimization if profiling shows allocation pressure. Add to Out of Scope. |
| 16 | Bone metadata / tags | Should Bone have user-data or tags for IK end effectors, attachment points, weapon slots? | No extensible user-data on Bone in v1. Consumers (DiaIK2D, game code) maintain their own side tables mapping bone indices to metadata. This keeps Bone small and avoids DiaRig2D knowing about its consumers. A future feature spec can add a `BoneTag` system if the side-table pattern proves cumbersome. Document as out of scope. |
| 17 | Multi-skeleton entities | Can an entity have multiple skeletons (rider + horse, body + face)? | An entity can have multiple SkeletonComponents — the component system already supports multiple components of the same type via unique StringCRC IDs. Each SkeletonComponent has its own ID (e.g., `"body_skeleton"`, `"face_skeleton"`). No special multi-skeleton API needed in DiaRig2D. Document this pattern in the Skeleton Component feature spec. |
| 18 | Thread safety of Pose | If game code, IK, and animation all touch Pose, what is the threading contract? | Pose is explicitly single-threaded per instance. No internal locking. The owner of a Pose is responsible for not writing to it from multiple threads simultaneously. This is consistent with the engine's general approach (ProcessingUnit/Phase scheduling controls thread access). Document as a contract in the Pose & Pose Blending feature spec — not enforced at runtime, just documented. |

## Status

`Approved`

**Plan:** [diarig2d.plan.md](diarig2d.plan.md)

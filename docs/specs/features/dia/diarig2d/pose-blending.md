# Feature Spec: Pose & Pose Blending

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarig2d.md | **pose-blending** |

**Status:** `Approved`

---

## Problem Statement

DiaIK2D solvers, a future animation system, and game code all need to manipulate and combine bone transforms on a skeleton. Without a dedicated Pose type, every consumer would store and blend bone transforms differently, leading to incompatible data and duplicated forward-kinematics math.

---

## Solution Overview

Provide `BoneTransform`, `Pose`, and `BlendPoses()` in the `Dia::Rig2D` namespace.

**BoneTransform** is a plain struct: position (Vector2D), rotation (float, radians), scale (Vector2D).

**Pose** is a mutable array of BoneTransforms sized to a specific skeleton's bone count. It can be initialized to the skeleton's bind pose. It provides `ComputeWorldTransforms()` for forward kinematics — a single forward pass over the flat bone array, composing local transforms into world-space using SRT order (RD-010).

**BlendPoses()** is a free function that lerps two poses per-bone with shortest-arc rotation interpolation.

### BoneTransform

```cpp
namespace Dia::Rig2D {
    struct BoneTransform {
        Dia::Maths::Vector2D    position;   // Default {0, 0}
        float                   rotation;   // Radians, default 0
        Dia::Maths::Vector2D    scale;      // Default {1, 1}
    };
}
```

### Pose

```cpp
namespace Dia::Rig2D {
    class Pose {
    public:
        explicit Pose(const Skeleton& skeleton);

        int                     GetBoneCount() const;
        BoneTransform&          GetLocalTransform(int boneIndex);
        const BoneTransform&    GetLocalTransform(int boneIndex) const;

        void SetToBindPose(const Skeleton& skeleton);

        void ComputeWorldTransforms(
            const Skeleton& skeleton,
            const BoneTransform& rootTransform,
            Dia::Core::Containers::DynamicArrayC<BoneTransform>& outWorldTransforms
        ) const;
    };

    void BlendPoses(const Pose& a, const Pose& b, float t, Pose& outResult);
}
```

### Forward Kinematics Algorithm

```
for i = 0 to boneCount-1:
    localMatrix = ComposeMatrix(localTransforms[i])    // S * R * T
    if bone[i].parentIndex == -1:
        worldTransforms[i] = rootMatrix * localMatrix
    else:
        worldTransforms[i] = worldTransforms[parentIndex] * localMatrix
    Decompose worldTransforms[i] -> outWorldTransforms[i]
```

Single forward pass — O(n) — guaranteed correct because bones are in topological order (RD-001).

### BlendPoses Algorithm

```
for i = 0 to boneCount-1:
    result.position = lerp(a.position, b.position, t)
    result.rotation = shortestArcLerp(a.rotation, b.rotation, t)
    result.scale    = lerp(a.scale, b.scale, t)
```

Shortest-arc rotation lerp: normalize angle difference to [-pi, pi] before interpolating (system spec AI Q5).

### Negative Scale / Mirroring (RD-011)

`ComputeWorldTransforms` must propagate negative scale correctly through the hierarchy. When a parent has negative X scale, child rotations are flipped. The matrix composition handles this naturally — no special case needed as long as decomposition extracts the sign from the scale columns.

### Threading Contract

Pose is explicitly single-threaded per instance. No internal locking. The owner is responsible for not writing from multiple threads simultaneously (system spec AI Q18).

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaRig2D/BoneTransform.h` | BoneTransform struct |
| `Dia/DiaRig2D/Pose.h` | Pose class declaration |
| `Dia/DiaRig2D/Pose.cpp` | Pose implementation (FK, SetToBindPose) |
| `Dia/DiaRig2D/BlendPoses.h` | BlendPoses free function declaration |
| `Dia/DiaRig2D/BlendPoses.cpp` | BlendPoses implementation |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `Pose` constructor takes a Skeleton reference and sizes the internal array to bone count | Unit test |
| 2 | `SetToBindPose` copies each bone's local transform from the Skeleton into the Pose | Unit test: set bind pose, verify each BoneTransform matches Bone local values |
| 3 | `GetLocalTransform(index)` returns correct transform; DIA_ASSERT on out-of-bounds | Unit test |
| 4 | `ComputeWorldTransforms` produces correct world-space positions for a simple 3-bone chain (root, child, grandchild) | Unit test: manually compute expected world transforms and compare |
| 5 | `ComputeWorldTransforms` applies root transform correctly (offset + rotation of entire skeleton) | Unit test: apply a non-identity root transform and verify |
| 6 | `ComputeWorldTransforms` handles negative X scale (mirroring) correctly — child rotations flip | Unit test: mirror root, verify child world positions are horizontally flipped |
| 7 | `ComputeWorldTransforms` uses SRT order: scale in local space, rotate, then translate relative to parent | Unit test: non-uniform scale + rotation, verify against manually computed matrix |
| 8 | `ComputeWorldTransforms` DIA_ASSERTs if outWorldTransforms capacity < bone count | Unit test |
| 9 | `ComputeWorldTransforms` DIA_ASSERTs if skeleton bone count != pose bone count | Unit test |
| 10 | `BlendPoses` lerps position and scale linearly | Unit test: blend at t=0, t=0.5, t=1 |
| 11 | `BlendPoses` uses shortest-arc rotation interpolation across the -pi/+pi boundary | Unit test: blend rotation -170deg to +170deg at t=0.5, expect ~180deg (not 0deg) |
| 12 | `BlendPoses` DIA_ASSERTs if bone counts of a, b, and outResult don't match | Unit test |
| 13 | `BlendPoses` clamps t to [0, 1] | Unit test: t < 0 produces a, t > 1 produces b |
| 14 | No STL in public API | Code review |
| 15 | All code in `Dia::Rig2D::` namespace | Code review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Implement `BoneTransform.h` — struct with defaults | Flat Skeleton feature | |
| 2 | Implement `Pose.h` / `Pose.cpp` — constructor, GetLocalTransform, SetToBindPose, ComputeWorldTransforms | 1 | |
| 3 | Implement `BlendPoses.h` / `BlendPoses.cpp` — shortest-arc rotation lerp | 2 | |
| 4 | Unit tests for Pose: bind pose init, FK with chain, root transform, mirroring, SRT order | 2 | |
| 5 | Unit tests for BlendPoses: linear lerp, shortest-arc rotation, boundary clamping, mismatched counts | 3 | |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — Pose doesn't introduce new IDs; uses Skeleton's bone indices |
| PD-004 | Platform | No STL containers in public APIs | Compliant — outWorldTransforms is DynamicArrayC; internal storage is DynamicArrayC |
| PD-007 | Platform | C++20 required | Compliant |
| AD-002 | Dia App | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::Rig2D:: |
| RD-001 | DiaRig2D | Flat bone array in topological order | Compliant — FK relies on topological order for single-pass world transform computation |
| RD-002 | DiaRig2D | 2D only | Compliant — scalar rotation, Vector2D position/scale |
| RD-003 | DiaRig2D | Pose is separate from Skeleton | Compliant — this feature implements that separation |
| RD-005 | DiaRig2D | No STL in public API | Compliant |
| RD-009 | DiaRig2D | FK outputs to caller-provided array | Compliant — ComputeWorldTransforms writes to outWorldTransforms |
| RD-010 | DiaRig2D | SRT transform order | Compliant — FK composes as worldParent * T * R * S per bone |
| RD-011 | DiaRig2D | Mirroring via negative scale | Compliant — FK propagates negative scale through hierarchy via matrix composition |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | FK | Should ComputeWorldTransforms resize outWorldTransforms or require pre-allocation? | Require pre-allocation (capacity >= boneCount). DIA_ASSERT if too small. Avoids hidden allocations in a per-frame hot path. Caller pre-allocates once and reuses. |
| 2 | FK | Matrix composition and decomposition — is there a floating-point precision concern with compose-then-decompose for deeply nested bone chains? | Minimal for 2D with typical bone counts (< 50). Rotation is a single angle so there's no quaternion drift. Scale sign extraction is the only risk — test explicitly with 5+ levels of nesting and negative scale. |
| 3 | BlendPoses | Should BlendPoses support blending more than 2 poses (multi-way blend)? | No — multi-pose blending is an animation concern (blend trees). Two-pose lerp is sufficient for DiaRig2D v1. Future DiaAnimation2D can chain BlendPoses calls or implement N-way blending. |
| 4 | Pose construction | Pose constructor takes a Skeleton reference — does it store the reference? | No — Pose stores only the bone count and the transform array. It does not hold a reference to the Skeleton. Callers pass the Skeleton explicitly to ComputeWorldTransforms and SetToBindPose. This avoids dangling reference bugs. |
| 5 | BoneTransform vs Bone | BoneTransform duplicates position/rotation/scale from Bone. Should Pose just reference Bone data? | No — Pose is mutable (animation, IK modify it per frame); Bone is the immutable bind pose. They have the same fields but different lifecycles. BoneTransform omits name, parentIndex, length because those are skeleton-structural, not pose-varying. |

---

## Open Questions

None — all resolved above.

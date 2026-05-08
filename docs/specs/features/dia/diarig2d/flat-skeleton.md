# Feature Spec: Flat Skeleton

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarig2d.md | **flat-skeleton** |

**Status:** `Approved`

---

## Problem Statement

DiaIK2D, a future DiaAnimation2D system, and game code all need a skeleton data structure to operate on. There is currently no representation of a bone hierarchy in the Dia engine. Without a shared skeleton definition, each consumer would invent its own bone representation, leading to incompatible data formats and duplicated transform math.

---

## Solution Overview

Provide three types — `Bone`, `SkeletonDef`, and `Skeleton` — in the `Dia::Rig2D` namespace.

**Bone** is a plain data struct holding a bone's name (StringCRC), parent index, local transform (position, rotation, scale), and precomputed length (distance to parent in bind pose).

**SkeletonDef** is a construction-time definition: a StringCRC id and a DynamicArrayC of Bones in topological order.

**Skeleton** is an immutable runtime object constructed from a SkeletonDef. It validates the bone hierarchy at construction, computes bone lengths, and provides read-only access to bones plus name-based lookup.

### Bone Data Layout

```cpp
namespace Dia::Rig2D {
    struct Bone {
        Dia::Core::StringCRC    name;
        int                     parentIndex;    // -1 = root bone
        Dia::Maths::Vector2D    localPosition;
        float                   localRotation;  // Radians
        Dia::Maths::Vector2D    localScale;     // Default {1, 1}
        float                   length;         // Distance to parent in bind pose; 0 for root
    };
}
```

### SkeletonDef

```cpp
namespace Dia::Rig2D {
    struct SkeletonDef {
        Dia::Core::StringCRC                        id;
        Dia::Core::Containers::DynamicArrayC<Bone>  bones;
    };
}
```

### Skeleton

```cpp
namespace Dia::Rig2D {
    class Skeleton {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"Skeleton"};
        static constexpr int kMaxBones = 256;

        explicit Skeleton(const SkeletonDef& def);

        int                         GetBoneCount() const;
        const Bone&                 GetBone(int index) const;
        int                         FindBoneIndex(const Dia::Core::StringCRC& name) const;
        const Dia::Core::StringCRC& GetId() const;

        bool                        IsValid() const;
    };
}
```

### Key Behaviours

1. **Topological invariant**: For every bone at index `i`, `parentIndex == -1` (root) or `parentIndex < i`. Validated at construction via DIA_ASSERT.
2. **Immutability**: Skeleton is immutable after construction. Bones cannot be added, removed, or reordered at runtime.
3. **Bone length computation**: During construction, `length` is computed as `|localPosition|` (distance from parent joint). Root bones get length 0. If the caller sets length explicitly in SkeletonDef (non-zero), that value is preserved.
4. **Bind pose**: The local transforms stored in each Bone struct ARE the bind/rest pose. No separate Pose object is stored internally. The Pose feature (pose-blending) provides mutable pose snapshots.
5. **FindBoneIndex**: Linear scan over the flat array comparing StringCRC names. Returns -1 if not found. Logs a `Rig2D` channel debug warning on miss.
6. **Max bone count**: DIA_ASSERT enforces `boneCount <= kMaxBones (256)`.
7. **Single root**: DIA_ASSERT that exactly one bone has `parentIndex == -1`.
8. **Unique names**: DIA_ASSERT that no two bones share the same StringCRC name.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaRig2D/Bone.h` | Bone struct |
| `Dia/DiaRig2D/Skeleton.h` | Skeleton class + SkeletonDef |
| `Dia/DiaRig2D/Skeleton.cpp` | Skeleton implementation (validation, length computation, FindBoneIndex) |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `Bone` struct stores name, parentIndex, localPosition, localRotation, localScale, length | Code review |
| 2 | `SkeletonDef` holds id (StringCRC) and DynamicArrayC of Bones | Code review |
| 3 | `Skeleton` constructor validates topological ordering: parentIndex < own index for all non-root bones | Unit test: construct with out-of-order parent, DIA_ASSERT fires |
| 4 | `Skeleton` constructor validates exactly one root bone (parentIndex == -1) | Unit test: construct with zero roots and with two roots, DIA_ASSERT fires |
| 5 | `Skeleton` constructor validates unique bone names | Unit test: construct with duplicate names, DIA_ASSERT fires |
| 6 | `Skeleton` constructor validates bone count <= 256 | Unit test: construct with 257 bones, DIA_ASSERT fires |
| 7 | `Skeleton` constructor computes bone length from `|localPosition|` for bones where length is 0 in the def | Unit test: construct skeleton, verify root length == 0, child length == distance to parent |
| 8 | `Skeleton` constructor preserves explicitly set non-zero length from SkeletonDef | Unit test: set explicit length in def, verify it survives construction |
| 9 | `GetBoneCount()` returns correct count | Unit test |
| 10 | `GetBone(index)` returns correct bone; DIA_ASSERT on out-of-bounds | Unit test |
| 11 | `FindBoneIndex()` returns correct index for existing bone name | Unit test |
| 12 | `FindBoneIndex()` returns -1 for non-existent bone name | Unit test |
| 13 | `IsValid()` returns true for a properly constructed Skeleton | Unit test |
| 14 | All public API uses Dia containers (DynamicArrayC), no STL | Code review |
| 15 | All code in `Dia::Rig2D::` namespace | Code review |
| 16 | Negative scale on a bone does not break validation (mirroring is legal) | Unit test: construct skeleton with negative X scale, IsValid() returns true |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `Dia/DiaRig2D/` directory, `DiaRig2D.vcxproj`, `.vcxproj.filters`, register in `Cluiche.sln` | - | Static library; follow PD-008 (no OutDir/IntDir overrides) |
| 2 | Create `dia.rig2d.architecture.module.md` YAML frontmatter | 1 | AD-001 compliance |
| 3 | Implement `Bone.h` — Bone struct with default scale {1,1} and length 0 | 1 | |
| 4 | Implement `Skeleton.h` / `Skeleton.cpp` — SkeletonDef, Skeleton class, all validation, length computation, FindBoneIndex | 3 | |
| 5 | Unit tests for Skeleton: valid construction, all DIA_ASSERT failure cases, FindBoneIndex hit/miss, length computation, negative scale | 4 | Tests in `Cluiche/Tests/GoogleTests/Rig/` |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant — Bone.name is StringCRC; Skeleton.kUniqueId is StringCRC; SkeletonDef.id is StringCRC |
| PD-004 | Platform | No STL containers in public APIs | Compliant — SkeletonDef.bones is DynamicArrayC; no std::vector, std::string, etc. |
| PD-005 | Platform | x64 only | Compliant — DiaRig2D.vcxproj targets x64 exclusively |
| PD-006 | Platform | VS project files are source of truth | Compliant — .vcxproj and .vcxproj.filters manually maintained with all files listed |
| PD-007 | Platform | C++20 required | Compliant — compiled under /std:c++20 |
| PD-008 | Platform | Directory.Build.props owns build paths/toolchain | Compliant — DiaRig2D.vcxproj will not override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, LanguageStandard |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — dia.rig2d.architecture.module.md created in Task 2 |
| AD-002 | Dia App | No STL in public APIs | Compliant — reinforces PD-004 |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — all code in Dia::Rig2D:: |
| RD-001 | DiaRig2D | Flat bone array in topological order | Compliant — this feature implements the invariant and validates it |
| RD-002 | DiaRig2D | 2D only: scalar rotation, Vector2D position/scale | Compliant — Bone uses float rotation, Vector2D position/scale |
| RD-004 | DiaRig2D | Bone lookup by StringCRC | Compliant — FindBoneIndex takes StringCRC, linear scan |
| RD-005 | DiaRig2D | No STL in public API | Compliant — reinforces PD-004/AD-002 |
| RD-010 | DiaRig2D | SRT transform order | Compliant — Bone stores decomposed S, R, T; concatenation order is the FK feature's concern but Bone layout supports it |
| RD-011 | DiaRig2D | Mirroring via negative scale | Compliant — Bone.localScale is Vector2D allowing negative components; validation does not reject negative scale |
| RD-012 | DiaRig2D | Bone length stored explicitly | Compliant — Bone.length computed at construction from bind pose; explicit non-zero values preserved |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Validation | Should Skeleton validation reject skeletons with unreachable bones (parentIndex valid but forms a disconnected subtree)? | No — topological ordering + single root + valid parent indices guarantee a connected tree. A bone with parentIndex pointing to a valid earlier bone is by definition reachable from root through the parent chain. No extra check needed. |
| 2 | Bone length | Length is computed as magnitude of localPosition. What about bones at the same position as their parent (length 0)? | Valid — some bones are "virtual" joints at the same position as their parent (e.g., a rotation-only joint). Length 0 is correct for these. DiaIK2D must handle zero-length bones gracefully (skip them in chain length computation). |
| 3 | Single root | Is single root too restrictive? Some skeleton formats allow multiple roots (e.g., separate body + props). | Single root is correct for v1. Multiple independent hierarchies should be separate Skeleton instances (and separate SkeletonComponents on the entity). This keeps FK as a simple single-pass forward traversal. |
| 4 | Immutability | Skeleton is immutable — what about detaching a limb at runtime? | Not a Skeleton mutation. Detaching a limb is a game-code concern: hide the rendering of detached bones, spawn a separate entity for the detached part. The Skeleton definition doesn't change. If true topological mutation is needed (very unlikely in 2D), it would be a future feature spec. |
| 5 | Default Bone values | Should Bone have a constructor that sets sensible defaults? | Yes — Bone should default-initialize: name = empty StringCRC, parentIndex = -1, localPosition = {0,0}, localRotation = 0, localScale = {1,1}, length = 0. This prevents uninitialized data when callers build SkeletonDefs programmatically. |
| 6 | FindBoneIndex logging | FindBoneIndex logs a debug warning on miss. Could this spam logs in legitimate use cases (checking if a bone exists)? | Good catch — split into two patterns: `FindBoneIndex()` returns -1 silently (no log); callers that expect the bone to exist use `GetBoneIndex()` which DIA_ASSERTs on miss. This avoids log spam for optional bone lookups while catching programming errors. Update: keep `FindBoneIndex` (silent, returns -1) and add `GetRequiredBoneIndex` (DIA_ASSERT on miss, logs warning). |

---

## Open Questions

None — all resolved above.

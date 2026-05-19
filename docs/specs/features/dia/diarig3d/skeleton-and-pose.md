# Feature Spec: skeleton-and-pose

## Parent System
@docs/specs/systems/dia/render-backend.md

**Hard dependencies (specced separately):**
- @docs/specs/systems/dia/diamaths.md — `Vector3D`, `Matrix44`, `Quaternion`, `Transform3D`, `Matrix34`
- @docs/specs/systems/dia/diamesh3d.md (this batch's `mesh-asset-and-loader` feature) — glTF skin index linkage

**Pattern reference:** @docs/specs/systems/dia/diarig2d.md (Approved — 2D analogue mirrored verbatim where possible)

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Create a new `Dia/DiaRig3D/` module — the 3D analogue of `DiaRig2D` — that owns the data layer for bone-based 3D character animation. After this feature:

- `Dia::Rig3D::Bone3D` — local transform (Vector3D position, Quaternion rotation, Vector3D scale) + parent index + StringCRC name + bind-pose inverse matrix
- `Dia::Rig3D::Skeleton3D` — flat topologically-sorted array of bones with forward-kinematics computation
- `Dia::Rig3D::Pose3D` — snapshot of bone local transforms, with lerp between poses
- `Dia::Rig3D::SkeletonDef3D` + JSON / glTF loaders for data-driven skeleton construction
- `Dia::Rig3D::SkeletonComponent3D` (IComponent) for entity attachment
- Forward kinematics produces both world-space `Matrix44` array (for FK debugging / IK / rendering) and the `Matrix34` array used by skinning palettes per DiaMaths SD-008

This feature **only owns rig data + FK**. Inverse kinematics is `DiaIK3D` (out of scope per system spec). Animation playback is `DiaAnimation3D`. Skinning evaluation is `DiaSkinning3D`. Rendering is `DiaBgfx`.

## Problem

Phase 2 needs skinned characters. Skinned characters require a skeleton: a parent-child bone hierarchy with local transforms that compose into world transforms via forward kinematics. The result feeds (a) the skinning system (`DiaSkinning3D`) which uses bone world transforms × inverse bind pose to compute the per-bone skinning matrix; (b) the IK system (future); (c) animation systems (`DiaAnimation3D`) which write into a `Pose3D` and ask the skeleton to compute world transforms.

`DiaRig2D` already exists with this exact shape. Mirroring it for 3D keeps the cognitive model symmetric and preserves the established Dia pattern (RB-013).

## Goals

- Mirror `DiaRig2D`'s public API shape with 3D types substituted: `Vector2D` → `Vector3D`, `float rotation` → `Quaternion`, `Matrix33` → `Matrix44`/`Matrix34`
- Define `Bone3D`, `Skeleton3D`, `Pose3D`, `SkeletonDef3D`, `SkeletonComponent3D`
- Forward kinematics: given root transform + local bone transforms, compute world-space transforms (Matrix44 for full transforms, Matrix34 for skinning palette)
- Bind-pose inverse matrices (Matrix44) cached per bone at skeleton construction; needed by skinning
- glTF skin loader: parse `skins[].joints` + `skins[].inverseBindMatrices` from a glTF file (using the cgltf dep added in `mesh-asset-and-loader`); produce a `SkeletonDef3D`
- JSON loader for hand-authored skeleton definitions (matches DiaRig2D's pattern)
- `SkeletonComponent3D` IComponent so skeletons attach to entities
- Rig3D DiaLogger channel for validation warnings (mirrors DiaRig2D's `Rig2D` channel)
- Tests under `Dia/DiaRig3D/Testing/` with a synthetic 3-bone skeleton fixture
- Per-bone joint count cap: `kMaxBones = 256` (compatible with bgfx skinning palette uniform array sizes)

## Non-Goals

- **IK solvers** — `DiaIK3D`, future feature
- **Animation playback** — `DiaAnimation3D`, separate Phase 2 feature
- **Skinning evaluation** — `DiaSkinning3D`, separate Phase 2 feature
- **Mesh deformation rendering** — `diabgfx-3d-renderers`
- **Skeleton editing UI** — future
- **Physics-coupled skeletons (ragdoll)** — DiaRigidBody3D is not part of Phase 2
- **Bone constraints** (look-at, twist, IK chains) — out of scope for the data layer; live in DiaIK3D
- **Per-instance bone scale animation** — supported (Vector3D scale on Bone3D) but most rigs use unit scale; treated as a tag-along feature

## Public Interfaces

### `Dia::Rig3D::Bone3D`

```cpp
// Dia/DiaRig3D/Bone3D.h
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Quaternion/Quaternion.h>
#include <DiaMaths/Matrix/Matrix44.h>

namespace Dia { namespace Rig3D {

struct Bone3D
{
    Dia::Core::StringCRC    name;
    int                     parentIndex;          // -1 = root
    Dia::Maths::Vector3D    localPosition;
    Dia::Maths::Quaternion  localRotation;
    Dia::Maths::Vector3D    localScale;           // default (1,1,1)

    // Cached at skeleton construction; used by skinning to transform mesh-space
    // vertices into bone-local space before reapplying the animated bone's
    // world transform.
    Dia::Maths::Matrix44    inverseBindMatrix;
};

} }
```

### `Dia::Rig3D::Skeleton3D`

```cpp
// Dia/DiaRig3D/Skeleton3D.h
#pragma once

#include "DiaRig3D/Bone3D.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaMaths/Matrix/Matrix34.h>

namespace Dia { namespace Rig3D {

class Pose3D;

class Skeleton3D
{
public:
    static constexpr Dia::Core::StringCRC kUniqueId{"Skeleton3D"};
    static constexpr unsigned int kMaxBones = 256;

    Skeleton3D();

    bool Initialise(const SkeletonDef3D& def);    // see below

    unsigned int                GetBoneCount() const;
    const Bone3D&               GetBone(unsigned int index) const;
    int                         FindBoneIndex(Dia::Core::StringCRC name) const;
    const Dia::Core::Containers::DynamicArrayC<Bone3D, kMaxBones>& GetBones() const;

    // Forward kinematics:
    // Given the skeleton's bind pose (or a Pose3D's local transforms), produce
    // the world-space transform per bone, into the caller-owned output arrays.
    // outWorldMatrices: full Matrix44 per bone (use for IK, debug, picking)
    // outSkinningPalette: Matrix34 per bone — outWorld * inverseBindMatrix —
    //                     the matrix uploaded to GPU for skinning.
    // The two outputs may be requested independently (pass nullptr to skip).
    void ComputeWorldTransforms(
        const Dia::Maths::Matrix44& rootTransform,
        const Pose3D&               pose,
        Dia::Core::Containers::DynamicArrayC<Dia::Maths::Matrix44, kMaxBones>* outWorldMatrices,
        Dia::Core::Containers::DynamicArrayC<Dia::Maths::Matrix34, kMaxBones>* outSkinningPalette
    ) const;

private:
    Dia::Core::Containers::DynamicArrayC<Bone3D, kMaxBones> mBones;
};

} }
```

### `Dia::Rig3D::Pose3D`

```cpp
// Dia/DiaRig3D/Pose3D.h
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Quaternion/Quaternion.h>

namespace Dia { namespace Rig3D {

struct LocalBoneTransform
{
    Dia::Maths::Vector3D   position;
    Dia::Maths::Quaternion rotation;
    Dia::Maths::Vector3D   scale;
};

class Pose3D
{
public:
    static constexpr unsigned int kMaxBones = 256;

    Pose3D();

    void Resize(unsigned int boneCount);
    unsigned int GetBoneCount() const;

    LocalBoneTransform&       GetBone(unsigned int index);
    const LocalBoneTransform& GetBone(unsigned int index) const;

    // In-place blend: this = lerp(a, b, t).
    // Position/scale: linear interpolation.
    // Rotation: Slerp via Quaternion::Slerp (DiaMaths).
    void Lerp(const Pose3D& a, const Pose3D& b, float t);

    // Initialise from a skeleton's bind pose (copies localPosition / localRotation / localScale).
    void InitialiseFromBindPose(const class Skeleton3D& skeleton);

private:
    Dia::Core::Containers::DynamicArrayC<LocalBoneTransform, kMaxBones> mBones;
};

} }
```

### `Dia::Rig3D::SkeletonDef3D`

```cpp
// Dia/DiaRig3D/SkeletonDef3D.h
namespace Dia { namespace Rig3D {

struct SkeletonDef3D
{
    Dia::Core::StringCRC                                    id;
    Dia::Core::Containers::DynamicArrayC<Bone3D, Skeleton3D::kMaxBones>  bones;
};

// Loaders — produce a SkeletonDef3D from disk.
class SkeletonDef3DLoader
{
public:
    // JSON loader (mirrors DiaRig2D's JSON loader).
    static bool LoadFromJson(const Dia::Core::Containers::String512& path,
                             SkeletonDef3D& outDef);

    // glTF loader — parses glTF skin section.
    // Path may point to a .gltf or .glb. The glTF skin index selects which skin
    // (most files have one); pass 0 for "first skin".
    static bool LoadFromGltf(const Dia::Core::Containers::String512& path,
                             unsigned int skinIndex,
                             SkeletonDef3D& outDef);
};

} }
```

### `Dia::Rig3D::SkeletonComponent3D`

```cpp
// Dia/DiaRig3D/SkeletonComponent3D.h
#include <DiaCore/Architecture/Component/IComponent.h>

namespace Dia { namespace Rig3D {

class SkeletonComponent3D : public Dia::Core::IComponent
{
public:
    static constexpr Dia::Core::StringCRC kUniqueId{"SkeletonComponent3D"};

    SkeletonComponent3D();

    bool Initialise(const SkeletonDef3D& def);

    Skeleton3D&        GetSkeleton();
    const Skeleton3D&  GetSkeleton() const;

    Pose3D&            GetCurrentPose();
    const Pose3D&      GetCurrentPose() const;

private:
    Skeleton3D mSkeleton;
    Pose3D     mCurrentPose;   // animation systems write here; renderer reads
};

} }
```

## Implementation

### Files introduced

```
Dia/DiaRig3D/                                 NEW MODULE
├── DiaRig3D.vcxproj                          NEW
├── DiaRig3D.vcxproj.filters                  NEW
├── dia.rig3d.architecture.module.md          NEW
├── Bone3D.h                                  NEW
├── Skeleton3D.h                              NEW
├── Skeleton3D.cpp                            NEW
├── Pose3D.h                                  NEW
├── Pose3D.cpp                                NEW
├── SkeletonDef3D.h                           NEW
├── SkeletonDef3DLoader.h                     NEW
├── SkeletonDef3DLoader.cpp                   NEW (JSON + cgltf-driven glTF)
├── SkeletonComponent3D.h                     NEW
├── SkeletonComponent3D.cpp                   NEW
└── Testing/
    ├── MockSkeleton3D.h                      NEW
    └── Fixtures/
        ├── three_bone_chain.json             NEW (test fixture)
        └── three_bone_chain.gltf             NEW (test fixture)
```

### Files modified

```
Cluiche/Cluiche.sln
   - Add DiaRig3D.vcxproj reference

Dia/DiaLogger/<channel-config>
   - Register Rig3D channel (mirrors Rig2D channel)
```

### Forward kinematics algorithm

Iterates bones in topological order (parent index < own index). For each bone:

```
local = TRS(bone.localPosition, pose.rotation, pose.scale)
if (parentIndex == -1)
    world[i] = rootTransform * local
else
    world[i] = world[parentIndex] * local

if (outSkinningPalette)
    skinning[i] = (world[i] * bone.inverseBindMatrix).ToMatrix34()
```

`Pose3D` provides the per-bone position/rotation/scale; `Bone3D::inverseBindMatrix` is cached at skeleton construction. `world[i]` accumulates parent transforms in topological order.

`Matrix44 → Matrix34` conversion is per-bone; the 3×4 affine drops the projection row (per DiaMaths SD-008). The skinning palette is what GPU shaders consume.

### glTF skin loading

```
1. cgltf_parse_file + cgltf_load_buffers + cgltf_validate (shared with Mesh3DAssetHandler — possibly factor out)
2. Locate cgltf_data->skins[skinIndex].
3. For each cgltf_node in skin.joints:
     bone.name         = StringCRC(node->name)
     bone.parentIndex  = (find parent's index in skin.joints array, or -1 if not present)
     bone.localPosition= node->translation
     bone.localRotation= node->rotation (xyzw → DiaMaths::Quaternion)
     bone.localScale   = node->scale (default (1,1,1))
4. Read skin.inverseBindMatrices (an accessor of mat4 floats); parse 16 floats per joint into Matrix44 (per DiaMaths SD-005 row-major: glTF inverseBindMatrices are column-major float[16], so transpose during read).
5. Topologically sort bones (parent index always < own index). Re-map parent indices accordingly.
6. Validate: max 256 bones; no cycles; each bone has at most one parent.
```

### Pose3D::Lerp

```cpp
for each bone i:
    out.bones[i].position = lerp(a.bones[i].position, b.bones[i].position, t)
    out.bones[i].rotation = Quaternion::Slerp(a.bones[i].rotation, b.bones[i].rotation, t)
    out.bones[i].scale    = lerp(a.bones[i].scale,    b.bones[i].scale,    t)
```

`Quaternion::Slerp` is owned by DiaMaths (per `quaternion.md` feature spec).

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaRig3D/` (entire module) | NEW |
| `Dia/DiaRig3D/DiaRig3D.vcxproj{,.filters}` | NEW |
| `Dia/DiaRig3D/dia.rig3d.architecture.module.md` | NEW |
| `Cluiche/Cluiche.sln` | Add DiaRig3D.vcxproj |
| DiaLogger channel config | Add Rig3D channel |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| **DiaMaths features** (`vector3d-cross`, `matrix44`, `matrix34`, `quaternion`, `transform3d`) | Hard | Skeleton3D is built on these |
| **cgltf** (added by `diamesh3d`'s `deps.json` entry) | Hard | glTF skeleton loader uses cgltf |
| `diamesh3d` | Soft | Mesh assets carry joint indices that index into Skeleton3D bones; both modules link to the same skeleton via shared StringCRC asset id |
| `diaskinning3d` | Reverse | Consumes Skeleton3D's `ComputeWorldTransforms(...)` skinning palette output |
| `diaanimation3d` | Reverse | Writes into a Pose3D each frame |

## Acceptance Criteria

1. `Dia/DiaRig3D/DiaRig3D.vcxproj` builds clean under `/std:c++20`; links DiaCore, DiaMaths
2. `Dia::Rig3D::Bone3D` struct exists with documented fields including `inverseBindMatrix` (Matrix44)
3. `Dia::Rig3D::Skeleton3D` exists with `Initialise(SkeletonDef3D)`, `GetBoneCount`, `GetBone(idx)`, `FindBoneIndex(StringCRC)`, `ComputeWorldTransforms(...)`
4. `Dia::Rig3D::Pose3D` exists with `Lerp(a, b, t)` using Quaternion::Slerp for rotations
5. `Dia::Rig3D::SkeletonDef3DLoader` provides both `LoadFromJson` and `LoadFromGltf` static methods
6. glTF loader correctly transposes `inverseBindMatrices` from glTF column-major to DiaMaths row-major during read
7. Forward kinematics produces correct results for the test fixture (three-bone chain, bone-1 parent of bone-2 parent of bone-3); world transforms verified against hand-computed reference
8. `Pose3D::Lerp` produces correct interpolated results (verified by interpolating between two poses with known endpoint values)
9. `kMaxBones = 256` honoured; loader rejects skeletons exceeding this
10. `SkeletonComponent3D` registers via the standard IComponent factory pattern (mirrors SkeletonComponent in DiaRig2D)
11. `dia.rig3d.architecture.module.md` validates via `python Tools/dia_modules.py --validate`
12. PD-004 audit: no STL containers in any DiaRig3D public header
13. `Dia run googletest` is green; new TestSkeleton3D, TestPose3D, TestSkeletonDef3DLoader suites cover FK, lerp, JSON load, glTF load
14. No bgfx types in DiaRig3D public headers (RB-006); no DiaGraphics dependency (rig data is renderer-agnostic)
15. Logger Rig3D channel emits validation warnings on bone-count exceeded and bone-not-found lookups (debug build only)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | RenderBackend | @docs/specs/systems/dia/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — bone.name is StringCRC; component id is StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC, raw structs, no STL |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | VS project files source of truth | Compliant |
| PD-007 | Platform | C++20 | Compliant |
| AD-001 | Dia App | Module YAML frontmatter | Compliant |
| AD-003 | Dia App | Namespace Dia::<Module>:: | Compliant — `Dia::Rig3D::` |
| AD-005 | Dia App | Component-based entities | Compliant — SkeletonComponent3D is an IComponent |
| **DiaMaths SD-005** | DiaMaths | Row-major matrix layout | Compliant — Matrix44 row-major; transpose only at glTF load and at GPU upload |
| **DiaMaths SD-008** | DiaMaths | Matrix34 affine for transform hierarchies | Compliant — skinning palette uses Matrix34 |
| RB-002 | RenderBackend | Two-phase delivery | Phase 2 |
| RB-003 | RenderBackend | Phase 2 specs Approved, not in progress | Compliant |
| RB-013 | RenderBackend | Phase 2 module decomposition mirrors 2D family | **Compliant — DiaRig3D mirrors DiaRig2D verbatim with 3D types** |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Bone storage | Why `inverseBindMatrix` per bone? Couldn't we compute it on demand? | Cached because skinning needs it every frame; recomputing would inflate per-frame cost. Computed once at skeleton construction. |
| 2 | Topological order | If a glTF skin's joints aren't already topologically sorted, what happens? | Loader sorts them and re-maps parent indices. Validation: parent index < own index after sort. |
| 3 | kMaxBones cap | Is 256 realistic? | Yes for "light 3D" — most skinned characters have 50–150 bones. bgfx skinning uniform arrays comfortably support 256 bones × Matrix34 = 24KB; well under the typical 64KB uniform buffer limit. |
| 4 | Pose3D vs glTF channels | glTF stores animation as channels (per-bone position/rotation/scale curves). Does Pose3D map cleanly? | Yes — DiaAnimation3D samples each channel into the corresponding bone's `LocalBoneTransform`. The Pose3D is the *output* of animation sampling. |
| 5 | Skinning palette format | Matrix34 vs Matrix44 for the GPU skinning palette? | Matrix34 — saves 25% uniform space (24KB vs 32KB at 256 bones). DiaMaths SD-008 specifies Matrix34 for affine transforms; matches bgfx's `bgfx::setUniform` with `UniformType::Mat3` × 4 (or `UniformType::Mat4` with packed 3×4 layout). Final wiring decided in `diaskinning3d`. |
| 6 | Animation playback | Does Skeleton3D know about animation? | No — purely data + FK. DiaAnimation3D writes into `SkeletonComponent3D::GetCurrentPose()`. Same pattern as DiaRig2D / DiaAnimation2D. |
| 7 | Skinning eval location | Where does ComputeWorldTransforms run — sim or render thread? | Sim thread (game tick). Output stored on the SkeletonComponent3D; FrameData carries the skinning palette index → renderer reads the palette via DiaSkinning3D's per-frame buffer. Captured in `diaskinning3d`. |
| 8 | Rig3D editor | Phase 2 visual debugger? | Out of scope. DiaRig3DVisualDebugger is a future system mirroring DiaRig2DVisualDebugger. |
| 9 | Bone constraints | Look-at, twist, IK rest poses? | Out of scope. Live in DiaIK3D when that lands. |
| 10 | Quaternion order | glTF stores quaternions as (x,y,z,w). DiaMaths::Quaternion stores (x,y,z,w). Match? | Yes — both follow the standard convention (DiaMaths spec confirms x,y,z,w). One-to-one mapping. |

---

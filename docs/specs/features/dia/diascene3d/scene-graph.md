# Feature Spec: scene-graph

## Parent System
@docs/specs/systems/dia/render-backend.md

**Hard dependencies:**
- @docs/specs/systems/dia/diamaths.md — `Transform3D`, `Matrix44`
- @docs/specs/systems/dia/diageometry3d.md — `Frustum`, `AABB`, `IntersectionTests::Test(AABB, Frustum)`
- This-batch features: `graphics-3d-types`, `mesh-asset-and-loader`, `skeleton-and-pose`, `clip-and-player`, `skinning-palette`

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Create a new `Dia/DiaScene3D/` module — the 3D-only "scene graph" introduced by RB-014. After this feature:

- `Dia::Scene3D::Scene3D` — a coherent bundle of camera + lights + renderables, populated by game code each frame
- `Dia::Scene3D::SceneRenderable3D` — a single mesh-on-a-skeleton with its own Transform3D, mesh asset id, optional skeleton + animation refs, AABB-derived bounds for culling
- `Dia::Scene3D::Submit(scene, frameData)` — produces a populated `Mesh3DFrameData` from a `Scene3D`, including frustum culling against the camera and skinning-palette index assignment per renderable
- Submission walks the scene's renderables, computes world transform via Transform3D's parent chain, performs frustum-vs-AABB culling, calls `SkinningManager::RegisterSkinned` for skinned renderables, and emits `Mesh3DDrawCommand`s

This is the **producer** side. The consumer (`diabgfx-3d-renderers`) reads `Mesh3DFrameData` and renders. `Scene3D` itself is renderer-agnostic.

## Problem

Per RB-014, 3D needs a scene-graph abstraction that 2D doesn't. Camera + lights + renderables are coherent in 3D — you can't render meshes correctly without a camera, lights without a scene, or skinned characters without a skeleton-pose linkage. Putting these in a single typed container:

- Centralises the per-frame state that every 3D draw needs
- Gives a clean boundary for frustum culling (Frustum derived from camera; queried against renderable AABBs)
- Encapsulates the skinning-palette assignment loop (per renderable: register skinned skeleton, write returned index into draw command)
- Provides the natural seam to add scene-level features later (per-mesh light overrides, layer masks, per-scene fog, etc.) without re-architecting

A scene-graph is a *useful* abstraction here, but **not a hierarchy of nodes**. We don't model a tree of scene nodes — Transform3D already provides the parent-child hierarchy at the transform level. `Scene3D` is a flat container of renderables; their transforms compose via Transform3D's parent pointers. This sidesteps the classic scene-graph pitfalls (deep recursion, cache-unfriendly node walks).

## Goals

- Create `Dia/DiaScene3D/` module with `dia.scene3d.architecture.module.md`
- `Dia::Scene3D::SceneRenderable3D` — mesh + transform + optional skeleton/animation + cached AABB bounds
- `Dia::Scene3D::Scene3D` — flat array of renderables, plus camera + lights
- `Dia::Scene3D::Submit(scene, outFrameData)` — produces a populated `Mesh3DFrameData`:
  - Sets camera (`outFrameData.SetCamera(scene.GetCamera())`)
  - Adds lights (`AddDirectionalLight`, `AddPointLight`)
  - Derives frustum from `camera.view * camera.projection`
  - For each renderable: compute world matrix via Transform3D, transform AABB to world, frustum-test; if visible, call `SkinningManager::RegisterSkinned` if skinned, emit `Mesh3DDrawCommand`
  - Drops counters surfaced via accessors (culled count, dropped due to capacity, etc.)
- `kMaxRenderables = 4096` (matches `Mesh3DFrameData::kMaxMeshDraws`)
- Tests under `Dia/DiaScene3D/Testing/` covering culling, skinning index assignment, transform composition

## Non-Goals

- **Hierarchical scene nodes** — no `SceneNode` class; renderables are a flat list. Transform3D's parent pointer chain is the only hierarchy
- **Octree / BVH3D scene partitioning** — `DiaGeometry3D::SpatialGrid3D` is available but not required for Phase 2; flat iteration is fine for "light 3D" scope (≤4096 renderables)
- **Visibility / layer masks beyond camera frustum** — single camera; no per-camera visibility groups
- **Shadow-caster culling per light** — out of scope; shadows in `diabgfx-3d-renderers` use the directional light's view from the camera
- **Per-mesh light culling** — global light list per scene; shader iterates them
- **LOD selection** — out of scope
- **Streaming** — `Scene3D` is fully resident in memory each frame
- **Multi-scene rendering / split-screen** — single scene per frame
- **Editor scene authoring** — out of scope; scene populated by game code

## Public Interfaces

### `Dia::Scene3D::SceneRenderable3D`

```cpp
// Dia/DiaScene3D/SceneRenderable3D.h
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Transform/Transform3D.h>
#include <DiaGeometry3D/Shapes/AABB.h>

namespace Dia { namespace Rig3D     { class SkeletonComponent3D; } }
namespace Dia { namespace Animation3D { class AnimationComponent3D; } }

namespace Dia { namespace Scene3D {

struct SceneRenderable3D
{
    SceneRenderable3D();

    // Required: which mesh to draw and where it is in the world.
    Dia::Core::StringCRC                meshId;          // resolves to DiaMesh3D::Mesh3DAsset
    Dia::Core::StringCRC                materialId;      // forwarded to draw command
    Dia::Maths::Transform3D*            transform;       // not owned

    // Local-space AABB (in the mesh's authoring frame); transformed at submit time.
    Dia::Geometry3D::AABB               localBounds;

    // Optional skinning. nullptr = static mesh.
    Dia::Rig3D::SkeletonComponent3D*    skeleton;        // not owned

    // Optional sort key.
    int16_t                             layer;
};

} }
```

### `Dia::Scene3D::Scene3D`

```cpp
// Dia/DiaScene3D/Scene3D.h
#pragma once

#include "DiaScene3D/SceneRenderable3D.h"
#include <DiaGraphics/Mesh3D/Camera3D.h>
#include <DiaGraphics/Mesh3D/Light.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Scene3D {

class Scene3D
{
public:
    static constexpr unsigned int kMaxRenderables = 4096;
    static constexpr unsigned int kMaxLights      = 32;

    Scene3D();

    // Per-frame setup — game code calls these to populate the scene.
    void                            Clear();
    void                            SetCamera(const Dia::Graphics::Camera3D& camera);
    void                            AddRenderable(const SceneRenderable3D& r);
    void                            AddDirectionalLight(const Dia::Graphics::DirectionalLight& light);
    void                            AddPointLight(const Dia::Graphics::PointLight& light);

    // Accessors for Submit().
    const Dia::Graphics::Camera3D&  GetCamera() const;
    const Dia::Core::Containers::DynamicArrayC<SceneRenderable3D, kMaxRenderables>& GetRenderables() const;
    const Dia::Core::Containers::DynamicArrayC<Dia::Graphics::DirectionalLight, kMaxLights>& GetDirectionalLights() const;
    const Dia::Core::Containers::DynamicArrayC<Dia::Graphics::PointLight,       kMaxLights>& GetPointLights() const;

private:
    Dia::Graphics::Camera3D                                                     mCamera;
    Dia::Core::Containers::DynamicArrayC<SceneRenderable3D, kMaxRenderables>    mRenderables;
    Dia::Core::Containers::DynamicArrayC<Dia::Graphics::DirectionalLight, kMaxLights> mDirectionalLights;
    Dia::Core::Containers::DynamicArrayC<Dia::Graphics::PointLight,       kMaxLights> mPointLights;
};

} }
```

### `Dia::Scene3D::Submit`

```cpp
// Dia/DiaScene3D/SceneSubmit.h
#pragma once

#include "DiaScene3D/Scene3D.h"
#include <DiaGraphics/Frame/Mesh3DFrameData.h>

namespace Dia { namespace Scene3D {

struct SubmitStats
{
    uint32_t totalRenderables;
    uint32_t culled;
    uint32_t submitted;
    uint32_t skinnedRegistered;
    uint32_t droppedCapacity;
};

// Walks the scene, performs culling, registers skinning palettes, emits draw
// commands into outFrameData. Must run on the same thread that owns SkinningManager
// (sim).
SubmitStats Submit(const Scene3D& scene, Dia::Graphics::Mesh3DFrameData& outFrameData);

} }
```

## Implementation

### Files introduced

```
Dia/DiaScene3D/                               NEW MODULE
├── DiaScene3D.vcxproj                        NEW
├── DiaScene3D.vcxproj.filters                NEW
├── dia.scene3d.architecture.module.md        NEW
├── SceneRenderable3D.h                       NEW
├── Scene3D.h                                 NEW
├── Scene3D.cpp                               NEW
├── SceneSubmit.h                             NEW
├── SceneSubmit.cpp                           NEW
└── Testing/
    └── MockScene3D.h                         NEW
```

### Submit algorithm

```cpp
SubmitStats Submit(const Scene3D& scene, Mesh3DFrameData& out)
{
    SubmitStats stats{};
    stats.totalRenderables = scene.GetRenderables().Size();

    out.SetCamera(scene.GetCamera());
    for (const auto& l : scene.GetDirectionalLights()) out.AddDirectionalLight(l);
    for (const auto& l : scene.GetPointLights())       out.AddPointLight(l);

    // Derive frustum from camera once.
    const Matrix44 viewProj = scene.GetCamera().projection * scene.GetCamera().view;
    const Frustum  frustum  = Frustum::FromViewProjection(viewProj);

    SkinningManager& skinning = SkinningManager::Instance();

    for (const SceneRenderable3D& r : scene.GetRenderables())
    {
        // World matrix from Transform3D (parent chain resolved internally).
        const Matrix44 worldMatrix = r.transform->GetWorldMatrix();

        // Transform local AABB into world space.
        const AABB worldBounds = AABB::Transform(r.localBounds, worldMatrix);

        // Cull.
        if (IntersectionTests::Test(worldBounds, frustum) == IntersectionClassify::kNoIntersection) {
            stats.culled++;
            continue;
        }

        // Build draw command.
        Mesh3DDrawCommand cmd;
        cmd.meshId               = r.meshId;
        cmd.materialId           = r.materialId;
        cmd.transform            = worldMatrix;
        cmd.layer                = r.layer;
        cmd.skinningPaletteIndex = (r.skeleton != nullptr)
            ? skinning.RegisterSkinned(*r.skeleton)
            : SkinningManager::kStaticPaletteIndex;

        if (r.skeleton != nullptr) stats.skinnedRegistered++;

        out.RequestDrawMesh(cmd);

        if (out.DroppedMeshCount() > 0) stats.droppedCapacity++;
        else stats.submitted++;
    }

    return stats;
}
```

`AABB::Transform(localBounds, matrix)` is a DiaGeometry3D helper; if not yet defined, request its addition to `shape-primitives` (it's a one-line generalisation of existing AABB ops).

### Threading

- `Submit` runs on **sim** (matches DiaAnimation3D + DiaSkinning3D's tick).
- The output `Mesh3DFrameData` is part of `FrameData` which crosses the frame stream to the render PU.

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaScene3D/` (entire module) | NEW |
| `Cluiche/Cluiche.sln` | Add DiaScene3D.vcxproj |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| **DiaMaths `transform3d`, `matrix44`** | Hard | World matrix composition |
| **DiaGeometry3D `shape-primitives`, `intersection-tests`** | Hard | AABB, Frustum, AABB-vs-Frustum |
| **`graphics-3d-types`** (this batch) | Hard | Mesh3DFrameData, Camera3D, lights, draw command |
| **`skinning-palette`** (this batch) | Hard | SkinningManager::RegisterSkinned |
| **`skeleton-and-pose`** (this batch) | Soft | Skinned renderables reference SkeletonComponent3D |
| `diabgfx-3d-renderers` | Reverse | Consumes Mesh3DFrameData produced by Submit |

## Acceptance Criteria

1. `Dia/DiaScene3D/DiaScene3D.vcxproj` builds clean
2. `SceneRenderable3D` struct exists with documented fields including optional skeleton pointer
3. `Scene3D` exists with `Clear`, `SetCamera`, `AddRenderable`, `AddDirectionalLight`, `AddPointLight`, accessors
4. `Submit(scene, outFrameData)` correctly populates camera + lights + renderables on outFrameData
5. Culling: a renderable whose worldBounds is fully outside the camera frustum is **not** added to outFrameData (verified by test fixture: camera at origin looking down -Z, renderable at +Z is visible, renderable at -Z is culled)
6. Skinning index assignment: a renderable with non-null skeleton receives `cmd.skinningPaletteIndex != kStaticPaletteIndex`; static renderable gets `kStaticPaletteIndex`
7. Multiple renderables sharing one skeleton receive the same palette index (verified by inspecting return values of RegisterSkinned)
8. Capacity overflow: more than `Mesh3DFrameData::kMaxMeshDraws` visible renderables increments stats.droppedCapacity; no crash
9. SubmitStats values consistent with per-renderable outcome (totalRenderables = culled + submitted + droppedCapacity)
10. `dia.scene3d.architecture.module.md` validates
11. PD-004 audit: no STL in headers
12. `dia run googletest` is green; new TestScene3D, TestSceneSubmit suites cover culling, skinning index, capacity overflow, multi-renderable shared skeleton
13. Per RB-014: this feature implements the DiaScene3D module justified by the 3D rendering's need for camera + lights + renderables bundled coherently

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | RenderBackend | @docs/specs/systems/dia/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC | Compliant — meshId, materialId |
| PD-004 | Platform | No STL in public APIs | Compliant |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | VS project files | Compliant |
| PD-007 | Platform | C++20 | Compliant |
| AD-001 | Dia App | Module YAML | Compliant |
| AD-003 | Dia App | Namespace Dia::<Module>:: | Compliant — `Dia::Scene3D::` |
| AD-005 | Dia App | Component-based entities | Compliant — SceneRenderable3D references SkeletonComponent3D (an IComponent) |
| RB-002 | RenderBackend | Two-phase delivery | Phase 2 |
| RB-003 | RenderBackend | Phase 2 specs Approved, not in progress | Compliant |
| RB-013 | RenderBackend | Module decomposition mirrors 2D | Acceptable departure — 2D has no DiaScene2D; per RB-014 the 3D-only scene is justified |
| RB-014 | RenderBackend | Phase 2 introduces DiaScene3D | **Compliant — this feature is the implementation of RB-014** |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | No SceneNode hierarchy? | Transform3D already does parent-child (it has a parent pointer per DiaMaths::Transform3D). Adding a separate SceneNode tree duplicates that and forces a flatten step at submit. The flat-list-with-Transform3D-chains approach scales to 4096 renderables comfortably. |
| 2 | Renderer dependency on SceneSubmit | Does diabgfx-3d-renderers depend on DiaScene3D? | No — `diabgfx-3d-renderers` consumes Mesh3DFrameData (a DiaGraphics type). It doesn't know about Scene3D. The scene is on the producer side; the renderer is on the consumer side. |
| 3 | Submit performance | 4096 renderables × matrix mul + AABB transform + frustum test. Cost? | Each iteration ~200 ns: matrix mul ~50 ns, AABB transform ~20 ns, frustum test (6 plane dot products) ~100 ns, draw command emit ~30 ns. 4096 × 200 ns = ~800 μs. Acceptable for a sim tick. If profiled-hot later, BVH3D culling becomes a clear next step. |
| 4 | Frustum derivation | DiaGeometry3D's Frustum::FromViewProjection — does it exist? | Listed as in-scope for `shape-primitives` per its public interface section ("Frustum (six Plane: near/far/left/right/top/bottom)"). The constructor variant from a viewProj matrix is standard frustum extraction. If not yet declared, request via `shape-primitives` feature. |
| 5 | AABB::Transform | Same — does DiaGeometry3D provide it? | Standard "rotated AABB envelope" — common helper. Request if absent. |
| 6 | Lights without culling | Lights are forwarded uncritiqued. Could a faraway point light still affect shading? | Out of scope for this feature; renderer handles per-fragment range falloff. Per-light culling against scene bounds is a renderer optimisation, not a scene producer concern. |
| 7 | Multi-camera | Multiple Camera3D → split-screen? | Single camera per Scene3D for Phase 2. Multi-camera is a future feature. |
| 8 | DiaScene2D? | Why no 2D analogue? | 2D's draw model is unstructured: `EntityFrameData` accumulates SpriteDrawCommands directly. No camera, no lights, no skeleton-mesh linkage. The scene-graph value-add is 3D-specific. RB-014 captures the asymmetry justification. |
| 9 | Static-vs-dynamic split | Should static renderables be precomputed once and dynamic ones submitted per frame? | Out of scope. Phase 2 submits everything every frame. Static caching is a future optimisation when profile shows submit overhead. |
| 10 | Animation pump location | Where does AnimationComponent3D::Update get called? | Out of scope for this feature. Game code drives sim ticks; AnimationComponent3D's owner module calls Update before populating Scene3D. Captured in CluicheTest's PR when it lands. |

---

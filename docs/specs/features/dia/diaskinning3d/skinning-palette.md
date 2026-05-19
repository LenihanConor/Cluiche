# Feature Spec: skinning-palette

## Parent System
@docs/specs/systems/dia/render-backend.md

**Hard dependencies:**
- @docs/specs/systems/dia/diamaths.md — `Matrix34`, `Matrix44`
- This-batch features: `skeleton-and-pose` (DiaRig3D), `mesh-asset-and-loader` (DiaMesh3D), `graphics-3d-types` (Mesh3DDrawCommand carries `skinningPaletteIndex`), `clip-and-player` (DiaAnimation3D writes Pose3D)

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Create a new `Dia/DiaSkinning3D/` module that owns vertex-skinning evaluation: takes a posed `Skeleton3D` (forward-kinematics output) and produces the per-frame **skinning palette** — a GPU-uploadable array of `Matrix34` bone matrices that the vertex shader uses to deform skinned-mesh vertices. The palette buffer is owned by `Dia::Skinning3D::SkinningManager` and indexed by `Mesh3DDrawCommand::skinningPaletteIndex`.

This feature is the **glue** between Skeleton3D's FK output and the renderer's vertex-shader inputs. It defines the per-frame palette format, the manager that allocates indices and uploads via the renderer, and the data the GPU shaders read.

## Problem

A skinned mesh's vertex shader needs:
- Per-vertex `JOINTS_0` (4 bone indices) and `WEIGHTS_0` (4 weights) — already in the mesh data (`Vertex3D`)
- Per-bone `Matrix34` skinning transform — `worldBoneMatrix * inverseBindMatrix` — uploaded as a uniform array

`DiaRig3D::Skeleton3D::ComputeWorldTransforms` produces the `Matrix34[]` skinning palette. Two questions remain:

1. **Multiplexing** — multiple skinned meshes per frame each need their own palette. The renderer needs an index to look up "which palette is this draw command's"; `Mesh3DDrawCommand::skinningPaletteIndex` exists for this. Who owns the per-frame palette buffer and assigns indices?
2. **Upload** — the palette data is host-side after FK; the GPU needs it via a uniform buffer or instanced-buffer upload. Who calls `bgfx::setUniform` (or equivalent)?

This feature owns concern (1) — index allocation and the per-frame palette buffer. The renderer (`diabgfx-3d-renderers`) reads the buffer and performs upload concern (2). Decoupling matters because the palette is renderer-agnostic data; only the upload mechanism is bgfx-specific.

## Goals

- Create `Dia/DiaSkinning3D/` module
- `Dia::Skinning3D::SkinningPalette` — a fixed-size `Matrix34[kMaxBones]` block
- `Dia::Skinning3D::SkinningPaletteBuffer` — a per-frame array of palettes (`SkinningPalette[kMaxPalettesPerFrame]`); index = palette slot
- `Dia::Skinning3D::SkinningManager` — singleton-style per-frame buffer manager:
  - `BeginFrame()` — clear the palette buffer
  - `RegisterSkinned(SkeletonComponent3D*, Pose3D&) → uint32_t paletteIndex` — call FK on the skeleton, store the resulting palette in slot `N`, return `N` (which goes into Mesh3DDrawCommand::skinningPaletteIndex)
  - `GetPalette(uint32_t index) → const SkinningPalette&` — renderer-side accessor
- Sim-thread API: game code calls `RegisterSkinned` after animation Update each tick; the resulting index goes into the draw command
- Render-thread API: renderer iterates draw commands; for each with `skinningPaletteIndex != 0`, fetches the palette and uploads to GPU
- `kMaxBones = 256` (matches Skeleton3D); `kMaxPalettesPerFrame = 256` (256 skinned characters per frame is generous for "light 3D")
- 0 = "static, no skinning" sentinel (matches `Mesh3DDrawCommand::skinningPaletteIndex` documentation)
- Tests under `Dia/DiaSkinning3D/Testing/`

## Non-Goals

- **GPU upload mechanism** — `diabgfx-3d-renderers` does the `bgfx::setUniform` / instance buffer upload
- **Compute-based skinning** — vertex-shader skinning only for Phase 2
- **Skin-cluster optimisations** (4-weight → 2-weight reduction at runtime) — out of scope
- **Multi-skin per character** — one palette per skinned character; multi-skin (e.g. body + cloth) is a future feature
- **Cross-frame palette caching** — re-evaluate every frame; don't bother with "skip if pose unchanged"
- **Dual-quaternion skinning** — out of scope; linear blend skinning only

## Public Interfaces

### `Dia::Skinning3D::SkinningPalette`

```cpp
// Dia/DiaSkinning3D/SkinningPalette.h
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Matrix/Matrix34.h>

namespace Dia { namespace Skinning3D {

static constexpr unsigned int kMaxBones = 256;

struct SkinningPalette
{
    Dia::Core::Containers::DynamicArrayC<Dia::Maths::Matrix34, kMaxBones> matrices;
};

} }
```

### `Dia::Skinning3D::SkinningManager`

```cpp
// Dia/DiaSkinning3D/SkinningManager.h
#pragma once

#include "DiaSkinning3D/SkinningPalette.h"
#include <DiaCore/Architecture/Singleton/Singleton.h>

namespace Dia { namespace Rig3D { class SkeletonComponent3D; class Pose3D; } }

namespace Dia { namespace Skinning3D {

static constexpr unsigned int kMaxPalettesPerFrame = 256;
static constexpr uint32_t     kStaticPaletteIndex  = 0;   // sentinel — "no skinning"

class SkinningManager
{
public:
    SkinningManager();

    // Sim-thread: clear the per-frame buffer at frame start.
    void BeginFrame();

    // Sim-thread: compute FK on the skeleton, write the resulting palette to
    // the next free slot, return the slot index (use as Mesh3DDrawCommand::
    // skinningPaletteIndex). Returns kStaticPaletteIndex on overflow.
    uint32_t RegisterSkinned(const Dia::Rig3D::SkeletonComponent3D& skeleton);

    // Render-thread: read-only accessor.
    const SkinningPalette& GetPalette(uint32_t index) const;

    uint32_t GetActivePaletteCount() const;

private:
    Dia::Core::Containers::DynamicArrayC<SkinningPalette, kMaxPalettesPerFrame> mPalettes;
    // index 0 reserved for the "no skinning" sentinel; first allocated index is 1
};

} }
```

### Threading

- `BeginFrame` and `RegisterSkinned` run on **sim** (matches DiaAnimation3D's tick).
- `GetPalette` runs on **render** (read-only after sim's `RegisterSkinned` completes for the frame).
- The handoff is the existing FrameStream — `Mesh3DFrameData` carries draw commands with palette indices; renderer reads palettes via `SkinningManager::Instance()` (singleton).
- The palette buffer is double-buffered if needed (sim writes frame N+1 while render reads frame N); investigate during implementation. For Phase 2 simplest path: single buffer with sim-finished-before-render barrier (existing PU phase ordering already provides this).

## Implementation

### Files introduced

```
Dia/DiaSkinning3D/                            NEW MODULE
├── DiaSkinning3D.vcxproj                     NEW
├── DiaSkinning3D.vcxproj.filters             NEW
├── dia.skinning3d.architecture.module.md     NEW
├── SkinningPalette.h                         NEW
├── SkinningManager.h                         NEW
├── SkinningManager.cpp                       NEW
└── Testing/
    └── MockSkinningManager.h                 NEW
```

### Skinning algorithm (per-bone)

```
for bone i in 0..N-1:
    skinning[i] = skeleton.world[i] * bone[i].inverseBindMatrix
                  // Matrix44 * Matrix44; convert result to Matrix34 (drop projection row)
                  // Stored in palette.matrices[i]
```

`Skeleton3D::ComputeWorldTransforms` already produces `Matrix34` skinning palette directly when its `outSkinningPalette` parameter is non-null. `SkinningManager::RegisterSkinned` calls that method, producing the palette in one step.

### Vertex shader (reference, lives in `diabgfx-3d-renderers`)

```glsl
// Pseudo-GLSL (the actual .sc file lives under Dia/DiaBgfx/Shaders/)
attribute vec3  a_position;
attribute uvec4 a_jointIndices;
attribute vec4  a_jointWeights;

uniform mat3x4 u_skinningPalette[256];   // Matrix34 per bone

vec3 SkinPosition()
{
    mat3x4 boneMatrix =
        u_skinningPalette[a_jointIndices.x] * a_jointWeights.x +
        u_skinningPalette[a_jointIndices.y] * a_jointWeights.y +
        u_skinningPalette[a_jointIndices.z] * a_jointWeights.z +
        u_skinningPalette[a_jointIndices.w] * a_jointWeights.w;

    return (boneMatrix * vec4(a_position, 1.0)).xyz;
}
```

The palette is uploaded per draw via `bgfx::setUniform(u_skinningPalette, palette.matrices.Data(), 256)`.

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaSkinning3D/` (entire module) | NEW |
| `Cluiche/Cluiche.sln` | Add DiaSkinning3D.vcxproj |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| **DiaMaths `matrix34`, `matrix44`** | Hard | Palette is Matrix34; FK uses Matrix44 |
| **DiaRig3D `skeleton-and-pose`** | Hard | `Skeleton3D::ComputeWorldTransforms` produces the palette |
| **DiaAnimation3D `clip-and-player`** | Soft | Animation populates the pose that FK consumes |
| `graphics-3d-types` (Approved) | Hard | `Mesh3DDrawCommand::skinningPaletteIndex` is set from `RegisterSkinned`'s return |
| `diabgfx-3d-renderers` | Reverse | Renderer reads palette via SkinningManager and uploads to bgfx |

## Acceptance Criteria

1. `Dia/DiaSkinning3D/DiaSkinning3D.vcxproj` builds clean
2. `Dia::Skinning3D::SkinningPalette` exists with `Matrix34[kMaxBones]` storage
3. `Dia::Skinning3D::SkinningManager` exists with `BeginFrame`, `RegisterSkinned`, `GetPalette`, `GetActivePaletteCount`
4. `kStaticPaletteIndex = 0` reserved sentinel; first allocated palette index is 1
5. `kMaxPalettesPerFrame = 256` honoured; overflow returns `kStaticPaletteIndex` and logs once per frame
6. `RegisterSkinned` calls `Skeleton3D::ComputeWorldTransforms` with `outSkinningPalette` populated; result stored in palette buffer
7. `BeginFrame` resets the buffer (size 0, next allocation is index 1)
8. Tests cover: register single skeleton → palette index 1 returned, palette contents match expected (manually computed FK × inverseBindMatrix); BeginFrame resets; overflow handling
9. PD-004 audit: no STL in headers
10. `dia.skinning3d.architecture.module.md` validates
11. `dia run googletest` is green
12. No bgfx types in DiaSkinning3D headers (RB-006)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System (primary) | RenderBackend | @docs/specs/systems/dia/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC | N/A |
| PD-004 | Platform | No STL in public APIs | Compliant |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | VS project files | Compliant |
| PD-007 | Platform | C++20 | Compliant |
| AD-001 | Dia App | Module YAML | Compliant |
| AD-003 | Dia App | Namespace Dia::<Module>:: | Compliant — `Dia::Skinning3D::` |
| **DiaMaths SD-008** | DiaMaths | Matrix34 affine for transform hierarchies | **Compliant — palette is Matrix34** |
| RB-002 | RenderBackend | Two-phase delivery | Phase 2 |
| RB-003 | RenderBackend | Phase 2 specs Approved, not in progress | Compliant |
| RB-006 | RenderBackend | No backend types in public surface | Compliant |
| RB-013 | RenderBackend | Module decomposition mirrors 2D | Compliant — DiaSkinning3D is the 3D analogue (2D had no equivalent because 2D rigs aren't typically skinned the same way) |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Singleton pattern | `SkinningManager` as a process-global singleton — correct? | Yes — matches DiaCore's `Singleton<T>` pattern (PD-002 compatible). One palette buffer per process per frame. |
| 2 | Double-buffering | Sim writes the palette while render reads it. Race? | Existing PU phase ordering: sim's `BeginFrame` + `RegisterSkinned` complete before render reads. If perf testing shows the barrier is wasteful, double-buffer. Defer that decision to implementation. |
| 3 | Palette memory | 256 palettes × 256 bones × Matrix34 (48 bytes) = 3 MB worst case. Acceptable? | Yes. 3 MB is small; uniform-buffer-style storage. Allocated once at SkinningManager construction. |
| 4 | Index 0 sentinel | Reserving 0 means first valid index is 1. Memory waste? | One palette slot (48 KB at 256 bones × 48 B). Acceptable for the simplicity of the sentinel. |
| 5 | Renderer access | How does `diabgfx-3d-renderers` access the manager? | Singleton. `SkinningManager::Instance().GetPalette(cmd.skinningPaletteIndex).matrices.Data()` returns the float pointer; renderer calls `bgfx::setUniform(u_palette, ptr, 256)`. |
| 6 | Re-skin on dirty pose only? | Skip recompute if pose unchanged? | Out of scope. FK is fast; the optimisation is premature for "light 3D" scope. |
| 7 | Multiple meshes sharing one skeleton | Can one palette index serve multiple draw commands? | Yes — caller registers once, reuses the index across all draw commands referencing that skeleton. Common case: one character with multi-submesh body sharing one skeleton. |
| 8 | Single-bone fast path | If a mesh has only joint 0 = bone 0 with weight 1, can we skip skinning? | Out of scope. Vertex shader handles weights of 1 with no special case. |
| 9 | What if Skeleton3D has >kMaxBones? | Skeleton3D's loader caps at 256; if exceeded, the load fails. SkinningManager assumes valid input. |
| 10 | DiaSkinning2D? | No 2D analogue today. Phase 2 takes the asymmetry (2D rigs use FK without skinning palettes; rendering is direct from bone transforms). |

---

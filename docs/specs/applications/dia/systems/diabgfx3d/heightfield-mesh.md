# Feature Spec: DiaBgfx3D Heightfield Mesh (Feature E)

## Parent System
@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md (Amendment 2026-11-20, Feature E added 2026-11-23)

**Gameplay Domains:** rendering
**Needed by:** DraoiHexWorldRender3D Hex Terrain Surface (@docs/specs/applications/draoi/systems/draoihexworldrender3d/hex-terrain-surface.md)
**Depends on:** DiaBgfx3D Feature A (dynamic textures)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (`buildWorldGPU`, `GROUND_VS`, sea skirt)

## Status
`Draft` (2026-11-23). The geometry half of the former DiaTerrainMap3D Heightfield Terrain feature (Approved 2026-11-22). The content is carried over unchanged from that approval, with terrain vocabulary removed; placement re-confirmed 2026-11-23 (user). **AD-001 signed off 2026-11-23 (user):** `HeightfieldMesh`, `heightfield.sh`.

## Summary

Draw a large heightfield from a height texture: a chunked vertex grid that fits 16-bit indices, displaced in the vertex shader, frustum-culled per chunk, with an optional horizon skirt so a tilted camera never sees the void. The consumer supplies the height texture and the material. Any game with terrain can use it.

## Problem

- `Mesh3DAsset` allows at most 65,535 vertices with 16-bit indices. A 157 × 112 unit world at four vertices per unit is about 280,000 vertices, so it must be chunked (about 20 chunks of 128 × 128).
- A tilted camera sees past the edge of the heightfield.
- `Vertex3D` (52 bytes) is wasteful when height and normal come from a texture.

## Goals

1. **Chunked mesh.** A regular vertex grid over a rectangle, split into chunks that each fit 16-bit indices, with seamless borders. Vertices store `(x, y)` only.
2. **Displacement.** A vertex include (`heightfield.sh`) reads height from a consumer-bound texture and displaces by `heightScale` (a per-frame uniform, so a camera can ease it with zoom).
3. **Horizon skirt.** An optional large quad around the rectangle at a base height, drawn with the consumer's material.
4. **Per-chunk frustum culling** using bounds inflated by `heightScale`.
5. **CPU height** helper: `HeightAt(x, y)` from a consumer-supplied CPU height array, bilinear, matching the GPU displacement.
6. **Default flat-lit material**, so the geometry can be checked with no consumer material.

## Non-Goals

- What the height means, and any texture other than height (consumer).
- LOD (ODQ 1). Collision. Runtime editing.

## Design

- Units are the consumer's world units. Grid spacing is `1 / vertsPerUnit` (default 4).
- A chunk is `chunkVerts × chunkVerts` (default 128 × 128 = 16,384 verts, 32,258 triangles). Adjacent chunks share their border rows, generated from the same texture, so the border is identical. Edge chunks may be partial.
- One shared vertex buffer per chunk shape, the chunk offset as a uniform, one shared index buffer. Dedicated bgfx static buffers, not `MeshGpuCache`.
- Winding and handedness follow `Camera3D`.

```cpp
namespace Dia::Bgfx3D {
    struct HeightfieldParams { uint32_t chunkVerts = 128; float vertsPerUnit = 4.0f; float skirtMargin = 400.0f; bool skirt = true; };
    struct HeightfieldExtent { float extentX, extentY; };

    class HeightfieldMesh {
    public:
        bool Build(const HeightfieldExtent&, const HeightfieldParams&);
        uint32_t ChunkCount() const;
        void Cull(const Camera3D&, float heightScale, uint32_t* outChunkIds, uint32_t& count) const;
        void Draw(const uint32_t* chunkIds, uint32_t count, const Material&, bool drawSkirt);
    };
    float HeightAt(const float* heights, uint32_t texW, uint32_t texH, float texelsPerUnit, float x, float y);
}
```

## Acceptance Criteria

1. **Chunk limits.** No chunk exceeds 65,535 vertices or 16-bit indices, including at 157 × 112 units.
2. **Coverage.** The chunks cover the rectangle exactly (no gaps, no overlap except shared border rows), including sizes that do not divide evenly.
3. **Seamless.** Adjacent chunks give identical displaced positions along their shared border (CPU check, 1e−5).
4. **Displacement.** A vertex's displaced height equals `heightScale × height` read from the texture at its `(x, y)` (CPU reference).
5. **Skirt.** At 70° tilt looking at the horizon, every pixel below the horizon line is skirt or heightfield, never clear colour.
6. **Culling.** A camera on one corner culls the opposite chunks (counter). Culling never removes a visible chunk at maximum height scale.
7. **HeightAt** matches texel centres within half precision and is bilinear in between; returns 0 outside.
8. **Default material.** A capture with the flat-lit material shows no holes or cracks at chunk borders (user taste check).
9. **Observability.** Profile scopes `Heightfield.Cull`, `Heightfield.Draw`; metric `bgfx3d.heightfield_chunks_drawn`.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| RB-006 | No backend types in DiaGraphics | Lives in DiaBgfx3D |
| PD-004 | No STL in public APIs | Fixed chunk table, raw arrays |
| AD-001 | Engine changes need sign-off | Feature E signed off 2026-11-23 |

## Open Design Questions

1. **LOD.** At strategic zoom, far chunks of a large map are sub-pixel. A simple LOD (every other vertex, with the border handled) fits the same chunk table later. Measure first.
2. **Axis convention.** The prototype's world is y-down with negative z up. Pin it in task 1 with one test that a known world point lands where `Camera3D` expects, so consumers' picking inherits it.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Chunk table, shared buffers, axis test | AC 1, 2, 3 | Not Started | sonnet | AD-001 signed off 2026-11-23 |
| 2 | `heightfield.sh` displacement + default flat-lit material | AC 4, 8 | Not Started | sonnet | |
| 3 | Skirt | AC 5 | Not Started | sonnet | |
| 4 | Frustum culling | AC 6 | Not Started | sonnet | |
| 5 | `HeightAt` | AC 7 | Not Started | sonnet | |
| 6 | Observability | AC 9 | Not Started | haiku | |

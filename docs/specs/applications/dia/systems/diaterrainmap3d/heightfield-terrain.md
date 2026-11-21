# Feature Spec: DiaTerrainMap3D Heightfield Terrain

## Parent System
@docs/specs/applications/dia/systems/diaterrainmap3d/diaterrainmap3d.md

**Gameplay Domains:** rendering
**Depends on:** DiaCellFieldBake Features 1–3 (fields, tiles), DiaBgfx3D Feature A (dynamic textures)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (`buildWorldGPU`, `GROUND_VS`, sea skirt, `commonUniforms`)

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

Turn baked fields into a 3D terrain you can look at: a chunked heightfield mesh whose vertices take their height from the baked elevation texture, a sea skirt out to the horizon, and the GPU textures the terrain material (Feature 2) reads. After this feature the map is visible, flat-coloured and lit but not yet styled.

It owns the *geometry and the textures*; Feature 2 owns the *look*.

## Problem

- A baked field is just arrays. Something has to become a mesh, upload textures, keep per-cell channel textures current, and draw in the right order against the rest of the scene.
- `Mesh3DAsset` allows at most 65,535 vertices per mesh with 16-bit indices. A Large grid at four vertices per world unit is about 100,000 vertices, so the terrain must be chunked.
- The camera sees an ocean to the horizon, not the grid's edge.

## Goals

1. **Chunked mesh.** A regular vertex grid over the world rectangle, split into chunks that each fit 16-bit indices, with seamless borders.
2. **Height from the field.** The vertex shader reads elevation from the baked scalar texture and displaces vertically (`height scale` is a style parameter).
3. **Texture residency.** Baked tiles (from DiaCellFieldBake) are uploaded to GPU textures; per-cell channel textures (condition, weather, events, water state) are created once and updated by `UpdateChannel`.
4. **Sea skirt.** A large quad around the map, drawn with the same material, so tilted views never show the void.
5. **Per-chunk culling** against the camera frustum, and a per-chunk request of the baked tiles it needs.
6. **A simple lit, flat-coloured material** as the default, so the geometry is verifiable before Feature 2.

## Non-Goals

- The terrain look: ink, hatching, palettes, snow, rivers, events (Feature 2).
- Props, particles, post pass (Features 3, 4 and DiaBgfx3D).
- LOD meshes. One mesh resolution; far chunks are the same mesh. (Open question 1.)
- Terrain collision or height queries beyond `HeightAt(x, y)` from the baked elevation.
- Editing the terrain at runtime.

## Design

### Mesh

- World units are cell units (hex edge = 1). The vertex grid spacing is `1 / vertsPerUnit` (default 4, so 0.25 units).
- A chunk is `chunkVerts × chunkVerts` vertices (default 128 × 128 = 16,384 verts, 32,258 triangles), covering `(chunkVerts − 1) / vertsPerUnit` units (31.75). Adjacent chunks **share their border vertex rows** (no skirts): each chunk generates its own border vertices from the same field, so the border is identical.
- A vertex stores `(x, y)` only (two floats); height, normal and everything else come from textures. One shared vertex buffer per chunk *shape*; the chunk's world offset is a uniform. Edge chunks may be partial.
- Index buffer shared across all full chunks.
- Winding and handedness follow `Camera3D` (y down in the prototype's world; z negative up). The mesh is registered via `MeshHandler::RegisterMesh` or a dedicated buffer; a dedicated `bgfx` static buffer is preferred (the terrain doesn't need `Vertex3D`).

### Textures

| Texture | Source | Format | Update |
|---|---|---|---|
| Field (scalars: elevation, land, lake, temperature) | baked tiles | RGBA16F | per tile, when baked |
| Owner / class (primary, secondary, margin) | baked tiles | RGBA8 | per tile |
| River (dist, width, dir) | baked tiles | RGBA8 | per tile |
| Per-cell channels (condition, weather, events, water state) | game | RGBA16F, one texel per cell, linear | `UpdateChannel` |
| Per-cell class data (primary, secondary, depth zone, health) | game | RGBA8, one texel per cell, nearest | `SetCellData` |

Baked tiles are packed into a **tile atlas texture** per kind (a grid of tile slots), so the material samples one texture per kind, with a small indirection table (`tileSlot[tx, ty]`) as a uniform array/texture. Slots are recycled when the `TileCache` evicts.

### Draw order

Sea skirt → terrain chunks → (Features 3/4: props, particles) in the scene target, depth write on. Chunks use frustum culling by their world bounding box inflated by `heightScale`.

### Interface (sketch)

```cpp
namespace Dia::TerrainMap3D {
    struct TerrainGeometryParams { uint32_t chunkVerts = 128; float vertsPerUnit = 4.0f; float seaSkirtMargin = 400.0f; };
    struct TerrainWorld { float extentX, extentY; /* cell units */ uint32_t gridW, gridH; };

    class TerrainMesh {
    public:
        bool Build(const TerrainWorld&, const TerrainGeometryParams&);          // chunk table, shared buffers
        uint32_t ChunkCount() const;
        void Cull(const Camera3D&, float heightScale, uint32_t* outChunkIds, uint32_t& count) const;
        void Draw(const uint32_t* chunkIds, uint32_t count, const TerrainMaterial&);
    };
    class TerrainTextures {
    public:
        bool Create(const TerrainWorld&, uint32_t tileSize, uint32_t atlasSlots);
        bool UploadTile(const CellFieldBake::TileKey&, const CellFieldBake::BakedFields&);
        void EvictTile(const CellFieldBake::TileKey&);
        void UpdateChannel(Channel, const float* rgbaPerCell);
        void SetCellData(const uint8_t* rgbaPerCell);
        float HeightAt(float x, float y) const;                                   // from resident baked tiles, 0 if absent
    };
}
```

## Acceptance Criteria

1. **Chunk limits.** Every chunk has at most 65,535 vertices and 16-bit indices; a Large world builds with no chunk exceeding them.
2. **Coverage.** The union of chunks covers the world rectangle exactly (no gaps, no overlap other than shared border rows), for Small, Standard and Large and for sizes that do not divide evenly.
3. **Seamless.** Two adjacent chunks produce identical vertex positions and heights along their shared border (checked CPU-side on the displaced positions, tolerance 1e−5).
4. **Height.** For a baked world, a vertex's displaced height equals `heightScale × max(elevation, 0)` read from the field at its `(x, y)` (CPU reference), including a flat lake at its surface height and ocean clamped to the sea plane.
5. **Sea skirt.** A camera looking at the horizon at 70° tilt sees ocean (the skirt colour) in every pixel below the horizon line; no clear colour shows below the horizon.
6. **Culling.** A camera looking at a corner of Large culls the opposite chunks; culled chunks are not submitted (counter). Culling never removes a chunk whose displaced geometry is visible (a test at max height scale).
7. **Tile atlas.** Uploading a tile puts its texels in a slot; the indirection table maps `(tx, ty)` to that slot; evicting a tile frees the slot for reuse; sampling a non-resident tile returns the outside value (sea), not garbage.
8. **Channel update.** `UpdateChannel` followed by a draw of a test material that outputs the channel shows the uploaded values; updating twice in a frame keeps the last; the texture is not recreated per update.
9. **HeightAt.** Matches the field at texel centres within half precision and bilinear in between; returns 0 for non-resident tiles.
10. **Default material.** With the default flat-lit material, a captured frame of a small S-1 world shows terrain relief with no holes or cracks at chunk borders (user taste check).
11. **Performance (Debug, ballpark).** Chunk count, vertices submitted and frame cost on Large are logged to the plan notes.
12. **Observability.** Log channel `TMAP`; profile scopes `Terrain.Cull`, `Terrain.Draw`, `Terrain.UploadTile`; metrics `tmap.chunks_drawn`, `tmap.tiles_resident`, `tmap.upload_ms`.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-002 | 3D heightfield, not tiles | This feature |
| TMAP-004 | Mountains are terrain relief | Relief is in the field, so chunks only displace |
| CFB-001..003 | DiaCellFieldBake | Uploads baked tiles only; no class lookups in the renderer |
| RB-006 | No backend types in DiaGraphics | Mesh and texture classes live in DiaTerrainMap3D over DiaBgfx |
| PD-004 | No STL in public APIs | Fixed tables, `DynamicArrayC` for the chunk list |
| AD-001 | Engine changes need sign-off | New module doc; uses existing DiaBgfx3D plus Feature A |

## Open Design Questions

1. **LOD.** At strategic zoom a Large map's far chunks are sub-pixel. One mesh resolution costs vertex work for nothing. A simple LOD (every other vertex for distant chunks, with the border handled) is easy to add later through the same chunk table. Measure before building.
2. **Vertical exaggeration at far zoom.** The height scale could shrink as the camera zooms out, easing occlusion. That belongs to the camera (Strategic Map Feature 4), but the mesh must expose `heightScale` per frame.
3. **Mesh through `MeshGpuCache` or dedicated buffers?** Dedicated is simpler and avoids `Vertex3D` and its 52 bytes per vertex. Confirm no shadow or debug path needs the terrain as a `Mesh3DAsset` (the terrain does not cast shadows in v1).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Module skeleton `Dia/DiaTerrainMap3D/`, docs, deps | `dia check deps` | Not Started | haiku | |
| 2 | Chunk table and shared index/vertex buffers | AC 1, 2, 3 | Not Started | sonnet | |
| 3 | Displacement vertex shader + default flat-lit material | AC 4, 10 | Not Started | sonnet | needs shader root (Feature 2 task 1) |
| 4 | Sea skirt | AC 5 | Not Started | sonnet | |
| 5 | Frustum culling | AC 6 | Not Started | sonnet | |
| 6 | Tile atlas and indirection, `HeightAt` | AC 7, 9 | Not Started | sonnet | |
| 7 | Channel and cell-data textures | AC 8 | Not Started | sonnet | needs DiaBgfx3D Feature A |
| 8 | Observability and Large-world numbers | AC 11, 12 | Not Started | haiku | |

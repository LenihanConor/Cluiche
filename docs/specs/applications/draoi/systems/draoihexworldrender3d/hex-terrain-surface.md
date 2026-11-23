# Feature Spec: DraoiHexWorldRender3D Hex Terrain Surface

## Parent System
@docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md

**Gameplay Domains:** rendering
**Depends on:** DiaCellFieldBake Features 1 and 3 (fields, whole-world bake, `Commit` rectangle); DraoiHexWorldRelief (relief, river field); DiaBgfx3D Feature A (dynamic textures with sub-rectangle `Update`) and Feature E (heightfield mesh, skirt, `HeightAt`)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (`buildWorldGPU`, `commonUniforms`)

## Status
`Approved` (2026-11-23, user: placement re-confirmed). **Was DiaTerrainMap3D Feature 1 Heightfield Terrain, `Approved` 2026-11-22 (user)** after review: tile atlas, indirection table and eviction removed; whole-world textures with rectangle updates on `Commit`; coast-fraction texture added. **Layer split 2026-11-23:** the chunked mesh, displacement, skirt, culling and `HeightAt` moved to DiaBgfx3D Feature E (@docs/specs/applications/dia/systems/diabgfx3d/heightfield-mesh.md). This feature keeps the hex-world textures and adds the standard channels and custom slots (TMAP-010). The approved content is otherwise unchanged.

## Summary

Turn a baked hex world into the GPU textures the hex terrain material (Feature 2) reads, and put it on the DiaBgfx3D heightfield: field, class, coast-fraction, relief and river textures from the bake; per-cell class data; the standard channel textures and the custom slot texture. After this feature the map is visible on DiaBgfx3D E's flat-lit material, but not yet styled.

It owns the *textures and their updates*. DiaBgfx3D E owns the *geometry*; Feature 2 owns the *look*.

## Problem

- A baked field is just arrays. Something has to upload them, keep per-cell channel textures current, and hand the height texture to the heightfield mesh.
- The standard channels and custom slots (TMAP-010) need a fixed GPU layout, so the material can be cooked offline.
- The camera sees an ocean to the horizon, not the grid's edge: the skirt (DiaBgfx3D E) must be drawn with the sea colour.

## Goals

1. **Texture upload.** The baked world (DiaCellFieldBake plus DraoiHexWorldRelief) is uploaded to whole-world GPU textures once, and refreshed by rectangle when the bake commits an incremental rebake. A bake at a different resolution (the in-game editor's low-res preview) recreates the textures.
2. **Standard channels** (Condition, Weather, Snow, Water, Events) as per-cell textures with a fixed layout, created once and updated by `UpdateChannel`.
3. **Custom slots**: one RGBA16F per-cell texture holding the 4 custom overlay values, updated by `UpdateCustomSlots`.
4. **Per-cell class data** (primary, secondary, stratum, health) by `SetCellData`.
5. **Heightfield hookup.** The elevation texture drives DiaBgfx3D E's displacement; the skirt draws at sea level with the material's sea colour.
6. **CPU height** for picking and props: `HeightAt` through DiaBgfx3D E's helper over the baked elevation.

## Non-Goals

- The terrain look: ink, hatching, palettes, snow, rivers, events (Feature 2).
- Geometry, chunking, culling, LOD (DiaBgfx3D E).
- Props, particles, post pass (Features 3, 4 and DiaBgfx3D).
- What fills the channels (the game's adapter).
- Editing the terrain at runtime.

## Design

### Textures

| Texture | Source | Format | Update |
|---|---|---|---|
| Field (first 4 baked scalars: elevation with relief, land, lake, a fourth static scalar) | baked world + DraoiHexWorldRelief | RGBA16F (the bake's half floats upload as they are) | whole, then rectangle on `Commit` |
| Class (primary, secondary, margin) | baked world | RGBA8 | same |
| Coast fractions (primary fractions, `kMaxPrimary` = 4) | baked world | RGBA8 | same. Needed for coast ink at fraction 0.5 |
| River (dist, width, dir) | DraoiHexWorldRelief post-pass | RGBA8 | same, absent when the world has no rivers |
| Standard channels: Condition (health, vegetation), Weather (cloud, precipitation, snow fraction, gust), Snow (pack, temperature), Water (lake level, flow state, flood, ice), Events (burn, scar) | game | RGBA16F each, one texel per cell, linear | `UpdateChannel` |
| Custom slots 0–3 | game | RGBA16F, one texel per cell, linear | `UpdateCustomSlots` |
| Per-cell class data (primary, secondary, stratum, health) | game | RGBA8, one texel per cell, nearest | `SetCellData` |

The owner array (`uint16_t`) stays on the CPU for `OwnerAt` and is not uploaded. A bake with more than four scalars uses extra RGBA16F textures. A Large world is about 3.4 M texels, so the field texture is about 27 MB and well inside the D3D11 texture size limit. The material samples whole-world textures directly; there is no atlas or indirection table.

### Draw order

Skirt → terrain chunks (both DiaBgfx3D E, with the Feature 2 material) → (Features 3/4: props, particles) in the scene target, depth write on.

### Interface (sketch)

```cpp
namespace Draoi::HexWorldRender3D {
    enum class Channel : uint8_t { Condition, Weather, Snow, Water, Events };
    class HexTerrainSurface {
    public:
        bool Create(const Dia::CellFieldBake::BakedFields& world, const HexWorldRelief::ReliefFields&); // whole-world textures
        bool UpdateRect(const Dia::CellFieldBake::BakedFields& world, const HexWorldRelief::ReliefFields&,
                        Dia::CellFieldBake::TexelRect changed);                                         // after Commit
        void UpdateChannel(Channel, const float* rgbaPerCell);
        void UpdateCustomSlots(const float* rgbaPerCell);
        void SetCellData(const uint8_t* rgbaPerCell);
        float HeightAt(float x, float y) const;            // DiaBgfx3D E helper over the baked elevation; basis of picking
        const Dia::Bgfx3D::HeightfieldMesh& Mesh() const;
    };
}
```

## Acceptance Criteria

1. **Height hookup.** For a baked world, a vertex's displaced height equals `heightScale × max(elevation, 0)` from the field (CPU reference), including a flat lake at its surface height and ocean clamped to the sea plane.
2. **Sea skirt.** A camera looking at the horizon at 70° tilt sees ocean (the sea colour) in every pixel below the horizon line.
3. **World upload and rectangle update.** After `Create`, a read-back of each texture equals the baked arrays. `UpdateRect` with a changed rectangle changes exactly that rectangle (a texel outside it is untouched), the coast-fraction and river textures are included, and a rectangle outside the world is rejected with a warning. A `Create` at a different resolution replaces the textures and the old ones are released.
4. **Channel update.** `UpdateChannel` for each standard channel and `UpdateCustomSlots`, followed by a draw of a test material that outputs the channel, shows the uploaded values; updating twice in a frame keeps the last; the texture is not recreated per update.
5. **HeightAt.** After `UpdateRect`, `HeightAt` reflects the committed data.
6. **Default material.** With DiaBgfx3D E's flat-lit material, a captured frame of a small S-1 world shows terrain relief with no holes or cracks (user taste check).
7. **Performance (Debug, ballpark).** Upload bytes and ms on Large are logged to the plan notes.
8. **Observability.** Log channel `HWR3D`; profile scope `HexTerrain.Upload`; metrics `hwr3d.upload_bytes`, `hwr3d.upload_ms`, `hwr3d.channel_uploads`.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-002 | 3D heightfield, not tiles | This feature |
| TMAP-004 | Mountains are terrain relief | Relief is in the field (DraoiHexWorldRelief), so the mesh only displaces |
| TMAP-010 | Hybrid channels | Five standard channel textures plus one custom slot texture, fixed layout |
| CFB-001..005 | DiaCellFieldBake | Uploads the baked world and committed rectangles only; no class lookups in the renderer; per-tick data is a channel (CFB-005) |
| PD-004 | No STL in public APIs | Fixed tables, `DynamicArrayC` for the chunk list |
| PD-011 to PD-014 | Draoi layer rules | Plain library; uses Dia through public APIs only |

## Open Design Questions

1. **Vertical exaggeration at far zoom.** The height scale could shrink as the camera zooms out, easing occlusion. That belongs to the camera (CoW Strategic Map Feature 4). DiaBgfx3D E takes `heightScale` per frame, so nothing is needed here.
2. **Channel packing.** Five standard RGBA16F textures plus the custom one is 6 samplers per pixel on top of the baked textures. If the sampler count or bandwidth hurts, pack Snow into Weather. Measure in Feature 2.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Package skeleton `Draoi/DraoiHexWorldRender3D/`, docs, deps | `dia check deps` | Not Started | haiku | |
| 2 | Whole-world texture upload, `UpdateRect`, `HeightAt` | AC 3, 5 | Not Started | sonnet | needs DiaBgfx3D A, E |
| 3 | Heightfield hookup and sea skirt | AC 1, 2, 6 | Not Started | sonnet | |
| 4 | Standard channel, custom slot and cell-data textures | AC 4 | Not Started | sonnet | |
| 5 | Observability and Large-world numbers | AC 7, 8 | Not Started | haiku | |

# Feature Spec: DiaTerrainMap3D Map Props and Emitters

## Parent System
@docs/specs/applications/dia/systems/diaterrainmap3d/diaterrainmap3d.md

**Gameplay Domains:** rendering
**Depends on:** DiaTerrainMap3D Heightfield Terrain; DiaBgfx3D Instanced Sprites (B) and Particles (C); DiaCellFieldBake (`OwnerAt`)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/props.js` (`placeProps`, `BIOME_PROPS`), `js/app.js` (`buildClouds`), `js/alive.js` (`Fauna.instances`), `js/weather.js` (`Precip`)

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

Everything that stands on or flies above the terrain: props scattered by class and condition, dynamic sprite layers (clouds, fauna), and particle emitters driven by per-cell density. DiaBgfx3D already draws instanced sprites and particles; this feature is the *map-specific use* of them:

- **Deterministic scatter.** From a per-class rule table, per-cell seeds and the condition channel, produce a static instance list that never lands on water.
- **Dynamic layers.** A small API for the game to push per-frame sprites (cloud puffs, herds, birds) with a ground height already resolved.
- **Particle density.** Map per-cell weather and event channels to DiaBgfx3D `DensityField`s for rain, snow and smoke, with rain-or-snow decided per cell.

## Problem

- 4,000 to 20,000 props must look natural, not move between runs, track ecology condition, and thin out or die as land degrades (Layer 2 expression).
- Props, clouds, herds and particles all need the terrain height at their position and must never stand in the sea or a lake.
- Rain and snow must appear only where the weather channel says so, and fall as snow on cold cells.

## Goals

1. **Deterministic scatter**: the same world, seed, class table and condition give the same instance list, at any thread count.
2. **Class rules** (data): per class, a list of `{ propType, density per cell, size, minimum slope, maximum slope, wet-only flag }`.
3. **Condition scaling**: density follows the ecology bands, so Collapsed cells have about 5% of baseline and Thriving about 130%. Trees degrade to dead trees at low condition.
4. **Placement validity** via `OwnerAt`/margin: never in water, never on a class border margin of 0 (avoid straddling), snap `z` to terrain `HeightAt`.
5. **Incremental rescatter**: when a cell's class or condition changes, only its instances are recomputed.
6. **Dynamic sprite layers** with per-frame instance building and ground snap.
7. **Particle density mapping**: weather channel → rain and snow density by the cell's own temperature; events channel → smoke density.
8. **Fixed-frequency sway** with amplitude from per-instance rigidity and the weather gust channel (TMAP-006).

## Non-Goals

- Authoring prop art or palettes (Strategic Map Feature 3).
- Simulating herds, clouds or fire (the game does; this feature draws instances it is handed).
- Collision, shadows, per-prop lighting.
- LOD billboards or impostors.

## Design

```cpp
namespace Dia::TerrainMap3D {
    struct PropRule { uint16_t propType; float perCell;         // expected count per cell at baseline
                      float size; float sizeJitter;             // world units tall
                      float minSlope, maxSlope; bool needsWet; uint16_t deadVariant; };
    struct ClassPropRules { const PropRule* rules; uint32_t count; };   // per class id

    struct ScatterInputs {
        const CellFieldBake::BakedFields* const* tiles;  // or a sampler over the cache
        const uint8_t* primaryClass; const uint8_t* secondaryClass;      // per cell
        const float*   condition;                                         // per cell 0..1
        const ClassPropRules* rulesByClass; uint32_t classCount;
        const uint32_t* paletteByClass;                                   // 3 x RGBA8 per class, per season set
        uint32_t seed;
    };
    class PropScatter {
    public:
        void Rebuild(const ScatterInputs&, SpriteInstanceSink&);           // all cells
        void RebuildCells(const uint16_t* cells, uint32_t n, const ScatterInputs&, SpriteInstanceSink&);
    };
}
```

- **Per-cell stream.** Each cell seeds its own RNG from `(cellId, seed)`, so rescattering one cell never perturbs another, and the result does not depend on iteration order or thread count.
- **Count.** For each rule, `n = perCell × densityScale(condition) × slopeFactor`; the integer part plus a Bernoulli on the fraction.
- **Density scale.** Piecewise: condition < 0.33 → `0.05 + 1.2c`; < 0.66 → `0.45 + 1.67(c − 0.33)`; else `1.0 + 0.9(c − 0.66)`.
- **Position.** Uniform in a disc of radius 0.85 around the cell centre, accepted only if `OwnerAt` is that cell (up to 6 tries), the point is land, and slope is within the rule's range. `z` from `HeightAt`.
- **Dead variants.** Tree rules with `deadVariant` switch to it when `condition < 0.22` with probability 0.6 (deterministic per instance).
- **Colour.** Each instance carries the class palette's three slot colours, with a seasonal override set supplied by the game.
- **Dynamic layers.** `DynamicSprites` takes `{x, y, type, scale, palette, flags}` and fills `z` from `HeightAt` (lifted by a per-layer altitude for clouds and birds), then submits to the translucent or cutout layer.
- **Particle mapping.** Per-cell channels → `DensityField` at cell resolution: rain density = `precip × (1 − snowFraction)`, snow density = `precip × snowFraction`, smoke density = burn intensity. The `DensityField` is rewritten when the channel changes.

## Acceptance Criteria

1. **Determinism.** Two scatters of the same inputs give identical instance lists (byte-identical); different seeds give different lists; the result is independent of thread count and cell iteration order.
2. **Locality.** `RebuildCells` for K cells changes only instances belonging to those cells; the result equals a full rebuild with the same inputs.
3. **No water.** Over S-1 Small worlds (all presets, seeds 1–3), no instance has `OwnerAt` pointing to a water cell or `kNoOwner`.
4. **Terrain snap.** Every instance's `z` equals `HeightAt(x, y)` within half precision.
5. **Condition scaling.** Average instance count per cell at condition 0.1 / 0.5 / 0.9 is about 0.17 / 0.75 / 1.2 × the baseline (within 20%) over a large hand-built grid; at 0.0 it is at most 5% of baseline.
6. **Dead variants.** Below condition 0.22 a tree rule produces dead variants about 60% of the time (±10%) and never above 0.22.
7. **Slope rules.** A rule with `maxSlope` produces no instances on steeper terrain, and `needsWet` produces none on cells with the wet tag absent.
8. **Palette and rigidity.** Instances carry the class palette; `rigid` rules set the rigid flag; other instances sway.
9. **Dynamic layers.** A dynamic sprite at `(x, y)` is drawn at terrain height plus layer altitude; cloud and bird layers use the translucent layer; counts and draws are within DiaBgfx3D limits (64k).
10. **Density mapping.** A cell with precip 0.8 and snow fraction 0 gives rain density 0.8 and snow density 0; with snow fraction 1 the reverse; zero precip gives zero in both; burn intensity gives smoke density; fields are rewritten only when the channel version changes.
11. **Rain or snow by cell.** In a test world with a cold plateau and a warm lowland under one storm, snow falls on the plateau cells and rain on the lowland (particle kinds counted by region).
12. **No jitter on props.** Prop screen positions are stable frame to frame with the camera still (no boil, no frequency changes); a gust changes amplitude only.
13. **Budget.** Instance counts at Large with default rules and density 1.0 are logged; the cap of 64k per layer is respected (excess dropped lowest-priority first with a warning, never crashes).
14. **Observability.** Log channel `TMAP`; scopes `Props.Scatter`, `Props.Rebuild`; metrics `tmap.props`, `tmap.dynamic_sprites`, `tmap.scatter_ms`.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-004 | Mountains are terrain | No peak sprites in the default rules |
| TMAP-005 | Per-cell channels | Particle density comes from per-cell channels |
| TMAP-006 | No prop boil, fixed-frequency sway | AC 12 |
| CFB-001..003 | DiaCellFieldBake | Placement uses `OwnerAt` and baked tiles only |
| PD-001 | StringCRC ids | Prop types are indices into the sprite atlas; class ids are small ints; rule tables keyed by `StringCRC` at the CoW layer |
| PD-004 | No STL in public APIs | Raw rule arrays |

## Open Design Questions

1. **Instances per cell vs per chunk.** Rescattering by cell is clean, but instance buffers are best uploaded as one array. `SpriteInstanceSink` collects per cell and the renderer concatenates, so one changed cell re-uploads the whole static set. At 20k instances (720 KB) that is fine; if not, per-chunk buffers are an easy split.
2. **Overlaps.** Prop overlap is allowed (as in the prototype). A minimum-distance relaxation would look tidier but costs determinism complexity. Skip unless the look-dev review asks.
3. **Cloud altitude and shadows.** The cloud sprites' shadows come from the weather channel in Feature 2, not from the sprites. If sprite and shadow positions disagree visibly, the game must derive both from the same data.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Rule types and per-cell RNG | AC 1 | Not Started | sonnet | |
| 2 | `PropScatter::Rebuild`: count, position, validity, snap | AC 3, 4, 7 | Not Started | sonnet | |
| 3 | Condition scaling, dead variants, palettes, rigidity | AC 5, 6, 8 | Not Started | sonnet | |
| 4 | `RebuildCells` | AC 2 | Not Started | sonnet | |
| 5 | Dynamic sprite layers | AC 9 | Not Started | sonnet | needs DiaBgfx3D B |
| 6 | Density mapping to particles | AC 10, 11 | Not Started | sonnet | needs DiaBgfx3D C |
| 7 | Sway/stability test, budget, observability | AC 12, 13, 14 | Not Started | sonnet | |

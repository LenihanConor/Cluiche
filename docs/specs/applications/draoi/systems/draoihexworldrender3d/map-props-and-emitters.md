# Feature Spec: DraoiHexWorldRender3D Map Props and Emitters

## Parent System
@docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md

**Gameplay Domains:** rendering
**Depends on:** Hex Terrain Surface (Feature 1, `HeightAt`); DiaBgfx3D Instanced Sprites (B) and Particles (C); DiaCellFieldBake (`OwnerAt`)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/props.js` (`placeProps`, `BIOME_PROPS`), `js/app.js` (`buildClouds`), `js/alive.js` (`Fauna.instances`), `js/weather.js` (`Precip`)

## Status
`Approved` (2026-11-24, user, after spec review: every number is a live-tweakable style field, neutral flat density default, instances store class id with season palette looked up in the shader, per-class `wet` flag). Drafted 2026-11-20 from the approved prototype. **Moved 2026-11-23 (layer split):** was DiaTerrainMap3D Feature 3; namespace now `Draoi::HexWorldRender3D`. The condition-to-density curve and the dead-variant threshold become `HexWorldStyle` parameters with the prototype values as defaults, so CoW's ecology bands are data (TMAP-012). Particle snow and rain now read the Weather channel's snow fraction.

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
2. **Class rules** (data): per class, a list of `{ propType, density per cell, size, minimum slope, maximum slope, needsWet }`. `needsWet` tests a per-class `wet` flag in `HexWorldVisuals` (not the Water channel, so props do not depend on rivers).
3. **Condition scaling**: density follows a style curve of condition. The **neutral default is flat (1.0 at every condition)**, so Draoi names no ecology bands (TMAP-012). CoW's style file supplies the prototype curve (about 5% of baseline when Collapsed, 130% when Thriving). Trees degrade to dead trees below `deadBelow`.
4. **Placement validity** via `OwnerAt`/margin: never in water, never on a class border margin of 0 (avoid straddling), snap `z` to terrain `HeightAt`.
5. **Incremental rescatter**: when a cell's class or condition changes, only its instances are recomputed.
6. **Dynamic sprite layers** with per-frame instance building and ground snap.
7. **Particle density mapping**: weather channel → rain and snow density by the channel's snow fraction; events channel → smoke density.
8. **Fixed-frequency sway** with amplitude from per-instance rigidity and the weather gust channel (TMAP-006).
9. **Everything tweakable (TOON-006, user 2026-11-24).** Every number is a live-tweakable style field with a range and a neutral default: the scatter disc radius, the placement tries, the condition curve (breakpoints and slopes), `deadBelow`, `deadChance`, global size and jitter scales, sway frequency (one fixed value, never per-instance), sway amplitude and gust gain, the global prop `densityScale` (multiplies every rule's `perCell`, default 1; scatter-affecting), and the rain, snow and smoke density gains. Scalar per-rule fields (`perCell`, `size`, `sizeJitter`, slopes) are map attributes in `HexWorldVisuals`; adding or removing a rule is a data reload.
10. **Dirty-rescatter rule.** A change to any scatter-affecting field (curve, radius, tries, dead-variant numbers, a rule field, a class `wet` flag) marks the scatter dirty, and the next frame runs one deterministic `Rebuild`. Sway, particle gains and palettes apply immediately with no rescatter.

## Non-Goals

- Authoring prop art, palettes or rule tables (CoW Strategic Map Features 1 and 3).
- Simulating herds, clouds or fire (the game does; this feature draws instances it is handed).
- Collision, shadows, per-prop lighting.
- LOD billboards or impostors.

## Design

```cpp
namespace Draoi::HexWorldRender3D {
    struct PropRule { uint16_t propType; float perCell;         // expected count per cell at baseline
                      float size; float sizeJitter;             // world units tall
                      float minSlope, maxSlope; bool needsWet; uint16_t deadVariant; };
    struct ClassPropRules { const PropRule* rules; uint32_t count; };   // per class id

    struct ScatterInputs {
        const Dia::CellFieldBake::BakedFields* world;     // the whole baked world
        const uint8_t* primaryClass; const uint8_t* secondaryClass;      // per cell
        const float*   condition;                                         // per cell 0..1
        const ClassPropRules* rulesByClass; uint32_t classCount;
        uint32_t seed;                                                    // palettes are not inputs: instances carry the class id
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
- **Density scale.** A `HexWorldStyle` curve: three segments by condition, each `{ upTo, base, slope }`, value `base + slope * (c - segmentStart)`. **Neutral default: all three segments `base 1.0, slope 0`** (flat). The prototype's curve, which CoW's style file supplies, is: c < 0.33 → `0.05 + 1.2c`; < 0.66 → `0.45 + 1.67(c − 0.33)`; else `1.0 + 0.9(c − 0.66)`.
- **Position.** Uniform in a disc of `scatterRadius` (default 0.85) around the cell centre, accepted only if `OwnerAt` is that cell (up to `placementTries`, default 6), the point is land, and slope is within the rule's range. `z` from `HeightAt`.
- **Dead variants.** Rules with `deadVariant` switch to it when `condition < deadBelow` with probability `deadChance`, deterministic per instance. Neutral defaults: `deadBelow 0` (never), so Draoi imposes no ecology threshold; CoW's file sets 0.22 and 0.6.
- **Colour.** Each instance stores its **class id**, not colours. The material looks up the class palette (and the season's override) in the shader, so a season or palette change recolours props with no rescatter.
- **Dynamic layers.** `DynamicSprites` takes `{x, y, type, scale, palette, flags}` and fills `z` from `HeightAt` (lifted by a per-layer altitude for clouds and birds), then submits to the translucent or cutout layer.
- **Particle mapping.** Per-cell channels → `DensityField` at cell resolution: rain density = `precip × (1 − snowFraction)`, snow density = `precip × snowFraction`, smoke density = burn intensity. The `DensityField` is rewritten when the channel changes.

## Acceptance Criteria

1. **Determinism.** Two scatters of the same inputs give identical instance lists (byte-identical); different seeds give different lists; the result is independent of thread count and cell iteration order.
2. **Locality.** `RebuildCells` for K cells changes only instances belonging to those cells; the result equals a full rebuild with the same inputs.
3. **No water.** Over synthetic hex worlds built in the test (islands, lakes, coasts; no CoW presets, so Draoi tests stay CoW-free), no instance has `OwnerAt` pointing to a water cell or `kNoOwner`.
4. **Terrain snap.** Every instance's `z` equals `HeightAt(x, y)` within half precision.
5. **Condition scaling.** With the neutral default curve, average instance count per cell is the same at every condition (within 20%). With the prototype curve set explicitly, it is about 0.17 / 0.75 / 1.2 × the baseline at condition 0.1 / 0.5 / 0.9 (within 20%) over a large hand-built grid, and at 0.0 at most 5% of baseline.
6. **Dead variants.** With `deadBelow 0.22` and `deadChance 0.6` set explicitly, a tree rule below 0.22 produces dead variants about 60% of the time (±10%) and never above 0.22. With the neutral default, none.
7. **Slope rules.** A rule with `maxSlope` produces no instances on steeper terrain, and `needsWet` produces none on cells whose class has the `wet` flag clear.
8. **Class id and rigidity.** Instances carry the class id (a season or palette change recolours them with no rescatter, counted); `rigid` rules set the rigid flag; other instances sway.
9. **Dynamic layers.** A dynamic sprite at `(x, y)` is drawn at terrain height plus layer altitude; cloud and bird layers use the translucent layer; counts and draws are within DiaBgfx3D limits (64k).
10. **Density mapping.** A cell with precip 0.8 and snow fraction 0 gives rain density 0.8 and snow density 0; with snow fraction 1 the reverse; zero precip gives zero in both; burn intensity gives smoke density; fields are rewritten only when the channel version changes.
11. **Rain or snow by cell.** In a test world with a cold plateau and a warm lowland under one storm, snow falls on the plateau cells and rain on the lowland (particle kinds counted by region).
12. **No jitter on props.** Prop screen positions are stable frame to frame with the camera still (no boil, no frequency changes); a gust changes amplitude only.
13. **Budget.** Instance counts at Large with default rules and density 1.0 are logged; the cap of 64k per layer is respected (excess dropped lowest-priority first with a warning, never crashes).
14. **Observability.** Log channel `HWR3D`; scopes `Props.Scatter`, `Props.Rebuild`; metrics `hwr3d.props`, `hwr3d.dynamic_sprites`, `hwr3d.scatter_ms`.
15. **Live tweak.** Changing a scatter-affecting field on the style or visuals attributes causes exactly one `Rebuild` on the next frame and the result equals a fresh scatter with those values; changing sway, gust or particle gains causes no rescatter (counter) and takes effect next frame. Every field in goal 9 has a range and a neutral default in its sidecar.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-004 | Mountains are terrain | No peak sprites in the default rules |
| TMAP-005 | Per-cell channels | Particle density comes from per-cell channels |
| TMAP-006 | No prop boil, fixed-frequency sway | AC 12 |
| CFB-001..003 | DiaCellFieldBake | Placement uses `OwnerAt` and the baked world only |
| TMAP-012 | Neutral defaults; numbers are game data | Density curve and dead-variant numbers are style parameters, neutral by default (flat curve, no dead variants) |
| TOON-006 / TMAP-013 | Live-tweakable attributes | Goal 9, 10; AC 15 |
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
| 8 | Style fields, sidecars, dirty-rescatter rule | AC 15 | Not Started | sonnet | do with task 1; neutral defaults per goal 3 |

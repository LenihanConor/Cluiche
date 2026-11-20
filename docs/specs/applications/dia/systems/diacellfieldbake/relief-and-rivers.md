# Feature Spec: DiaCellFieldBake Relief and Rivers

## Parent System
@docs/specs/applications/dia/systems/diacellfieldbake/diacellfieldbake.md

**Gameplay Domains:** rendering
**Depends on:** @docs/specs/applications/dia/systems/diacellfieldbake/fields-and-classes.md (Feature 1)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/world.js` (`MOUNTAIN_WEIGHT`, `ridged`, relief in `buildFields`, `buildRivers`)

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

Feature 1 gives smooth fields, but smoothed elevation alone is too gentle: mountains read as bumps and lakes as pits. Feature 2 adds the two remaining things the prototype needed to look right:

1. **Relief.** Extra height on mountain-like terrain classes, shaped by ridged noise, so ranges read as crags and spurs. Mountains are terrain, not sprites (TMAP-004).
2. **Rivers as a distance field.** River cells and their flow direction become wiggled segments, rasterised into a per-texel signed distance, width and flow direction, so a renderer can draw water, inked banks and animated flow from one texture.

It also defines how **lake surface height and ocean depth** compose into the elevation layer the bake smooths, so that lakes are flat and the sea falls away below zero.

## Problem

- S-1 elevation is smooth and low. Without added relief, Mountain and Peak cells look like gentle hills at 3D tilt.
- Hex-aligned river polylines look like zig-zags. They need a deterministic hand-drawn wiggle and a width driven by flow accumulation.
- Lakes drawn from raw cell elevation sag at their edges, and ocean cells at height 0 sit flush with the coast. The caller needs a defined convention for both.

## Goals

1. **Relief by terrain class**, with weights supplied as data and a ridged-noise shape, added to one designated scalar (elevation).
2. **Flat lakes.** A lake cell's elevation is its surface height, and interior lake texels come out flat.
3. **Sea below zero.** Ocean cells map their depth below zero through a base and a scale.
4. **River field** per texel: signed distance to the channel edge, half-width, and flow direction, ready for upload as RGBA8.
5. **Tile independent.** Relief and rivers are as tile-safe as Feature 1: any rectangle gives the same bytes as the whole bake.

## Non-Goals

- **Water simulation or flow state.** Width by flow state (Dry/Low/Normal/Flood/Frozen) is a render-time multiplier in DiaTerrainMap3D. The field stores the base width from flow accumulation.
- **River routing.** S-1 owns the network (`flowDirection`, `riverMask`, `flowAccumulation`). This feature draws what it is given.
- **Lake extent changes over time.** The per-frame water channel does that. The field stores the static shore.
- **Erosion, rivers carving the terrain.** S-1's `valleyCarve` is already in elevation.

## Design

### Relief

1. Input: a per-cell terrain class `uint8_t` (`reliefClass`) and `ReliefTable { float weight[32]; }` (class id to weight; 0 = none).
2. During the Feature 1 neighbourhood loop, also accumulate `mnt = Σ wᵢ · weight[reliefClass[i]] / Σ wᵢ`.
3. After the scalar average: `elevation += mnt · (0.35 + 0.65 · ridged(wx · 0.9, wy · 0.9)) · reliefScale`, with `reliefScale` default 0.36.
4. `ridged(x, y)`: four octaves, amplitude 0.55 halved each octave, frequency ×2.1, each octave `1 − |2·valueNoise − 1|`, normalised by 0.95 and clamped to 1.
5. The warped point `(wx, wy)` is the same one Feature 1 used for that texel, so relief and class borders stay coherent.
6. `BakeParams::reliefScalar` selects which scalar gets the relief (default 0, the elevation).

### Elevation composition (helper)

```cpp
struct ElevationSpec {
    const float*   elevation;        // per cell; lake cells hold their surface height
    const float*   oceanDepth;       // per cell 0..1, may be null
    const uint8_t* primaryClass;     // to find sea cells
    uint8_t        seaClass;         // primary class id of the sea
    float          seaBase = -0.05f; // elevation of a zero-depth sea cell
    float          seaScale = -0.25f; // elevation per unit depth (negative = lower)
};
void ComposeElevation(const ElevationSpec&, uint32_t cellCount, float* out);
```

Sea cells: `out = seaBase + seaScale × depth`. Every other cell, including lake cells, passes `elevation` through, because S-1 already stores lake surface height there.

### Rivers

1. Inputs: `riverMask`, `flowDirection` (0..5 into the hex neighbour table), `flowAccumulation`.
2. **Segments.** For each river cell with a valid downstream neighbour `m`:
   - `halfWidth = min(0.24, 0.05 + 0.03 · log2(1 + flowAccumulation))` cell units.
   - Build the polyline from the cell centre to `m`'s centre in 8 steps. Step `t = s/8` is offset perpendicular by `sin(πt) · (valueNoise(ax·3.1 + t·2.3, ay·3.1) − 0.5) · 0.7`. The last step has zero offset, so segments join at cell centres.
   - Each sub-segment stores its endpoints, half-width and the unit flow direction of the whole cell-to-cell leg.
3. **Rasterise.** Each texel takes the segment with the smallest signed distance `d = dist(point, segment) − halfWidth`, within `MAXD = 0.6` cell units. Segments are visited in index order and a strictly smaller distance wins, so ties are stable.
4. **Pack** into RGBA8: R = `(d + 0.3) / 0.9` quantised to 0..255 (clamped), G = `halfWidth / 0.3`, BA = flow direction unit vector mapped from [−1, 1].
5. **Tile independence.** Per region, only segments within `MAXD` of the region are rasterised, but the texel result is a function of all segments in index order, so it never depends on the region.

### Data

```cpp
struct ReliefInputs { const uint8_t* reliefClass; ReliefTable table; };
struct RiverInputs  { const uint8_t* riverMask; const uint8_t* flowDirection; const float* flowAccumulation; };

// Extends Feature 1:
struct BakeInputs { /* ... */ const ReliefInputs* relief = nullptr; const RiverInputs* rivers = nullptr; };
struct BakeParams { /* ... */ float reliefScale = 0.36f; uint32_t reliefScalar = 0; };
struct BakedFields { /* ... */ uint8_t* river = nullptr;   // RGBA8 per texel, or null when no rivers }
```

`RiverNetwork` is built once per world (`BuildRiverNetwork(grid, RiverInputs, params)`), is immutable, and is shared by every `BakeRegion` call.

## Acceptance Criteria

1. **Relief off by default.** With no `ReliefInputs`, Feature 1's output is byte-identical to a bake without Feature 2 compiled in.
2. **Relief on weighted classes only.** With one mountain cell in a flat field, texels within the cell's neighbourhood rise above the smooth value; texels more than 4 cell units away are unchanged (within half precision).
3. **Relief range.** Added height is between 0.35 × mnt × scale and 1.0 × mnt × scale (the ridged factor 0.35..1.0). With all-mountain input, added height is within [0.126, 0.36] at `reliefScale` 0.36.
4. **Ridged noise.** `ridged` is within [0, 1], repeatable, and changes with the seed.
5. **Elevation composition.** Sea cells compose to `seaBase + seaScale × depth`; deeper sea is strictly lower; lake and land cells pass through unchanged. Null `oceanDepth` composes sea cells to `seaBase`.
6. **Flat lakes.** In a bake with a lake of surface height h, texels more than one ring inside the lake are within 1e−3 of h.
7. **River segments.** A river cell with a downstream neighbour yields 8 sub-segments whose endpoints chain continuously, with the first at the cell centre and the last at the downstream centre. A river cell with no valid downstream yields none.
8. **Half-width.** Half-width increases with flow accumulation and is capped at 0.24.
9. **Distance field.** A texel on a segment's centre line has a negative signed distance (inside the channel); a texel more than `MAXD` from every segment decodes to ≥ `MAXD`. The distance is continuous across cell boundaries along the river.
10. **Tile independence.** Baking tiles in any order and stitching gives output byte-identical to the whole bake, including the river field. Hash covers the river array.
11. **Determinism.** Same inputs and seed give the same segments and the same hash. Changing the seed changes the wiggle.
12. **No rivers.** `rivers == nullptr` leaves `BakedFields::river` null and costs no memory.
13. **Observability.** The Feature 1 log, scope and metrics gain a river segment count and river build ms.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| CFB-002 | Class borders from majority + margin | Relief reads the same warped point as the class decision, so relief never contradicts class borders |
| CFB-003 | Deterministic, hashable at any thread count | All new arrays are in `Hash()`; the river field is tile independent (AC 10) |
| TMAP-004 | Mountains are terrain relief | This feature is where relief comes from |
| PD-004 / AD-002 | No STL in public APIs | Raw pointers and fixed tables; `RiverNetwork` holds a `DynamicArrayC` of segments internally |

## Open Design Questions

1. **Relief weights per class are CoW data.** The CoW weights (`Mountain` 1.0, `Peak` 1.45, `Volcano` 1.3, `Hills` 0.22, `Highland` 0.18, `Cliff` 0.15) came from the prototype. They belong in the Strategic Map's visual table (Strategic Map Feature 1), not here. Confirm there that they carry over unchanged.
2. **River width stored as 8 bits.** A 0..0.3 range in 256 steps gives about 0.001 resolution, fine for drawing. If the renderer wants width by flow state with sub-texel accuracy at close zoom, the width may need 16 bits.
3. **Where do river mouths end?** Rivers end at the downstream cell centre, which is inside the sea or lake cell. Renderers hide the mouth under water, as the prototype does. If a delta is wanted later it is a new feature.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `ridged` noise and `ReliefTable` | AC 4 | Not Started | sonnet | |
| 2 | Relief accumulation in the bake loop | AC 1, 2, 3 | Not Started | sonnet | |
| 3 | `ComposeElevation` | AC 5, 6 | Not Started | sonnet | |
| 4 | `BuildRiverNetwork`: segments, wiggle, half-width | AC 7, 8, 11 | Not Started | sonnet | |
| 5 | River rasterisation per region and packing | AC 9, 10, 12 | Not Started | sonnet | |
| 6 | Hash and observability additions | AC 10, 13 | Not Started | haiku | |
| 7 | S-1 golden + prototype parity capture (mountains, lakes, rivers) | user sign-off | Not Started | sonnet | extends Feature 1 task 13 |

# Feature Spec: Terrain Generator

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md
**Design source:** GDD `02_WorldLore/GeographyAndRegions.md` (Locked 2026-07-17), Parts 1, 2 and 11; GDD `10_PrototypeStrategy/S1_WorldGeneration.md` (DesignDocs repo)

> Supersedes the Placeholder Generation Algorithm (renamed from `placeholder-generation-algorithm.md`, 2026-10-15). Biomes moved to Feature 8 (Biome Classification) and regions and sites to Feature 9 (Regions and Sites).

**Gameplay Domains:** simulation

## Summary

The Terrain Generator builds the physical world of a CoW map from a seed and a set of designer sliders (`WorldShapeParams`). Fixed rules from the GDD decide the shape:

- a main continent with a coastline, 1–2 peninsulas and an island chain offshore, one island volcanic;
- a mountain spine or ring with passes, and a second deep barrier around the interior;
- rivers that start in the highlands, run downhill to the sea and carve valleys;
- lakes that fill basins;
- depth zones (First Shores, Wild Reaches, Ancient Interiors) that follow from the continent's shape.

Dia seeded noise (dia-needs #15) is only raw material inside these rules.

The generator is a staged pipeline that caches each stage's result. Each slider names the first stage it affects. When a slider moves, the pipeline re-runs from that stage in the same SimPU frame, so S-1 works as a live terrain editor (SD-S1-001, SD-S1-004). Sliders are thresholds and weights over fields the seed has already fixed, so each one adds or removes only what it controls. More lakes fills more of the basins that are already ranked. More rivers lowers the flow threshold, so tributaries appear on the existing rivers. Nothing reshuffles.

The output is terrain only: per-cell elevation, latitude, water source, terrain type, depth zone and landmass, plus a named layer for every stage result so the debug view can show how the world was built. Biomes (Feature 8) and regions and sites (Feature 9) are built on top of it.

## Problem

- **The placeholder looked like noise.** Near-random biomes gave A4 no coasts or mountain ranges, so it could not show coastal rain, rain shadows or snow. Designers could not judge "does this feel like the Far Ranges?"
- **Tuning by reroll is not shaping.** If moving a slider reshuffles the whole map, the designer can't make a small change. Each slider has to change only what it names.
- **Hidden rules can't be trusted.** If the designer can't see why a river went where it did, they can't trust the tool. Every stage needs a visible result.
- **Downstream needs fixed inputs.** Feature 8 needs elevation, latitude and water. Feature 9 needs depth zones, landmasses, passes and barriers. Feature 2's navigation table needs terrain types. A4 needs elevation, latitude, water and hex topology.

## Goals

1. **Looks and reads like a world.** Every map has the GDD's structure (continent, coast, peninsulas, islands, spine or ring, second barrier, rivers, lakes, depth zones), and the shape checks in the Acceptance Criteria measure it.
2. **Live shaping.** Staged pipeline with cached stage results. A slider re-runs from its first affected stage, and the whole terrain run takes under 20 ms at Small (66 × 42) in Release.
3. **Stable sliders.** Each slider has a stated stability rule (additive, prefix, monotone or rebalancing), and each rule has a test.
4. **Deterministic.** Seed + `WorldShapeParams` → byte-identical terrain. A live-preview run (stages reused from cache) is byte-identical to a fresh Regenerate.
5. **Transparent, not tweakable.** The rules are fixed in code and documented below. Their constants live in a versioned tuning file, and every stage result is a named layer the debug view can show. Rule constants are not exposed in the UI.
6. **Presets as assets.** Versioned JSON preset files under `Cluiche/Assets/CoW/`, three starting presets, and the DiaAPI commands to list, load and save them (SD-S1-011).
7. **Clear hand-off.** The `Cell` terrain fields that Feature 8, Feature 9, Feature 2 and A4 consume are defined here.

## Non-Goals

1. **Biomes.** Feature 8. This feature reports no temperature or moisture.
2. **Regions, nodes and sites** (Hearths, Nexus, thrones, dynasty starts, corridor nodes, migration routes). Feature 9.
3. **Validation and retry.** Features 2 and 3. Retry never reshapes terrain (SD-S1-008).
4. **Editable rules.** Rule constants change only by editing the tuning file and restarting, never from the panel.
5. **The panel and the drawers.** Features 6 and 7 build them on the commands and layers defined here.
6. **Per-size tuning.** Any size from 1×1 to 128×128 runs. Small is the dev default; Standard and Large are acceptance targets (amended 2026-10-19, SD-S1-006). Tuning is measured at Small.
7. **Cross-build bit identity.** Determinism is within one build of the game. Exported worlds store values, not recipes, so nothing has to recompute bit-exactly on another build.
8. **Erosion simulation, plate tectonics and climate.** The generator uses simple rules, not physics.

## Architecture

### Coordinates and conventions

- **Grid.** Dia `HexTopology` (flat-top, odd-q, no wrap). The cell index is `y * width + x`, and neighbours come from `NeighbourAt(i, d)`, with `d = 0..5` in Dia's order and `kNoCell` past the edge. Distances are `HexTopology::Distance` (hex steps). The generator defines no hex maths of its own.
- **Map space.** Shape rules work in map space: `p = CellCenter(c, 1) / maxCentreY`. The map is 1 unit tall and `A` units wide (`A ≈ 1.32` at Small), so shapes are not stretched by the map's aspect ratio.
- **Elevation.** Elevation is in `[0, 1]`. Ocean is exactly `0`. Land is in `[kCoastElev, 1]` (`kCoastElev = 0.02`). A lake cell holds its lake's surface height.
- **Constants.** `k…` names are rule constants from the tuning file (see Tuning file). The values in this spec are the starting defaults.
- **Ties** are always broken by lowest cell index, then by lowest direction `d`.

### Stage pipeline

The stages form a small DAG. Each stage caches its outputs. A change dirties every stage that reads the changed slider, plus everything downstream of those stages. The generator then re-runs only the dirty stages, in order. A stage reads only its declared inputs (upstream stage outputs, the seed fields and its own sliders), so a cached result always equals a fresh one.

| # | Stage | Reads | Sliders | Output layers |
|---|-------|-------|---------|---------------|
| 0 | SeedFields | seed | (seed) | none; caches noise seeds and ranked candidate lists |
| 1 | Landmass | 0 | Land, Continents, Islands, ContinentIslands, Peninsulas, Coastline | `landPotential`, `islandPotential`, `landmass`, `coastDistance`, `volcanic` (island vent) |
| 2 | Mountains | 0, 1 | Mountainousness, SpineToRing, Orientation, Passes | `ridgeMask`, `passMask` |
| 3 | Barrier | 0, 1 | Barrier, BarrierForm | `barrierMask`, `volcanic` (chain vents, merged) |
| 4 | Elevation | 0, 1, 2, 3 | Ruggedness | `elevationRaw`, `oceanDepth` |
| 5 | Drainage | 4 | none | `filledElevation`, `basinDepth`, `basinRank`, `flowDirection`, `flowAccumulation` |
| 6 | Rivers | 4, 5 | Rivers | `riverMask` |
| 7 | Lakes | 5 | Lakes | `lakeMask` |
| 8 | DepthZones | 1, 2, 3, 6 | none | `zoneCostBase`, `zoneCost`, `depthZone` |
| 9 | Valleys | 4, 5, 6, 7 | ValleyDepth | `valleyCarve`, `elevation` |
| 10 | Classify | 1, 2, 3, 6, 7, 8, 9 | none | `slope`, `terrainType`, `waterSource` |
| 11 | Latitude | grid | LatitudeNorth, LatitudeSouth | `latitude` |

**Live preview vs Regenerate.** Both call the same `TerrainGenerator::Update`. Regenerate first calls `Invalidate()`, so every stage runs. A live slider change runs only the dirty stages. Each run records how long each stage took (`StageTiming`), and `cow.worldgen.layers` reports these timings for the panel. Live preview stops after stage 11. Feature 8 then runs (it is also live). Feature 9, validation and retry run only on Regenerate (SD-S1-004).

### Stage 0: SeedFields

At the start of stage 0, the generator reseeds the registered `WLD` stream (`RandomStreamManager`, F-2) from the world seed. It then makes a **fixed number of draws in a fixed order**, so the number of draws never depends on any slider:

1. Noise seeds, one integer per channel: `warp`, `coast`, `ridged`, `rugged`, `barrierWobble`, `vent`.
2. Continent candidates `k = 0..2`. Each candidate is a centre, an ellipse axis ratio in `[0.65, 1.0]` and an angle. Candidate 0 lies within 0.12 of the map centre. Candidates 1 and 2 lie at least 0.45 from candidate 0 and from each other, and at least 0.2 from the edges (rejection sampling with a fixed draw budget: on exhaustion, the furthest of the tries is used).
3. Peninsula candidates `j = 0..3`: angles around continent 0, at least 60° apart.
4. Island candidates `i = 0..23`: map-space points, each with 1–4 blob offsets (≤ 2.5 cells) and blob radii (1.5–3.5 cells).
5. The spine's lateral offset, in `[-0.15, 0.15]` of the continent's inradius.
6. Pass candidates: 8 curve parameters `t` in `[0.12, 0.88]`, in rank order. There is no spacing rule here, because the needed spacing depends on the ridge width (a slider), so stage 2 enforces it.

All later randomness is Dia seeded noise (dia-needs #15) sampled with these seeds. Noise is a pure function of (seed, x, y), so later stages never touch the RNG stream.

### Stage 1: Landmass

1. **Continent shape.** For each active continent `k < Continents`, `blob_k(p) = w_k · (1 − d_k(p) / R_k)`:
   - `d_k` is the distance from the candidate centre in the candidate's ellipse frame, measured after domain warping: `p' = p + Coastline · kWarpAmp · warpNoise(p)`.
   - Weights `w = (1.0, 0.75, 0.6)` and radii `R = (0.55, 0.36, 0.30)`. The main continent always dominates.
2. **Peninsulas.** For each `j < Peninsulas`, add a lobe on continent 0: an ellipse centred at `c0 + 0.62 R0 · dir(φ_j)`, long axis `0.75 R0` along `φ_j`, short axis `0.2 R0`, with value `w_0 · (1 − ellipseDist)`.
3. **Potential.** `M(p) = max(all blobs and lobes) + Coastline · kCoastAmp · (2 · fBm_coast(p) − 1)`. Cells within `kBorderWidth = 2` steps of a map edge get `M = −∞`, and cells 2–6 steps from the edge lose up to `kEdgeFade = 0.5` linearly, so the coast never runs along the edge. This is the `landPotential` layer.
4. **Land budget.** The total land is `L = round(Land · N)` cells (`N` = all cells). `ContinentIslands` splits it: the island budget is `K = round(L · clamp(1 − ContinentIslands, kMinIslandShare = 0.02, kMaxIslandShare = 0.95))`, or 0 when `Islands = 0`.
5. **Reference mainland.** The `L` cells with the highest `M` (ties by index), cleaned up as in step 7, are the reference mainland `M_ref`. This is the mainland as it would be with no island budget. It depends on `Land`, `Continents`, `Peninsulas` and `Coastline`, but never on `ContinentIslands`, so islands are placed against a fixed coast.
6. **Islands.** Walk the island candidates in rank order and accept candidate `i` if:
   - its centre cell is outside `M_ref`;
   - its hex distance to `M_ref` is in `[kIslandGapMin = 2, kIslandGapMax = 7]`;
   - it is at least `kBorderWidth + 1` steps from the edge;
   - it is at least `kIslandSpacing = 6` steps from every island already accepted.

   Stop after `Islands` accepted groups. Fewer are accepted if the candidates run out; the number accepted is reported as `effective.islands`.
   - **Island potential.** `I(c) = max over the accepted groups' blobs of (1 − dist(c, blob) / r_blob)` (`islandPotential` layer). It is positive only inside a blob.
   - **Eligible cells:** `I > 0`, outside `M_ref`, not next to `M_ref`, and at least `kBorderWidth + 1` steps from the edge. Because `I > 0` confines every island to its own blobs, islands never grow into a second continent and groups never merge, whatever the budget.
   - **Island cells.** The `min(eligible, max(K, 3 · groups))` eligible cells with the highest `I` become island land (ties by index). Every blob centre has `I = 1`, so every group gets its centre first.
   - **Overflow.** The island budget is a *preference* bounded by island capacity (the eligible cells). Any unplaced budget returns to the mainland. At the defaults, capacity is roughly 15–25% of land, so `ContinentIslands` below about 0.8 saturates there. `effective.islandShare` reports the share actually reached, and the panel shows it next to the slider.

   The first accepted group is the **volcanic island**: its centre cell is a vent (`volcanic` layer).
7. **Mainland and clean-up.** The `L − placed island cells` cells with the highest `M` are the mainland. Then:
   - any mainland component smaller than `kMinLandmass = 6` cells becomes ocean;
   - any ocean component not connected to the border ocean becomes land (an enclosed sea becomes a lowland basin, which the Lakes stage may refill).

   Island cells are exempt from clean-up. The mainland is always a subset of `M_ref`, so islands never touch it. Total land is `L` (±0.01 after clean-up) at every `ContinentIslands` value.
8. **Landmasses.** Label the land components (`landmass` = 1, 2, …, numbered by smallest cell index). The **main landmass** is the component that contains the mainland's highest-`M` cell (continent 0's peak), **not** the largest component. That cell is never removed by clean-up, so continent 0 stays the main landmass at every slider value, including when `ContinentIslands` or `Continents` would otherwise make another component bigger. `WorldStructure::mainLandmass` holds its id.
9. **Coast distance.** Multi-source BFS gives each land cell its steps to the nearest ocean cell (coast cells = 1), and each ocean cell its steps to the nearest land cell (`coastDistance`).

### Stage 2: Mountains (spine or ring)

Mountains are built on the main landmass only, around its centre `C`: the centroid of the landmass's cell centres, moved to the nearest main-landmass land cell (ties by index) when the centroid is not on land (a horseshoe or peninsula-heavy continent can have its centroid in the sea).

1. **Reach.** `reach(φ)` is the distance from `C` along direction `φ` to the first ocean cell (march in steps of 0.25 cells, read the cell with `CellFromPoint`). The curve is scaled by `reach`, so it stays inside the land.
2. **Curve.** For a curve parameter `t ∈ [0, 1]` and `θ = Orientation`:
   - Line: `L(t) = C + n·offset + (2t − 1) · kSpineReach · reach(θ or θ+180°) · dir(θ)`, where `n` is the normal to `θ` and `offset` comes from stage 0. The line reaches `kSpineReach = 0.8` of the way to the coast at each end.
   - Ring: `G(t) = C + kRingReach · reach(φ) · dir(φ)`, with `φ(t) = θ − 90° − 360° · t` and `kRingReach = 0.6`.
   - Blend: `P(t) = lerp(L(t), G(t), SpineToRing)`. At 0 this is a straight spine along `θ`. As the slider rises the spine bends into a horseshoe whose open side faces `θ − 90°`. At 1 its two ends meet and it closes into a ring.
3. **Mask.** `ridge(c) = max(0, 1 − d_curve(c) / w)`, where `d_curve` is the distance from the cell centre to the polyline (256 segments). The width is `w = lerp(kRidgeWidthMin = 1.2, kRidgeWidthMax = 2.8, Mountainousness)` cells. Off the main landmass the mask is 0.
4. **One range.** Ridge cells are those with `ridge ≥ kRidgeCellMin = 0.35`. Keep only the largest connected group of ridge cells, and zero the mask everywhere else.
5. **Passes.** Let `g = 0.7 · w + 0.5` cells (the half-width of a gap, wide enough that no ridge cell bridges it). Walk the pass candidates in rank order and accept candidate `j` only if the arc length along `P` between `P(t_j)` and every accepted pass is at least `2g + kMinPiece` (`kMinPiece = 2` cells), so every range piece between two passes survives. Stop after `Passes` accepted passes. Fewer are accepted if the curve is too short to fit them; the number accepted is reported as `effective.passes`. For each accepted pass, multiply the mask by `kPassFloor = 0.2` on every cell within `g` cells of `P(t_j)`. These cells set `passMask`. The final mask is the `ridgeMask` layer.

### Stage 3: Second barrier

The second barrier creates the deep interior. It is a closed band on the main landmass at a fixed depth inland.

1. `b* =` the `kBarrierQuantile = 0.75` quantile of `coastDistance` over main-landmass cells.
2. `band(c) = max(0, 1 − |D(c) + kBarrierWobble · (2 · noise_barrier(p) − 1) − b*| / kBarrierHalfWidth)`, with `kBarrierWobble = 1.5` and `kBarrierHalfWidth = 1.5`.
3. **The `barrierMask` layer depends on the form:**
   - **Plateau:** `barrierMask = max(band, kPlateauInner · [D > b*])`, with `kPlateauInner = 0.5`. The result is a raised rim with a high interior table, which can later become rainforest or desert in Feature 8.
   - **Volcanic chain:** `barrierMask = 0.5 · band`. Vents go on band cells that are local maxima of `noise_vent`, at least `kVentSpacing = 4` apart.
4. If `Barrier = 0` there is no barrier: the mask and vents are empty.

The biome of the barrier (rainforest, desert or volcanic) is decided by Feature 8. This stage only shapes the land.

### Stage 4: Elevation

For each land cell (`D = coastDistance`, `p` = map position):

```
base    = kLowlandRise · (1 − exp(−(D − 1) / kInlandScale))                 // 0.25, 6
mount   = Mountainousness · kMountainHeight · ridge^1.5 · (0.65 + 0.35 · ridged(p))   // 0.6
barrier = Barrier · kBarrierHeight · barrierMask                             // 0.35
vents   = kVentHeight · max(0, 1 − dist(c, nearest vent) / kVentRadius)      // 0.5, 2 cells
rugged  = Ruggedness · kRuggedAmp · (2 · fBm_rugged(p) − 1) · min(1, (D − 1) / 2)    // 0.12
elevationRaw = clamp(kCoastElev + base + mount + barrier + vents + rugged, kCoastElev, 1)
```

- Ocean cells are `0`.
- `oceanDepth = min(1, coastDistance / kShelfScale)` (with `kShelfScale = 6`) is a layer only, used for Relief shading. A4 sees ocean as elevation 0.
- Ruggedness fades out over the first two cells inland, so coasts stay low.

### Stage 5: Drainage

1. **Fill.** Priority-flood depression filling (seeded from every ocean cell, ε-increment `1e-5` per step, ties by cell index) gives `filledElevation`. In it, every land cell has a strictly descending path to the ocean.
2. **Basins.** `basinDepth = filledElevation − elevationRaw`, treating values below `1e-4` as 0. Each connected group of cells with depth > 0 is a basin, with a volume (sum of depths), a maximum depth and a spill height (the fill level).
   - **Eligible:** max depth ≥ `kMinBasinDepth = 0.01` and at least 2 cells.
   - **Rank:** volume (largest first), then smallest cell index. `basinRank` is 1-based; 0 = not an eligible basin.
3. **Flow direction.** Each land cell drains to the neighbour with the largest drop on `filledElevation` (ties by lowest `d`), and the neighbour is always ocean or lower land. Ocean cells are sinks (`flowDirection = 6`).
4. **Flow accumulation.** `acc(c) = (1 + kHighlandRain · elevationRaw(c)) + Σ acc(upstream)`, summed in order from the highest filled cell to the lowest. Highland cells contribute more water (`kHighlandRain = 2`).

### Stage 6: Rivers

1. **Threshold.** `T = kFlowMax · (kFlowMin / kFlowMax)^Rivers`, with `kFlowMax = 400` and `kFlowMin = 12`. Every land cell with `acc ≥ T` is a river candidate. `Rivers = 0` means no rivers.
2. **River trees.** Candidates group into trees by the ocean cell they drain into. A whole tree is kept only if:
   - its highest source (a candidate with no candidate upstream) is at `filledElevation ≥ kSourceMinElev = 0.3`, so rivers start in the highlands. It uses the filled surface so the test is monotone: a lower threshold only adds upstream cells, and filled elevation never falls going upstream, so a kept tree stays kept (AC10);
   - its longest source-to-mouth run is at least `kMinRiverLength = 4` cells.

   Kept cells form the `riverMask` layer.
3. **Rivers run through cells.** Each river cell's `downstream` is its flow direction, so every river runs cell to cell to the sea, matching A4's per-cell `waterSource`. A river that enters a lake is drawn as lake inside it (Classify) and continues from the lake's spill cell.

On the filled surface, downstream accumulation is always at least upstream accumulation. So every cell below a river cell is also a river cell, a lake cell or the sea, and every river reaches the sea or a lake by construction.

### Stage 7: Lakes

Fill the first `min(Lakes, eligible)` basins by rank. A lake covers every cell of its basin, its surface is the basin's spill height, and its outflow is the river or flow path leaving the spill cell. The lakes form the `lakeMask` layer. Unfilled basins stay dry hollows: the river crosses them on the filled surface, and the Valleys stage cuts its outlet. Lakes do not change depth zones.

### Stage 8: Depth zones

Depth zones come from the continent's shape: how far a cell is from the coast, counting barriers as costly to cross and river valleys as corridors through them (GDD Part 3: rivers cut through barriers at valleys).

1. **Base cost** (no rivers). Dijkstra over the land and lake cells of each landmass, with the coast cells as sources (cost 1). Entering cell `c` costs `1 + kRidgeCost · ridge(c) + kBarrierCost · Barrier · barrierMask(c)`, with `kRidgeCost = kBarrierCost = 4`. This is the `zoneCostBase` layer.
2. **Thresholds**, from the base cost of the main landmass:
   - `t1` is the highest cost in the lowest `kFirstShoresShare = 0.45` of its cells;
   - `t2` is the lowest cost in the highest `kAncientShare = 0.15`.
   - Costs tie often (flat rings of equal coast distance), so the shares are approximate. If `t2 ≤ t1`, `t2` moves up to the next distinct cost above `t1`. If no such cost exists, the map has no Ancient Interiors.

   Rivers never move the thresholds.
3. **Zone cost** (with rivers). The same Dijkstra, except that entering a river cell (`riverMask`) costs `1 + kRiverCorridor · (ridge and barrier terms)`, with `kRiverCorridor = 0.25`. A river valley is a cheap way through a barrier. This is the `zoneCost` layer, and it is never above `zoneCostBase`.
4. **Zones.** First Shores where `zoneCost ≤ t1`. Ancient Interiors where `zoneCost ≥ t2` **and** coast distance `D ≥ kAncientMinDepth = 3`, on the main landmass only (a cell that fails the depth test is Wild Reaches). Wild Reaches everywhere else. Other landmasses (extra continents and islands) get First Shores or Wild Reaches by `t1`, so **Ancient Interiors appear only on the main landmass.**
5. Ocean cells are `DepthZone::None`. A lake cell has the zone of its position.

**River monotonicity.** As `Rivers` rises, the river cell set only grows (stage 6). So entry costs only fall, `zoneCost` only falls, and the thresholds stay fixed. Zone edges move only inland, along river corridors: a cell can go Ancient Interiors → Wild Reaches → First Shores, never back.

### Stage 9: Valleys

1. **Carving.** Each kept river cell is lowered by `carve = ValleyDepth · kCarveMax · size`:
   - `kCarveMax = 0.12`;
   - `size` rises from 0.4 to 1.0 with `log(acc / T)`, reaching 1.0 at `kCarveFullAcc = 8 · T`.

   Land cells one step from a river are lowered by `0.5×` the deepest adjacent carve, and cells two steps away by `0.2×` (`valleyCarve` layer). Lake cells are not carved.
2. **New elevation.** `e = max(kCoastElev, elevationRaw − valleyCarve)`.
3. **Lake floors.** Every river cell has a floor: the surface height of the lake it ultimately drains into, or `kCoastElev` if it drains straight to the sea. Raise any river cell that carving left below its floor back up to it. Without this, a river could end up lower than the lake it flows into.
4. **Downhill walk.** Visit river cells in increasing `acc` order, which visits upstream cells before downstream ones. Set `e(downstream) = max(floor(downstream), min(e(downstream), e(c)))` whenever the downstream cell is land. Floors fall going downstream (an upstream lake's surface is above the next lake's), so every river runs downhill to its mouth. A river leaving a dry basin cuts a gorge through the rim, and the outlet of a lake is never higher than the lake's surface.
5. **Lakes.** Lake cells take their surface height. The result is the `elevation` layer, which becomes `Cell::elevation`.

### Stage 10: Classify (terrain type and water source)

`slope(c) =` the largest `|e(c) − e(n)|` over neighbouring land and lake cells.

**Terrain type** takes the first rule that matches:

| # | Rule | TerrainType |
|---|------|-------------|
| 1 | Ocean, `coastDistance ≥ kShelfWidth (3)` | `DeepWater` |
| 2 | Ocean | `ShallowWater` |
| 3 | Lake cell | `Lake` |
| 4 | River cell | `River` |
| 5 | Vent cell | `Volcano` |
| 6 | `slope ≥ kCliffSlope (0.18)` | `Cliff` |
| 7 | Pass cell on land | `Pass` |
| 8 | `e ≥ kPeakElev (0.80)` | `Peak` |
| 9 | `e ≥ kMountainElev (0.55)` or `slope ≥ kSteepSlope (0.10)` | `Mountain` |
| 10 | `e ≥ kHighlandElev (0.35)` | `Highland` |
| 11 | `e ≥ kHillElev (0.15)` or `slope ≥ kHillSlope (0.05)` | `Hills` |
| 12 | otherwise | `Lowland` |

**Water source:** `Ocean` for ocean cells, `Lake` for lake cells, `River` for river cells, otherwise `None`. The border ring is ocean, so every world has a non-`None` cell.

**Flags:**

| Flag | Set on |
|------|--------|
| `Coast` | Land next to ocean |
| `Ridge` | `ridgeMask ≥ kRidgeCellMin` |
| `Barrier` | `band ≥ 0.5` |
| `Pass` | Cells in `passMask` |
| `Volcanic` | Vents and cells within `kVentRadius` |
| `RiverCrossesBarrier` | River cells with `Ridge` or `Barrier` |

`RiverCrossesBarrier` is Feature 9's river-corridor hint. The flags are stored in `Cell::terrainFlags`.

### Stage 11: Latitude

- `latDeg(c) = LatitudeNorth + (LatitudeSouth − LatitudeNorth) · cy(c) / maxCentreY`. Here `cy` is the cell centre's y, so odd columns sit half a row further south. North is at the top.
- `latitude = min(1, |latDeg| / 90)`: 0 = equator, 1 = pole, as A4 reads it.
- With the default span 60°–20°, the map runs from cool-temperate in the north to subtropical in the south, not pole to pole.
- **Known limitation (accepted 2026-10-15).** `latitude` has no sign. On a span that crosses the equator (e.g. 30° to −30°), the two hemispheres fold together, and A4 can't tell north from south (e.g. for hemisphere-dependent wind bands). A4 accepts this. If a later system needs the hemisphere, a signed field can be added alongside; that is an additive change.

### Sliders

`Land`, `Continents`, `Islands`, `Peninsulas`, `Coastline` and the latitude sliders shape geography. The rest shape relief and water. Every slider is clamped to its range on input.

| Slider (`WorldShapeParams`) | Range | Default | First stage | Stability rule (each has a test) |
|---|---|---|---|---|
| `landFraction` (Land) | 0.20–0.65 | 0.42 | 1 Landmass | **Additive.** Total land is within ±0.01 of the target. At a fixed `ContinentIslands`, the mainland at a higher value is a superset of the mainland at a lower value. Islands are re-placed against the new reference coast |
| `continents` | 1–3 | 1 | 1 Landmass | **Rebalancing, positions fixed.** Adding continent `k` keeps the centres, axes and angles of continents `< k`. Total land stays at `Land`, so existing land shrinks to make room. Continent 0 stays the main landmass |
| `islands` | 0–6 | 4 | 1 Landmass | **Prefix of groups, rebalancing of cells.** The group centres at `n + 1` are the centres at `n` plus the next one. The island budget is set by `ContinentIslands`, so existing groups shrink a little to make room. Group 0 is always the volcanic island |
| `continentIslands` (ContinentIslands) | 0–1 | 0.7 | 1 Landmass | **Monotone split, total held, capacity-bound.** Lower values ask for more land in islands, but islands are limited to their blobs (stage 1 step 6), so the share saturates at island capacity. Total land is unchanged (±0.01). As the value rises, the mainland is a superset of the previous mainland and the island cells are a subset of the previous island cells, so the island share never rises |
| `peninsulas` | 0–3 | 2 | 1 Landmass | **Rebalancing, positions fixed.** Lobe `j` keeps its direction and shape. Total land stays at `Land` |
| `coastline` | 0–1 | 0.5 | 1 Landmass | **Bounded, total held.** Changes only cells near the coast: at least 95% of the land cells that are ≥ 4 steps inland at 0 stay land at 1. Coastline length (land–ocean edges) is larger at 1 than at 0 |
| `mountainousness` | 0–1 | 0.55 | 2 Mountains | **Monotone.** The ridge curve is unchanged. Ridge width and height rise. `elevationRaw` never falls on any cell as it rises |
| `spineToRing` | 0–1 | 0.15 | 2 Mountains | **Continuous.** Land and coast are unchanged. Moving the slider by 0.05 moves each curve point by at most `0.05 · |G(t) − L(t)|` |
| `orientationDeg` | 0–359 (wraps) | 30 | 2 Mountains | **Rigid.** Land is unchanged. The line form rotates about the centroid. Passes keep their `t` |
| `passes` | 0–4 | 2 | 2 Mountains | **Prefix of accepted passes.** Passes at `n + 1` = passes at `n` plus the next accepted candidate. A candidate that would crowd an accepted pass is skipped (stage 2 step 5), so a short range can have fewer than `n` (`effective.passes`). With `passes = 0` the ridge is one connected group of cells |
| `barrier` | 0–1 | 0.6 | 3 Barrier | **Monotone.** The band position is fixed by the landmass. Height and zone cost rise |
| `barrierForm` | Plateau, VolcanicChain | Plateau | 3 Barrier | **Discrete.** Only stages 3+ change |
| `ruggedness` | 0–1 | 0.4 | 4 Elevation | **Bounded.** Land, ridge, barrier and `zoneCostBase` are unchanged. Each cell's `elevationRaw` moves by at most `kRuggedAmp`. Rivers may re-route, and the zones along them with them |
| `rivers` | 0–1 | 0.5 | 6 Rivers | **Additive; zones move inland only.** The river cells at a higher value are a superset of those at a lower value. `zoneCost` never rises on any cell, so no cell's zone moves outward (only Ancient Interiors → Wild Reaches → First Shores). Land and lakes are unchanged |
| `valleyDepth` | 0–1 | 0.4 | 9 Valleys | **Monotone.** Rivers and lakes are unchanged. `elevation` never rises on any cell as it rises. Only cells within 2 steps of a river change |
| `lakes` | 0–12 (count) | 4 | 7 Lakes | **Prefix.** Lake count is `min(lakes, eligible basins)`. The lakes at `n + 1` = the lakes at `n` plus the next ranked basin, and every filled lake stays filled |
| `latitudeNorthDeg` | −90–90 | 60 | 11 Latitude | **Isolated.** Only `latitude` changes |
| `latitudeSouthDeg` | −90–90 | 20 | 11 Latitude | **Isolated.** Only `latitude` changes |

The seed is not a slider. Changing it dirties stage 0, so everything re-runs.

**Isolation.** For every slider, the cached outputs of stages before its first stage, and of stages that don't depend on it, are byte-identical before and after the change (AC14).

### Tuning file

`Cluiche/Assets/CoW/Global/WorldGen/terrain_rules.json` holds every `k…` constant above, as a flat key → value object with a `rulesVersion`.

- The default values in this spec are a **first guess**, written before anyone has seen a map from them. They get a tuning pass after the first preset screenshots (S-1 plan task 18). Changing a constant's value, or adding a constant with a default, bumps `rulesVersion` and does not reopen this spec. Changing a rule (a stage's logic) does.
- It is loaded once when the module starts.
- A missing key falls back to the compiled default and logs a warning.
- A value out of its sane range (declared beside each compiled default) is clamped, logs a warning and reports Degraded health.
- Nothing in the UI edits it. The `layers` command shows the active constants and `rulesVersion` (read-only), so the rules stay transparent.

### Presets

**Location and deployment.** Presets are source assets in `Cluiche/Assets/CoW/Global/WorldGen/Presets/<id>.worldshape.json`. They are registered in `assets.catalogue.json` and deploy through the asset pipeline (SD-S1-011). At runtime the `presets` command lists the deployed copies.

**Saving** (decided 2026-10-15; amended 2026-10-19, F-3 World Assets). `preset.save` writes the real source file under `Cluiche/Assets/CoW/Global/WorldGen/Presets/` through F-3's `source` alias (dia-needs #16, Approved 2026-10-19, not built).
- If the alias is absent (non-dev builds, or a launch without it), the preset is written to `Cluiche/out/CoW/presets/` (PD-009), and the response says so (`writtenTo`, `isSource: false`).
- A new preset id also needs a catalogue entry. When the entry is missing, the response includes the `dia` command to add it.

**Format (version 1; envelope format confirmed by F-3 World Assets, 2026-10-19):**

```json
{
  "format": "cow.worldshape",
  "version": 1,
  "id": "continental",
  "displayName": "Continental",
  "description": "One large continent, a gentle spine, plateau interior.",
  "params": {
    "landFraction": 0.42, "continents": 1, "islands": 4, "continentIslands": 0.7,
    "peninsulas": 2, "coastline": 0.5,
    "mountainousness": 0.55, "spineToRing": 0.15, "orientationDeg": 30, "passes": 2,
    "barrier": 0.6, "barrierForm": "Plateau", "ruggedness": 0.4,
    "rivers": 0.5, "valleyDepth": 0.4, "lakes": 4,
    "latitudeNorthDeg": 60, "latitudeSouthDeg": 20,
    "temperatureOffset": 0, "moistureOffset": 0
  }
}
```

**Loading rules:**

- The preset has no seed: preset + seed = world (SD-S1-010).
- `id` must match `[a-z0-9_]{1,32}` and the file name.
- A missing `params` key takes its default and logs a warning. An unknown key is ignored with a warning. An out-of-range value is clamped with a warning.
- `version` greater than the supported version is rejected with an error. Older versions are migrated in code.
- An unknown `barrierForm` is rejected.

**Starting presets:**

| Param | Continental | Archipelago | Highlands |
|-------|-------------|-------------|-----------|
| landFraction | 0.42 | 0.30 | 0.55 |
| continents | 1 | 2 | 1 |
| islands | 4 | 5 | 3 |
| continentIslands | 0.7 | 0.5 | 0.85 |
| peninsulas | 2 | 2 | 1 |
| coastline | 0.5 | 0.8 | 0.35 |
| mountainousness | 0.55 | 0.4 | 0.85 |
| spineToRing | 0.15 | 0.0 | 0.75 |
| orientationDeg | 30 | 60 | 120 |
| passes | 2 | 3 | 2 |
| barrier | 0.6 | 0.45 | 0.8 |
| barrierForm | Plateau | VolcanicChain | Plateau |
| ruggedness | 0.4 | 0.5 | 0.7 |
| rivers | 0.5 | 0.35 | 0.65 |
| valleyDepth | 0.4 | 0.3 | 0.75 |
| lakes | 4 | 2 | 8 |
| latitudeNorthDeg / South | 60 / 20 | 40 / 10 | 65 / 35 |

`WorldShapeParams{}` defaults equal Continental.

**Archipelago keeps 2 continents on purpose** (decided 2026-10-15). This deviates from the GDD's "single large continent" (Part 1). Archipelago is a test preset for the multi-landmass rules (Ancient Interiors only on the largest landmass). The deviation goes into the same GDD fold-back as SD-S1-003.

### DiaAPI commands (terrain-facing)

All are `cow.worldgen.*` JSON commands (SD-S1-002), registered by `WorldGenerationModule`. Actions queue a request, which applies on the next SimPU frame with last value per key winning within a frame. Queries read the snapshot of the latest revision. Feature 6 lists the full command set; these are the terrain ones.

| Command | Kind | Args | Result |
|---|---|---|---|
| `set` | action | Any subset of `WorldShapeParams` keys, plus optional `seed`, plus optional `unlock` | `{success, applied[], clamped[], rejected[]}`. Triggers a live preview. On a locked import it fails with `error: "locked"` unless `unlock: true` (SD-S1-009) |
| `params` | query | — | `{seed, params{…}, preset, modified, locked, ranges{key: {min, max, default, firstStage}}, effective{…}}`. `ranges` lets the panel build sliders from data. `effective` reports what the rules actually produced when it differs from the request: `continents`, `islands` (groups accepted), `islandShare`, `passes` (accepted), `lakes` (eligible basins), so the panel can show "Lakes 4 (2 available)" instead of a slider that seems to do nothing. Imported worlds report `effective` from the recorded world |
| `preset.load` | action | `{id}` | Replaces every param (seed kept) and previews. Fails on an unknown id or an invalid file, with the reason. Subject to the lock |
| `preset.save` | action | `{id, displayName, description?, overwrite?}` | Writes the preset from the current params: the source asset via the `dia launch` alias, or `out/CoW/presets/` without it. Returns `{success, writtenTo, isSource}`. Refuses an existing id unless `overwrite` |
| `presets` | query | — | `[{id, displayName, description, version, sourcePath}]`, sorted by id |
| `layers` | query | `{layer?, offset?, count?}` | Without args: `{rulesVersion, totalMs, revision, stages: [{id, name, ms, ranLastUpdate, layers: [{name, type, min, max, hash}]}]}`; `hash` is FNV-1a 64 (16 hex digits) over the raw 4-byte cell values in cell-index order. With `layer`: one page `{name, type, revision, offset, count, total, nextOffset, values[]}` in cell-index order (amended 2026-11-08: sim replies are capped at 16 KB, so values are paged; `count` defaults to 500, max 1000 (500 for float layers, since doubles print at 17 digits and a 1000-value page would exceed 16 KB); `nextOffset` is null on the last page). Imported worlds report `available: false` |

The Cell Inspector (Feature 6's `cell` query) adds every layer value for the cell, e.g. `flowAccumulation 37, basinDepth 0.04, zoneCost 11.5`.

### Locked imports and provenance (terrain side)

- Every generated world carries a `WorldProvenance`: seed, `WorldShapeParams`, preset id (empty if modified), `kTerrainGeneratorVersion` and `rulesVersion`.
- Feature 4 writes the provenance on export and reads it on import.
- An imported world has no stage cache and is **locked**: `set` and `preset.load` need `unlock`.
- Regenerating from the provenance with the same generator and rules versions reproduces the un-edited world. If either version differs, the panel says so (the world may differ).

### Hand-off to other features

| Consumer | Reads | Notes |
|---|---|---|
| **Feature 8 (Biome Classification)** | `elevation`, `latitude`, `waterSource`, `terrain`, `depthZone`, `terrainFlags` + layers `coastDistance` and `slope` (live) | Temperature comes from latitude + elevation, moisture from distance to water + rain shadow across `Ridge`/`Barrier`. Barrier biome (rainforest, desert, volcanic) is decided there: `VolcanicChain` and `Volcanic` cells bias to Volcanic. Runs live, after stage 11 |
| **Feature 9 (Regions and Sites)** | `depthZone`, `landmass`, `terrain`, `terrainFlags`, `downstream`, biomes | Its own spec (`regions-and-sites.md`, plan task 12). Regions (10–16) grow inside depth zones and never cross a zone boundary. It finds corridor nodes from `Pass` cells, `RiverCrossesBarrier` cells (where the depth zones already bend inland) and coastal gaps at spine ends, and places sites. It is the only stage that uses sub-seeds, so retries re-roll only it (SD-S1-008). It writes `Cell::region` |
| **Feature 2 (Reachability)** | `terrain` (`TerrainTableNavigationProfile` keys by TerrainType name), `depthZone` | `WorldThroneGuarantee` reaches any cell with `depthZone == AncientInteriors` (Feature 2 amended 2026-10-15) |
| **Feature 4 (JSON)** | All `Cell` terrain fields + `WorldProvenance` | Stage layers are not exported; they are regenerable from the provenance |
| **Feature 7 (Rendering)** | Layers by name; `downstream` for the Rivers overlay; `elevation` + `oceanDepth` for Relief | Adds Relief, Depth Zone and per-layer colour modes (plan task 10) |
| **A4 Weather** | `elevation`, `latitude`, `waterSource`, `HexTopology` | Coast and ridges give it coastal rain, rain shadows and snow. Ocean elevation 0 |

## Public Interfaces

```cpp
// CoW/Shared/WorldGeneration/TerrainTypes.h
namespace CoW::WorldGeneration {

enum class WaterSource : uint8_t { None = 0, Ocean, Lake, River };
enum class DepthZone   : uint8_t { None = 0, FirstShores, WildReaches, AncientInteriors };  // None = ocean
enum class TerrainType : uint8_t { DeepWater, ShallowWater, Lake, River, Lowland, Hills, Highland,
                                   Mountain, Peak, Cliff, Pass, Volcano, Count };
enum class BarrierForm : uint8_t { Plateau, VolcanicChain };

Dia::Core::StringCRC TerrainTypeId(TerrainType t);     // e.g. "Terrain.Mountain"; terrain table + JSON key (PD-001)
const char*          TerrainTypeName(TerrainType t);

namespace TerrainFlag { enum : uint8_t { Coast = 1, Ridge = 2, Barrier = 4, Pass = 8, Volcanic = 16, RiverCrossesBarrier = 32 }; }
constexpr uint8_t kNoDirection = 7;

struct WorldShapeParams {                 // trivially copyable: carried inside GenParams on the bus
    float       landFraction     = 0.42f;  // 0.20..0.65
    uint8_t     continents       = 1;      // 1..3
    uint8_t     islands          = 4;      // 0..6
    float       continentIslands = 0.7f;   // 0..1; 0 = nearly all land in islands, 1 = nearly all in continents
    uint8_t     peninsulas       = 2;      // 0..3
    float       coastline        = 0.5f;   // 0..1
    float       mountainousness  = 0.55f;  // 0..1
    float       spineToRing      = 0.15f;  // 0..1
    float       orientationDeg   = 30.0f;  // wraps to [0, 360)
    uint8_t     passes           = 2;      // 0..4
    float       barrier          = 0.6f;   // 0..1
    BarrierForm barrierForm      = BarrierForm::Plateau;
    float       ruggedness       = 0.4f;   // 0..1
    float       rivers           = 0.5f;   // 0..1
    float       valleyDepth      = 0.4f;   // 0..1
    uint8_t     lakes            = 4;      // 0..12
    float       latitudeNorthDeg = 60.0f;  // -90..90
    float       latitudeSouthDeg = 20.0f;  // -90..90
    float       temperatureOffset = 0.0f;  // -0.3..0.3; Feature 8 (biome-classification.md), stage 12
    float       moistureOffset    = 0.0f;  // -0.3..0.3; Feature 8, stage 13
    void Clamp();
    bool operator==(const WorldShapeParams&) const = default;
};

enum class ShapeParam : uint8_t { LandFraction, Continents, Islands, ContinentIslands, Peninsulas, Coastline, Mountainousness,
                                  SpineToRing, Orientation, Passes, Barrier, BarrierForm, Ruggedness,
                                  Rivers, ValleyDepth, Lakes, LatitudeNorth, LatitudeSouth,
                                  TemperatureOffset, MoistureOffset, Count };   // last two: Feature 8

enum class TerrainStage : uint8_t { SeedFields, Landmass, Mountains, Barrier, Elevation, Drainage,
                                    Rivers, Lakes, DepthZones, Valleys, Classify, Latitude,
                                    Temperature, Moisture, Biomes, Count };     // last three: Feature 8
TerrainStage FirstStageAffected(ShapeParam p);

struct WorldProvenance {
    uint64_t         seed;
    WorldShapeParams shape;
    char             presetId[33];        // "" if modified or none
    uint32_t         generatorVersion;    // kTerrainGeneratorVersion
    uint32_t         rulesVersion;        // terrain_rules.json
};

} // namespace CoW::WorldGeneration
```

**`Cell` terrain fields** (this feature writes them; the full `Cell` is in the system spec's Output Contract):

```cpp
struct Cell {
    // Terrain (Feature 1)
    float       elevation;     // [0,1]; ocean 0, land >= kCoastElev, lake = surface
    float       latitude;      // [0,1]; 0 = equator, 1 = pole (unsigned: hemisphere not kept)
    WaterSource waterSource;   // None / Ocean / Lake / River
    TerrainType terrain;       // derivation table above
    DepthZone   depthZone;     // None for ocean
    uint8_t     landmass;      // 0 = ocean; 1.. per landmass
    uint8_t     terrainFlags;  // TerrainFlag bits
    uint8_t     downstream;    // river cells: direction 0..5 to the next cell; kNoDirection otherwise
    // Feature 8: Dia::Core::StringCRC biome;   Feature 9: region id
};
```

`WorldStructure` gains `WorldProvenance provenance` and `uint8_t mainLandmass`.

```cpp
// CoW/Shared/WorldGeneration/TerrainGenerator.h
namespace CoW::WorldGeneration {

enum class LayerType : uint8_t { Float, UInt };
struct LayerInfo   { const char* name; LayerType type; TerrainStage stage; };
struct StageTiming { TerrainStage stage; float milliseconds; bool ranLastUpdate; };

struct TerrainRules {                       // terrain_rules.json; compiled defaults = the values in this spec
    uint32_t rulesVersion;
    // ... one float/int per k-constant ...
    static TerrainRules Defaults();
    bool LoadFromJson(const Json::Value& v, Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& warnings);
};

class TerrainLayers {
public:
    static int              GetLayerCount();
    static const LayerInfo& GetLayerInfo(int index);
    int                     FindLayer(const char* name) const;      // -1 if unknown
    const float*            GetFloat(int index) const;              // nullptr if not a float layer
    const uint32_t*         GetUInt(int index) const;
};

class TerrainGenerator {
public:
    explicit TerrainGenerator(const TerrainRules& rules);

    // Re-runs only the stages dirtied since the last Update (all of them after Invalidate or a
    // seed/dimension change). Randomness: reseeds 'wldStream' (the registered WLD stream) in stage 0 only.
    void Update(uint64_t seed, const WorldShapeParams& shape, GridDimensions dims,
                Dia::Maths::RandomStream& wldStream);
    void Invalidate();

    void WriteTerrain(WorldStructure& world) const;   // fills the Cell terrain fields + provenance (minus presetId)
    const TerrainLayers& GetLayers() const;
    const StageTiming*   GetTimings() const;          // TerrainStage::Count entries
    float                GetLastTotalMs() const;
};

} // namespace CoW::WorldGeneration
```

The generator, types, rules and preset I/O (`WorldShapePreset.h`: `LoadPreset`, `SavePreset`, `ListPresets`) are pure CoW-Shared code under `Cluiche/CoW/Shared/WorldGeneration/`. The command registration lives in `WorldGenerationModule` (`Cluiche/CoW/Modules/`), and every command wraps a typed command-model call (SD-S1-002).

**Events:** none. The module publishes `WorldGenerationComplete` on Regenerate. A live preview bumps a preview revision that the debug domain reads, so live preview sends no bus traffic.

## Dependencies

| Dependency | Why | Status |
|---|---|---|
| Dia `HexTopology` (`hex-rectangular-grid.md`) | Neighbours, `Distance`, `CellCenter`, `CellFromPoint`, `kNoCell` | Done |
| Dia seeded noise (dia-needs #15) | Warp, coast, ridged, rugged, barrier wobble and vent noise. Needs pure functions of (integer seed, float x, float y): gradient noise, fBm, ridged, domain warp. Spec: @docs/specs/applications/dia/systems/diamaths/seeded-noise.md | Done (`ac646b22`); no longer blocks the build |
| `RandomStreamManager` / `RandomStreamModule` (F-2) | The registered `WLD` stream, used in stage 0 only | Done |
| Dia CellField (dia-needs #14) | Shows the layers in Feature 7. Not needed to build or test this feature | Spec Approved, not built |
| DiaAPI JSON commands, jsoncpp | Commands; preset and rules files | Done |
| F-3 World Assets `source` alias (dia-needs #16) | `preset.save` writes source presets through the `source` alias. Without it, falls back to `out/CoW/presets/` (PD-009) | Approved 2026-10-19 (F-3 World Assets), not built |

No other Dia change is needed. Priority-flood, Dijkstra and BFS over 2,772 cells are small local code. A second consumer would justify moving them to Dia; that is not needed now (AD-002).

## Acceptance Criteria

Unless stated otherwise, every AC runs at Small (66 × 42) on the default params and on 20 seeds. "Shape checks" means AC2–AC13.

### AC1: Determinism and live-preview identity
- Two fresh runs with the same seed and params give byte-identical `Cell` arrays and layers.
- **Live path:** from a fresh world, apply a scripted sequence of 200 slider changes (every slider, random order, values from a fixed list), calling `Update` after each. After each change, compare against a fresh `Invalidate()` + `Update` with the same seed and params.
- **Pass:** byte-identical every time, on 5 seeds.

### AC2: Terrain contract (system Test 1c)
**Pass:**
- Every `elevation` and `latitude` is in [0, 1].
- Ocean `elevation == 0`, land `≥ kCoastElev`, and every cell of a lake has the same elevation.
- `waterSource` is a valid enum value, and at least one cell has `waterSource != None`.
- `NeighbourAt(NeighbourAt(c, d), (d + 3) % 6) == c` for every in-bounds neighbour.
- Every cell has exactly one `TerrainType` and `DepthZone`, and `depthZone == None` exactly on ocean cells.

### AC3: Ocean border
**Pass:** every cell within `kBorderWidth` (2) steps of a map edge is ocean, including the outer ring.

### AC4: Land fraction tracks its slider
For `landFraction` from 0.20 to 0.65 in steps of 0.05, at `continentIslands` 0, 0.7 and 1. **Pass:** total land cells / all cells is within ±0.01 of the target.

### AC5: Land is additive
For each consecutive pair of `landFraction` values in AC4, at a fixed `continentIslands`. **Pass:** every mainland cell at the lower value is mainland at the higher value.

### AC5b: Continent/island split
`continentIslands` from 0 to 1 in steps of 0.1, at `islands = 4`. **Pass:**
- Total land is unchanged (within ±0.01 of `landFraction`) at every value.
- Island share (island cells / land cells) never rises as the value rises. At 0 it equals `min(requested share, island capacity)`, where capacity is the count of eligible cells (`I > 0`) over land, and `effective.islandShare` equals the measured share. At 1 it is ≤ 0.05.
- No island group merges with another (no island cell is adjacent to a cell of a different group).
- At each step, the mainland is a superset of the previous step's mainland, and the island cells are a subset of the previous island cells.
- No island cell is next to a mainland cell.

### AC6: Continents
`continents` 1 → 2 → 3, repeated at `continentIslands` 0, 0.7 and 1. **Pass:**
- The landmass that holds the mainland's highest-`M` cell (continent 0's peak) is the main landmass at every value, even when another landmass is larger.
- Ancient Interiors cells are only on the main landmass.
- The landmasses nearest continents 1 and 2 have First Shores and Wild Reaches only.

### AC7: Islands and the volcanic island
`islands` 0 → 6. **Pass:**
- The group centres at `n` are identical at `n + 1` (prefix), and total land is unchanged.
- No island cell is next to the mainland.
- With `islands ≥ 1`, exactly one island group holds a vent, and it is the first group.
- No island cell is Ancient Interiors.

### AC8: Peninsulas
`peninsulas` 0 → 3. **Pass:**
- Lobe directions are unchanged as the count rises.
- At each step, the land-cell count in the new lobe's sector (±20° of `φ_j`, beyond `0.6 R0` from the centre) increases.
- Total land still meets AC4.

### AC9: Mountain range
**Pass:**
- With `passes = 0`, the ridge cells (`Ridge` flag) form one connected group, at `spineToRing` 0, 0.5 and 1.
- At `spineToRing = 1`, the ridge splits the rest of the main landmass into ≥ 2 connected parts (it encloses the interior) on ≥ 90% of seeds.
- With `passes = n` at `spineToRing = 0`, the ridge has `effective.passes + 1` connected groups, and `effective.passes = n` whenever the curve is long enough (checked on the default params, where `n ≤ 3` fits on ≥ 90% of seeds).
- Between every two accepted passes there is at least one ridge cell (no piece is erased).
- Accepted passes are a prefix as `n` rises.

### AC10: Rivers
**Pass:**
- From every river cell, following `downstream` reaches an ocean or lake cell through river and lake cells only.
- `elevation` never rises along a river toward its mouth, and no river cell is below the surface of the lake it drains into.
- Every river tree's highest source is at `filledElevation ≥ kSourceMinElev`.
- `rivers` from 0 to 1 in steps of 0.1: each river cell set is a superset of the previous one, and `rivers = 0` has no river cells.
- At the default params, at least one river exists on ≥ 95% of seeds.

### AC11: Lakes
`lakes` 0 → 12. **Pass:**
- Lake count is `min(lakes, eligible)` and never decreases.
- Every lake filled at `n` is filled, with identical cells, at `n + 1`.
- Each lake's outflow path reaches the ocean.

### AC12: Valley depth
`valleyDepth` 0 → 1 in steps of 0.25. **Pass:**
- River and lake cells are unchanged.
- `elevation` is non-increasing on every cell as the value rises.
- Only cells within 2 steps of a river change.
- AC10's downhill rule holds at every value.

### AC13: Depth zones
**Pass:**
- On each landmass, every First Shores cell's `zoneCost` < every Wild Reaches cell's, which is < every Ancient Interiors cell's.
- `zoneCost ≤ zoneCostBase` on every cell.
- Every `Coast` cell is First Shores.
- Ancient Interiors cells are only on the main landmass, and there is at least one when the main landmass has ≥ 40 cells and the base costs have more than one distinct value above `t1` (stage 8 step 2).
- Every Ancient Interiors cell is ≥ `kAncientMinDepth` (3) steps from the coast (enforced by the rule, not just observed).
- `t1 < t2` on every world.

### AC13b: Rivers move zone edges inland only
`rivers` from 0 to 1 in steps of 0.1. **Pass:**
- `zoneCost` never rises on any cell.
- No cell's zone moves outward. Order the zones FirstShores < WildReaches < AncientInteriors: as `rivers` rises, a cell's zone never moves up this order.
- Every cell whose zone changes has a cheapest path to the coast that crosses a river cell.
- `zoneCostBase`, `t1` and `t2` are unchanged.

### AC14: Slider isolation and stage caching
For each slider in turn, change it on a cached world. **Pass:**
- Only the stages that depend on it re-run (`ranLastUpdate`).
- Every other stage's layers are byte-identical before and after.
- `FirstStageAffected` matches the Sliders table.

### AC15: Terrain classification
On a hand-built elevation, water and mask grid with one cell per table row (including the ties at each threshold). **Pass:** each cell gets the `TerrainType` of the first matching row.

### AC16: Latitude
**Pass:**
- With north/south = 60/20, the top-left cell's `latitude == 60/90` and the lowest cell centre's `== 20/90` (within 1e-6).
- Latitude is monotone in the cell centre's y.
- With 30/−30, the cells nearest the equator have the minimum latitude.
- Changing latitude re-runs stage 11 only.

### AC17: Performance and timings
Release build, Small, median of 50 runs. **Pass:**
- A full `Invalidate()` + `Update` takes < 20 ms.
- A `rivers`, `lakes` or `valleyDepth` change takes < 5 ms.
- `GetTimings()` returns one entry per stage, with `ranLastUpdate` correct.
- `cow.worldgen.layers` reports the same per-stage timings and the total.
- The 20 ms budget is measured as soon as stages 0–4 exist (the first build task), then re-measured as each stage lands, so a miss shows up while the algorithm is still cheap to change. It is a gate on the finished pipeline, not only on the last task.

### AC18: Presets produce worlds
For each of Continental, Archipelago and Highlands, on 50 seeds. **Pass:** every world passes the shape checks: AC2, AC3, the AC4 tolerance, the AC9 connectivity (`passes = 0` variant of the preset), the AC10 reach and downhill rules, the AC11 outflow rule and AC13.

### AC19: Preset files
**Pass:**
- The three preset files exist under `Cluiche/Assets/CoW/Global/WorldGen/Presets/` and load with no warnings.
- `SavePreset` → `LoadPreset` round-trips every param exactly.
- A missing key loads its default with one warning. An unknown key is ignored with a warning, and an out-of-range value is clamped with a warning.
- A file with `version: 2` is rejected. An id that doesn't match its file name is rejected.

### AC20: Terrain commands
Each command is called through `ExecuteCommandJson` from a non-SimPU thread. **Pass:**
- `set {rivers: 0.7}` applies on the next frame, and `params` reflects it.
- Two `set`s in one frame apply the last value.
- An out-of-range value appears in `clamped`.
- `preset.load {id: "highlands"}` makes `params` equal the file.
- `preset.save` writes a file that `presets` lists, and refuses an existing id without `overwrite`.
- `layers` lists every stage and layer, and paging `layers {layer: "flowAccumulation"}` to the end returns 2,772 values equal to `GetLayers()` (amended 2026-11-08: paged, each reply under the 16 KB sim limit).
- `params.effective` matches the generated world: `passes`, `islands` and `lakes` equal the accepted counts, and `islandShare` equals the measured share (checked with `lakes = 12`, `passes = 4`, `islands = 6`, where the request exceeds what fits on at least one seed).

### AC21: Locked import and provenance
**Pass:**
- A generated world's provenance holds its seed, params, preset id and both versions.
- After a world is imported, `set` without `unlock` fails with `"locked"` and changes nothing.
- With `unlock: true` it regenerates.
- Regenerating from an exported world's provenance reproduces the exported cells byte for byte.

### AC22: Visual gate (manual)
The automated shape checks measure structure, not whether a map looks like a world. After the first build, render the Relief view for each preset on 3 fixed seeds (9 screenshots, via the S-1 plan task 18 capture). **Pass:** the user reviews them and signs off that each reads as the preset's name promises: Continental shows one clear continent, a visible spine with passes and an inland plateau; Archipelago shows several landmasses; Highlands shows a rugged, ring-like mountain mass. Constants are tuned until it passes (Tuning file), and the signed-off set is kept as reference images.

## Inherited Binding Decisions

| ID | Decision | How it constrains this feature |
|----|----------|--------------------------------|
| PD-001 | StringCRC ids | `TerrainTypeId` names key the navigation table and JSON. Biomes are Feature 8's StringCRC ids |
| PD-004 | No STL containers in public APIs | Layers and arrays are `DynamicArrayC`. `WorldShapeParams` and `WorldProvenance` are trivially copyable (fixed `char` buffer) |
| AD-002 | Classify cross-cutting needs | Generator is CoW-Shared. Noise is Dia (dia-needs #15). Flood, BFS and Dijkstra stay local |
| AD-003 | GDD owns design intent | This spec deviates from S-1 GDD decision 1 (via SD-S1-003). The GDD must be updated (BACKLOG loose end) |
| SD-S1-002 | Every action/query is a DiaAPI command | Terrain commands above. The panel calls only these |
| SD-S1-003 | Rule-based world | The GDD's Part 1–2 rules are the stages |
| SD-S1-004 | Live shaping | Cached DAG, first-stage table, stability rules, layers, constants in a tuning file not exposed in the UI |
| SD-S1-005 | Terrain first, then biomes | No biome here. Hand-off table defines Feature 8 and Feature 9 inputs |
| SD-S1-006 | Three map sizes | Small (66 × 42) is dev default; Standard and Large are acceptance targets (amended 2026-10-19). Tuning and every AC at Small |
| SD-S1-008 | Retry never reshapes terrain | Terrain depends only on seed + `WorldShapeParams` (never on attempt sub-seeds) |
| SD-S1-009 | Provenance, locked imports | `WorldProvenance`; `set`/`preset.load` lock rule |
| SD-S1-010 | Preset + seed hand-off | Determinism makes a preset id + seed a complete world reference |
| SD-S1-011 | Presets are authored assets | Format, location, `preset.save` writes source |

## Open Design Questions

None open. All four questions from the first draft were answered on 2026-10-15:

| Question | Decision |
|----------|----------|
| Should Continents, Peninsulas and Coastline hold total land, or add to it? | **Hold it.** `Land` fixes total land for Continents, Peninsulas, Coastline and the new `ContinentIslands` slider, which splits the total between continents and islands |
| Should rivers shift depth zones? | **Yes**, for GDD fidelity. Rivers are cheap corridors in the zone cost, and zone edges move only inland as `Rivers` rises (stage 8, AC13b) |
| Where does `preset.save` write from a running game? | **A dev-only source-assets-root alias set by `dia launch`.** It writes the real source file under `Cluiche/Assets/CoW/`. Non-dev builds fall back to `out/CoW/presets/` (PD-009). Needs a small `dia launch` change (Dependencies). Rejected: always writing to `out/` with a promote command, and returning JSON for the caller to write |
| Latitude has no hemisphere sign on an equator-crossing span | **Accepted for A4.** Recorded as a known limitation (stage 11) |

Archipelago's 2 continents is a deliberate deviation from the GDD (Presets).

## Status

`Approved` 2026-10-18 (re-spec 2026-10-15, S-1 plan task 3; review pass below applied before approval).

- Supersedes the Placeholder Generation Algorithm (was `Approved` 2026-10-11, amended 2026-10-14).
- **Deviates from S-1 GDD decision 1** ("placeholder is naive"): the GDD needs updating, and the BACKLOG has the loose end (SD-S1-003). The Archipelago preset's 2 continents is a second, deliberate deviation for the same fold-back.
- Amended 2026-10-15 with the user's answers:
  - added the `ContinentIslands` slider;
  - depth zones use rivers as corridors;
  - `preset.save` writes through a `dia launch` dev alias;
  - the latitude sign limitation is accepted.

  No open questions remain.
- Review pass 2026-10-18 (all applied):
  - **Accepted:** main landmass is continent 0's, not the largest; islands confined to their blobs (`I > 0`) with the budget as a capacity-bound preference; default constants marked provisional; 20 ms budget measured early.
  - **Added:** passes spaced by arc length (adjacent passes could erase the range between them); mountain centre moved onto land; Ancient Interiors need depth ≥ 3 and `t1 < t2`; river cells floored at their lake's surface; river sources tested on the filled surface (monotone); `params.effective` so sliders that hit a limit say so; AC22 visual sign-off; Archipelago `continentIslands` 0.35 → 0.5.
- Amended 2026-10-18 by Feature 8 (@biome-classification.md): `WorldShapeParams` gains `temperatureOffset` and `moistureOffset` (so 19 sliders), `ShapeParam` and `TerrainStage` gain the climate entries (stages 12–14 run after Latitude), and the three preset files take the two keys at their defaults (0). Terrain stages 0–11, their ACs and the 20 ms budget are unchanged; the budget now also covers stages 12–14. Feature 8 Approved 2026-10-18.
- Plan: @terrain-generator.plan.md (16 tasks, written 2026-10-18).
- The build is plan task 7; seeded noise (dia-needs #15) is Done, so nothing blocks it.

---

## Implementation Notes

Create `terrain-generator.plan.md` when the build starts (TDD per `.claude/skills/tdd.md`). Its Implementation Patterns section must record:

- **Stage cache layout:** one struct of `DynamicArrayC` layers per stage, dirty bits, and the dependency table as data.
- **Fixed draw order for stage 0,** and how the 64-bit seed is folded into the stream's seed.
- **Determinism of priority-flood and Dijkstra:** heap ordering keys and tie rules.
- **The `terrain_rules.json` schema** and the compiled-default table.
- **Test helpers in `Testing/`:** hand-built grids for AC15, a slider-sequence driver for AC1/AC14, and shape-check helpers shared with AC18.
- **Test tiers.** Heavy sweeps (AC1's 200-change sequences, AC4/AC5 slider sweeps, AC18's 150 worlds) go in `SLOW_` suites. Each also has a cheap default-run subset (e.g. 2 seeds, 5 changes) so a plain `dia run googletest` still covers the logic. Golden values for fixed seeds are always in the default run. This is the lesson from the seeded-noise test-gap work.

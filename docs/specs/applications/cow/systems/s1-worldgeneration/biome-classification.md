# Feature Spec: Biome Classification

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md
**Design source:** GDD `02_WorldLore/GeographyAndRegions.md` (Locked 2026-07-17), Part 3 (Biome Rules, Adjacency Rules, Edge Biomes); GDD `10_PrototypeStrategy/S1_WorldGeneration.md` (DesignDocs repo)

**Gameplay Domains:** simulation

## Summary

Biome Classification gives every cell of a Terrain Generator world a first-guess biome. It runs as stages 12–14 of the same cached pipeline, straight after Latitude, so it is live: moving a slider re-runs only the stages it touches.

1. **Temperature (12)** comes from latitude and elevation.
2. **Moisture (13)** comes from distance to water plus a rain shadow. Latitude-banded winds carry moisture inland, and ridges wring it out.
3. **Biomes (14)** classify each cell. Rules come first: water, volcanic, alpine and wetland cells are pinned by terrain. Everything else takes the best climate match among the GDD's biomes. A zone fix-up then enforces the GDD's depth-zone table, and a bounded fix-up removes forbidden neighbours.

The output is `Cell::biome`, one of ten shared `StringCRC` ids (the GDD's 8 biomes plus Ocean and Lake), and a layer for each intermediate result. Two live sliders, `temperatureOffset` and `moistureOffset`, shift the whole climate.

These biomes are a starting point. A4 needs a biome per cell to find a cell's temperature baseline before it can run, and A3 evolves biomes over time (SD-S1-005). Regions (Feature 9) grow after this and take their primary biome as the majority of their cells, so classification stays per cell.

## Problem

- **A4 cannot start without biomes.** `IBiomeSeasonalBaseline::GetSeasonalTemperature(biome, season)` is keyed by biome. A4 needs one for every cell, water included.
- **Biomes must obey the GDD.** Each biome has valid depth zones, and seven pairings are forbidden. A map that breaks them gives Feature 9 regions it cannot make sense of and gives dragons impossible ecologies.
- **They should fall out of the world.** Biomes assigned independently of terrain put Desert on a rainy windward coast. A designer who reshapes the land expects the climate to follow.
- **No circularity.** A4's temperature depends on the biome, so the biome cannot depend on A4's output. Feature 8 uses a static temperature and moisture proxy built from terrain only.

## Goals

1. **A valid biome on every cell.** Exactly one of the ten ids, Ocean and Lake matching `waterSource`.
2. **GDD rules hold.** No biome outside its valid depth zones, and no forbidden pairing, on every generated world.
3. **Reads as a climate.** Warm and wet is Rainforest, warm and dry is Desert, cold is Tundra, mountains are Alpine, and the lee of a ridge is drier than its windward side.
4. **Live and stable.** Moving a climate slider re-runs stages 12–14 only, in under 5 ms at Small (66 × 42), and live preview is byte-identical to a fresh Regenerate.
5. **Transparent.** Temperature, moisture, score and the fix-up result are named layers. The constants live in the tuning file and are not editable from the panel.
6. **Clear hand-off.** Feature 9, A4 and Feature 7 know exactly what they read.

## Non-Goals

1. **Regions, edge-biome nodes and sites.** Feature 9. The GDD's 2–4 node edge biomes are regions of a biome, so they belong there.
2. **Smoothing.** Biomes stay per cell (decided 2026-10-18). Feature 9 takes each region's majority biome and keeps minority cells as edge texture.
3. **Weather and real winds.** A4 owns weather. The winds here are a first-guess proxy for the rain shadow only.
4. **Succession.** A3 evolves biomes after generation.
5. **Per-biome distribution weights.** Dropped (decided 2026-10-18). `GenParams::biomeWeights[]` is removed and the two climate offsets replace it.
6. **Hemisphere.** Latitude is unsigned (terrain spec, stage 11), so the climate is symmetric about the equator.
7. **Anomaly zones.** Biome-rule exceptions are a Phase 8 concern.

## Architecture

### Biome ids

```cpp
enum class Biome : uint8_t {
    Ocean, Lake,                                   // water ids (SD-S1-005)
    GrasslandSteppe, TemperateForest, WetlandDelta, DesertBasin,
    AlpineMountain, TundraBoreal, Rainforest, VolcanicAshland,   // the GDD's 8
    Count   // 10
};
Dia::Core::StringCRC BiomeId(Biome b);   // "Biome.Grassland", "Biome.TemperateForest", ... (PD-001)
```

`Cell::biome` holds `BiomeId(b)`. The ids are the ones A4's `IBiomeSeasonalBaseline` and A3's `BiomeTransitionComplete` use.

### Pipeline stages

Stages 12–14 join the DAG in the terrain spec and are cached the same way.

| # | Stage | Reads | Sliders | Output layers |
|---|-------|-------|---------|---------------|
| 12 | Temperature | `elevation`, `latitude` | `temperatureOffset` | `temperature` |
| 13 | Moisture | `elevation`, `latitude`, `waterSource`, ocean/lake/river masks | `moistureOffset` | `waterDistance`, `airMoisture`, `rainOut`, `moisture` |
| 14 | Biomes | 12, 13, `terrain`, `depthZone`, `terrainFlags`, `barrierMask`, `waterSource`, the barrier form | none | `biomeScore`, `biomePin`, `biomeIndex`, `fixupChanges` |

Only `elevation` is read from stage 9 onwards and `latitude` from stage 11, so a terrain change dirties 12–14 and a climate change leaves stages 0–11 untouched. `temperatureOffset` dirties 12 and 14 only; `moistureOffset` dirties 13 and 14 only. Live preview stops after stage 14.

### Stage 12: Temperature

```
temperature = clamp(1 − kLatCool · latitude − kLapse · elevation + temperatureOffset, 0, 1)
```

- `kLatCool = 0.85`, `kLapse = 0.6`. Ocean has elevation 0, so ocean cells get the latitude term only.
- With the default 60°–20° span, sea-level temperature runs from about 0.43 in the north to 0.81 in the south.

### Stage 13: Moisture

**Water proximity.** `waterDistance(c)` is the multi-source hex-step distance to the nearest water, over land and ocean. Ocean and lake cells start at 0, and river cells start at `kRiverPenalty = 2` (a river waters its banks less than a sea does). `proximity = exp(−waterDistance / kMoistScale)`, with `kMoistScale = 8`.

**Winds.** The wind blows along the map's x axis, chosen by the latitude band of the cell (`deg = latitude · 90`):

| Band | Latitude | Wind |
|------|----------|------|
| Trade | < 30° | easterly (blows west) |
| Westerly | 30°–60° | westerly (blows east) |
| Polar | > 60° | easterly (blows west) |

Bands blend linearly across `±kBandBlend = 5°` at each boundary. A flat-top hex has no east or west neighbour, so the upwind neighbours of an eastward wind are the NW and SW cells (directions 2 and 3), and for a westward wind the NE and SE cells (0 and 5).

**Air moisture.** Two sweeps, each over columns in wind order (west→east for westerly, east→west for easterly):

```
water cell (ocean, lake):   air = 1
land cell:  carried = mean(air of the in-bounds upwind neighbours)
            rise    = max(0, elevation(c) − mean(elevation of the upwind neighbours))
            rainOut = min(carried, kOrographic · rise)
            air     = clamp(carried − kAirDecay − rainOut, 0, 1)      // kAirDecay = 0.04, kOrographic = 4.0
```

Each cell takes the sweep that matches its own band, blending across the band edges. The two sweeps are independent of each other, so the result does not depend on iteration order beyond the column order, which is fixed.

**Moisture.**

```
moisture = clamp(kProximityMix · proximity + (1 − kProximityMix) · air + kRainBonus · rainOut + moistureOffset, 0, 1)
```

with `kProximityMix = 0.4` and `kRainBonus = 1.0`. The rain bonus gives the windward slope the water the ridge wrung out, and the lee side lacks it, which is the rain shadow.

### Stage 14: Biomes

Steps 1–3 classify (climate wins). Step 4 fixes depth zones. Step 5 fixes neighbours. All tie-breaks go to the lowest cell index, then the lowest biome enum value.

**1. Pinned cells.** Take the first rule that matches. `biomePin` records the rule's strength (higher wins a conflict in step 5).

| # | Rule | Biome | Pin |
|---|------|-------|-----|
| 1 | `waterSource == Ocean` | Ocean | water |
| 2 | `waterSource == Lake` | Lake | water |
| 3 | `Volcanic` flag, or terrain `Volcano`, or (`barrierForm == VolcanicChain` and the `Barrier` flag) | VolcanicAshland | 3 |
| 4 | Terrain `Mountain`, `Peak`, `Cliff` or `Pass` | AlpineMountain | 2 |
| 5 | Wetland-eligible (below) | WetlandDelta | 1 |

**Wetland-eligible:** a land cell with `elevation ≤ kWetlandMaxElev = 0.12` that is (a) a river mouth cell (a river cell next to ocean) or a land cell within one step of one, or (b) next to a lake cell, or (c) a coast cell with `moisture ≥ kWetlandMoist = 0.7`.

**2. Plateau interior.** A cell with `barrierMask ≥ kPlateauInner` (0.5) when `barrierForm == Plateau` and `Barrier > 0` chooses between Rainforest (`moisture ≥ kPlateauWet = 0.5`) and DesertBasin. It is not pinned (pin 0), so the fix-up may still change it.

**3. Climate score.** Every other land cell scores each climate biome by distance from its target:

```
score(b) = −((T − T0_b) / wT_b)² − ((M − M0_b) / wM_b)²
```

| Biome | T0 | M0 | wT | wM |
|-------|----|----|----|----|
| GrasslandSteppe | 0.60 | 0.35 | 0.25 | 0.20 |
| TemperateForest | 0.50 | 0.60 | 0.25 | 0.20 |
| DesertBasin | 0.80 | 0.08 | 0.25 | 0.12 |
| TundraBoreal | 0.12 | 0.30 | 0.15 | 0.30 |
| Rainforest | 0.85 | 0.90 | 0.20 | 0.15 |

The cell takes the best score (`biomeScore` records it). These targets are a first guess (see Tuning file).

**4. Zone fix-up (after).** Valid zones come from the GDD (Part 3):

| Biome | First Shores | Wild Reaches | Ancient Interiors |
|-------|:---:|:---:|:---:|
| GrasslandSteppe | ✓ | ✓ | |
| TemperateForest | ✓ | ✓ | |
| WetlandDelta | ✓ | ✓ | |
| DesertBasin | | ✓ | ✓ |
| AlpineMountain | | ✓ | ✓ |
| TundraBoreal | | ✓ | ✓ |
| Rainforest | | ✓ | ✓ |
| VolcanicAshland | | ✓ | ✓ |

A cell whose biome is not valid in its `depthZone` re-picks the best-scoring biome that is. The candidates are the five climate biomes and, if the cell is wetland-eligible, WetlandDelta. This overrides pins (a pinned Alpine cell on a First Shores coast becomes Grassland or Forest). The exception is the **vent island**: cells within `kVentRadius` of a vent keep VolcanicAshland in any zone, because the GDD's volcanic island is a deliberate landmark (see Open Design Questions). Ocean and Lake have no zone and are skipped. Changed cells set `fixupChanges = 1`.

**5. Forbidden-neighbour fix-up.** The forbidden pairs (GDD Part 3) are Volcanic↔Wetland, Volcanic↔Rainforest, Volcanic↔Tundra, Rainforest↔Desert, Rainforest↔Tundra, Tundra↔Wetland and Desert↔TemperateForest. AlpineMountain is a legal neighbour of everything, and so are Ocean and Lake.

Up to `kMaxFixupPasses = 8` passes, each over the cells in index order. For each forbidden pair `(a, b)` of neighbours:

1. The cell with the lower pin strength re-picks. On a tie in pin strength, the cell with the lower `biomeScore` re-picks, then the lower index.
2. It re-picks the best-scoring zone-valid biome (candidates as in step 4) that forms no forbidden pair with any of its neighbours' current biomes.
3. If none exists, it keeps its biome and the pair is left for a later pass.

Changed cells set `fixupChanges = 2`. After the last pass, the number of pairs left is `unresolvedPairs`. Because the fix-up only ever moves a cell to a biome that forms no forbidden pair, a changed cell cannot create a new violation, so passes strictly reduce violations or stop.

**Wetland moisture rule.** A WetlandDelta cell is adjacent to a moisture source (coast, lake, river, or another wetland) by construction of the eligibility test. The fix-up can only change a wetland cell away, never create one, so the rule cannot be broken by it.

### Sliders

| Slider | Range | Default | First stage | Stability rule |
|---|---|---|---|---|
| `temperatureOffset` | −0.3–0.3 | 0 | 12 Temperature | **Exact shift.** `temperature` changes by exactly the offset (clamped to [0, 1]) and `moisture` is byte-identical. Stages 0–11 are untouched |
| `moistureOffset` | −0.3–0.3 | 0 | 13 Moisture | **Exact shift.** `moisture` changes by exactly the offset (clamped) and `temperature` is byte-identical. `airMoisture`, `rainOut` and `waterDistance` do not change |

Biomes change discretely as an offset moves: a biome can appear where it had none, or vanish, and the fix-up can ripple. The rule is that the *layers* shift exactly. The biome result is always exactly what a fresh run would give (AC1).

### Tuning file

All `k…` constants here join `terrain_rules.json` (same loader, same clamp-and-warn behaviour), as does the target table. Adding them bumps `rulesVersion`. The values in this spec are a **first guess**, tuned after the first biome screenshots (AC15). Changing a value does not reopen this spec; changing a rule does.

### DiaAPI commands

No new commands. The terrain commands cover Feature 8 (SD-S1-002):

- `set` accepts `temperatureOffset` and `moistureOffset`; `params` reports them in `params`, `ranges` and `effective`. `effective` also reports `unresolvedPairs`.
- `layers` lists the stage 12–14 layers and timings, and `layers {layer: "moisture"}` returns the values.
- The Cell Inspector adds temperature, moisture, `waterDistance`, the winning and runner-up biome with scores, the pin rule that fired, and `fixupChanges` with a reason ("zone: Desert is not valid in First Shores").
- Counts per biome appear in the `layers` response (`biomeCounts`).

### Locked imports

An imported world keeps its stored biomes. Nothing reclassifies it (the stage cache is empty, as for terrain). Regenerating from provenance reproduces the un-edited world, climate offsets included, because the offsets are part of `WorldShapeParams`.

### Hand-off to other features

| Consumer | Reads | Notes |
|---|---|---|
| **Feature 9 (Regions and Sites)** | `biome`, `depthZone`, `terrain`, layers `temperature`, `moisture` | Takes each region's primary biome as the majority of its cells. Wetland and Alpine cells give it river-mouth and pass hints. Regions never need to re-run biome rules |
| **A4 Weather** | `biome` per cell via `BiomeId(b)` | The default `baseline_table` needs an entry for every one of the ten ids. A4's `TableBiomeSeasonalBaseline` covers unknown ids with 0.5 (EC-19), but the ten here must all be known |
| **A3 Biome Succession** | the starting `biome` array | Evolves it. Does not call back into this feature |
| **Feature 7 (Rendering)** | layers `temperature`, `moisture`, `biomeIndex`, `biomeScore`, `fixupChanges` | A Biome colour mode, Temperature and Moisture ramps, and a Fix-ups overlay |
| **Feature 4 (JSON)** | `Cell::biome` | Stored as the id string. Stage layers are regenerable and not exported |

## Public Interfaces

```cpp
// CoW/Shared/WorldGeneration/BiomeTypes.h
namespace CoW::WorldGeneration {

enum class Biome : uint8_t { Ocean, Lake, GrasslandSteppe, TemperateForest, WetlandDelta, DesertBasin,
                             AlpineMountain, TundraBoreal, Rainforest, VolcanicAshland, Count };
constexpr int kBiomeCount = static_cast<int>(Biome::Count);

Dia::Core::StringCRC BiomeId(Biome b);                      // PD-001
const char*          BiomeName(Biome b);                    // "Grassland / Steppe", ...
bool                 BiomeFromId(Dia::Core::StringCRC id, Biome& out);
bool                 IsBiomeValidInZone(Biome b, DepthZone z);     // GDD Part 3 table; water ids valid for DepthZone::None only
bool                 AreBiomesForbidden(Biome a, Biome b);         // symmetric; Alpine, Ocean, Lake never forbidden

} // namespace CoW::WorldGeneration
```

`WorldShapeParams` (terrain spec) gains `float temperatureOffset = 0; float moistureOffset = 0;` (each −0.3..0.3). `ShapeParam` gains `TemperatureOffset` and `MoistureOffset`, and `TerrainStage` gains `Temperature`, `Moisture` and `Biomes`. `FirstStageAffected` returns the stages in the table. The `Cell` already has `Dia::Core::StringCRC biome` (system spec).

```cpp
// CoW/Shared/WorldGeneration/BiomeClassifier.h
class BiomeClassifier {
public:
    explicit BiomeClassifier(const TerrainRules& rules);   // biome constants are part of TerrainRules
    // Called by TerrainGenerator::Update for the dirty stages among 12..14. Reads the terrain layers.
    void Update(const TerrainLayers& terrain, const WorldShapeParams& shape, GridDimensions dims, DirtyMask dirty);
    void Invalidate();
    void WriteBiomes(WorldStructure& world) const;          // fills Cell::biome
    int  UnresolvedPairs() const;
    const BiomeLayers& GetLayers() const;
};
```

`TerrainGenerator` owns a `BiomeClassifier` and runs it as stages 12–14, so there is one `Update`, one set of timings and one layer registry. `GenParams::biomeWeights[]` is removed (system spec amended).

**Events:** none. A live preview bumps the same preview revision as terrain.

## Dependencies

| Dependency | Why | Status |
|---|---|---|
| Terrain Generator (@terrain-generator.md) | Elevation, latitude, water, terrain types, depth zones, flags, barrier mask; the stage cache | Approved 2026-10-18, not built |
| Dia `HexTopology` | Neighbours, hex distance | Done |
| A4 `IBiomeSeasonalBaseline` | The ten ids need table entries | Draft (A4 spec). Not a build dependency |

No Dia change is needed. The sweeps and BFS are small local code (AD-002).

## Acceptance Criteria

Unless stated otherwise: Small (66 × 42), default params and the three presets, 20 seeds.

### AC1: Determinism and live identity
- Two fresh runs with the same seed and params give byte-identical `biome` arrays and layers.
- Extends terrain AC1: the 200-change slider sequence includes both new sliders, and after each change the live result equals a fresh `Invalidate()` + `Update`. **Pass:** byte-identical on 5 seeds.

### AC2: Valid ids and water
- Every cell has exactly one `Biome` in range.
- `waterSource == Ocean` ⇔ Ocean, `waterSource == Lake` ⇔ Lake, and no land cell is Ocean or Lake.

### AC3: Zone rules
**Pass:** for every cell, `IsBiomeValidInZone(biome, depthZone)`, except cells within `kVentRadius` of a vent, which may be VolcanicAshland in any zone. 100% on all worlds.

### AC4: Forbidden pairings
**Pass:** no two neighbouring cells form a forbidden pair, on all default-param and preset worlds. `unresolvedPairs == 0`. If it is not zero on some world, the count is reported (`effective.unresolvedPairs`, Degraded health); the world is still produced.

### AC5: Wetland
**Pass:** every WetlandDelta cell has `elevation ≤ kWetlandMaxElev` and is a river mouth cell, within one step of one, next to a lake, or a wet coast cell (`moisture ≥ kWetlandMoist`). It is also next to a moisture source: a coast, lake or river cell, or another Wetland.

### AC6: Special rules
**Pass (on worlds, before the zone fix-up changes, and checked via `fixupChanges == 0`):**
- Cells with a `Volcanic` flag, terrain `Volcano`, or the `Barrier` flag with `VolcanicChain` are VolcanicAshland.
- Mountain, Peak, Cliff and Pass cells are AlpineMountain, unless a volcanic rule applies.
- On a Plateau barrier, plateau-interior cells are Rainforest or DesertBasin (and Volcanic where flagged), unless `fixupChanges != 0`.

### AC7: Climate table (hand-built)
On a hand-built grid with one cell per row, set temperature and moisture directly. **Pass:** warm and wet → Rainforest; warm and dry → DesertBasin; cold → TundraBoreal; mid → Grassland / TemperateForest by moisture; each at a documented margin from the next. Includes a tie at each boundary (lowest enum wins).

### AC8: Rain shadow
On a hand-built grid with a north–south ridge in the westerly band and ocean to the west. **Pass:** `moisture` on the leeward (east) side is lower than on the windward side at the same distance from the ridge. `rainOut` is positive on the windward slope and zero on flat land. With the same ridge in the trade band (easterly), the shadow flips to the west side.

### AC9: Slider isolation and exact shifts
For each of the two sliders, on a cached world. **Pass:**
- Only the dependent stages re-run (`ranLastUpdate`): `temperatureOffset` runs 12 and 14; `moistureOffset` runs 13 and 14. Stages 0–11 are byte-identical.
- The shifted layer changes by exactly the offset (clamped to [0, 1]); the other climate layer is byte-identical.
- `FirstStageAffected` matches the Sliders table.

### AC10: Diversity (tuning-gated)
**Pass:** at default params, at least 5 distinct land biomes appear on at least 90% of seeds, and no single land biome covers more than 60% of land cells. Failures here mean the constants need tuning, not that the rules are wrong. Revisited at AC15.

### AC11: Performance
Release, Small, median of 50 runs. **Pass:** stages 12–14 together take < 5 ms from `Invalidate()`, and a climate-slider change takes < 3 ms. Terrain AC17's 20 ms budget still covers the whole pipeline including these stages.

### AC12: A4 contract
**Pass:** `BiomeId` returns ten distinct ids; `BiomeFromId` round-trips them. A test reads A4's default `baseline_table` (or the table in its spec until A4 Feature 1 exists) and fails if any of the ten ids has no entry.

### AC13: Commands
Called through `ExecuteCommandJson` from a non-SimPU thread. **Pass:**
- `set {moistureOffset: 0.2}` applies next frame, and `params` reports it in `params`, `ranges` and `effective`.
- An out-of-range value appears in `clamped`.
- `layers` lists the stage 12–14 layers and timings, and `layers {layer: "moisture"}` returns 2,772 values equal to `GetLayers()`.
- The cell query reports temperature, moisture, the two best biomes with scores, the pin rule and any `fixupChanges` reason.

### AC14: Imported worlds
**Pass:** after importing a world, `biome` is unchanged and nothing reclassifies it. `set` without `unlock` fails with `"locked"`. Regenerating from the exported provenance reproduces the original `biome` array byte for byte, climate offsets included.

### AC15: Visual gate (manual)
Render the Biome view for each preset on 3 fixed seeds (alongside the terrain AC22 set). **Pass:** the user signs off that the biomes read as a believable climate: Continental shows a coastal grassland and forest belt, mountains with alpine caps and a drier lee, and an interior plateau of rainforest or desert; Archipelago shows maritime, wetter biomes; Highlands is dominated by alpine and tundra. Constants are tuned until it passes.

## Inherited Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-001 | StringCRC ids | `BiomeId` returns `StringCRC`; `Cell::biome` is a `StringCRC`; the ten ids are the shared ids |
| PD-004 | No STL containers in public APIs | Layers are `DynamicArrayC`; `WorldShapeParams` stays trivially copyable with the two new floats |
| AD-002 | Classify cross-cutting needs | Feature-local CoW-Shared code. The sweeps and BFS are small and local; no Dia change |
| SD-S1-002 | Every action/query is a DiaAPI command | Climate sliders go through `set`; no new command or second path |
| SD-S1-004 | Live shaping | Cached stages 12–14, first-stage table, transparent layers, constants in the tuning file only |
| SD-S1-005 | Terrain first, then biomes | Biomes derive from terrain by rule into the GDD's 8 biomes + Ocean and Lake, as shared ids. First-guess for A3/A4 |
| SD-S1-006 | Small map only | Tuning and every AC at 66 × 42 |
| SD-S1-008 | Retry never reshapes the world | Biomes depend only on seed + `WorldShapeParams`, never on attempt sub-seeds |
| SD-S1-009 | Provenance and locked imports | Offsets are in `WorldShapeParams`, so provenance reproduces the biomes; imports are not reclassified |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | **Volcanic vent island in First Shores.** The GDD says Volcanic is Ancient Interiors (primary) or Wild Reaches (edge) only, but the terrain spec makes the first island group volcanic, and islands are First Shores or Wild Reaches. Zone rules (AC3) would turn it Grassland, losing the landmark | Visible landmark vs strict GDD | **Proposed exception:** cells within `kVentRadius` of a vent keep Volcanic in any zone (AC3 allows exactly these). The GDD fold-back must record it. Revisit if you would rather the island fall back to a normal biome |
| 2 | **Climate-vs-zone fix-up on a coast.** "Climate wins, zone fixed up after" means a cold or dry First Shores coast first scores as Tundra or Desert, then flips to Grassland or Forest. A whole coastline can be fix-up, so it never *reads* as the climate. Fix-up share is reported (`fixupChanges`) | Whether the coast looks right; whether the zone table is too tight for high-latitude maps | Not a bug. Watch the fix-up share at AC15. If it is large on a preset, tune the temperature/lapse constants or revisit "zone-aware scoring" (which would remove the fix-up as a separate pass) |
| 3 | **Latitude-banded winds vs A4's real winds.** The first-guess rain shadow can disagree with A4's wind field, so a biome placed for a westerly shadow may sit under A4's easterly rain | Weather and biome coherence in S-2.5 and S-3 | Accepted as a first guess. A4's weather will overrule it and A3 evolves biomes. Revisit if S-2.5 shows a visible mismatch |

## Status

`Approved` 2026-10-18 (S-1 plan task 11). Open design questions 1–3 stand as written.

- Feature 8 of @worldgenerationpipeline.md.
- Amends the Approved Terrain Generator (@terrain-generator.md): `WorldShapeParams` gains `temperatureOffset` and `moistureOffset`, `ShapeParam`/`TerrainStage` gain the three new stages, and the preset files gain the two keys. Amends the system spec: `GenParams::biomeWeights[]` removed, Feature 6 and Test 2b reworded.
- Build is plan task 11, after the Terrain Generator build (plan task 7).

---

## Implementation Notes

Create `biome-classification.plan.md` when the build starts (TDD per `.claude/skills/tdd.md`). Its Implementation Patterns section must record:

- **Wind sweeps:** the column-order loops, the NW/SW and NE/SE upwind neighbour indices, and the band-blend weights.
- **Candidate sets and tie rules** for the zone and forbidden-neighbour fix-ups.
- **Test helpers in `Testing/`:** hand-built climate grids for AC7 and AC8, and the biome validity helpers shared with Feature 9.
- **Test tiers:** the default run keeps golden biome arrays for fixed seeds and the hand-built tables. Seed sweeps and the 200-change sequence go in `SLOW_` suites with a cheap default subset (as in the terrain spec).

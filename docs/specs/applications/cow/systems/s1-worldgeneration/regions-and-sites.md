# Feature Spec: Regions, Nodes and Site Hook

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md
**Design source:** GDD `02_WorldLore/GeographyAndRegions.md` (Locked 2026-07-17), Parts 1–2 (regions, depth-zone distribution), Part 3 (primary biomes); GDD `10_PrototypeStrategy/S1_WorldGeneration.md` (DesignDocs repo)

> Re-scoped 2026-10-18 (twice). This feature was "Regions and Sites" (regions plus Hearths, Nexus nodes, world-thrones, dynasty starts, corridors and resource nodes). Hearths, Nexus nodes, world-thrones and dynasty starts are **dragon-layer** and move out, added later through a **site hook** (a registry for kinds S-1 doesn't know about). The map's own geography stays in S-1 as plain built-in code, not hooks: regions, **corridor nodes, River nodes, Hunting Grounds and Resource Deposits**. Filename kept as `regions-and-sites.md` so the system spec's links hold.

**Gameplay Domains:** simulation

## Summary

Feature 9 runs after Terrain (1) and Biomes (8), on Regenerate only (SD-S1-004). It is the one stage that retry re-rolls (SD-S1-008). It does three things:

1. **Regions.** It partitions the land into the GDD's strategic regions: 10–16 contiguous territories on the main landmass, each in one depth zone, each with a primary biome (the majority of its cells). Every other landmass (an island or extra continent) is one region of its own. Regions write `Cell::region` and a `RegionTable`.
2. **Built-in nodes.** It places the GDD's contested map nodes that come from geography: **Corridor nodes** (2–4 through each barrier), **River nodes**, and **Hunting Ground** and **Resource Deposit** nodes filled to 6–15 nodes per region (80–150 in total). This is ordinary S-1 code, not registered through anything.
3. **The site hook.** A `SiteRegistry` lets a later layer register a kind S-1 doesn't know about, with a placer, optional guarantees and display info. S-1 registers **no kinds** through it. The dragon layer registers Hearth Sites, Nexus Nodes, world-thrones and dynasty starts (and their guarantees) later. Generic code handles ordering, per-kind seeding, retry, JSON and drawing for those, and the built-in nodes share the same `Site` storage so one export, one overlay and one query cover both.

Regions and nodes are the map the ecology and the dragon layer sit on. A1 derives its own river, corridor and slope data from terrain (A1 spec, C11) and reads its site flags as false until a dragon-layer kind exists, so the hook is only for the dragon layer.

## Problem

- **Regions are a unit the rest of the system needs.** `Cell::region` and the `RegionTable` are in S-1's output contract. The dragon layer's territory, S-3's per-region views and the strategic map all assume them.
- **Site kinds will keep arriving.** Hearths, Nexus nodes, thrones, dynasty starts, corridors and resource nodes are all needed eventually, and more will come (anomaly zones, event nodes). Without a hook, each one edits S-1's structs, retry loop, validator, JSON schema and debug view.
- **Retry must stay stable as kinds are added.** If a new kind shares one random stream with the others, adding it reshuffles every existing kind's placement. Each kind needs its own sub-seed.
- **Regions must be right by construction.** A region that is not contiguous, spans two zones or is tiny gives every downstream system a bad base.

## Goals

1. **Regions that follow the GDD.** 10–16 on the main landmass (4–6 First Shores, 4–6 Wild Reaches, 2–4 Ancient Interiors), contiguous, zone-pure, each with a primary biome.
2. **Regions follow the land.** They prefer biome and ridge boundaries, so a region reads as a place (a forest, a highland) and not an arbitrary Voronoi cell.
3. **The GDD's geography nodes.** 2–4 corridor nodes per barrier, river nodes along rivers, and Hunting and Resource nodes filling each region to 6–15 nodes (80–150 in total), placed on terrain and biome fit.
4. **A generic site hook** (`SiteRegistry`, `ISitePlacer`, `Site`, `SiteSet`) for dragon-layer kinds: placement order, per-kind sub-seeds, guarantees, JSON round trip, debug markers and a `sites` query all work from registration alone, with no S-1 edits.
5. **Deterministic and retry-stable.** Same outer seed + attempt → identical regions and nodes. Adding or removing a registered kind never changes regions, nodes or other kinds (AC9).
6. **Fast.** Under 15 ms per attempt at Small (66 × 42) in Release, so retries stay cheap.
7. **Terrain-honest.** A world whose terrain can't hold the GDD's region, corridor or node counts reports the limit instead of failing or reshaping the world (SD-S1-008).

## Non-Goals

1. **Hearth Sites, Nexus Nodes, world-thrones, dynasty starts.** Dragon layer, through the hook. The three GDD guarantees on them (Hearth↔Nexus reachability, dynasty spacing, world-throne reachability) move with them. The built-in nodes are already in the world when the hook runs, so the dragon layer can use them (for example, the GDD's Hearth rule needs a Hunting Ground nearby).
2. **Event nodes and Anomaly Zones.** Event nodes are created by disturbance events at runtime; Anomaly Zones are a Phase 8 concern.
3. **Arrival waves and rival assignment.** C4.
4. **Smoothing or editing biomes.** Biomes are fixed by Feature 8. Regions take a majority and leave the cells alone.
5. **Edge-biome nodes.** The GDD's 2–4 node edge biomes are a region-of-a-biome concept; not built here.
6. **Live preview.** Regions and sites are computed on Regenerate only.
7. **Per-size tuning.** Small is the dev default; Standard and Large are acceptance targets (amended 2026-10-19, SD-S1-006).

## Architecture

### Where it runs

```
Regenerate:  terrain + biomes (once, outer seed)  ──►  attempt n: RegionsAndSites::Place(world, DeriveSubSeed(outer, n))
                                                        ├─ Regions                  (this feature)
                                                        ├─ Built-in nodes           (this feature): corridors, rivers, hunting, resource
                                                        └─ registered site placers  (dragon layer), in dependency order
                                                     ──►  validate guarantees (Feature 2)  ──►  accept, or retry with n + 1
```

`RegionsAndSites::Place` reads terrain and biome data and writes `Cell::region`, the `RegionTable` and the `SiteSet`. It never writes anything else, so every attempt starts from the same terrain and biomes (SD-S1-008). Each attempt takes `DeriveSubSeed(outer, attempt)`; each step inside it (regions, each built-in node step, then each registered kind) takes its own sub-seed from that (see Determinism).

### Region partition

All tie-breaks go to the lowest cell index, then the lowest region id. `kMinRegionCells = 36` and the other `k…` constants live in the tuning file.

**1. Units.** The *main landmass* (`mainLandmass`) holds the partitioned regions. **Each other landmass is exactly one region** (an island or extra continent), whatever its size, with the zone of its majority of cells. Island regions are exempt from the zone-purity and size rules, and not counted in the 10–16 (see Open Design Questions).

**2. Targets.** On the main landmass, per zone `Z` with `cells_Z` land cells:
- `capacity_Z = floor(cells_Z / kMinRegionCells)`.
- Draw a desired count `n_Z` uniformly from the GDD range (First Shores 4–6, Wild Reaches 4–6, Ancient Interiors 2–4), then `n_Z = min(n_Z, capacity_Z)`.
- If the total is below 10, add regions one at a time to the zone with the most unused capacity below its range maximum (6 / 6 / 4), ties by zone order, until the total is 10 or no zone can take one. If it is above 16, remove one at a time from the zone with the least spare capacity above its range minimum.
- If the total can't reach 10 because of capacity, the world is **terrain-limited**: counts stay at what fits and `RegionTable::limitedBy` names the zone. This is reported, not a failure (Goal 6).

**3. Seeds.** In each zone, split the zone's cells into connected components (a region never spans two). Give each component a share of `n_Z` proportional to its cells, at least 1 for components of at least `kMinRegionCells` cells (tiny components are handled in step 5). Within a component, place seeds by farthest-point sampling: the first seed is a cell chosen by the attempt's stream; each next seed maximises `minHexDistance(seeds) + kSeedBiomeBonus · [biome differs from every seed's biome]` (`kSeedBiomeBonus = 3`).

**4. Growth.** Multi-source Dijkstra over the component's cells (heap key `(cost, cell index)`), one source per seed. Stepping onto cell `c` from a region with seed biome `b` costs

```
1 + kBiomeMismatch · [biome(c) ≠ b] + kRidgeCost · [terrainFlags(c) & Ridge]       // 0.8, 1.0
```

A region stops claiming at `kMaxRegionShare · (component cells / seeds)` cells (`kMaxRegionShare = 1.7`). When every capped region is done, leftover cells go to the cheapest adjacent region, ignoring the cap. Growth never leaves its zone or landmass. Lake cells and river cells are ordinary cells here; ocean cells are region 0.

**5. Repair.**
- A region split into several pieces keeps its largest piece. The other pieces go to the adjacent region with the longest shared border (same zone if possible).
- A region under `kMinRegionCells` merges into the adjacent same-zone region with the longest border, and if there is none, into the adjacent region with the longest border of any zone. Cells that end up in a different zone's region are *absorbed*; their number is `RegionTable::absorbedCells` and is limited by AC3.
- A zone component smaller than `kMinRegionCells` is absorbed the same way.
- Regions are renumbered `1..N` in order of smallest cell index.

**6. Records.** For each region: `{id, zone, landmass, isIsland, primaryBiome, purity, cellCount, centreCell, neighbours[]}`.
- `primaryBiome` is the majority biome of its cells, ignoring Lake cells (ties by lowest enum).
- `purity` is that majority's share of the cells.
- `centreCell` is the cell nearest the centroid, moved onto land if needed.
- `neighbours` lists region ids sharing at least one hex edge, by lowest id, so the dragon layer can ask "are these regions adjacent" without scanning cells.

### Built-in nodes

Nodes are plain S-1 code, run in a fixed order after the regions, and store `Site` records (below) with fixed kind ids, so export, drawing and queries are shared with registered kinds. A **node** is a land cell (never ocean or Lake). Up to one node of each kind per cell, and a cell can hold several kinds (a river node on a corridor). All node placers share the `kNodeSpacing = 3` minimum distance (hex steps) between nodes of the same or different kind, except where stated.

| Kind id | Display name | Source |
|---|---|---|
| `CoW.Node.Corridor` | Corridor Node | Passes, river cuts and coastal gaps through a barrier |
| `CoW.Node.River` | River Node | River cells inside claimable territory |
| `CoW.Node.Hunting` | Hunting Ground | Biome and terrain fit; the most common node |
| `CoW.Node.Resource` | Resource Deposit | Biome and terrain fit |

**1. Corridors (GDD Part 11).** There are two barriers: *barrier 1* between First Shores and Wild Reaches (the mountain spine or ring), and *barrier 2* between Wild Reaches and Ancient Interiors (the second barrier). For each barrier, candidates are land cells at the boundary of the two zones that are one of:

- **Pass:** the `Pass` flag (terrain `Pass`). Single-file.
- **River cut:** `RiverCrossesBarrier`. Broad.
- **Coastal gap:** a boundary cell with no `Ridge`/`Barrier` flag within 2 steps and within `kCoastalGapReach = 4` of the coast, which means the barrier ends before the sea. Width varies.

Draw the corridor count `n_b` from 2–4 with the stream. Pick candidates in order of a stream-seeded shuffle, preferring passes and rivers over coastal gaps, accepting a candidate only if it is at least `kCorridorSpacing = 6` steps from every accepted corridor of the same barrier, until `n_b` are accepted or the candidates run out. Fewer than 2 accepted makes the barrier **corridor-limited**: reported in `effective.corridorLimited`, never a failure (it is geography, not a defect). A corridor of width 1 is one node. Broad and coastal corridors are 2–3 nodes: the accepted cell plus the 1–2 neighbouring boundary cells of the same source, subject to `kNodeSpacing` waived inside the corridor. `variant` stores the corridor id, and `flags` the barrier (1 or 2), source and width.

**2. River nodes.** A river node goes on each river's mouth cell (the river cell next to ocean or lake) and then every `kRiverNodeSpacing = 7` steps upstream along the longest path of each river tree, skipping cells not in the main landmass's claimable land (island rivers get one at the mouth). A river node on a `RiverCrossesBarrier` cell doubles as that barrier's corridor (a corridor node is also placed there if the corridor rules accept it, and the two coexist).

**3. Hunting and Resource fill.** Each region has a node quota:

```
quota(region) = clamp(round(cellCount / kCellsPerNode), 6, 15)           // kCellsPerNode = 10
```

When the quotas sum above 150 or below 80, scale them (rounded, still clamped to 6–15) toward the nearest bound; if the clamps stop it reaching the bound, the world is **node-limited** (`effective.nodeLimited`), not failed. The fill count for a region is its quota minus the corridor and river nodes already in it. For each fill slot, in region order then slot order:

1. **Kind.** Hunting if a stream draw is below the region's food share, else Resource. The share is `Food / (Food + Materials)` for the region's primary biome, from the GDD's yield table (High = 3, Medium = 2, Low = 1): Grassland 3/1, Temperate Forest 2/3, Wetland 2/1, Desert 1/2, Alpine 1/3, Tundra 1/2, Rainforest 3/2, Volcanic 1/3. A region whose primary biome is a water biome can't exist (water cells have no region).
2. **Cell.** Weighted draw over the region's land cells at least `kNodeSpacing` from every existing node. The weight is `yield(kind, biome(cell)) · terrainFactor(kind, terrain(cell))`, where `yield` is the same 1–3 value for the cell's own biome and the terrain factor prefers Lowland, Hills and Highland for Hunting and Hills, Highland, Mountain and Cliff for Resource (Peak, Volcano, River, Lake and Pass cells are excluded from both). If no cell is eligible, the slot is dropped and the region is node-limited.

The quotas count S-1's nodes only. The dragon layer's kinds (Hearth, Nexus, thrones, starts) are separate and add to what a region holds.

### Layout guarantee

`WorldLayoutGuarantee` (an `IWorldGuarantee`, Feature 2) is registered by default. It checks, with evidence (violating cells, counts per zone and per region):

1. every land cell has a region and every ocean cell has none;
2. every main-landmass region is contiguous, in one zone (except absorbed cells), and at least `kMinRegionCells`;
3. region counts are in range, or the world is terrain-limited and the counts equal what fits;
4. `absorbedCells ≤ kMaxAbsorbedShare · land cells` (3%);
5. every built-in node is on a valid land cell, at least `kNodeSpacing` from the others (corridor widths excepted), with 2–4 corridors per barrier unless corridor-limited, 6–15 nodes per region and 80–150 in total unless node-limited.

Regions and nodes are right by construction, so this guarantee rarely fails. It exists so retry has a real stage to retry before any dragon-layer kind is registered, and so a bug in the growth or placement shows up as a named failure.

### The site hook

The hook is for kinds S-1 doesn't know about: the dragon layer's Hearth Sites, Nexus Nodes, world-thrones and dynasty starts, and whatever comes after. It is **not** how S-1 places its own nodes. The built-in nodes use the same `Site`/`SiteSet` storage and `SiteKindInfo` (so the generic services below cover them), but they are fixed code, not registered, and cannot be unregistered.

```cpp
// CoW/Shared/WorldGeneration/Sites.h
struct Site {                       // trivially copyable
    Dia::Core::StringCRC kind;      // the kind's id (PD-001)
    uint32_t             cell;
    RegionId             region;    // filled by the registry from Cell::region
    uint16_t             variant;   // kind-defined (e.g. a throne's Apotheosis type)
    uint16_t             flags;     // kind-defined
};

enum class SiteShape : uint8_t { Dot, Diamond, Square, Triangle, Star, Ring };
struct SiteKindInfo {               // how the generic view and exports describe a kind
    Dia::Core::StringCRC kind;
    const char*          displayName;    // "Hearth Site"
    uint32_t             debugColour;    // 0xRRGGBB
    SiteShape            debugShape;
    const char*          variantLabel;   // nullptr if none
};

class SiteSet {                     // flat; sorted by (kind, cell)
public:
    void                 Add(const Site& s);
    Dia::Core::Span<const Site> Of(Dia::Core::StringCRC kind) const;
    Dia::Core::Span<const Site> All() const;
    int                  Count(Dia::Core::StringCRC kind) const;
    bool                 Has(uint32_t cell) const;                  // any kind at this cell
    Dia::Core::Span<const Site> At(uint32_t cell) const;
};

struct PlacementContext {           // read-only view of everything placed so far
    const WorldStructure& world;    // terrain, biomes, regions, the built-in nodes, and the sites of earlier kinds
    const GenParams&      params;
    GridDimensions        dims;
    int                   attempt;
};

class ISitePlacer {
public:
    virtual ~ISitePlacer() = default;
    virtual const SiteKindInfo& Info() const = 0;
    // Kinds that must be placed before this one (e.g. dynasty starts after hearths).
    virtual Dia::Core::Span<const Dia::Core::StringCRC> DependsOn() const = 0;
    // Adds sites of this kind to 'out'. 'rng' is reseeded for this kind and this attempt, so it is
    // independent of every other kind. Return a failure to stop the attempt (see Failure).
    virtual PlaceResult Place(const PlacementContext& ctx, Dia::Maths::RandomStream& rng, SiteSet& out) = 0;
};

class SiteRegistry {
public:
    bool Register(ISitePlacer* placer);                 // false: duplicate kind id, or a dependency cycle
    bool RegisterGuarantee(IWorldGuarantee* guarantee); // runs in Feature 2's validator with the rest
    bool Unregister(Dia::Core::StringCRC kind);
    // Placement order: dependencies first, then registration order. Computed on demand.
    Dia::Core::Span<ISitePlacer* const> Order() const;
    const SiteKindInfo* FindInfo(Dia::Core::StringCRC kind) const;
};
```

`WorldGenerationModule` owns the registry and exposes `SiteRegistry& GetSiteRegistry()`. A later module registers its kinds in `DoStart`, before the first generation (the module defers a generation request until its dependants have started, so a stage's registrations are always in place). `Unregister` runs in `DoStop`.

**Determinism.** Each step's stream is reseeded with `DeriveSubSeed(DeriveSubSeed(outer, attempt), stepId)`. The step ids are `"CoW.Regions"`, `"CoW.Node.Corridor"`, `"CoW.Node.River"` and `"CoW.Node.Fill"` for S-1's steps, and the kind id for a registered kind. So a step's output depends only on the outer seed, the attempt, its own code and the world it reads. Registering another kind never changes it, unless it declares that kind as a dependency and reads its sites. Registered kinds always run after the built-in nodes, so they never change regions or nodes.

**Failure.** A placer returns `PlaceResult { ok | retryable(reason) | terrainCause(reason) }`:
- `retryable`: a layout this attempt can't satisfy (e.g. no separated start regions). The attempt ends and Feature 3 retries with the next sub-seed.
- `terrainCause`: terrain or biomes alone make the kind impossible (e.g. no Ancient Interiors for thrones). Retry can't help (it never reshapes terrain), so generation ends immediately with a failure that names the kind and the cause (SD-S1-008).

**Generic services**, which work for any registered kind without code:

| Service | Behaviour |
|---|---|
| **JSON export/import** (Feature 4) | Each kind exports as `{cell, region, variant, flags}` objects under `sites.<kindId>`, with `SiteKindInfo` recorded alongside. On import, kinds with a registered placer are validated; **unknown kinds are kept and re-exported unchanged**, so an authored world survives a build that lacks the kind |
| **Debug drawing** (Feature 7) | One Sites overlay with a toggle per registered kind, using the kind's `debugColour` and `debugShape`. No drawer code per kind |
| **Queries** | `sites {kind?}` lists sites. The Cell Inspector lists every site at the selected cell |
| **Guarantees** | Registered guarantees appear in Feature 2's results with evidence, and in Feature 6's Validation section, like the built-ins |
| **Reports** | `GenerationReport` records the placers that ran, in order, and which one (if any) ended an attempt |

The dragon layer, for example, registers a `Hearth` placer (it can use the built-in Hunting Ground nodes for the GDD's food-proximity rule), a `Nexus` placer, a `Throne` placer, a `DynastyStart` placer (depends on `Hearth`, and can use the built-in corridors for its access rule), and its three GDD guarantees. That needs no S-1 change.

### Parameters

`GenParams::placementParams` stays, and is **empty** in S-1: regions have no Regenerate-time parameter. A site kind that needs one (for example the dragon layer's `dynastyCount`) adds a field to `PlacementParams` when it is specced, which is an S-1 struct change but a one-line one. The hook does not try to carry arbitrary parameters.

### Tuning file

The `k…` constants here join `terrain_rules.json`. The values in this spec are a first guess, tuned at the visual gate (AC15). Changing a value bumps `rulesVersion` and does not reopen this spec; changing a rule does.

### Hand-off to other features

| Consumer | Reads | Notes |
|---|---|---|
| **Feature 2 (Validator)** | `WorldLayoutGuarantee`, registered guarantees, `SiteSet` | The three site-based built-ins move to the dragon layer (see Amendments) |
| **Feature 3 (Retry)** | `RegionsAndSites::Place`, `PlaceResult` | Retry re-runs this stage per attempt; `terrainCause` ends it |
| **Feature 4 (JSON)** | `RegionTable`, `Cell::region`, `SiteSet` | Generic per-kind export and import |
| **Feature 6/7 (panel, drawers)** | `RegionTable`, `SiteKindInfo` | Region colours and edges, a Sites overlay with a toggle per kind, and the Cell Inspector |
| **A1 Ecology** | nothing required | Its site flags (`hearth_site`, `nexus_node`, `world_throne_type`) stay false until the dragon layer registers kinds. It derives corridor and river data itself and may later read the built-in Corridor, River, Hunting and Resource nodes |
| **S-3, V-1** | `Cell::region`, `RegionTable` | Per-region views and strategic-map regions |
| **Dragon layer** | everything above, including the built-in nodes | Registers its kinds against the hook |

## Public Interfaces

```cpp
// CoW/Shared/WorldGeneration/Regions.h
namespace CoW::WorldGeneration {

using RegionId = uint8_t;                  // 0 = ocean, 1..N
constexpr RegionId kNoRegion = 0;

struct RegionRecord {
    RegionId   id;
    DepthZone  zone;                       // majority zone for island regions
    uint8_t    landmass;
    bool       isIsland;
    Biome      primaryBiome;
    float      purity;                     // [0, 1]
    uint16_t   cellCount;
    uint32_t   centreCell;
    Dia::Core::DynamicArrayC<RegionId> neighbours;
};

struct RegionTable {
    Dia::Core::DynamicArrayC<RegionRecord> regions;        // index = id - 1
    uint16_t  absorbedCells;
    uint8_t   mainlandRegionCount;
    DepthZone limitedBy;                                    // DepthZone::None unless terrain-limited
    bool      AreAdjacent(RegionId a, RegionId b) const;
    const RegionRecord* Find(RegionId id) const;
};

} // namespace CoW::WorldGeneration
```

`WorldStructure` (system spec Output Contract) replaces `hearths`, `nexuses`, `thrones` and `starts` with `RegionTable regions;` and `SiteSet sites;`. `Cell::region` is a `RegionId`. The built-in nodes live in `sites` under the fixed kind ids in the Built-in nodes table (`BuiltinNodeKinds::kCorridor`, `kRiver`, `kHunting`, `kResource`).

```cpp
// reported in params.effective and the layers query, from the last Regenerate
struct LayoutLimits {
    DepthZone regionLimitedBy;        // None unless terrain-limited (RegionTable::limitedBy)
    bool      corridorLimited[2];     // per barrier: fewer than 2 candidates
    bool      nodeLimited;            // quotas could not reach 80..150, or a slot found no cell
};
```

```cpp
// CoW/Shared/WorldGeneration/RegionsAndSites.h
PlaceResult RegionsAndSites::Place(WorldStructure& world, const SiteRegistry& registry,
                                   const GenParams& params, uint64_t attemptSeed,
                                   Dia::Maths::RandomStream& wldStream);
```

**Events:** none new. `WorldGenerationComplete` is unchanged; sites and regions are read through `GetCurrentWorld()`.

## Dependencies

| Dependency | Why | Status |
|---|---|---|
| Terrain Generator (Feature 1) | Zones, landmasses, flags, `mainLandmass` | Approved 2026-10-18, not built |
| Biome Classification (Feature 8) | `Cell::biome` for growth and primary biomes | Approved 2026-10-18, not built |
| Reachability Validator (Feature 2) | `IWorldGuarantee`, `WorldLayoutGuarantee` evidence | Approved; amended here, not built |
| Dia `HexTopology` | Neighbours, hex distance | Done |
| `RandomStreamManager` (F-2) | The registered `WLD` stream, reseeded per kind and attempt | Done |

No Dia change is needed (AD-002).

## Amendments to other specs

Feature 9 changes the system's contract. These apply with this feature's approval; the detailed text of the Feature specs is updated when each is built (they are still unbuilt), and each gets a pointer note now.

| Spec | Change |
|---|---|
| **System** (`worldgenerationpipeline.md`) | Output Contract: `hearths`, `nexuses`, `thrones`, `starts` replaced by `regions` and `sites`. Summary and Goal 2: the three hard guarantees become the dragon layer's, registered through the hook; S-1 enforces `WorldLayoutGuarantee`. "Three Hard Guarantees" section marked as moved. Tests 1a/1b/5a retargeted (below). Features table updated |
| **Feature 2** (`reachability-validator.md`) | The framework (`IWorldGuarantee`, `INavigationProfile`, evidence) stays. `HearthNexusReachabilityGuarantee`, `DynastySpacingGuarantee` and `WorldThroneGuarantee` are specified there but **built with the dragon layer**; their ACs run on hand-authored worlds, so they can still be written later without S-1 placement. Adds `WorldLayoutGuarantee` |
| **Feature 3** (`retry-mechanism.md`) | The retried stage is `RegionsAndSites::Place` (regions + registered kinds). `PlaceResult::terrainCause` ends generation with no retry. `GenerationReport` records the placers that ran |
| **Feature 4** (`json-export-import.md`) | Schema v1 stores `regions` and `sites.<kind>` generically. Unknown kinds round-trip. The fixed hearth/nexus/throne/start arrays go |
| **Feature 6/7** | A Sites overlay with a toggle per registered kind; region colour mode and region edges; the `regions` and `sites` queries; Dynasty Starts as a fixed toggle goes (a kind-driven toggle replaces it) |
| **A1 Ecology** (`a1-ecology.md`) | No change needed. Its "S-1 Feature 9 sites" flags are produced by the dragon layer's kinds, not S-1 |

**System tests retargeted.**
- **Test 1a (success rate):** 100 seeds at Small, ≥95% pass `WorldLayoutGuarantee` (plus any registered guarantees).
- **Test 1b (compliance):** 100% of successful worlds satisfy every registered guarantee.
- **Test 5a (retry and fallthrough):** a test placer and a test guarantee that fails on attempt 0 force a retry. An engineered never-passing guarantee falls through to Failure. The dragon layer's real guarantees extend these later.

## Acceptance Criteria

Unless stated otherwise: Small (66 × 42), default params and the three presets, 20 seeds, attempt 0.

### AC1: Determinism and attempt variation
- Two runs with the same seed, params and attempt give byte-identical `Cell::region`, `RegionTable` and `SiteSet`.
- Attempts 0 and 1 give a different region layout (at least one cell's region differs) on ≥ 90% of seeds, and byte-identical terrain and biome arrays on 100%.

### AC2: Partition
- Every land cell has a region ≥ 1 and every ocean cell has region 0.
- Each main-landmass region is one connected group of cells.
- No region spans two landmasses.
- Region ids are `1..N` in order of smallest cell index.

### AC3: Zone purity
**Pass:** every main-landmass region's cells share one depth zone, except absorbed cells, and `absorbedCells ≤ kMaxAbsorbedShare · land` on all worlds. Island regions are exempt.

### AC4: Counts
**Pass:** the main landmass has 10–16 regions, with First Shores 4–6, Wild Reaches 4–6 and Ancient Interiors 2–4, **or** the world is terrain-limited (`limitedBy != None`) and each count equals `min(range-driven target, capacity)`. Every non-main landmass has exactly one region. At `landFraction = 0.20` the world is reported terrain-limited if capacity requires it, never failed.

### AC5: Sizes
**Pass:** every main-landmass region has ≥ `kMinRegionCells` cells, and none has more than `kMaxRegionShare · mean + kMaxRegionSlack` cells (tuning-gated).

### AC6: Records
**Pass:**
- `primaryBiome` equals the majority biome of the region's cells (Lake ignored; ties by lowest enum), and `purity` equals its share.
- `neighbours` is symmetric and equals the set found by scanning every hex edge.
- `centreCell` is a land cell of its region, or of the nearest cell of the region.

### AC7: Biome and ridge coherence (tuning-gated)
**Pass:** mean `purity` over main-landmass regions ≥ 0.6 on ≥ 90% of seeds, and the share of region borders that run along a `Ridge` cell or a biome change is higher than for a random same-size partition on the same world. Failures mean the cost constants need tuning.

### AC8: Layout guarantee
**Pass:** `WorldLayoutGuarantee` is satisfied on ≥ 95% of 100 seeds at attempt 0, and 100% after retries. On hand-built worlds: an uncovered land cell, a disconnected region, a two-zone region, a region under `kMinRegionCells`, a count outside the range (not terrain-limited), `absorbedCells` over the cap, a node on water, two nodes closer than `kNodeSpacing`, a barrier with 5 corridors, and a region with 16 nodes are each detected with evidence naming the violating cells or region.

### AC8a: Corridors
**Pass (default params, 20 seeds, all presets):**
- Each barrier has 2–4 corridors, or is `corridorLimited` with fewer than 2 candidates (verified by counting Pass, `RiverCrossesBarrier` and coastal-gap candidates independently).
- Every corridor node lies on a Pass cell, a `RiverCrossesBarrier` cell or a coastal-gap cell on the boundary of the two zones it joins.
- Corridors of one barrier are ≥ `kCorridorSpacing` apart. Widths are 1 for passes and 1–3 for rivers and coastal gaps, with `flags` and `variant` decoding correctly.
- With `passes = 0` and `rivers = 0` on a seed with no coastal gap, barrier 1 is `corridorLimited` and generation still succeeds.

### AC8b: Nodes
**Pass:**
- Every node is on a land cell that is neither Lake nor ocean, and not Peak, Volcano, or (for Hunting and Resource) River or Pass.
- Each region has 6–15 nodes of the built-in kinds and the world has 80–150, or is `nodeLimited` and the counts equal what fits.
- Nodes are ≥ `kNodeSpacing` apart (corridor widths excepted).
- Every river tree that reaches the sea has a river node on its mouth cell, and river nodes along it are `kRiverNodeSpacing` apart.
- Over 20 seeds, Hunting nodes are over-represented in Grassland and Rainforest regions and Resource nodes in Alpine, Volcanic and Forest regions compared with the opposite kind (tuning-gated, checked by the food/materials ratio of node counts per primary biome).

### AC9: Hook ordering and independence
A test placer `A`, a test placer `B` that `DependsOn` `A`, and a test placer `C` with no dependencies. **Pass:**
- `Order()` places `A` before `B`, and `C` follows registration order among the rest.
- A dependency cycle is rejected by `Register`, as is a duplicate kind.
- Registering or removing `C` leaves the regions, the built-in nodes and the sites of `A` and `B` byte-identical.
- Registered kinds run after the built-in nodes, and a placer can read them through `PlacementContext`.
- Each placer receives a stream reseeded from `DeriveSubSeed(DeriveSubSeed(outer, attempt), kind)`.

### AC10: Hook failure and retry
**Pass:**
- A placer returning `retryable` on attempt 0 and `ok` on attempt 1 yields a world accepted on attempt 1, with terrain and biomes identical to attempt 0, and the report listing the placers that ran per attempt.
- A placer returning `terrainCause` ends generation on attempt 0 with a failure naming the kind and the reason, and no retry.
- A registered guarantee that fails on attempt 0 and passes on attempt 1 behaves like `retryable`.

### AC11: Generic export, import and drawing
**Pass:**
- A test kind round-trips through JSON export and import exactly (cell, region, variant, flags).
- A world exported with a kind that is not registered on import keeps that kind's sites and re-exports them unchanged.
- The Sites overlay lists a toggle for each registered kind with its `debugColour` and `debugShape`, with no per-kind drawer code.
- `sites {kind}` returns the kind's sites, and the Cell Inspector shows the sites at a selected cell.

### AC12: Performance
Release, Small, median of 50 runs. **Pass:** `RegionsAndSites::Place` (regions and built-in nodes, no registered kinds) takes < 15 ms, and with three test kinds adds < 2 ms. Terrain AC17's 20 ms budget and Biome AC11 cover the live stages only; this stage is not live.

### AC13: Commands and fields
Called through `ExecuteCommandJson` from a non-SimPU thread. **Pass:**
- `regions` returns every `RegionRecord` (including `neighbours` and `limitedBy`).
- `sites` returns all sites (built-in nodes included), or those of the given kind.
- `params.effective` reports `corridorLimited` and `nodeLimited`.
- The cell query reports the region id and zone, and any sites at the cell.
- `Cell::region` is populated for every land cell of a generated world.

### AC14: Imported worlds
**Pass:** an imported world keeps its stored regions and sites; nothing recomputes them. Regenerating from the exported provenance reproduces the original regions and sites byte for byte when the same kinds are registered.

### AC15: Visual gate (manual)
Render the Regions view and region edges for each preset on 3 fixed seeds, alongside the terrain AC22 and biome AC15 sets. **Pass:** the user signs off that regions read as places: each follows a recognisable biome or highland, boundaries run along ridges and biome changes, and the three depth zones show the GDD's counts.

## Inherited Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-001 | StringCRC ids | Site kinds and the constant `"CoW.Regions"` are `StringCRC`; `Site::kind` is a `StringCRC` |
| PD-004 | No STL containers in public APIs | `RegionTable` and `SiteSet` use `DynamicArrayC` and Dia spans; `Site` and `SiteKindInfo` are trivially copyable |
| AD-002 | Classify cross-cutting needs | Hook and regions are CoW-Shared. Dijkstra and seed sampling are small and local. Site kinds are CoW-Shared or dragon-layer; none are Dia |
| SD-S1-002 | Every action/query is a DiaAPI command | `regions` and `sites` are queries; there is no action to add (Regenerate already exists) |
| SD-S1-004 | Live shaping | Regions and sites run on Regenerate only, never during live preview |
| SD-S1-008 | Retry never reshapes the world | This is the only retried stage. It reads terrain and biomes and writes nothing else. `terrainCause` ends generation instead of reshaping |
| SD-S1-009 | Provenance and locked imports | Same seed + params + attempt reproduces regions and sites; imports are not recomputed |
| SD-S1-010 | World hand-off | Regions and sites ride in `WorldStructure` via `GetCurrentWorld()` |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | **Island regions sit outside the 10–16.** The GDD's 10–16 and zone ranges are written for a single continent; it doesn't say whether the 3–5 island groups count. Here each non-main landmass is one extra region, so a world has 10–16 mainland regions plus one per island group and extra continent | Totals above 16 on island-heavy worlds; Archipelago's second continent becomes one large region | Chosen to keep the GDD's zone ranges meaningful for the continent. Revisit if S-3 or the strategic map wants islands inside the count, or Archipelago's second continent wants real regions |
| 2 | **Retry is mostly idle in S-1.** Regions and nodes are right by construction, so `WorldLayoutGuarantee` rarely fails. Retry's real work arrives with the dragon layer's guarantees; until then it is exercised by test placers and guarantees, and by node placement that can change with the sub-seed | Test 1a measures layout only; the ≥95% target is easy to meet | Accepted. The AC10 tests prove the machinery now. The GDD's three site guarantees get their own success-rate tests when they are built |
| 5 | **Node quotas exclude the dragon layer's sites.** The GDD's 6–15 nodes per region includes Hearth Sites. S-1 counts only its own nodes, so a world with Hearths added can exceed 15 in a region | Whether the GDD range is a hard cap | Accepted as written. When the dragon layer lands, either its Hearths take slots from the Hunting/Resource fill (a `PlacementContext` quota hook), or the range is read as S-1's nodes only |
| 6 | **Node density is a first guess.** `kCellsPerNode = 10` gives about 117 nodes at the defaults (1,165 land cells), inside 80–150, but node spacing 3 may feel tight or loose once the overlay is seen | Look of the map; ecology and dragon layers' later use | Tuned at the visual gate (AC15). A constant, not a rule |
| 3 | **`PlacementParams` is one shared struct.** A new site kind that wants a Regenerate-time parameter edits S-1's struct. A per-kind parameter block would avoid that, but needs a generic panel and JSON path for typed parameters | A one-line S-1 edit per kind vs a bigger generic mechanism now | Kept simple (see Parameters). Revisit if a second kind needs a parameter |
| 4 | **Do regions have any consumer before the dragon layer?** A1 doesn't read them; S-3 and V-1 do (per-region views, strategic map) | Whether to keep regions in S-1 at all | Kept: `Cell::region` is in the output contract and the dragon layer needs it. Cheap to build and to drop if it proves unused |

## Status

`Approved` 2026-10-18 (S-1 plan task 12). Open design questions 1–6 stand as written.

- Feature 9 of @worldgenerationpipeline.md.
- Re-scope (2026-10-18, at the user's direction, twice): Hearths, Nexus nodes, thrones and dynasty starts moved to the dragon layer, which uses a generic site hook; S-1 keeps regions and builds the geography nodes (corridors, rivers, Hunting and Resource) as plain code, not hooks. Amends the system spec and Features 2, 3, 4, 6 and 7 as listed under Amendments to other specs.
- Build is plan task 12, after the Terrain Generator (task 7) and Biome Classification (task 11).

---

## Implementation Notes

Create `regions-and-sites.plan.md` when the build starts (TDD per `.claude/skills/tdd.md`). Its Implementation Patterns section must record:

- **Dijkstra heap keys and tie rules** for growth, repair and renumbering.
- **Seed sampling:** how the first seed is drawn from the stream and how farthest-point sampling breaks ties.
- **Registry ordering:** the topological sort with registration-order ties, cycle detection, and how a late registration invalidates the cached order.
- **Per-kind sub-seed derivation** and the stream reseed call.
- **Generic JSON path:** how unknown kinds are kept on import.
- **Node placement:** the candidate lists and the stream-seeded shuffle for corridors, the longest-path walk for river nodes, the food/materials weight tables, and the order in which fill slots draw from the stream.
- **Test helpers in `Testing/`:** hand-built worlds for AC8 and AC8a/AC8b (barriers with and without passes, rivers and coastal gaps), the test placers `A`, `B`, `C` and the failing test guarantee for AC9/AC10, and region-validity helpers shared with the dragon layer.
- **Test tiers:** golden region arrays for fixed seeds and the hand-built cases in the default run; the 100-seed sweeps in `SLOW_` suites with a cheap default subset.

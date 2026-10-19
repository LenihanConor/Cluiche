# System Spec: WorldGenerationPipeline

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/S1_WorldGeneration.md` (DesignDocs repo)

**Gameplay Domains:** simulation

## Summary

WorldGenerationPipeline (WLD) is CoW's world data generation system. It produces a seeded, deterministic world structure — grid (terrain/biome/region per cell), a region table, and a **generic site set** — and validates the guarantees registered against it. *(Re-scoped 2026-10-18: S-1 builds terrain, biomes, regions and the geography nodes (corridors, rivers, Hunting Grounds, Resource Deposits), plus a site hook for the dragon layer only; Hearth Sites, Nexus Nodes, world-throne sites, dynasty starts and their three hard placement guarantees — Hearth↔Nexus reachability, dynasty minimum spacing, world-throne reachability — are the dragon layer's, registered through the hook. See @regions-and-sites.md.)* WLD is a **data pipeline only**: it generates static world structure, not live simulation state (no A1 ecology tick state, no dragon/clan/dynasty runtime state).

The system supports **runtime generation** with debug-adjustable parameters (seed, terrain and climate sliders, placement tuning), **export to human-readable JSON** for hand-editing, **import from JSON** for loading authored worlds, **retry mechanism** with sub-seeds when placement guarantees fail, and **timeout/failure handling** that routes through CoW's Cancel path (GDD decision 5).

WorldGenerationPipeline enables CoW's testbed iteration workflow: generate → inspect → tweak parameters → regenerate, all from F-1's debug host without leaving the game (GDD decision 3). It resolves **PARK-P6-001** (worldgen timeout/failure behavior).

## Problem

CoW requires worlds that are:
- **Structurally correct** — every registered guarantee must hold. S-1 enforces the region layout; the dragon layer adds the three GDD placement guarantees (Hearth↔Nexus reachability, dynasty minimum spacing, world-throne existence/reachability) or the world is unplayable for dragons
- **Deterministic** — same seed always produces byte-for-byte identical output, including retry sequences
- **Authorable** — designers must be able to export a procedural world, hand-edit problematic placements in a text editor, and re-import the result
- **Tunable** — designers need fast iteration (generate → inspect → tweak → regenerate under 5 seconds) without switching to a separate tool
- **Validated** — reachability checks use injected navigation profiles (B2's body-plan rules plug in later), not naive graph connectivity
- **Failure-safe** — unlucky seeds or unsatisfiable parameters must not hang forever or silently return broken worlds

Without this system, every downstream testbed (S-2/S-3 ecology, S-4 dragons, S-5+ lineage/clans) would be blocked on placeholder worlds, and tuning world generation quality would require manual binary editing or separate standalone tools.

## Goals

1. **Seeded deterministic generation** — Same outer seed → same retry sequence → same world output, byte-for-byte identical across runs
2. **Registered guarantees enforced** — Retry mechanism ensures every successfully-generated world satisfies every registered guarantee. S-1 registers `WorldLayoutGuarantee`. The dragon layer registers the GDD's three: (a) every Hearth Site reachable from at least one Nexus Node, (b) dynasty starting positions respect minimum spacing, (c) all six world-throne sites exist (one per Apotheosis type) and are reachable from Ancient Interiors
3. **Export/import round-trip** — Export world to JSON, hand-edit, re-import as exact match (not a re-roll)
4. **Debug-adjustable parameters** — Seed, terrain and climate sliders, placement tuning exposed via F-1's debug host (no app restart needed)
5. **Timeout and failure handling** — Hard timeout prevents infinite hangs; unsatisfiable seeds route through Cancel's discard-and-return path with distinguishing assert/message
6. **Fast iteration** — Regeneration (seed or parameter change) completes under 5 seconds (GDD §7 metric)
7. **Debug-grade visualization that explains the data** — A `WorldGenDebugDomain` (`IDebugDomain`) shows every per-cell field (colour-by Biome, Terrain, Region, Elevation, Latitude, Water Source), a click-to-inspect cell inspector, the regions and every registered site kind, and why each guarantee passed or failed (proof paths, violations, per-attempt retry history). Not V-1's real tile art *(amended 2026-10-15)*

## Non-Goals

1. ~~**Real/tuned generation algorithm**~~ — *Superseded 2026-10-15 (SD-S1-003):* S-1 no longer uses a near-random placeholder. It builds a rule-based world per GDD `GeographyAndRegions.md`. Still a non-goal: final art-quality tuning, which comes from playtesting
2. **A1 ecology state** — WLD produces world structure (grid, sites, regions); A1 (ecology tick pipeline) and A3 (succession) initialize runtime state on top of that structure
3. **Live simulation** — No ticking, no dragons, no clans; those are S-2/S-3/S-4's responsibility
4. **Standalone WorldDesign editor** — S-1 uses F-1's in-game debug host for iteration; a future CluicheEditor-based toolset is possible but out of scope (GDD decision 3)
5. **V-1 visual polish** — Debug-grade rendering only (colored tiles + site markers); V-1 owns real tile art and camera controls
6. **Network synchronization** — CoW is single-player; no lockstep worldgen across clients

## Architecture

### Output Contract

WorldGenerationPipeline produces:

```cpp
struct WorldStructure {
    GridDimensions dimensions;       // width × height in cells (hex grid, see Hex Grid below)
    std::vector<Cell> cells;         // terrain (Feature 1), biome (Feature 8), region (Feature 9) per cell
    RegionTable regions;             // 10-16 mainland regions + one per other landmass (Feature 9)
    SiteSet sites;                   // generic, per registered kind; empty in S-1 until a layer registers kinds (Feature 9 hook).
                                     // Dragon layer kinds: Hearth, Nexus, Throne (x6), DynastyStart (replaces the old fixed lists)
    WorldProvenance provenance;      // seed, WorldShapeParams, preset, generator + rules versions (SD-S1-009)
    uint8_t mainLandmass;            // landmass id holding the Ancient Interiors
};

// Amended 2026-10-15: terrain fields per Feature 1 (terrain-generator.md), which defines the enums.
struct Cell {
    // Terrain (Feature 1)
    float elevation;         // [0, 1]; ocean 0, land >= kCoastElev, lake = surface; read by A4 (orographic lift)
    float latitude;          // [0, 1]; 0 = equator, 1 = pole; read by A4 (solar heating)
    WaterSource waterSource; // None / Ocean / Lake / River; read by A4 (evaporation source)
    TerrainType terrain;     // DeepWater .. Volcano, derived from elevation + slope + water; navigation table key
    DepthZone depthZone;     // None (ocean) / FirstShores / WildReaches / AncientInteriors
    uint8_t landmass;        // 0 = ocean; 1.. per connected landmass
    uint8_t terrainFlags;    // Coast, Ridge, Barrier, Pass, Volcanic, RiverCrossesBarrier
    uint8_t downstream;      // river cells: direction 0..5 to the next cell; kNoDirection otherwise
    // Biomes (Feature 8)
    Dia::Core::StringCRC biome;  // GDD's 8 biomes + Ocean/Lake (SD-S1-005)
    // Regions (Feature 9)
    RegionId region;         // 10-16 regions grown inside depth zones
};
```

**No ecology state** (A1's species populations, resource levels), **no dragon/clan/dynasty state** (B6/C1/C2 runtime) — structure only. Ancient Interiors is a `depthZone`, not a region (amended 2026-10-15). `elevation`, `latitude` and `waterSource` are static terrain, not simulation state: A4 reads them and never writes them.

### Hex Grid

*Amended 2026-10-14 (A4 Weather).* The world is a **hex grid**, stored as a rectangle of `width × height` cells.

- **Layout:** **flat-top** hexes, **odd-q offset** (odd columns shifted half a cell down). Row index grows downward (south). `GridCoord{x, y}` is the offset coordinate, and the cell index is `y * width + x`, so flat arrays and the JSON cell array keep their shape.
- **Direction order:** 6 directions `d = 0..5`, at on-screen angle `30° + d × 60°` counter-clockwise (a flat-top hex has no east or west neighbour): `0` NE, `1` N, `2` NW, `3` SW, `4` S, `5` SE. This is the order A4 depends on.
- **One hex grid, in Dia (decided 2026-10-14).** The grid and topology are Dia's `Dia::Pathfinding::HexTopology` and `HexPathGrid(width, height)` (rectangular flat-top odd-q layout), specified in @docs/specs/applications/dia/systems/diapathfinding/hex-rectangular-grid.md. S-1 defines no hex graph or topology of its own. Neighbours, `kNoCell`, `Distance`, `DirectionFromAngle`, `DirectionVector` and the cell geometry (`CellCenter`, `CellFromPoint`) all come from there, so S-1, the reachability search, the renderer and A4 share one definition. The old radius-based `HexPathGrid` could not hold a map (1,261 cells at most); the Dia feature extends the same class rather than adding a second one.
- **Prerequisite:** the Dia feature is `Done` (2026-10-14); S-1 is unblocked on the hex grid.
- **No world wrap.** Edges are hard; `NeighbourAt` returns `kNoCell` past them.

### Map Sizes

*Amended 2026-10-14, re-scoped 2026-10-15, re-scoped 2026-10-19.* The world-sim stack supports and tests three sizes:

| Size | Dims (width × height) | Cells | Role |
|------|----------------------|-------|------|
| Small (default) | 66 × 42 | 2,772 | Dev/testbed default |
| Standard | 80 × 52 | 4,160 | Acceptance target |
| Large | 104 × 64 | 6,656 | Release perf gate for world-sim stack |

**Small is the dev default.** S-1 acceptance tests, performance targets and debug-draw budgets are measured at Small. **Standard and Large are acceptance targets** (amended 2026-10-19): the world-sim stack (S-1, A4 Weather, A1 Ecology) must pass acceptance at Standard and meet the Large perf gate for release. Nothing in the code is size-specific: the generator, validator and renderer take any `dimensions` from 1×1 up to 128×128 (the tested range of the Dia grid).

**Per-size budget scaling (amended 2026-10-19).** Where a spec states only a Small budget, Standard and Large budgets scale by cell count from Small: Standard ≈ ×1.5 (4,160 / 2,772 = 1.501), Large ≈ ×2.4 (6,656 / 2,772 = 2.401), unless stated otherwise.

**Size picker (amended 2026-10-19).** The S-1 testbed exposes a size picker through a dedicated `cow.worldgen.set_size` DiaAPI command (args: `Small` | `Standard` | `Large`); changing size regenerates the world. F6 Goal 10 amended: size display becomes a picker.

**Test coverage (amended 2026-10-19).** Determinism and acceptance tests run at all three sizes. Large tests carry a `SLOW_` prefix where the run time is expensive.

### Three Hard Guarantees *(moved to the dragon layer, 2026-10-18)*

These three are the GDD's guarantees on Hearths, Nexus nodes, dynasty starts and world-thrones. S-1 no longer places those sites, so S-1 does not enforce them; the dragon layer registers the sites and these guarantees through the Feature 9 site hook (@regions-and-sites.md). Feature 2 still specifies them. S-1 enforces `WorldLayoutGuarantee`. When registered, they must hold for every successfully-generated world (enforced by retry mechanism):

1. **Hearth↔Nexus reachability:** Every Hearth Site is reachable from at least one Nexus Node via traversable paths under the injected navigation profiles
2. **Dynasty minimum spacing:** Every pair of dynasty starting positions (player + rivals) respects the minimum spacing rule (exact value TBD by placeholder algorithm)
3. **World-throne existence/reachability:** All six world-throne sites exist (one per Apotheosis type) and are each reachable from the Ancient Interiors

**Validation uses injected navigation profiles** (`INavigationProfile` → DiaPathfinding `IPathCostProvider`), not naive graph connectivity — "reachable" means a path exists under at least one profile, per GDD open question #3. S-1 ships a placeholder terrain-table profile; B2 injects one profile per body plan when built. The three guarantees are also injected (`IWorldGuarantee`), so later systems can add guarantees without editing S-1.

### Retry Mechanism with Sub-Seeds

```
1. Generate world from outer seed
2. Validate three hard guarantees
3. If all satisfied → return world
4. Else: retry with deterministic sub-seed (outer seed + attempt counter)
5. Repeat up to MAX_RETRY_ATTEMPTS
6. If exhausted → Failure path
```

**Deterministic:** Same outer seed always produces same retry sequence and same final outcome (success with specific placement, or fallthrough to Failure).

**Timeout overrides retry:** If total generation time (including all retries) exceeds hard timeout, Failure path triggers immediately.

### Export/Import: JSON for Hand-Editing

**Export format:** Human-readable JSON capturing full world structure (grid, biomes, terrain, sites) — not seed-only, because you can't hand-edit a seed (GDD decision 4).

**Use case:** Designer generates world → exports to `my_world.wld.json` → hand-edits one bad Hearth Site placement in a text editor → re-imports into S-1 or downstream testbeds.

**Round-trip guarantee:** Import(Export(world)) == world, byte-for-byte identical (not similar, not a re-roll).

**Distinct from E1 save/load:** E1 serializes live session state (ecology runtime, dragon positions, clan economies); WLD export freezes a procedural result into an authored asset (same spirit as GDD Phase 12's landmark dragons).

### Timeout and Failure Paths

Both route through **Cancel's existing discard-and-return path** (reuses Phase 6 mechanism, GDD decision 5):

```
Timeout:   total generation time exceeds hard timeout → Cancel + "TIMEOUT" assert/message
Failure:   retries exhausted or actual generation error → Cancel + "FAILURE" assert/message
Manual:    player clicks Cancel button → Cancel + "MANUAL_CANCEL" assert/message
```

**Distinguishing messages ensure** they're never conflated. Timeout wins over retry (prevents infinite hangs on unlucky seeds while preserving determinism).

### Layered Debug Rendering (GDD decision 8)

**Base layer (always on):** Relief hillshade (default; Feature 7 colour modes cycle through Biome, Terrain, Region, Elevation, Latitude, Water Source and Relief)

**Toggleable overlays:**
- Sites (one toggle per registered site kind, e.g. the dragon layer's Hearth/Nexus/world-throne markers; none in S-1)
- Regions (boundary overlays)

**Rationale:** Prevents visual clutter when debugging one specific concern (e.g., "are Hearth Sites well-distributed?" doesn't need dynasty-start markers). Matches S-3's layered emergence visualization pattern (GDD decision 7).

### Data Dependencies

1. **B2's body-plan traversability** — no longer a blocker. Navigability is injected (`INavigationProfile`); S-1 ships a placeholder terrain-table profile and B2 supplies the real profiles later (GDD open question #3).

2. **A1's species biome-affinity data** — Optional, for design reference toggle. If not externalized, cut the toggle (doesn't block S-1's core pipeline).

## Public Interfaces

### APIs

```cpp
// WorldGenerationModule API (CoW-Shared)
class WorldGenerationModule : public IModule {
public:
    // Generate world from seed + parameters; blocks until complete or timeout
    // Returns: success (with WorldStructure), or failure (timeout/unsatisfiable)
    Result<WorldStructure> GenerateWorld(uint64_t seed, const GenParams& params);
    
    // Export world to JSON file at specified path
    Result<void> ExportWorld(const WorldStructure& world, const std::string& filePath);
    
    // Import world from JSON file; validates structure on load
    Result<WorldStructure> ImportWorld(const std::string& filePath);
    
    // Query current generation progress (for debug UI progress bar)
    GenerationProgress GetProgress() const;

    // Read access to the current accepted world (generated or imported).
    // nullptr until the first world is accepted. SimPU only: the pointer is valid until the next
    // accepted world, so consumers must not hold it across frames; re-read when GetWorldRevision() changes.
    const WorldStructure* GetCurrentWorld() const;

    // Incremented every time a new world is accepted (WorldGenerationComplete or import).
    uint32_t GetWorldRevision() const;
};

struct GenParams {
    WorldShapeParams shape;                // Terrain sliders (Feature 1) + climate offsets (Feature 8); trivially copyable
    PlacementParams placementParams;       // Regenerate-time site parameters; empty in S-1, a site kind adds its field when specced (Feature 9)
    uint32_t maxRetryAttempts;             // Bounded retry count (default TBD)
    float timeoutSeconds;                  // Hard timeout (default TBD)
    GridDimensions dimensions;             // Map size; default Small 66 × 42 (see Map Sizes)
};
```

### Events

**Published by WorldGenerationModule:**

```cpp
// Published on successful generation (GDD Phase 7)
// A small notification, not a payload: bus messages are small POD, so the world itself is
// read through WorldGenerationModule::GetCurrentWorld() (amended 2026-10-14).
struct WorldGenerationComplete {
    uint64_t seed;
    uint32_t retryAttempts;  // 0 if first attempt succeeded
    uint32_t worldRevision;  // matches GetWorldRevision() at publish time
};
// Subscribers: A2/GameSpeedController (resets to season 0, paused), A1/Ecology (initializes grid state), 
//              A4/Weather (re-reads terrain, re-runs spin-up), C4/Dynasty (schedules rival arrival waves)

// Published on timeout/failure
struct WorldGenerationFailed {
    FailureReason reason;  // TIMEOUT / UNSATISFIABLE / ERROR
    char message[128];    // Fixed buffer (truncated); distinguishing text per GDD decision 5
};
```

**Subscribed by WorldGenerationModule:**

```cpp
// Triggered by F-1's debug host "Regenerate" button
struct WorldRegenerationRequested {
    uint64_t seed;
    GenParams params;
};

// Triggered by F-1's debug host "Export World" button
struct WorldExportRequested {
    char filePath[260];   // Fixed buffer: bus messages must be trivially copyable
};

// Triggered by F-1's debug host "Import World" button
struct WorldImportRequested {
    char filePath[260];   // Fixed buffer: bus messages must be trivially copyable
};

// Triggered by F-1's debug host Cancel button or timeout
struct WorldGenerationCancelled {
    CancelSource source;  // MANUAL / TIMEOUT / ERROR
};
```

## Dependencies

### Upstream Systems (must exist before S-1 implementation)

| System | Why needed | Status |
|--------|-----------|---------|
| **F-2 Core Runtime** | RNG determinism (same seed → same retry sequence), GameSpeedController integration (time controls for debug), band resolver (if WLD uses DiaAttribute Feature 7) | Approved (backlog row) |
| **F-1 Harness** | Debug host integration (seed/param inputs, Regenerate button, Export/Import UI, Cancel button) | Approved (backlog row) |
| **DiaPathfinding** | `IPathCostProvider` injection point and `FindPath<TGraph>` for reachability (Done). B2 is **not** a dependency: its body-plan profiles are injected later | Done |
| **DiaPathfinding: Rectangular Hex Grid** | `HexPathGrid(width, height)` and `HexTopology` (see Hex Grid). The only hex graph; S-1 adds none | Done (2026-10-14) |
| **Dia debug-draw budget** ([dia-needs](../../dia-needs.md) #13) | Raise `DebugFrameData::kGeometryCapacity` from 2,048 to 8,192 for Feature 7's line and marker overlays. Blocks Feature 7 only | Done |
| **Dia CellField renderer** ([dia-needs](../../dia-needs.md) #14) | GPU-retained per-cell colour layer for the hex tiles, colour modes and grid lines; reused later by A4/S-2.5/S-3 overlays. Blocks Feature 7 only | Spec Approved 2026-10-15, not built |
| **DiaDebugDomain** | `IDebugDomain` + DiaDebugPanel: the F-1 debug host for S-1's controls, inspector and drawers (Features 6 and 7) | Done |
| **A1 species biome-affinity data** | Optional for design reference toggle | Optional — cut the toggle if not externalized |

### Dia Engine Dependencies

- **DiaSimTime** (RandomStreamManager for seeded RNG)
- **DiaSaveGame** (if WLD reuses serialization helpers for JSON export/import, or just uses jsoncpp directly)
- **DiaCore** (containers, CRC, file I/O)
- **DiaGraphics** (for debug-grade tile rendering via ICanvas or DiaBgfx)
- **DiaMessageBus** (for event pub/sub: `WorldGenerationComplete`, `WorldRegenerationRequested`, etc.)

## Features

| # | Feature | Description | Spec | Status |
|---|---------|-------------|------|--------|
| 1 | Terrain Generator *(was Placeholder Generation Algorithm; re-specced 2026-10-15)* | Rule-based world per GDD `GeographyAndRegions.md`: continent, coast, islands, mountain spine, second barrier, rivers carving valleys, lakes, depth zones; per-cell elevation, latitude and water source. Driven by `WorldShapeParams` sliders + presets, shaped live (sliders only add or remove what they control). Rules fixed and documented; each stage viewable as a layer | @terrain-generator.md | Approved 2026-10-18 |
| 2 | Reachability Validator | Runs injected world guarantees (three built-in hard guarantees) using injected navigation profiles (placeholder now, B2 body plans later) over a hex path graph; not naive graph connectivity. Each `GuaranteeResult` keeps its evidence (proof paths, violating cells/pairs) for the debug view | @reachability-validator.md | Approved |
| 3 | Retry Mechanism | Deterministic sub-seed retry (outer seed + attempt counter) when guarantees fail; bounded attempt count; same outer seed → same retry sequence. Keeps a `GenerationReport` (one entry per attempt: sub-seed, failed guarantee ids) for the debug view | @retry-mechanism.md | Approved |
| 4 | JSON Export/Import | Human-readable JSON format capturing full world structure (grid, biomes, terrain, sites); round-trip guarantee (Import(Export(world)) == world) | @json-export-import.md | Approved |
| 5 | Timeout and Failure Handling | Hard timeout monitors total generation time; timeout/unsatisfiable/error route through Cancel's discard-and-return with distinguishing assert/message | @timeout-failure-handling.md | Approved |
| 6 | F-1 Debug Host Integration | `WorldGenDebugDomain` panel: seed input, terrain and climate (`temperatureOffset`, `moistureOffset`) sliders, placement param sliders, Regenerate, Export/Import, progress, Cancel; plus the **Cell Inspector** (every field of the selected cell) and **Validation** section (per-guarantee result and per-attempt retry history) | @f1-debug-host-integration.md | Approved |
| 7 | Layered Debug Rendering | `WorldGenDebugDomain` drawers: tile layer coloured by Biome / Terrain / Region / Elevation / Latitude / Water Source, grid lines, region edges, Sites, Dynasty Starts, Guarantee Evidence (proof paths, violations), selected-cell highlight, Design Reference (optional). Needs dia-needs #13 and #14 (CellField) | @layered-debug-rendering.md | Approved |
| 8 | Biome Classification *(new, 2026-10-15)* | Biomes fall out of terrain by rule, as live pipeline stages 12–14: temperature (latitude + elevation) and moisture (distance to water + latitude-banded rain shadow) → the GDD's 8 biomes + Ocean/Lake as shared ids; pinned water/volcanic/alpine/wetland cells; climate scoring; zone fix-up; forbidden-neighbour fix-up. Two live sliders (`temperatureOffset`, `moistureOffset`) replace `biomeWeights[]`. First-guess biomes that A3/A4 evolve | @biome-classification.md | Approved 2026-10-18 |
| 9 | Regions, Nodes and Site Hook *(split out of Feature 1, 2026-10-15; re-scoped 2026-10-18)* | 10–16 contiguous mainland regions within depth zones (one region per other landmass), each with a primary biome; built-in geography nodes (2–4 corridors per barrier, river nodes, Hunting and Resource nodes to 6–15 per region, 80–150 in total); `WorldLayoutGuarantee`; and a generic site hook for dragon-layer kinds only (`SiteRegistry`, `ISitePlacer`) so the dragon layer can add Hearth, Nexus, world-throne, dynasty-start and node kinds, with their guarantees, export and drawing, without editing S-1 | @regions-and-sites.md | Approved 2026-10-18 |

## Acceptance Criteria

### Test 1a — Generation Success Rate
Generate worlds across 100 random seeds at the Small preset (66 × 42). Record how many succeed (every registered guarantee satisfied; in S-1 that is `WorldLayoutGuarantee`, the dragon layer adds its three) vs. fallthrough to Failure (exhausted retry attempts). **Target: ≥95% success rate** (GDD §6, §7).

### Test 1b — Guarantee Compliance
For all successfully-generated worlds from Test 1a, confirm every registered guarantee holds (S-1: `WorldLayoutGuarantee`; the dragon layer extends this to Hearth↔Nexus reachability, dynasty minimum spacing and world-throne existence/reachability). **Target: 100% compliance** — zero violations among successful generations (GDD §6, §8).

### Test 1c — Terrain Contract for A4 *(added 2026-10-14)*
For every world generated in Test 1a: every cell has `elevation` and `latitude` in [0, 1] and a valid `waterSource`; at least one cell has `waterSource != None` (A4 asserts this); and for every cell `NeighbourAt` is symmetric (if B is A's neighbour in direction `d`, A is B's neighbour in direction `(d + 3) mod 6`). **Target: 100%.**

### Test 2a — Generation Determinism
Generate a world with a specific seed, record output, regenerate with same seed. Confirm byte-for-byte identical (every cell's biome/terrain/region, every site placement). Test on ≥10 seeds, including at least one that triggers retries. **Target: 100% match** (GDD §4, §6).

### Test 2b — Debug-Adjustable Regeneration
Via F-1's debug host, change seed, then separately change the `moistureOffset` climate slider, regenerating after each change. Confirm each regeneration reflects changed input and completes within **under 5 seconds** (GDD §7 metric).

### Test 3 — Export/Import Round Trip
Generate world, export to JSON, reload. Confirm reloaded world is identical to exported one (not re-roll of same seed, not similar-but-different). **Target: exact match, every field** (GDD §6, §7).

### Test 4 — Timeout Path
Force generation past hard timeout (injected delay). Confirm auto-cancels through discard-and-return with assert/log message explicitly identifying "TIMEOUT", distinct from manual Cancel and Failure (GDD §6, §8).

### Test 5 — Failure Path
Force generation error (not merely slowness). Confirm routes through discard-and-return with assert/log message explicitly identifying "FAILURE", distinct from timeout and manual Cancel (GDD §6, §8).

### Test 5a — Guarantee Retry and Fallthrough
Force a first attempt that violates a registered guarantee (S-1 uses a test site kind and a test guarantee that fails on attempt 0; the dragon layer later adds e.g. a narrow map making Hearth↔Nexus unreachable). Confirm generation retries with sub-seed and either succeeds within bounded attempts, or — for engineered-to-never-succeed seed — falls through to Failure rather than returning world that violates guarantee. Confirm outer seed produces same retry sequence and outcome every time (GDD §6).

## Inherited Binding Decisions

The following binding decisions from parent specs apply to this system:

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | Use StringCRC for all entity/component IDs | Biome, TerrainType, Region enums use StringCRC for comparison; site types use StringCRC IDs |
| PD-002 | ProcessingUnit/Phase/Module architecture | WorldGenerationModule lives on SimPU (generation is deterministic sim-time work, not wall-clock); F-1 debug host lives on MainPU (UI input) |
| PD-004 | No STL containers in public APIs | WorldGenerationModule API uses Dia containers (DynamicArrayC, HashTable) for WorldStructure fields, not std::vector/std::map |
| PD-007 | C++20 is required | Placeholder generation can use C++20 features (concepts, std::span, constexpr improvements) |
| PD-009 | All non-binary output under `Cluiche/out/<AppName>/` | Exported `.wld.json` files default to `Cluiche/out/CoW/worlds/` (user can override path via debug host). Source baked worlds from `cow.world.terrain.bake` go to `Assets/CoW/Global/WorldGen/Baked/<id>.wld.json` (amended 2026-10-19, F-3 World Assets) |
| PD-010 | `.diagame` is project root; typed imports route loading | S-1 stage declared in `CoW.diagame` as a `stage` import; `.diastage` contains stage metadata (name, manifest path) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-001 | CoW edits to Dia require ask permission | If WLD hits a gap in Dia (e.g., RNG API missing feature, serialization helper needed), flag for approval before editing Dia; propose capability, don't silently add it |
| AD-002 | Classify cross-cutting needs (Dia / CluicheGameBaseline / CoW-Shared / Feature-local) | WorldGenerationModule is **CoW-Shared** (reused by S-2/S-3/S-4 stages but specific to CoW's mechanics); placeholder algorithm is **Feature-local** (stays in S-1 until proven reusable) |
| AD-003 | GDD owns design intent; spec tree owns implementation | GDD `S1_WorldGeneration.md` defines pass/fail metrics, three hard guarantees, testbed purpose; this spec defines WorldGenerationModule API, retry mechanism implementation, event contracts |
| AD-004 | Promote BootMenuModule; dev builds boot to debug menu | S-1 stage launches from F-1's debug menu (no `--s1` flag; menu selection only); relies on promoted BootMenuModule being in CluicheGameBaseline |
| AD-005 | One stage per testbed; modes chosen in-panel | S-1 is one `.diastage` listed as Boot transition; if S-1 later gains modes (e.g., "Single Cell" vs "Living Ecology"), they're chosen via in-stage panel, not separate stages |

## System Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-S1-001 | S-1 is a **live terrain editor** that runs in game: sliders and presets shape the world in near real time, rendered by the game's own view. If the in-game panel becomes the limit, the next step is a CluicheEditor panel that drives the running game over DiaDebugServer (Inspector tier), not an editor viewport | The game already has the renderer, hex view, live module and panel; CluicheEditor has no world viewport. What you see is what A4/A1 run on | Accepted 2026-10-15 | Yes |
| SD-S1-002 | **Every editor action and query is a DiaAPI command** (`cow.worldgen.*`, JSON commands). The panel invokes the same commands; there is no second path. Actions queue a request (thread-safe) and apply on the next SimPU frame; queries read a snapshot published per revision; during slider drags only the last value per frame applies | Anything the panel does can be scripted (Python, automation, DiaChatPlugin, a future editor panel). One typed command model, UI on top (UI/business separation) | Accepted 2026-10-15 | Yes |
| SD-S1-003 | **Rule-based world, not random noise.** Feature 1 is re-specced as a Terrain Generator following the physical rules of GDD `02_WorldLore/GeographyAndRegions.md` (Locked 2026-07-17): continent, coast, peninsulas, islands, mountain spine, second barrier, rivers that run downhill and carve valleys, lakes in basins, depth zones. Noise (dia-needs #15) is raw material inside the rules, never the result. **Deviates from S-1 GDD decision 1** ("placeholder is naive"); the GDD owns that decision (AD-003), so the GDD needs updating | The world must look and feel like a world, and A4 needs coasts and mountains for coastal rain, rain shadows and snow | Accepted 2026-10-15 (GDD fold-back pending) | Yes |
| SD-S1-004 | **Live shaping.** Designer inputs are `WorldShapeParams` sliders (land vs sea, mountainousness, ruggedness, river density, lakes, coastline complexity, islands, peninsulas) + saved presets (e.g. Continental, Archipelago, Highlands). The pipeline is staged with cached results; a slider re-runs from its first affected stage in the same frame. Sliders are thresholds/weights over seed-fixed fields, so they add or remove only what they control (more lakes adds lakes; nothing reshuffles). Live preview never places sites, validates or retries; those run on Regenerate. Rules are fixed in code and documented step by step, constants in a tuning file, and every stage result is viewable as a layer: **transparent, not tweakable in the UI** | Near-real-time shaping makes it a tool; stability makes it feel like shaping, not rerolling | Accepted 2026-10-15 | Yes |
| SD-S1-005 | **Terrain first, then biomes.** Feature 1 (Terrain) and Feature 8 (Biome Classification) are separate. Biomes fall out of terrain by rule (temperature from latitude + elevation, moisture from distance to water + rain shadow) into the GDD's 8 biomes + Ocean and Lake, as shared `StringCRC` ids, honouring zone rules and forbidden neighbours. S-1 supplies these first-guess biomes because A4's temperature needs a biome per cell before it can run; A3/A4 evolve them later. Regions and Sites become Feature 9 | Keeps terrain testable and viewable on its own; avoids an A4 ↔ biome circularity | Accepted 2026-10-15 | Yes |
| SD-S1-006 | **Three map sizes** (66 × 42 Small, 80 × 52 Standard, 104 × 64 Large). Small is the dev default; Standard and Large are acceptance targets and the Large perf gate applies to the whole world-sim stack (S-1, A4, A1). Per-size budgets scale by cell count from Small (Standard ≈ ×1.5, Large ≈ ×2.4) unless stated otherwise. Code accepts any size up to 128 × 128 | Amended 2026-10-19, product review: the world-sim stack ships at all three sizes | Accepted 2026-10-15, amended 2026-10-19 | Yes |
| SD-S1-007 | **Debug view = `WorldGenDebugDomain` + CellField.** One `IDebugDomain` holds the panel and drawers on SimPU. Hex tiles, colour modes and grid lines render through the Dia CellField renderer (dia-needs #14: runtime geometry per map size, per-cell value texture, palette/ramp shader); overlays use debug draw with the budget raised to 8,192 (#13). Rejected: a 32,768 budget with immediate-mode tiles (CPU cost grows with size and overlays), a baked mesh asset per size (fixed sizes, drifts from `HexTopology`, colours baked in), and bending the 3D mesh path (lit shader, no updates, two cameras) | One renderer serves S-1, A4's 8 overlays, S-2.5, S-3 and fog of war | Accepted 2026-10-15 | Yes |
| SD-S1-008 | **Retry never changes the shaped world.** Terrain and biomes are fixed by seed + `WorldShapeParams`. When a guarantee fails, retries re-roll only Regions and Sites (Feature 9) with sub-seeds. If the terrain itself can't satisfy a guarantee (e.g. no room for Ancient Interiors), generation fails with a reason naming the guarantee and the terrain cause; it never silently reshapes | What you shaped in preview is what Regenerate keeps | Accepted 2026-10-15 | Yes |
| SD-S1-009 | **Provenance and locked imports.** Every export records its seed, `WorldShapeParams`, preset name and generator version. An imported world opens **locked**: sliders show its recorded params but moving one asks whether to drop the edits and regenerate. Regenerating from the recorded provenance reproduces the un-edited world | Hand edits are never lost by accident; any world can be traced back to its recipe | Accepted 2026-10-15 | Yes |
| SD-S1-010 | **World hand-off to other stages.** A stage names its world in its `.diastage` config: a `.wld.json` asset, preset + seed (deterministic), or `"world_bundle": "<id>"` (F-3 World Assets, amended 2026-10-19). `WorldGenerationModule` loads it at stage start (import or generate) and the world is read through `GetCurrentWorld()`. S-2.5, S-3 and A4 get their worlds this way | This is what makes S-1 the foundation for weather and biomes | Accepted 2026-10-15, amended 2026-10-19 | Yes |
| SD-S1-011 | **Presets are authored assets.** Preset files (`WorldShapeParams` + display name, versioned JSON) live under `Cluiche/Assets/CoW/` and deploy through the asset pipeline. Saving a preset from the running tool writes the source asset (not `out/`), through a DiaAPI command | Presets are inputs a designer keeps and shares, unlike generated output (PD-009) | Accepted 2026-10-15 | Yes |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What are the exact placeholder algorithm parameters? (biome distribution weights, minimum-spacing value, reachability-check implementation, bounded retry-attempt count, hard timeout value) | Affects GenParams struct and default values; doesn't block S-1 implementation (these are tunable defaults) | **Superseded 2026-10-15:** the designer-facing inputs are `WorldShapeParams` sliders + presets (SD-S1-004); rule constants live in a tuning file. Retry count and timeout stay tunable defaults |
| 2 | ~~Is B2's terrain traversability data externalized yet?~~ | Was blocking | **Resolved 2026-10-13:** navigability is injected via `INavigationProfile` (DiaPathfinding `IPathCostProvider`); S-1 ships a placeholder profile and no longer waits on B2. See reachability-validator.md |
| 3 | Is A1's species biome-affinity data externalized? | Design Reference toggle only; can cut if not externalized without blocking S-1 core pipeline | Optional — if not externalized by S-1 Feature 7 implementation, cut the Design Reference toggle (non-blocking) |
| 5 | Undo/redo for slider and preset changes? | Tool ergonomics. Cheap now that every action is a DiaAPI command (keep a command history) | **Resolved 2026-10-15: out.** Feature 6 keeps it as a non-goal |
| 4 | Should a future standalone WorldDesign (CluicheEditor-based) toolset be built? | Out of scope for S-1 (GDD decision 3 chose in-game iteration), but GDD leaves this open as "genuinely open" for later | **Resolved 2026-10-15 (SD-S1-001):** in-game live terrain editor; if the panel becomes the limit, a CluicheEditor Inspector-tier panel driving the running game, not an editor viewport |

## Status

`Approved` — 2026-10-11. Amended 2026-10-14 for A4 Weather: hex grid (Dia's rectangular `HexPathGrid` and `HexTopology`; map-size presets Small/Standard/Large), per-cell `elevation`/`latitude`/`waterSource`, world read accessor, `WorldGenerationComplete` reduced to a notification. No code existed, so the JSON schema stays at v1. Amended 2026-10-15: S-1 targets the Small map only (Standard and Large deferred); the debug view is a `WorldGenDebugDomain` that explains the data (colour-by per-cell field, cell inspector, guarantee evidence, retry history), which adds evidence to `GuaranteeResult`, a `GenerationReport` to the retry loop, and a Dia debug-draw budget raise (dia-needs #13). Same day: tiles and grid move to a Dia CellField renderer (dia-needs #14), and #13 shrinks to 8,192. Same day: Feature 1 becomes a rule-based Terrain Generator shaped live by sliders (re-spec pending; deviates from S-1 GDD decision 1), and Biome Classification (8) and Regions and Sites (9) are added; build order is terrain, then biomes, then the rest. Amended 2026-10-18: Feature 8 spec'd (climate offsets replace `biomeWeights[]`); Feature 9 re-scoped at the user's direction to regions, the geography nodes (corridors, rivers, Hunting and Resource) built in, and a generic site hook used only by the dragon layer, with Hearths, Nexus nodes, thrones and dynasty starts (and their three GDD guarantees) moved to it.

**Plan:** @worldgenerationpipeline.plan.md

---

## Implementation Notes

When implementation begins (after approval), create a plan file `worldgenerationpipeline.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- WorldGenerationModule class structure (init/shutdown, API entry points)
- Retry mechanism state machine (attempt counter, sub-seed derivation)
- JSON schema for export format (cell array, site arrays, version tag)
- Event wiring (which events WorldGenerationModule publishes/subscribes, handler signatures)
- Debug rendering layer registration (base layer + toggle IDs for F-1's host)

These patterns are validated against actual implementation before tasks are marked Done.

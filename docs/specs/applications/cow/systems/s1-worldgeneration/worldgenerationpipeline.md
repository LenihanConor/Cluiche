# System Spec: WorldGenerationPipeline

**Parent:** @docs/specs/applications/cow/cow.md  
**Design source:** `Output Docs/GDD/10_PrototypeStrategy/S1_WorldGeneration.md` (DesignDocs repo)

**Gameplay Domains:** simulation

## Summary

WorldGenerationPipeline (WLD) is CoW's world data generation system. It produces a seeded, deterministic world structure — grid (biome/terrain/region per cell), Hearth Sites, Nexus Nodes, world-throne sites, and dynasty starting positions — and validates three hard placement guarantees (Hearth↔Nexus reachability, dynasty minimum spacing, world-throne existence/reachability). WLD is a **data pipeline only**: it generates static world structure, not live simulation state (no A1 ecology tick state, no dragon/clan/dynasty runtime state).

The system supports **runtime generation** with debug-adjustable parameters (seed, biome distribution weights, placement tuning), **export to human-readable JSON** for hand-editing, **import from JSON** for loading authored worlds, **retry mechanism** with sub-seeds when placement guarantees fail, and **timeout/failure handling** that routes through CoW's Cancel path (GDD decision 5).

WorldGenerationPipeline enables CoW's testbed iteration workflow: generate → inspect → tweak parameters → regenerate, all from F-1's debug host without leaving the game (GDD decision 3). It resolves **PARK-P6-001** (worldgen timeout/failure behavior).

## Problem

CoW requires worlds that are:
- **Structurally correct** — three hard placement guarantees must hold (Hearth↔Nexus reachability, dynasty minimum spacing, world-throne existence/reachability) or the world is unplayable
- **Deterministic** — same seed always produces byte-for-byte identical output, including retry sequences
- **Authorable** — designers must be able to export a procedural world, hand-edit problematic placements in a text editor, and re-import the result
- **Tunable** — designers need fast iteration (generate → inspect → tweak → regenerate under 5 seconds) without switching to a separate tool
- **Validated** — reachability checks use injected navigation profiles (B2's body-plan rules plug in later), not naive graph connectivity
- **Failure-safe** — unlucky seeds or unsatisfiable parameters must not hang forever or silently return broken worlds

Without this system, every downstream testbed (S-2/S-3 ecology, S-4 dragons, S-5+ lineage/clans) would be blocked on placeholder worlds, and tuning world generation quality would require manual binary editing or separate standalone tools.

## Goals

1. **Seeded deterministic generation** — Same outer seed → same retry sequence → same world output, byte-for-byte identical across runs
2. **Three hard guarantees enforced** — Retry mechanism ensures every successfully-generated world satisfies: (a) every Hearth Site reachable from at least one Nexus Node, (b) dynasty starting positions respect minimum spacing, (c) all six world-throne sites exist (one per Apotheosis type) and are reachable from Ancient Interiors
3. **Export/import round-trip** — Export world to JSON, hand-edit, re-import as exact match (not a re-roll)
4. **Debug-adjustable parameters** — Seed, biome distribution weights, placement tuning exposed via F-1's debug host (no app restart needed)
5. **Timeout and failure handling** — Hard timeout prevents infinite hangs; unsatisfiable seeds route through Cancel's discard-and-return path with distinguishing assert/message
6. **Fast iteration** — Regeneration (seed or parameter change) completes under 5 seconds (GDD §7 metric)
7. **Debug-grade visualization** — Layered rendering (biome tiles, sites, regions, dynasty starts, design reference toggle) lets designers eyeball placement correctness without V-1's real tile art

## Non-Goals

1. **Real/tuned generation algorithm** — S-1 uses a placeholder (near-random biome assignment, naive site placement) that satisfies the three hard guarantees but doesn't aim for quality; real tuning is deferred to later playtesting
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
    GridDimensions dimensions;       // width × height in cells
    std::vector<Cell> cells;         // biome, terrain, region per cell
    std::vector<HearthSite> hearths; // 1-3 per region (GDD Phase 2 lore)
    std::vector<NexusNode> nexuses;  // count TBD by placeholder algorithm
    std::vector<WorldThrone> thrones;// exactly 6 (one per Apotheosis type)
    std::vector<DynastyStart> starts;// player + rivals (GDD Phase 2 rival count)
};

struct Cell {
    Biome biome;           // Phase 2 biome types
    TerrainType terrain;   // Phase 2 traversal/interaction types
    Region region;         // Phase 2 Ancient Interiors / Verdant Lowlands / etc.
};
```

**No ecology state** (A1's species populations, resource levels), **no dragon/clan/dynasty state** (B6/C1/C2 runtime) — structure only.

### Three Hard Guarantees

These must hold for every successfully-generated world (enforced by retry mechanism):

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

**Base layer (always on):** Biome-colored tiles

**Toggleable overlays:**
- Sites (Hearth/Nexus/world-throne markers)
- Regions (boundary overlays)
- Dynasty Starts (player + rival position markers)
- Design Reference (reads A1's species biome-affinity data, displays species-per-biome as text overlay — e.g., "Verdant Lowlands → deer (preferred), elk (preferred), wolf (preferred), cougar (tolerated)")

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
};

struct GenParams {
    std::map<Biome, float> biomeWeights;   // Distribution weights per biome
    PlacementParams placementParams;       // Site placement tuning (TBD by placeholder)
    uint32_t maxRetryAttempts;             // Bounded retry count (default TBD)
    float timeoutSeconds;                  // Hard timeout (default TBD)
};
```

### Events

**Published by WorldGenerationModule:**

```cpp
// Published on successful generation (GDD Phase 7)
struct WorldGenerationComplete {
    WorldStructure world;
    uint64_t seed;
    uint32_t retryAttempts;  // 0 if first attempt succeeded
};
// Subscribers: A2/GameSpeedController (resets to season 0, paused), A1/Ecology (initializes grid state), 
//              C4/Dynasty (schedules rival arrival waves)

// Published on timeout/failure
struct WorldGenerationFailed {
    FailureReason reason;  // TIMEOUT / UNSATISFIABLE / ERROR
    std::string message;   // Distinguishing assert text per GDD decision 5
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
    std::string filePath;
};

// Triggered by F-1's debug host "Import World" button
struct WorldImportRequested {
    std::string filePath;
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
| **DiaPathfinding** | `IPathCostProvider` injection point and `FindPath` for reachability. B2 is **not** a dependency: its body-plan profiles are injected later | Done |
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
| 1 | Placeholder Generation Algorithm | Biome assignment, terrain type assignment, region boundaries per GDD Phase 2 lore; site placement (Hearth/Nexus/world-throne/dynasty starts) implementing output contract | @placeholder-generation-algorithm.md | Approved |
| 2 | Reachability Validator | Runs injected world guarantees (three built-in hard guarantees) using injected navigation profiles (placeholder now, B2 body plans later); not naive graph connectivity | @reachability-validator.md | Approved |
| 3 | Retry Mechanism | Deterministic sub-seed retry (outer seed + attempt counter) when guarantees fail; bounded attempt count; same outer seed → same retry sequence | @retry-mechanism.md | Approved |
| 4 | JSON Export/Import | Human-readable JSON format capturing full world structure (grid, biomes, terrain, sites); round-trip guarantee (Import(Export(world)) == world) | @json-export-import.md | Approved |
| 5 | Timeout and Failure Handling | Hard timeout monitors total generation time; timeout/unsatisfiable/error route through Cancel's discard-and-return with distinguishing assert/message | @timeout-failure-handling.md | Approved |
| 6 | F-1 Debug Host Integration | Seed input, biome weight sliders, placement param sliders, Regenerate button, Export/Import UI, progress bar, Cancel button | @f1-debug-host-integration.md | Approved |
| 7 | Layered Debug Rendering | Base (biome-colored tiles always on) + toggles (Sites, Regions, Dynasty Starts, Design Reference reading A1 biome-affinity) | @layered-debug-rendering.md | Approved |

## Acceptance Criteria

### Test 1a — Generation Success Rate
Generate worlds across 100 random seeds. Record how many succeed (all three guarantees satisfied) vs. fallthrough to Failure (exhausted retry attempts). **Target: ≥95% success rate** (GDD §6, §7).

### Test 1b — Guarantee Compliance
For all successfully-generated worlds from Test 1a, confirm all three hard guarantees hold (Hearth↔Nexus reachability, dynasty minimum spacing, world-throne existence/reachability). **Target: 100% compliance** — zero violations among successful generations (GDD §6, §8).

### Test 2a — Generation Determinism
Generate a world with a specific seed, record output, regenerate with same seed. Confirm byte-for-byte identical (every cell's biome/terrain/region, every site placement). Test on ≥10 seeds, including at least one that triggers retries. **Target: 100% match** (GDD §4, §6).

### Test 2b — Debug-Adjustable Regeneration
Via F-1's debug host, change seed, then separately change a biome weight, regenerating after each change. Confirm each regeneration reflects changed input and completes within **under 5 seconds** (GDD §7 metric).

### Test 3 — Export/Import Round Trip
Generate world, export to JSON, reload. Confirm reloaded world is identical to exported one (not re-roll of same seed, not similar-but-different). **Target: exact match, every field** (GDD §6, §7).

### Test 4 — Timeout Path
Force generation past hard timeout (injected delay). Confirm auto-cancels through discard-and-return with assert/log message explicitly identifying "TIMEOUT", distinct from manual Cancel and Failure (GDD §6, §8).

### Test 5 — Failure Path
Force generation error (not merely slowness). Confirm routes through discard-and-return with assert/log message explicitly identifying "FAILURE", distinct from timeout and manual Cancel (GDD §6, §8).

### Test 5a — Guarantee Retry and Fallthrough
Force seed whose first placement violates a guarantee (e.g., narrow map making Hearth↔Nexus unreachable on first try). Confirm generation retries with sub-seed and either succeeds within bounded attempts, or — for engineered-to-never-succeed seed — falls through to Failure rather than returning world that violates guarantee. Confirm outer seed produces same retry sequence and outcome every time (GDD §6).

## Inherited Binding Decisions

The following binding decisions from parent specs apply to this system:

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | Use StringCRC for all entity/component IDs | Biome, TerrainType, Region enums use StringCRC for comparison; site types use StringCRC IDs |
| PD-002 | ProcessingUnit/Phase/Module architecture | WorldGenerationModule lives on SimPU (generation is deterministic sim-time work, not wall-clock); F-1 debug host lives on MainPU (UI input) |
| PD-004 | No STL containers in public APIs | WorldGenerationModule API uses Dia containers (DynamicArrayC, HashTable) for WorldStructure fields, not std::vector/std::map |
| PD-007 | C++20 is required | Placeholder generation can use C++20 features (concepts, std::span, constexpr improvements) |
| PD-009 | All non-binary output under `Cluiche/out/<AppName>/` | Exported `.wld.json` files default to `Cluiche/out/CoW/worlds/` (user can override path via debug host) |
| PD-010 | `.diagame` is project root; typed imports route loading | S-1 stage declared in `CoW.diagame` as a `stage` import; `.diastage` contains stage metadata (name, manifest path) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-001 | CoW edits to Dia require ask permission | If WLD hits a gap in Dia (e.g., RNG API missing feature, serialization helper needed), flag for approval before editing Dia; propose capability, don't silently add it |
| AD-002 | Classify cross-cutting needs (Dia / CluicheGameBaseline / CoW-Shared / Feature-local) | WorldGenerationModule is **CoW-Shared** (reused by S-2/S-3/S-4 stages but specific to CoW's mechanics); placeholder algorithm is **Feature-local** (stays in S-1 until proven reusable) |
| AD-003 | GDD owns design intent; spec tree owns implementation | GDD `S1_WorldGeneration.md` defines pass/fail metrics, three hard guarantees, testbed purpose; this spec defines WorldGenerationModule API, retry mechanism implementation, event contracts |
| AD-004 | Promote BootMenuModule; dev builds boot to debug menu | S-1 stage launches from F-1's debug menu (no `--s1` flag; menu selection only); relies on promoted BootMenuModule being in CluicheGameBaseline |
| AD-005 | One stage per testbed; modes chosen in-panel | S-1 is one `.diastage` listed as Boot transition; if S-1 later gains modes (e.g., "Single Cell" vs "Living Ecology"), they're chosen via in-stage panel, not separate stages |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What are the exact placeholder algorithm parameters? (biome distribution weights, minimum-spacing value, reachability-check implementation, bounded retry-attempt count, hard timeout value) | Affects GenParams struct and default values; doesn't block S-1 implementation (these are tunable defaults) | Leave as tunable defaults, not fixed in spec; designers adjust via F-1's debug host during S-1 challenge pass |
| 2 | ~~Is B2's terrain traversability data externalized yet?~~ | Was blocking | **Resolved 2026-10-13:** navigability is injected via `INavigationProfile` (DiaPathfinding `IPathCostProvider`); S-1 ships a placeholder profile and no longer waits on B2. See reachability-validator.md |
| 3 | Is A1's species biome-affinity data externalized? | Design Reference toggle only; can cut if not externalized without blocking S-1 core pipeline | Optional — if not externalized by S-1 Feature 7 implementation, cut the Design Reference toggle (non-blocking) |
| 4 | Should a future standalone WorldDesign (CluicheEditor-based) toolset be built? | Out of scope for S-1 (GDD decision 3 chose in-game iteration), but GDD leaves this open as "genuinely open" for later | Deferred — S-1 proves in-game iteration first; revisit after S-1 challenge pass shows iteration speed/ergonomics |

## Status

`Approved` — 2026-10-11

---

## Implementation Notes

When implementation begins (after approval), create a plan file `worldgenerationpipeline.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- WorldGenerationModule class structure (init/shutdown, API entry points)
- Retry mechanism state machine (attempt counter, sub-seed derivation)
- JSON schema for export format (cell array, site arrays, version tag)
- Event wiring (which events WorldGenerationModule publishes/subscribes, handler signatures)
- Debug rendering layer registration (base layer + toggle IDs for F-1's host)

These patterns are validated against actual implementation before tasks are marked Done.

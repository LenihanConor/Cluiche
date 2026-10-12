# Feature Spec: Placeholder Generation Algorithm

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

The Placeholder Generation Algorithm generates CoW world structure (grid with biomes/terrain/regions, plus Hearth/Nexus/world-throne/dynasty-start site placements) using a seeded, deterministic algorithm. It produces mechanically correct worlds that satisfy the structural requirements for the three hard guarantees to be validated, but is intentionally not tuned for quality — real generation tuning is deferred to later playtesting after downstream systems (S-2/S-3 ecology, S-4 dragons) exist to validate against.

This feature is the data generation core of WLD: it takes a seed and `GenParams`, performs biome assignment (weighted random per biome distribution weights), terrain-type assignment, region boundary assignment per Phase 2 lore, and site placement (Hearth/Nexus/world-throne/dynasty starts), then returns a complete `WorldStructure` ready for Feature 2 (Reachability Validator) to check.

## Problem

CoW's testbed iteration workflow requires worlds that are:
- **Structurally complete** — grid fully populated with biomes/terrain/regions, all required site types placed (Hearth/Nexus/world-thrones/dynasty starts)
- **Deterministic** — same seed + same parameters always produces byte-for-byte identical output (including intermediate state like RNG call sequences)
- **Fast to generate** — completes under 5 seconds (excluding validation/retry) so designers can tweak parameters and regenerate without flow disruption
- **Mechanically correct** — produces output that can satisfy the three hard guarantees (Hearth↔Nexus reachability, dynasty minimum spacing, world-throne existence/reachability) when placement is lucky, or at least enables the retry mechanism (Feature 3) to detect violations

Without this generator, S-1 cannot prove WLD's data pipeline, and every downstream testbed (S-2/S-3/S-4/S-5/S-6) would be blocked on placeholder worlds or forced to use hand-authored-only data.

## Goals

1. **Deterministic generation** — Same seed + same `GenParams` → same `WorldStructure` output, byte-for-byte identical across runs
2. **Complete output contract** — Produces all fields of `WorldStructure` (dimensions, cells array with biome/terrain/region per cell, Hearth/Nexus/world-throne/dynasty-start site arrays)
3. **Weighted biome distribution** — Applies `GenParams.biomeWeights` to biome assignment (not uniform-random — designers must be able to favor certain biomes)
4. **Phase 2 lore compliance** — Region boundaries and Hearth-per-region counts match Phase 2's Ancient Interiors / Verdant Lowlands / etc. structure (1–3 Hearths per region per GDD lore)
5. **Six world-thrones exactly** — Places exactly 6 world-throne sites (one per Apotheosis type, per GDD §2 output contract)
6. **Naive placement correctness** — Site placement is not tuned for quality but must be mechanically valid (no out-of-bounds, no duplicate positions, all arrays correctly sized)
7. **Fast generation** — Completes under 5 seconds for typical parameter ranges (grid size, site counts) excluding validation/retry time

## Non-Goals

1. **Tuned/quality generation** — Placeholder algorithm is near-random biome assignment + naive site placement; real tuning (interesting region shapes, good Hearth distribution, natural-looking biome transitions) is deferred to later playtesting (GDD decision 1)
2. **Guarantee validation** — This feature generates worlds; Feature 2 (Reachability Validator) checks the three hard guarantees
3. **Retry logic** — This feature performs one generation attempt; Feature 3 (Retry Mechanism) orchestrates retries when guarantees fail
4. **Export/import** — Feature 4's responsibility
5. **Timeout handling** — Feature 5's responsibility
6. **Debug UI integration** — Feature 6's responsibility
7. **Rendering** — Feature 7's responsibility

## Architecture

### Algorithm Phases

The placeholder generator runs five sequential phases per generation:

```cpp
WorldStructure Generate(uint64_t seed, const GenParams& params) {
    // Phase 1: Grid initialization
    // Phase 2: Biome assignment (weighted random per params.biomeWeights)
    // Phase 3: Terrain-type assignment (derived from biome)
    // Phase 4: Region boundary assignment (Phase 2 lore patterns)
    // Phase 5: Site placement (Hearth/Nexus/world-throne/dynasty starts)
    return WorldStructure{...};
}
```

Each phase consumes the seed via RandomStreamManager, ensuring determinism.

### Biome Assignment (Phase 2)

**Input:** `params.biomeWeights` (map of Biome → float weight)

**Algorithm:** For each grid cell, weighted-random selection from biome types using provided weights. Phases 2 defines biome enum (exact types TBD in Phase 2 GDD material, not fixed here).

**Determinism:** Seeded RNG stream per cell or single stream with cell-count draws — either is acceptable as long as same seed → same biome grid.

### Terrain-Type Assignment (Phase 3)

**Input:** Biome per cell (Phase 2 output)

**Algorithm:** Map biome → terrain-type via lookup table (e.g., "Forest" biome → "Foliage" terrain type, "Mountain" biome → "Rock" terrain type). Exact biome-to-terrain mappings TBD by Phase 2 data design, placeholder uses arbitrary 1:1 mapping.

**Purpose:** Terrain types drive B2's traversability rules (body-plan block matrix), required for Feature 2 reachability validation.

### Region Boundary Assignment (Phase 4)

**Input:** Grid with biomes/terrain per cell

**Algorithm:** Partition grid into Phase 2 regions (Ancient Interiors, Verdant Lowlands, etc.) using placeholder logic (e.g., divide grid into N roughly-equal zones, assign region IDs). Exact region count and names from Phase 2 lore; placeholder implementation just proves the data flow (region per cell populated).

**GDD §2 constraint:** 1–3 Hearth Sites per region (enforced in Phase 5 site placement, not here).

### Site Placement (Phase 5)

**Input:** Grid with biomes/terrain/regions per cell

**Algorithm:**

1. **Hearth Sites:** For each region, place 1–3 Hearths (random count within [1, 3]) at random valid cells within that region (avoid out-of-bounds, avoid cell already occupied by another site type)
2. **Nexus Nodes:** Place N Nexus Nodes at random valid cells (count TBD by `params.placementParams`, exact value left as tunable default per GDD open question)
3. **World-Thrones:** Place exactly 6 world-throne sites at random valid cells, one per Apotheosis type (exact Apotheosis enum from Phase 2, placeholder assigns arbitrary type per throne)
4. **Dynasty Starts:** Place player + rival starting positions (rival count per Phase 2's rival count, TBD); each at random valid cell

**Validity rules (all site types):**
- Within grid bounds
- Not on same cell as another site (any type)
- Placeholder does not enforce minimum spacing here — Feature 2 validates dynasty spacing as one of the three hard guarantees

**Determinism:** Seeded RNG for all random choices (counts, positions, types).

### Data Types

```cpp
// Output contract (from system spec)
struct WorldStructure {
    GridDimensions dimensions;       // width × height in cells
    DynamicArrayC<Cell> cells;       // biome, terrain, region per cell (PD-004)
    DynamicArrayC<HearthSite> hearths;
    DynamicArrayC<NexusNode> nexuses;
    DynamicArrayC<WorldThrone> thrones; // exactly 6
    DynamicArrayC<DynastyStart> starts;
};

struct Cell {
    Biome biome;           // Phase 2 biome types (enum TBD)
    TerrainType terrain;   // Phase 2 traversal/interaction types (enum TBD)
    Region region;         // Phase 2 regions (enum TBD)
};

struct HearthSite {
    GridCoord position;    // cell coordinates
    Region parentRegion;   // which region this Hearth belongs to (lore constraint)
};

struct NexusNode {
    GridCoord position;
};

struct WorldThrone {
    GridCoord position;
    ApotheosisType type;   // Phase 2 Apotheosis types (enum TBD)
};

struct DynastyStart {
    GridCoord position;
    DynastyRole role;      // PLAYER vs. RIVAL (enum)
};

// Input parameters (from system spec)
struct GenParams {
    std::map<Biome, float> biomeWeights;   // Distribution weights per biome
    PlacementParams placementParams;       // Site placement tuning (TBD by placeholder)
    uint32_t maxRetryAttempts;             // Not used by this feature (Feature 3)
    float timeoutSeconds;                  // Not used by this feature (Feature 5)
};

struct PlacementParams {
    uint32_t nexusNodeCount;               // How many Nexus Nodes to place (tunable default)
    // Other tuning knobs TBD (spacing hints, clustering params, etc.)
};
```

**PD-004 compliance:** Use `DynamicArrayC<T>`, not `std::vector<T>`, in `WorldStructure` (public API).

## Public Interfaces

### APIs

```cpp
// PlaceholderGenerator.h (CoW/Shared/WorldGeneration/)
namespace CoW::WorldGeneration {

class PlaceholderGenerator {
public:
    // Generate world structure from seed + parameters
    // Single-attempt only; retry orchestration is Feature 3's responsibility
    // Blocking call; timeout monitoring is Feature 5's responsibility
    static WorldStructure Generate(uint64_t seed, const GenParams& params);

private:
    static void InitializeGrid(WorldStructure& world, const GenParams& params);
    static void AssignBiomes(WorldStructure& world, uint64_t seed, const GenParams& params);
    static void AssignTerrainTypes(WorldStructure& world);
    static void AssignRegions(WorldStructure& world);
    static void PlaceSites(WorldStructure& world, uint64_t seed, const GenParams& params);
};

} // namespace CoW::WorldGeneration
```

**Rationale:** Static generator (no instance state needed for placeholder algorithm). When real tuning adds stateful generation (e.g., noise generators, caching), refactor to instance-based.

### Events

**None.** This feature is a pure function (seed + params → world structure). Feature 6 (F-1 Debug Host Integration) publishes events when user triggers regeneration; this feature just implements the callable.

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **F-2 Core Runtime** | RandomStreamManager for seeded RNG determinism | Approved (backlog) |
| **DiaSimTime** | RandomStreamManager API | Done |
| **Phase 2 data design** | Biome enum, TerrainType enum, Region enum, ApotheosisType enum, rival count | GDD in-progress (types TBD — placeholder uses arbitrary enums until Phase 2 finalizes) |

### Dia Engine Dependencies

- **DiaCore** (containers: `DynamicArrayC`, CRC for StringCRC if needed, file I/O if reading authored defaults)
- **DiaSimTime** (`RandomStreamManager` for seeded RNG per F-2 determinism contract)
- **DiaMaths** (`Vector2`, `GridCoord` if distinct type, bounds checks)

## Acceptance Criteria

### AC1 — Deterministic Generation
Generate a world with a specific seed and parameters, record the output. Regenerate with the same seed and parameters. Confirm byte-for-byte identical output (every cell's biome/terrain/region, every site's position/type). Test on ≥3 seeds. **Pass:** 100% match across all seeds.

### AC2 — Complete Output Contract
Generate a world. Confirm `WorldStructure` contains:
- Non-zero `dimensions` (width, height)
- `cells.size() == dimensions.width * dimensions.height`
- Every cell has valid (non-zero/non-default) `biome`, `terrain`, `region`
- `hearths.size() >= region_count` (at least 1 per region) and `<= 3 * region_count` (at most 3 per region, per GDD lore)
- `nexuses.size() > 0` (at least some Nexus Nodes placed)
- `thrones.size() == 6` (exactly 6 world-thrones, per GDD §2 output contract)
- `starts.size() >= 1` (player at minimum) and includes at least one `role == PLAYER`

**Pass:** All fields populated correctly on ≥5 random seeds.

### AC3 — Weighted Biome Distribution
Generate world with `biomeWeights = {Forest: 0.8, Mountain: 0.2}` (arbitrary example biomes). Count cells per biome type. Confirm Forest cells significantly outnumber Mountain cells (tolerance: ±15% from expected ratio due to random variance). Repeat with opposite weights `{Forest: 0.2, Mountain: 0.8}` and confirm distribution flips. **Pass:** Weighted selection honors provided weights within tolerance.

### AC4 — Site Placement Validity
Generate world. For each site array (hearths, nexuses, thrones, starts), confirm:
- Every site's position is within grid bounds
- No two sites (across all arrays) share the same grid cell (no position collisions)

**Pass:** Zero out-of-bounds or collision violations on ≥5 random seeds.

### AC5 — Six World-Thrones Always
Generate world on ≥10 random seeds with varying `GenParams`. Confirm `thrones.size() == 6` every time. Confirm each throne has a distinct `type` (one per Apotheosis type, no duplicates). **Pass:** 100% compliance (exactly 6 thrones, all distinct types).

### AC6 — Generation Speed
Generate worlds with typical parameters (128×128 grid, biome weights, site counts in expected ranges per placeholder defaults). Measure time from `Generate()` call to return. **Target: <5 seconds** (GDD §7 metric). Excludes validation/retry time (those are other features).

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | Use StringCRC for all entity/component IDs | Biome, TerrainType, Region, ApotheosisType enums use StringCRC for comparison (placeholder uses arbitrary CRCs until Phase 2 data design finalizes names) |
| PD-004 | No STL containers in public APIs | `WorldStructure` uses `DynamicArrayC<T>`, not `std::vector<T>`, for all arrays |
| PD-007 | C++20 is required | Placeholder generator can use C++20 features (std::span for cell grids, concepts for RNG traits if needed) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | `PlaceholderGenerator` is **Feature-local** initially (stays in S-1); if S-2/S-3 reuse it, promote to **CoW-Shared** (already in that directory but scoped to WLD). Not Dia (too CoW-specific), not Baseline (no other game needs CoW's biome/region/site logic) |
| AD-003 | GDD owns design intent | GDD defines "placeholder is naive, not tuned" (decision 1), three hard guarantees (§2), and Phase 2 lore (region structure, Hearth counts). This spec defines the implementation (algorithm phases, data types, determinism via RandomStreamManager) |

### From System (WorldGenerationPipeline)

| ID | Decision | How it applies |
|----|----------|----------------|
| N/A | No system-level binding decisions in parent spec | System spec has Open Design Questions but no SD- binding decisions |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What are the exact default values for `GenParams` (grid dimensions, biome weights, nexusNodeCount, dynasty rival count)? | Affects test data for ACs and initial debug host sliders; does not block implementation (use arbitrary defaults, tune via F-1 debug host during S-1 challenge pass) | Leave as tunable defaults in `PlaceholderGenerator::Generate()` or loaded from authored config file; GDD open question 1 explicitly defers this |
| 2 | What are Phase 2's final biome/terrain/region/Apotheosis enums? | Placeholder currently uses arbitrary placeholder enums; when Phase 2 finalizes, update to real names | Unblocking: use placeholder enums now (`enum class Biome { Placeholder1, Placeholder2, ... }`), refactor to Phase 2 names when GDD data design completes |

## Status

`Approved` — 2026-10-11

---

## Implementation Notes

When implementation begins (after approval), create a plan file `placeholder-generation-algorithm.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- Phase sequencing (which phase mutates which fields of `WorldStructure`, order dependencies)
- RNG stream usage (single global stream vs. per-phase streams, draw count per phase for determinism audit)
- Weighted biome selection implementation (cumulative-weight table vs. alias method)
- Site placement collision avoidance (occupied-cell set vs. grid marking)
- Default `GenParams` values (grid size, biome weights, site counts) used for testing

These patterns are validated against actual implementation before tasks are marked Done.

# Feature Spec: Reachability Validator

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

The Reachability Validator validates the three hard placement guarantees that every successfully-generated CoW world must satisfy: (1) every Hearth Site is reachable from at least one Nexus Node, (2) every pair of dynasty starting positions respects minimum spacing, (3) all six world-throne sites exist and are each reachable from the Ancient Interiors. Validation uses **dragon-traversable paths** (B2's body-plan terrain block matrix), not naive graph connectivity — "reachable" means at least one dragon body plan can traverse the path.

This feature prevents Feature 1 (Placeholder Generation Algorithm) from returning playable-looking but mechanically broken worlds. It enables Feature 3 (Retry Mechanism) to detect guarantee violations and trigger sub-seed retries.

## Problem

Without validation, CoW worlds could be:
- **Unplayable** — Hearth Sites unreachable from Nexus Nodes means dragons can't migrate to those Hearths (breaks Phase 2 lore's migration flows)
- **Unfair** — Dynasty starts too close together means rivals can rush the player before they've built up (breaks Phase 2 balance)
- **Unwinnable** — Missing or unreachable world-thrones means Apotheosis is impossible (violates Constitution §8, makes late-game unachievable)
- **Silently broken** — Feature 1's naive placeholder algorithm can easily produce these violations; without validation, they'd only be discovered during S-4/S-5/S-6 manual playtesting, wasting iteration time

Naive graph connectivity is insufficient: a graph-connected path might traverse terrain no dragon body plan can cross (e.g., water for land-only dragons, cliffs for wingless dragons). B2's body-plan block matrix defines which terrain types each body plan can traverse — reachability must respect these rules.

## Goals

1. **Three hard guarantees enforced** — Validates all three guarantees (Hearth↔Nexus reachability, dynasty minimum spacing, world-throne existence/reachability); returns pass/fail per guarantee and overall pass/fail
2. **Dragon-traversable reachability** — Uses B2's body-plan terrain block matrix to determine "can at least one dragon body plan path from A to B" (not naive graph connectivity ignoring terrain)
3. **Fast validation** — Completes within generation's 5-second budget (reachability checks for typical world sizes must be efficient, not exhaustive search)
4. **Clear failure reporting** — When a guarantee fails, returns which guarantee and diagnostic info (e.g., "Hearth at (X, Y) unreachable from any Nexus", "Dynasty starts at (X1, Y1) and (X2, Y2) too close: distance D < minimum M")
5. **Deterministic** — Same world structure always produces same validation result (no randomness in path-finding)

## Non-Goals

1. **Tuned generation** — This feature validates worlds; Feature 1 generates them
2. **Retry logic** — This feature returns pass/fail; Feature 3 orchestrates retries when validation fails
3. **Pathfinding API for runtime** — This is validation-time reachability only; S-4's actual dragon pathfinding is a separate system
4. **Optimal paths** — Any valid path proving reachability is sufficient; shortest/best path not required
5. **Reachability for other game mechanics** — Only validates the three hard guarantees, not general-purpose "can entity X reach tile Y"

## Architecture

### Validation Flow

```cpp
struct ValidationResult {
    bool passed;  // Overall: all three guarantees satisfied
    GuaranteeResult hearthNexusReachability;
    GuaranteeResult dynastySpacing;
    GuaranteeResult worldThroneExistence;
};

struct GuaranteeResult {
    bool satisfied;
    DynamicArrayC<std::string> diagnostics;  // Empty if satisfied; failure details if not
};

ValidationResult Validate(const WorldStructure& world, const ValidationParams& params);
```

**Three-stage validation:**

1. **Hearth↔Nexus Reachability:** For each Hearth Site, attempt to find a dragon-traversable path from that Hearth to any Nexus Node. If any Hearth is unreachable from all Nexuses, guarantee fails.

2. **Dynasty Minimum Spacing:** For each pair of dynasty starting positions, compute grid distance. If any pair's distance < `params.minimumDynastySpacing`, guarantee fails.

3. **World-Throne Existence/Reachability:** Confirm exactly 6 world-thrones exist (one per Apotheosis type). For each throne, attempt to find a dragon-traversable path from that throne to any Ancient Interiors cell (region enum check). If any throne is unreachable, guarantee fails.

**Short-circuit:** All three guarantees checked independently (don't stop at first failure — report all violations for better diagnostics).

### Dragon-Traversable Reachability

**Data source:** B2's body-plan terrain block matrix (GDD §2, system spec Open Question #2). Format TBD by B2 spec, assumed to be an authored file (e.g., JSON/CSV) mapping `(BodyPlan, TerrainType) → bool canTraverse`.

**Algorithm:**

```cpp
bool IsReachable(GridCoord from, GridCoord to, const WorldStructure& world, const TraversabilityMatrix& matrix) {
    // Attempt pathfinding for each body plan in matrix
    for (auto bodyPlan : matrix.GetAllBodyPlans()) {
        if (PathExists(from, to, world, bodyPlan, matrix)) {
            return true;  // At least one body plan can traverse
        }
    }
    return false;  // No body plan can reach destination
}

bool PathExists(GridCoord from, GridCoord to, const WorldStructure& world, BodyPlan plan, const TraversabilityMatrix& matrix) {
    // BFS or A* with terrain-type traversability check per body plan
    // For each neighbor cell, check: can this body plan traverse the cell's terrain type?
    // If matrix.CanTraverse(plan, cell.terrain) == false, skip that neighbor
}
```

**Rationale:** Checking all body plans (not just one arbitrary plan) ensures "playable" — if zero body plans can reach a site, it's mechanically inaccessible (player can never breed/evolve a dragon that reaches it). If at least one plan can reach it, the player has a path (even if constrained).

### Minimum Dynasty Spacing

**Algorithm:** Grid distance (Manhattan or Euclidean, TBD by `ValidationParams`).

```cpp
bool CheckDynastySpacing(const DynamicArrayC<DynastyStart>& starts, float minimumSpacing) {
    for (size_t i = 0; i < starts.size(); ++i) {
        for (size_t j = i + 1; j < starts.size(); ++j) {
            float distance = GridDistance(starts[i].position, starts[j].position);
            if (distance < minimumSpacing) {
                return false;  // Violation found
            }
        }
    }
    return true;  // All pairs respect minimum
}
```

**Exact `minimumSpacing` value:** Tunable default per GDD open question 1, left as `ValidationParams` field (not hardcoded).

### World-Throne Existence/Reachability

**Existence check:** `world.thrones.size() == 6` and all six Apotheosis types present (one per type, no duplicates).

**Reachability check:** For each throne, find any cell with `cell.region == Region::AncientInteriors`, then check `IsReachable(throne.position, ancientInteriorsCell, world, matrix)`.

**Assumption:** Ancient Interiors is a contiguous region (if multiple disjoint regions exist, any cell from any of them suffices for "reachable from Ancient Interiors").

## Public Interfaces

### APIs

```cpp
// ReachabilityValidator.h (CoW/Shared/WorldGeneration/)
namespace CoW::WorldGeneration {

struct ValidationParams {
    float minimumDynastySpacing;  // Grid units (tunable default, GDD open question 1)
    // Future: other tuning knobs (timeout per reachability check, max path length, etc.)
};

struct ValidationResult {
    bool passed;  // Overall: all three guarantees satisfied
    GuaranteeResult hearthNexusReachability;
    GuaranteeResult dynastySpacing;
    GuaranteeResult worldThroneExistence;
};

struct GuaranteeResult {
    bool satisfied;
    DynamicArrayC<std::string> diagnostics;  // Empty if satisfied; failure details if not
};

class ReachabilityValidator {
public:
    // Validate world against three hard guarantees
    // Blocking call; completes within generation's 5-second budget for typical worlds
    static ValidationResult Validate(const WorldStructure& world, const ValidationParams& params);

private:
    static GuaranteeResult ValidateHearthNexusReachability(const WorldStructure& world);
    static GuaranteeResult ValidateDynastySpacing(const WorldStructure& world, const ValidationParams& params);
    static GuaranteeResult ValidateWorldThroneExistence(const WorldStructure& world);
    
    // Dragon-traversable reachability (uses B2 traversability matrix)
    static bool IsReachable(GridCoord from, GridCoord to, const WorldStructure& world);
};

} // namespace CoW::WorldGeneration
```

**Rationale:** Static validator (no instance state needed for validation). Traversability matrix loaded once per validation call (or cached globally if loading is expensive).

### Events

**None.** This feature is a pure validator (world → validation result). Feature 3 (Retry Mechanism) calls it per generation attempt; Feature 6 (Debug Host Integration) may display results in UI.

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **Feature 1 (Placeholder Generation Algorithm)** | Produces `WorldStructure` to validate | Draft (being specced) |
| **B2 terrain traversability data** | Body-plan block matrix as authored file (defines which terrain types each body plan can traverse) | **Blocking:** GDD open question #3 and system spec Open Question #2 — must externalize B2's traversability rules before this feature can validate correctly |
| **Phase 2 data design** | Region enum (Ancient Interiors), ApotheosisType enum (6 types), BodyPlan enum (body-plan list for traversability matrix) | GDD in-progress (types TBD — placeholder uses arbitrary enums until Phase 2 finalizes) |

### Dia Engine Dependencies

- **DiaCore** (containers: `DynamicArrayC`, file I/O for loading traversability matrix)
- **DiaMaths** (`GridCoord`, distance calculations, bounds checks)
- **Pathfinding utility** (BFS or A* for reachability checks — either implement locally or use future Dia pathfinding module if available; S-4 will build dragon pathfinding later, but validator doesn't need full A* features, just "does path exist")

## Acceptance Criteria

### AC1 — Hearth↔Nexus Reachability Validation
Generate world where all Hearth Sites are manually confirmed reachable from at least one Nexus Node (via visual inspection or hand-authored test world). Validate. **Pass:** `hearthNexusReachability.satisfied == true`.

Generate world where at least one Hearth is unreachable (hand-edit test world to create isolated island with Hearth, no Nexus). Validate. **Pass:** `hearthNexusReachability.satisfied == false` and `diagnostics` contains Hearth position.

### AC2 — Dynasty Spacing Validation
Generate world with dynasty starts placed ≥ minimum spacing apart. Validate with `minimumDynastySpacing = 10` (arbitrary value). **Pass:** `dynastySpacing.satisfied == true`.

Generate world with two dynasty starts placed < minimum spacing apart (hand-edit test world). Validate with same `minimumDynastySpacing`. **Pass:** `dynastySpacing.satisfied == false` and `diagnostics` contains both positions and actual distance.

### AC3 — World-Throne Existence/Reachability Validation
Generate world with exactly 6 world-thrones, all reachable from Ancient Interiors. Validate. **Pass:** `worldThroneExistence.satisfied == true`.

Generate world with < 6 world-thrones (hand-edit test world). Validate. **Pass:** `worldThroneExistence.satisfied == false` and `diagnostics` mentions count mismatch.

Generate world with 6 world-thrones but one unreachable from Ancient Interiors (hand-edit test world to place throne on isolated island). Validate. **Pass:** `worldThroneExistence.satisfied == false` and `diagnostics` contains throne position.

### AC4 — Overall Pass/Fail
Generate world where all three guarantees pass. Validate. **Pass:** `ValidationResult.passed == true`.

Generate world where at least one guarantee fails. Validate. **Pass:** `ValidationResult.passed == false`.

### AC5 — Dragon-Traversable Reachability (Not Naive Graph Connectivity)
Create test world with path from A to B that is graph-connected but blocked for all body plans (e.g., water tiles between A and B, and traversability matrix says no body plan can traverse water). Validate reachability from A to B. **Pass:** `IsReachable(A, B) == false` (proves validator respects traversability matrix, not just grid adjacency).

Create test world with path from A to B that is traversable by at least one body plan per matrix. Validate. **Pass:** `IsReachable(A, B) == true`.

### AC6 — Validation Speed
Generate worlds with typical sizes (128×128 grid, ~10 Hearth Sites, ~5 Nexus Nodes, 6 world-thrones, ~5 dynasty starts). Measure time from `Validate()` call to return. **Target: completes within 5-second generation budget** (exact fraction TBD, but must not dominate generation time — target <1 second for typical worlds).

### AC7 — Deterministic Validation
Generate world, validate twice with identical `ValidationParams`. **Pass:** Both validation results identical (same `passed` value, same `diagnostics` per guarantee). Proves no randomness in pathfinding or distance calculations.

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | Use StringCRC for all entity/component IDs | Region enum (Ancient Interiors), ApotheosisType, BodyPlan enums use StringCRC for comparison when loading from traversability matrix file |
| PD-004 | No STL containers in public APIs | `ValidationResult` and `GuaranteeResult` use `DynamicArrayC<std::string>` for diagnostics, not `std::vector<std::string>` |
| PD-007 | C++20 is required | Validator can use C++20 features (std::span for grid access, concepts for pathfinding traits if needed) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | `ReachabilityValidator` is **CoW-Shared** (reused by S-1 retry mechanism, potentially S-2/S-3 if they regenerate worlds). Not Dia (too CoW-specific: uses CoW's Hearth/Nexus/world-throne concepts). Not Baseline (no other game needs CoW's three hard guarantees) |
| AD-003 | GDD owns design intent | GDD defines three hard guarantees (§2), "reachable means dragon-traversable not graph-connected" (decision 2), and B2 body-plan traversability dependency (open question 3). This spec defines validation algorithm (BFS per body plan, distance checks, diagnostic reporting) |

### From System (WorldGenerationPipeline)

| ID | Decision | How it applies |
|----|----------|----------------|
| N/A | No system-level binding decisions in parent spec | System spec has Open Design Questions but no SD- binding decisions |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What is the exact `minimumDynastySpacing` value? | Affects AC2 test data and Feature 3 retry success rate; does not block implementation (use arbitrary default, tune via F-1 debug host) | GDD open question 1 defers this; leave as `ValidationParams` field with placeholder default (e.g., 10.0 grid units) |
| 2 | Is B2's terrain traversability data (body-plan block matrix) externalized as an authored file yet? | **Blocking:** Cannot implement dragon-traversable reachability without this data | **Must resolve before implementation:** If B2 traversability rules aren't externalized, that's a Cluiche-side task (log to `dia-needs.md` per AD-002) before Feature 2 can be built. This is system spec Open Question #2. |
| 3 | Should reachability use BFS (guaranteed shortest path, slower) or greedy best-first (faster but not guaranteed shortest)? | Affects AC6 validation speed; does not affect correctness (any valid path proves reachability) | Placeholder uses BFS (simpler, correctness-first); optimize to A* or greedy if AC6 fails speed target |
| 4 | Should validator cache traversability matrix globally (loaded once at module init) or per-call (reload each validation)? | Affects validation speed if matrix loading is expensive | Load once per `Validate()` call initially; profile and add global cache if loading dominates validation time |

## Status

`Approved` — 2026-10-11

---

## Implementation Notes

When implementation begins (after approval), create a plan file `reachability-validator.plan.md` alongside this spec. The plan must include an **Implementation Patterns** section documenting:

- Traversability matrix file format (JSON schema, loading code, caching strategy)
- Pathfinding algorithm choice (BFS vs. A* vs. Dijkstra for reachability checks)
- Diagnostic message format (how to report unreachable Hearth/throne positions for Feature 3 retry logs)
- Distance calculation (Manhattan vs. Euclidean for dynasty spacing)
- Timeout/early-exit for pathfinding (if any reachability check runs too long, how to handle)

These patterns are validated against actual implementation before tasks are marked Done.

**Blocker resolution:** Coordinate with user on B2 traversability data externalization before starting implementation. If B2 data not ready, log to `docs/specs/applications/cow/dia-needs.md` per AD-002 and defer Feature 2 until data available.

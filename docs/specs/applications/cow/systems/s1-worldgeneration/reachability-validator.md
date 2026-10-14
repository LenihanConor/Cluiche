# Feature Spec: Reachability Validator

**Parent:** @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md

**Gameplay Domains:** simulation

## Summary

The Reachability Validator checks a generated `WorldStructure` against a set of **injected world guarantees** (`IWorldGuarantee`). S-1 ships three built-in guarantees that every successfully-generated CoW world must satisfy: (1) every Hearth Site is reachable from at least one Nexus Node, (2) every pair of dynasty starting positions respects minimum spacing, (3) all six world-throne sites exist and are each reachable from the Ancient Interiors. Other systems (A1 ecological viability, C1 territory, D4 regions) can add guarantees later without editing S-1.

"Reachable" is never naive graph connectivity. It is answered under injected **navigation profiles** (`INavigationProfile`): a profile supplies a Dia `IPathCostProvider` bound to the world, and two cells are reachable if a path exists under **any** injected profile. S-1 ships a placeholder terrain-table profile; B2 (Dragon) later injects one profile per body plan. The validator never knows what a body plan is.

This feature prevents Feature 1 (Placeholder Generation Algorithm) from returning playable-looking but mechanically broken worlds. It enables Feature 3 (Retry Mechanism) to detect guarantee violations and trigger sub-seed retries.

## Problem

Without validation, CoW worlds could be:
- **Unplayable** — Hearth Sites unreachable from Nexus Nodes means dragons can't migrate to those Hearths (breaks Phase 2 lore's migration flows)
- **Unfair** — Dynasty starts too close together means rivals can rush the player before they've built up (breaks Phase 2 balance)
- **Unwinnable** — Missing or unreachable world-thrones means Apotheosis is impossible (violates Constitution §8, makes late-game unachievable)
- **Silently broken** — Feature 1's naive placeholder algorithm can easily produce these violations; without validation, they'd only be discovered during S-4/S-5/S-6 manual playtesting, wasting iteration time

Naive graph connectivity is insufficient: a graph-connected path might traverse terrain no dragon can cross (e.g., water for land-only dragons, cliffs for wingless dragons). Which terrain each actor can cross is B2's concern and is not built yet, so S-1 must not depend on it: navigability is injected.

## Goals

1. **Guarantees are pluggable** — The validator runs a list of `IWorldGuarantee` and returns one result per guarantee plus an overall pass/fail. The three hard guarantees are built-in implementations, not special cases
2. **Navigability is injected** — Reachability is answered under injected `INavigationProfile`s via Dia's `IPathCostProvider` (DiaPathfinding). "Reachable" = a path exists under at least one profile. No B2 types appear in S-1
3. **Fast validation** — Completes within generation's 5-second budget (reachability checks for typical world sizes must be efficient, not exhaustive search)
4. **Clear failure reporting** — When a guarantee fails, returns which guarantee and diagnostic info (e.g., "Hearth at (X, Y) unreachable from any Nexus", "Dynasty starts at (X1, Y1) and (X2, Y2) too close: distance D < minimum M")
5. **Deterministic** — Same world structure, guarantees and profiles always produce the same validation result (no randomness in path-finding; results ordered by injection order)

## Non-Goals

1. **Tuned generation** — This feature validates worlds; Feature 1 generates them
2. **Retry logic** — This feature returns pass/fail; Feature 3 orchestrates retries when validation fails
3. **Pathfinding API for runtime** — This is validation-time reachability only; S-4's actual dragon pathfinding is a separate system (it will reuse the same profile idea, but not this validator)
4. **Optimal paths** — Any valid path proving reachability is sufficient; shortest/best path not required
5. **Owning B2's traversability data** — The real body-plan matrix and its `INavigationProfile` implementation belong to B2. S-1 ships only the placeholder profile
6. **Selecting which guarantees run in a game session** — The caller (retry mechanism / debug host / app wiring) assembles the list; the validator has no registry or auto-discovery

## Architecture

### Layering

| Piece | Responsibility | Owner |
|-------|----------------|-------|
| Hex path grid | Dia's `HexPathGrid(width, height)` (rectangular flat-top odd-q), built from the world's terrain table per `Validate()` call; passed to Dia `FindPath<HexPathGrid>`. No S-1 graph class (amended 2026-10-14) | Dia (`hex-rectangular-grid.md`); S-1 only fills in passability |
| `INavigationProfile` | Named factory: `CreateCostProvider(world)` returns an `IPathCostProvider` bound to that world | Interface: S-1. Implementations: S-1 (placeholder), B2 (body plans), others later |
| `INavigationQuery` | `IsReachable(from, to)` — true if any profile finds a path | S-1 (built by the validator per `Validate()` call) |
| `IWorldGuarantee` | `Check(ValidationContext)` → `GuaranteeResult` | Interface: S-1. Implementations: S-1 (three built-ins), others later |
| `ReachabilityValidator` | Binds profiles to the world, builds the context, runs each guarantee, aggregates | S-1 |

**Why a profile is a factory, not a bare cost provider:** `IPathCostProvider::GetCost(from, to)` takes only cell coordinates, so a provider needs the world to look up terrain. Profiles are injected before any world exists (and each retry generates a new one), so the profile creates a world-bound provider per `Validate()` call.

### Validation Flow

```cpp
ValidationResult ReachabilityValidator::Validate(const WorldStructure& world, const ValidationParams& params);
```

1. For each `INavigationProfile` in `params.profiles`, call `CreateCostProvider(world)` once; build an `INavigationQuery` over them.
2. Build a `ValidationContext { world, navigation }`.
3. For each `IWorldGuarantee` in `params.guarantees` (in injection order), call `Check(context)`.
4. `passed` = every guarantee satisfied.

**All guarantees run independently** — don't stop at first failure; report every violation for better diagnostics.

An empty `profiles` list makes every reachability query return false. `Validate()` logs an error and fails the overall result with a diagnostic rather than silently passing or throwing.

### Navigation: Reachable Under Any Profile

```cpp
bool NavigationQuery::IsReachable(GridCoord from, GridCoord to) const {
    for (auto& costs : mBoundProviders) {
        // Dia DiaPathfinding: A* over the world's hex grid; kImpassableCost edges are blocked.
        if (FindPath(mGrid, from, to, *costs).success) return true;
    }
    return false;
}
```

**Rationale:** "Any profile" keeps the original intent: if zero actor kinds can reach a site, it is mechanically inaccessible; if at least one can, the player has a path (even if constrained). With the placeholder, there is one profile; with B2 injected, one per body plan.

**Cost semantics are free:** because the profile is an `IPathCostProvider`, weighted costs (e.g., "reachable within cost N") need no new interface. Not used by the three built-ins.

**Speed option:** per-pair `FindPath` is the baseline. If AC6 fails, `INavigationQuery` may be backed by a multi-source DiaFlowField (all Nexuses → every Hearth in one pass). That is an implementation detail behind the same interface.

### Built-in Guarantees

**`HearthNexusReachabilityGuarantee`** — For each Hearth Site, `IsReachable(hearth, nexus)` for any Nexus Node. Diagnostic per unreachable Hearth.

**`DynastySpacingGuarantee(minimumSpacing)`** — For each pair of dynasty starts, hex distance (`HexTopology::Distance`, in hex steps; amended 2026-10-14). If any pair < `minimumSpacing`, fails. Exact value is a constructor argument (tunable default per GDD open question 1), not hardcoded. Does not use navigation.

```cpp
for (size_t i = 0; i < starts.size(); ++i)
    for (size_t j = i + 1; j < starts.size(); ++j)
        if (GridDistance(starts[i].position, starts[j].position) < minimumSpacing) { /* violation */ }
```

**`WorldThroneGuarantee`** — Existence: `world.thrones.size() == 6` and all six Apotheosis types present once. Reachability: each throne must reach any cell with `cell.region == Region::AncientInteriors` via `IsReachable`. If Ancient Interiors is several disjoint regions, any cell from any of them suffices.

### Placeholder Profile

`TerrainTableNavigationProfile` loads a small authored file mapping `TerrainType → passable (and optionally cost)`. Its bound provider returns `kImpassableCost` for edges entering a blocked terrain type. This is the S-1 default and the test double for AC5. It is **not** B2's matrix: the terrain types are S-1's, and B2 later replaces or supplements it by injecting its own profiles.

## Public Interfaces

### APIs

```cpp
// ReachabilityValidator.h (CoW/Shared/WorldGeneration/)
namespace CoW::WorldGeneration {

struct GuaranteeResult {
    StringCRC id;                          // Which guarantee produced this
    bool satisfied;
    DynamicArrayC<std::string> diagnostics;  // Empty if satisfied; failure details if not
};

struct ValidationResult {
    bool passed;                                // Overall: every guarantee satisfied
    DynamicArrayC<GuaranteeResult> guarantees;  // One per injected guarantee, injection order
    const GuaranteeResult* Find(StringCRC id) const;  // nullptr if not present
};

// Answers reachability under the injected profiles (any-profile semantics).
class INavigationQuery {
public:
    virtual ~INavigationQuery() = default;
    virtual bool IsReachable(GridCoord from, GridCoord to) const = 0;
};

// A kind of actor's navigability. Injected; S-1 ships the placeholder, B2 ships body plans.
class INavigationProfile {
public:
    virtual ~INavigationProfile() = default;
    virtual StringCRC GetId() const = 0;
    // Returned provider is bound to 'world' and owned by the caller for one Validate() call.
    virtual std::unique_ptr<Dia::Pathfinding::IPathCostProvider>
        CreateCostProvider(const WorldStructure& world) const = 0;
};

struct ValidationContext {
    const WorldStructure& world;
    const INavigationQuery& navigation;
};

// A rule every accepted world must satisfy. Stateless per call; must be deterministic.
class IWorldGuarantee {
public:
    virtual ~IWorldGuarantee() = default;
    virtual StringCRC GetId() const = 0;
    virtual GuaranteeResult Check(const ValidationContext& context) const = 0;
};

struct ValidationParams {
    DynamicArrayC<const INavigationProfile*> profiles;   // non-owning; must outlive Validate()
    DynamicArrayC<const IWorldGuarantee*>    guarantees; // non-owning; must outlive Validate()
};

// Owns the three built-in guarantees and the placeholder profile, wired into a ValidationParams.
class DefaultValidationSet {
public:
    explicit DefaultValidationSet(float minimumDynastySpacing);
    const ValidationParams& GetParams() const;
    // Callers append extra profiles/guarantees (e.g., B2's body-plan profiles) to a copy of GetParams().
};

class ReachabilityValidator {
public:
    // Blocking call; completes within generation's 5-second budget for typical worlds
    static ValidationResult Validate(const WorldStructure& world, const ValidationParams& params);
};

} // namespace CoW::WorldGeneration
```

**Rationale:** Static validator (no instance state). Callers (Feature 3 retry, Feature 5 timeout, Feature 6 debug host) keep the same `Validate(world, params)` call shape. The injection happens where `ValidationParams` is assembled, which is the app wiring in the F-1 harness and S-1 stage.

### Events

**None.** This feature is a pure validator (world → validation result). Feature 3 (Retry Mechanism) calls it per generation attempt; Feature 6 (Debug Host Integration) may display results in UI.

## Dependencies

### Upstream Dependencies

| System/Feature | Why needed | Status |
|----------------|-----------|---------|
| **Feature 1 (Placeholder Generation Algorithm)** | Produces `WorldStructure` to validate | Draft (being specced) |
| **DiaPathfinding** | `FindPath<TGraph>`, `IPathCostProvider` (`kImpassableCost`) | Done |
| **DiaPathfinding: Rectangular Hex Grid** | `HexPathGrid(width, height)`, `HexTopology::Distance`, corrected hex heuristic | Draft — must be built first |
| **Phase 2 data design** | Region enum (Ancient Interiors), ApotheosisType enum (6 types), TerrainType enum (placeholder profile table) | GDD in-progress (types TBD — placeholder uses arbitrary enums until Phase 2 finalizes) |

**Not a dependency:** B2 (Dragon). B2 consumes `INavigationProfile` later. Nothing in S-1 waits on it.

### Dia Engine Dependencies

- **DiaCore** (containers: `DynamicArrayC`, file I/O for loading the placeholder terrain table)
- **DiaMaths** (`GridCoord`, distance calculations, bounds checks)
- **DiaPathfinding** (`FindPath`, `IPathCostProvider`). DiaFlowField is an optional speed-up behind `INavigationQuery`

No new Dia header is required: the injection point already exists in DiaPathfinding, and the S-1 interfaces use CoW's own world concepts (AD-002).

## Acceptance Criteria

Tests use the `DefaultValidationSet` with the placeholder profile unless stated. Look results up with `ValidationResult::Find(id)`.

### AC1 — Hearth↔Nexus Reachability Validation
Hand-authored world where all Hearth Sites are reachable from at least one Nexus Node. Validate. **Pass:** the Hearth↔Nexus result has `satisfied == true`.

World where at least one Hearth is unreachable (isolated island with Hearth, no Nexus). Validate. **Pass:** `satisfied == false` and `diagnostics` contains Hearth position.

### AC2 — Dynasty Spacing Validation
Dynasty starts placed ≥ minimum spacing apart, validated with `minimumDynastySpacing = 10` (arbitrary value). **Pass:** the dynasty-spacing result has `satisfied == true`.

Two dynasty starts < minimum spacing apart, same minimum. **Pass:** `satisfied == false` and `diagnostics` contains both positions and actual distance.

### AC3 — World-Throne Existence/Reachability Validation
Exactly 6 world-thrones, all reachable from Ancient Interiors. **Pass:** the world-throne result has `satisfied == true`.

< 6 world-thrones. **Pass:** `satisfied == false` and `diagnostics` mentions count mismatch.

6 world-thrones but one unreachable from Ancient Interiors (isolated island). **Pass:** `satisfied == false` and `diagnostics` contains throne position.

### AC4 — Overall Pass/Fail
All built-in guarantees pass. **Pass:** `ValidationResult.passed == true` and `guarantees.size() == 3`.

At least one guarantee fails. **Pass:** `passed == false`.

### AC5 — Navigability Is Respected (Not Naive Graph Connectivity)
Test world with a path from A to B that is graph-connected but crosses only terrain the placeholder profile blocks (e.g., water blocked). **Pass:** `INavigationQuery::IsReachable(A, B) == false` — proves the validator honours the injected `IPathCostProvider`, not grid adjacency.

Test world with a path crossing only passable terrain. **Pass:** `IsReachable(A, B) == true`.

### AC6 — Validation Speed
Typical sizes (Standard 80×52 and Large 104×64 grids, ~10 Hearth Sites, ~5 Nexus Nodes, 6 world-thrones, ~5 dynasty starts). Measure time from `Validate()` call to return. **Target: completes within the 5-second generation budget**, and under 1 second for typical worlds.

### AC7 — Deterministic Validation
Validate the same world twice with identical `ValidationParams`. **Pass:** identical `passed`, identical per-guarantee `satisfied` and `diagnostics`, in the same order.

### AC8 — Injected Navigation Profiles (Any-Profile Semantics)
Two test profiles on the same world: profile P blocks water, profile Q allows water, and A→B crosses only water. **Pass:** with `{P}` alone `IsReachable(A, B) == false`; with `{P, Q}` it is `true`; with `{Q}` alone it is `true`. Proves new navigation rules are added by injection with no change to S-1 code.

Empty `profiles`. **Pass:** `Validate()` returns `passed == false` with a diagnostic stating no navigation profile was supplied.

### AC9 — Injected Guarantees
A test `IWorldGuarantee` that always fails, appended to the built-in set. **Pass:** `passed == false`, `guarantees.size() == 4`, the extra result is last and `Find(itsId)->satisfied == false`. A test guarantee that always passes leaves `passed == true`. A custom guarantee that queries `context.navigation` receives the same answers as the built-ins.

## Inherited Binding Decisions

### From Platform (Cluiche)

| ID | Decision | How it applies |
|----|----------|----------------|
| PD-001 | Use StringCRC for all entity/component IDs | Guarantee and profile IDs are `StringCRC`; Region/ApotheosisType/TerrainType enums use StringCRC when loaded from the placeholder terrain table file |
| PD-004 | No STL containers in public APIs | `ValidationResult`, `GuaranteeResult` and `ValidationParams` use `DynamicArrayC`, not `std::vector` |
| PD-007 | C++20 is required | Validator can use C++20 features (std::span for grid access, concepts for pathfinding traits if needed) |

### From Application (CoW)

| ID | Decision | How it applies |
|----|----------|----------------|
| AD-002 | Classify cross-cutting needs | `ReachabilityValidator`, `IWorldGuarantee` and `INavigationProfile` are **CoW-Shared** (reused by S-1 retry mechanism, potentially S-2/S-3 if they regenerate worlds). Not Dia: they use CoW's Hearth/Nexus/world-throne concepts, and the generic part (cost injection) already exists in DiaPathfinding. Not Baseline (no other game needs CoW's guarantees) |
| AD-003 | GDD owns design intent | GDD defines three hard guarantees (§2), "reachable means traversable not graph-connected" (decision 2), and the B2 traversability dependency (open question 3). This spec resolves that dependency by injection: S-1 defines the seam, B2 fills it when built |

### From System (WorldGenerationPipeline)

| ID | Decision | How it applies |
|----|----------|----------------|
| N/A | No system-level binding decisions in parent spec | System spec has Open Design Questions but no SD- binding decisions |

## Open Design Questions

| # | Question | Impact | Resolution |
|---|----------|--------|------------|
| 1 | What is the exact `minimumDynastySpacing` value? | Affects AC2 test data and Feature 3 retry success rate; does not block implementation (use arbitrary default, tune via F-1 debug host) | GDD open question 1 defers this; constructor argument of `DynastySpacingGuarantee` with placeholder default (e.g., 10.0 grid units) |
| 2 | ~~Is B2's terrain traversability data externalized yet?~~ | Was blocking | **Resolved 2026-10-13:** navigability is injected via `INavigationProfile` / DiaPathfinding `IPathCostProvider`. S-1 ships a placeholder terrain-table profile and does not wait for B2. B2's follow-up (one profile per body plan) is logged in `dia-needs.md` as a CoW-side task for when B2 is built |
| 3 | Per-pair `FindPath` or a multi-source DiaFlowField behind `INavigationQuery`? | Affects AC6 speed; not correctness | Start with per-pair `FindPath` (simplest, correctness-first); switch the query implementation to DiaFlowField if AC6 fails |
| 4 | Which profiles count toward a given guarantee? | Today every guarantee uses "any profile"; a future guarantee might need a specific profile (e.g., "a human road network connects the hearths") | Not needed for the built-ins. If one arrives, extend `INavigationQuery` with `IsReachable(from, to, profileId)`; additive, no break |
| 5 | Should the guarantee list be assembled by a registry rather than by the caller? | Today the caller builds `ValidationParams` (app wiring) | Caller-assembled until a second consumer proves a registry is needed (Non-Goal 6) |

## Status

`Approved` — 2026-10-11. Amended 2026-10-13: navigability and guarantees made injectable (Open Q #2 resolved; blocker removed). Amended 2026-10-14: hex world on Dia's rectangular `HexPathGrid`; spacing uses hex distance; map sizes per the parent spec.

---

## Implementation Notes

When implementation begins, create a plan file `reachability-validator.plan.md` alongside this spec (TDD per `.claude/skills/tdd.md`). The plan must include an **Implementation Patterns** section documenting:

- Placeholder terrain-table file format (JSON schema, loading code, where it lives)
- `WorldStructure` → DiaPathfinding grid adapter (build per `Validate()` call vs cache; cost of rebuilding on every retry)
- Pathfinding choice (per-pair `FindPath` vs DiaFlowField multi-source)
- Diagnostic message format (how to report unreachable Hearth/throne positions for Feature 3 retry logs)
- Distance calculation (settled: `HexTopology::Distance`, hex steps)
- Timeout/early-exit for pathfinding (if any reachability check runs too long, how to handle)
- Test doubles: an always-fail guarantee, an always-pass guarantee, a water-blocking and a water-allowing profile (ship under a `Testing/` folder alongside the feature)

These patterns are validated against actual implementation before tasks are marked Done.

**B2 hand-off:** when B2 is built, it implements `INavigationProfile` once per body plan and the app wiring appends them to the `ValidationParams`. No change to S-1 code is needed; add the follow-up to `docs/specs/applications/cow/dia-needs.md` only if B2 turns out to need something Dia-side.

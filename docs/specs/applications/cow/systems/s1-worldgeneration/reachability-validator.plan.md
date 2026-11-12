**Spec:** @docs/specs/applications/cow/systems/s1-worldgeneration/reachability-validator.md
**Status:** In Progress

System plan row: task 13 in @worldgenerationpipeline.plan.md. Code in `Cluiche/CoW/Shared/WorldGeneration/`, tests in `Cluiche/Tests/GoogleTests/CoW/`.

**Scope decisions (2026-11-12):**
- The framework is built in full (evidence types, `INavigationQuery`/`INavigationProfile`/`IWorldGuarantee`, `ReachabilityValidator`, `DefaultValidationSet`, placeholder `TerrainTableNavigationProfile`).
- The three site-based built-ins (Hearth↔Nexus, DynastySpacing, WorldThrone) are generic over `SiteSet` kinds (`CoW.Site.Hearth`, `.Nexus`, `.DynastyStart`, `.WorldThrone`; `Site::variant` = Apotheosis type 0..5). S-1 registers no such kinds and adds no placer; the dragon layer registers them later. Their ACs run on hand-authored worlds.
- `WorldLayoutGuarantee` (wraps the existing `CheckLayout`) is built here, and `SiteRegistry::RegisterGuarantee` takes `IWorldGuarantee` instead of the `ILayoutGuarantee` placeholder.
- Not built: retry (task 14), timeout (task 15), any wiring of the validator into `WorldGenSession`.

## Implementation Patterns

Decisions made at plan time; check against the real code before marking tasks Done.

**Cells are indices, not coordinates.** Evidence and the query API use `uint32_t` cell indices (row-major `y * width + x`), the same as `Site::cell` and `LayoutViolation::cells`. The query converts to Dia `CellCoord` (offset col,row via `HexTopology::ToCoord`) internally. Deviation from the spec's `GridCoord` fields, recorded in the spec amendment note.

**Ids are stored as `uint32_t` CRC values.** `StringCRC` is not trivially copyable (64-byte string), so `GuaranteeResult::idCrc`, `ProofPath::profileCrc` and `Measurement::keyCrc` store `StringCRC::Value()`; `Find(StringCRC)` and `MeasurementKeys::*()` give the typed edge (Site precedent). All evidence element types are trivially copyable.

**Containers (PD-004).** `EvidenceList<T>` is a small header-only list: private `std::vector<T>`, public `Add`/`Size`/`operator[]`/`begin`/`end`/`Span()`/`operator==`. Public structs expose no std containers (Regions precedent). `ValidationParams` holds non-owning pointers in `DynamicArrayC` (16 profiles, 32 guarantees) with `AddProfile`/`AddGuarantee` returning false when full.

**Diagnostics.** `Diagnostic { char text[160]; }` (trivially copyable, truncated by `snprintf`), at most 32 stored per guarantee, then one `"... and N more"` line. Format examples: `Hearth at (x,y) unreachable from any Nexus`; `Dynasty starts at (x1,y1) and (x2,y2) too close: distance D < minimum M`; `expected 6 world-thrones, found N`; `no Ancient Interiors cells (terrain cause)`. Coordinates are `(x,y)` of the cell, so the retry log (task 14) can print them directly.

**Grid adapter.** One all-passable `HexPathGrid(width, height)` per `Validate()` call (offset layout). Passability lives entirely in the profile's `IPathCostProvider` (`kImpassableCost` on edges entering a blocked cell). Building the grid is O(cells) (about 2,800 bytes at Small); cost providers are built once per profile per call and reused for every pair.

**Pathfinding choice (deviation from the spec baseline).** Dia `FindPath<HexPathGrid>` returns its path in a `DynamicArrayC<CellCoord, 256>` and `Add` has no overflow guard in Release, so a winding path on a bigger world would corrupt memory. The query therefore runs its own deterministic Dijkstra (heap key `(cost, cellIndex)`, early exit when the first target is popped) over the same Dia `HexPathGrid` and the injected `IPathCostProvider`, so the injection point and `kImpassableCost` semantics are exactly Dia's, but the proof path is a `std::vector` with no cap. One search serves `IsReachable`, `FindProofPath` (single target) and the additive `FindProofPathToAny(from, targets, out)`, so an unreachable site costs one flood per profile, not one search per target. `FindPath` itself stays untouched (an engine-side cap guard is a separate Dia follow-up, noted in the report).

**Any-profile semantics.** Profiles are tried in injection order; the first success wins and its id is recorded. An empty profile list: every query is false and `Validate()` fails the overall result with the diagnostic `no navigation profile was supplied` (a synthetic result with id `CoW.Validation.NoProfile`) and logs an error.

**Placeholder terrain table.** `Cluiche/Assets/CoW/Global/WorldGen/terrain_navigation.json`: `{"version":1,"terrain":{"<TerrainType name>":{"passable":bool,"cost":float}}}`; unknown names are warnings, missing names keep the default, cost clamped to [0.1, 100]. Defaults (code and shipped file agree, tested): DeepWater, ShallowWater, Lake, Peak, Cliff, Volcano blocked; River 1.5; Lowland 1; Hills 1.5; Highland 2; Mountain 4; Pass 1. A first guess; B2 replaces or supplements it by injecting its own profiles. Catalogue entry `json.terrain_navigation`.

**Timeout / early exit.** None in the validator (blocking, deterministic, Small world well inside the 5 s budget; AC6 measured in the last task). The retry/timeout features wrap the whole call.

**Determinism.** All iteration is over sorted data (`SiteSet::Of` is sorted by cell); ties in distance-ordered target lists break on cell index; the Dijkstra heap key is `(cost, cellIndex)`.

**Test doubles in `Testing/`.** `ValidationTestWorlds.h` (hand-built worlds with land blocks, water gaps, islands, sites by kind; Apotheosis-type throne helper), `TestGuarantees.h` (always-fail, always-pass, navigation-querying guarantee), `TestProfiles.h` (water-blocking, water-allowing, block-nothing). `TestPlacers.h`'s `TestGuarantee` becomes an `IWorldGuarantee`.

**Build wiring.** `dia docs vcxproj-add CoW "Shared\WorldGeneration\X.h" --filter Shared` (+ .cpp); `dia docs vcxproj-add GoogleTests "..\..\CoW\Shared\WorldGeneration\X.cpp"` per new Shared .cpp and `dia docs vcxproj-add GoogleTests "CoW\TestX.cpp"`; check both `.filters` files for a bogus `..` folder. Scoped runs: `dia run googletest --filter="*Validat*:*Navigation*:*Guarantee*:*SiteRegistry*" --shards 2`; one full run only in the last task.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Framework types and test doubles.** `ValidationTypes.h/.cpp` (`EvidenceList`, `Diagnostic`, `ProofPath`, `CellPair`, `Measurement`, `MeasurementKeys`, `GuaranteeResult`, `ValidationResult::Find`, `INavigationQuery`, `INavigationProfile`, `ValidationContext`, `IWorldGuarantee`, `ValidationParams`); `Testing/TestGuarantees.h`, `Testing/TestProfiles.h` | RED-GREEN: evidence types trivially copyable; `Find` hit/miss/order; `EvidenceList` add/equality; diagnostics truncate; params capacity (AddProfile/AddGuarantee false when full) | Done | sonnet | 8 tests in TestValidationTypes.cpp (RED was include-not-found). Evidence = cell indices + uint32 CRC ids; EvidenceList wraps private vector; diagnostics 160 chars, cap 32 then droppedDiagnostics counter (not a trailing line); ProofPath holds a list so it is not trivially copyable (Diagnostic/CellPair/Measurement are). Added INavigationQuery::FindProofPathToAny |
| 2 | **Navigation.** `TerrainTableNavigationProfile` (+ `terrain_navigation.json`, catalogue entry), `NavigationQuery` (HexPathGrid per call, `IsReachable`, `FindProofPath`, `FindProofPathToAny`) | RED-GREEN: AC5 (graph-connected but blocked is unreachable; passable is reachable), AC8 any-profile ({P}, {P,Q}, {Q}), proof path is neighbour-consecutive and avoids blocked cells, ToAny equals nearest-target semantics, shipped file == code defaults, JSON warnings | Pending | sonnet | |
| 3 | **Validator and default set.** `ReachabilityValidator::Validate`, `DefaultValidationSet` | RED-GREEN: AC4 (3 guarantees pass/fail), AC7 determinism (twice, cell-for-cell), AC8 empty profiles, AC9 injected guarantees (always-fail appended: passed false, size 4, last, `Find`; always-pass; navigation-querying guarantee agrees with built-ins) | Pending | sonnet | Depends on task 4 for the real built-ins inside `DefaultValidationSet`; task 3 tests use test doubles, task 4 re-runs AC4 on the real set |
| 4 | **Built-in guarantees with evidence.** `SiteGuarantees.h/.cpp`: `HearthNexusReachabilityGuarantee`, `DynastySpacingGuarantee`, `WorldThroneGuarantee`, `SiteKinds` | RED-GREEN: AC1, AC2, AC3 (incl. no Ancient Interiors cells), AC10 evidence (proof paths valid, violating cells/pairs, measurements), AC4 on the real default set | Pending | sonnet | |
| 5 | **`WorldLayoutGuarantee` and SiteRegistry swap.** Wrap `CheckLayout` as an `IWorldGuarantee` (violation cells to `violatingCells`, counts to measurements, one diagnostic per violation, terrain-cause note for `RegionCountRange`/`AbsorbedOverCap` when `regionLimitedBy` is set); `RegisterGuarantee(IWorldGuarantee*)`, dedupe by `GetId()`; `TestGuarantee` becomes an `IWorldGuarantee` | RED-GREEN: generated worlds satisfy it, hand-built violations fail with evidence, registry tests updated, validator runs it as guarantee 4 | Pending | sonnet | |
| 6 | **Exhaustive pass and close.** Gap audit (AC1-10), edge tests (1x1, no sites, hearth on a nexus cell, all-water, duplicate throne types, huge pair counts, 128x128), mutation checks on pure logic only (spacing threshold, throne count/type check, any-profile loop, sort tie), AC6 timing at Small (Debug number, Release if cheap), one full `dia run googletest --shards 4`, SLOW region suites, Observation scan, spec amendment note, system plan row 13, statuses | `dia run googletest --shards 4` | Pending | sonnet | |

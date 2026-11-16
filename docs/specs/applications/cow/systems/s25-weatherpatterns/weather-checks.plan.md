# Plan: S-2.5 Weather Checks (S-2.5 Feature 2)

**Spec:** @docs/specs/applications/cow/systems/s25-weatherpatterns/weather-checks.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.plan.md (row 2)

**Start conditions.** Needs A4 Feature 1 built (`WeatherFields`, `WeatherTerrain`) plus A4 Feature 5's
pure `DroughtDetector` and `StormDetector` and A4 Feature 6's `ComputeWaterDistance` (this feature adds
one additive overload to it). No stage, module, bus, clock or real A4 run is needed: this feature is
**pure code** tested only with hand-built hex terrain and scripted traces. It can run in parallel with
Feature 1 (different files; the vcxproj is shared and `dia docs vcxproj-add` is idempotent) and is the
earliest part of S-2.5 that can start (the system plan says "Feature 2 first").

**Cross-plan dependencies.**
- Task 4 edits A4 Feature 6's `ComputeWaterDistance` file (additive overload; it becomes a call into
  the new function). Flag in the A4 Feature 6 plan so the two do not edit it concurrently. CoW code
  only, no Dia edit (AD-001 N/A).
- Task 1 introduces `IWeatherTrace` and `ScriptedTrace`; Feature 3 (`RecordedTrace`) and Feature 4
  (`HistoryTrace`, `CheckRegistry`) implement/consume them, so keep the header stable once Done.
- The `CheckRegistry` type is Feature 4's. This feature defines only the `ICheck` shape and
  `RegisterAll(CheckRegistry&)`; until Feature 4 exists, tests register into a local test registry that
  satisfies the same minimal interface (`Add(const ICheck&)`), and task 21 re-targets it.

No Dia files are added or edited.

## Implementation Patterns

### Files (all under `Cluiche/CoW/Shared/Weather/S25/`; namespace `CoW::S25`)
```
CheckTypes.h            CheckKind, CheckState, CheckResult, CheckRequirements, CheckInputs, ICheck
WeatherTrace.h          IWeatherTrace, IRegionMap, WorldInfo
S25Helpers.h/.cpp       SeasonOfWeek, ComputeDistance usage, aggregates, storm tracker, dry spells
S25WeatherChecks.h/.cpp the registered checks (split per test into S25Checks_T1..T9.cpp if the file grows)
BaselineStore.h/.cpp
Testing/ScriptedTrace.h trace builder for tests (ships next to the code per the test-utilities rule)
Cluiche/Tests/GoogleTests/CoW/S25/TestS25{Helpers,Checks,Baseline}.cpp
Cluiche/Assets/CoW/Stages/S25WeatherPatterns/baseline.json   (empty, envelope cow.s25_baseline v1)
```
Register every file with `dia docs vcxproj-add CoW "<path>" --filter S25` as the file is created.
Test helpers live in `S25/Testing/`, not in GoogleTests (feedback_testing_utilities).

### Conventions
- Pure functions, no statics beyond `constexpr` thresholds, no singletons, no module access.
- Public APIs use Dia containers (`DynamicArrayC`, `String256`, `String32`) and `StringCRC` ids; jsoncpp
  only in `BaselineStore` (PD-004).
- Numbers are `double` and accumulated in `double` in **ascending cell order** so results are
  bit-reproducible (AC 3, AC 15). Never iterate a hash table when ordering matters.
- Every check id is a `constexpr StringCRC` in one header table so the registry-vs-spec test (task 21)
  compares against a single list.

### Result model and requirements (tasks 1-2)
- `CheckResult` exactly as the spec (id, test 1..9, kind, state, `measured` double (NaN when not a
  magnitude), `threshold` double, `detail` String256, `reopenSection` String32).
- `CheckRequirements { DynamicArrayC<WeatherLayer> layers; uint32_t minWeeks; uint32_t firstWeek; bool needsComparison; }` (amendment 1).
- `CheckInputs { const IWeatherTrace& primary; const IWeatherTrace* comparison; const WorldInfo& world; }`. `comparison` is the
  second run for `t2.shadow_repeatable`, `t8.*` (the other edge setting) and `t9.determinism`; null otherwise.
- `WorldInfo { StringCRC world; WorldSize size; const WeatherTerrain* terrain; uint32_t stormReleaseWeek; uint32_t stormSpawnCell; uint32_t holdWeeks; }`
  (amendment 2). The storm fields are used by Test 3 only and are 0 elsewhere.
- `measured` and `threshold` are `double` (amendment 7; the system spec's `float` is superseded).
- `ICheck` interface: `Id()`, `Kind()`, `Test()`, `Requirements()`, `ReopenSection()`,
  `CheckResult Evaluate(const CheckInputs&) const`.
- A shared `EvaluateWithGuards(const ICheck&, const CheckInputs&)` wrapper applies requirements first
  (`Pending` with reason "needs weeks 1..52, have 9" in `detail`; **`needsComparison` true with a null `comparison` is `Pending`**,
  and a live context reports it as `HeadlessOnly`, which is Feature 4's mapping) and maps an empty band to
  `InvalidWorld` (a check signals this through a small internal `BandEmpty` result, not an exception).
  Hard `Fail` gets `reopenSection` filled by the wrapper from the check's declared section.
- Default checks return a raw measured value + GDD default; `BaselineStore::Compare` (task 18) turns it
  into `Pass`/`MagnitudeMiss`. Evaluate itself judges against the GDD default.

### ScriptedTrace (task 1, built first)
A builder in `Testing/ScriptedTrace.h`:
`ScriptedTrace(const WeatherTerrain&, regions, params).Weeks(n).Set(layer, cellSet, week, value)`,
plus range and ramp setters (`SetRange(layer, cells, w0, w1, fn)`), named cell sets (`Ocean()`,
`Coast()`, `Inland()`, `Mountains()`, `Lowlands()`), a `Without(layer)` and `Truncate(weeks)` for the
Pending cases, and a `Perturb(layer, week, cell, ulp)` for the one-ULP determinism case. Unset values
default to a documented neutral value; `Has` is false only for layers/weeks removed on purpose. The
builder is itself tested (task 1) before any check uses it, because every later test depends on it.

### Table-driven check tests (task 2)
`TestS25Checks.cpp` is driven by a table `{ id, passTrace builder, failTrace builder, expectedFailState }`.
A **meta test** iterates the registry (or the local test registry) and fails for any registered id that
has no table row, or a row missing either trace. So adding a check without both a passing and a failing
scripted trace fails the build. Write the meta test (RED: zero rows) before the first check, and give
each check task a table row. AC 5, 6, 7 and 15 are asserted once over the whole table, not per check:
`Fail` carries `reopenSection`, `measured`/`detail` are populated, truncated/missing-layer variants give
`Pending`, an emptied band gives `InvalidWorld`, and double evaluation is bit-identical.

### Helpers (tasks 3-6)
- `SeasonOfWeek(uint32_t)` returns `Season` and rejects 0 and 53 (returns an invalid sentinel or an
  error out-param; pick the repo's usual convention). The GameSpeedController agreement test uses
  days 0-363, `week = day / 7 + 1`.
- `ComputeDistance(const WeatherTerrain&, SourcePredicate)` BFS over `HexTopology` with predicates
  `OceanOnly`, `AnyWater`, `MapEdge`; unreachable = a named sentinel excluded from bands. Odd/even
  column hex tests; lakes/rivers are not `OceanOnly` sources. `ComputeWaterDistance` forwards to it.
- Aggregates: `MeanGlobal`, `MeanOverCells`, `MeanPerRegion`, `SnowCoveragePercent` (strictly
  `snowPack > 0.1`, A4's `kSnowCoverThreshold`). Tested against a reference naive double loop with
  bitwise comparison (`memcmp` on the double).
- Storm tracker: A4's `StormDetector` clusters, pressure-weighted centroid snapped to the nearest cell
  (ties lowest index), link by nearest centroid within 6 steps, 1-week gap keeps the track, 3-week gap
  starts a new one, 2-week re-find keeps it.
- Dry spells: thin wrapper over A4's `DroughtDetector` with `dry_threshold` 0.02 per S-1 region
  (one region if absent) and a consecutive-dry-week counter; the one shared "dry" definition.

### Checks (tasks 7-16)
One task per GDD test, in test order. Each task adds: the check implementations, their registered ids
(matching the spec table exactly, `s25.tN.<name>` ids as in the spec table with the `s25.` prefix),
pass and fail scripted traces, and the table rows. Thresholds (band distances, `minShadowCrests`, 0.5 ratio,
0.01, 0.05, 6 weeks, 10 weeks, 60 degrees, 4 of 6 neighbours, 0.5%, r 0.9) are named `constexpr`s in
one `S25CheckThresholds` struct so tests and the baseline can name them, never inline literals.
- Test 2: crest-cell finder shared by all three checks (crest definition per amendment 3: a land cell with elevation >= 0.6 whose elevation
  is >= both its prevailing-upwind and downwind neighbour's and strictly greater than at least one of them, a local maximum along the wind line);
  `shadow_repeatable` compares sorted crest lists of the primary and `comparison` traces (`needsComparison`).
- Test 3: measure only weeks after release; the release week and spawn cell reach the check through
  `WorldInfo.stormReleaseWeek` / `stormSpawnCell` / `holdWeeks` (amendment 2); the ±60 degree convergence test uses hex-neighbour direction angles.
- Test 4: 3-week moving mean, direction changes ignoring |delta| < 1e-4, season sums by `SeasonOfWeek`.
- Test 5: weeks 40, 52, 65 exactly; `gw_peak_week` measured value is the week number, state judged
  against Spring (53-65).
- Test 7: Pearson r over the 4 seasonal-mean precipitation values per year versus the 4-year mean
  pattern; `years_differ` result text "goes to the user..." with empty `reopenSection`.
- Test 8: upwind-edge set from the **on** trace, reused for the **off** trace; two traces via `CheckInputs.comparison` (`needsComparison`).
- Test 9: byte comparison of committed fields at weeks 13, 26, 39, 52 via `memcmp`, reporting the
  first differing week, field and cell in `detail`.

### Baseline store (tasks 17-20)
`BaselineStore` exactly as in the spec: `Load` (all-or-nothing, unknown key, non-finite value, wrong
`format`/`version` rejected, `BaselineError` names the key), `Lookup`, `Compare`, `Record(force)`,
`Serialize`. Comparison rule: relative +-10%, absolute +-0.01 when the baseline is under 0.1.
Serialization sorts keys and prints doubles at 17 significant digits for byte-stability. Tests compare
serialized strings, not parsed values, for the "byte-identical" claims.

### Registry (task 21)
`S25WeatherChecks::RegisterAll(CheckRegistry&)` registers each id exactly once with kind, requirements
and reopen section. The check objects are **function-local statics owned by `S25WeatherChecks`**; the registry stores non-owning
pointers valid for the process lifetime (amendment 6); a test compares the registered id list with the spec table's list (kept as one
`constexpr` array in `CheckTypes.h` plus a doc-comment pointing at the spec section).

### Verification cadence
- Per task: `dia run googletest --filter="S25Helpers*:S25Checks*:S25Baseline*" --shards 2`; narrow to
  one suite while iterating.
- One full run in task 24. No mutation checks. Pure code, so no sanitizer pass unless a task finds UB.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Create `S25/` folder and empty headers/sources for the files above, test files, `Testing/` folder; `dia docs vcxproj-add` each; confirm A4 Feature 1 `WeatherFields`/`WeatherTerrain` and A4 Feature 5/6 `DroughtDetector`, `StormDetector`, `ComputeWaterDistance` headers exist and note their paths | Project compiles with the new empty files | Not Started | haiku | Mechanical, TDD-exempt. If any A4 header is missing, stop and mark blocked in the system plan |
| 1 | RED -> GREEN: `CheckTypes.h` (incl. `CheckInputs`, `CheckRequirements.needsComparison`, double `measured`/`threshold`), `WeatherTrace.h` (incl. `WorldInfo` fields per amendment 2), `Testing/ScriptedTrace.h` and tests for the builder itself (set/ramp/range, named cell sets, `Has` false only for removed layers/weeks, `Truncate`, `Perturb` by one ULP) | Builder returns exactly the scripted values and nothing else; foundation for AC 5-7, 14 | Not Started | sonnet | **ScriptedTrace first**: every later task depends on it. Keep the header stable once Done (Features 3-4 build on `IWeatherTrace`) |
| 2 | RED -> GREEN: the table-driven harness `TestS25Checks.cpp` and its **meta test** (every registered id has a pass trace and a fail trace); `EvaluateWithGuards(const ICheck&, const CheckInputs&)` for requirements/Pending (including `needsComparison` with a null `comparison` giving `Pending`) and empty-band InvalidWorld with a stub check | AC 5 (harness), AC 6, AC 7, AC 15 (over the whole table) | Not Started | sonnet | Table rows carry an optional comparison-trace builder for two-trace checks. RED: the meta test fails with a deliberately row-less stub check. This is the mechanism that makes "add a check without a pass+fail trace" fail |
| 3 | RED -> GREEN: `SeasonOfWeek` and the GameSpeedController agreement test (days 0-363) | AC 1 | Not Started | sonnet | The agreement test includes the real calendar header only through its public season API |
| 4 | RED -> GREEN: `ComputeDistance` with `OceanOnly`/`AnyWater`/`MapEdge`; `ComputeWaterDistance` becomes a call into it and A4 Feature 6's tests still pass | AC 2; run `WeatherDistance*` or A4's existing distance filter too | Not Started | sonnet | **Cross-plan: edits A4 Feature 6 file** (additive, no behaviour change). Odd and even columns, sentinel for unreachable, lakes/rivers not ocean sources |
| 5 | RED -> GREEN: aggregates (global, per-region, snow coverage) | AC 3 | Not Started | sonnet | Bitwise-equal to a reference loop; strictly `> 0.1` boundary cases at 0.1 and next-float above |
| 6 | RED -> GREEN: storm tracker (centroid, link, 1-week gap kept, 3-week gap new track, tie to lowest index) and dry-spell helper over `DroughtDetector` | AC 4 | Not Started | sonnet | Uses A4's `StormDetector` (pressure < 0.3) and `DroughtDetector` (0.02) unchanged; consecutive-dry-week counter is the new code |
| 7 | RED -> GREEN: Test 1 checks `t1.moisture_monotonic`, `t1.precip_weeks`, three default band means | AC 5 (T1), AC 6 (empty band), AC 8 (each failing order reported) | Not Started | sonnet | Bands: ocean = distance 0 from ocean, coast = land at 1-2, inland >= 8 |
| 8 | RED -> GREEN: Test 2 checks `t2.shadow_present`, `windward_gt_leeward`, `shadow_repeatable` and the crest-cell finder | AC 5 (T2) | Not Started | sonnet | Crest definition per amendment 3 (local maximum along the wind line; the earlier "lower than the prevailing-upwind neighbour's" wording is superseded); test a plateau (equal to both neighbours, not a crest) and a one-sided step (crest). `shadow_repeatable` passes the comparison trace through `CheckInputs` |
| 9 | RED -> GREEN: Test 3 checks (centre moves, wind converges, not stationary, ends or intensifies, travel 5 cells) measuring only post-release weeks | AC 5 (T3), AC 9 | Not Started | sonnet | Release week and spawn cell come from `WorldInfo` (`stormReleaseWeek`, `stormSpawnCell`, `holdWeeks`; amendment 2). Scripted storm whose held centre is stationary pre-release and moves after must pass `centre_moves` |
| 10 | RED -> GREEN: Test 4 checks (extremes, smoothness, winter wetter, melt crossing, ratio) | AC 5 (T4), AC 10 | Not Started | sonnet | `t4.melt_crossing` uses weeks 1..13 (first Spring after spin-up, minWeeks 13); the other Test 4 checks use minWeeks 52 (amendment 4); test Pending at 12 weeks for melt crossing. 1e-4 hysteresis tested at 0.9e-4 and 1.1e-4; saw-tooth curve fails |
| 11 | RED -> GREEN: Test 5 checks (snow builds, snow falls, flood follows melt, gw peak week) | AC 5 (T5), AC 11 | Not Started | sonnet | Weeks 40, 52 and 65 exactly; `t5.*` use `minWeeks` 65 and `firstWeek` 40 (amendment 4). Peak outside 53-65 fails `flood_follows_melt` and `gw_peak_week` records the actual week |
| 12 | RED -> GREEN: Test 6 checks (droughts found, inland drier, dry spell 4 weeks) | AC 5 (T6) | Not Started | sonnet | Dry spell start window weeks 14-39; consistent with the shared dry definition |
| 13 | RED -> GREEN: Test 7 checks (`years_differ` 0.5%, `seasonal_shape` r >= 0.9) with "goes to the user" detail text and empty `reopenSection` | AC 5 (T7), AC 12 | Not Started | sonnet | Bit-identical four years fail; 0.6% passes; inverted seasonal pattern fails. S-2.5 never invents variability |
| 14 | RED -> GREEN: Test 8 checks (toggle effective, edge on > off, interior tolerance) with the upwind-edge set computed from the on trace | AC 5 (T8), AC 13 | Not Started | sonnet | Two traces via `CheckInputs.comparison` (on is primary, off is comparison; `needsComparison` true; amendment 1) |
| 15 | RED -> GREEN: Test 9 `t9.determinism` (weeks 13/26/39/52, `memcmp`, first differing week/field/cell) | AC 5 (T9), AC 14 | Not Started | sonnet | Run A is primary and run B the `comparison` (`needsComparison`). One-ULP perturbation via `ScriptedTrace::Perturb`; identical traces pass |
| 16 | RED -> GREEN: whole-registry determinism sweep: every check evaluated twice is bit-identical in `measured`; empty-band and Pending sweeps over all ids | AC 15, AC 6, AC 7 (all ids) | Not Started | sonnet | Registry-driven, so it also catches checks added later. Mostly a test-only task; fix any non-determinism found (iteration order, uninitialised temporaries) |
| 17 | RED -> GREEN: `BaselineStore::Load` and `BaselineError` (atomic, unknown key / non-finite / wrong `format` or `version` rejected) | AC 16 | Not Started | sonnet | Inline JSON; no asset yet. Failed Load must leave the store unchanged |
| 18 | RED -> GREEN: `Lookup` and `Compare` (GDD default with no entry; +-10% relative, +-0.01 under 0.1) | AC 17 | Not Started | sonnet | First-run rule (amendment 5): a Default check with no baseline entry is judged against the GDD default and is `Pass` if met; `Pass` does not require a recorded baseline, and `Compare` exposes the "unrecorded" fact so Feature 3 can list `baseline.unrecorded[]`. Boundary cases exactly at 10% and 0.01; NaN measured is `Pending`/`MagnitudeMiss` per spec text, document choice |
| 19 | RED -> GREEN: `Record` (no-force adds only absent, byte-identical existing; force replaces the world's entries; record-then-compare passes) | AC 18 | Not Started | sonnet | Compare serialized bytes for the "byte-identical" claims |
| 20 | RED -> GREEN: `Serialize` (stable key order, 17-digit doubles, round-trip with `Load`) and the empty shipped `baseline.json` (`cow.s25_baseline` v1); `dia docs vcxproj-add` for any asset entry | AC 19 | Not Started | sonnet | Shipped file parsed by a test so a malformed asset fails CI |
| 21 | RED -> GREEN: `S25WeatherChecks::RegisterAll` and the id-list-vs-spec test; wire registry harness (task 2) to the real list; each id registered once with kind, requirements and reopen section | AC 20 | Not Started | sonnet | Check objects are function-local statics owned by `S25WeatherChecks`; registry stores non-owning pointers valid for the process lifetime (amendment 6); test that a registered pointer is stable across two `RegisterAll` calls into different registries. If Feature 4's `CheckRegistry` does not exist yet, use the local minimal registry and record the follow-up in the Feature 4 plan; do not block |
| 22 | Exhaustive test pass: gap audit of every AC (1-20) and every check id against the table; edge pass (0-week trace, 1-week trace, single-cell bands, all-NaN layer, NaN in one cell, regions absent, maximum distance sentinel cases, week index at the boundaries 13/14/26/27/39/40/52/53/65) | Gap list in notes; every id has both traces | Not Started | sonnet | No mutation checks. Scoped filter `S25Helpers*:S25Checks*:S25Baseline*` |
| 23 | Observation Opportunity Scan over all touched files | Suggestions in notes (expected: nothing, code is pure) | Not Started | sonnet | Suggestions do not block Done |
| 24 | One full verification run: `dia run googletest --shards 2` (full suite), `dia check deps`, `dia check cppcheck`, `dia check sln-sync` | All green, failures named | Not Started | sonnet | Single full run; report pass/fail and failing test names only. Also confirm A4's distance tests still pass after task 4 |
| 25 | Close-out: `dia docs plan docs/specs/applications/cow/systems/s25-weatherpatterns/weather-checks.plan.md <#> --status Done` per row; `dia docs spec-done docs/specs/applications/cow/systems/s25-weatherpatterns/weather-checks.md`; update system plan row 2 | Spec status Done | Not Started | haiku | **Do not run until all prior rows are Done.** The system spec's edits (`CheckResult`, Checks section) are applied per the spec's "Spec edits on approval" line by whoever owns that doc |

## Open items

Resolved items are kept for traceability; only the last two are genuinely open.
- **Two-trace checks.** **Resolved by amendment 1 (2026-11-16)**: `Evaluate(const CheckInputs&)` with an optional `comparison` trace and `needsComparison`.
- **`WorldInfo` is undefined.** **Resolved by amendment 2 (2026-11-16)**: fields `world`, `size`, `terrain`, `stormReleaseWeek`, `stormSpawnCell`, `holdWeeks`.
- **Test 2 crest definition.** **Resolved by amendment 3 (2026-11-16)**: local maximum along the wind line.
- **`minWeeks` for Test 4 melt and Test 5.** **Resolved by amendment 4 (2026-11-16)**: melt 1..13; `t5.*` 65 with firstWeek 40; other Test 4 checks 52.
- **First-run `Pass`.** **Resolved by amendment 5 (2026-11-16)**.
- **Registry ownership.** **Resolved by amendment 6 (2026-11-16)**.
- The system spec's float `measured`/`threshold`: **Resolved by amendment 7 (2026-11-16)**: double.
- **Pending versus `NaN measured`**: no rule for a check that gets all-NaN data (still open; task 18 documents the choice).
- The Pending/InvalidWorld cases' `reopenSection` is assumed empty (spec only states it for the Test 7 Fail case; still open).

# Plan: A4 Weather Debug (A4 Weather Feature 5)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-debug.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 5)

Rebuilt 2026-11-18 against the world-sim review amendment. It also folds in the T-1 consumer amendment (storm ids,
`weather.storm_track`, `weather.drought_end`, `cow.weather.storm_tracks_dropped`, ACs 27 to 31) and the wind
component layers (ACs 32 to 34), which the earlier plan had not absorbed. What changed from the old plan:
- **Commands.** `cow.weather.params` is built by `ParamCommandHelper` (`set | get | list | reload | save_defaults`)
  plus A4's own `cycle` action; `cow.weather.reload` is retired; every name is registered through `SimCommandSet`
  (all-or-nothing, with its own rollback). The hand-written all-or-nothing registrar and the hand-written params
  and reload handlers are deleted.
- **Schema keys** are `cow.weather.layers/<field>` built by `WorldSimNames.h` (no `weather/<key>`).
- **Cells** are selected through `CellSelectorSet`; A4 contributes its committed fields to `WorldCellFields` under
  slot `weather`, and the detail rows read from there.
- **Health** reads Failing, not "Unhealthy"; `cow.weather.step_ms` and `cow.weather.discarded_steps` are the stage
  base's.

**Start conditions.** Needs these plans Done:
- `weather-module.plan.md` (layers hosting, the queue API, the stage) and A4 Feature 6 (`ApplyPreset`, used by
  `load_preset`).
- `worldsim-kit.plan.md` (World Step): `SimCommandSet`, `ParamCommandHelper`, `WorldCellFields`, `CellSelectorSet`,
  `WorldSimNames.h`.
- `weather-save.plan.md`: the state codec, `EncodeState`/`DecodeState` and the `StateReplace` apply branch, for
  `state export`/`import`.
- The `ConditionExpr::Validate(const IConditionContext&)` overload and `WorldCellFields` consumers need
  `worldsim-kit.plan.md` only. `parameter-files.plan.md` (for `ParameterFile` reads behind `params get|list`) and
  `biome-registry.plan.md` (for the `biome` detail row) are transitive.
- DiaCellLayers, DiaCellHistory, DiaValueSchema, `SimQueryHostModule`, `DiaDebugDomainRegistry`/`DiaDebugPanel` and
  DiaObservation are built.

`grid-search-helpers.plan.md` and `blackboard-scopes.plan.md` are not needed. **Startable earlier on fakes:** the
pure parts (command model, `StormDetector`, `DroughtDetector`, `WindOverlayPlan`, `ExplainCell`; Tasks 5 to 9) need
only A4 Features 1 and 2 and hand-built terrain, so they can begin after the ca-pipeline passes are built. The
three spikes (Tasks 0 to 2) can start now. Wiring tasks use a fake `CellLayerRegistry`, fake `SimQueryHostModule`,
the kit's fake command set and a fake `CellHistoryRecorder`.

No Dia files are added or edited. Any task that would need a Dia change is `AD-001 ask first`.

## Implementation Patterns

### Files and layout
- `Cluiche/CoW/Modules/WeatherDebug/`: `WeatherLayerRegistration`, `WeatherWindOverlay`, `WeatherCommands`,
  `WeatherCellFields`, `WeatherCellDetails`, `WeatherHistoryEvents`, `WeatherDebugDomain` (`DIA_DEBUG` only).
- Pure, `Cluiche/CoW/Shared/Weather/Debug/`: `WindOverlayPlan`, `WeatherCommand` (typed model, parser, validation,
  `IWeatherCommandSink`), `StormDetector`, `DroughtDetector`, `ExplainCell`.
- Layer and event sidecar: `Assets/CoW/Global/Weather/weather_schema.json`, entries keyed
  `cow.weather.layers/<field>` (and `cow.weather.layers/wind_east`, `.../wind_north`). Catalogued, resolved through
  `PathStore`.
- Tests: `Tests/GoogleTests/CoW/Weather/{TestWindOverlayPlan,TestWeatherCommands,TestWeatherDetectors,TestWeatherCellDetails,TestWeatherLayers}.cpp`. Fakes in `Cluiche/CoW/Shared/Weather/Testing/`.
- Layers, commands and metrics compile in all configurations; the `IDebugDomain`, card and overlay draw are
  `#if DIA_DEBUG`.
- No string literals for names: layer ids (`weather.<snake_case>`), command names, schema keys and metric names
  come from `WorldSimNames.h`, and the kit's pattern test must pass for them.

### Spikes (Tasks 0 to 2, gating)
- **Panel stream.** Find the stream the debug panel really reads for a domain's SimPU values (not the
  `debug.domain_state` mutex precedent). Output: stream, message type and `WeatherDebugStats` shape; if it is a
  bus message, declare it in `cow_worldsim.diagamemessages` (never hand-registered). Gates Task 22.
- **History read.** Fix the DiaCellHistory call that returns per-week floats for a tracked layer, the tracked query
  and the retained-window query; add a `PrecipitationHistoryAdapter : IPrecipitationHistory` stub. Gates Task 7
  and 20.
- **Hotkeys and descriptors.** Read the S-2.5 GDD hotkey table (Snow = K), `CellLayerDesc`, `CellOverlayDesc`,
  `HistoryEvent`, `SimHelpInfo`, and the kit's `SimCommandSet`/`ParamCommandHelper` registration shapes. Gates
  Tasks 11 and 15.

### Typed command model and sinks (Task 5)
- `WeatherCommand` is a tagged union of `Force`, `WindSet`, `WindSetRegion`, `WindClear`, `ParamsCycle`,
  `StateExport`, `StateImport`, `LoadPreset`, `Influence`, `FindStorms`, `FindDroughts`, `FindRainShadow`,
  `InfoProfile`, `InfoMemory`, `InfoCycles`. The `params` actions `set|get|list|reload|save_defaults` are the kit
  helper's and are **not** parsed here; only `cycle` is. `ParseWeatherCommand(const Json::Value&, WeatherCommand&,
  CommandError&)` validates every range and names the field. Parse once; handlers and both sinks use the typed value.
- Cell arguments that select a set of cells take a **selector name** resolved by `CellSelectorSet` (amendment 4).
  A single `cell` index stays an index. The S-1 `region` id form is kept as a selector alias (open item 3).
- `IWeatherCommandSink`: `QueuedSink` calls the stage's `Queue*` methods; `DirectSink` is bound to a
  `WeatherSimulation&` and applies between steps (headless). Equivalence is one test comparing committed state
  byte for byte.
- Degrees convert to radians through the one helper shared with Feature 2's wind code. A full queue is an error
  reply, never a drop.
- `force storm` calls Feature 7's `AddInfluence` (source `"debug.force"`, centre `cell`, strength from `intensity`,
  radius default 3, lifetime from `weeks`) and queues no hold; until Feature 7 exists it stays on the holds and the
  task notes it. `high_pressure` is a pressure hold high plus a moisture hold low; `rain` is a one-week `Rain`.
  Replies say the change latches at the next `Capture`; the kit's `set` reply shape (`effectiveDay`, `visibleDay`)
  is used for `params` actions.

### Detectors and plan (Tasks 7 to 9)
- `StormDetector`: connected components over hex neighbours of `pressure < storm_pressure`, size, anchor (lowest
  pressure, ties by lowest index). With the T-1 amendment it also assigns ids: a storm keeps its `id` while this
  week's region overlaps last week's (largest overlap wins); a split gives the smaller part a new id; a merge keeps
  the older id; ids are per world from 1, never reused, reset on a world change and on a state import; last week's
  per-storm cell sets are kept on SimPU, not saved.
- `DroughtDetector(const IPrecipitationHistory&, weeks, dry_threshold)` returns cell sets grouped by region, or an
  error naming the retained window. At `Commit` the history adapter appends the just-committed `precipitation` as
  the newest week; `find droughts` reads only sampled weeks.
- `WindOverlayPlan` exactly as the spec: aligned 4 x 4 blocks, speed-weighted circular spread plus speed range,
  storm flag, stride 1/2/4, truncation by lowest score stable by block index, base arrows always kept,
  `windSpeed < 0.02` omitted, `kMaxArrows = 2700`; config validated by `ValidateWindOverlayConfig`; accumulation in
  index order so the plan is deterministic.
- `ExplainCell` calls Feature 2's exported `PrevailingDirection`, `PressureGradient`, `SnapWindDirection` (export is
  a linkage change only) and reads weather values from `WorldCellFields` slot `weather`, not a private list.

### `WorldCellFields` contribution (Task 10)
A4 registers its committed fields under slot `weather` (the ten arrays by their snake_case names, plus
`weather.climate_offset`), as typed readers over a cell index reading committed state only. Handles are
`(slot, field)` CRCs resolved at use; the registry is rebuilt on `WorldGenerationComplete`. Reading during `Run`
asserts in Debug. The same fields serve Feature 6's preset selectors.

### Verification cadence
Per task a narrow `dia run googletest --filter="<suite under work>" --shards 2` (always `--shards 2`); the
five-suite filter runs only in the scoped-verification task. The full suite and the single Release compile check
run once in worldstep.plan.md Weather-stack close-out. No sanitizer unless asked. No `SLOW_` suites. No mutation
checks. No layer goldens (Feature 6 owns them). Timing is Debug ballpark only. World-sim checks use cell data and
stats, not screenshots; the panel layout taste check is S-2.5's.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | **[old 0]** Prerequisite check, read only: `WeatherModule` and its queue (`QueueRegionForcing`, `QueueStateReplace`, `QueueClearWindOverrides`), `ApplyPreset`, the save codec and `StateReplace` apply, `SimCommandSet`, `ParamCommandHelper` (and its system-action hook), `WorldCellFields`, `CellSelectorSet`, `WorldSimNames.h` builders, `CellLayerRegistry`, `CellHistoryRecorder`, `SimQueryHostModule`, `DebugGroupAccents`. Record gaps and which tasks use fakes **[old 3]** **Spike: hotkeys, descriptor shapes and kit registration shapes.** Read the S-2.5 hotkey table, `CellLayerDesc`, `CellOverlayDesc`, `HistoryEvent`, `SimHelpInfo`, and the `SimCommandSet`/`ParamCommandHelper` signatures | TDD-exempt (read-only prerequisite check). [old 0] Findings recorded in notes; [old 3] Findings in notes (no code) | Not Started | haiku | [old 0] Read-only, TDD-exempt. A missing item blocks the tasks that need it, not the plan. Do not reimplement foundation pieces [old 3] Read-only. Gates Tasks 11 and 15 **Merged from old rows 0, 3 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** **Shared-base audit:** confirm row 4a (shared-base audit of `WorldStepStageBase`, `WorldSimSaveCodec`, `HexWorldSaveCodec`, `WorldStepModule`) in `a4-weather.plan.md` is Done, or record that it is not and why, before the first row below that touches shared code (5 `Queue*` sinks, 10 `WorldCellFields`, 17 and 18 state import and `StateReplace`, 20 `PublishCommit`). |
| 1 | **Spike: debug panel stream.** Find the stream and message type the panel reads for domain values; write the finding and the `WeatherDebugStats` shape | A test proving a stats message published from SimPU reaches a fake panel reader without a cross-thread mutex read | Not Started | sonnet | Resolves system open item F5 ODQ 3. Gates Task 22. Do not use the `debug.domain_state` mutex precedent. If a new bus type is needed, declare it in `cow_worldsim.diagamemessages` |
| 2 | **Spike: DiaCellHistory read call.** Fix the exact call for the last N weeks of a tracked float layer, the tracked query and the retained window; add the `PrecipitationHistoryAdapter` stub | A test reading a tracked layer from a real or fake ring through the adapter returns the planted weeks, newest last; untracked reports not tracked | Not Started | sonnet | Gates Tasks 7 and 20. Ask before escalating past sonnet |
| 4 | Scaffold all placement files, the sidecar file and five test files; `dia docs vcxproj-add CoW <file>` and `dia docs vcxproj-add GoogleTests <file>` for each | TDD-exempt (mechanical scaffold). Project compiles | Not Started | haiku | Mechanical, TDD-exempt. Idempotent |
| 5 | **[old 5]** `WeatherCommand` typed model, parser and validation. RED then GREEN **[old 6]** `IWeatherCommandSink` with `QueuedSink` and `DirectSink` over the stage's `Queue*` entries. RED then GREEN | [old 5] AC 14 (each bad input returns an error naming the field and produces no command: unknown `action`, missing field, `cell` out of range, `degrees` outside 0 to 360, `speed`/`intensity`/`amount` outside 0 to 1, `weeks` outside 1 to 52, an unknown selector or region, an unknown preset listing names, an unknown cycle id); parameter-key and parameter-range errors for `params set` are asserted through the helper's reply, not re-parsed here; [old 6] AC 13 (a valid request queues the expected forcing, state replacement or preset, effective at the next `Capture` and not before, and the reply says so), AC 15 (`set_region` queues one region forcing with exactly the selector's cells, `clear` removes every wind override at the next `Capture`), AC 17 (direct and queued sinks give identical state after the same commands and steps), AC 19 (a full queue is an error reply, not a drop) | Not Started | sonnet | [old 5] Parse once. Selector, region, preset and cycle lookups are injected resolvers so the parser stays pure [old 6] No queue edit: the entries exist from Feature 3. **Merged from old rows 5, 6 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 7 | `StormDetector` (with ids and continuity) and `DroughtDetector`, pure. RED then GREEN | AC 18 first half (`storms` returns connected regions on a planted storm), AC 20 (flags exactly the cells dry for all of the last `weeks` weeks; one wet week excludes the cell; grouped by region; `weeks` longer than the window errors naming it), AC 27 (a persisting storm keeps one id across N weeks and centroids follow the region), AC 28 (a split keeps the id on the larger part and gives the smaller a new id; a merge keeps the older id; ids never reused within a world and reset on a world change and on a state import) | Not Started | sonnet | Vector-free fake history in tests. Anchor ties by lowest index. Cell sets of last week live on SimPU, not saved |
| 8 | `WindOverlayPlan` and `WindOverlayConfig` validation. RED then GREEN | AC 4 (calm uniform wind: one arrow per 4 x 4 block, 416 at 104 x 64), AC 5 (storm block stride 1; spread above `refine_high` stride 1; above `refine_low` stride 2), AC 6 (never above `max_arrows`; lowest score dropped first, stable by block index; every base arrow survives; a world-wide storm stays within the cap), AC 7 (deterministic; recomputed only on a version change), AC 8 (speed under 0.02 omitted; 3 primitives per arrow; at the cap at most 6,000), AC 9 (stride 0, `refine_low > refine_high`, `max_arrows` over 2,700, a threshold outside [0, 1] rejected naming the key; defaults used) | Not Started | sonnet | Biggest pure task; ask before escalating past sonnet. Defaults are tuned later in S-2.5 (config only) |
| 9 | `ExplainCell` plus the Feature 2 helper exports (`PrevailingDirection`, `PressureGradient`, `SnapWindDirection`, no behaviour change). RED then GREEN | AC 10 (matches the pipeline helpers for prevailing direction, snapped direction and the orographic-lift decision on a hand-built terrain with known answers); the committed weather values come from `WorldCellFields`, asserted by planting a value there | Not Started | sonnet | Export is a linkage change; run the existing `WeatherSimulation*` and pipeline tests in the same filter. Commit the group of tasks 7 to 9 |
| 10 | RED then GREEN: A4 contributes `weather.*` committed fields to `WorldCellFields` (slot `weather`, ten arrays plus `climate_offset`), rebuilt on `WorldGenerationComplete` | New (amendment 5 and 12): each field reads the committed value by cell index; as an `IConditionContext` a `ConditionExpr` such as `weather.temperature < 0.3` evaluates per cell and matches a hand count; the slot is empty before the first world and after `DoStop`; reading during `Run` asserts in Debug; `Validate` accepts the field names and rejects `weather.nonexistent` | Not Started | sonnet | Handles are `(slot, field)` CRCs resolved at use. The names come from `WorldSimNames.h` |
| 11 | **[old 11]** Sidecar `weather_schema.json` and layer registration for the eight scalar base layers (seven fields plus `weather.climate_offset`): read callback, `version()`, count mismatch, hotkey clash, `DoStop` unregister, re-entry. RED then GREEN **[old 12]** RED then GREEN: the two wind component layers `weather.wind_east` and `weather.wind_north` (`windSpeed * cos/sin(windDirection)`, range -1 to 1, diverging ramp), computed at `Commit` | [old 11] AC 1, 1a (all eight register `Ok` after the first world with `min`/`max` and label under keys `cow.weather.layers/<field>`; `climate_offset` is all zero with no cycle and has `min` -0.3 / `max` 0.3; a duplicate or capacity result logs an Error and sets Degraded), AC 2 (`read` returns exactly the committed array; `version()` changes on `Commit` only; a mismatch writes zeros and logs once), AC 3 (zero cells before ready; re-entry registers again with no duplicate); [old 12] AC 32 (for any cell `hypot(east, north)` equals `windSpeed` and `atan2(north, east)` equals `windDirection` mod 360 within float tolerance), AC 33 (after `wind set {degrees: 90, speed: 0.5}` that cell reads `east` near 0, `north` near 0.5), AC 34 (schema entries exist, recorded in history aggregates when tracked, unregistered in `DoStop`) | Not Started | sonnet | [old 11] Needs spike 3. `pressureAnomaly` has no layer. Fake `CellLayerRegistry` reports `Ok`, duplicate and capacity results. These sidecar ranges are what `weather-save.plan.md` reads for validation [old 12] Ten layers in all. Consumers derive speed and direction; A4 registers no speed or direction layer **Merged from old rows 11, 12 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 13 | Overlay registration (`CellOverlayDesc` draw callback, `DIA_DEBUG` only); arrow geometry as shaft plus two head lines through `IDebugDraw` | TDD-exempt (wiring/instrumentation: tests written with the code, no separate RED proof). AC 8 draw half (3 primitives per arrow, colour from the schema ramp by speed), AC 26 half (the overlay draw is absent in Release) | Not Started | sonnet | Thin over `WindOverlayPlan`; test with a recording fake `IDebugDraw`. Plan-truncation Warning once per world |
| 14 | Cell detail rows via `CellLayerRegistry::Details()` and the hover extra from `ExplainCell`, reading `WorldCellFields` | AC 11 (rows only for exactly one selected cell; at most 64 rows) | Not Started | sonnet | Needs spike 3 for the row descriptor shape |
| 15 | **[old 15]** Command registration through `SimCommandSet` and `cow.weather.params` through `ParamCommandHelper` with the `cycle` system action; help for every name **[old 16]** Handlers for `cow.weather.force`, `cow.weather.wind`, `cow.weather.influence` (thin over Feature 7's API) and `cow.weather.load_preset`. RED then GREEN **[old 19]** `cow.weather.find` and `cow.weather.info` handlers: `storms`, `droughts` (via the history adapter), `rain_shadow`, `profile`, `memory`. RED then GREEN | Registration half TDD-exempt (wiring); handler half RED then GREEN. [old 15] AC 12 (the registered name set equals the expected list, each with non-empty help, and the list length is **recounted at build** against the 192-handler cap: 8 today, 9 if the records command is added, see open item 4; a forced failure of the Nth registration leaves zero names and reports Degraded naming the failing entry, for every N), `params` actions `set|get|list|reload|save_defaults` reachable and `cycle` reachable under the same name (a fake `cycle` first, the real handler in Task 17), `cow.weather.reload` is not registered; [old 16] AC 13 (storm calls `AddInfluence`, replies with the id and the "next weekly step" wording, queues no hold; a Feature 7 rejection comes back as an error naming the reason), AC 13a (a planted storm seeds a carried `pressureAnomaly` of -0.3 x weight and moisture once and a later `find storms` still reports the low for more than one week), AC 14 and 15 at handler level, AC 19, AC 13 and 14 for `load_preset` (an unknown name lists the available names); [old 19] AC 18 (`rain_shadow` windward and leeward means for a planted ridge; `info profile` returns **14** pass entries, asserted against `WeatherStepProfile::kPassCount`; `info memory` returns the documented keys including the influence count), AC 21 (`droughts` with `weather.precipitation` untracked returns an error saying so) | Not Started | sonnet | [old 15] The all-or-nothing rollback with each entry's own kind is the kit's; A4 asserts it by forcing a failure on its own list. No hand-written registrar. Description at most 256 characters, up to 16 arguments [old 16] Trace zone per handler. `load_preset` needs Feature 6's loader; its reply lists names from the preset directory. `influence` handler is defined in `weather-influence.md` [old 19] Cell filters use `CellSelectorSet`. **Merged from old rows 15, 16, 19 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |
| 17 | `params cycle` and `reload target:cycles` action handler (Feature 8), `info cycles`, and the `cycle` reply shape | AC 13a (`cycle` and `reload target:cycles` latch at the next `Capture`; `info cycles` equals `PhaseAt`/`WaveAt` for the chosen day, optional `day` scrubs; Feature 8 ACs 37 and 38), the `set` reply carries `effectiveDay` and `visibleDay` through the helper | Not Started | sonnet | Needs Feature 8's cycle catalogue and queue entries; until then it runs on their fakes and stays Not Started |
| 18 | `cow.weather.state` export and import over Feature 4's `EncodeState`/`DecodeState` and the `cow.weather_state` envelope. RED then GREEN | AC 16 (export then import of the same file reproduces the committed state bit-identically at the next `Capture`; import of other dimensions, a bad envelope or a non-finite value is rejected before anything is queued; the file name has no separators and no `..`) | Not Started | sonnet | Needs `weather-save.plan.md`'s codec and `StateReplace` apply. Writes under `out/CoW/weather_states/`. Resets storm ids on import |
| 20 | History events from the stage's `PublishCommit`: `weather.storm`, `weather.storm_track`, `weather.drought`, `weather.drought_end`, with schema entries (`kind: "event"`), caps, de-duplication and the every-4th-week drought cadence. RED then GREEN | AC 22 (a >= 3-cell low-pressure region fires one `weather.storm`, at most 4 per week, payload under 1 KB, a persisting storm does not re-fire), AC 23 (drought once per region per episode and again only after a non-dry week; an untracked layer gives no event and one Info), AC 27 (a storm lasting N weeks emits one `weather.storm` and N `weather.storm_track` with the same id), AC 28 (event side of split and merge), AC 29 (at most 8 tracks per week, largest first, the excess counted in `cow.weather.storm_tracks_dropped`), AC 30 (exactly one `weather.drought_end` on the first non-dry evaluation, none while it runs), AC 31 (both new types have schema entries, payloads under 1 KB) | Not Started | sonnet | Fake `CellHistoryRecorder` records emits. The drought adapter appends the just-committed `precipitation` before `DroughtDetector` runs: a region dry for exactly the window including the just-committed week fires on that commit |
| 21 | Metrics and health: register `cow.weather.raining_cells`, `cow.weather.mean_moisture` and `cow.weather.storm_tracks_dropped` with `MetricInfo` (the stage base owns `step_ms` and `discarded_steps`); health for a failed layer, overlay or command registration; `WTH` logs | TDD-exempt (wiring/instrumentation: tests written with the code, no separate RED proof). AC 25 (the four metrics exist with `MetricInfo`, written only on SimPU once per applied step; `step_ms` is the base's metric and equals the last step's total CPU), AC 1 and 12 health assertions as Degraded or Failing per cause, no "Unhealthy" text | Not Started | sonnet | Commit the group of tasks 20 and 21 |
| 22 | `WeatherDebugDomain` and card (`DIA_DEBUG` only): stats through the stream from Task 1, the overlay toggle, commands posted through the console path | AC 24 (card strings come from the stats struct, not module state; description at most 80 characters; accent is a `DebugGroupAccents` constant), AC 26 half (domain and card absent in Release) | Not Started | sonnet | Gated by Task 1. Layout is S-2.5 Feature 1's, not this feature's |
| 24 | Exhaustive test pass: gap audit of every AC (1 to 34, see the coverage note) against the written tests, then edge cases | Gap list in notes. Edge cases: 1-cell and empty grids, a grid not divisible by 4, all-calm and all-storm worlds, two storms touching at a corner, a drought window exactly equal to the retained window, `weeks` 1 and 52, a selector resolving to zero cells, `state import` of a file with extra keys, export to an existing file, a file name with `..`, the Nth registration failing after N-1 succeeded, commands received before the first world is ready, a storm splitting into three | Not Started | sonnet | No mutation check. Tests only; production bugs go to a separate fix task |
| 25 | Observation Opportunity Scan over every touched file across the five DiaObservation pillars | TDD-exempt (verification/close-out). Findings listed in notes as suggestions | Not Started | haiku | Suggestions only. Expected finds: an overlay plan profile scope, an event-detection duration metric, a rejected-command counter |
| 26 | **[old 26]** Scoped verification: `dia run googletest --filter="WindOverlayPlan*:WeatherCommand*:WeatherDetector*:WeatherCellDetails*:WeatherLayers*" --shards 2`; `dia check deps`; `dia docs precommit` (includes `dia validate schema` on the sidecar) **[old 23]** Release compile check: layers, commands and metrics present; domain, card and overlay draw absent (AC 26). **Owned by the worldstep.plan.md Weather-stack close-out**; this row closes Done with a note when that close-out reports it **[old 27]** Close-out: `dia docs plan` rows to Done with notes, then `dia docs spec-done docs/specs/applications/cow/systems/a4-weather/weather-debug.md` | TDD-exempt (verification/close-out). [old 26] Pass/fail and failing names only; every AC green or listed; [old 23] AC 26; [old 27] Plan and spec statuses updated | Not Started | haiku | [old 26] The full suite and the Release compile check are in worldstep.plan.md Weather-stack close-out [old 23] Row kept so AC 26 has a home. If run, use the CLI, never the executable. No Release run in this feature [old 27] **Do not run until all rows above are Done.** Spec edits (system debug section, Feature 2 helper exports) are not made by this row **Merged from old rows 26, 23, 27 (speed-up pass 2026-11-24): one dispatch, one scoped build, one commit and one `dia docs plan` update for the merged row; write the RED tests for every merged part first, then GREEN. The `[old N]` labels name the original row so its text stays traceable.** |

**AC coverage (was 28 rows, 0-27; now 21 rows after the 2026-11-24 merges: old 3 into 0, 6 into 5, 12 into 11, 16 and 19 into 15, 23 and 27 into 26; row numbers are kept, so some numbers are absent).** AC 1, 1a, 2, 3: Task 11 (health half Task 21). AC 4 to 9: Tasks 8, 13. AC 10: Task 9. AC 11: Task 14. AC 12: Task 15. AC 13: Tasks 5, 15, 18. AC 13a: Tasks 15, 17. AC 14: Tasks 5, 15. AC 15: Tasks 5, 15. AC 16: Task 18. AC 17, 19: Task 5. AC 18: Tasks 7, 15. AC 20: Task 7. AC 21: Task 15. AC 22, 23: Task 20. AC 24: Task 22. AC 25: Task 21. AC 26: Tasks 13, 22, 26. AC 27 to 31 (T-1 consumer amendment, previously unplanned): Tasks 7, 20. AC 32 to 34 (wind component layers, previously unplanned): Task 11. New for the amendment (`WorldCellFields` contribution, selectors, the `params` actions through the helper): Tasks 10, 15. Deleted: the hand-written params, reload and registrar handlers.

## Open items

1. **Hotkey table and panel stream** are resolved only by spikes 1 and 3; a new Dia message type would be an `AD-001 ask first` item. Still open until the spikes run.
2. **Drought cadence and overlay defaults** (spec Open Q1, Q2) are unmeasured guesses; they are config and taste checks for S-2.5, not tests.
3. **`set_region` and S-1 `region` ids versus selectors.** Amendment 4 says cell selection uses `CellSelectorSet`, but the spec still shows `region` ids from S-1's `RegionTable` and does not say whether regions are registered as selectors. The plan keeps `region` as a selector alias; the spec should say.
4. **Command name count.** The 8 names are unchanged by the amendment (`reload` was always an action), but the kit's `cow.<sys>.records {action: reload|list}` (spec-6 resolution 2) applies if A4's influence-kind and cycle catalogues are record files, which would make 9. The spec's "recount the names against the 192 cap" has no number; Task 15 asserts the registered list, not a literal 8.
5. **`params get|list|save_defaults` for per-biome map entries** (`<key>.<mapKey>` rows): whether `list` returns one row per biome or one per map is the kit's choice and is not in the A4 spec. The plan asserts only that `list` equals the sidecar's entries.
6. **`cow.weather.state` and the biome palette**: the export file's fields are the save record's, which now include `biome_palette`; the amendment does not say whether `state import` remaps through the registry (the plan assumes it does, via the kit codec).
7. **The T-1 consumer amendment says "plan impact"** (fold in before start); this plan folds it in. It adds a fourth metric (`storm_tracks_dropped`) and ACs 27 to 34, which the spec's own AC numbering lists out of order (27 to 31 after 26, 32 to 34 after the wind section).

> **Re-plan required (Draoi amendment, 2026-11-21):** file paths, package split and test locations changed; see the spec's Draoi layer amendment section. Do not build from this plan as written.

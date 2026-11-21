# Feature Spec: A4 Weather Debug

**Parent:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.md (Feature 5)
**Status:** Approved (2026-11-16). Amended 2026-11-17 (user decisions: A5 hand-off, Features 7–9): the `weather.ground_water` layer becomes `weather.water_to_ground`; ground water is A5's `hydro.ground`. Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d): eighth scalar layer `weather.climate_offset` (ten layers with the wind components), **eight** names (`cow.weather.influence`), 14-entry profile, `info cycles`, `params cycle`, `force storm` through the influence API, ten-field cell details.
**Depends on:** A4 Features 1, 2, 3, 4, 6 (fields, pipeline helpers, `WeatherModule` and its mutation queue, state codec, `ApplyPreset`); DiaCellLayers (`CellLayerRegistry`, `CellOverlayDesc`, `Details()`), DiaCellHistory (`CellHistoryRecorder`, events), DiaValueSchema, `SimQueryHostModule` with `SimHelpInfo`, `DiaDebugDomainRegistry`/`DiaDebugPanel`, DiaObservation (all built).
**Consumed by:** S-2.5 (hosts everything here), S-3 and S-3.5 (host the layers), V-1 (reads nothing from here).

**Amended 2026-11-18 (world-sim review, user decisions):** see "World-sim review amendment" below.

## Summary

Everything a developer uses to look at and poke the weather: eight scalar cell layers (the seven field layers plus `weather.climate_offset`; ten with the two wind component layers), a wind overlay that is dense only where it needs to be, per-cell detail rows, a panel card, **eight** sim-hosted `cow.weather.*` names (consolidated from the system spec's sixteen, decided 2026-11-16; Feature 7 adds `cow.weather.influence`, amended 2026-11-17), history events for storms and droughts, metrics, and health. It reads committed state only and changes state only by queuing mutations through Feature 3's queue.

The panel's *layout* is S-2.5 Feature 1's mockup. This feature defines the data the card shows and the commands it posts.

## Goals

1. Each of A4's seven scalar fields, and the cycle temperature offset (`weather.climate_offset`, Feature 8), is a base cell layer with a schema range, a label and a hotkey, readable on SimPU and re-read only when a step commits.
2. The wind overlay shows every large-scale pattern at a bounded cost and refines where wind changes fast or a storm sits (decided 2026-11-16).
3. Every action the panel offers is a registered command; the panel posts the same commands (UI/business separation, editor-actions rule).
4. The command set takes **8 names** of the per-PU sim-hosted budget (7 here plus Feature 7's `cow.weather.influence`; amended 2026-11-17, row 6d), with registration all-or-nothing. (Checked 2026-11-16: `SimQueryHost::kMaxHandlers` is **192** in code today, so the 64 quoted earlier in the A4 and World Step specs is stale. The consolidation is kept because it still maps one name to one GDD verb and leaves headroom; it is no longer a blocker.)
5. The same typed command model drives the headless runner, so a scenario's setup means the same thing live and offline.
6. Nothing here adds state to the Approved save format.

## Non-Goals

- The stage, the S-2.5 card layout, click-to-inspect and the success-criteria walkthrough (S-2.5).
- DiaCellHistory's in-game panel, `step_back`, snapshots and the cell inspector (DiaCellLayers and DiaCellHistory commands; deleted from A4 on 2026-11-13).
- The preset files and `ApplyPreset` (Feature 6); this feature only exposes `load_preset`.
- Weather mechanics, parameters and save/load (Features 1–4).

## Design

### Placement

`Cluiche/CoW/Modules/WeatherDebug/`: `WeatherLayerRegistration`, `WeatherWindOverlay`, `WeatherCommands`, `WeatherCellDetails`, `WeatherHistoryEvents`; pure parts in `Cluiche/CoW/Shared/Weather/Debug/`: `WindOverlayPlan`, `WeatherCommand` (the typed model, parser, validation), `StormDetector`, `DroughtDetector`, `ExplainCell`. `WeatherModule` owns them; layers and commands are compiled in **all** configurations (S-3.5's headless regression needs them in Release), and the `IDebugDomain`, the panel card and the overlay draw callback are `DIA_DEBUG` only.

### Cell layers

Registered on SimPU through `CellLayerRegistry::Register(CellLayerDesc)` after `WorldGenerationComplete` (the shared `CellGridDesc` is S-1's `WorldHexGrid`). A result other than `Ok` logs an Error and sets Degraded.

| Layer id | Field | Range |
|---|---|---|
| `weather.moisture` | `moisture` | 0–1 |
| `weather.pressure` | `pressure` | 0–1 |
| `weather.temperature` | `temperature` | 0–1 |
| `weather.precipitation` | `precipitation` | 0–1 |
| `weather.snow_pack` | `snowPack` | 0–1 |
| `weather.cloud_cover` | `cloudCover` | 0–1 |
| `weather.water_to_ground` | `waterToGround` | 0–2 (rain up to 1 plus melt up to 1; replaces `weather.ground_water`, amended 2026-11-17, A5 Hydrology) |
| `weather.climate_offset` | the latched climate-cycle `tempOffset` (Feature 8) | −0.3–0.3 (the eighth scalar base layer; amended 2026-11-17, row 6d; all-zero with no active cycle) |

`pressureAnomaly` has **no layer**: it is shown by the cell details and is visible through `weather.pressure` (pass 2 adds it) and `find storms`. `climate_offset` is not tracked by default (the `history` config is capped at 255 characters; adding it to a `track` list is a per-stage choice).

- Each is a **base layer** with a pull callback `read(float* dest, uint32_t count)` that copies the committed array, and a `version()` that is the commit counter, so DiaCellLayers re-reads only after a commit. `count` must equal the active cell count; a mismatch writes zeros and logs once.
- Before the first world is ready the callbacks report zero cells and nothing is drawn.
- Labels, ramps, `min`/`max` and hotkeys come from DiaValueSchema entries `weather/<key>` (the schema file ships with this feature). Float layers need `min`/`max` or history tracking is refused. Hotkeys follow the S-2.5 GDD table (Snow = **K**); a clash logs a Warning and the later layer gets none. The exact key table is read from the GDD in the first plan task, not restated here.
- Wind speed and direction are **not** scalar layers (no vector layer kind exists); they are the overlay below. **Superseded 2026-11-16:** wind is also recorded as two component layers, `weather.wind_east` and `weather.wind_north`, see "Wind component layers" at the end.
- Layers unregister in `DoStop`; re-entry re-registers.

### Wind overlay (`CellOverlayDesc`, `DIA_DEBUG`)

No arrow primitive exists in `IDebugDraw`, so each arrow is **a shaft and two head lines** (3 line primitives).

**Sampling plan** (`WindOverlayPlan`, pure, recomputed only when the version changes and at most once per commit):

1. Divide the grid into aligned **4 × 4** blocks. Base sample: **one arrow per block**, the block's centre cell (hence every 4th cell in each axis).
2. Score each block from committed data:
   - `variation` = the speed-weighted circular spread of `windDirection` in the block plus the range of `windSpeed` in the block, scaled to [0, 1];
   - `storm` = true if any cell in the block has `pressure < storm_pressure` (default **0.3**, the GDD storm centre).
3. Refine: `storm` → stride **1** in the block; else `variation >= refine_high` (default **0.50**) → stride 1; `variation >= refine_low` (default **0.25**) → stride 2; else stride 4 (the base arrow). Refined arrows draw the sampled cell's own wind.
4. **Hard cap** `max_arrows` (default **2,000**, so at most 6,000 primitives against the 8,192-entry budget). When the plan exceeds it, refinements are dropped in order of lowest score (stable by block index) until it fits; the base arrows (at most `ceil(N/16)`, 416 at Large) are always kept.
5. A cell with `windSpeed < 0.02` draws no arrow. Arrow length is a fixed fraction of a cell, scaled by speed; colour is by speed through the schema ramp.

The four defaults are overlay config in the `.diaapp` `config` block (`wind_overlay`: `base_stride`, `refine_low`, `refine_high`, `storm_pressure`, `max_arrows`) and validated (strides ≥ 1, thresholds in [0, 1], `refine_low <= refine_high`, `max_arrows <= 2,700`). The plan is deterministic: the same committed state gives the same arrow list. System Open Question 2 is closed by this section.

### Per-cell detail rows

Registered with `CellLayerRegistry::Details()` (64 rows max, called only when exactly one cell is selected) and a hover extra. `ExplainCell(sim, cell)` (pure) returns the committed ten fields (amended 2026-11-17, row 6d: `pressureAnomaly` is the tenth) and the derived values the pipeline would use (amended 2026-11-17, A5 Hydrology: `waterToGround` replaces `groundWater`; ground-water and soil rows are A5's), plus the cell's current climate offsets (temperature, evaporation multiplier, wind shift) and the ids of the influences covering it, plus the latched A5 inputs for the cell (`surfaceWetness`, `openWater`, `glacierEligible`, or "no A5"): biome, seasonal baseline temperature for the current season, prevailing wind angle for the cell's latitude band, the pressure-perturbation vector magnitude, the upwind neighbour's elevation difference and whether orographic lift applies, the snapped direction index, and distance to the nearest water cell. It calls Feature 2's pure helpers rather than re-deriving them (Feature 2 exports `PrevailingDirection`, `PressureGradient`, `SnapWindDirection` unchanged; no behaviour change).

### Commands (8 sim-hosted names)

(Amended 2026-11-17, row 6d: the 8th, `cow.weather.influence`, is Feature 7's and is registered here with the other seven, all-or-nothing.) All registered through `SimQueryHostModule` with mandatory `SimHelpInfo` (non-empty description ≤ 256 chars, category `weather`, owner `WeatherModule`, arguments as `ValueParam`s ≤ 16). **Arguments are JSON objects** with an `action` field where one name has several. A bad or unknown `action`, a missing required field, an out-of-range value or an unknown cell index returns an error naming the field and **queues nothing**.

| Name | Kind | Actions and arguments |
|---|---|---|
| `cow.weather.force` | command | `action`: `storm` / `high_pressure` / `rain`. `storm` **creates a STORM influence through Feature 7's `AddInfluence`** with source `"debug.force"` (amended 2026-11-17, row 6d; the debug command and gameplay use one path): `cell` is the centre, `intensity` (0–1) maps to `strength`, optional `radius` (default 3), `weeks` (1–`max_lifetime_weeks`, 13) is the lifetime; the storm seeds a carried `pressureAnomaly` and moisture once, then the CA moves it. It queues no hold. `high_pressure` (`cell`, `intensity`, `weeks` 1–52) is a pressure hold high + moisture hold low through the Feature 2 hooks, and `rain` (`cell`, `amount` 0–1) a one-week `Rain`. A rejected storm returns Feature 7's rejection reason |
| `cow.weather.wind` | command | `action`: `set` (`cell`, `degrees` 0–360 on-screen counter-clockwise, `speed` 0–1) / `set_region` (`region` id from S-1's `RegionTable`, same angle and speed) / `clear`. Region forms resolve the region's cells into a cell set and queue **one** region forcing |
| `cow.weather.params` | command | `action`: `reload` (optional `target`: `params` (default) or `cycles`, re-reading the cycle file; Feature 8) / `set` (`key` dotted, `value`) / `cycle` (`id`, and one of `enabled`, `pin` (a `pinned_phase_days`), `unpin`, `start` (a `phase_days`); Feature 8). All route through Feature 3's `QueueReload` / `QueueParameterSet` / the cycle queue entries |
| `cow.weather.state` | command | `action`: `export` (`file`) writes the committed state as a `cow.weather_state` envelope using Feature 4's codec, to `out/CoW/weather_states/<file>.json`; `import` (`file`) validates it fully, requires matching dimensions, then queues a replacement applied at the next `Capture` |
| `cow.weather.load_preset` | command | `name`. Loads and validates the preset (Feature 6), then queues it. Unknown name lists the available names |
| `cow.weather.influence` | command | Feature 7's `action`: `add` / `remove` / `list` / `clear`, over `AddInfluence`/`RemoveInfluence`/`ListInfluences`; defined in `weather-influence.md` (amended 2026-11-17, row 6d) |
| `cow.weather.find` | query | `what`: `storms` (pressure below `storm_pressure`; connected regions, size, anchor cell; a planted storm now stays below the threshold for several weeks because the carried anomaly keeps the low, aligned to locked GDD §4.1) / `droughts` (`weeks`; see below) / `rain_shadow` (`cell`; windward vs leeward mean precipitation over the cell's upwind and downwind neighbours within 3 steps) |
| `cow.weather.info` | query | `what`: `profile` (per-pass wall and CPU microseconds of the last step, range count; **14 entries**, amended 2026-11-17, row 6d) / `memory` (buffer sizes, queue depth, parameter source, baseline provider, week, season, influence count) / `cycles` (Feature 8: per cycle id, enabled, pinned, `u`, `wave`, contributions, and `coldness` for a chosen cell; optional `day` scrubs) |

- All mutations are **queued and latched at the next `Capture`**. The reply says so and names the week they will first affect when known.
- A **typed command model** (`WeatherCommand`, parsed once) is the single execution path. `IWeatherCommandSink` has two implementations: the **queued sink** (module, live) and the **direct sink** bound to a `WeatherSimulation&` (the headless runner and S-2.5's scenario setup, which applies immediately between steps). The panel posts the same JSON commands through the console path; it never calls the module.
- **Registration is all-or-nothing:** if registering any of the eight fails, the ones already registered in that call are removed and the module reports Degraded with the failing name. A test forces the failure and asserts zero names left. Names unregister in `DoStop`.
- The system spec's 16 names map as: `spawn_storm`/`spawn_high_pressure`/`force_rain` → `force`; `set_wind`/`set_wind_region`/`clear_wind_overrides` → `wind`; `reload`/`set_param` → `params`; `export_state`/`import_state` → `state`; `find_storms`/`find_droughts`/`check_rain_shadow` → `find`; `profile`/`report_memory` → `info`; `load_preset` unchanged. The eighth name, `influence`, is new (Feature 7).

### Droughts (decided 2026-11-16: read DiaCellHistory)

A4 keeps **no dry-week counter** and the save format is unchanged. `DroughtDetector` (pure) takes `IPrecipitationHistory` (a per-week float span over the tracked `weather.precipitation` ring, newest last) and `weeks`, and returns the cells whose precipitation was below `dry_threshold` (default 0.02) for **every** one of the last `weeks` weeks, grouped by S-1 region. An adapter over DiaCellHistory's read interface (the exact call is fixed in the first plan task) supplies the history. If `weather.precipitation` is not tracked, or the retained window is shorter than `weeks`, the query returns an error saying which, with the retained window. S-2.5 already tracks the layer.

### History events

Emitted from the stage's `Commit` with `CellHistoryRecorder::Emit(HistoryEvent{type, group, cell, payloadJson, text})`, committed by the driver's next sample (so ordering holds by construction). `payloadJson` is compact and under 1 KB.

- `weather.storm`: when a connected region of `pressure < storm_pressure` of at least 3 cells appears whose anchor (lowest-pressure cell) was not part of a storm last week. Payload: size, minimum pressure, anchor. At most 4 per week.
- `weather.drought`: evaluated every 4th applied week through `DroughtDetector` at `drought_weeks` (default 8), once per region per episode (a region already reported is not re-reported until it has had a non-dry week). Skipped with one Info log per world if the precipitation layer is not tracked.
- Each type has a `kind: "event"` DiaValueSchema entry with its payload fields, or the recorder logs a warning per type in Debug.

### Panel card (`DIA_DEBUG`)

`WeatherDebugDomain : IDebugDomain` registered with `VisualDebuggerModule::RegisterDomain`; `GetDescription()` ≤ 80 characters; accent from `DebugGroupAccents`. The card **shows**: week, season, cell count, grid totals (mean moisture, total snow pack, raining cells), parameter source (file, fallback or pack), baseline provider (A1 or table), the last step's CPU ms, and the coordinator's slack (read from `WorldStepModule`'s status, not recomputed); and, once Features 7–9 are built (amended 2026-11-17, row 6d), the live influence count, one row per climate cycle (id, `u` as a percentage, wave, on/off/pinned) and `commitDay`, the smoothing fraction `t` and the live preview count, with a raw/smoothed wind toggle; **toggles** the wind overlay; and **posts commands only**. The SimPU-owned values reach the card the way other domains get them (a published stats message, not a cross-thread read; see Open Design Question 3).

### Instrumentation

- **Metrics** (`MetricInfo` on each, written once per applied step on SimPU, SD-PERF-7): `cow.weather.step_ms` (total CPU of the last step, required by S-3.5 R5), `cow.weather.raining_cells`, `cow.weather.mean_moisture`.
- **Trace/profile:** `DIA_TRACE_ZONE` on each command handler, the overlay plan and event detection; the pipeline's per-pass `DIA_PROFILE_SCOPE` already backs `info profile` (Feature 2).
- **Health:** adds to Feature 3's: Degraded if layer, overlay or command registration fails.
- **Logs** (`WTH`): Info on layers/commands registered, preset loaded, state exported/imported; Warning on a hotkey clash, a rejected command, a plan truncated by `max_arrows` (once per world), a history layer not tracked; Error on a registration failure.

## Public API

As Placement. New messages: none (a stats struct reaches the card through the existing debug stats path). Edited: `WeatherModule` (owns the debug parts), `WeatherStage::Commit` (event detection), Feature 3's queue gains `QueueRegionForcing(const WeatherForcing&, const DynamicArrayC<uint32_t>& cells)` so a region forcing's cell set is registered inside `Capture` (amendment to Feature 3), Feature 2 exports three pure helpers (no behaviour change).

## Acceptance Criteria

### Layers
1. All eight scalar base layers (the seven field layers and `weather.climate_offset`) register with `Ok` after the first world is ready, with schema `min`/`max` and a label; a duplicate or capacity result logs an Error and sets Degraded. (Amended 2026-11-17, row 6d; the two wind component layers are AC 32–34.)
1a. `weather.climate_offset` equals the latched `tempOffset` of the committed week (Feature 8 AC 39), is all zero with no active cycle, and has `min` −0.3 / `max` 0.3 in its schema.
2. `read` returns exactly the committed array; `version()` changes on `Commit` and not otherwise; a `count` mismatch writes zeros and logs once.
3. Before a world is ready the callbacks report zero cells and nothing is drawn; after stage re-entry the layers are registered again and none is duplicated.

### Wind overlay
4. Calm uniform wind plans exactly one arrow per 4 × 4 block (416 at Large 104 × 64).
5. A block with a `pressure < 0.3` cell is planned at stride 1; a block with direction spread above `refine_high` is stride 1; above `refine_low` stride 2.
6. The plan never exceeds `max_arrows`; when over, refinements are dropped lowest score first and every base arrow survives; a world-wide storm does not exceed the cap.
7. The plan is deterministic and recomputed only when the version changes.
8. Arrows with speed under 0.02 are omitted; each arrow is 3 primitives; total primitives at the cap are ≤ 6,000.
9. Invalid overlay config (stride 0, `refine_low > refine_high`, `max_arrows` over 2,700, a threshold outside [0, 1]) is rejected with the key named and defaults are used.

### Cell details
10. `ExplainCell` matches the pipeline helpers for the same cell (prevailing direction, snapped direction, orographic lift decision) on a hand-built terrain with known answers.
11. Detail rows appear only for a single selected cell and stay within 64 rows.

### Commands
12. Exactly **8** names register (amended 2026-11-17, row 6d: seven here plus `cow.weather.influence`), each with non-empty help; a forced failure of the Nth registration leaves **no** names behind and reports Degraded.
13. For each action: a valid request queues the expected forcing, parameter change, state replacement or preset, takes effect at the next `Capture` and not before, and the reply says so. `force` `storm` instead calls `AddInfluence` (source `"debug.force"`), replies with the id and the "next weekly step" wording, and queues no hold; a Feature 7 rejection comes back as an error naming the reason (amended 2026-11-17, row 6d).
13a. `force` `storm` seeds a carried `pressureAnomaly` (−0.3 × weight at full strength) and moisture once; a later `find storms` still reports the low for more than one week. `params` `cycle` and `reload target:cycles` latch at the next `Capture`; `info cycles` equals `PhaseAt`/`WaveAt` for the chosen day (Feature 8 AC 37, 38).
14. Each of these returns an error naming the field and queues nothing: unknown `action`, missing required field, `cell` out of range, `degrees` outside 0–360, `speed`, `intensity` or `amount` outside 0–1, `weeks` outside 1–52, unknown `region`, unknown preset (the reply lists names), unknown parameter key, an out-of-range parameter.
15. `wind` `set_region` queues one region forcing covering exactly the region's cells; `clear` removes every wind override at the next `Capture`.
16. `state export` then `state import` of the same file reproduces the committed state bit-identically at the next `Capture`; `import` of a file with other dimensions, a bad envelope or a non-finite value is rejected before anything is queued.
17. The direct sink and the queued sink produce **identical** simulation state after the same command list and the same steps (the live-vs-headless equivalence, tested once).
18. `find` `storms` returns connected low-pressure regions on a hand-built terrain with a planted storm; `rain_shadow` returns windward and leeward means for a planted ridge; `info profile` returns **14** pass entries (`WeatherStepProfile::kPassCount`: 0, 1, 2, 3, 4, 5a, 5b, 5c, 6a, 6b, 7, 8, 9, finish; amended 2026-11-17, row 6d); `info memory` returns the documented keys, including the influence count.
19. A full mutation queue (Feature 3) turns a command into a clear error, not a drop.

### Droughts and events
20. `DroughtDetector` flags exactly the cells dry for all of the last `weeks` weeks (a cell with one wet week inside the window is excluded) and groups them by region; `weeks` longer than the window returns an error naming the retained window.
21. `find droughts` with `weather.precipitation` untracked returns an error saying so.
22. A `weather.storm` event fires when a ≥3-cell low-pressure region appears, once per storm, at most 4 per week, with payload under 1 KB; a persisting storm does not re-fire.
23. A `weather.drought` event fires once per region per episode and again only after a non-dry week; no event and one Info log if the layer is untracked.

### Card and instrumentation
24. The card's strings come from the stats struct, not from a cross-thread read of module state; its description is ≤ 80 characters and its accent is a `DebugGroupAccents` constant.
25. The three metrics register with `MetricInfo` and are written only on SimPU once per applied step; `step_ms` equals the last step's total CPU.
26. In a Release build the layers, commands and metrics exist and the domain, card and overlay draw do not.

## Files Touched

New: the placement files; schema file `Assets/CoW/Global/Weather/weather_schema.json` (layer labels, ranges, hotkeys, event types); tests `Tests/GoogleTests/CoW/Weather/{TestWindOverlayPlan,TestWeatherCommands,TestWeatherDetectors,TestWeatherCellDetails,TestWeatherLayers}.cpp`. Edited: `WeatherModule`, `WeatherStage`, Feature 3's queue, Feature 2 (export three helpers), vcxproj via `dia docs vcxproj-add`. **Spec edits on approval:** the system spec's debug section (8 names, overlay plan, droughts via history, closed Open Q2), Feature 3's queue (`QueueRegionForcing`), Feature 2's helper exports. No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL in public APIs | Plans, commands and detector results use Dia containers and fixed-capacity arrays |
| AD-001 | Ask permission before Dia edits | None. All the DiaCellLayers, DiaCellHistory, DiaValueSchema and debug-domain APIs are built and used as-is |
| SD-PERF-7 | Telemetry cost budgets | Three gauges, written once per applied step on SimPU; no per-cell telemetry; overlay capped at 6,000 primitives |
| Sim-hosted name cap (192/PU in code; the 64 in earlier specs is stale) | Per-PU budget | 8 names (7 here plus `cow.weather.influence`), registered all-or-nothing |
| Editor-actions rule | Every tool action is a command; the panel calls the same commands | The panel posts JSON commands only; criterion 17 proves one execution path |
| PU boundaries are streams | No mutex-guarded cross-PU reads | The card reads a stats struct published from SimPU (criterion 24) |
| DFS-002 | `Run` reads only the captured copy | Nothing here touches the working copy; all reads are of committed state on SimPU |

## Test Plan

Pure parts first and heavily (overlay plan, command parsing and validation, detectors, `ExplainCell`), each with hand-built terrain and known answers; then the module wiring against a fake `CellLayerRegistry`, fake `SimQueryHostModule` (including the forced registration failure) and fake recorder; then one equivalence test between the two sinks. TDD order: command model, detectors, overlay plan, layers, details, wiring, events. Layer goldens are not added (Feature 6 owns them). Scoped `--filter` while iterating, one full `--shards >= 2` run at the end. No mutation checks. Gap audit and edge pass is the last plan task. **Observation scan** at verify. The panel layout and a screenshot taste check are S-2.5 Feature 1's.

## Open Design Questions

1. **Is the drought event cadence (every 4th week) right?** Evaluating each week costs a history read of `weeks` layers (8 × 6,656 floats at Large, about 200 KB) on SimPU at `Commit`; every 4th week is a guess that keeps it unmeasured-but-small. If it shows in the commit profile, move detection off SimPU or lengthen the cadence; if droughts are reported late, shorten it.
2. **Are the overlay defaults (0.25 / 0.50 / 2,000) right?** They are first guesses chosen to keep the cap far under the geometry budget. They should be tuned against real S-2.5 storms, which is a taste check for you, not a test. Since they are config, tuning needs no code.
3. **How does the card get SimPU values?** The system spec says "the way other domains do" but the existing precedent (`debug.domain_state`) is the known cross-PU debt in memory. The stats-struct-through-streams route is assumed; the first plan task should confirm which stream the debug panel actually reads and use it rather than the mutex precedent.
4. **Does `find` belong in A4 at all long-term?** `rain_shadow` and `storms` are weather-specific and fine here, but `droughts` is mostly a history query. If T-1 grows a generic history-query surface, `droughts` should move there and drop to 6 names.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Dependencies.** This feature also depends on Feature 4's codec (`EncodeState`/`DecodeState`, the `cow.weather_state` envelope defined in Feature 4's amendment 2) for `state export`/`import`.
2. **Queue.** `QueueRegionForcing` already exists in Feature 3; this feature adds nothing to it. The two entries this feature needs, `StateReplace` and `ClearWindOverrides`, are in Feature 3's amendment 1.
3. **`info profile`** returns Feature 2's pass entries (**14**, amended 2026-11-17, row 6d: 0, 1, 2, 3, 4, 5a, 5b, 5c, 6a, 6b, 7, 8, 9, finish; `WeatherStepProfile::kPassCount`; was 13 with the retired pass 10 still counted).
4. **Drought at commit.** `DroughtDetector` at `Commit` cannot see the week being committed in DiaCellHistory (the driver samples afterwards). The history adapter therefore appends the **just-committed** `precipitation` as the newest week before evaluating; the same adapter is used by `find droughts`, which reads only committed, sampled weeks.

## T-1 consumer amendment (2026-11-16, approved with T-1 Feature 10)

Requested by @docs/specs/applications/cow/systems/t1-worldsiminspector/space-over-time.md for storm and drought tracks. Supersedes contradicting text above.

- **Storm ids.** `weather.storm` payload gains `id` (uint32, per world, from 1, reset on world change and on state import).
- **`weather.storm_track`** (new type). Emitted at `Commit` every applied week for each active storm, the largest first, at most **8 per week**. Payload `{ id, anchor, centroid: [q, r], size, minPressure }` (about 100 bytes). The week a storm starts emits both `weather.storm` and its first `weather.storm_track`.
- **Continuity.** A storm keeps its `id` while this week's low-pressure region overlaps last week's region with that id; the largest overlap wins. A split gives the smaller part a new id (and a `weather.storm` event, within the existing cap of 4 per week). A merge keeps the older id. A storm whose region has no overlap is over and its id is never reused. This extends the existing "anchor not part of a storm last week" rule; the per-storm cell sets of last week are kept on SimPU in the module (not saved; after a load, ids restart and the first week re-announces live storms).
- **`weather.drought_end`** (new type). On the drought cadence (every 4th applied week), for a region already reported and not yet ended, the first evaluation with a non-dry week emits `{ region, weeks }`, where `weeks` is the episode length up to that evaluation.
- **Schema.** Both new types get `kind: "event"` DiaValueSchema entries with their payload fields.
- **Metric.** `cow.weather.storm_tracks_dropped` (counter, with `MetricInfo`) counts tracks beyond the cap of 8. AC 25 now covers four metrics.

Added criteria:

27. A storm that persists for N weeks emits one `weather.storm` and N `weather.storm_track` events with the same id; centroids follow the low-pressure region.
28. A split keeps the id on the larger part and gives the smaller part a new id with its own `weather.storm`; a merge keeps the older id; ids are never reused within a world.
29. At most 8 track events per week are emitted, the largest storms first, and the excess is counted in `cow.weather.storm_tracks_dropped`.
30. A region reported in drought emits exactly one `weather.drought_end` on the first evaluation with a non-dry week, and none if the drought is still running.
31. Both new event types have schema entries; payloads are under 1 KB.

**Plan impact:** the plan (written, not started) needs these folded into its events task before it starts.

## Wind component layers (2026-11-16, user decision, T-1)

Supersedes "Wind speed and direction are **not** scalar layers" above. The user needs wind to be derivable in T-1 (charts, maps, per-region aggregates, correlations). Without a vector layer kind (a Dia engine change, deferred), wind is recorded as two scalar layers:

| Layer id | Field | Range |
|---|---|---|
| `weather.wind_east` | `windSpeed × cos(windDirection)` | −1–1 (fraction of full speed, east positive) |
| `weather.wind_north` | `windSpeed × sin(windDirection)` | −1–1 (north positive) |

- `windDirection` is the A4 on-screen counter-clockwise angle used by `cow.weather.wind` (0° = east). The components are computed at `Commit` from the committed `windSpeed` and `windDirection`, so they match the overlay exactly.
- Same registration rules as the other scalar layers (base layers, pull callback, `version()` is the commit counter, schema entries `weather/wind_east` and `weather/wind_north` with `min -1`, `max 1`, a diverging ramp). **Ten weather layers in total** (eight scalar base layers plus these two; amended 2026-11-17, row 6d).
- **Derivation is done by consumers, not A4.** T-1 derives `speed = hypot(east, north)` and `direction = atan2(north, east)` (see T-1 Feature 2 "Derived metrics"). A4 registers no speed or direction layer.
- **Aggregate caveat (documented in T-1):** DiaCellHistory aggregates are min/mean/max of each component, so a group's derived value from its **mean** components is the *resultant* wind (speed shrinks when directions disagree). Exact mean speed needs per-cell data (tracked layers), where `hypot` is applied per cell first.
- Cost: two more recorded layers, and 26 KB per sample per cell tracking both at Large. S-2.5 tracks both by default (its recorder budget is unchanged: the S-2.5 tracked-layer list is nine weather layers; `weather.climate_offset` is not tracked by default).

Added criteria:

32. `weather.wind_east` and `weather.wind_north` register with ranges −1 to 1; for any cell, `hypot(east, north)` equals the committed `windSpeed` and `atan2(north, east)` equals `windDirection` (mod 360), within float tolerance.
33. After `cow.weather.wind set {degrees: 90, speed: 0.5}` on a cell, that cell reads `east ≈ 0`, `north ≈ 0.5`.
34. The two layers have schema entries, are recorded in DiaCellHistory aggregates when tracked, and unregister in `DoStop`.

**Plan impact:** `weather-debug.plan.md` and S-2.5 Feature 5's tracked-layer config need these folded in before they start.

## World-sim review amendment (2026-11-18)

User decisions from the world-sim review. Each item **supersedes** any contradicting text in this spec. Sources: [summary](../../../../../research/worldsim_data_driven/summary.md) §0, §0.1; [parameter-files](../../../dia/systems/diaattribute/parameter-files.md); [biome-registry](../s1-worldgeneration/biome-registry.md); [worldsim-kit](../worldstep/worldsim-kit.md); [grid-search-helpers](../../../dia/systems/diapathfinding/grid-search-helpers.md); [blackboard-scopes](../../../dia/systems/diablackboard/blackboard-scopes.md).

1. **Commands (system items 8, 9).** Supersedes the eight-name list, the params, `reload` and `cycle` command descriptions and the name budget: one `cow.weather.params {action}` via `ParamCommandHelper` with `set | get | list | reload | save_defaults` and A4's `cycle` action; `set` replies `{queued, key, value, effectiveDay, visibleDay}`; `save_defaults` is dev-only. `cow.weather.reload` is retired. Registration through `SimCommandSet` (all-or-nothing). Recount the names against the 192 cap.
2. **Schema keys.** Supersedes `weather/<key>` and `weather/wind_east`, `weather/wind_north`: layer entries are `cow.weather.layers/<field>` (built by `WorldSimNames.h`). Layer ids stay `weather.<snake_case>`.
3. **Health and metrics (item 5).** "Unhealthy" reads Failing; `cow.weather.step_ms` and `cow.weather.discarded_steps` come from the stage base.
4. **Selectors (item 12).** Any command argument or panel filter that selects cells uses `CellSelectorSet` / `WorldCellFields`.
5. **Cell details.** Weather values appear in `WorldCellFields` under slot `weather`; detail rows read from that, not a private list.

## Status

**Status:** `Approved` (2026-11-16; T-1 consumer amendment added the same day). Amended 2026-11-17 (A5 hand-off). Amended 2026-11-17 (aligned to locked GDD, CHANGE-047; row 6d): Features 7–9 now add one command name, one layer, the `cycles` info action and the `cycle` params action, specced in their own files and folded in above. Plan: [weather-debug.plan.md](weather-debug.plan.md) (written 2026-11-16, not started; fold in the T-1 consumer amendment).

**Amended 2026-11-18 (world-sim review).** Status stays `Approved`. Amended 2026-11-18 (world-sim review decisions; user-approved designs in the specs referenced below).


## Draoi layer amendment (2026-11-21)

Applies the World-Sim Kit's "Draoi layer amendment" (@docs/specs/applications/cow/systems/worldstep/worldsim-kit.md) and the system spec's matching section. **This whole feature stays in CoW** (debug-domain, cell-layer, overlay and command wiring are application code under PD-012); status stays `Approved`.

- **Reads from the Draoi sim:** layers, detail rows, droughts and the overlay read the committed `WeatherFields` of `DraoiHexWorldWeather` through the stage; types and field names are unchanged, only their header path moves. The layer and cell-detail registrations remain in `WeatherModule` / `WeatherStage`.
- **Commands:** the 8 `cow.weather.*` names stay in CoW (`WorldSimNames`). Command bodies call the sim public API (`AddForcing`, `RegisterCellSet`, `Reseed`, `SetCommittedWeek`, ...) and the CoW adapter queue; they do not move.
- **Schema, labels, hotkeys:** `weather_schema.json` stays content under `Assets/CoW/Global/Weather/`.
- **Files Touched:** header paths in includes change; no new Draoi files from this feature. The three helpers Feature 2 exports for the commands are exported from the Draoi package.
- **Open question:** none new.
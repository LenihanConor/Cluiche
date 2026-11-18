# Feature Spec: A5 Hydrology Debug & History

**Parent:** @docs/specs/applications/cow/systems/a5-hydrology/a5-hydrology.md (Feature 5)
**Status:** Approved (2026-11-17, user, after the locked-GDD alignment)
**Design source:** the locked GDD `C:\GitHub\DesignDocs\COW\Output Docs\GDD\08_SystemsDeepDive\A5_Hydrology\` (locked 2026-11-17: `A5_AuthoringDebug.md` §1–§5a, `A5_TuningParameters.md`; DECISION-129..134), the A5 brief in `Cluiche/out/CoW/gdd-foldback-hydrology-weather-2026-11-17.md`, and the user decisions of 2026-11-17. The GDD owns design intent (AD-003). Aligned to it (aligned to locked GDD 2026-11-17). Where the GDD differs from the user decisions, the decisions win (see "GDD differences").
**Depends on:**
- A5 Features 1–2 (fields, parameters, `WaterTotals`, `Passability`, `SetParam`).
- A5 Features 3–4 (`HydrologyModule`, `HydrologySnapshot`, `HydrologyUpdated{week}`, the parameter and preset latch path). Referred to by name only; their specs are being written in parallel.
- A5 Feature 6 (preset files and `cow.hydro.preset`'s loader).
- World Step's `HistoryDriverModule` (S-3.5 Feature 1).
- Built Dia: DiaCellLayers (`CellLayerRegistry`, `CellOverlayDesc`, `Details()`), DiaCellHistory (`CellHistoryRecorder`, events, aggregates), DiaValueSchema, `SimQueryHostModule` with `SimHelpInfo`, `DiaDebugDomainRegistry`/`DiaDebugPanel`, DiaObservation.

**Consumed by:** S-2.6 Rivers & Floods (hosts everything here; not specced), S-3 and S-3.5 (host the layers), T-1 (reads the history), V-1 (reads nothing from here).

## Summary

Everything a developer uses to look at and poke the water: eight cell layers, an inundation overlay, per-cell detail rows, a panel card, **six** sim-hosted `cow.hydro.*` names, history events for floods, dry rivers, lake records and freeze-ups, a weekly totals record, metrics and health. It reads committed state only. It changes state only through the queue Feature 3 provides.

It mirrors A4 Weather Debug (Feature 5 there). The panel's *layout* is S-2.6's mockup. This feature defines the data the card shows and the commands it posts.

## Goals

1. Each of A5's eight displayed fields is a base cell layer with a schema label, unit, ramp and range, re-read only when a step commits.
2. Inundation is visible as a mask of the cells under basin water, with each basin's fullness available on demand.
3. A cell inspector answers "why is this cell like this?" from committed values and static tables.
4. History records the water balance week by week and the four layers most worth scrubbing per cell. The other layers are opt-in.
5. Floods, dry rivers, lake records and freeze-ups are findable as debug queries and as history events. They are not gameplay events (no D4 yet).
6. Every action the panel offers is a registered command. The panel posts the same commands.
7. The command set takes **6 names** of the per-PU sim-hosted budget (192 in code), registered all-or-nothing, each with mandatory help.
8. A5 is observable in the five DiaObservation pillars: profile scopes, a trace zone, three metrics, a health reporter and a log channel.
9. Nothing here adds state to the A5 save format.

## Non-Goals

- The stage, the S-2.6 card layout, click-to-inspect and the success-criteria walkthrough (S-2.6, not specced).
- DiaCellHistory's in-game panel, `step_back`, snapshots and the cell inspector (DiaCellLayers and DiaCellHistory commands; T-1 Feature 7 reads the history).
- Calling `CellHistoryRecorder::Sample`. World Step's `HistoryDriverModule` samples once per applied world step. A5 only registers layers and emits events.
- The preset files and their application (Feature 6). This feature exposes `cow.hydro.preset` and forwards to Feature 6's loader.
- Hydrology mechanics, parameters and save/load (Features 1–4).
- Narrative events (`FloodBegan`, `RiverRunsDry`, `LakeLevelRecord`, `FreezeUp` as D4 events). They are debug queries and history events only (parent Non-Goal).
- Performance gates. Step time is reported, not gated (parent decision 7).

## Design

### Placement

`Cluiche/CoW/Modules/HydrologyDebug/`: `HydrologyLayerRegistration`, `HydrologyInundationOverlay`, `HydrologyCommands`, `HydrologyCellDetails`, `HydrologyHistoryEvents`, `HydrologyObservation`. Pure parts in `Cluiche/CoW/Shared/Hydrology/Debug/`: `HydrologyCommand` (typed model, parser, validation), `FloodEpisodeDetector`, `DryRiverDetector`, `LakeRecordTracker`, `FreezeUpDetector`, `ExplainHydroCell`, `HydroFindQueries`, `WeekTotalsRecord`. `HydrologyModule` owns them.

Layers, commands, metrics and health are compiled in **all** configurations (S-3.5's headless regression needs them in Release). The `IDebugDomain`, the panel card and the overlay draw callback are `DIA_DEBUG` only.

### Cell layers

Registered on SimPU through `CellLayerRegistry::Register(CellLayerDesc)` after `WorldGenerationComplete`, against the shared `CellGridDesc` (S-1's `WorldHexGrid`). A result other than `Ok` logs an Error and sets Degraded.

| Layer id | Source field | Kind | Range / values |
|---|---|---|---|
| `hydro.soil` | `soilMoisture` | float | 0 to `SoilMax` (cell-depth; TBD, see Open Question 1) |
| `hydro.ground` | `groundWater` | float | 0 to `GroundMax` (TBD) |
| `hydro.surface` | `surfaceWater` | float | 0 to `SurfaceMax` (TBD) |
| `hydro.river_flow` | `riverFlow` (river cells; 0 elsewhere) | float | 0 to `FlowMax` (TBD), log-style ramp |
| `hydro.flooded` | `flooded` | float 0/1 (converted at commit) | 0–1 |
| `hydro.ice` | `ice` | float | 0–1 (TBD: `ice` is unbounded in Feature 2; the layer clamps for display) |
| `hydro.glacier` | `glacier` | float | 0 to `GlacierMax` (TBD) |
| `hydro.passability` | `passability` | **category** | `Dry`, `Wet`, `Fordable`, `Deep`, `Frozen` (the `Passability` enum, in order) |

- Each is a **base layer** with a pull callback `read(float* dest, uint32_t count)` that copies the committed array. The two `uint8_t` fields are converted to float at `Commit` into module-owned buffers (about 53 KB each at Large), so the callback never converts on the render path.
- `version()` is the commit counter, so DiaCellLayers re-reads only after a commit. `count` must equal the active cell count. A mismatch writes zeros and logs once.
- Before the first world is ready the callbacks report zero cells and nothing is drawn.
- Labels, units, ramps, `min`/`max`, enum labels, swatch colours and hotkeys come from DiaValueSchema entries `hydro/<key>` (the schema file ships with this feature). The passability swatches follow the GDD draft (grey, tan, yellow, navy, white). Float layers need `min`/`max` or history tracking is refused. A hotkey clash logs a Warning and the later layer gets none. The key table is chosen in the first plan task so that it does not clash with A4's.
- Layers unregister in `DoStop`. Re-entry re-registers.

### Inundation overlay (`CellOverlayDesc`, `DIA_DEBUG`)

User decision: "inundation shown via the basin mask". There is no `hydro.inundated` layer. The overlay draws a translucent fill on every cell whose `openWater` is above 0 (that is, bed below the basin level) plus an outline round each basin's footprint. It reads `HydrologySnapshot` (`OpenWater(cell)`, per-basin `Level`, `Volume`). The overlay is recomputed once per commit. Primitives are capped at a configured `max_primitives` (default **4,000**, TBD; `.diaapp` config `hydro_overlay`, validated). Over the cap, inundated cells are drawn as a coarser mask and a Warning is logged once per world. Fullness per basin (percent of spill volume) is available through `cow.hydro.basins`, not drawn.

### Per-cell detail rows

Registered with `CellLayerRegistry::Details()` (64 rows max, called only when exactly one cell is selected) plus a hover extra. `ExplainHydroCell(snapshot, terrain, params, cell)` (pure) returns:

- **Committed fields:** the eight layer values plus `meanFlow`, `openWater`, `glacierEligible` and `RiverState` (Dry / Low / Normal / Flood / Frozen, from Feature 3's snapshot).
- **Latched counters:** `inundatedWeeks`, `exposedWeeks`, `lowFlowWeeks`, `glacierWeeks`, `glacierFreeWeeks`, `snowYears`.
- **Static:** biome and its three biome-table values (`soil_capacity`, `infil_rate`, `veg_factor`) for this cell, `isRiver`, `isCrossing`, flow direction and the downstream cell, the release rate `k` for this cell (Feature 2's rule: a river cell is `clamp(release_min + release_slope · drop, release_floor, release_ceil)`, any other land cell is `hillslope_release`), bed elevation, elevation.
- **Basin (if any):** basin id, level, volume, percent of spill volume, spill cell, depth at this cell (`level − bed`), `iceFraction`, `isLake`.
- **Derived:** the flood threshold (`bankfull_factor · meanFlow`) and the ratio of this week's `riverFlow` to it; the passability reason ("Deep: depth 0.07 > ford_depth", "Fordable: crossing cap").

It calls Feature 2's pure helpers (release rate, basin curve, passability rule) rather than re-deriving them. It adds no behaviour to Feature 2.

The GDD draft's **per-pass deltas** (infiltration, ET, runoff, baseflow, inflow, out for the cell this week) are not shown: Feature 2 keeps no per-cell per-pass records (see GDD differences).

### History

History is **sampled by World Step's `HistoryDriverModule`** (once per applied world step). A5 does not call `Sample`.

**Aggregates (free).** DiaCellHistory records min, mean and max per layer per group for every layer registered with `CellLayerRegistry` (the aggregate tier, no tracking needed). So the eight `hydro.*` layers get world and per-region aggregates automatically. **Per-region soil means** are the `hydro.soil` aggregate over S-1's region groups. That needs S-1's region layer to carry `history.group: true` (the same check as S-2.5 Feature 5; flagged there, not assumed here).

**Weekly water totals.** DiaCellHistory has no scalar series. The totals are recorded as one history event per applied week, `hydro.totals`, emitted from `Commit` (so the driver's next sample commits it with the week). The payload is compact JSON under 1 KB:

- the storage of each store: surface, soil, ground, river store, ice, glacier, basins (sums of `WaterTotals.storage` split by store; the split comes from Feature 2's totals, see Open Question 2);
- the named flows: `in`, `seaOut`, `etOut`, `drawOut`, `drawUnmet`, `basinEvapOut`, `fixedLakeMakeup`;
- `error` (balance error), `floodedCells`, `deepCells`, `glacierCells`, `largestTreeNodes`.

T-1 reads the series from the event stream. A scalar-series capability in DiaCellHistory would be a better fit but is a Dia change (AD-001, not proposed here).

**Per-cell tracking defaults** ("totals + key layers", user decision): `hydro.river_flow`, `hydro.soil`, `hydro.flooded`, `hydro.glacier`. The module exposes the list as `kDefaultTrackedHydroLayers`. The testbed (S-2.6) posts `cells.history.track` with it at stage start. Every other layer is opt-in through the layer picker (the existing `cells.history.track` command).

**Events** (`CellHistoryRecorder::Emit(HistoryEvent{type, group, cell, payloadJson, text})`) from `Commit`, committed by the driver's next sample. State for detection lives on SimPU in the module and is **not saved**. After a load or a world change, detectors restart and the first week re-announces what is happening (the same rule as A4's storm ids). Thresholds are config (`.diaapp` `hydro_events`), validated, and **provisional**.

| Type | When | Payload |
|---|---|---|
| `hydro.flood_start` | A connected set of flooded cells (neighbour-connected) appears with no cell in common with last week's flooded set | `id`, `size`, `anchor` (cell with the highest `riverFlow / (bankfull · meanFlow)`), `peakRatio` |
| `hydro.flood_end` | A flood set from `flood_start` has no flooded cell left this week | `id`, `weeks`, `peakSize` |
| `hydro.river_dry` | A connected set of at least `dry_min_cells` river cells (TBD) with `riverFlow < dry_fraction · meanFlow` (`river.low_flow_fraction`, 0.1 in Feature 1) and `meanFlow > flood.min_flow` appears, once per episode; an episode ends after one non-dry week | `id`, `size`, `anchor`, `region` |
| `hydro.lake_record` | A basin's level exceeds its highest level since the world loaded, or falls below its lowest, by at least `record_margin` (TBD). The first `record_warmup_weeks` (TBD) after load or spin-up record only, no event | `basin`, `level`, `kind` (`high`/`low`), `previous` |
| `hydro.freeze_up` | In a cold spell (`freeze_up_fraction` of river cells, TBD, hold `ice ≥ freeze.ice_crossing_min`), the first week the fraction is crossed upward; re-armed when it falls back below half | `riverFraction`, `week` |
| `hydro.totals` | Every applied week | as above |

- Floods, dry rivers and records are capped at **4 events per type per week**, largest first. Excess goes to the counter `cow.hydro.events_dropped`.
- Each type has a `kind: "event"` DiaValueSchema entry with its payload fields, or the recorder logs a warning per type in Debug.

### Commands (6 sim-hosted names)

All registered through `SimQueryHostModule` with mandatory `SimHelpInfo`: non-empty description ≤ 256 chars, category `hydrology`, owner `HydrologyModule`, arguments as `ValueParam`s ≤ 16. **Arguments are JSON objects.** A bad or unknown argument, a missing required field, an out-of-range value or an unknown cell index returns an error naming the field and **queues nothing**. Replies stay under the sim-hosted payload limit (`kSimPayloadBytes` 16,384; replies are capped lower, as DiaCellHistory's queries are, with a `next` cursor where a list can be long).

| Name | Kind | Arguments and reply |
|---|---|---|
| `cow.hydro.state` | query | `cell` (optional index or `{q, r}`). Without a cell: the committed week, the totals above, the balance error, parameter source, input source (A4 or fixed-input), `basinsFromLakesOnly`, counts. With a cell: the inspector dump (the detail rows as JSON) |
| `cow.hydro.basins` | query | `id` (optional). Without: every basin with level, volume, percent of spill volume, spill cell, `isLake`, inundated cell count (paged). With: one basin in detail, including its capacity-curve summary |
| `cow.hydro.find` | query | `kind`: `floods` (current flooded sets: size, anchor, peak ratio) / `dry_rivers` (current dry sets) / `records` (basin level records since load: high and low per basin) / `deep` (cells with `Passability::Deep`, with their basin and depth). Optional `limit` (1–200, default 50) |
| `cow.hydro.set_param` | command | `key` (a dotted scalar key, or `biome_table.<BiomeId>.<column>`), `value`. Calls Feature 1's `SetParam` check, then queues the change through Feature 3's mutation path. Latched at the next `Capture` |
| `cow.hydro.preset` | command | `name`, `mode` (`fixed` default or `coupled`; see Feature 6). Loads and validates the preset (Feature 6), then queues it. An unknown name lists the available names |
| `cow.hydro.balance` | query | `weeks` (optional, 1–52). Without: the live balance record (last error, worst error since load, cumulative drift, weeks over tolerance). With: runs that many weeks headless on a **copy** of the committed state under the fixed-input driver (constant at the last applied week's mean input) and reports the worst error. See Open Question 3 |

- All mutations are **queued and latched at the next `Capture`**. The reply says so and names the week they will first affect when known.
- A **typed command model** (`HydrologyCommand`, parsed once) is the single execution path. `IHydrologyCommandSink` has two implementations: the queued sink (module, live) and a direct sink bound to a `HydrologySimulation&` (the headless runner and S-2.6's scenario setup, which apply immediately between steps). The panel posts the same JSON commands through the console path. It never calls the module.
- **Registration is all-or-nothing.** If registering any of the six fails, the ones already registered in that call are removed and the module reports Degraded with the failing name. Names unregister in `DoStop`.
- The GDD draft's `cow.hydro.reload` and `cow.hydro.set` are not shipped (see GDD differences). Reload goes through Feature 3's parameter path if it wants a command later.

### Panel card (`DIA_DEBUG`)

`HydrologyDebugDomain : IDebugDomain`, registered with `VisualDebuggerModule::RegisterDomain`. `GetDescription()` ≤ 80 characters. The accent comes from `DebugGroupAccents`. The card **shows** the week, season, cell and basin counts, the water totals (soil, ground, channels, basins, ice, glacier; in, to sea, evaporation), the **balance error gauge** (green inside tolerance, red outside), the last step's CPU ms, the largest river tree's node count, and the input source. It **toggles** the inundation overlay and **posts commands only**. SimPU-owned values reach the card the way A4's do: a stats struct published through the existing debug stats path, not a cross-thread read (PU-boundary rule).

### Instrumentation

- **Profile:** `DIA_PROFILE_SCOPE` per pass is Feature 2's (and backs the card's pass list).
- **Trace:** `DIA_TRACE_ZONE` on the step in `HydrologyStage::Run`/`Commit`, on each command handler, on event detection and on the overlay plan.
- **Metrics** (`MetricInfo` on each; written **once per applied step on SimPU**, SD-PERF-7):
  - `cow.hydro.step_ms` (gauge): total CPU of the last step, required by S-3.5 R5.
  - `cow.hydro.balance_error` (gauge): `|WaterTotals.error|`.
  - `cow.hydro.largest_tree_nodes` (gauge): `WaterTotals.largestTreeNodes`.
  - `cow.hydro.events_dropped` (counter): events beyond the per-type cap.
- **Health** (`IHealthReporter`, adds to Feature 3's):
  - Degraded when `|error| > 1e-4 · max(in, out, 1)` (Feature 2's weekly tolerance), clearing after a week inside it.
  - **Failing** when any total is NaN or infinite (storage and error carry any NaN from the fields).
  - Degraded if layer, overlay or command registration fails.
- **Logs** (`HYD` channel):
  - Info on init, spin-up done, layers and commands registered, preset loaded.
  - Warning on a balance breach (each breach week, rate-limited to one per simulated year with a count), on unmet draws (`drawUnmet > 0`, same rate limit), on a hotkey clash, a rejected command, an overlay truncated by `max_primitives` (once per world), a history layer not tracked.
  - Error on a registration failure or a non-finite total.

## Public API

As Placement, in `CoW::Hydrology`. No STL in public APIs: plans, commands and detector results use Dia containers and fixed-capacity arrays, and strings are `String64`/`String256`.

```cpp
namespace CoW::Hydrology {
    extern const Dia::Core::StringCRC kDefaultTrackedHydroLayers[4];   // river_flow, soil, flooded, glacier

    struct HydroEventConfig;                    // thresholds, validated, provisional (see History)
    class  FloodEpisodeDetector;                // update(snapshot) -> events (pure, caps applied by caller)
    class  DryRiverDetector;
    class  LakeRecordTracker;
    class  FreezeUpDetector;

    HydroCellExplanation ExplainHydroCell(const HydrologySnapshot&, const HydrologyTerrain&,
                                          const HydrologyParameters&, uint32_t cell);
}
```

New messages: none (a stats struct reaches the card through the existing debug stats path). Edited: `HydrologyModule` (owns the debug parts), `HydrologyStage::Commit` (event detection and `hydro.totals`). Feature 3's queue gains nothing here: `set_param` and `preset` use the entries Feature 3 defines for parameter changes and preset application (names TBD in Feature 3).

## Acceptance Criteria

### Layers
1. All eight layers register with `Ok` after the first world is ready, each with schema `min`/`max` (float layers) or an enum (passability) and a label. A duplicate or capacity result logs an Error and sets Degraded.
2. `read` returns exactly the committed array (passability and flooded as the float conversion of the `uint8_t` values). `version()` changes on `Commit` and not otherwise. A `count` mismatch writes zeros and logs once.
3. Before a world is ready the callbacks report zero cells and nothing is drawn. After stage re-entry the layers are registered again and none is duplicated.
4. `hydro.passability` is a category layer whose enum order equals the `Passability` enum, with five labelled swatches.

### Inundation overlay and details
5. The overlay covers exactly the cells with `openWater > 0` on a hand-built terrain with a known basin, plus the basin outline. It is recomputed only when the version changes.
6. The overlay never exceeds `max_primitives`. Over the cap it degrades to the coarser mask and logs once per world. Invalid overlay config is rejected with the key named and defaults are used.
7. `ExplainHydroCell` matches Feature 2's helpers for the same cell on a hand-built terrain with known answers: release rate, basin depth, percent of spill volume, flood threshold ratio, passability reason (including the crossing cap).
8. Detail rows appear only for a single selected cell and stay within 64 rows. A cell outside any basin has no basin rows.

### History
9. The module registers no history layers of its own beyond the eight base layers. After a run with the driver sampling, each of the eight layers has a world aggregate.
10. `kDefaultTrackedHydroLayers` is exactly `river_flow`, `soil`, `flooded`, `glacier`. Tracking them with `cells.history.track` succeeds, and the other four layers are not tracked until asked.
11. One `hydro.totals` event is emitted per applied week, under 1 KB, and its stores sum to `WaterTotals.storage` and its flows equal the totals to within float tolerance.
12. `FloodEpisodeDetector`: a planted flooded set emits one `hydro.flood_start` with the right size and anchor. A persisting flood does not re-fire. Its end emits one `hydro.flood_end` with the episode length. A flood that merges with another keeps one id (the older).
13. `DryRiverDetector`: a river reach below `dry_fraction · meanFlow` for at least `dry_min_cells` connected cells emits one `hydro.river_dry` per episode, and again only after a non-dry week. A reach with `meanFlow ≤ min_flood_flow` never counts as dry.
14. `LakeRecordTracker`: a new high or low beyond `record_margin` emits `hydro.lake_record` once. Nothing is emitted during `record_warmup_weeks`. The records reset on world change and on load.
15. `FreezeUpDetector` fires once when the river-ice fraction crosses `freeze_up_fraction` upward, and re-arms only after it falls below half.
16. At most 4 events per type per week are emitted, largest first. The excess is counted in `cow.hydro.events_dropped`.
17. Each of the six event types has a `kind: "event"` schema entry. Event payloads are under 1 KB.

### Commands
18. Exactly **6** names register, each with non-empty help (≤ 256 chars, category `hydrology`). A forced failure of the Nth registration leaves **no** names behind and reports Degraded.
19. For `set_param` and `preset`: a valid request queues the expected change, takes effect at the next `Capture` and not before, and the reply says so.
20. Each of these returns an error naming the field and queues nothing: unknown argument, missing required field, `cell` out of range, unknown `kind`, `limit` outside 1–200, `weeks` outside 1–52, unknown preset (the reply lists names), unknown parameter key or biome id or column, an out-of-range parameter value, an unknown basin id.
21. `set_param` accepts every scalar key and every `biome_table.<BiomeId>.<column>` that Feature 1's `SetParam` accepts, and rejects what it rejects.
22. `cow.hydro.state` without a cell returns the documented totals keys. With a cell it returns the same rows as the detail panel.
23. `cow.hydro.basins` lists every basin on a hand-built terrain, pages when over the reply cap, and returns one basin in detail for a valid id.
24. `cow.hydro.find` returns: `floods` for a planted flood, `dry_rivers` for a planted dry reach, `records` after a planted level change, `deep` for planted deep cells with depth. `limit` truncates and says so.
25. `cow.hydro.balance` without `weeks` returns the live record. With `weeks` it runs on a copy: the committed state is byte-identical afterwards, and the worst error is below Feature 2's tolerance on a hand-built grid.
26. The direct sink and the queued sink produce **identical** simulation state after the same command list and the same steps (the live-vs-headless equivalence, tested once).
27. A full mutation queue (Feature 3) turns a command into a clear error, not a drop.

### Card and instrumentation
28. The card's strings come from the stats struct, not a cross-thread read of module state. Its description is ≤ 80 characters and its accent is a `DebugGroupAccents` constant.
29. The four metrics register with `MetricInfo` and are written only on SimPU, once per applied step. `step_ms` equals the last step's total CPU. `balance_error` equals `|error|` and `largest_tree_nodes` equals the total's field.
30. Health: a week with `|error|` over tolerance reports Degraded. A NaN total reports Failing. The next clean week clears Degraded. A registration failure reports Degraded.
31. The `HYD` Warning for a balance breach or unmet draws is emitted at most once per simulated year, with a count of the suppressed ones.
32. In a Release build the layers, commands, metrics, health and events exist. The domain, card and overlay draw do not.

## Files Touched

| File | Change |
|---|---|
| `Cluiche/CoW/Modules/HydrologyDebug/*` (the six files in Placement) | New |
| `Cluiche/CoW/Shared/Hydrology/Debug/*` (the pure parts in Placement) | New |
| `Cluiche/Assets/CoW/Global/Hydrology/hydrology_schema.json` | New: layer labels, units, ramps, ranges, hotkeys, passability enum, event types |
| `Cluiche/Tests/GoogleTests/CoW/Hydrology/TestHydrologyLayers.cpp`, `TestHydrologyCommands.cpp`, `TestHydrologyDetectors.cpp`, `TestHydrologyCellDetails.cpp`, `TestHydrologyObservation.cpp` | New |
| `HydrologyModule`, `HydrologyStage` | Edited (own the debug parts; event detection at `Commit`) |
| `CoW.vcxproj`, `GoogleTests.vcxproj` (+ filters) | Via `dia docs vcxproj-add` |
| CoW `pipeline.toml` | Deploys the schema file with `Global/Hydrology/` |

**Spec edits on approval:** the system spec's debug section (6 names, `hydro.totals` as an event, the inundation overlay, default tracked layers). No Dia files are added or edited, so there is no AD-001 sign-off. **Flagged:** a scalar-series store in DiaCellHistory would be a better home for the weekly totals than an event. That is a Dia change and is not assumed here (Open Question 2).

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL in public APIs | Detectors, plans, commands and explanations use Dia containers and fixed-capacity arrays |
| AD-001 | Ask permission before Dia edits | None. The DiaCellLayers, DiaCellHistory, DiaValueSchema, sim-hosted and debug-domain APIs are built and used as-is |
| AD-003 | GDD owns design intent | The layers, commands and history follow the locked GDD (2026-11-17). Event thresholds, display ranges and hotkeys are provisional and listed for fold-back |
| SD-PERF-7 | Telemetry cost budgets | Four metrics, written once per applied step on SimPU. No per-cell telemetry leaves SimPU. The overlay is capped |
| Sim-hosted name cap (192/PU in code) | Per-PU budget | 6 names, registered all-or-nothing |
| Editor-actions rule | Every tool action is a command; the panel calls the same commands | The panel posts JSON commands only. AC 26 proves one execution path |
| PU boundaries are streams | No mutex-guarded cross-PU reads | The card reads a published stats struct (AC 28) |
| DFS-002 | `Run` reads only the captured copy | Nothing here touches the working copy. Events and layers read committed state at `Commit` on SimPU |
| Parent Non-Goal (narrative events) | No D4 events yet | The five event types are history events and `find` queries only. No D4 message is published |

## GDD differences

Only real remaining differences (aligned to locked GDD 2026-11-17). The user decisions of 2026-11-17 win:

| GDD | This spec |
|---|---|
| Layers `hydro.river_state` (Dry/Low/Normal/Flood), `hydro.basins` (footprints, % full), `hydro.flow_dir` (arrows) | Not shipped. Eight layers only. Inundation is an overlay mask. Basin fullness is in `cow.hydro.basins`. `RiverState` (the GDD's `river_state`, including `Frozen`) is in Feature 3's snapshot and the cell detail rows, but has no layer. Flow-direction arrows deferred |
| Cell inspector shows this week's per-pass deltas | Not shown. Feature 2 keeps no per-cell per-pass records. Adding them costs about six float arrays and a pass-by-pass write. Revisit if "where did this water go?" is needed in S-2.6 |
| Panel shows "water totals" as a gauge only | Also recorded each week as a `hydro.totals` event, so T-1 can chart it |
| `cow.hydro.reload` and `cow.hydro.set` (dev store override) | Not shipped. `set_param` covers temporary parameter overrides. A store override (EC-27) would need an external-source ledger entry in Feature 2's totals and is not decided |
| `cow.hydro.find floods\|dry\|records\|frozen\|glaciers` | `floods`, `dry_rivers`, `records`, `deep`. `frozen` and `glaciers` are read from the `hydro.ice` and `hydro.glacier` layers and the totals' counts. `deep` is new (user decision) |
| `cow.hydro.set_param` as a temporary override cleared by reload | Routes through Feature 3's mutation path like A4's `params set`. Whether it survives a reload is Feature 3's call |
| `river_flow` shown on a log scale, width by flow | A schema ramp with a log-style spacing. Width-by-flow is V-1's (player view) and not a debug layer |

## Test Plan

TDD per the project rule. Pure parts first and heavily, each with hand-built terrain and known answers. TDD order: command model, detectors, `ExplainHydroCell`, event config validation, `WeekTotalsRecord`, then the module wiring against a fake `CellLayerRegistry`, a fake `SimQueryHostModule` (including the forced registration failure), a fake `CellHistoryRecorder` and fake metric and health sinks, then the one sink-equivalence test.

Scoped `--filter="*Hydrology*"` while iterating. One full `--shards 2` run at the end. `SLOW_` tests are skipped by default (none in this feature). **No mutation checks.** Layer goldens are Feature 6's, not here. The gap audit and edge pass is the last plan task, and the Observation scan runs at verify. The panel layout and a screenshot taste check are S-2.6's.

## Open Design Questions

1. **What are the display ranges?** `soilMoisture` is documented as [0, 1] in the system spec, but Feature 1's `soil_capacity` goes up to 5 and the GDD defaults reach 1.0. `riverFlow`, `glacier` and `ice` are unbounded sums. A fixed range clips big rivers and glaciers. A per-world observed range (DiaCellLayers also shows the observed min/max) is honest but moves the colours week to week. Pick in the first plan task: schema range = the largest provisional capacity, and let the legend show the observed max. Revisit after S-2.6 tunes the numbers.
2. **Is an event the right home for the weekly totals?** It works with no Dia change and T-1 can read it, but events are not a numeric series: charting means parsing JSON payloads, and the recorder's event budget must hold 52 extra events a year per world. If T-1 wants totals as first-class series, a small scalar-series API in DiaCellHistory (AD-001, needs sign-off) is cleaner. Decide when T-1's water panel is specced. Related: the per-store split of `storage` needs Feature 2's `WaterTotals` to expose it (today it has one `storage` double). Feature 2 would add a per-store array to its totals.
3. **How does `cow.hydro.balance weeks=N` run without hitching SimPU?** A headless copy stepped N times on SimPU costs N full steps at once, which could miss the World Step slack at Large. Options: cap `weeks` low (the 1–52 here is a placeholder), run it as a DiaDeferredStep job and reply when done (needs the sim-hosted reply to be asynchronous, which this spec has not confirmed), or run it in the headless runner only (not live). The first plan task checks what `SimQueryHost` allows.
4. **Are the event thresholds (`river.low_flow_fraction`, `dry_min_cells`, `record_margin`, `freeze_up_fraction`) meaningful before S-2.6 tunes the water numbers?** They are config so tuning needs no code, but a bad first guess floods the stream (capped at 4 per type per week) or stays silent. Treat the first S-2.6 session as their tuning pass.

## Status

**Status:** Approved (2026-11-17, user, after the locked-GDD alignment). Written from the user's decisions of the same day:
- eight DiaCellLayers base layers with inundation shown through the basin mask;
- DiaCellHistory defaults "totals + key layers" (weekly totals and per-region soil means; per-cell `river_flow`, `soil`, `flooded`, `glacier`; five event types; other layers opt-in), sampled by World Step's `HistoryDriverModule`;
- six consolidated sim-hosted commands with mandatory help (`state`, `basins`, `find`, `set_param`, `preset`, `balance`);
- observability: profile per pass, trace zone, `cow.hydro.step_ms`, balance and tree gauges, health Degraded/Failing, `HYD` logs;
- performance report-only.

Aligned to the locked GDD on 2026-11-17. Approval is a separate step.

Plan: [hydrology-debug.plan.md](hydrology-debug.plan.md) (written 2026-11-17, not started).

# Plan: S-1 World Generation panel (system plan task 9)

**Spec:** @f1-debug-host-integration.md
**System plan:** @worldgenerationpipeline.plan.md (task 9)
**Mockup:** @s1-editor-mockup.html (approved 2026-11-09, task 8c)
**Status:** Done

Scope: the terrain part of Feature 6: `WorldGenDebugDomain` and its panel on top of the task 8b `cow.worldgen.*` commands. Seed, `WorldShapeParams` sliders, presets load/save, map size, stage timings, View section (colour mode and drawer toggles as state only; rendering is task 10), Cell Inspector with stage values. Validation, Cancel, Export/Import are stubs (tasks 13-16). Biome/region/site fields are stubs (tasks 11-12).

## Decisions (2026-11-09)

- **Cross-PU route.** The panel is read on Main, the world lives on SimPU, so panel state crosses by streams only (no cross-PU module read, as `GameSpeedDebugDomain` does). The domain itself runs on SimPU; Main registers a `ViewerStateAdapter` (Dia, generic) fed by a snapshot FrameStream.
- **Shared bridge/relay in CluicheGameBaseline, not Dia and not copied.** `DomainStateBridgeModule` (Main) and `DomainStateRelayModule` (Sim) generalise CluicheTest's `CellLayersStateBridgeModule` / `CellLayersStateRelayModule`: `Attach(channelName, domain)` / `Detach(channelName)` over a fixed pool of 4 named channels (streams `<name>State`, `<name>Demand`, `<name>Channel`). CluicheTest's two modules are replaced by them (AD-001 sign-off for that CluicheTest edit given 2026-11-09). Dia is unchanged: it already has `ViewerStateAdapter` / `ViewerStateSnapshotPublisher`.
- **Widgets are CoW-only.** New widgets go in CoW's `Cluiche/Assets/CoW/Global/Presentation/UI/debug-panel.html`. Copying them to CluicheTest's panel needs separate sign-off.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `DomainStateBridgeModule` + `DomainStateRelayModule` in CluicheGameBaseline: 4 named channels, Attach/Detach, same behaviour as the CellLayers pair (demand-armed publish, one demand event per Main tick, health registration). Module docs, vcxproj, DIA_MODULE | TDD: attach two domains on distinct channels, snapshots do not cross; unknown/over-pool channel returns nullptr; Detach stops publishing | Done | sonnet | Mirror the CellLayers modules; do not change them yet | Done 2026-11-09: 6/6 DomainStateModules tests pass; Attach(domain) takes first free of 4 slots (no names). No module .md exists in CluicheGameBaseline, so none added. |
| 2 | Migrate CluicheTest CellLayers viewer + history onto the new modules; delete `CellLayersStateBridgeModule` / `CellLayersStateRelayModule`; update stage manifests, vcxproj, docs | Existing celllayers GoogleTests and the celllayers E2E pass unchanged | Done | sonnet | Mechanical but wide. No behaviour change allowed | 2026-11-09: CellLayers gtests 51+52 pass, celllayers E2E 11/11; registeredtypes.diaschema regenerated, version 1.42 to 2.0 (type removal is breaking). History domain now health-registered. |
| 3 | `WorldGenDebugDomain` (CoW, SimPU, DIA_DEBUG): `GetJSONState` from `WorldGenSession` (controls built from the `params` ranges, status, stage timings, cell inspector, presets, size); `OnCommand` routes to the same handlers the `cow.worldgen.*` commands use | TDD: JSON state matches `params`/`status`/`cell` query results; each command changes the session as the DiaAPI command does; unknown key ignored | Done | sonnet | SD-S1-002: no second path. Find how to call the sim-hosted handlers from the domain without duplicating logic | 2026-11-09: 13 WorldGenDebugDomain tests pass. barrierForm control is type select (task 4 must render it). Per-stage timings exist (GetTimings). |
| 4 | `debug-panel.html` (CoW copy): slider throttled send on `input`, plus button, select, text/number input, checkbox, segmented buttons, collapsible section, key/value grid, stage-timing bars, legend swatches. Built to the mockup | Vitest or the panel's existing test harness if present; else a mockup-vs-panel manual check recorded in notes | Done | sonnet | CoW file only | 2026-11-09: 46 new Vitest tests, debugpanel suite 557/557. sendCommand caps args at 63 chars (String64): presetSave with long id/name is refused client-side; fixing needs a Dia change. No legend swatches (contract has no legend data). |
| 5 | Wire into the S-1 stage: add bridge/relay to the stage manifest, register the domain via the relay in `S1WorldGenerationTestbedStageModule`, unregister on stop | Stage boots; `debug.domain_state` returns the World Generation card | Done | sonnet | Check `dia validate manifest` | 2026-11-09: card live on the real S-1 stage (cow/default E2E 1/1, gtests 94+93). CoW.vcxproj now links DiaCellLayers (WHOLEARCHIVE, as CluicheTest). Panel-command forwarding is covered by gtest only (no debug command reaches the panel event stream). Teardown/Escape-to-Boot not exercised by a test. |
| 6 | Verify: slider drag → preview update end to end; gap audit, edge tests; one scoped gtest run (shards >= 2); `dia run cluichetest` celllayers E2E; observation scan | Feature 6 ACs for the terrain sections (AC1, AC2, AC8-label parts, AC9, AC11, AC14) | Done | sonnet | Final verification task per cadence rule |

## State JSON contract

Produced by `CoW::WorldGenDebugDomain::GetJSONState` (task 3; runs on SimPU, relayed to Main). The panel (task 4) consumes exactly this. Compact size must stay under 12 KB at Large (32 KB snapshot slot).

- `drawers`: `[]` (no world drawers)
- `stats`: `{status, revision, seed, size, width, height, locked(false), lastMs, preset ("" if none), presetModified, stagesRan[names]}`
- `controls`: one per WorldShapeParams key, ordered by first stage: `{key,label,type:"slider",min,max,step,value,default,stage,stageName}`. `step` is 1 for integer params, 0.01 for floats. `barrierForm` is `{type:"select", values[], value:string, default:string}` (no min/max/step).
- `presets`: `[{id,displayName}]`
- `sizes`: `[{name,width,height,selected}]`
- `timings`: `[{stage,name,ms,ran}]` per-stage ms of the last generation (generator records these); `ran` = stage re-ran in the last update
- `view`: `{colourMode, modes[], drawers:[{name,enabled,available}]}`; modes = `"terrain"` then every terrain layer name; drawers = Tiles, Grid lines, Selection (available), Region edges, Sites, Evidence (available=false)
- `cell`: `null`, or `session.Cell({x,y})` (all fields, `layers{}`, `neighbours[]`) plus `selected:{x,y}`. Selection out of range or before first world => `cell:null` plus `cellError` string.
- `lastSave`: `null`, or last presetSave result (`{success,writtenTo,isSource}` / `{success:false,error,status?}`)

Commands: `setScale{key,value}`, `regenerate{seed?}`, `setSeed{seed}`, `setSize{size}`, `presetLoad{id}`, `presetSave{id,displayName,description?,overwrite?}`, `selectCell{x,y}`, `clearCell`, `setColourMode{mode}`, `toggle{drawer}`. (Selection to move to DiaCellLayers `cells.select.cell` in task 10.)

## Known gaps (task 4, 2026-11-09)

- **Missing pieces:** there are no legend swatches, because the JSON state carries no legend data. Cancel, Export and Import are not in the terrain slice (tasks 13-16).
  - Legend: add a `view.legend` (`[{label, colour}]` per colour mode) to the state contract when task 10 defines the CellField colour ramps, then render swatches in the View section.
- `sendCommand` drops command args longer than 63 characters (String64), so `presetSave` with a long id or display name is refused in the panel. Fixing it needs a Dia change.
- Seeds above 2^53 are rejected in the panel (it sends a JS number); random seeds are below 1e8.

## Follow-up closed (2026-11-09)

- Teardown E2E: S-1 to Boot to S-1 three times, card rebuilt each time, WorldGen domain gone while on Boot, no new ERROR log lines.
- `debug.panel_command {domain, cmd, args?}`: sim-hosted, in `VisualDebuggerModule` (Baseline; AD-001 given 2026-11-09), registered lazily on the first sim tick with a host. Uses the panel's own `EnqueueCommand` path. CoW `cow/default` E2E now 7/7 (setScale, setSize, presetLoad, selectCell/clearCell, bad requests). The panel's JS to event-stream hop is still untested.
- No RED run for the `debug.panel_command` gtests (implementation was written first).

## Task 6 verification (2026-11-09)

Fresh runs: manifest 78/0 errors; scoped gtests 210 per shard; Vitest debugpanel 557/557; CoW and CluicheTest build; `cow/default` E2E 7/7; celllayers E2E 11/11; full `dia run googletest --shards 4` 0 failures (about 14,770 tests). Release and sanitizer runs skipped. 10 edge tests added, no production bug found. Compact state at Large with a selected cell and lastSave: 6,552 bytes (slot 32 KB).

Not covered: panel JS to event-stream hop; AC2 moisture effect through the panel; AC8 and Validate are N/A in this slice; site display and export equality (later tasks).

**Limitation:** after a relay slot is reused, the new adapter can briefly show the previous occupant's last snapshot until the first fresh publish (the slot's FrameStream is not cleared on Attach). Converges within a frame or two. Fix option: publish an empty snapshot on Attach (a Baseline change, not made).

**Observation suggestions (do not block Done):** trace/profile `WorldGenDebugDomain::GetJSONState` and the relay's per-slot publish; metrics for snapshot bytes per slot, attached-slot count, panel-command accepted/rejected, per-command counts; health reporters for `WorldGenDebugDomain` (Failed state) and the panel-command queue-full case; log `debug.panel_command` rejections and snapshot-oversize.

## Perf fix (2026-11-09)

Problem (Debug build only): with the World Generation card open, MainPU ran about 85 ms and SimPU 35 to 65 ms against a 33.3 ms budget.

Causes and fixes (no Dia/ file changed):

1. **Log feedback loop (Main).** `pu.over_budget` warnings fed the panel's log ring, and `PushLogLines` pushed every frame (each `appendLogLines` rebuilt up to 500 DOM rows, about 45 ms), keeping the PU over budget. Fix: `LogPushBatcher` (Baseline) limits pushes to one per 250 ms, newest 200 lines, with one "N older log lines not shown" summary line; `debug-panel.html` appends rows incrementally and does not render a collapsed log view.
2. **Per-frame data flow.** (a) `DebugPanelPageModule` skips the parse and push for a byte-identical snapshot and pushes only changed domains (`DomainRootDelta`; `{"__partial":true, ...}`, merged by `updateDomainState`). (b) `WorldGenDebugDomain` splits state into static and dynamic parts (see the contract note below), caches the static JSON, scans the preset directory at most every 5 s or after `presetSave`, and re-runs `Params()` only when the shape changes (`GetJSONState` 6.5 ms to about 1.3 ms). (c) `VisualDebuggerModule` publishes the panel snapshot every 2nd Sim tick (`kSnapshotTickDivisor`) and `DomainStateRelayModule` publishes every 2nd tick on the other phase (`kPublishTickDivisor`); the panel therefore updates at about 15 Hz (user approved). `CellLayersIntegrationStreams.PublishingStopsAfterTheSixtyTickWindow...` now expects about 30 publishes per 60-tick window.
3. **Race.** `DomainStateChannel::mDemandPending` was documented Main-only, but `ViewerStateAdapter::GetJSONState` (which calls `RequestState`) runs on the Sim thread: the S1 stage registers the adapter with the Sim-side `VisualDebuggerModule` registry and `DomainStateSnapshotPublisher::Update` packs on Sim, while `TakeDemand` runs on Main. Now `std::atomic<bool>` with `exchange`; comment corrected; cross-thread gtest added (no ThreadSanitizer run).

**State JSON contract change.** Every frame carries `full` (bool) and `staticVersion` (int). A FULL frame (first call, `staticVersion` change, every `kFullFrameInterval` = 30 calls) has the complete shape documented above. A LEAN frame omits static fields: `controls[]` are `{key, value}`, `sizes[]` are `{name, selected}`, `timings[]` are `{stage, ms, ran}`, and `presets` and `view.modes` are absent. The page keeps the static part of the last full frame (`wgResolveState` in `debug-panel.html`). Both panel pages (CoW and CluicheTest) got the partial-merge change; the CluicheTest edit was signed off (AD-001) for the partial merge and log append only.

| Debug, 40 s on S1 stage (ms, median / p95) | Main | Sim | over_budget lines |
|---|---|---|---|
| Before | 82-89 / 98-101 | 34-38 / 68-71 | hundreds (bistable 16 vs 600) |
| After Fix 1 (log batching) | 34.1 / 78.3 | 33.9 / 69.9 | 705 |
| After Fix 2 before throttles | 18-22 / 28-34 | 19-25 / 29-38 | Main 19-43, Sim 44-179 |
| Final (two runs) | 18.1 / 26.1 and 10.3 / 23.9 | 19.3 / 26.2 and 17.8 / 23.4 | Main 6 and 0, Sim 22 and 8, Render 2 |
| Stage without card (reference) | 10.3 / 15.5 | 13.4 / 27 | |

No sustained over_budget remains; the residual Sim lines are isolated spikes (the 22-line run was about one per 2 s).

**Dia changes wanted (not made, need approval):**
1. `PackDomainStates` / `DomainStateSnapshotPublisher`: skip unchanged domains or cache per-domain JSON (the pack is 5 to 10 ms per call in Debug).
2. `ViewerStateAdapter`: it re-parses the relayed JSON on the Sim thread and calls the Main-owned `StreamReader::FetchLatest` from Sim; it should not be registered in the Sim registry, or should read the text without parsing.
3. `ProcessingUnit.cpp` `pu.over_budget` warning should be rate-limited (covered by the approved profiling amendment).

**Not verified:** Release build, sanitizer run (ASan/UBSan only would not catch the race), full unscoped gtest run, in-page behaviour under Ultralight's JavaScriptCore (Vitest uses jsdom).

**State JSON contract change (worldgen-view plan task 8, 2026-11-09).** With an `IWorldGenView` attached (the S-1 stage; the domain keeps its old local state when none is set), selection, colour mode and overlays are owned by the hosted DiaCellLayers viewer and the domain only forwards `selectCell`/`clearCell`/`setColourMode`/`toggle`. `view.colourMode` is the viewer's active base layer with `relief` reported as `terrain` (`setColourMode "terrain"` is accepted); `view.modes` = `terrain` then every other registered base layer id (STATIC part: lean frames omit it; a change bumps `staticVersion`); `view.drawers` = `Tiles` (always on), one entry per viewer overlay (`rivers`, `selection.neighbours`; `available` = has something to draw), `Selection` (always on); NEW `view.legend` (DYNAMIC part, on every frame, it follows the active layer): null, a `[{label, colour:"#rrggbb"}]` array (category layers) or `{min, max, ramp, unit, stops:[{t, colour}]}` (float layers); `cell` follows the viewer's first selected cell, so clicks in the map show up in the card. Compact state at Large with a 13-category legend and a selected cell: 6576 bytes (limit 12 KB).

# System Spec: DiaCellHistory

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug, tools

**Origin:** 2026-10-19 world-sim product review. Outcome B: "debuggable … through history". Before this, history was thin and different in each system:
- S-1 had none.
- A4 had a 13-week step-back.
- A1 had a ledger covering the last band only.
- S-2.5 kept aggregate rings that were never saved.

**Mockup:** @docs/specs/applications/dia/systems/diacelllayers/mockups/cell-layer-viewer.html (timeline, snapshots, delta view)
**Sibling:** @docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md

## Purpose

DiaCellHistory records what happened on a cell grid over time. You can:
- scrub back through it in the viewer
- compare now with then, snapshot with snapshot, or run with run
- restore a named snapshot to resume the simulation from there
- stream it all to the World Sim Inspector

It records **DiaCellLayers** layers, so anything a system shows can also be recorded. It is generic; CoW supplies the time labels, the groupings (region, biome) and the snapshot store (E1).

## Responsibilities

- **Tiered recording:**
  - aggregates for every layer for the whole run
  - per-cell rings for opted-in layers within a memory budget
  - an event log
- **Sample timing:** the caller says when to sample, with a sim tick and a display label.
- **Scrub:** show a past sample of the active layer in the viewer without touching the simulation.
- **Snapshots:** named, through an `ISnapshotStore` that the game implements. CoW implements it with E1 SaveLoad. Restore means load.
- **Compare:** delta layers (snapshot vs snapshot, now vs a past sample) and per-group deltas.
- **Export:** live topics, chunked queries, and the same record format in report files.

## Non-Responsibilities

- **Deterministic re-simulation** to arbitrary ticks. Restore goes to a snapshot only.
- **Saving the simulation state.** The game's `ISnapshotStore` does that (CoW: E1).
- **Charts.** The World Sim Inspector draws them; in-game graphs stay the systems' own.
- **Replaying input or bus traffic.** That is F-1 / dia-needs #1.

## Design

### Tiers

| Tier | What | Depth | Cost at Large (6,656 cells) |
|---|---|---|---|
| **Aggregates** | min/mean/max per layer, per group (world, plus each category of the grouping layers, e.g. region and biome) | Whole run | about 7 KB per sample for 20 layers × ~30 groups (float32 min/mean/max, 12 bytes each; corrected 2026-10-20 from "2 KB"); own budget, default 64 MB |
| **Per-cell** | opted-in layers, 16-bit quantised over the schema range | Ring sized by the budget | 13 KB per layer per sample; 6 layers × 520 weeks ≈ 40 MB; default budget 64 MB |
| **Events** | typed records `{tick, type, cell or group, value, text}` emitted by systems | Whole run | small |

- **Grouping layers** are any category layers flagged `history.group: true` in their schema `ui` (CoW: region, biome). Aggregates are computed per group value.
- **Per-cell opt-in:** `cells.history.track <layer…>` / `untrack`, or `history.track: true` in the schema for always-on layers. When the budget would be exceeded, the oldest samples of the largest ring are dropped first. This is logged once, and the timeline shows the gap.
- **Quantisation:** 16-bit over the schema `min`–`max`. Values outside the range clamp, matching DiaValueSchema's clamp rule. Category layers store an 8-bit id.

### Sampling

```cpp
// CoW: GameSpeedController band callback (SimPU)
history.Sample({ tick, "Y12 W30", SampleBand::Week });
```

- On `Sample`, the recorder reads each recorded layer through its DiaCellLayers `read` (skipped if `version` is unchanged) and appends it to each tier.
- Bands let the timeline zoom (week, season, year), and let a game sample different layers at different bands.
- **Events:** `history.Emit({ type: StringCRC("cow.drought"), group: region 3, value: 4, text: "4 dry weeks" })`. Event types are described in DiaValueSchema like any value (`kind: event`).

### Scrub, snapshot and compare

- **Scrub:** `cells.history.view <sampleIndex>` swaps the viewer's base values for the recorded sample.
  - This needs a per-cell ring for that layer. Aggregate-only layers show "no per-cell history" and a group-mean map.
  - The simulation keeps running or stays paused as before. `cells.history.live` returns to live values.
- **Snapshots:**
  - `cells.snapshot.take <name>` calls `ISnapshotStore::Take(name)`. It records the sample index and stores a full per-cell copy of every recorded layer at that moment, so compares work even after the rings roll past it.
  - `cells.snapshot.restore <name>` calls `ISnapshotStore::Restore(name)`. History is truncated after that point, with a Warning.
  - `cells.snapshot.list` and `delete` manage them.
- **Delta view:** `cells.history.delta <layer> <A> <B>`, where A and B are each `now`, `sample:<i>` or `snapshot:<name>`. It shows B − A with the schema's diverging ramp (`ui.delta_ramp`, default `diverging.blue_red`), plus per-group deltas in the selection inspector.
- **Run vs run:** two exported histories (report files) are compared in the World Sim Inspector, on aggregates and events. In-game compare covers one run only.

### Export

- **Topics:**
  - `cells.history.aggregates` (periodic, 1 s): new aggregate rows since the last send
  - `cells.history.events` (event-driven)
- **Queries:**
  - `cells.history.cells {layer, cells?, from, to}`: per-cell series, chunked under the 1 MB message cap
  - `cells.history.snapshot {name, layer}`
- **File:** `cells.history.export <path>` writes `{format: "dia.cell_history", version: 1, world, schema, groups, aggregates, events, snapshots: [names], tracked}`. `schema` embeds the DiaValueSchema entries for every recorded layer and event type, so an offline file explains itself (amended 2026-10-19, T-1). `world` is the loaded world id, used for navigation gating. Per-cell data is optional (`--cells`).
  - Headless runners (S-2.5, S-3 long-run) write the same file next to their reports, so the inspector reads live and offline data the same way.

### CoW binding (consumer side)

- **Snapshots:** `ISnapshotStore` is implemented by a small CoW adapter over E1 SaveLoad (`slot = "snapshot:<name>"`), which is tested bit-identical already.
- **Sampling:** driven from GameSpeedController bands.
- **Groupings:** S-1's region and biome layers.
- **Event emitters:**
  - A4: droughts, storms
  - A3: transitions, recovery stages
  - A1: ecology state changes, crashes
- **What it replaces:**
  - A4's `WeatherHistory` and step-back become a history view plus restore.
  - S-2.5's aggregate rings become history aggregates.
  - A1's ledger stays as the per-cell "why" for the last band (`describeCell`). A1 Open Q4 (Season deltas lost on Year ticks) is closed by recording the ledger's top causes as events per band.

### Logging and observation

- **Channel:** `HIS`.
- **Info:** track/untrack, snapshot take/restore, export.
- **Warning:** budget eviction, truncation on restore, a sample with no recorded layers.
- **Metrics:** memory used, ring depth per layer, samples per second.
- **Health:** budget over 90% is reported as Degraded.

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Recorder and tiers | `Sample`, aggregates by group, per-cell 16-bit rings (one shared sample window), three budgets, events with schema-described payloads | Approved | [recorder-and-tiers.md](recorder-and-tiers.md) |
| 2 | Scrub and timeline | `cells.history.view` / `live` / `step` / `event`, the view value source hook (owned by DiaCellLayers), timeline card (bands, event markers, gaps, sparkline); no playback in v1 | Approved | [scrub-and-timeline.md](scrub-and-timeline.md) |
| 3 | Snapshots | `ISnapshotStore`, take/restore/list/delete, full copies of recorded layers | Draft | TBD |
| 4 | Compare | Delta layers (now/sample/snapshot), per-group deltas in the inspector | Draft | TBD |
| 5 | Export | Topics, chunked queries, `dia.cell_history` file (base64 numeric arrays, embedded schema, world id), runner integration through one `HistoryExporter::WriteFile` | Approved | [export.md](export.md) |

Build order: 1 → 2 → 4 → 3 → 5. Feature 1 needs DiaCellLayers Feature 1.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaCellLayers | Layers, viewer, selection inspector | Approved, not built |
| DiaValueSchema | Ranges (quantisation), ramps, event descriptions | Approved, not built |
| DiaDebugServer | Topics, queries | Done |
| DiaCore `CircularBufferC` | Rings (sized at runtime from the budget) | Done (may need a runtime-sized variant) |
| Game `ISnapshotStore` | Snapshot take/restore (CoW: E1) | Interface in this spec |

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| CH-001 | Tiered recording: aggregates and events always, per-cell opt-in within a budget (user decision 2026-10-19) | Accepted | Yes |
| CH-002 | Going back is a view-only scrub; restore is loading a named snapshot through the game's store, with no re-simulation (user decision 2026-10-19) | Accepted | Yes |
| CH-003 | Compare covers snapshot vs snapshot, now vs past and run vs run; run vs run happens in the inspector over exported files (user decision 2026-10-19) | Accepted | Yes |
| CH-004 | Aggregates and events stream; per-cell data is queried and chunked; live and file use one format (user decision 2026-10-19) | Accepted | Yes |
| CH-005 | Generic: time labels, groupings and the snapshot store come from the game | Accepted | Yes |

## Test Strategy

- **GoogleTests:**
  - Aggregates per group on known data.
  - 16-bit round-trip error ≤ range / 65,535.
  - Budget eviction order and gap marking.
  - Event log ordering.
  - Delta layer correctness.
  - Snapshot take and restore with a fake store, including history truncation.
  - Chunked query reassembly.
  - The export file round-trips.
- **CoW proof (S-3):**
  1. Run 20 years at Small with 3 tracked layers.
  2. Scrub to year 5.
  3. Take "pre-drought" and force a drought.
  4. Delta view shows the expected sign.
  5. Restore, and the A1 state hash matches the hash at the time of the take.
  6. The inspector shows identical aggregates live and from the export.
- **Perf:** a sample at Large with 6 tracked and 20 aggregated layers takes ≤ 3 ms in Release.

## Resolved Decisions

1. **Snapshots are files** (2026-10-19). The CoW store writes E1 save files (`out/saves/cow/snapshot_<name>.sav`), so a snapshot survives restarts and can be attached to a bug report. The per-cell copies of the recorded layers are written next to it (`snapshot_<name>.cells.json`).
2. **Each testbed declares its default tracked layers** (2026-10-19) in its `.diastage` config (`"history": {"track": [...]}`). For S-3: `ecology.producers_total`, `ecology.group.herbivore`, `weather.precipitation` (ids from A1 and A4).

## Status

`Approved` (2026-10-19). Feature specs TBD; no plan yet.

# Feature Spec: DiaCellHistory Recorder and Tiers

**Parent:** @docs/specs/applications/dia/systems/diacellhistory/diacellhistory.md
**Builds on:** @docs/specs/applications/dia/systems/diacelllayers/registry-and-viewer.md (DiaCellLayers Feature 1), @docs/specs/applications/dia/systems/diavalueschema/format-loader-and-service.md

**Gameplay Domains:** debug, tools

## Summary

The recorder at the heart of DiaCellHistory. A game calls `Sample` at the moments it chooses; the recorder reads the recorded layers and appends to three tiers: **aggregates** (min, mean and max, or category counts, per layer and per group, for the whole run), **per-cell rings** (opted-in layers, 16-bit quantised, inside a memory budget, one shared window of samples) and an **event log** (typed records with a payload described in DiaValueSchema). Scrub, snapshots, compare and export (Features 2 to 5) all read what this feature stores.

It delivers the new module `Dia/DiaCellHistory/`, the recorder and its service, the storage tiers with their budgets and eviction, the `Emit` API for events, `cells.history.track` / `untrack`, and the read API the later features use.

## Problem

History is thin and different in every system: S-1 has none, A4 keeps a 13-week step-back, A1's ledger covers the last band, S-2.5's rings are never saved. The World Sim Inspector needs one recording of the whole run that is cheap enough to keep always on, deep enough to explain a crash, and bounded so a 40-year run does not eat memory.

## Goals

1. **Always-on aggregates.** For every layer on the recorded grid, per group, for the whole run.
2. **Opt-in per-cell history.** Quantised rings for chosen layers, bounded by a budget, with one shared window so a sample index means the same thing for every layer.
3. **A typed event log** with schema-described payloads, assigned to samples deterministically.
4. **Bounded memory.** Separate budgets for per-cell rings, aggregates and events; eviction is oldest-first and always visible (gap markers, Warnings, health).
5. **Cheap sampling.** A sample at Large with 6 tracked and 20 aggregated layers takes at most 3 ms in Release.
6. **Generic.** Time labels, bands, groupings and tracked layers come from the game; no game types.

## Non-Goals

- Scrub, the timeline, snapshots, compare, export and the topics (Features 2 to 5).
- Saving history across a game restart.
- Per-layer sample rates (every sample records all recorded layers; `band` is a label).
- Deterministic re-simulation.
- Compressing unchanged samples (see Open Q2).
- Charts or any UI.

## Architecture

New engine module `Dia/DiaCellHistory/` (AD-001, namespace `Dia::CellHistory`), created with `dia scaffold module`, with a module doc. Depends on DiaCore (containers, `StringCRC`, Json), DiaValueSchema and DiaCellLayers. No game types.

```
Dia::CellHistory
  CellHistoryRecorder     Sample, Emit, Track/Untrack, read API; ServiceStream-published
  AggregateStore          per sample: groups, per-layer-per-group stats, category counts
  CellRing                shared-window ring of quantised per-cell samples for tracked layers
  EventLog                events with payload, budgeted
  HistoryConfig           bands, tracked layers, budgets, recorded grid (from module config)
  CellHistoryModule       creates the recorder, applies config, registers commands
```

The recorder runs on SimPU with the DiaCellLayers viewer and providers (same-PU contract, no locking).

### Configuration

From the module config, set per testbed in its `.diastage` (resolved decision 2 of the system spec):

```json
"history": {
  "bands": ["Day", "Week", "Season", "Year", "Decade", "Century"],
  "track": ["ecology.producers_total", "ecology.group.herbivore", "weather.precipitation"],
  "grid": "world.hex",
  "cellBudgetMB": 64, "aggregateBudgetMB": 64, "eventBudgetMB": 16,
  "maxAggregateLayers": 64
}
```

Every key is optional. `grid` defaults to the grid of the first registered layer. Layers whose schema sets `history.track: true` are tracked in addition to the list.

### Sampling

```cpp
struct SampleInfo { uint64 tick; const char* label; uint8 band; };   // band indexes the configured band names
SampleResult CellHistoryRecorder::Sample(const SampleInfo&);          // returns the sample index
```

- A **sample index** is a monotonically increasing integer starting at 0; it is never reused. The label (for example "Y12 W30") and band are stored per sample.
- **Recorded layers** are all float and category layers registered on the recorded grid, except those whose schema sets `history.aggregate: false`. The first 64 (in registration order) are aggregated; the rest log a Warning once and are skipped.
- On `Sample`, for each recorded layer the recorder reads the **live** values through the layer's own `read` (not the scrubbed values the viewer may show), skipping the read when the layer's `version()` is unchanged since the last sample and reusing the previous aggregate row. It then writes:
  1. the aggregate row,
  2. a per-cell slot for each tracked layer.
- A `Sample` with no recorded layers logs a Warning and records only the sample header.

### Tier 1: aggregates

- **Groups:** the implicit group `world`, plus one group per enum value of every category layer whose schema sets `history.group: true` (CoW: region and biome). Group ids are `<layerId>=<enumId>`. Membership is read from the grouping layer at each sample, so a changing grouping (biome) is reflected per sample.
- **Float layers:** per group, `min`, `mean`, `max` as float32 over non-NaN cells; NaN cells are excluded.
- **Category layers:** per group, a count per enum value (uint16).
- **Group sizes:** per group, the number of cells (uint16), stored once per sample, so weighted means across groups can be rebuilt.
- **Cost:** a float layer takes 12 bytes per group per sample. 20 layers × 27 groups is about 6.5 KB per sample; 64 layers × 27 groups about 21 KB. The aggregate budget (default 64 MB) therefore holds roughly 3,000 samples at the cap, and a 40-year weekly run (2,080 samples) of 20 layers is about 14 MB.
- When the aggregate budget would be exceeded, the oldest samples' aggregate rows are dropped and a gap is recorded (see Open Q1).

### Tier 2: per-cell rings

- **Tracked layers:** the config list, the schema flag `history.track: true`, and `cells.history.track <layer…>` / `untrack` at run time.
- **Storage:** float layers as 16-bit values quantised over the schema `min`–`max` (out-of-range values clamp, matching DiaValueSchema's rule); category layers as 8-bit ids. A float layer of 6,656 cells is 13 KB per sample.
- **One shared window.** Every tracked layer keeps the same sample indices. The window length is `cellBudget / bytesPerSample`, where `bytesPerSample` is the sum over tracked layers. When the budget would be exceeded, the oldest sample is dropped from **all** rings together and the retained range `[firstRetained, latest]` advances. This is logged once, and a gap is recorded for the timeline.
- **Tracking later:** a layer tracked mid-run has data only from the sample at which it was tracked; the earlier samples read "no per-cell history" for that layer. Tracking a layer changes `bytesPerSample`, so the window is recomputed (shrinking drops the oldest samples).
- **Quantisation error:** the dequantised value differs from the original by at most `range / 65,535`.
- An attempt to track a layer that does not exist, is on another grid, or whose schema has no `min`/`max` (float) is rejected with an Error.

### Tier 3: events

```cpp
struct HistoryEvent {
    StringCRC   type;          // e.g. "cow.drought", "cow.ecology.cause"
    GroupRef    group;         // optional: grouping layer + value, or none
    uint32      cell;          // optional: cell index, or 0xFFFFFFFF
    const char* payloadJson;   // compact JSON object, at most 1 KB; may be null
    const char* text;          // short human text, at most 64 chars; may be null
};
void CellHistoryRecorder::Emit(const HistoryEvent&);
```

- An event is assigned to the **next sample**: the sample index equals the number of samples taken so far. A system that steps and emits, then the game calls `Sample`, therefore sees its events attached to that sample. Events emitted after the last sample wait for the next one.
- **Payload** is a JSON object described by the event type's DiaValueSchema entry (`kind: event`, `payload` fields, `vocabulary`). In debug builds the recorder checks field names and types against the schema; an unknown event type or a mismatch logs a Warning once per type. A payload over 1 KB is rejected with an Error.
- **Storage:** events are kept for the whole run within the event budget (default 16 MB). When exceeded the oldest events are dropped and a gap is recorded.
- Events are ordered by emit order within a sample.

### Read API (used by Features 2 to 5)

```cpp
uint32  SampleCount() const;  uint32 FirstRetained() const;          // per-cell window start
const SampleHeader& Header(uint32 sample) const;                     // tick, label, band
bool    Aggregate(uint32 sample, StringCRC layer, StringCRC group, AggregateStats& out) const;
bool    CategoryCounts(uint32 sample, StringCRC layer, StringCRC group, uint16* out, uint32 n) const;
bool    CellValues(uint32 sample, StringCRC layer, float* out, uint32 n) const;   // dequantised
EventRange Events(uint32 fromSample, uint32 toSample) const;
```

Each returns false for a sample or layer that was not recorded or has been evicted.

### Commands

| Command | Effect |
|---|---|
| `cells.history.track <layer…>` | Start per-cell recording for the layers; returns the new window length |
| `cells.history.untrack <layer…>` | Stop and free their rings |
| `cells.history.status` | Returns sample count, retained range, memory by tier, budgets, tracked layers and gaps |

### Logging and observation

- **Channel:** `HIS`.
- **Info:** track / untrack (with the new window length), first sample, budget settings.
- **Warning:** per-cell budget eviction (once per eviction episode), aggregate or event eviction, layers beyond the 64 cap, a sample with no recorded layers, unknown event type or payload mismatch.
- **Error:** invalid track request, payload over 1 KB.
- **Traces / Profiling:** `DIA_TRACE_ZONE` per sample; `DIA_PROFILE_SCOPE` on the aggregate pass, the quantise pass and `Emit`.
- **Metrics:** memory used per tier, window length, samples recorded, events recorded, sample ms, layers skipped by the version gate.
- **Health:** `IHealthReporter` reports Degraded when any tier is over 90% of its budget.

## Acceptance Criteria

1. `Sample` returns monotonically increasing indices and stores tick, label and band for each.
2. Aggregates for a float layer give the correct min, mean and max per group on fixture data, excluding NaN; for a category layer they give the correct count per enum value per group; group sizes are stored per sample.
3. A changing grouping layer (cells changing group between samples) gives per-sample aggregates that match the group membership at that sample.
4. Layers beyond 64 are skipped with one Warning; a layer with `history.aggregate: false` is not recorded.
5. A layer whose `version()` is unchanged is not read again, and its aggregate row is reused.
6. Per-cell values are stored as 16-bit (float) or 8-bit (category) and dequantise within `range / 65,535`; out-of-range values clamp.
7. The per-cell window is shared: all tracked layers retain the same sample indices, and when the budget is exceeded the oldest sample is dropped from all rings together and a gap is recorded.
8. Tracking a layer mid-run gives it data from that sample on; earlier samples report no per-cell history for it; untracking frees its ring.
9. Tracking a nonexistent layer, a layer on another grid, or a float layer with no schema range is rejected with an Error and changes nothing.
10. An event is assigned to the next sample, ordered by emit order; events emitted after the last sample are attached to the following one.
11. In debug builds, an event whose type or payload does not match its schema logs one Warning per type; a payload over 1 KB is rejected.
12. The aggregate and event budgets evict oldest-first with a gap recorded and a Warning; any tier above 90% of its budget makes the health reporter Degraded.
13. The read API returns false for evicted or unrecorded samples and layers.
14. The recorder reads live values, not scrubbed ones.
15. A sample at Large (6,656 cells) with 6 tracked and 20 aggregated layers takes at most 3 ms in Release.
16. `cells.history.status` reports sample count, retained range, memory per tier, budgets, tracked layers and gaps.
17. The module has a module doc, `dia check deps` is clean, and no game types or STL appear in public headers.

## Test Strategy

- **GoogleTests (TDD):**
  - Aggregates on known data (float, category, NaN, empty group, changing grouping).
  - 16-bit round trip within `range / 65,535`; clamp.
  - Shared-window eviction order, gap marking, window recomputation on track and untrack, late tracking.
  - Event assignment to the next sample, ordering, payload validation, size limits, event eviction.
  - Version gate (counting stub provider).
  - Read API for present, evicted and unrecorded data.
  - Commands, health at 90%, and live (not scrubbed) reads.
- **Test utilities** in `DiaCellHistory/Testing/`: a stub grid with settable layers and versions, a fake clock for labels, a budget-sizing helper, fixture event schemas.
- **Perf:** the 3 ms sample budget at Large, plus a 40-year (2,080-sample) run with 20 layers within the aggregate budget.
- **Integration (with DiaCellLayers):** register fixture layers, drive 100 samples, check aggregates match direct computation.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CH-001 | Tiered recording: aggregates and events always, per-cell opt-in within a budget | Three tiers, per-cell opt-in, budgets |
| CH-005 | Generic: time labels, groupings and the snapshot store come from the game | Labels, bands, groupings and tracked layers come from the caller and config |
| CL-005 / CL-006 (DiaCellLayers) | Generic; no singletons | The recorder is a `ServiceStream`; no game types |
| VS-004 | Clamp rule at runtime | Quantisation clamps like DiaValueSchema |
| PD-004 | No STL in public APIs | DiaCore containers and fixed arrays |
| AD-001 / AD-003 (Dia) | New module has a module doc; namespace | `Dia/DiaCellHistory/`, `Dia::CellHistory` |
| Game logic on SimPU | Recorder on SimPU with the viewer | Same-PU contract |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaCellLayers Feature 1 | Layer registry, `read`, `version`, grids | Approved, not built |
| DiaValueSchema Features 1 and 6 | Ranges, `history.*` keys, event payload schema | Approved, not built |
| DiaCore containers | Rings sized at runtime | Done (may need a runtime-sized variant) |
| Game sampling driver | CoW: GameSpeedController bands | Built (consumer side) |

## Open Design Questions

1. **Aggregate overflow.** Dropping the oldest aggregate rows loses the start of a very long run, against the "whole run" goal. At roughly 3,000 samples for 64 layers (about 60 weekly years) this is rare, but a decimation rule (keep every Nth old row) may be preferable later. Decide when a real run reaches the budget.
2. **Unchanged samples.** Static tracked layers (terrain) cost a full slot per sample. A "same as previous" marker would save memory but complicates eviction (a dropped keyframe must be promoted). Skipped in v1; revisit if the per-cell budget forces short windows on mostly-static layers.
3. **Correction to the system spec.** The system spec estimated about 2 KB per sample for 20 layers × 30 groups; with float32 min/mean/max it is about 7 KB. The estimate is corrected in the system spec table and the aggregate budget raised accordingly.

## Status

`Approved` (2026-10-20). Plan not yet created.

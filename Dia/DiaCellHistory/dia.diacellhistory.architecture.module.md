---
schema: dia.module.v1
module_id: dia.cellhistory
name: DiaCellHistory
path: Dia/DiaCellHistory
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: dev
version: "0.1"
dependencies:
  required:
    - dia.core
    - dia.valueschema
    - dia.celllayers
    - dia.debugserver
    - dia.visualdebugger
  forbidden: []
public_api:
  headers:
    - DiaCellHistory/DiaCellHistory.h
    - DiaCellHistory/Testing/StubGrid.h
    - DiaCellHistory/Testing/FakeClock.h
    - DiaCellHistory/Testing/BudgetSizing.h
    - DiaCellHistory/Testing/FixtureEventSchemas.h
    - DiaCellHistory/CellHistoryCommands.h
    - DiaCellHistory/TimelineCardState.h
    - DiaCellHistory/CellHistoryDomain.h
    - DiaCellHistory/ViewHistorySourceAdapter.h
    - DiaCellHistory/SideReader.h
    - DiaCellHistory/DeltaValueSource.h
    - DiaCellHistory/DeltaScale.h
  namespaces:
    - Dia::CellHistory::
  entry_points: []
---
# DiaCellHistory

Generic recorder for cell-grid layers registered in DiaCellLayers: always-on aggregates, opt-in quantised per-cell rings in a shared sample window, and a typed event log. Feature 1 (recorder and tiers) of the DiaCellHistory system; scrub, snapshots, compare and export are Features 2 to 5.

Log channel: `HIS`. Namespace: `Dia::CellHistory`. Runs on SimPU with the DiaCellLayers viewer. No game types.

## Public API

| Type | Purpose |
|---|---|
| `CellHistoryRecorder` | `Sample`, `Emit`, `TrackLayer`/`Untrack`, read API (`Header`, `Aggregate`, `CategoryCounts`, `CellValues`, `Events`), status. Published via a `ServiceStream`. |
| `CellHistoryModule` | SimPU module: creates the recorder, applies the `.diastage` `history` block, registers commands. |
| `CellHistoryCommands` | `cells.history.track`, `untrack`, `status` (DIA_DEBUG). |
| `TimelineCardState` | Typed model of the timeline card + not-live banner JSON (`TimelineModel` + `HistoryViewStatus`); `HistoryField` is the future `cells.view.state.history` value. Rebuilds only on sample/view/layer-state change. |
| `ViewHistorySourceAdapter` | `IViewHistorySource` over (recorder, `HistoryViewState`): `HistoryField()` = `TimelineCardState::HistoryField`, `HistoryStamp()` = `Epoch()*2 + (Sample mode ? 1 : 0)` (new recorder samples do not change the field, so they are not in the stamp). Install with `ViewStatePublisher::SetHistorySource`. |
| `CellHistoryDomain` | Panel-only `IDebugDomain` "Cell History" (id `cellhistory`): state = the card; panel commands `view`/`live`/`step`/`event`/`delta`/`track` call the same executors as `cells.history.*` (DIA_DEBUG). |
| `SideReader` | Compare (F4): reads one delta side (`now` = layer live `read`; `sample:<i>` = ring `CellValues`; `snapshot:<name>` = Unavailable until 4.9) with `ParseSide`/`SidesEqual`. NoData never falls back to live. |
| `DeltaValueSource` | Compare (F4): delta-mode values, helper owned by `HistoryValueSource` (not installed itself). Float layer = B - A from two `SideReader` reads; NaN on either side = NaN cell; category layer, NoData or Unavailable side = `GetValues` false (no live fallback). Recorded-vs-recorded error <= 2 x range / 65,535; `now` exact. One HIS Warning per layer per view epoch on a data gap; profile scope `cellhistory.delta_read`. |
| `DeltaScale` | Compare (F4): pure auto-scale logic for delta mode, time injected. Scale = max abs delta rounded up to 1/2/5 x 10^n (`NiceCeil`); grows immediately; shrinks only after max < 40% of scale for 2 s; first read of an all-zero delta = 1.0, floor 1e-6; NaN/inf/negative maxima ignored. `DeltaValueSource::CurrentScale(view)` returns the fixed scale or the auto value; reset on view epoch change (new delta, fixed/auto switch); `SetClock` injects the wall clock. |
| `HistoryConfig` | Parsed `history` block (bands, track, grid, budgets, `maxAggregateLayers`). |
| `AggregateStore` | Tier 1: per-group min/mean/max and category counts per sample; oldest-row eviction with gaps. |
| `CellRing` | Tier 2: shared-window quantised per-cell rings (uint16 float, uint8 category). |
| `EventLog` | Tier 3: typed events assigned to the next sample, budgeted. |
| `CellHistoryObservation` / `CellHistoryHealth` | Metrics, trace/profile scopes, Degraded above 90% of any budget. |

All tier stores provide `TruncateAfter(sample)` for Feature 3 (snapshot restore). `Testing/` ships stubs (`StubGrid`, `FakeClock`, `BudgetSizing`, `FixtureEventSchemas`).

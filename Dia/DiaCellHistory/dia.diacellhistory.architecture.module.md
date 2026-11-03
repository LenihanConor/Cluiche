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
  forbidden: []
public_api:
  headers:
    - DiaCellHistory/DiaCellHistory.h
    - DiaCellHistory/Testing/StubGrid.h
    - DiaCellHistory/Testing/FakeClock.h
    - DiaCellHistory/Testing/BudgetSizing.h
    - DiaCellHistory/Testing/FixtureEventSchemas.h
    - DiaCellHistory/CellHistoryCommands.h
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
| `HistoryConfig` | Parsed `history` block (bands, track, grid, budgets, `maxAggregateLayers`). |
| `AggregateStore` | Tier 1: per-group min/mean/max and category counts per sample; oldest-row eviction with gaps. |
| `CellRing` | Tier 2: shared-window quantised per-cell rings (uint16 float, uint8 category). |
| `EventLog` | Tier 3: typed events assigned to the next sample, budgeted. |
| `CellHistoryObservation` / `CellHistoryHealth` | Metrics, trace/profile scopes, Degraded above 90% of any budget. |

All tier stores provide `TruncateAfter(sample)` for Feature 3 (snapshot restore). `Testing/` ships stubs (`StubGrid`, `FakeClock`, `BudgetSizing`, `FixtureEventSchemas`).

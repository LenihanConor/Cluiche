---
schema: dia.module.v1
module_id: dia.cellhistory
name: DiaCellHistory
path: Dia/DiaCellHistory
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: stub
version: "0.1"
dependencies:
  required:
    - dia.core
    - dia.valueschema
    - dia.celllayers
  forbidden: []
public_api:
  headers:
    - DiaCellHistory/DiaCellHistory.h
    - DiaCellHistory/Testing/StubGrid.h
    - DiaCellHistory/Testing/FakeClock.h
    - DiaCellHistory/Testing/BudgetSizing.h
    - DiaCellHistory/Testing/FixtureEventSchemas.h
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
| `DiaCellHistory` | Placeholder from the scaffold; replaced by `CellHistoryRecorder` and the tier stores in Tasks 4 to 10. |

# CoW Needs Log

Running list of capabilities CoW work has surfaced that don't belong inside `Cluiche/CoW/` — classified per the four-tier model in [cow.md](cow.md). Blocking needs get flagged immediately instead of logged here; this file is for everything else, reviewed in batches.

Columns: `Tier` is one of `Dia` / `CluicheGameBaseline` / `CoW-Shared` (built directly, no sign-off needed) / `Feature-local`.

| # | Need | Tier | Why CoW needs it | Status |
|---|------|------|-------------------|--------|
| 1 | Debug-only bus traffic file recorder (versioned per-message records: tick, type id, payload; no replay reader yet) | Dia | F-1 Test 6: recorded traffic must survive the run, feeding canned data to V-3 later | Proposed |
| 2 | `dia scaffold stage --app <App>` so stages can be generated under `Cluiche/CoW/` | Dia (DiaCLI) | CoW's 12 testbed stages and Boot stage | Proposed |
| 3 | Promote `BootMenuModule` to CluicheGameBaseline with a baseline `StageStatusModule` abstraction (AD-004) | CluicheGameBaseline | F-1 debug menu | Proposed |
| 4 | Fix stale `status: stub` in `dia.messagebus.architecture.module.md` | Dia | Doc is wrong; code is implemented and tested | Proposed |
| 5 | Audio module (ambient, notifications, time-speed-aware) | Dia | A-1; Dia has none | Proposed |

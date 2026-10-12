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
| 6 | `dia scaffold sln-add` path option (currently hard-codes `Dia/<name>/`) so `Cluiche/CoW` can be added | Dia (DiaCLI) | CoW skeleton | Proposed |
| 7 | `dia scaffold app <Name>`: generate a new Cluiche application skeleton (project, sln entry, `main`, manifests, `pipeline.toml` target, Boot stage) | Dia (DiaCLI) | CoW is built by hand from CluicheTest; any future game needs the same | Proposed |
| 8 | Data-driven DiaCLI target registry from `pipeline.toml` (replaces hard-coded target lists in `launch.py`, `deploy_runner.py`, help text) | Dia (DiaCLI) | `scaffold app`; makes adding the `cow` target toml-only | Approved (signed off with the scaffold-app spec) |
| 9 | DiaAttribute Feature 7: band-based initialization (schema-defined band ranges, `InitializeFromBands()`, `band_schema_version`) | Dia | Dragon stat authoring (NONE/LOW/BASELINE/MEDIUM/HIGH/APEX bands instead of raw floats), F-2 band resolver tests, GDD Phase 12 genome system. Spec Draft at @docs/specs/applications/dia/systems/diaattribute/band-based-initialization.md (~11 hours) | Draft |
| 10 | RandomManager: multi-stream RNG support (8 named streams: WLD, A1, B4, B6, C1, C2, B5, C4) with per-stream serialization and debug-build enforcement that nothing bypasses registered streams | Dia | F-2 determinism proofs (Tests 1-3), CoW RNG architecture (GDD F-2 decision 2/7). Built as `RandomStreamManager` in DiaMaths + `RandomStreamModule` in CoW (commit d0bb72ed) | Done |

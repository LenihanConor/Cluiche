**Spec:** @docs/specs/features/draoi/extract-draoigamespeed.md
**Status:** In Progress

Decisions (2026-11-22, user): "no pause", continue straight on from extract-hexworld-terrain. The Approved spec (2026-11-21) is the package sign-off for task 2. Same rule as terrain: no CoW strings or content in the package; CoW values injected from config.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Parameterise calendar and `TimeBand` in place (still in CoW): `Calendar` ladder of named periods and band ladder from config, `PeriodOf`; CoW `.diaapp` carries the current 7/13/4/10/10 values and band names; TDD | Equivalence test: CoW calendar and current `TimeBand` behaviour reproduced exactly from data; no-calendar case | Not Started | sonnet | First task, before the move; config schema addition |
| 2 | Create `Draoi/DraoiGameSpeed` header-only project, module doc, sln entry (Draoi folder); CoW and GoogleTests reference (template: commit `57047287e`) | Builds; `dia check deps`; `dia check sln-sync` | Not Started | sonnet | Sign-off: spec Approved |
| 3 | Move `GameSpeedController.h`; update CoW and F2Harness includes; namespace `Draoi::GameSpeed` or as core (`Draoi::`), CoW keeps `TimeBand` alias | `--filter=*GameSpeed*` | Not Started | sonnet | Mechanical |
| 4 | Move `TestGameSpeedController.cpp` to `Tests/GoogleTests/Draoi/` | Filtered run | Not Started | sonnet | |
| 5 | Close-out: gap audit, one full sharded suite (`--shards >= 2`), F2 harness fingerprint check, precommit | Full suite | Not Started | sonnet | No mutation checks |

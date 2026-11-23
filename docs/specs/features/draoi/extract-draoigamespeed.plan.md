**Spec:** @docs/specs/features/draoi/extract-draoigamespeed.md
**Status:** Done

Decisions (2026-11-22, user): "no pause", continue straight on from extract-hexworld-terrain. The Approved spec (2026-11-21) is the package sign-off for task 2. Same rule as terrain: no CoW strings or content in the package; CoW values injected from config.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Parameterise calendar and `TimeBand` in place (still in CoW): `Calendar` ladder of named periods and band ladder from config, `PeriodOf`; CoW `.diaapp` carries the current 7/13/4/10/10 values and band names; TDD | Equivalence test: CoW calendar and current `TimeBand` behaviour reproduced exactly from data; no-calendar case | Done | sonnet | First task, before the move; config schema addition; Calendar/band ladder from config; CoWCalendar.h holds CoW values; verified 115 GameSpeed tests |
| 2 | Create `Draoi/DraoiGameSpeed` header-only project, module doc, sln entry (Draoi folder); CoW and GoogleTests reference (template: commit `57047287e`) | Builds; `dia check deps`; `dia check sln-sync` | Done | sonnet | Sign-off: spec Approved; Header-only Utility project, sln Draoi folder, CoW/GoogleTests refs; deps + sln-sync pass |
| 3 | Move `GameSpeedController.h`; update CoW and F2Harness includes; namespace `Draoi::GameSpeed` or as core (`Draoi::`), CoW keeps `TimeBand` alias | `--filter=*GameSpeed*` | Done | sonnet | Mechanical; Namespace Draoi::GameSpeed; CoW pulls it in via CoWCalendar.h using-directive; includes switched |
| 4 | Move `TestGameSpeedController.cpp` to `Tests/GoogleTests/Draoi/` | Filtered run | Done | sonnet | Controller tests to Draoi/ (band run uses local generic Calendar); CoW-value calendar tests split to CoW/TestGameSpeedCoWCalendar.cpp; 192 tests same count |
| 5 | Close-out: gap audit, one full sharded suite (`--shards >= 2`), F2 harness fingerprint check, precommit | Full suite | Done | sonnet | No mutation checks; Full sharded suite green (4 shards, 0 failures); F2 fingerprint tests pass; precommit PASS; gap audit: added module calendar-block tests (default, custom, empty, malformed) + diaapp drift guard, 120 GameSpeed tests. No mutation checks. |

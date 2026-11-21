# Feature Spec: Extract DraoiGameSpeed

**Parent:** @docs/specs/applications/draoi/draoi.md (extraction order step (e), part 3)
**Status:** `Draft`
**Origin:** Draoi layer audit, 2026-11-20/21. Source system: @docs/specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md.

## Summary
Move the pure logic of GameSpeedController (calendar, speed presets, pause/modal state machine, day-boundary accumulator; today the header-only `Cluiche/CoW/Shared/GameSpeedController.h`) into `DraoiGameSpeed`. It is not tied to hexes, so no infix. CoW keeps the module, bus messages, debug domain, save participant and the `.diaapp` presets.

**Admission test (honest verdict):** the weakest of the three. One consumer (CoW). A credible second is any game with a day-tick clock and EU4-style speed controls, but the header's calendar is CoW's (7-day week, 13-week season, 364-day year, 10-year decade, 100-year century) and `TimeBand` names those units. Unless the calendar is parameterised, the package is not content-free and fails "mechanism, not content". See Open Design Question 1.

## Goals
- `DraoiGameSpeed` stays header-only (or a tiny library) depending only on the standard library.
- CoW module and tests behave identically; the controller state struct and saved format are unchanged.

## Non-goals
- Moving `Modules/GameSpeed/*` (module, `GameSpeedMessages.h`, debug domain) or the save participant: all wiring (PD-012).
- Owning the clock (DiaSimTime does) or any scheduling (PD-013: state in, state out).
- Changing preset values or the config schema; presets stay in CoW's `.diaapp`.

## Design

### File moves
| From | To (`DraoiGameSpeed/`) |
|---|---|
| `Cluiche/CoW/Shared/GameSpeedController.h` | `GameSpeedController.h` |
| `Cluiche/Tests/GoogleTests/CoW/TestGameSpeedController.cpp` | Draoi test location (core spec Question 2) |

`TestGameSpeedControllerModule`, `TestGameSpeedControllerObservability` and `TestGameSpeedSaveParticipant` stay in CoW. `Modules/GameSpeed/*` and the `Modules/F2Harness` headers that include the controller switch to `<DraoiGameSpeed/GameSpeedController.h>`. Namespace follows the core spec's decision (today `CoW::`).

### Calendar
The calendar constants and `ResolveBand` are the only content in the header. Option A: move as-is, CoW's calendar documented as the default. Option B: a `Calendar` struct supplied by the application, `TimeBand` generalised; this changes API and is not part of a pure move (Question 1).

### Project
`DraoiGameSpeed.vcxproj` (header-only project), sln folder, module doc and registry as in the core spec; CoW references it.

## Binding Decisions
- PD-012/PD-013: no module, no bus, no scheduler; the controller stays pure state and arithmetic.
- Presets come from application config, never compiled into Draoi (existing GameSpeedController goal 2).

## Acceptance Criteria
1. `TestGameSpeedController` passes unchanged from its new location; module, observability and save-participant tests pass in CoW.
2. Saved state struct and bus messages unchanged; F2 harness fingerprints identical.
3. The package includes only the standard library and no CoW header.
4. `dia check deps`, `dia check sln-sync`, `dia docs precommit` pass.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | Create project, module doc, sln folder; CoW reference | Builds; `dia check deps` | User sign-off on the new package |
| 2 | Move `GameSpeedController.h`; update CoW and F2Harness includes | `--filter=*GameSpeed*` | Mechanical |
| 3 | Move `TestGameSpeedController.cpp` | Filtered run | |
| 4 | Close-out: one full sharded suite, F2 harness check | Full suite | |

## Open Design Questions
1. **Admission and calendar.** One consumer and a CoW-specific calendar. Options: (a) defer until a second game wants it; (b) move as-is with CoW's calendar as the documented default; (c) parameterise the calendar first (own feature spec), then move. The audit does not decide this.
2. **Layout and namespace.** Follows the core spec's questions on Draoi folder, test location and namespace.

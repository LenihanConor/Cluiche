# Feature Spec: Extract DraoiGameSpeed

**Parent:** @docs/specs/applications/draoi/draoi.md (extraction order step (e), part 3)
**Status:** `Done` (2026-11-23; approved 2026-11-21)
**Origin:** Draoi layer audit, 2026-11-20/21. Source system: @docs/specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md.
**Depends on / ordering:** Strata -> core -> terrain -> GameSpeed. Last of the three extractions (after @docs/specs/features/draoi/generic-strata.md, @docs/specs/features/draoi/extract-hexworld-core.md and @docs/specs/features/draoi/extract-hexworld-terrain.md); it is the weakest admission case.

## Summary
Parameterise the calendar and time bands as data, then move the pure logic of GameSpeedController (data-driven calendar and bands, speed presets, pause/modal state machine, day-boundary accumulator; today the header-only `Cluiche/CoW/Shared/GameSpeedController.h`) into `DraoiGameSpeed`. It is not tied to hexes, so no infix. CoW keeps the module, bus messages, debug domain, save participant and the `.diaapp` presets.

**Admission test (honest verdict):** the weakest of the three, hence last in the order. One consumer (CoW). A credible second is any game with a day-tick clock and EU4-style speed controls. Today the header's calendar is CoW's (7-day week, 13-week season, 364-day year, 10-year decade, 100-year century) and `TimeBand` names those units, so the package passes "mechanism, not content" only once calendar and bands are data (decisions 1 and 2).

## Goals
- The calendar (ladder of named periods) and the time bands (named band ladder) are data; CoW's configured values reproduce current behaviour exactly.
- `DraoiGameSpeed` stays header-only (or a tiny library) depending only on the standard library.
- CoW module and tests behave identically; the controller state struct and saved format are unchanged.

## Non-goals
- Moving `Modules/GameSpeed/*` (module, `GameSpeedMessages.h`, debug domain) or the save participant: all wiring (PD-012).
- Owning the clock (DiaSimTime does) or any scheduling (PD-013: state in, state out).
- Changing CoW's preset, calendar or band values; they stay in CoW's `.diaapp` (the config schema gains calendar and band sections, see task 1).

## Design

### File moves
| From | To (`DraoiGameSpeed/`) |
|---|---|
| `Cluiche/CoW/Shared/GameSpeedController.h` | `GameSpeedController.h` |
| `Cluiche/Tests/GoogleTests/CoW/TestGameSpeedController.cpp` | Draoi test location (core spec Question 2) |

`TestGameSpeedControllerModule`, `TestGameSpeedControllerObservability` and `TestGameSpeedSaveParticipant` stay in CoW. `Modules/GameSpeed/*` and the `Modules/F2Harness` headers that include the controller switch to `<DraoiGameSpeed/GameSpeedController.h>`. Namespace follows the core spec's decision (today `CoW::`).

### Calendar and bands (data-driven)
The speed/time-scale logic (ticks, presets, day base tick) is the generic part. The calendar is data supplied by the application: a ladder of named periods with lengths, each rung a multiple of the previous (for example day -> week 7 -> season 13 weeks -> year 4 seasons; CoW also has decade 10 years and century 10 decades). Names and lengths come from config, so a game defines its own calendar or none (no rungs: every tick is a plain day). CoW's values (7-day week, 13-week season, 364-day year, 10-year decade, 100-year century, as today's constants) live in CoW's `.diaapp` presets/config; `WeekOf`/`SeasonOf`/`YearOf` become a generic `PeriodOf(rung, day)`.

`TimeBand` is a named band ladder over the day, defined in the same calendar config. Today it is the ordered `Day, Week, Season, Year, Decade, Century` resolved by the day count dividing each cumulative period length (subscribers gate with `band >= TimeBand::Season`); that mapping becomes data (band names and boundaries from config; band = index of the highest matching rung, 0 = plain day). CoW's band names and boundaries live in CoW's `.diaapp`. CoW keeps its `TimeBand` enum as an alias over band indices so `SimulationTick{day, band}` messages, subscribers and saved data are unchanged.

### Project
`DraoiGameSpeed.vcxproj` (header-only project), sln folder, module doc and registry as in the core spec; CoW references it.

## Binding Decisions
- PD-012/PD-013: no module, no bus, no scheduler; the controller stays pure state and arithmetic.
- **Decision 1 (2026-11-21): `DraoiGameSpeed` is generic and data-driven.** The speed/time-scale logic is the generic part; the calendar is a data ladder of named periods with lengths, so a game defines its own calendar or none. CoW's values live in CoW's `.diaapp` presets/config. Parameterisation is an explicit first task, before the move, with a test that CoW's calendar values reproduce current behaviour exactly. Ordered last of the three extractions (weakest admission case).
- **Decision 2 (2026-11-21): `TimeBand` is data-driven.** It becomes a named band ladder over the day, defined in the same calendar config as the period ladder; CoW's band names and boundaries live in CoW's `.diaapp`. CoW's current `TimeBand` behaviour must be reproduced exactly from data.
- Presets come from application config, never compiled into Draoi (existing GameSpeedController goal 2).

## Acceptance Criteria
1. A parameterisation test proves CoW's configured calendar and bands reproduce current `ResolveBand`/`WeekOf`/`SeasonOf`/`YearOf` results exactly (including day 0, boundaries and large days); a no-calendar config yields plain days. `TestGameSpeedController` passes from its new location; module, observability and save-participant tests pass in CoW.
2. Saved state struct and bus messages unchanged; F2 harness fingerprints identical.
3. The package includes only the standard library and no CoW header.
4. `dia check deps`, `dia check sln-sync`, `dia docs precommit` pass.

## Tasks
| # | Task | Test | Notes |
|---|------|------|-------|
| 1 | Parameterise calendar and `TimeBand` in place (still in CoW): `Calendar` ladder of named periods and band ladder from config, `PeriodOf`; CoW `.diaapp` carries the current 7/13/4/10/10 values and band names; TDD | Equivalence test: CoW calendar and current `TimeBand` behaviour reproduced exactly from data; no-calendar case | First task, before the move; config schema addition |
| 2 | Create project, module doc, sln folder; CoW reference | Builds; `dia check deps` | User sign-off on the new package |
| 3 | Move `GameSpeedController.h`; update CoW and F2Harness includes | `--filter=*GameSpeed*` | Mechanical |
| 4 | Move `TestGameSpeedController.cpp` | Filtered run | |
| 5 | Close-out: one full sharded suite, F2 harness check | Full suite | |

## Open Design Questions
None. Layout, test location and namespace follow the core spec's decisions.

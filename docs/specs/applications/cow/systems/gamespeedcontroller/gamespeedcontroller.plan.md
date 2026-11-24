**Spec:** @docs/specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Core: calendar, preset validation, `GameSpeedController`, `DayClock` (`CoW/Shared/GameSpeedController.h`) | 63 unit tests (`TestGameSpeedController.cpp`) | Done | sonnet | Header-only so GoogleTests can include it (CoW is an exe). Not strict RED-first: tests were written after the header. |
| 2 | Messages + `GameSpeedControllerModule` (config load, bus wiring, world scale/step, `SimulationTick`) | 21 integration tests in a real `Application` (`TestGameSpeedControllerModule.cpp`) | Done | sonnet | Pause is scale 0, not `SimTimeDomain::Pause()` (a paused world domain stops the SimPU forward pass). Spec amended. |
| 3 | `cow_main.diaapp` entry + vcxproj wiring | `dia validate manifest`, `dia launch cow` smoke | Done | haiku | Module starts Active with configured presets. |
| 5 | Exhaustive test pass: metrics/health assertions, tick-lag, bus failure paths, restore ordering, pause lag, message storms, stage restart, seeded soak, century census | 21 tests in `TestGameSpeedControllerObservability.cpp` (+2 calendar) | Done | sonnet | 105 GameSpeed/DayClock tests total. Added `GetHealthReporter()` accessor. |
| 6 | Debug panel card: `GameSpeedDebugDomain` (calendar, speed, pause stats + speed slider via `SpeedChangeRequested`) | 6 tests in `TestGameSpeedControllerObservability.cpp`; registers in the running game | Done | sonnet | Panel never touches state directly. Slider posts through the bus. |
| 7 | Save participant (`ISaveable` over the state struct, register with E1 `SaveLoadModule`) | Save/load round trip (F-2 Test 5) | Done | sonnet | Built with E1 (2026-10-14): participant `game_speed`, lazy registration via `ModuleRef<SaveLoadModule>`, `RestoreState` applies the paused scale immediately. 3 tests + E1 integration tests. |
| 4 | Observation pass: logs, trace/profile, metrics, health | Review of touched files | Done | sonnet | 12 `cow.gamespeed.*` metrics, `GameSpeedHealthReporter`, DoStart/DoUpdate trace + profile, transition/config/publish logs. |

## Observation pass

| Pillar | Result |
|---|---|
| Logs | State transitions, ignored requests (debug), rejected steps (warn), config fallback (error), publish failure (error), season-or-larger day ticks (info), clock-backwards (warn), start/stop |
| Traces | `DoStart`, `DoUpdate` zones |
| Profiling | `DoUpdate` scope |
| Metrics | gauges: day, tier, paused, modal_depth, carry_days; counters: ticks, steps, steps_rejected, speed_changes, tick_publish_dropped, clock_backwards, config_fallbacks |
| Health | Failing: subscription or tick-publish failure; Degraded: day ticks lagging behind the one-per-frame cap, or fallback presets in use |

Follow-up: the in-game speed/pause/step buttons are F-2 Test 4 / UI-I. Until then the debug card's speed slider is the only in-game way to change speed.

## Known behaviours found by the test pass

- A tick is published during `DoUpdate` and delivered on the next bus pass, so subscribers see it one frame later.
- Different message types drain in registration order (Opened, then Closed), not send order, within one frame.
- Per-type bus queues are 16 slots with drop-oldest; a burst of more than 16 modal opens loses the oldest.
- A stage stop/restart of the module resets it to day 0, paused. The manifest entry uses `"all"` stages so this does not happen in CoW.
- If something moves the world clock forward while the module runs (for example E1 restoring DiaSimTime) and `RestoreState` is not called before the module's next update, the jump drains as one day tick per frame. E1 must call `RestoreState` in the same frame as the clock restore.

**Spec:** @docs/specs/applications/cow/systems/e1-saveload/e1-saveload.md
**Status:** Done

Decisions (user, 2026-10-14): RandomStream persists as seed + draw count, replayed on restore (small Dia addition, approved). Atomic load is deferred: `SaveLoadModule` pre-validates (slot exists, parses, manifest compatible) and the spec is amended to document that a mid-load participant failure is unsupported until `ISaveable` gains a validate step. No `ISaveable` change.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Dia `RandomStream`: count draws; add `GetSeed()`, `GetDrawCount()`, `Restore(seed, drawCount)` (re-seed + discard N draws) | RED-GREEN: restore reproduces the exact next draws for every draw API (`RandomInt`, `RandomRange` int/float, `RandomUnit`, `RandomBilateral`, `RandomChance`, `RandomUnitVector`, `WeightedRandom`, `Shuffle`) | Done | sonnet | 30 RandomStream tests pass. Dia edit approved. Draw count must be exact, so count generator invocations, not API calls. |
| 2 | `SaveLoadModule` core: config, registry, `SaveManager`, `Register/UnregisterParticipant`, `RequestSave/RequestLoad`, pending executed in `DoUpdate`, `EnumerateSlots`, pre-validation before load, `cow.save`/`cow.load`/`cow.save.list` commands | RED-GREEN: spec unit tests 1-8, 10 using a fake `ISaveable` (`GoogleTests/CoW/SaveLoad`) | Done | sonnet | 11 tests pass. Dia quirk: SlotManager::EnumerateSlots returns file name minus .sav (slot_x), module strips the pattern prefix. Base dir overridable via config base_directory. 64KB save buffer limit. Pure CoW. Use `Dia::API` command registration. |
| 3 | GameSpeedController save participant (day counter only, v1), registered in `GameSpeedControllerModule::DoStart`; load calls `RestoreState` | RED-GREEN: save at day N, mutate, load, day == N and paused | Done | sonnet | 3 tests pass (110 GameSpeed+SaveLoad). Registers lazily from DoUpdate via ModuleRef. Round trip does not yet cover SimTime clock restore (task 6). |
| 4 | RandomStream save participant (8 streams: seed + draw count, v1) in `RandomStreamModule` | RED-GREEN: draw, save, draw more, load, next draws match the original run | Done | sonnet | 5 tests pass (145 RandomStream+SaveLoad+GameSpeed). RandomStreamModule had no DoUpdate and no DIA_MODULE registration: task 5 must add the registration. |
| 5 | Wiring: inject `SaveLoadModule` registry into `DiaSimTimeModule::SetSaveRegistry` before its `DoStart`; add `SaveLoadModule`, `RandomStreamModule`, `DiaSimTimeModule` (if absent) to `cow_main.diaapp` with `SaveLoadModule` before `GameSpeedControllerModule`; vcxproj entries | `dia validate manifest`; `dia launch cow` smoke | Done | sonnet | 148 tests pass, manifest validates, CoW compiles. SaveLoadModule::OnConnectStreams injects the registry into DiaSimTime (instance id must be "DiaSimTime"). Added DiaSimTime project ref + SimTime streams to cow_main. Live smoke NOT run: no headless launch for cow. |
| 6 | Integration round trip (F-2 Test 5 precursor): real `Application` with SaveLoad + SimTime + GameSpeed + RandomStream; save, advance, load; day counter, world clock and RNG continuation match | Integration test | Done | sonnet | 4 integration tests pass. Found 2 Dia bugs on the way (both fixed, user approved): AdvanceTo ignored rewinds -> TimeServer::SetTime; paused scale applied one tick late -> SetTimeScaleImmediate. Both restore-only APIs. |
| 7 | Observation pass; spec amended for atomic-load gap; update GameSpeedController plan task 7 | Review of touched files | Done | haiku | Spec amended (atomic-load gap, as-built). Full `googletest`: 9300 passed. |

## Observation scan (suggestions, non-blocking)

| Pillar | Result |
|---|---|
| Logs | Save/load success and failure (with result code), command registration, participant registration (once), restore skips/errors |
| Traces / Profiling | `SaveLoadModule` DoStart/DoUpdate zones. `RandomStreamModule` has none (trivial work) |
| Metrics | `cow.saveload.saves/save_failures/loads/load_failures`. Suggest: save/load duration and slot size gauges, RNG draw-count gauges (S-7 Endurance will care) |
| Health | None. Suggest a `SaveLoadHealthReporter`: Degraded after a failed save/load, Failing if the save directory cannot be created |

Open items: live smoke of `cow_main.diaapp` (no headless launch for CoW); `ISaveable::Validate` for true atomic load (before A1); 64 KB `SaveManager` buffer.

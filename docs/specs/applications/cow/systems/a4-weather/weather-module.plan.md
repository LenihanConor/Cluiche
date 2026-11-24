# Plan: A4 WeatherModule & Deferred Step (A4 Weather Feature 3)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-module.md
**Status:** In Progress
**System plan:** @docs/specs/applications/cow/systems/a4-weather/a4-weather.plan.md (row 3)

Re-planned 2026-11-24 against the Draoi layer amendment and the code as it stands. The 25-row plan of 2026-11-18 is replaced.

## Foundation audit (Task 0, done 2026-11-24)

Present: `CoW::WorldStep::WorldStepStageBase<TSim,TEntry,TLatch>` (+ `Hooks`, `MutationQueue`, `SpinUpSlot`, `HoldGenerationToken`, `Testing/FakeWorldStage.h`, `WorldSimSaveCodec`) in `Cluiche/CoW/Shared/WorldStep/`; `WorldStepModule::RegisterStage/UnregisterStage` (Cluiche/CoW/Modules/WorldStep); `Dia::DeferredStep::IStepStage`; `WorldGenerationModule` (`GetCurrentWorld`, `HoldGeneration`, `ReleaseGeneration`, `IsReady`) and `WorldGenerationComplete/Failed` in `WorldGenerationMessages.h`; `Draoi::HexWorld::Weather::WeatherSimulation` (`Initialise`, `Capture`, `LatchClimate`, `Run`, `Commit`, `Step`, `SetCommittedWeek`, `AddForcing`, `RegisterCellSet`), `WeatherParameters` + `BindWeatherParameters`, `HydrologyInputs`, `SeasonalBaselineInput`, `InfluenceSnapshot`; `Dia::Attribute::ParameterFile`/`ParamBinding`; `Draoi::HexWorld::BiomeRegistry`; `CoW::WeekOf/SeasonOf/kDaysPerSeason` in `Shared/CoWCalendar.h`; WorldGenerationModule shows the ParameterFile load pattern.

Absent (decisions):
- `WeatherRunner` does not exist. Spin-up is a loop of `WeatherSimulation::Step` (week index i, season from `i`, calendar offset 0) inside the stage's `InitialiseSim` hook, cancel checked between weeks.
- `WeatherUpdated` is not declared anywhere. Follow `GameSpeedMessages.h` / `WorldGenerationMessages.h` hand-written POD pattern unless `cow_worldsim.diagamemessages` + `dia codegen messages` exist (subagent checks; if codegen exists use it).
- `JobSystemPlaceholderConsumerModule` is already gone from code: AC 28 reduces to confirming that.
- **Features 7, 8, 9 not built** (no `InfluenceBook`, `ClimateCycles`, preview source): pass an empty `InfluenceSnapshot`, no `LatchClimate`. Spec Task 16 and ACs 38, 39 are **deferred**, tracked here as Skipped with the reason, to be done when those features land.
- **A5 not built.** A5's amendment drops `IWaterInputSource`, `IHydrologySpinUpHost` and `HydrologyCoupling.h` in favour of adapter-filled arrays. So the coupled spin-up and the water-source seam (ACs 33, 34, 35, 36(coupled part), 37) are **deferred to A5's module feature**. Built here: `IHydrologyInputsProvider` in its own header, `SetHydrologyProvider(IHydrologyInputsProvider*)` latched at `Capture` (ACs 31, 32, 40), and `IsReadyForRevision(rev)` (AC 36).

## Design for this build
- `Cluiche/CoW/Modules/Weather/WeatherModule.h/.cpp` (SimModule, stage-scoped, deps `MessageBusHostModule, WorldStepModule, WorldGenerationModule, SimQueryHostModule`), `Cluiche/CoW/Shared/Weather/` : `WeatherStage.h/.cpp` (derives the base; owns the hooks), `WeatherEntry.h` (entry kinds), `WeatherLatch.h`, `HydrologyInputsProvider.h`, `WeatherCause.h`. Namespace `CoW::Weather`, log channel `WTH`, ids from `WorldSimNames.h`.
- Hooks: `InitialiseSim` (terrain from the world -> `WeatherTerrain`, `Initialise`, then `spinup_weeks` `Step`s with cancel between weeks via the slot's revision/cancel check; EC-01 false + cause), `LatchInputs` (queue already drained by the base; `ParamBinding::Snapshot`, season `Season(SeasonOf(day)%4)`, week `WeekOf(day) % 52`, baseline provider or table, clamp [0,1], hydro inputs, empty influences, then `sim.Capture`), `ApplyEntry` (SetParam, ParamOverrides, Reload, Forcing, RegionForcing, BiomeChange, Preset, StateReplace, ClearWindOverrides), `RunStep` (`sim.Run`), `PublishCommit` (`sim.Commit`, `SetCommittedWeek(WeekOf(day))`, publish `WeatherUpdated{week, season}`).
- Ordering: base Capture applies queue entries, then calls `LatchInputs` (so snapshot sees queued sets).
- Verification per task: `dia run googletest --filter="<suite>" --shards 2` only.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Foundation audit (above) | Findings recorded | Done | sonnet | See audit |
| 1 | Scaffold: files, vcxproj (CoW + GoogleTests), `WeatherUpdated` message, `HydrologyInputsProvider.h`, `WeatherEntry/Latch/Cause`, empty stage deriving the base and compiling against a fake registry | AC 3 (register id `weather` in DoStart, unregister in DoStop), AC 40 (standalone header TU), message round trip | Pending | sonnet | Check codegen availability first |
| 2 | Hooks `InitialiseSim` + world ingest + spin-up loop (cancel between weeks, `spinup_weeks` 0 = none) | RED-GREEN: AC 4, 5, 6, 7 (bit-identical to a direct Step loop; no `WeatherUpdated`), 8 (sync launcher), 9, 10, 26 | Pending | sonnet | Spin-up wall time ballpark in notes |
| 3 | Hooks `LatchInputs` + `RunStep` + `PublishCommit`: calendar derivation, baseline provider/table/clamp, hydro provider latch, `SetCommittedWeek` | RED-GREEN: AC 11, 12, 13, 14, 15, 16, 17, 24, 25, 27, 31, 32, 36 | Pending | sonnet | Retried step uses represented day. Poison tests per spec |
| 4 | Parameters at `DoStart` (ParameterFile + ParamBinding strict, Failing on fault), `ApplyEntry` nine kinds, `Queue*` API | RED-GREEN: AC 18 (amended: Failing, never ready), 19, 20, 21, 22, 23 | Pending | sonnet | Injectable file source so read count is observable |
| 5 | Module: config validation (`spinup_weeks` 0..520), subscriptions + `HoldGeneration` release, DoUpdate poll, DoStop order, health, logs, trace zones, hosting stage manifest wiring | AC 1, 2, 29, 30, 28 (placeholder absent), health causes (Degraded/Failing) | Pending | sonnet | Hosting `.diaapp` after `WorldStepModule`; throwaway negative manifest in fixtures |
| 6 | Exhaustive pass: gap audit of every AC vs tests, edge cases (regen mid-spin-up, spinup 0/520, failed then complete, queue exactly 128, rapid regens, 1- and 2-cell worlds, Commit after DoStop, sidecar fault then reload) | Gap list in notes | Pending | sonnet | Tests only. No mutation checks |
| 7 | Observation scan, scoped verification (`WeatherStage*:WeatherModule*`, `dia check deps`, `dia docs precommit`) | pass/fail | Pending | haiku | |
| 8 | Close-out: plan rows, spec-done only if nothing deferred is required | statuses | Pending | haiku | Deferred ACs (33-35, 37-39, climate/influence/coupled) stay listed; spec stays Approved-with-deferrals unless user decides otherwise |

## Deferred (waiting on other features)
Spec Task 16 / AC 38, 39 (Features 7-9 types); AC 33, 34, 35, 37 and the `IWaterInputSource`/`IHydrologySpinUpHost` seams (A5 module, whose amendment replaces them); AC 33's A4-only half (deferred-vs-synchronous, 52 weeks, carried from ca-pipeline) is built in Task 3/2 against the real stage and manual launcher.

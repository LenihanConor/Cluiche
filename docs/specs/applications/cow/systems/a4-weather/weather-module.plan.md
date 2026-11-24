# Plan: A4 WeatherModule & Deferred Step (A4 Weather Feature 3)

**Spec:** @docs/specs/applications/cow/systems/a4-weather/weather-module.md
**Status:** Done (buildable scope); deferrals listed below
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
| 1 | Scaffold: files, vcxproj (CoW + GoogleTests), `WeatherUpdated` message, `HydrologyInputsProvider.h`, `WeatherEntry/Latch/Cause`, empty stage deriving the base and compiling against a fake registry | AC 3 (register id `weather` in DoStart, unregister in DoStop), AC 40 (standalone header TU), message round trip | Done | sonnet | Check codegen availability first; 808d5dae1 |
| 2 | Hooks `InitialiseSim` + world ingest + spin-up loop (cancel between weeks, `spinup_weeks` 0 = none) | RED-GREEN: AC 4, 5, 6, 7 (bit-identical to a direct Step loop; no `WeatherUpdated`), 8 (sync launcher), 9, 10, 26 | Done | sonnet | Spin-up wall time ballpark in notes; aea783a5a; spin-up via Step loop, serial executor |
| 3 | Hooks `LatchInputs` + `RunStep` + `PublishCommit`: calendar derivation, baseline provider/table/clamp, hydro provider latch, `SetCommittedWeek` | RED-GREEN: AC 11, 12, 13, 14, 15, 16, 17, 24, 25, 27, 31, 32, 36 | Done | sonnet | Retried step uses represented day. Poison tests per spec; 4e11fef99; AC33 A4-only half proved |
| 4 | Parameters at `DoStart` (ParameterFile + ParamBinding strict, Failing on fault), `ApplyEntry` nine kinds, `Queue*` API | RED-GREEN: AC 18 (amended: Failing, never ready), 19, 20, 21, 22, 23 | Done | sonnet | Injectable file source so read count is observable; c03ed8b6d + 15734e364 (Draoi SetCellBiome/ClearWindOverrides, user-approved); preset/state-replace apply = Features 6/4 |
| 5 | Module: config validation (`spinup_weeks` 0..520), subscriptions + `HoldGeneration` release, DoUpdate poll, DoStop order, health, logs, trace zones, hosting stage manifest wiring | AC 1, 2, 29, 30, 28 (placeholder absent), health causes (Degraded/Failing) | Done | sonnet | Hosting `.diaapp` after `WorldStepModule`; throwaway negative manifest in fixtures; 55dda33a7 + 7197001f2 job-system launcher, dev reload; afd082a32 validator deps check AC1; no production manifest (S-2.5 unbuilt) |
| 6 | Exhaustive pass: gap audit of every AC vs tests, edge cases (regen mid-spin-up, spinup 0/520, failed then complete, queue exactly 128, rapid regens, 1- and 2-cell worlds, Commit after DoStop, sidecar fault then reload) | Gap list in notes | Done | sonnet | Tests only. No mutation checks; d78a61e77; gap table below |
| 7 | Observation scan, scoped verification (`WeatherStage*:WeatherModule*`, `dia check deps`, `dia docs precommit`) | pass/fail | Done | haiku | precommit PASS, deps OK, arch clean, 239 scoped tests pass; Low obs: spin-up duration metric, profile scope on spin-up, capped executor unused (needs Dia API) |
| 8 | Close-out: plan rows, spec-done only if nothing deferred is required | statuses | Done | haiku | Deferred ACs (33-35, 37-39, climate/influence/coupled) stay listed; spec stays Approved-with-deferrals unless user decides otherwise; Spec stays Approved: deferred ACs 33/34/35/37/38/39 + Task 16 (Features 7-9, A5); full suite left to worldstep.plan close-out |
| 9 | Spin-up through the DeferredStep capped executor (spec "World ingest and spin-up" step 3): `DeferredStepModule::GetLauncher()`, `WeatherStage` `kCappedLauncher` + `SpinUpStep` adapter + token table (4), module selects it when `DeferredStepModule` resolves | RED-GREEN: GetLauncher null/non-null + step through it completes (DeferredStepModule suite); SpinUp suite: bit-identical via real JobStepLauncher + gate admitted, Stop joins, launcher-null fallback one Warning (inline and pool), stale discarded + no token leak over 14 regens | Done | sonnet | Not committed. Executor reaches `InitialiseSim` through a thread_local set around `RunJob` (restore on SimPU keeps the serial executor). Stage Stop/dtor cancel, Wait and Release live tokens (AC 30); a token from a swapped-out launcher is dropped. No manifest change: WorldStepModule already hard-depends on DeferredStepModule and the validator only checks declared deps. Gotcha: `dynamic_cast` on `CappedParallelExecutor` throws (no RTTI in the lib), so tests assert via CapGate `admitted` instead. 446 scoped tests pass, deps OK, 80 manifests OK |

## Deferred (waiting on other features)
Spec Task 16 / AC 38, 39 (Features 7-9 types); AC 33, 34, 35, 37 and the `IWaterInputSource`/`IHydrologySpinUpHost` seams (A5 module, whose amendment replaces them); AC 33's A4-only half (deferred-vs-synchronous, 52 weeks, carried from ca-pipeline) is built in Task 3/2 against the real stage and manual launcher.

## Task 6 gap audit

Files: `Cluiche/Tests/GoogleTests/CoW/Weather/`: Stage=TestWeatherStage, Module=TestWeatherModule, SpinUp=TestWeatherSpinUp, Steps=TestWeatherSteps, Params=TestWeatherParameters, Queue=TestWeatherQueue, Sidecar=TestWeatherSidecar, Edge=TestWeatherEdgeCases (new, Task 6; 25 active + 1 DISABLED_). Deferred and out of scope: AC 33, 34, 35, 37, 38, 39 and spec Task 16.

| AC | Covered by | Gap closed in Task 6 |
|----|------------|----------------------|
| 1 | Fixtures `Cluiche/Tests/Fixtures/WeatherManifest/{valid,negative_order}`; checked by hand with `dia validate manifest --path` (valid: 0 errors; weather before WorldStepModule: REQUIRES_TICK_AFTER_ORDER); `Dia/DiaCLI/tests/test_validate_requires_tick_after.py` covers the ordering rule with a generic chain | Open: no automated test consumes the fixtures (see findings) |
| 2 | Module `WeatherModuleConfig.*` (range, non-integer, key named, malformed, config error stops start) | none |
| 3 | Module `RegistersStageIdWeatherInDoStartAndUnregistersInDoStop`, `ModuleCanStartAgainAfterStop` | none |
| 4 | SpinUp `ReadyForRevisionOnlyOnceItsSpinUpCompleted`; Steps `ReadyForRevisionOnlyForTheTargetOnceSpunUp` | Edge `ZeroSpinUpWeeks...`, `MaximumSpinUpOf520Weeks...` |
| 5 | SpinUp `NoWaterSourceIsFailingWithCauseAndNoResult`, `NoWaterSourceThenWorldWithWaterRecovers` | Edge `OneCellWorldWithoutWaterFailsWithNoWaterSource` |
| 6 | SpinUp `SecondCompletion...`, `FinishedResultOfOlderRevision...`, `SupersededMidSpinUp...` | Edge `ThreeRapidRegenerations...`, `RapidRegenerationsAfterEarlierJobsAlreadyRan...`, `PendingResultThenRegenerationInTheSameTick...`, `SynchronousSpinUpCompletingInTheSameTick...` |
| 7 | SpinUp `SpunUpFieldsAreBitIdenticalToDirectStepLoop` (60 weeks), `SpinUpStartsAtCalendarOffsetZero...`; Steps `NoMessageDuringSpinUp`; Module `WorldGenerationCompleteOnTheBus...` (no WeatherUpdated) | Edge 520-week bit-identical loop |
| 8 | SpinUp `SynchronousLauncherCompletesBeforeAnnouncementReturns` | Edge synchronous same-tick regeneration |
| 9 | SpinUp `WorldGenerationFailedIs...`, `FailureWhileSpinningUp...`; Module `...FailedOnTheBus...` | Edge `FailureBeforeAnyWorldThenComplete...`, `TheSameRevisionAnnouncedAgainAfterAFailure...`, `FailureWhileJobQueuedThenNewRevision...`, `WorldFailureWhileAStepIsInFlight...` |
| 10 | SpinUp `GenerationHoldTakenInStart...`; Module `HoldsGenerationThroughStart...` | none |
| 11 | Steps `StepDaysMapToSeasonsAndPublishTheirWeek`, `FirstLiveStepSeasonComesFromItsStepDay` | none |
| 12 | Steps `RetriedStepWithEarlierDay...` | none |
| 13 | Steps `OneMessagePerAppliedStep...`; Edge `RegenerationBetweenCaptureAndRun...` (discard publishes nothing) | none |
| 14 | Steps `WorldRegeneratedBeforeCommitDiscards...` | Edge `RegenerationBetweenCaptureAndRunDiscardsTheStepAndLeavesTheNewWorldClean`, `EntriesQueuedDuringADiscardedStep...`, `WorldFailureWhileAStepIsInFlight...` |
| 15 | Steps `PromotionOnlyInsideCapture` | none |
| 16 | Steps `RunIgnoresParametersQueueAndTerrainChangedAfterCapture` | none |
| 17 | Steps `RunNeverCallsTheBaselineProvider` | none |
| 18 (amended) | Params `MissingSidecar...`, `FaultedStageNeverBecomesReady...`, `BadValue...`, `BoundKeyMissing...`, `SidecarKeyWithNoBinding...`, `PackThatBreaksAConstraint...`; Params module tests; Sidecar suite | Edge `SidecarFaultIsNotRecoveredBy...`, `SidecarFaultThenStopAndStartLoadsAgain...` |
| 19 | Queue `ValidSetAppliesAtNextCaptureAndNotBefore`, `InvalidSetReturnsFalseAtOnce...`, `TooLongKey...` | none |
| 20 | Params `ReloadDiscardsSets...`, `FailedReloadKeepsThePreviousValues...`, `PackIsAtomic...`, `ValidPack...`, `NonObjectPack...` | none |
| 21 | Queue `EntriesApplyInArrivalOrder...`, `FullQueueRefusesTheNextEntry...`, `SideStorePayloads...` | Edge `ExactlyOneHundredTwentyEightEntries...` (two rounds, five refused kinds, five warnings, drain frees the queue) |
| 22 | Queue `PresetAndStateReplace...` (drained and logged only: Features 4 and 6 not built), `ForcingDoesNotChangeWhatAReaderSees...` | Apply-side of preset and state replace deferred with Features 4 and 6 |
| 23 | Params `NoSidecarReadAfterInitExceptThroughAQueuedReload` | none |
| 24 | Steps `NoBaselineProviderIsDegraded...`, `SettingTheBaselineProvider...` | none |
| 25 | Steps `OutOfRangeBaselineIsClamped...` | Edge `NanBaseline...`, `InfiniteBaselineValuesClamp...` |
| 26 | SpinUp `SnapshotCellCountIsZeroBeforeFirstWorld...`, `SnapshotIsZeroWhileANewerWorld...` | none |
| 27 | Steps `ReaderInTheHandlerSeesThatStepsCommittedState` | none |
| 28 | `JobSystemPlaceholderConsumerModule` has no tracked file (`git ls-files`), no `cow_main.diaapp` entry in source | none (no test needed) |
| 29 | Module `NothingFromTheFirstVisitSurvives...`, `ASpinUpJobFinishingAfterExit...`, `RepeatedEnterExitCycles...` | Edge `CommitAfterStop...`, `CaptureRunCommitAfterStop...` |
| 30 | Module `DoStopDuringARunningSpinUp...` (thread mode, 520 weeks) | none (sanitizer on request only) |
| 31 | Steps `ProviderArraysAreLatchedOnce...` | Edge `HydrologyProviderWithWrongSizedArrays...`, `HydrologyProviderRemovedAfterUse...` |
| 32 | Steps `NoHydrologyProviderIsNotDegraded...`, `HydrologyProviderWithNothingToGiveBehavesAsAbsent` (provider returns false) | none |
| 36 | Steps `ReadyForRevisionOnlyForTheTargetOnceSpunUp`; SpinUp AC 4 test | none |
| 40 | `TestHydrologyInputsProviderHeader` | none |
| Amendments | 1 (base stage) Stage suite, 2 (parameters) Params, 3 (health) Steps/Queue/Params, 4 (message) Module `WeatherMessages.*`, 5 (blackboard writes) not built | Not covered: amendment 5 (`Blackboard::Set` from PublishCommit) and amendment 6 (save id `cow.weather`, Feature 4); both deferred with their features |

### Task 6 findings (production code untouched)
1. **Stop leaves `mStepValid` set (WorldStepStageBase.h).** `Capture`, then `Stop`, then `Run` or `Commit` dereferences the null active sim (Teardown calls DropSims but never clears the step state). Production is protected only by DoStop order (unregister from the coordinator first). Test `EdgeTest.DISABLED_RunAndCommitAfterStopWithAStepInFlightAreHarmless` is written and disabled; enable when the base clears `mStepValid`/`mInFlight` in Teardown. Low severity.
2. **A parameter fault is permanent for the stage object.** A queued reload, a successful second `LoadParameters`, and a new world do not clear Failing; only `Stop` then `Start` does (asserted by `SidecarFaultIsNotRecoveredBy...` and `SidecarFaultThenStopAndStartLoadsAgain...`). Matches the amended AC 18 and the `FailStage` contract, recorded so nobody expects live recovery. `QueueReload` on a faulted stage returns true but the entry is dropped at the next Capture.
3. **AC 1 has no automated consumer** of the `Tests/Fixtures/WeatherManifest` fixtures; verified by hand this session with `dia validate manifest --path`. Promote to a pytest in the DiaCLI tests if wanted.

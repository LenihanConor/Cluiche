# Plan: World Step (system)

**Spec:** @docs/specs/applications/cow/systems/worldstep/worldstep.md
**Status:** Not Started

System-level tracker. Each row is a feature spec with its own plan; the detailed tasks live there. The three weather systems share one cross-system order: see `a4-weather.plan.md` and `s25-weatherpatterns.plan.md`.

**Dia code status (2026-11-20):** `Dia/DiaDeferredStep/` is built (Done 2026-11-18): `StepKey`, `IParallelExecutor`, the scheduler, capped executor, save participant and `DeferredStepModule` all exist. `IDeferredStep::ValidatePending` is already in `IDeferredStep.h`, and `DeferredStepModule` already attaches its save participant lazily in `DoUpdate`. F1 Task 0 passes; F3 tasks 1 and 11 only verify and add tests where missing. (The 2026-11-16 note that the code was unbuilt is stale.)

**Speed-ups applied (2026-11-20):** F1 RED/GREEN cycles are grouped (tasks 0+1 and 2-4 as group A; 5-7 group B; 8-9 group C; 10-13 group D), one commit per group, and the gap audit, observation scan and verification run (tasks 15, 17, 18) are one pass. Spec housekeeping (F1 task 14, F3 task 14) is done once at the end. Row 6 (Weather-stack close-out) is out of scope for this run.

The feature plans carry the 2026-11-16 Gap-pass amendments (they supersede contradicting spec text): F1 adds the `IChainObserver` task; F2 maps lazy `SetKeySource`, `slack_days` polling, the `KickResult` table, five metrics and `GetPendingVersion`; F3 adds `latest_day` (AC 21, 22), enabled-set Validate, `BeginObject` stage sub-objects, and a lazy-attach Dia task; AC 11 is not applicable.

## Build order and parallelism

```
F1 step chain ──► F2 module & cadence ──► F3 save & history signal
(pure, fakes)     (needs DiaDeferredStep      (needs DiaDeferredStep F3 and the
                   F1, F2, F4 built)           ValidatePending edit)
```

- **F1 needs only DiaDeferredStep F1 Task 1** (`StepKey`, `IParallelExecutor` headers), which is not built yet (see the note above). It can start the moment those headers ship, in parallel with the rest of DiaDeferredStep.
- **F2** needs F1 and the DiaDeferredStep scheduler, capped executor and `DeferredStepModule` (F1, F2, F4). Its first task proves whether `SimulationTick` reaches the module before `DeferredStepModule::DoUpdate` in the same sim step.
- **F3** needs F2 and DiaDeferredStep F3 (`DeferredStepSaveParticipant`). It contains two Dia code edits: `IDeferredStep::ValidatePending` (signed off) and the lazy attach of the save participant (needs confirmation).
- World Step is consumed by A4 F3 (stage registration) and by every world-sim stage; A4 F3 can use a fake `WorldStepModule` until F2 exists.

## Sign-offs (AD-001)

| Change | Where | Status |
|---|---|---|
| `IDeferredStep::ValidatePending` (defaulted virtual) and its call in `DeferredStepSaveParticipant::Validate` | DiaDeferredStep | **Given 2026-11-16**; the edit is a task in F3's plan, with a Dia test |
| `DeferredStepModule` reads its key source lazily so `SetKeySource` may be called any time before the first `DoUpdate` (DiaDeferredStep F4; module-and-observability amendment 1) | DiaDeferredStep (unbuilt) | **Given 2026-11-16** Consumed by F2 task 1 and task 3 |
| `DeferredStepModule` attaches `DeferredStepSaveParticipant` on its first `DoUpdate`, not `DoStart` (DiaDeferredStep F4; module-and-observability amendment 2) | DiaDeferredStep (unbuilt) | **Given 2026-11-16** Built by F3 task 11 |
| `WorldStepModule::SetDeferred` and the `cow.worldstep.step` `action` argument | CoW-Shared, not Dia | No sign-off needed; **owned by S-2.5 F1's need, built in F2** (see Open items) |

## Cross-feature rules

- **Verification cadence** (feedback_verification_cadence): scoped `dia run googletest --filter=... --shards 2` per task, using a narrow filter on the suite under work; feature-wide filters only in a feature's verification task. Sanitizer: on request only (user, 2026-11-17). Long tests (>= 10 simulated years, all-presets x all-seeds loops, multi-minute runs) are tagged `SLOW_`, skipped by default, run once in the feature's scoped verification task and once in the close-out. Perf is Debug ballpark only; a bad ballpark queues a perf pass. No per-feature Release builds: Release checks move to the Weather-stack close-out (row 6). Full suite and the cross-feature checks run once, in the Weather-stack close-out (2026-11-17, speed-ups).
- **TDD** for all new public API, RED proven. **No mutation checks.** **Gap audit** closes every feature plan. **Commit after each task**, plan row updated in the same commit.
- **Model column**: haiku and sonnet only; ask first if opus seems needed (feedback_confirm_opus).
- **Engine boundary**: CoW code under `Cluiche/CoW/Shared/WorldStep` and `Cluiche/CoW/Modules`. The `ValidatePending` edit and the lazy-attach edit (F3 tasks 1 and 11) are the only Dia changes and are tagged as such.
- **Name budget**: two sim-hosted names (`cow.worldstep.status`, `cow.worldstep.step`), registered all-or-nothing. The per-PU cap is 192 in code (the "64" in earlier text is stale).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Feature 1 — Step chain.** @step-chain.plan.md | Its final task | Not Started | — | Pure logic and fake stages. Startable as soon as DiaDeferredStep F1 Task 1 ships the headers |
| 2 | **Feature 2 — Module & cadence.** @module-and-cadence.plan.md | Its final task | Not Started | — | Includes `SetDeferred` / `set_mode` (S-2.5 F1 amendment) and the retry-a-missed-week rule |
| 3 | **Feature 3 — Save & history signal.** @save-and-history.plan.md | Its final task | Not Started | — | Includes the Dia `ValidatePending` edit, the Dia lazy-attach edit (confirm first), `latest_day` and the `RestoreRan` chain method |
| 4 | **System close-out (World Step only).** Defers the full suite, Release build and cross-system checks to row 6 (Weather-stack close-out). Run here, scoped: `dia run googletest --filter="WorldStep*" --shards 2`; the deferred-equals-synchronous oracle over 52 weeks and a save-mid-step-then-load run with two real-shaped fake stages (executed in row 6); `dia docs registry`; observation scan across all five pillars reported in notes. No sanitizer (on request only, user, 2026-11-17). (2026-11-17, speed-ups) | All of the above | Not Started | haiku | Run and report only. Then `dia docs spec-done` for each feature and the system once every child is done |
| 5 | **Hand off the CoW follow-ups.** Backlog rows or spec notes for: A4 F3, A5 F3 and A1 F7 registering as stages `weather`, `hydrology` and `ecology` (A5 added 2026-11-17, A5 Hydrology); the Century-tick measurement task A1 owes (system Open Q1); `HistoryDriverModule` as a listener (S-3.5 F1); GameSpeedController is **not** the key source; S-3.5 declares the instance with all three stages (amended 2026-11-17: was both); S-2.6 Rivers & Floods declares `["weather", "hydrology"]` | — | Done | haiku | Docs only. Do not edit the GDD (feedback_gdd_edits_via_gdd_session); Backlog items and spec notes updated: stage IDs corrected to weather/hydro/ecology (not hydrology) throughout; amendments section updated to reference listener interface and ValidatePending; A5 Hydrology backlog row already has hydro stage ID; Century-tick measurement is A1's system Open Q1 in a1-ecology.md; HistoryDriverModule as listener is S-3.5 F1 responsibility; GameSpeedController confirmed not the key source (WorldStepModule is); S-3.5 declares instance with all three stages; S-2.6 will declare [weather,hydro]. |
| 6 | **Weather-stack close-out.** Run once when A4, A5, World Step and S-2.5 feature plans are Done. (a) Full `dia run googletest --shards 2`. (b) The `SLOW_` suites once: `SLOW_S25Verdict`, A5 `SLOW_HydrologyReach.*` (ReachLongRun, ReachCoupledA4A5), `SLOW_HydrologyBalance.*`, `SLOW_HydrologyPipelineConservation.*` (run with `dia run googletest --filter="SLOW_*" --shards 2`; SLOW_ goes on the suite name, which is what the CLI's default exclusion matches), any A4 `SLOW_` regressions. (c) **One Release build** of GoogleTests covering the Release compile checks moved from feature plans (A4 weather-debug `DIA_DEBUG`-only absence, A5 hydrology-debug same, A4/A5 save size in Release if wanted, report only; World Step Release-path chain/listener re-entrancy tests); Release perf reported for information only. (d) Cross-system checks: deferred-vs-synchronous oracle over `["weather"]`, `["weather","hydrology"]`, `["weather","hydrology","ecology"]` (when A1 is in); mid-step save/reload bit-identical across all stages. (e) `dia check deps`; `dia docs precommit`; `dia check spec-sync`. Report pass/fail + failing names only. No sanitizer. | All green | Not Started | haiku | One combined run for A4, A5, World Step, S-2.5 (and S-2.6 when specced) (user, 2026-11-17) |

## Open items carried from the specs

| Source | Item | When to decide |
|---|---|---|
| F2 ODQ 1 | Is the key one sim step stale? Lazy `SetKeySource` read resolves timing (F2 amendment 1); variant B (GameSpeedController by `ModuleRef`) stays as fallback | F2 first task verifies |
| F2 ODQ 3 | Degraded vs silent for a retired stage | Revisit if S-3.5 finds it noisy |
| F3 ODQ 2 | Resolved by amendment 5 (2026-11-16): lazy attach on first `DoUpdate` (F3 task 11, Dia, confirm first) | Resolved |
| F3 ODQ 3 | Resolved by amendment 1 (2026-11-16): `latest_day` saved (AC 21, 22) | Resolved |
| System ODQ 1 | The Century tick has never been measured and may stall SimPU once per ~100 game years | A1's plan task; reopens the two-channel option only if measured badly |
| S-2.5 F1 | `SetDeferred` idle-only rule: confirm it is cheap before F2 is closed | F2 plan |

## Open-items review (2026-11-16)

All accepted by the user on 2026-11-16. These supersede the matching "Open items" lines in the feature plans.

| # | Decision |
|---|---|
| 9 | A retired stage reports Degraded (not silent); revisit if S-3.5 finds it noisy |
| 15 | `Configure` while a step is in flight: the chain refuses (returns false) and the module logs the Error |
| 17 | `RegisterStage` beyond 8 stages returns false, the same cap as `Configure` (`kMaxStages = 8`) |
| C | Settled by running: key-source ordering hazard (F2 Task 1); if the fallback is needed it adds a `GameSpeedControllerModule` dependency then; Century tick cost is A1's measurement task |
| A.1 | **New scheduled task, between World Step F3 and S-2.5 F1: build `HistoryDriverModule`** (the S-3.5 Feature 1 driver: world-step listener mode plus clock mode). It gates S-2.5's step-back, timeline, history export and Feature 5. Owned by S-3.5 F1; schedule it first in that feature |

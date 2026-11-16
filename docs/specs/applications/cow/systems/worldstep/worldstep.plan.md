# Plan: World Step (system)

**Spec:** @docs/specs/applications/cow/systems/worldstep/worldstep.md
**Status:** Not Started

System-level tracker. Each row is a feature spec with its own plan; the detailed tasks live there. The three weather systems share one cross-system order: see `a4-weather.plan.md` and `s25-weatherpatterns.plan.md`.

## Build order and parallelism

```
F1 step chain ──► F2 module & cadence ──► F3 save & history signal
(pure, fakes)     (needs DiaDeferredStep      (needs DiaDeferredStep F3 and the
                   F1, F2, F4 built)           ValidatePending edit)
```

- **F1 needs only DiaDeferredStep F1 Task 1** (`StepKey`, `IParallelExecutor` headers). It can start the moment those headers ship, in parallel with the rest of DiaDeferredStep.
- **F2** needs F1 and the DiaDeferredStep scheduler, capped executor and `DeferredStepModule` (F1, F2, F4). Its first task proves whether `SimulationTick` reaches the module before `DeferredStepModule::DoUpdate` in the same sim step.
- **F3** needs F2 and DiaDeferredStep F3 (`DeferredStepSaveParticipant`). It contains the one Dia code edit, `IDeferredStep::ValidatePending`.
- World Step is consumed by A4 F3 (stage registration) and by every world-sim stage; A4 F3 can use a fake `WorldStepModule` until F2 exists.

## Sign-offs (AD-001)

| Change | Where | Status |
|---|---|---|
| `IDeferredStep::ValidatePending` (defaulted virtual) and its call in `DeferredStepSaveParticipant::Validate` | DiaDeferredStep | **Given 2026-11-16**; the edit is a task in F3's plan, with a Dia test |
| `WorldStepModule::SetDeferred` and the `cow.worldstep.step` `action` argument | CoW-Shared, not Dia | No sign-off needed; **owned by S-2.5 F1's need, built in F2** (see Open items) |

## Cross-feature rules

- **Verification cadence** (feedback_verification_cadence): scoped `dia run googletest --filter=... --shards 2` per task; sanitizer runs where a spec asks (F1 mid-step unregister, F2 lifecycle, F3 save path). Full suite and the cross-feature checks run once, in the system close-out.
- **TDD** for all new public API, RED proven. **No mutation checks.** **Gap audit** closes every feature plan. **Commit after each task**, plan row updated in the same commit.
- **Model column**: haiku and sonnet only; ask first if opus seems needed (feedback_confirm_opus).
- **Engine boundary**: CoW code under `Cluiche/CoW/Shared/WorldStep` and `Cluiche/CoW/Modules`. The `ValidatePending` edit is the only Dia change and is tagged as such.
- **Name budget**: two sim-hosted names (`cow.worldstep.status`, `cow.worldstep.step`), registered all-or-nothing. The per-PU cap is 192 in code (the "64" in earlier text is stale).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **Feature 1 — Step chain.** @step-chain.plan.md | Its final task | Not Started | — | Pure logic and fake stages. Startable as soon as DiaDeferredStep F1 Task 1 ships the headers |
| 2 | **Feature 2 — Module & cadence.** @module-and-cadence.plan.md | Its final task | Not Started | — | Includes `SetDeferred` / `set_mode` (S-2.5 F1 amendment) and the retry-a-missed-week rule |
| 3 | **Feature 3 — Save & history signal.** @save-and-history.plan.md | Its final task | Not Started | — | Includes the Dia `ValidatePending` edit and the `RestoreRan` chain method |
| 4 | **System close-out.** Full `dia run googletest --shards 2`; `dia check sanitizer`; Release build; the deferred-equals-synchronous oracle over 52 weeks and a save-mid-step-then-load run with two real-shaped fake stages; `dia check deps`; `dia docs registry`; observation scan across all five pillars reported in notes | All of the above | Not Started | haiku | Run and report only. Then `dia docs spec-done` for each feature and the system once every child is done |
| 5 | **Hand off the CoW follow-ups.** Backlog rows or spec notes for: A4 F3 and A1 F7 registering as stages `weather` and `ecology`; the Century-tick measurement task A1 owes (system Open Q1); `HistoryDriverModule` as a listener (S-3.5 F1); GameSpeedController is **not** the key source; S-3.5 declares the instance with both stages | — | Not Started | haiku | Docs only. Do not edit the GDD (feedback_gdd_edits_via_gdd_session) |

## Open items carried from the specs

| Source | Item | When to decide |
|---|---|---|
| F2 ODQ 1 | Is the key one sim step stale? Falls back to reading GameSpeedController state by `ModuleRef` | F2 first task |
| F2 ODQ 3 | Degraded vs silent for a retired stage | Revisit if S-3.5 finds it noisy |
| F3 ODQ 2 | Participant order is a manifest-order contract; a priority field on `ISaveable` is the proper fix and is a Dia change not proposed | If the ordering test proves fragile |
| F3 ODQ 3 | Also save `latestDay` in the pending record (recommended: yes) | F3 plan, before the record format is fixed |
| System ODQ 1 | The Century tick has never been measured and may stall SimPU once per ~100 game years | A1's plan task; reopens the two-channel option only if measured badly |
| S-2.5 F1 | `SetDeferred` idle-only rule: confirm it is cheap before F2 is closed | F2 plan |

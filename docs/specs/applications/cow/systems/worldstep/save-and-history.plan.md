# Plan: World Step Save & History Signal (World Step Feature 3)

**Spec:** @docs/specs/applications/cow/systems/worldstep/save-and-history.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/worldstep/worldstep.plan.md (row 3)

**Start condition:** World Step Feature 2 (`module-and-cadence.plan.md`) is Done, and DiaDeferredStep Feature 3
(`DeferredStepSaveParticipant`, `RestorePending`, drain on `OnSaveStarted`, discard on `OnLoadStarted`) is built.
The stages' own `SerializePending` (A4 Feature 4, A1 Feature 8) are **not** required: this feature is tested with
fake stages that serialize a small checksum.

**This feature contains one Dia edit (task 1).** `IDeferredStep::ValidatePending(LoadContext&) const` is an
Approved amendment: **AD-001 sign-off was GIVEN 2026-11-16**. The spec text for it is already in DiaDeferredStep's
`scheduler-core.md` and `save-load.md` (AC 12a); only the code and its tests remain. It is an additive, defaulted
virtual (returns true), so no existing step changes behaviour. Task 1 is deliberately its own row, gated on the
DiaDeferredStep Feature 3 code existing, and is the only task that edits `Dia/`. Every other task is CoW code.

The chain amendment `RestoreRan` is built by Feature 1 task 12; this plan only exercises it (task 9).

## Implementation Patterns

### Files
```
Cluiche/CoW/Shared/WorldStep/IWorldStepListener.h        new: WorldStepApplied, IWorldStepListener
Cluiche/CoW/Modules/WorldStepModule.h/.cpp               edited: Serialize/Deserialize/ValidatePending, listeners, Apply
Cluiche/CoW/Shared/WorldStep/Testing/FakeWorldStepStage.h edited: checksum Serialize/DeserializePending, listener fake
Tests/GoogleTests/CoW/WorldStep/TestWorldStepSave.cpp
Tests/GoogleTests/CoW/WorldStep/TestWorldStepHistorySignal.cpp
Dia (task 1 only): IDeferredStep.h, the DeferredStepSaveParticipant Validate path, and their existing test files
```
New files added with `dia docs vcxproj-add`.

### Record format (spec "Pending record")
```json
{ "world_version":1, "step_day":412, "pending_day":-1, "stage_count":2,
  "stage_0":{"id":3471002234, ...}, "stage_1":{...} }
```
- `SaveContext` has no `uint64_t` and no array writer, so stage ids are `int64_t` (the participant's existing
  convention) and stages are numbered objects `stage_N`. Study `SaveContext`/`LoadContext` and the
  `DeferredStepSaveParticipant` writer first and reuse its helpers; do not invent a new serialization style.
- Written in `chain.StageAt(i)` order, enabled stages only; `step_day = mStepDay`, `pending_day = mPendingDay` (or -1).
- Each stage writes into its own sub-object through the `SaveContext` scope the module hands it; the module
  writes `id` before delegating, the stage writes its own keys.
- `world_version = 1`, a `constexpr`.

### Validate (pure, no mutation)
`ValidatePending(LoadContext&) const` reads and checks only. It returns false and logs **one** Error naming the first
offending key and, where relevant, the stage id for: `world_version > 1`, missing or wrong-typed field,
`stage_count` != number of `stage_N` objects, duplicate stage id, **a stage currently in `order` with no blob**.
A blob for a stage not in `order` passes. Test AC 10 by snapshotting `mStepDay`, `mPendingDay`, the chain phase and
the fake stages' committed state before and after a failing validate and comparing.

### Deserialize
Order is fixed by the spec: per-stage `DeserializePending` in declared order (blobs for non-`order` stages skipped
with **exactly one** Warning naming the id) -> `mStepDay`/`mPendingDay` -> `chain.RestoreRan(step_day)` (Error log
if false). The scheduler's `RestorePending` is called by the participant afterwards, not by the module. A stage
whose `DeserializePending` fails after Validate logs an Error, marks health Failing, and its own `IsReady()` goes
false; the load is not rolled back.

### Apply tolerance
`Apply()` calls `chain.Commit()`; if it returns false (restored channel whose chain stayed Idle) log once, raise
no signal, update no metric, and leave the next kick acceptable.

### Listener
`IWorldStepListener` as the spec, `WorldStepApplied { StepKey applyKey; StepKey stepDay; uint32_t stageCount; }`.
The module keeps a fixed array of 4 raw non-owning pointers (PD-004; no STL). Raised from `Apply()` after
`chain.Commit()` returns, in registration order; the synchronous-mode path calls the same private
`RaiseApplied(applyKey, stepDay)` with `applyKey == stepDay`. A re-entrancy guard (`mRaising` flag) makes
`RegisterListener`, `UnregisterListener`, `RegisterStage` and a kick from inside a listener a
`DIA_ASSERT` in debug and a no-op returning false in release. No allocation in the no-listener path.
No listener or sampling logic lives in the module beyond this.

### Test rig
Real `DeferredStepScheduler` over DiaDeferredStep's synchronous test launcher, a real `DeferredStepSaveParticipant`
attached to a real `SaveManager`/`SaveRegistry` on a temp slot, two `FakeWorldStepStage` that serialize their
working copy as a checksum, one real jsoncpp text round trip (the `int64`/key conventions are the risk). The
Feature 2 `WorldStepTestRig.h` is reused and extended.

### Verification cadence
- Per task: `dia run googletest --filter="WorldStepSave*:WorldStepHistory*:WorldStepModule*:WorldStepCadence*:WorldStepLifecycle*" --shards 2`.
- Task 1 additionally runs the DiaDeferredStep suites: `dia run googletest --filter="DeferredStep*" --shards 2`.
- One full run near the end; sanitizer over the mid-step-unregister-plus-load scenario.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Confirm start conditions (Feature 2 Done; DiaDeferredStep Feature 3 built: `DeferredStepSaveParticipant`, `RestorePending`; Feature 1 task 12 `RestoreRan` Done). Create the listener header and the two test files; add with `dia docs vcxproj-add`; build `dia run googletest --filter="WorldStep*" --shards 2` | Builds; no new tests | Not Started | haiku | Mechanical, TDD-exempt. BLOCKED if DiaDeferredStep Feature 3 code is absent |
| 1 | **Dia amendment, AD-001 signed off 2026-11-16, edits Dia code.** In DiaDeferredStep: add `virtual bool ValidatePending(Dia::SaveGame::LoadContext&) const { return true; }` to `IDeferredStep.h`; make `DeferredStepSaveParticipant::Validate` call it for every saved channel whose step is registered, returning false (aborting the load, mutating nothing) when any returns false. Add the tests to the existing DiaDeferredStep save/load test file. RED first: a step overriding it to false must make `Validate` fail | DiaDeferredStep `save-load.md` AC 12a (and AC 13: `Validate` mutates nothing); default-returns-true leaves every existing DeferredStep test green | Not Started | sonnet; ask before escalating | **Dia edit. AD-001: SIGNED OFF 2026-11-16 (the amendment is already written into scheduler-core.md and save-load.md).** Touch only `ValidatePending` and the one call site. Run the whole `DeferredStep*` filter, not only the new test. Tag the commit as a Dia change. If the code diverges from the spec text (e.g. const-ness), STOP and flag rather than adapt |
| 2 | RED then GREEN: extend `FakeWorldStepStage` with checksum `SerializePending`/`DeserializePending` and a `FakeWorldStepListener` that records `WorldStepApplied` and, from inside the callback, reads each stage's committed value and optionally calls back into the module | Support for AC 15, 20; the fake round-trips its checksum through a real `SaveContext`/`LoadContext` | Not Started | sonnet | Test support ships with the library (`Testing/`). Prove RED with the compile failure |
| 3 | RED then GREEN: `IWorldStepListener` + `RegisterListener`/`UnregisterListener` (capacity 4, no duplicates), no-op and no allocation with zero listeners | AC 19 (four in registration order, fifth false, duplicate false, no listener: no-op, no allocation) | Not Started | sonnet | Allocation proof by capacity/pointer stability or the project's allocation hook, as in Feature 1 AC 20 |
| 4 | RED then GREEN: the signal raised from `Apply()` after `chain.Commit()` in deferred mode and from the inline path in synchronous mode | AC 14 (`stepDay`, `applyKey`, `stageCount`), 15 (listener reads every stage's committed value inside the signal), 17 (retried week carries the **missed** `stepDay` and the later `applyKey`), 18 (synchronous: `applyKey == stepDay`) | Not Started | sonnet | Reuses Feature 2's retry machinery; do not reimplement it |
| 5 | RED then GREEN: signal suppression and `Apply` tolerance: never raised on abandon, unregister-mid-step discard, a save drain, `DoStop` or a failed load; `Apply` tolerates `chain.Commit()` returning false (log once, no signal, no metric, next kick accepted) | AC 16 (all five suppression cases; the restored-step-raises half is added in task 10), 20a | Not Started | sonnet | A drain does not apply (DiaDeferredStep Feature 1 AC 26), so a drain test must show zero signals |
| 6 | RED then GREEN: listener re-entrancy guard: a listener calling `RegisterListener`, `UnregisterListener`, `RegisterStage` or a kick from inside the signal trips the debug assert and is a no-op returning false in release | AC 20 | Not Started | sonnet | Test the release path with the assert compiled out or with the project's assert-capture hook, whichever Feature 1 task 8 used |
| 7 | RED then GREEN: `SerializePending` record writer | AC 1 (all five fields and one `stage_N` per enabled stage in declared order, each with its id), 2 (`pending_day` set vs -1), 3 (registered-but-not-in-`order` stage and retired stage write no blob) | Not Started | sonnet | Parse the produced text with jsoncpp in the test and assert on keys and `int64` ids, not on string compares |
| 8 | RED then GREEN: `ValidatePending` on the module (uses the Dia hook from task 1) | AC 8 (missing blob for an in-`order` stage fails, names it, no participant state changed), 9 (`world_version` 2; missing `step_day`; non-integer `stage_count`; `stage_count` mismatch; duplicate id; each names the key), 10 (nothing mutated, before/after comparison), plus a blob for a non-`order` stage passes | Not Started | sonnet | Table-driven fault rows. Depends on task 1; if task 1 is blocked, STOP, do not implement the fallback (AD-001 was granted) |
| 9 | RED then GREEN: `DeserializePending` + `RestoreRan` use: per-stage delegation in declared order, one Warning for each non-`order` blob, `mStepDay`/`mPendingDay`, `chain.RestoreRan`, Error and Failing on a post-Validate stage failure | AC 6 (`["weather","ecology"]` save loads on `["weather"]`, exactly one Warning naming ecology), 7 (phase `Ran`, `Apply` commits, next kick accepted), 13 (`RestoreRan` false when not Idle, not ready, or already captured; no stage method called) | Not Started | sonnet | AC 13 is mostly Feature 1 AC 27; here it is asserted again through the module. Warning count asserted exactly |
| 10 | RED then GREEN: restored-step signal and pending week: a restored step applied after a successful load raises the signal once; a restored `pending_day` kicks on the first free tick with the same represented day | AC 5, AC 16 (restored step raises once) | Not Started | sonnet | The driver's own discard of post-load signals is S-3.5's job, not tested here |
| 11 | RED then GREEN: participant ordering contract: with the DeferredStep participant attached **before** the stage's state participant a fake stage's committed-state check fails; with the correct order it passes | AC 12 | Not Started | sonnet | See Open items 1: the manifest-order wording may contradict. If the test cannot be made to pass with both the Feature 2 manifest order and this rule, stop and report |
| 12 | RED then GREEN: the primary DFS-005 test: save mid-step with two fake stages through real `SaveManager`/`SaveRegistry` and jsoncpp text, load into a fresh module with the same config, run N further weeks, compare stage state byte for byte with an uninterrupted run; also the restored step applies at its original apply key | AC 4 | Not Started | sonnet | Covered once, here, for the coordinator's delegation (spec Test Plan). Include a save taken with a retried week pending |
| 13 | Spec housekeeping owed on approval (verify first, apply only what is missing): S-3.5 `stage-and-run-mode.md` AC8 and the line near 119 (label by `stepDay`, register as a listener); the system spec's public interface, History sampling section, pending-record format, and replacement of `HistoryDriverModule::OnWorldStepApplied(key)` with the listener; confirm DiaDeferredStep `scheduler-core.md`/`save-load.md` carry `ValidatePending` | Spec text diff reviewed; no code | Not Started | haiku | Docs only. Use `dia docs` commands where applicable. Never edit the DesignDocs GDD (feedback_gdd_edits_via_gdd_session). Anything beyond these edits goes to Open items |
| 14 | Exhaustive test pass: gap audit of AC 1-20 and 20a against test names, plus edge cases: a save with zero stages enabled; `pending_day` set on a step with `step_day` earlier; a load into a module whose stage is not ready (`RestoreRan` false path end to end, Apply tolerance); load twice in a row; save taken with no step in flight (participant writes nothing for the channel); a listener unregistering after the raise; stage ids near the `int64` boundary (high-bit `StringCRC`) | Gap list in Notes; edge tests green | Not Started | sonnet | No mutation check. AC 11 is intentionally unplanned (see Open items 2) |
| 15 | Sanitizer: `dia check sanitizer` scoped to `WorldStepSave*:WorldStepHistory*:WorldStepLifecycle*`, covering the mid-step unregister plus load path | ASan/UBSan clean | Not Started | haiku | DiaCLI run-and-report only. Also run it on the `DeferredStep*` filter since task 1 touched Dia |
| 16 | Observation Opportunity Scan across all touched files and the five DiaObservation pillars (logs, traces, profiling, metrics, health). Suggestions only | Suggestions recorded in Notes | Not Started | sonnet | Candidates: a trace zone around Serialize/Deserialize, a counter for signals raised, health on validate refusals. Does not block |
| 17 | One full verification run: `dia run googletest --shards 2`, `dia check deps`; report pass/fail and failing names only. Release perf gates reported, not enforced | All green, including every `DeferredStep*` test | Not Started | haiku | The only unscoped run in this plan |
| 18 | Close-out (do **not** run now): `dia docs plan docs/specs/applications/cow/systems/worldstep/save-and-history.plan.md <n> --status Done` per row; `dia docs spec-done docs/specs/applications/cow/systems/worldstep/save-and-history.md`; update system plan row 3; add a line to DiaDeferredStep's plan notes that `ValidatePending` landed; commit with Dia changes in a separate, labelled commit | Spec Done, plan Done, system plan updated | Not Started | haiku | Listed for the implementer; executes only after task 17 is green |

## Open items

Spec silences and tensions, recorded and not resolved here.

1. **Participant order vs manifest order (a likely contradiction).** The spec says the DeferredStep participant must
   deserialize after every stage's own state participant, because "`DeferredStepModule` attaches its participant after
   `SaveLoadModule` and after the stage modules". But Feature 2's manifest order lists `DeferredStepModule` **before**
   `WorldStepModule` and the stage modules (reverse-stop reasoning). Unless the participant is attached lazily or at
   a later point than `DoStart`, the two rules conflict. Task 11 will expose it; Open Design Question 2 already
   notes the fragility and the missing `ISaveable` priority (a Dia change, not proposed).
2. **AC 11 (fallback if the amendment is declined) is not planned**: the amendment was signed off, so the fallback is
   dead code. Revive it only if sign-off is withdrawn.
3. **`latestDay` in the pending record is undecided.** Open Design Question 3 recommends saving it (so the key is not 0
   on the first frame after a load) but the record format and the ACs omit it. Not planned; one extra field and test
   if the user confirms.
4. **Validate's "stage currently in `order`" vs "enabled".** A stage that is in `order` but currently unregistered
   (retired) has no blob by design (AC 3) yet an in-`order` stage with no blob fails Validate. The spec does not say
   which set Validate checks; a retired stage would make a legitimate save un-loadable on a degraded host.
5. **`SaveContext` nesting API.** The spec says there is no array-element writer so stages use numbered objects, but it
   does not name how a stage obtains its sub-object scope from the module. Task 7 follows whatever the participant does.
6. **Stage-failure after Validate** is "logs an Error and marks Failing" but the module's health is specified in
   Feature 2 as Failing for missing stages only. A new Failing cause is introduced here without a Feature 2 amendment.
7. **Edits to Dia specs:** the spec's Files Touched says `ValidatePending` is added to `scheduler-core.md`/`save-load.md`
   "once signed off". Both files already contain it, so task 13 is verification only.
8. **AC numbering:** 20a precedes 20 in the spec.
9. **Restored `pending_day` with no in-flight step after load.** After a load the channel is complete-not-applied;
   whether the retry fires before or after that apply is implied (on the next free tick) but not tested by an AC;
   task 10 asserts the literal reading.

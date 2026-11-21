# Feature Spec: World Step Save & History Signal

**Parent:** @docs/specs/applications/cow/systems/worldstep/worldstep.md (Feature 3)
**Status:** `Done` (2026-11-20)
**Depends on:** World Step Features 1 and 2; DiaDeferredStep Feature 3 (`DeferredStepSaveParticipant`, `RestorePending`, drain on `OnSaveStarted`, discard on `OnLoadStarted`); A4 Feature 4, A5 Feature 4 and A1 Feature 8 for the stages' own `SerializePending` (A5 added 2026-11-17, A5 Hydrology).
**Dia amendment (AD-001, signed off 2026-11-16):** `IDeferredStep::ValidatePending(LoadContext&) const` (see Design, Validate).
**Consumed by:** S-3.5 Feature 1 (`HistoryDriverModule` is the first listener), A4 Feature 4, A1 Feature 8.

## Summary

Two small, load-bearing seams on the module from Feature 2:

1. **Save.** `WorldStepModule::SerializePending` / `DeserializePending` delegate to each enabled stage in declared order, tag each blob with its stage id, and persist the module's own step bookkeeping, so a save taken mid-step loads and continues identically to the uninterrupted run (DFS-005). The module writes no participant of its own and E1 needs no change.
2. **History signal.** After every stage has committed, the module raises exactly one synchronous `OnStepChainApplied` to registered listeners, carrying both the day the step stands for and the day it became visible. The module contains no sampling logic.

## Goals

1. Save mid-step, load, continue: stage state after N further weeks is bit-identical to a run that never saved (the primary DFS-005 test, covered once for the coordinator's delegation).
2. A save with a stage that is no longer in `order` loads, with that stage's blob discarded and a Warning.
3. A save missing a blob for a stage that **is** in `order` is **refused** before any state changes, naming the stage (decided 2026-11-16).
4. Exactly one `OnStepChainApplied` per applied step, after every stage has committed, never on abandon, discard, drain or `DoStop`.
5. History samples are labelled by the **step day** (the day the step represents), so Year/Decade/Century bands come out right and a retried week cannot mislabel (decided 2026-11-16).

## Non-Goals

- The scheduler's drain, the `ISaveable`, the pending-result record (DiaDeferredStep Feature 3).
- Stage serialization (A4 Feature 4 `SerializePending` of the working set; A5 Feature 4; A1 Feature 8). A5's `hydrology` blob follows the same stage-id tagging and the same absent-from-`order` and missing-blob rules (amended 2026-11-17, A5 Hydrology).
- `HistoryDriverModule`'s sampling, label arithmetic and reset on bundle load (S-3.5 Feature 1).
- Any change to E1 (`SaveLoadModule`).

## Design

### Pending record

`WorldStepModule::SerializePending(ctx)` is called by the DeferredStep participant for the `world` channel, after `DrainAll`, so the step is complete and in the `Ran` phase. The module writes into the channel's object:

```json
{
  "world_version": 1,
  "step_day": 412,
  "pending_day": -1,
  "stage_count": 2,
  "stage_0": { "id": 3471002234, "...": "stage data" },
  "stage_1": { "id": 118840551,  "...": "stage data" }
}
```

- `step_day` is `mStepDay`, the day the step represents (not the apply key, which the participant stores).
- `pending_day` is `mPendingDay` or `-1`. A retried week exists only while a step is in flight, so it is saved exactly when it can be non-empty; after a load it is restored and kicked on the next free tick, so **no week is lost across a save** (matches the uninterrupted run).
- Stage entries are written in declared order. Each `id` is the `StringCRC` as `int64_t` (the participant's convention; `SaveContext` has no `uint64_t` writer and no array-element writer, so numbered objects).
- If no step is in flight there is nothing to write and the participant writes nothing for the channel (DiaDeferredStep rule). Synchronous mode never reaches this code.

### Load

`DeferredStepSaveParticipant` runs, in order: discard on `OnLoadStarted`, **Validate** for every participant, then **Deserialize**.

**Validate** (the Dia amendment). `IDeferredStep` gains a defaulted hook, `virtual bool ValidatePending(LoadContext&) const { return true; }`, which the participant's `Validate` calls for each saved channel. It must not mutate anything. `WorldStepModule::ValidatePending` returns false, with the first offending key and stage id in an Error log, when:

- `world_version` is greater than 1, or a field is missing or the wrong type;
- `stage_count` does not match the number of `stage_N` objects;
- an id appears twice;
- **a stage currently in `order` has no blob** (refuse the load; a step restored for only some stages would commit a half-world);
- **a present stage blob fails that stage's `IStepStage::ValidatePending`** (amended 2026-11-18, World-Sim Kit: the module calls it, read-only, on each in-`order` stage's sub-object, so a corrupt stage body is refused before any Deserialize instead of being found after Validate).

A blob for a stage that is **not** in the current `order` is not a validation failure.

**Deserialize**, SimPU, after Validate succeeded:

1. For each stage in declared order, if it has a blob call `stage.DeserializePending(ctx)` on that stage's sub-object. A blob for a stage not in `order` is skipped with exactly one Warning naming the id (a save from S-3.5 loads on S-2.5).
2. `mStepDay = step_day`; `mPendingDay = pending_day`.
3. `chain.RestoreRan(step_day)`: a new, narrow chain method (Feature 1 amendment) that moves the chain from `Idle` to `Ran` with the given key without calling any stage, so the scheduler's `Apply()` → `chain.Commit()` is a legal sequence. It returns false (and the module logs an Error) if the chain is not `Idle` or not `AllReady()`.
4. The participant then calls `scheduler.RestorePending(world, kickKey, applyKey)`. The step applies on the first `Tick` with `currentKey >= applyKey`.

**Apply tolerance (amended 2026-11-16).** If `chain.RestoreRan` failed (a stage could not restore, or was not ready), the scheduler's restored channel still reaches its apply key and `Apply()` calls `chain.Commit()`, which returns false from `Idle`. `Apply` treats that as a no-op: Warning, no signal, no metric update.

**Ordering requirement.** The pending result applies on top of committed stage state, so each stage's own committed-state participant (A4 `WeatherSaveParticipant`, A5's, A1's; A5 added 2026-11-17, A5 Hydrology) must deserialize **before** the DeferredStep participant. Participants deserialize in registration order, so `DeferredStepModule` attaches its participant after `SaveLoadModule` and after the stage modules. A test asserts it (a deliberately wrong order fails on a fake stage that checks its committed state exists).

If `Validate` passes but a stage's `DeserializePending` still reports failure (a corrupt body the module could not pre-check), the stage returns false through its own health and the module logs an Error and marks Failing. The load is not rolled back at that point (it cannot be, past Validate); the stage's `IsReady()` goes false so no further kicks occur.

If the Dia amendment is declined, the fallback is: no `ValidatePending`; the missing-blob case is detected in `DeserializePending`, which discards the whole pending step (`scheduler` channel reset to idle, `chain` stays `Idle`) and logs an Error. That loses one week and breaks bit-identity for that load, which is why the amendment is preferred.

### History signal

A listener interface in CoW-Shared, so the module has no compile-time dependency on `HistoryDriverModule` (which S-3.5 Feature 1 builds later):

```cpp
namespace CoW::WorldStep {
    struct StepChainApplied {
        StepKey applyKey;      // the day the result became visible
        StepKey stepKey;       // the day the step stands for (what the stages computed)
        uint32_t stageCount;   // enabled stages that committed
    };
    class IStepChainListener {
    public:
        virtual ~IStepChainListener() = default;
        virtual void OnStepChainApplied(const StepChainApplied& applied) = 0;   // SimPU, synchronous
    };
}
// on WorldStepModule
bool RegisterListener(IStepChainListener& listener);   // false if 4 are registered or it is a duplicate
void UnregisterListener(IStepChainListener& listener);
```

- Raised from `Apply()`, **after** `chain.Commit()` returns, once, to each listener in registration order. In synchronous mode it is raised from the same inline path and `applyKey == stepDay`.
- Not raised when a step is abandoned, discarded on unregister, drained for a save (a drain does not apply), or at `DoStop`. A restored step applied after a load **is** raised (it is an applied step); the driver discards signals that arrive after a bundle load, as its own spec already says.
- A listener must not call back into the module (no `RegisterListener`, `RegisterStage` or kick from inside the signal). This is asserted in debug builds.
- With no listener the signal is a no-op and nothing else changes.
- `stageCount` lets a listener see a world step with a retired stage (fewer than configured).

**Label rule for listeners (S-3.5 amendment).** The driver labels the sample by `stepDay` and keeps `applyKey` as the visible-at day. For the sample's band, the driver uses the band of `stepDay` (Year, Decade, Century on boundaries) and `history.band` filtering applies to that band. This replaces S-3.5's "uses the apply key, not the current tick" with "uses the step day".

## Public API

New: `Dia/DiaDeferredStep/IStepChainListener.h`. Edited: `WorldStepModule` (`SerializePending`, `DeserializePending`, `ValidatePending`, `RegisterListener`/`UnregisterListener`), `StepChain` (`RestoreRan`). Dia: `IDeferredStep::ValidatePending` (amendment, see above).

## Acceptance Criteria

### Pending record
1. After `DrainAll` with a step in flight, `SerializePending` writes `world_version`, `step_day`, `pending_day`, `stage_count` and one `stage_N` per enabled stage in declared order, each with its id.
2. With a retried week pending, `pending_day` holds it; otherwise it is `-1`.
3. Enabled stages only: a registered stage not in `order` writes no blob. A retired stage writes none.

### Load: success paths
4. Save mid-step with two fake stages, load into a fresh module with the same config: the restored step applies at its original apply key, and after N further weeks the stage state is bit-identical to an uninterrupted run.
5. A restored `pending_day` is kicked on the first free tick after the load, with the same represented day it had.
6. A save taken on `["weather","ecology"]` loads on `["weather"]`: the ecology blob is skipped with exactly one Warning naming it, and the load succeeds.
7. After a load that restores a pending step, `chain.Phase()` is `Ran`, `Apply()` commits it, and the next kick is accepted.

### Load: refusals
8. A save lacking a blob for a stage that is in `order` fails `Validate` with that stage named; **no** participant state has changed after the failed load.
9. Each of these fails `Validate` and names the key: `world_version` 2, a missing `step_day`, a non-integer `stage_count`, `stage_count` not matching the objects present, a duplicate stage id.
10. `Validate` mutates nothing: the stage's committed state, `mStepDay` and `mPendingDay` are unchanged after a failed validate (checked by comparing before and after).
11. If the Dia amendment is declined, the fallback discards the pending step and logs an Error, and the next kick works.

### Ordering and phase
12. The test with a wrong participant order (DeferredStep before the stage's state participant) fails the fake stage's committed-state check; the right order passes.
13. `chain.RestoreRan(key)` returns false when not `Idle`, when not `AllReady()`, and when a step is already captured; it calls no stage method.

### History signal
14. One `OnStepChainApplied` per applied step: `stepDay` equals the represented day, `applyKey` the day applied, `stageCount` the enabled count.
15. It is raised only after every stage's `Commit` has returned (a fake listener reads each stage's committed value from inside the signal).
16. Not raised on: abandon, unregister-mid-step discard, a save drain, `DoStop`, a failed load. Raised once for a restored step after a successful load.
17. A retried week's signal carries the **missed** `stepDay` and the later `applyKey`.
18. Synchronous mode raises it inline with `applyKey == stepDay`.
19. No listener: no-op, no allocation. Four listeners get it in registration order; a fifth `RegisterListener` returns false; a duplicate returns false.
20a. `WorldStepModule::Apply` tolerates `chain.Commit()` returning false (a restored channel whose stage could not restore its pending result, A4 Feature 4): it logs once, raises **no** history signal, and the next kick is accepted (amended 2026-11-16).
20. A listener that calls back into the module trips the debug assertion (and in release the call is a no-op returning false).

## Files Touched

New: `IStepChainListener.h`; `Tests/GoogleTests/CoW/WorldStep/{TestWorldStepSave,TestWorldStepHistorySignal}.cpp`. Edited: `WorldStepModule.h/.cpp`, `StepChain.h/.cpp`. **Edits to other specs on approval:** World Step Feature 1 (`RestoreRan`); World Step Feature 2 (close Open Q2; the signal is the listener interface); the system spec (public interface, History sampling section, the pending-record format, drop `HistoryDriverModule::OnStepChainApplied(key)` in favour of the listener); S-3.5 `stage-and-run-mode.md` (AC8 and the line at 119: label by `stepDay`, register as a listener); DiaDeferredStep `save-load.md` and `scheduler-core.md` (`ValidatePending`, once signed off). No Dia code is added here; the one Dia change is the amendment above.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| DFS-005 | Saves drain, then store pending results with their apply keys | The module's blob is exactly the pending result plus its bookkeeping; the scheduler stores the keys |
| AD-001 | Ask permission before Dia edits | **`IDeferredStep::ValidatePending` signed off 2026-11-16.** A default-returning-true virtual, no behaviour change for existing steps. Fallback described above if declined |
| PD-004 | No STL in public APIs | Listener list is a fixed-capacity Dia array |
| PD-002 | Module architecture | Listener registration is a direct call on a `ModuleRef`'d module, no singleton |

## Test Plan

GoogleTests with the real `DeferredStepScheduler` over its test launcher, `DeferredStepSaveParticipant` attached to a real `SaveManager`/`SaveRegistry` on a temp slot, and two `FakeStepStage` objects that serialize their working copy as a small checksum. One save/load round trip with real jsoncpp text, not a mock, since the key and `int64` conventions are the risk. TDD order: record format, validate refusals, deserialize and `RestoreRan`, bit-identity round trip, listener. No mutation checks. Gap audit and edge pass is the last plan task. **Sanitizer** over the mid-step unregister plus load path.

## Open Design Questions

1. **Will the Dia amendment be accepted?** It is small and generic, but it touches an Approved spec. Without it, the refusal in criterion 8 degrades to a discard plus Error (criterion 11) and the DFS-005 bit-identity guarantee only holds for loads with matching stages. Confirm before the plan.
2. **Is the participant order really a contract?** The DeferredStep participant must deserialize after every stage's own state participant. `SaveRegistry` order follows `Attach` order, which follows module start order, which follows the manifest. That is the same fragility the stage ordering decision rejected for `order`. A cleaner fix is a priority field on `ISaveable`, which is a Dia change and not proposed here; for now the manifest-order rule and the failing test above carry it.
3. **Should the pending record also save `latestDay`?** The key source is rebuilt from the first `SimulationTick` after a load, so a restored step whose apply key is already `<= latestDay` applies on the first tick. That is correct, but the load-time key is momentarily 0 and `Tick(0)` applies nothing, so the apply is one tick late relative to the uninterrupted run in the degenerate case where a save is loaded exactly on its apply day. If bit-identity is the goal there, save and restore `latestDay` in the same record. Cheap, and it also gives the history driver a correct day on the first frame. Recommended default: yes, add it.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **`latestDay` is saved.** The pending record adds `latest_day` (`int64_t`), restored into `mLatestDay` on load so the key source is correct on the first frame. This is acceptance criteria 21 and 22 below; Open Design Question 3 is resolved (yes).
   21. A save writes `latest_day`; a load restores it before the first `GetCurrentKey()`.
   22. A pending step whose apply key is already `<= latest_day` applies on the first tick after the load.
2. **`Validate` checks the enabled set.** The "stage in `order` with no blob" refusal applies to stages that are **registered and in `order`** at load time (the enabled set). A stage that is configured but not registered then is not required to have a blob (the load proceeds and the restored step is a logged no-op, see the Apply tolerance rule).
3. **The Dia amendment is signed off.** Acceptance criterion 11 and the "if the amendment is declined" fallback paragraph are **not applicable** and are not planned.
4. **Stage sub-objects.** The module wraps each stage's data in `ctx.BeginObject("stage_N")` / `EndObject()` before calling `SerializePending`/`DeserializePending`; the stage writes and reads at the current node.
5. **Participant order is by lazy attach.** `DeferredStepModule` attaches its save participant on its **first `DoUpdate`** (after every module's `DoStart`), so it follows every stage participant whatever the manifest order. This is a DiaDeferredStep Feature 4 amendment (AD-001: a design change to an unbuilt Dia feature, confirmed with the user before that task). Open Design Question 2 is resolved by it.
6. **A restore failure after Validate** (a stage returns not-ready from `DeserializePending`) is Degraded health (World Step Feature 2 amendment 9).

## Status

**Status:** `Done` (2026-11-20; Approved 2026-11-16; the `ValidatePending` Dia amendment signed off, AD-001). Amended 2026-11-17 (A5 Hydrology): a third stage blob (`hydrology`); the rules are unchanged. Plan: [save-and-history.plan.md](save-and-history.plan.md) (written 2026-11-16, not started).

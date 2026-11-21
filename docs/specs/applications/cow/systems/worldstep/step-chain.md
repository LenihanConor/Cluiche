> **Moved to Dia:** the step chain (`StepChain`, `IStepStage`, `StepChainConfig`, `IStepChainListener`) now lives in DiaDeferredStep, see `docs/specs/applications/dia/systems/diadeferredstep/step-chain.md`. The names below are the pre-move CoW names; this spec is kept as history.

# Feature Spec: World Step Chain

**Parent:** @docs/specs/applications/cow/systems/worldstep/worldstep.md (Feature 1)
**Status:** `Done` (2026-11-20)
**Depends on:** DiaDeferredStep Feature 1 Task 1 (`StepKey`, `IParallelExecutor` headers). `Dia::SaveGame::SaveContext` / `LoadContext` (built) are only forward-declared. No other Dia code.
**Consumed by:** World Step Feature 2 (`WorldStepModule` wraps the chain), Feature 3 (save delegation uses `StageAt` and the stage-id tags). A4 Feature 3, A5 Feature 3 and A1 Feature 7 implement `IStepStage`. (amended 2026-11-17, A5 Hydrology)

## Summary

The pure-logic core of the world-step coordinator. `IStepStage` is the contract a world-sim layer implements. `StepChain` holds the stage table, resolves it against the declared `order`, gates on readiness, and sequences `Capture`, `Run` and `Commit` over the enabled stages in declared order. It knows nothing about modules, the bus, the scheduler, cadence, saving, or history. It is unit-tested with fake stages.

## Goals

1. Stage order is **the config's order**, independent of registration order.
2. One predicate, `AllReady()`, says whether a kick may be issued: every configured stage is registered **and** reports ready.
3. `Capture`, `Run`, `Commit` visit enabled stages in declared order and cannot be called out of sequence.
4. A stage may leave while a step is in flight without dangling anywhere inside `Run`.
5. A bad config is rejected whole, with the reason named.

## Non-Goals

- Cadence, `k`, the week gate, kick gating on in-flight state (Feature 2).
- Registering the channel with DiaDeferredStep, synchronous mode, commands, metrics, health, logs (Feature 2).
- `SerializePending` delegation and the history signal (Feature 3). The chain exposes the enabled stages in order and nothing more for it.
- Interpreting bands. A1 reads the key itself.
- Any stage failure channel. A failing stage reports it through its own health and `IsReady()` (decided 2026-11-16).

## Design

### The stage contract (`IStepStage.h`)

As the system spec, with one correction to its own text:

```cpp
namespace CoW::WorldStep {
    using Dia::DeferredStep::StepKey;
    class IStepStage {
    public:
        virtual ~IStepStage() = default;
        virtual bool IsReady() const = 0;                              // initialised and spun up; SimPU
        virtual void Capture(StepKey key) = 0;                         // SimPU
        virtual void Run(Dia::DeferredStep::IParallelExecutor&) = 0;   // worker thread(s); working copy only
        virtual void Commit() = 0;                                     // SimPU
        virtual void SerializePending(Dia::SaveGame::SaveContext&) const = 0;
        virtual void DeserializePending(Dia::SaveGame::LoadContext&) = 0;
        virtual bool ValidatePending(Dia::SaveGame::LoadContext&) const = 0;   // amended 2026-11-18 (World-Sim Kit): read-only pre-check of this stage's pending blob
    };
}
```

Threading contract, stated in the header: `IsReady`, `Capture`, `Commit`, `SerializePending` and `DeserializePending` are called on SimPU only. `Run` is called on **one** worker thread, never concurrently with `Capture`/`Commit`, and may call `ParallelFor` on the executor repeatedly. A stage's own readiness flag must not be written during `Run`.

### Config (`StepChainConfig.h`)

```cpp
struct StepChainConfig {
    StageIdList order;                 // fixed capacity kMaxStages = 8; ids are strings in the file
    uint32_t    applyAfterSteps = 7;
    bool        deferred       = true;
};
bool LoadStepChainConfig(const Json::Value& root, StepChainConfig& out, ConfigError& err);
```

The system spec's `historyBand` field is **removed** (2026-11-16): `history_band` and the band names stay `HistoryDriverModule`'s concern, as Resolved Decision 2 already says. The system spec's interface block is stale on this and is corrected in the same edit that approves this feature.

`LoadStepChainConfig` is all-or-nothing (the Feature 1 rule used across A4): on any fault it returns false, leaves `out` equal to a default `StepChainConfig`, and sets `err` to the key and reason. Faults:

- `order` missing, **empty** (decided 2026-11-16), not an array, longer than `kMaxStages`, or containing a non-string, an empty string, or a **duplicate** id.
- Two distinct ids with the same `StringCRC` (a hash collision, caught at load).
- `apply_after_steps` not an integer, `< 1`, or `> 365`.
- `deferred` not a bool.
- An unknown key at the top level (typos are invalid, as for A4's parameter file).

Ids are matched case-sensitively and are not validated against a fixed vocabulary here: `"weather"`, `"hydro"` and `"ecology"` (the world-sim system tokens; amended 2026-11-18, World-Sim Kit, was `"hydrology"`) are conventions of the stages that register them, and a wrong id surfaces as a missing stage.

### The chain (`StepChain`)

```cpp
class StepChain {
public:
    bool Configure(const StepChainConfig& config);                   // false if already configured with a step in flight
    bool RegisterStage(StringCRC id, IStepStage& stage);        // false: duplicate id, null table slot, kMaxStages hit
    void UnregisterStage(StringCRC id);

    bool AllReady() const;                                           // no missing configured stage, and every enabled stage ready
    bool MissingStages(StageIdList& out) const;                      // configured but unregistered, declared order; true if any
    bool UnknownStages(StageIdList& out) const;                      // registered but not in order; true if any (skipped, to be warned)

    bool Capture(StepKey key);                                       // false if not Idle
    bool Run(Dia::DeferredStep::IParallelExecutor& executor);        // false if not Captured
    bool Commit();                                                   // false if not Ran
    void Abandon();                                                  // discard the in-flight step: no Commit; apply deferred removals
    bool RestoreRan(StepKey key);                                    // load only (Feature 3): Idle -> Ran without calling any stage; false if not Idle or not AllReady

    ChainPhase Phase() const;                                        // Idle | Captured | Ran
    StepKey CapturedKey() const;
    uint32_t EnabledCount() const;
    IStepStage* StageAt(uint32_t index) const;                  // declared order, enabled only
    StringCRC StageIdAt(uint32_t index) const;
};
```

- **Enabled** means registered **and** present in `order`. A registered stage absent from `order` is *unknown*: never called, reported by `UnknownStages` so the module can warn once.
- **The enabled table is rebuilt on `Register`/`Unregister`/`Configure`, never on a step**, so `Capture`, `Run`, and `Commit` walk a fixed array in declared order and allocate nothing.
- **Sequencing.** The chain is a three-state machine `Idle → Captured → Ran → Idle`. A call in the wrong state returns false and does nothing (no partial effect). `Capture` captures **every** enabled stage in order, then moves to `Captured`; the chain does not require `AllReady()` itself (the module gates the kick). It is a programming error to `Capture` when `AllReady()` is false and a debug assertion fires, but the release behaviour is a no-op returning false.
- **`Run`** calls each enabled stage's `Run` in declared order on the calling thread, passing the same executor. It reads the enabled table and nothing else.
- **`Commit`** calls each stage's `Commit` in declared order. A stage's `Commit` publishes its own message (A4 `WeatherUpdated`), so by the time stage *n+1* commits, stage *n*'s result is already committed and visible.
- **Unregistering mid-step (decided 2026-11-16).** If `UnregisterStage` is called while the phase is not `Idle`, the stage is placed on a **pending-removal list** that only SimPU reads and `Run` never reads. The step runs and commits that stage as normal, because the object is still alive. Removal takes effect when the phase returns to `Idle` (after `Commit` or `Abandon`). Calling it at `Idle` removes at once. Re-registering the same id while its removal is pending cancels the removal and replaces the stage pointer **only after** the step ends; the call returns false until then.
- **`Abandon`** is the teardown path. It moves to `Idle` without calling `Commit` on anyone, then applies pending removals. It does not call any stage method. The system spec's DoStop text (abandon, no commit) is the rule.
- **Stage failure.** None is modelled. A stage that finds itself broken logs, sets its own health, and returns false from `IsReady()`. The next `AllReady()` is false, so the module stops kicking. The step already running completes. (Rejected: a `Failed` return from `Commit`, or a `bool Run`, because by then earlier stages have committed and there is no rollback story.)

### Test support (`Dia/DiaDeferredStep/Testing/FakeStepStage.h`)

`FakeStepStage` records every call as `(stageId, phase, key)` into a shared `CallLog` (a `DynamicArrayC`), supports `SetReady`, a **poisoned source** (`Capture` copies `source` to `working`; the test then overwrites `source`, and `Run` must produce from `working`), a configurable number of `ParallelFor` calls in `Run`, and a `CommitOrderProbe` that records the other fake's committed value at its own `Commit` time, to prove "earlier stage is already visible". Tests ship with the library, per the project rule.

## Public API

Files in `Cluiche/CoW/Shared/WorldStep/`: `IStepStage.h`, `StepChainConfig.h/.cpp`, `StepChain.h/.cpp`, `Testing/FakeStepStage.h`. No SimModule here.

## Acceptance Criteria

### Config
1. A valid config (`order: ["weather","ecology"]`, `apply_after_steps: 7`, `deferred: true`) loads. Missing `apply_after_steps` and `deferred` take defaults 7 and true.
2. Each of these returns false, leaves `out` default, and names the key: `order` missing; empty; not an array; 9 entries; a non-string entry; an empty-string entry; a duplicate id; `apply_after_steps` 0, 366, 7.5, `"7"`; `deferred` as `"true"`; an unknown top-level key.
3. A forced `StringCRC` collision between two different strings is rejected. (The test injects a collision through the config's hashing seam, not by finding a real one.)
4. `apply_after_steps` 1 and 365 are valid.

### Registration and resolution
5. A configured and registered stage is enabled. `EnabledCount` and `StageAt` follow **declared** order whatever order the stages registered in (register ecology first, assert weather is index 0).
6. A registered stage absent from `order` is not enabled, is never called, and appears in `UnknownStages`.
7. A configured stage that never registers appears in `MissingStages` in declared order, and `AllReady()` is false.
8. `RegisterStage` returns false for a duplicate id and when the table is full, and changes nothing.
9. `UnregisterStage` at Idle removes the stage at once; an unknown id is a no-op.
10. `Configure` called again replaces the order and rebuilds the table; stages already registered re-resolve. It returns false and changes nothing while a step is in flight.

### Readiness
11. `AllReady()` is false until every enabled stage reports ready **and** no configured stage is missing. Flipping one stage to not-ready makes it false again.
12. An unknown (not enabled) stage's readiness does not affect `AllReady()`.

### Sequencing
13. `Capture`, `Run`, `Commit` each visit all enabled stages in declared order (the shared call log shows weather, ecology for each phase).
14. `Run` before `Capture`, `Commit` before `Run`, `Capture` twice, `Run` twice and `Commit` twice each return false with **no** stage called.
15. After `Commit` the phase is Idle and a new step runs again with the same chain.
16. `Capture` receives the same key in every stage and `CapturedKey()` returns it.
17. Capture isolation: a fake poisons its source after `Capture`; its `Run` result is unaffected.
18. When stage *n+1* commits, stage *n*'s committed value is already visible to it (`CommitOrderProbe`).
19. The same executor instance is passed to every stage's `Run`. A stage may call `ParallelFor` several times in one `Run`.
20. `Capture`, `Run` and `Commit` allocate nothing (a counting allocator hook around the call, or an assertion on the table capacity not changing).

### Mid-step changes
21. A stage unregistered after `Capture` still runs and commits in that step, and is absent from the next step's `EnabledCount`.
22. `Abandon` after `Capture` or after `Run` calls no stage method, returns the phase to Idle, and applies pending removals.
23. After `Abandon`, a new `Capture` works and the abandoned step's stage working state is not visible to it (stage's own concern, asserted with a fake that resets in `Capture`).
24. Re-registering a stage whose removal is pending returns false until the phase is Idle, then succeeds on retry.
25. `UnregisterStage` mid-step does not modify the table `Run` walks: removal goes to a separate queue that `Run` never reads (asserted structurally, not by a race test).

### Restore (amended 2026-11-16, Feature 3)
27. `RestoreRan(key)` at `Idle` with `AllReady()` moves the phase to `Ran`, sets `CapturedKey()` to `key`, calls no stage method, and a following `Commit` commits every enabled stage in order. It returns false (no effect) when not `Idle`, when not `AllReady()`, or when a step is already captured.

### Failure
26. A stage that goes not-ready after `Commit` makes the next `AllReady()` false; the step already committed is unaffected.

## Files Touched

New: the files above; `Tests/GoogleTests/CoW/WorldStep/{TestStepChainConfig,TestStepChain}.cpp`. Edited: `CoW.vcxproj(.filters)`, `GoogleTests.vcxproj(.filters)` via `dia docs vcxproj-add`. **System spec edit on approval:** drop `historyBand` from the `StepChainConfig` sketch; replace `StepChain`'s listed interface with this one; fix Test Strategy line "DoStop with a step in flight drains, commits and samples" to "drains and abandons, no commit, no sample" (the Lifecycle section is correct, the test line contradicts it). No Dia files are added or edited.

## Binding Decisions

| ID | Decision | How this feature complies |
|----|----------|---------------------------|
| PD-004 | No STL in public APIs | `StageIdList`, `CallLog`, error types are fixed-capacity Dia containers |
| AD-001 | Ask permission before Dia edits | None. Only `StepKey` and `IParallelExecutor` headers are consumed, both Approved in DiaDeferredStep |
| AD-002 | Four-tier dependency model | CoW-Shared, consumed by A4, A1 and every world-sim testbed |
| DFS-002 | `Run` reads only the captured working copy | The chain's `Run` reads only the enabled table, which is immutable during a step (removals are queued) |
| Resolved Decision 4 (system) | Order is declarative; a missing configured stage is a hard error | `MissingStages` and `AllReady` encode it; the module turns it into health Failing |

## Test Plan

GoogleTests only, no engine or SimPU needed. TDD in the order: config load, registration and resolution, readiness, sequencing, mid-step changes. Config tests are table-driven. Sanitizer: the chain tests run under `dia check sanitizer`, including criterion 21 and 22 (a stage unregistered, then destroyed, while a step is in flight; the chain must not touch it after the step). Criterion 25 is a structural assertion (the removal queue is separate from the table `Run` walks), not a threading test: the chain is not thread-safe, and the module guarantees `Register`/`Unregister` are SimPU-only calls. No mutation checks. Gap audit and edge pass is the last plan task.

## Open Design Questions

1. **Is `kMaxStages = 8` the right bound?** Three stages exist since A5 Hydrology (amended 2026-11-17, A5 Hydrology) and A3 is explicitly not one. The bound only sizes fixed arrays, so it is cheap to raise, but a config with more is rejected, so a future layer (a tectonic stage) would need a spec touch. Chosen small to keep `StageIdList` on the stack.
2. **Should the chain, not the module, own the `Idle`-only restriction on `Configure`?** It does here, because a mid-step reconfigure would change which stages `Commit` walks. The module may prefer to refuse reconfiguration earlier and louder (an Error log); decide in Feature 2 whether the chain's silent `false` is enough.
3. **Does criterion 14's "no stage called" hold for a half-built chain?** `Capture` walks stages in order; if stage 2's `Capture` could fail, stage 1 would already have captured. Stages cannot fail `Capture` today (void), so it holds, but it is a reason to keep `Capture` infallible in the contract.

## Gap-pass amendments (2026-11-16)

Found by the plan-writing pass; each item below **supersedes** any contradicting text above. Numbering is stable.

1. **Types.** `StageIdList` is `DynamicArrayC<StageId, kMaxStages>` with `StageId { StringCRC crc; String32 name; }` (the name is kept for logs and errors). `ConfigError { String32 key; String64 reason; }`. A hash collision between two names is detected on `crc`.
2. **Optional observer.** The chain accepts an optional `IChainObserver*` (`SetObserver`), called as `OnStageBegin(index, phase)` / `OnStageEnd(index, phase)` around each stage's `Capture`/`Run`/`Commit`. It lets the module add per-stage profiling scopes without the chain depending on DiaObservation. Null by default.
3. **Criterion 20 (no allocation).** The test records the enabled table's pointer and capacity and the internal arrays' capacity before and after a full `Capture`/`Run`/`Commit` and asserts they are unchanged; no global allocation hook is used.
4. **Criterion 14 and the re-entrancy guard.** Calling a chain method from inside a stage method is a programming error: a debug assertion in Debug builds and a no-op returning false in Release. The test covers the **Release path only** (compiled out of Debug builds).
5. **Ordering of criteria.** Criterion 27 (Restore) sits after 26 (Failure); ids are stable and are not renumbered.

## Status

**Status:** `Done` (2026-11-20). Approved 2026-11-16. Amended 2026-11-17 (A5 Hydrology): the stage id list gains `hydrology`; no change to the chain's logic or `kMaxStages`. Plan: [step-chain.plan.md](step-chain.plan.md) (written 2026-11-16, not started).

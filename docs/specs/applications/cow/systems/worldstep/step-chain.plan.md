# Plan: World Step Chain (World Step Feature 1)

**Spec:** @docs/specs/applications/cow/systems/worldstep/step-chain.md
**Status:** Not Started
**System plan:** @docs/specs/applications/cow/systems/worldstep/worldstep.plan.md (row 1)

**Start condition:** DiaDeferredStep Feature 1 **Task 1** is Done (it ships `StepKey` and `IParallelExecutor`).
Nothing else from Dia is needed: `Dia::SaveGame::SaveContext` / `LoadContext` are built and only forward-declared
here. No Dia files are added or edited by this feature (AD-001: not triggered). This feature has no SimModule,
no bus and no scheduler, so every test is a plain GoogleTest with fake stages.

Feature 3 later amends this feature's chain with `RestoreRan` (AC 27). It is planned here (task 12) because the
amendment is already written into this spec and the method belongs to the chain's state machine.

## Implementation Patterns

### Files (all under `Cluiche/CoW/Shared/WorldStep/`, namespace `CoW::WorldStep`)
```
IWorldStepStage.h              header only; the contract + threading comment block
WorldStepConfig.h/.cpp         WorldStepConfig, StageIdList, ConfigError, LoadWorldStepConfig
WorldStepChain.h/.cpp          ChainPhase, WorldStepChain
Testing/FakeWorldStepStage.h   FakeWorldStepStage, CallLog, CommitOrderProbe (ships with the library)
Tests/GoogleTests/CoW/WorldStep/TestWorldStepConfig.cpp
Tests/GoogleTests/CoW/WorldStep/TestWorldStepChain.cpp
```
Added to `CoW.vcxproj(.filters)` and `GoogleTests.vcxproj(.filters)` with `dia docs vcxproj-add`, never by hand.

### Containers and types (PD-004)
- `StageIdList` is a fixed-capacity Dia container (`DynamicArrayC`-style, capacity `kMaxStages = 8`), on the stack.
- `CallLog` is a `DynamicArrayC<CallRecord, N>` of `(StringCRC stageId, CallPhase phase, StepKey key)`.
- `StageId { StringCRC crc; String32 name; }` and `StageIdList = DynamicArrayC<StageId, kMaxStages>` (Gap-pass amendment 1); the name is kept for logs and errors; a collision is detected on `crc`.
- `ConfigError { String32 key; String64 reason; }` (amendment 1).
- No STL in any public signature. `Json::Value` is the one external type, as in A4's parameter loaders.

### Config (`LoadWorldStepConfig`)
- All-or-nothing: parse into a local `WorldStepConfig`, validate everything, and only on success assign to `out`.
  On any fault `out = WorldStepConfig{}` (defaults) and `err` names the key and reason.
- Table-driven tests: one `struct { const char* json; const char* expectedKey; }` row per fault in AC 2, so adding
  a fault is one row. Keep the JSON inline; do not read files.
- Unknown top-level key check iterates `root.getMemberNames()` against the three known keys.
- Integer check for `apply_after_days`: `isInt()` (so `7.5` and `"7"` fail, `7.0`-style doubles are rejected too).
- Collision seam (AC 3): `LoadWorldStepConfig` forwards to an overload taking a hash function pointer
  (`StringCRC (*)(const char*)`) defaulting to the real one. The test passes a hash that returns a constant.
  Do not change `StringCRC` itself.

### Chain (`WorldStepChain`)
- Three-state machine `ChainPhase { Idle, Captured, Ran }`. Every sequencing method checks the phase first and
  returns false with **no** stage call and no state change (AC 14).
- Two fixed arrays: the **registration table** (`StringCRC id`, `IWorldStepStage*`, capacity `kMaxStages`) and the
  **enabled table** (`StringCRC id`, `IWorldStepStage*` in declared order). The enabled table is rebuilt only in
  `RegisterStage`, `UnregisterStage`, `Configure` and the end-of-step removal sweep; `Capture`/`Run`/`Commit`
  walk it and allocate nothing (AC 20).
- **Pending-removal list** is a third fixed array. `UnregisterStage` at non-Idle pushes onto it and touches
  neither of the other two tables, which is the structural assertion for AC 25 (a test inspects the table pointer
  and contents are unchanged, no thread needed). The sweep that applies removals runs at the end of `Commit` and
  in `Abandon`, then rebuilds the enabled table once.
- `RegisterStage` for an id whose removal is pending returns false until Idle (AC 24).
- `Configure` while phase is not Idle returns false and changes nothing (AC 10).
- `Capture` has a `DIA_ASSERT(AllReady())` in debug and a plain `return false` in release (spec Sequencing).
- `Abandon` calls no stage method; it sets phase Idle, clears `CapturedKey`, applies removals.
- `RestoreRan(StepKey)` (task 12): `Idle` and `AllReady()` required, sets phase `Ran` and `CapturedKey`, calls
  no stage method.
- Optional `IChainObserver*` (`SetObserver`, null default, amendment 2): `OnStageBegin/OnStageEnd(index, phase)` around each stage method (task 13).
- Calling a chain method from inside a stage method is a programming error: `DIA_ASSERT` in Debug, no-op returning false in Release (amendment 4). The test covers the **Release path only** (compiled out of Debug).
- The chain is not thread-safe by design; document "SimPU only for Register/Unregister" in the header.

### `FakeWorldStepStage`
- Constructed with `(id, CallLog&)`. Records `IsReady` is **not** logged (it is a query); `Capture`, `Run`,
  `Commit` are logged with their key where applicable.
- `SetReady(bool)`, `SetParallelForCalls(n)` (calls `executor.ParallelFor` n times in `Run`, using a
  `SerialExecutor` supplied by the test), `source` / `working` / `committed` integers for the poison test
  (`Capture`: `working = source`; `Run`: `result = working * 3`; `Commit`: `committed = result`),
  `resetOnCapture` flag for AC 23, `CommitOrderProbe*` that reads another fake's `committed` at its own
  `Commit` time (AC 18).
- Records the executor address passed to `Run` so AC 19 can compare identity.
- `SerializePending`/`DeserializePending` are empty bodies here (Feature 3 extends the fake with a checksum).

### Allocation check (AC 20)
- Assert capacity and pointer, never a global allocator hook (Gap-pass amendment 3): record the enabled table's pointer and capacity and the internal arrays' capacity before and after a full Capture/Run/Commit cycle and assert they are unchanged.

### Verification cadence
- Per task: `dia run googletest --filter="WorldStepConfig*:WorldStepChain*" --shards 2`.
- One full `dia run googletest --shards 2` in task 18 only. Sanitizer in task 16.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Confirm DiaDeferredStep Feature 1 Task 1 is Done (`StepKey.h`, `IParallelExecutor.h` exist). Create `Shared/WorldStep/` and `Tests/GoogleTests/CoW/WorldStep/` with empty `IWorldStepStage.h`, `WorldStepConfig.h/.cpp`, `WorldStepChain.h/.cpp`, `Testing/FakeWorldStepStage.h`, and the two test files; register all with `dia docs vcxproj-add` (CoW and GoogleTests, with filters); build with `dia run googletest --filter="WorldStep*" --shards 2` | Builds; zero tests run | Not Started | haiku | Mechanical, TDD-exempt. If Task 1 is not Done, stop and report BLOCKED |
| 1 | `IWorldStepStage.h` (contract exactly as the spec, threading contract comment block) and `Testing/FakeWorldStepStage.h` with `CallLog`. RED: a test that constructs a fake, drives its methods directly and reads the log | AC 13 (call-log shape only), 19 (executor identity recorded) | Not Started | sonnet | Public API of the test support is new, so it is TDD'd. Fake must compile against `IParallelExecutor` only |
| 2 | RED then GREEN: `WorldStepConfig` + `LoadWorldStepConfig` happy path and defaults. Includes `StageIdList` and `ConfigError` | AC 1, 4 (`apply_after_days` 1 and 365 valid; missing keys take 7 and true); `StageId` carries crc and name, `ConfigError` carries key and reason (amendment 1) | Not Started | sonnet | Types fixed by Gap-pass amendment 1 (`StageId{crc,name}`, `ConfigError{key String32, reason String64}`). Prove RED with the quoted compile/link failure or failing assertion |
| 3 | RED then GREEN: config fault table, all-or-nothing behaviour (`out` equals a default config after every failure, `err` names the key) | AC 2: every row (order missing/empty/not array/9 entries/non-string/empty string/duplicate; `apply_after_days` 0, 366, 7.5, `"7"`; `deferred` `"true"`; unknown top-level key) | Not Started | sonnet | Table-driven; one row per fault. Assert `out` unchanged from default **and** the key text in `err` |
| 4 | RED then GREEN: `StringCRC` collision rejection through the hash seam | AC 3 (collision detected on `crc`) | Not Started | sonnet | Seam is an overload with a hash function pointer; do not touch `StringCRC` (Dia boundary) |
| 5 | RED then GREEN: `WorldStepChain` registration, resolution and declared-order table (`Configure`, `RegisterStage`, `UnregisterStage` at Idle, `EnabledCount`, `StageAt`, `StageIdAt`) | AC 5 (register ecology first, weather is index 0), 8 (duplicate id, table full: no change), 9 (Idle unregister immediate, unknown id no-op) | Not Started | sonnet | Establish the registration/enabled/pending-removal tables here even though pending removal is exercised in task 10 |
| 6 | RED then GREEN: unknown and missing stages, `Configure` re-call | AC 6 (`UnknownStages`, never called), 7 (`MissingStages` in declared order), 10 (re-resolve; false and no change mid-step: the mid-step half is asserted after task 9 exists) | Not Started | sonnet | AC 10's in-flight half is added as an extra test in task 9 once `Capture` exists |
| 7 | RED then GREEN: `AllReady()` | AC 11 (false until all ready and none missing; flips false again), 12 (unknown stage's readiness ignored), 26 (stage goes not-ready after `Commit`: next `AllReady` false, committed step unaffected; needs `Commit`, finish after task 8) | Not Started | sonnet | Split: AC 11/12 here, AC 26 appended in task 8 |
| 8 | RED then GREEN: sequencing state machine `Capture`/`Run`/`Commit`, `Phase()`, `CapturedKey()` | AC 13 (declared order each phase), 14 (all five out-of-sequence calls false, no stage called), 15 (Idle after `Commit`, second step works), 16 (same key to every stage), 19 (same executor to every stage, `ParallelFor` several times), 26 | Not Started | sonnet | `Capture` debug-asserts `AllReady()`; the release path returns false. Test the false path only in a build where the assert is compiled out, or guard with the project's assert-throw test hook if one exists. Also the re-entrancy guard (amendment 4: a chain method called from inside a stage method): **Release path only**, compiled out of Debug |
| 9 | RED then GREEN: capture isolation, commit-order visibility, no allocation, and the in-flight `Configure` refusal | AC 17 (poison source after `Capture`, `Run` unaffected), 18 (`CommitOrderProbe`), 20 (enabled-table pointer and capacity, and internal array capacity, unchanged across a cycle; amendment 3), 10 (in-flight `Configure` returns false, nothing changes) | Not Started | sonnet | Should mostly pass after task 8; if any test is GREEN immediately, record that in Notes and add the missing assertion rather than skipping RED |
| 10 | RED then GREEN: mid-step unregister via the pending-removal list | AC 21 (stage unregistered after `Capture` still runs and commits, absent next step), 24 (re-register false until Idle, then succeeds), 25 (structural: `Run`'s table untouched, removal in a separate list) | Not Started | sonnet | The unit tests also destroy the fake after the step to give the sanitizer run (task 16) something to catch. Chain must never touch a removed stage after `Commit` |
| 11 | RED then GREEN: `Abandon` | AC 22 (after `Capture` and after `Run`: no stage method called, Idle, removals applied), 23 (new `Capture` after `Abandon` works; fake with `resetOnCapture` shows no leaked working state) | Not Started | sonnet | `Abandon` at Idle is a defined no-op; test it |
| 12 | RED then GREEN: `RestoreRan(StepKey)` (the 2026-11-16 amendment for Feature 3) | AC 27: Idle + `AllReady` moves to `Ran`, `CapturedKey` set, no stage method called, following `Commit` commits every enabled stage in order; false with no effect when not Idle, not `AllReady`, or already `Captured` | Not Started | sonnet | Only the chain edit; the module-side use is Feature 3. Record in Notes that this row is the System plan's "Feature 3 amendment landed" gate |
| 13 | RED then GREEN: optional `IChainObserver` (Gap-pass amendment 2): `SetObserver(IChainObserver*)`, null by default; `OnStageBegin(index, phase)` / `OnStageEnd(index, phase)` called around each stage's `Capture`/`Run`/`Commit` in declared order; the chain gains no DiaObservation dependency | Amendment 2: a recording observer sees begin/end pairs per stage per phase in declared order; null observer is a no-op; observer is not called by `Abandon` or `RestoreRan` | Not Started | sonnet | CoW Shared only, no Dia edit. The Feature 2 module uses this for per-stage profiling scopes (module-and-cadence task 17) |
| 14 | Spec housekeeping owed on approval (verify, then apply only what is still missing): drop `historyBand` from the system spec's `WorldStepConfig` sketch; replace its `WorldStepChain` block with this feature's interface; change the Test Strategy line "DoStop with a step in flight drains, commits and samples" wording to "drains and abandons, no commit, no sample". Use `dia docs` commands where one exists | Spec text diff reviewed; no code | Not Started | haiku | Docs only; not a verification-gate task. See Open items: the system spec has further stale lines beyond these three |
| 15 | Exhaustive test pass: gap audit of every AC 1-27 (27 sits after 26; ids stable) against the test names (each AC has at least one named test), edge cases not in the ACs (see Open items), then edge tests: empty-order never reaches the chain; `kMaxStages` registrations; `Configure` shrinking the order to exclude a registered stage; unregister then re-register at Idle; `Capture` with a stage that is registered, enabled and not ready (release behaviour); phase after a false return unchanged | Gap list in Notes; edge tests green | Not Started | sonnet | No mutation check. Report gaps found, do not widen scope silently |
| 16 | Sanitizer: `dia check sanitizer` scoped to `WorldStepConfig*:WorldStepChain*`, with criteria 21 and 22 included (stage unregistered then destroyed while a step is in flight) | ASan/UBSan clean | Not Started | haiku | DiaCLI run-and-report only. Report failures verbatim, do not fix in this task |
| 17 | Observation Opportunity Scan over every file touched, across the five DiaObservation pillars (logs, traces, profiling, metrics, health). Suggestions only; the chain deliberately has no DiaObservation dependency | Suggestions recorded in Notes | Not Started | sonnet | Expected finding: logging for registration/duplicate/unknown-stage belongs in the Feature 2 module, not here. Do not block |
| 18 | One full verification run: `dia run googletest --shards 2` and `dia check deps`; quote pass/fail and failing names only | All GoogleTests green; deps clean | Not Started | haiku | The only unscoped run in this plan. Release perf gates not applicable here |
| 19 | Close-out (do **not** run now): `dia docs plan docs/specs/applications/cow/systems/worldstep/step-chain.plan.md <n> --status Done` per row; `dia docs spec-done docs/specs/applications/cow/systems/worldstep/step-chain.md`; update the system plan row 1; commit (conventional message with the attribution lines) | Spec Done, plan Done, system plan row updated | Not Started | haiku | Listed for the implementer; executes only after task 18 is green |

## Open items

Spec silences and tensions, recorded and not resolved here.

1. **`StageIdList` element type.** Config ids are strings in the file but the chain API takes `StringCRC`, and the
   module must log missing ids by name. The spec does not say whether `StageIdList` stores strings, CRCs, or both.
   Resolved by amendment 1 (2026-11-16): `StageId { StringCRC crc; String32 name; }`.
2. **`ConfigError` is never defined**: resolved by amendment 1 (2026-11-16), `ConfigError { String32 key; String64 reason; }`. `ChainPhase`'s header is still only sketched in Patterns; confirm.
3. **AC 20's mechanism**: resolved by amendment 3 (2026-11-16): pointer and capacity assertions, no allocation hook.
4. **AC 8's "table is full" for `RegisterStage`** vs Configure's `kMaxStages = 8` order bound: whether the registration
   table is also 8 (so unknown stages count against it) is not stated.
5. **Release behaviour of `Capture` when `AllReady()` is false** is "no-op returning false", but task 8 needs a way to
   test the debug assertion. Resolved by amendment 4 (2026-11-16): the Release path only is tested; the Debug assertion is not.
6. **Validation of ids against a vocabulary** is explicitly not done; a typo surfaces as a missing stage in Feature 2.
7. **System spec staleness beyond the three edits in task 14:** (resolved by World Step system amendments 1, 2, 4 of 2026-11-16, which supersede these lines; task 14 verifies only) `WorldStepModule::RegisterStage` is shown as `void`
   (Feature 2 says `bool`); `LoadWorldStepConfig` is shown without `ConfigError`; the `WorldStepChain` block lacks
   `UnknownStages`, `StageIdAt`, `CapturedKey`, `RestoreRan`; the Test Strategy still says an in-flight step is
   "drained and committed first" on stage unregister and that the signal "carries the apply key" (Feature 2 and 3
   abandon without commit and carry both days); the build-order note says Feature 1 can start once DiaDeferredStep
   Feature 2 exists while the Dependencies section says Feature 1 Task 1. This spec follows Task 1.
8. **AC numbering:** 27 is listed after 26 in the spec; resolved by amendment 5 (2026-11-16): ids are stable and not renumbered.
9. **Open Design Question 2** (who owns the Idle-only `Configure` refusal, chain or module) is deferred to Feature 2;
   this plan implements the chain's silent `false`.

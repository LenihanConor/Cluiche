# Implementation Plan: JobSystem ServiceStream Provider

**Spec:** @docs/specs/applications/dia/systems/diathreading/jobsystem-servicestream.md

**Status:** Done

---

## Implementation Patterns

### Pattern 1: TrySubmit Implementation
- Check `mThreadPool != nullptr` before calling internal Submit logic
- Return default-constructed `JobHandle` (invalid) if pool is null
- Otherwise, execute identical logic to `Submit` (enqueue, increment counters, return valid handle)
- No assert — silent failure with invalid handle return

### Pattern 2: ServiceStreamWriter Lifecycle
- Declare `ServiceStreamWriter<Dia::Threading::JobSystem>` member in `JobSystemModule.h`
- `OnConnectStreams`: call `mJobSystemService.Connect(app)`
- `DoStart`: call `mJobSystemService.Register(mJobSystem)` after `mJobSystem.Initialize()`
- `DoStop`: call `mJobSystemService.Deregister()` before `mJobSystem.Shutdown()`

### Pattern 3: Manifest Schema Entry
- Add ServiceStream entry to `cow_main.diaapp` streams array
- Fields: `id`, `kind: "ServiceStream"`, `payload_type: "JobSystem"`, `from: "MainPU"`
- Register `JobSystem` type in `registeredtypes.diaschema`

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Add `TrySubmit` to JobSystem.h/.cpp | Unit test: call after shutdown, verify invalid handle | Done | haiku | Non-asserting variant of Submit |
| 2 | Add ServiceStreamWriter member to JobSystemModule.h | Compiles | Done | haiku | Declaration only, needed Application.h include |
| 3 | Wire ServiceStreamWriter lifecycle in JobSystemModule.cpp | Compiles, no validation errors | Done | haiku | Connect/Register/Deregister |
| 4 | Add ServiceStream entry to cow_main.diaapp | `dia validate manifest` passes | Done | haiku | JSON manifest + provides role |
| 5 | Add ServiceStream entry to cluiche_main.diaapp | `dia validate manifest` passes | Done | haiku | CluicheTest manifest + provides role |
| 6 | Register JobSystem in registeredtypes.diaschema | Schema validation passes | Done | haiku | Not needed - only modules/components tracked |
| 7 | Add unit tests for TrySubmit | 3 tests pass (before init, after shutdown, normal) | Done | haiku | 3 new tests, all pass |
| 8 | Integration test: cross-PU ServiceStream access | Test module reads service, calls TrySubmit | Skipped | sonnet | Deferred - would need test consumer module |
| 9 | Run full test suite | All tests pass | Done | haiku | Full suite passes, 9183 tests |

---

## Notes

- Task 1 implements AC3
- Tasks 2-3 implement AC1-AC2
- Tasks 4-6 implement AC4-AC5
- Tasks 7-8 verify all ACs
- Task 9 is the final verification gate before commit

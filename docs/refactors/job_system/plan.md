# Refactor Plan — JobSystem (Phases 1–3)

**Spec:** `docs/refactors/job_system/reports/plan.md` (audit + 5-phase plan)
**Scope this session:** Phases 1–3. Static shim stays; ParallelFor / parent-child / priority dropped per the plan. Phases 4–5 (delete shim, drain semantics + diagnostics) deferred.

## Session Notes — Spec Decisions Summary

`Dia::Core::JobSystem` is a header-only Meyers-singleton with leak-prone manual `Job*` lifetime, an unguarded double-`Run`, work-stealing comments that don't match the impl, and dead surface (`priority`, `ParallelFor`, parent-child) used only by tests. Production callers — `JobSystemModule` (CluicheGameBaseline), `TextureHandler` (DiaSFML) — only use Create/Run/Wait. The refactor goal: replace the singleton with a value-typed `JobSystem` owned by `JobSystemModule`, exposing three primitives — `Submit` / `Wait` / `IsComplete` — returning a refcounted `JobHandle`. Phase 1 lands the new API behind a static shim (non-breaking); Phases 2–3 migrate the two production callers; the shim and dead surface stay until a follow-up Phase 4. Platform constraints: PD-001 (StringCRC for IDs — N/A here), PD-004 (no STL in public Dia APIs — `JobHandle` is allowed to wrap `shared_ptr<Job>` internally but the public typedef should be opaque), PD-006 (vcxproj is source of truth — every new .cpp is added to DiaCore.vcxproj + .filters), PD-007 (C++20).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Split `ThreadPool` into `.h` (decls) + `.cpp` (impl). Add `.cpp` to DiaCore.vcxproj + .filters. No API change. | `dia run googletest --filter="JobSystem*"` (existing tests) + full build | Done | sonnet | All 29 existing tests pass after the split. |
| 2 | New instance API in `JobSystem.cpp`: class `JobSystem` with `Init`/`Quit`/`Submit(JobFn)`/`Wait(JobHandle)`/`IsComplete(JobHandle)`. `JobHandle` wraps `shared_ptr<Job>`. Existing static surface kept as a shim that delegates to a hidden Meyers singleton. Instance methods named `Init`/`Quit` (temporary) to avoid signature collision with the static `Initialize`/`Shutdown`; rename happens in Phase 4 when the shim is removed. `Job` stays a public struct (the legacy shim hands `Job*` to existing callers). Add `.cpp` to DiaCore.vcxproj + .filters. | Existing `TestJobSystem.cpp` passes unchanged | Done | sonnet | Found and fixed a `shared_ptr<Job>` cycle in `Submit` (lambda captured `jobPtr` while being assigned to `jobPtr->function`). Replaced with task-lambda capture only. |
| 3 | Add new test cases in `TestJobSystem.cpp` against the new instance API: (a) `Submit_ExecutesFunction`; (b) `WaitFromInsideRunningJob_DoesNotDeadlock` (2-thread pool, 5s timeout); (c) `SubmitAfterQuit_Asserts` (Debug only); (d) `ReInitAfterQuit_ProducesFreshPool`; (e) `HandleOutlivesWait_AndIsCompleteSafe`; (f) `HandleDestructionFreesJob_AfterCompletion` (weak_ptr expiry); (g) `IsCompleteOnDefaultHandle_ReturnsTrue`. New `JobSystemInstance` test suite — local instance, no singleton. | `dia run googletest --filter="JobSystem*"` includes new tests | Done | sonnet | Found that `Submit` crashed after `Quit` (assert fired but didn't return). Added an early-return to `Submit` after the assert; the assert + invalid-handle return are now tested. 36/36 tests pass. |
| 4 | Commit P1. Update plan rows 1–3 to Done. | `git status` clean | Done | haiku | Single commit. Plan update + code in same commit. Commit `c1fe4d5`. |
| 5 | `JobSystemModule` holds `Dia::Core::JobSystem` by value. `DoStart` sets `sInstance = this`, calls `mJobSystem.Init(0)` AND legacy `Dia::Core::JobSystem::Initialize(0)` (parallel — TextureHandler still needs the shim). `DoStop` reverses. Cross-module access: `JobSystemModule::GetStatic()` + `GetJobSystem()`, mirrors `AssetServiceModule::GetStatic()` pattern. Spec `docs/specs/features/cluichetest/async-asset-loading/jobsystem-module.md` updated. | `dia run cluichetest` launches; JobSystem `DoStart entry`/`DoStart ready` log lines present | Done | sonnet | DiaApplicationFlow has no service-locator; reused the project's `GetStatic()` convention. Pre-existing GL teardown noise (`RenderTextureImplFBO.cpp`) is unrelated. |
| 6 | Commit P2. Update plan row 5 to Done. | `git status` clean | In Progress | haiku | |
| 7 | `TextureHandler` takes a `JobSystem*` in its constructor (or via `AssetService` wiring — pick the path that exists). Replace `JobSystem::CreateJob/Run/Wait` static calls with `instance.Submit` + handle-based `Wait`. Drop `Job* job` from `PendingUpload`, replace with `JobHandle`. Keep `shared_ptr<PendingUpload>`. | `dia run cluichetest` dummy stage texture load + teardown clean | Pending | sonnet | Phase 4 (later) revisits whether `shared_ptr<PendingUpload>` is still needed once handle is refcounted. |
| 8 | Commit P3. Update plan row 7 to Done. | `git status` clean | Pending | haiku | |

## Acceptance for this session

- AC2 (no Job leaks via refcounted handles): partial — Phase 1 lands the mechanism, Phase 3 makes TextureHandler use it.
- AC3 (Wait from inside running job no longer deadlocks): proven in Phase 1 (new test).
- AC4 (Submit after Shutdown asserts): proven in Phase 1.
- AC6 (.h/.cpp split): done in Phase 1 for `ThreadPool` and `JobSystem`.
- AC7 (cluichetest texture load + teardown preserved): proven in Phases 2 + 3.
- AC8 (existing GoogleTest threading still passes; new tests for missing list): done in Phase 1.

Deferred to Phase 4–5:
- AC1 (no caller hits the static surface) — shim stays.
- AC5 (header docstrings honest, no work-stealing/priority claims) — wait until shim is removed.

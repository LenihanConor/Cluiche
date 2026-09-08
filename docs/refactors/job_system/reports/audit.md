# Refactor Audit — JobSystem

**Session date:** 2026-05-17
**Folder:** docs/refactors/job_system/

## Subsystem Summary

`Dia::Core::JobSystem` (Dia/DiaCore/Threading/JobSystem.h) is a header-only Meyers-singleton wrapping a `ThreadPool`, exposing `Job*` / `CreateJob` / `Run` / `Wait` plus parent-child relationships and a `ParallelFor` helper. Production callers — `TextureHandler` (DiaSFML), `AsyncFileLoader` (DiaCore), and `JobSystemModule` (CluicheGameBaseline) — only use Create/Run/Wait. The implementation has correctness bugs (child-job leaks, no-op work-stealing, unguarded double-Run), advertised features that don't exist (priorities, work stealing), and confused ownership (singleton plus a module wrapper that "owns" lifecycle but not the instance).

## Strengths

- Single small header — easy to replace wholesale without ripple.
- `JobSystemModule` already owns init/shutdown lifecycle on MainPU — the right ownership home exists, just under-used.
- Sibling primitives (`Atomic.h`, `Mutex.h`, `Thread.h`) are clean and reusable.
- Existing GoogleTest suite covers Submit/Wait/parent-child/ParallelFor — useful regression net even if some cases will be retired.

## Debt

- **Child jobs leak unconditionally.** `WaitImpl` only `DIA_DELETE`s its argument; `CreateChildJob` allocations are never freed. (`JobSystem.h:166-178`)
- **Wait() can deadlock from inside a running job.** Spin/yield loop never executes pending tasks. The comment `// Could implement work stealing here` is the entire problem; the docstring (line 33) advertises stealing that doesn't exist.
- **ThreadPool::Shutdown drops queued tasks.** Sets the flag, joins, and never drains the queue — every enqueued-but-unrun lambda + Job leaks. (`ThreadPool.h:94-110`)
- **Run() is not idempotent and unguarded.** Calling twice enqueues two copies; the second `Finish` underflows `unfinishedJobs` and races with `Wait`'s `DIA_DELETE`. No assert. (`JobSystem.h:157-164`)
- **Job lifetime contract is undocumented and unenforceable.** `IsComplete` reads memory `Wait` has freed; second `Wait` double-deletes. Callers (TextureHandler) work around it with `shared_ptr<PendingUpload>`. (`JobSystem.h:99-103, 177`)
- **Initialize/Shutdown not thread-safe and silently no-op on re-init.** `if (!mThreadPool)` is not guarded. (`JobSystem.h:121-136`)
- **Static-destructor Shutdown.** `~JobSystem` runs at process exit and tries to join workers — duplicates the module's stop path with worse ordering guarantees. (`JobSystem.h:110-113`)
- **Two `std::function` layers per submission.** `Job::function` is captured into a `ThreadPool::Task`; both can heap-allocate.
- **Job is heap-allocated per submission.** `DIA_NEW(Job())` per `CreateJob`; no pool. ParallelFor of N produces N allocations + N lambda captures + N mutex pushes.
- **`priority` field exists but is ignored.** `ThreadPool` uses a FIFO `std::queue`. (`JobSystem.h:48`, `ThreadPool.h:166`)
- **All implementation lives in headers.** Every change recompiles every TU that includes it.
- **Singleton + module wrapper is a confused ownership contract.** `JobSystemModule` owns lifecycle but not the instance. Cannot run two instances (tests, multi-app); cannot inject a stub; the dependency is invisible to callers.
- **Header docstring advertises features not implemented.** Work stealing, priorities (lines 28-33).

## Duplication

- **Two "wait" semantics.** `ThreadPool::WaitAll` waits for queue drain; `JobSystem::Wait` waits for one job tree. `AsyncFileLoader` uses both.
- **Two thread abstractions.** `Dia::Core::Thread` exists with name/priority slot but `ThreadPool` uses raw `std::thread`.

## Hidden Coupling

- **Callers must externally keep job-captured state alive.** `TextureHandler` wraps `PendingUpload` in `shared_ptr` because `Job::function` is consumed by `Finish` and the Job is then deleted by `Wait`. (`TextureHandler.cpp:101-114`)
- **Caller must drain its own outstanding jobs before `JobSystemModule::DoStop`.** Per the spec's AI Q2: AssetService stops first and must Wait on outstanding texture loads; an undrained caller can hang or leak Shutdown. The contract is unwritten in code.
- **Header-only design means ThreadPool internal changes reach every consumer's TU.**

## Missing Tests

- Double-`Run` of the same job (currently silent corruption).
- `Wait` invoked from inside a running job (the deadlock scenario).
- Shutdown-with-pending-work leak / drain behaviour.
- Submit after Shutdown — should be an asserting programmer error.
- Proof that child-job storage is actually freed (would catch the leak).
- Re-Init after Shutdown produces a clean, independent worker pool.

## Likely Invariants to Preserve

- `Submit(fn)` eventually calls `fn()` exactly once on a worker thread.
- `Wait(handle)` returns only after `fn()` has fully executed.
- After `Wait` returns, the handle and any captured lambda state are safe to release.
- Shutdown completes only after all submitted work has finished (current behaviour, intended).
- Submit-after-Shutdown is a programmer error.
- Re-Init after Shutdown produces a fresh worker pool.
- Concurrent `Submit` from many threads is safe.
- `Wait` is safe from any thread, including a worker thread on the same instance — must not deadlock.

## Recommended Cleanup Order

1. **Decide ownership.** Drop the singleton; `JobSystem` becomes a value type owned by `JobSystemModule`. Hand it to callers via service locator or pointer.
2. **Collapse the API to three primitives** — `Submit` / `Wait` / `IsComplete` — returning a refcounted `JobHandle` that owns Job storage. Eliminates manual delete and double-`Run` hazard in one move.
3. **Move impl out of header** to `JobSystem.cpp` / `ThreadPool.cpp`.
4. **Strip dead/misleading surface.** Remove `priority`, remove work-stealing comments, defer `ParallelFor` and parent-child until a real caller needs them.
5. **Tighten ThreadPool shutdown.** Either drain pending work or assert it's empty; document the choice.
6. **Add diagnostics.** Assert Submit-after-Shutdown; rely on refcounted handle to make Wait-on-stale-handle safe.
7. **Add the missing tests above.**
8. **Update existing tests** to drop parent/child + ParallelFor cases (until those features return); keep core Submit/Wait coverage.
9. **Migrate callers** (`TextureHandler`, `AsyncFileLoader`) to the handle-based API; verify behaviour parity.

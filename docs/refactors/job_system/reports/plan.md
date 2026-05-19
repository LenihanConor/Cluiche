# Refactor Plan — JobSystem

**Input:** docs/refactors/job_system/outputs/audit.json

## Refactor Goal

Replace the singleton-based, header-only, leak-prone `JobSystem` with a value-typed instance owned by `JobSystemModule` that exposes a three-primitive RAII handle API (`Submit` / `Wait` / `IsComplete`).

## Current Problem

Singleton + header-only impl + manual `Job*` lifetime causes child-job leaks, double-`Run` hazards, undocumented lifetime contract, and dead surface (`priority`, work-stealing, `ParallelFor`) that no production caller uses.

## Target Shape

`JobSystem` is a plain class. `JobSystemModule` holds an instance. Callers reach it via service locator (or constructor wiring). The public API is three methods returning/accepting a refcounted `JobHandle`.

```cpp
namespace Dia::Core {
    using JobFn = std::function<void()>;

    struct JobHandle {
        // Refcounted; copyable; safe to outlive Wait
    };

    class JobSystem {
    public:
        void       Initialize(unsigned numThreads = 0);
        void       Shutdown();

        JobHandle  Submit(JobFn fn);
        void       Wait(const JobHandle& h);    // safe from any thread, including a worker
        bool       IsComplete(const JobHandle& h);
    };
}
```

`JobHandle` wraps a `shared_ptr<Job>` (or equivalent intrusive refcount). The `Job` is freed when the last handle drops AND the job has finished — no manual `delete`, no double-`Run` hazard, no UAF on `IsComplete` after `Wait`.

### Boundary Changes

- `JobSystem` moves from a globally-accessible singleton to an instance reachable via `JobSystemModule` (service locator or accessor).
- `ParallelFor` is removed from `JobSystem.h` (deferred until a real caller needs it).
- `ThreadPool` implementation moves from `.h` to `.cpp` (public surface unchanged).

### Responsibilities Before vs After

| Responsibility | Before | After |
|----------------|--------|-------|
| Worker thread management | `ThreadPool` (header-only) | `ThreadPool` (split `.h`/`.cpp`) |
| Job lifetime | Manual: caller calls `Wait` which deletes; child jobs leak | Refcounted `JobHandle`; freed when refcount = 0 and job finished |
| Lifecycle ownership | Singleton + `JobSystemModule` both manage | `JobSystemModule` alone owns the instance |
| Fan-out (`ParallelFor`, parent-child) | Implemented (leaky) | Removed; reintroduced via separate spec when needed |
| Priority scheduling | Field exists, ignored | Field removed |
| Submit-after-Shutdown | Silent early return | `DIA_ASSERT` |

## Affected Systems

- **JobSystemModule** (`Cluiche/CluicheGameBaseline/Modules/JobSystemModule.{h,cpp}`) — holds the instance; service-locator-registers on `DoStart`. Spec: `docs/specs/features/cluichetest/async-asset-loading/jobsystem-module.md`.
- **TextureHandler** (`Dia/DiaSFML/TextureHandler.{h,cpp}`) — receives `JobSystem*`; replaces static `Create/Run/Wait` calls with instance-method equivalents.
- **AsyncFileLoader** (`Dia/DiaCore/FilePath/AsyncFileLoader.h`) — uses `ThreadPool` directly, not `JobSystem`. No API change; recompiles when `ThreadPool` is split into `.cpp`.
- **GoogleTests** (`Cluiche/Tests/GoogleTests/Core/Threading/TestJobSystem.cpp`) — drops parent/child + `ParallelFor` cases (deferred); adds tests for the missing-test list; constructs a local `JobSystem` instead of using the singleton.

## API / Interface Changes

- **`Dia/DiaCore/Threading/JobSystem.h`** — replace static surface with instance class plus `JobHandle` value type. `Job` becomes an internal struct in the `.cpp`.
- **`Dia/DiaCore/Threading/ThreadPool.h`** — declarations only; bodies move to `ThreadPool.cpp`.

## Migration Phases

| Phase | Name | Description |
|-------|------|-------------|
| 1 | Carve out impl files + `JobHandle` skeleton | Split `ThreadPool` into `.h`/`.cpp` (no API change). Introduce `JobSystem.cpp` with the new instance API behind a static-shim that delegates to a hidden global instance. Add `JobHandle`. Add the missing tests against the new API. **Checkpoint:** builds + all tests pass. |
| 2 | Migrate `JobSystemModule` to instance ownership | Module holds `JobSystem` by value (or `unique_ptr`). Service-locator-registers `JobSystem*` on `DoStart`. Update spec architecture section. **Checkpoint:** cluichetest launches. |
| 3 | Migrate `TextureHandler` to instance API | Inject `JobSystem*` (constructor or `AssetService` wiring). Replace static calls with instance calls. **Checkpoint:** dummy-stage texture load + teardown clean. |
| 4 | Remove the static-API shim and dead surface | Delete the singleton bridge from Phase 1. Delete `ParallelFor`, parent-child fields, `priority`, work-stealing comments. Update tests. |
| 5 | Tighten `ThreadPool` shutdown + add diagnostics | Drain on shutdown (documented). Assert Submit-after-Shutdown, double-Initialize. Add tests. |

## Minimal Viable Slice

**Phase 1 alone.** ThreadPool impl is split, `JobHandle` exists, missing-test cases land. The static shim keeps existing callers working — even if Phases 2-5 stall, the bug fixes (drain semantics, leak fix via refcount) are reachable.

## Risks and Rollback Points

- **Service-locator wiring may not exist in current AppFlow.** *Mitigation:* check for an existing registry in `DiaApplicationFlow` during Phase 2; if absent, fall back to constructor wiring in module factories. *Rollback:* revert Phase 2 commit; the shim keeps the old code working.
- **TextureHandler's `shared_ptr<PendingUpload>` was added to outlive the `Job`.** Refcounted `JobHandle` may make it redundant. *Mitigation:* keep `shared_ptr` in Phase 3 (zero risk); revisit later. *Rollback:* revert TextureHandler commit independently.
- **Dropping `ParallelFor` could break a test that uses it transitively.** *Mitigation:* Phase 4 audits `TestJobSystem.cpp` before deleting. *Rollback:* restore from prior commit; `ParallelFor` can live in a deprecated namespace if needed.
- **Phase 5's drain semantic could shift observable shutdown timing.** *Mitigation:* drain is the de-facto behaviour callers expect today (they `Wait` first). *Rollback:* revert Phase 5 commit.

## Acceptance Criteria

1. **AC1** — All callers (`JobSystemModule`, `TextureHandler`) use the instance-based `JobSystem`; no caller calls `JobSystem` static methods.
2. **AC2** — Job allocations have no leaks: every `Submit` returns a `JobHandle`, and the `Job` is freed when refcount hits zero AND the job has finished.
3. **AC3** — Calling `Wait` from inside a running job does not deadlock under any existing or new test.
4. **AC4** — `Submit` after `Shutdown` asserts (test verifies).
5. **AC5** — Header docstrings describe only implemented features; no work-stealing or priority claims.
6. **AC6** — `JobSystem.h` and `ThreadPool.h` have only declarations (impl in `.cpp`).
7. **AC7** — Existing TextureHandler behaviour (cluichetest dummy stage texture load + teardown) is preserved.
8. **AC8** — GoogleTest Threading suite still passes; new tests cover the missing-test list.

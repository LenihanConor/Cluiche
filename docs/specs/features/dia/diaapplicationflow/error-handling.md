# Feature Spec: Error Handling

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Error Handling | (this file) |

## Problem Statement

Module lifecycle failures (DoStart returning kFailed, timeouts) need well-defined recovery behavior that differs by context — fail-fast in development, graceful degradation in production, and hard abort for unrecoverable boot failures.

## Acceptance Criteria

1. Three error policies: Assert (debug), Rollback (release), Shutdown (boot)
2. Policy selection based on context: boot stage always uses Shutdown, non-boot uses build-config-dependent policy
3. DIA_ASSERT fires in debug builds on any module failure or timeout (non-boot)
4. Rollback in release: stops all START modules, restarts all STOP modules, returns to previous stage
5. Shutdown: logs diagnostic, calls RequestShutdown() — clean teardown
6. If rollback itself fails (restarted module also fails), escalate to Shutdown
7. All failures logged with: module instance_id, failure type (kFailed/timeout), elapsed time, stage context
8. Timeout diagnostic includes which module and how long it was in Starting/Stopping state
9. Error policy is framework-internal — modules don't choose their own policy
10. RequestShutdown() is always available as an explicit framework call (not just error-triggered)

## Error Policy Matrix

| Condition | Debug Build | Release Build |
|-----------|-------------|---------------|
| Boot stage: module kFailed | DIA_ASSERT then Shutdown | Shutdown |
| Boot stage: module timeout | DIA_ASSERT then Shutdown | Shutdown |
| Non-boot: module kFailed | DIA_ASSERT | Rollback to previous stage |
| Non-boot: module timeout | DIA_ASSERT | Rollback to previous stage |
| Rollback failure | DIA_ASSERT then Shutdown | Shutdown |
| RequestShutdown() called | Clean shutdown | Clean shutdown |

## Rollback Algorithm

```
1. Module failure detected during transition (Starting phase)
2. Mark failed module as Failed state
3. For all other START modules currently in Starting:
   - Call BeginStop() → wait for kDone (with stop_timeout)
4. For all STOP modules (previously stopped in this transition):
   - Call BeginStart() → wait for kReady (with start_timeout)
5. Restore current stage to previous stage
6. Log: "Rollback complete: returned to stage [X] due to [module] failure"
7. If any module in step 3 or 4 fails → escalate to Shutdown
```

## Shutdown Sequence

```
1. RequestShutdown() called (or escalated from error)
2. Set application state to ShuttingDown (no more transitions accepted)
3. For each PU (reverse startup order):
   - BeginStop() on all active modules (reverse dependency order)
   - Wait for kDone (with stop_timeout — if timeout, force-continue)
4. Join all dedicated threads
5. Destroy all modules
6. Application::Update() returns false → caller exits main loop
```

## Public API

```cpp
namespace Dia::ApplicationFlow {

    class Application {
    public:
        void RequestShutdown();
        bool IsShuttingDown() const;
        // Update() returns false when shutdown is complete
        bool Update();
    };
}
```

## Logging Format

All error logs use DiaLogger Application channel:

```
[Application][ERROR] Module 'DummyLevel' failed during stage transition to 'DummyStage' (DoStart returned kFailed after 2340ms)
[Application][ERROR] Module 'AssetService' timed out during startup (start_timeout_ms=10000, elapsed=10002ms, stage='Boot')
[Application][WARN]  Initiating rollback: reverting to stage 'Boot' due to module failure
[Application][INFO]  Rollback complete: returned to stage 'Boot'
[Application][ERROR] Rollback failed: module 'Logger' could not restart. Escalating to shutdown.
[Application][INFO]  Shutdown requested. Stopping all modules.
```

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Application.h` | Add — RequestShutdown, shutdown state, error policy logic |
| `Dia/DiaApplicationFlow/Application.cpp` | Add — rollback algorithm, shutdown sequence |
| `Dia/DiaApplicationFlow/ApplicationError.h` | Rewrite — v2 error types (v1 exists but different model) |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Update |

## Dependencies

- **Module Lifecycle** (this system) — BeginStart/BeginStop, ModuleState, timeout detection
- **Stage System** (this system) — transition context (which stage, boot vs non-boot)
- **DiaCore/Core/DIA_ASSERT** — debug assertions
- **DiaLogger** — structured error logging

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all IDs | Compliant — error diagnostics reference module/stage by StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — no STL in public error API |
| PD-007 | Platform | C++20 | Compliant |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::ApplicationFlow:: |
| SD-009 | DiaAppFlow | Assert debug, rollback release, shutdown boot | Compliant — this feature implements that decision exactly |
| SD-011 | DiaAppFlow | Shutdown is framework-level, not a stage | Compliant — RequestShutdown is a framework method, not a stage transition |
| SD-017 | DiaAppFlow | Clean break | Compliant — v1 error handling rewritten |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Rollback | Should rollback attempt to preserve retained ("all") modules, or restart them too? | Preserve. "all" modules never stop during transitions, so rollback doesn't affect them. They keep updating throughout. |
| 2 | Shutdown | Should stop_timeout during shutdown be enforced, or should we wait indefinitely? | Enforced with shorter grace (2x normal). If a module hangs during shutdown, log and force-continue. Don't hang the process. |
| 3 | Debug | Should DIA_ASSERT halt execution or just log and continue to rollback/shutdown? | Halt (that's what DIA_ASSERT does — breaks into debugger). Developer sees the failure site, then can continue if they choose. In automated test builds, assert = test failure + shutdown. |
| 4 | Multiple failures | If two modules fail simultaneously during a transition, how is that handled? | First failure triggers rollback. Second failure is logged but doesn't change the response (already rolling back). If the second is during rollback itself, escalate to shutdown. |
| 5 | Notification | Should the framework emit an event/callback on error for external consumers (e.g., editor)? | Yes, via IApplicationInspectable (Inspectable feature). Error state is queryable. No callback needed — polling is sufficient for editor refresh rates. |

## Open Questions

None.

## Status

`Approved` — 2026-05-09

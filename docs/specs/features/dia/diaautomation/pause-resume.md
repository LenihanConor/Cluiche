# Feature Spec: DiaAutomation Pause Resume

## Parent System
@docs/specs/systems/dia/diaautomation.md

## Depends On
@docs/specs/features/dia/diaautomation/module-and-build.md

## Purpose

Modules register cooperative pause/resume callbacks. The orchestrator sends `dia.automation.pause` or `dia.automation.resume` to invoke all registered callbacks. What "pause" means is entirely up to each module — DiaAutomation just stores and invokes the callbacks.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `RegisterPauseCallback(owner, pause, resume)` stores the entry. | Unit test |
| AC2 | `UnregisterPauseCallbacks(owner)` removes all entries for that owner. | Unit test |
| AC3 | `Pause()` invokes all registered pause callbacks in registration order; sets `mPaused = true`. `IsPaused()` returns true. | Unit test |
| AC4 | `Resume()` invokes all registered resume callbacks in registration order; sets `mPaused = false`. `IsPaused()` returns false. | Unit test |
| AC5 | `Pause()` while already paused is a no-op (callbacks not called again). | Unit test |
| AC6 | `Resume()` while not paused is a no-op. | Unit test |
| AC7 | `dia.automation.pause` command registered. Returns `{"paused": true}` in data. | Unit test via ExecuteCommandJson |
| AC8 | `dia.automation.resume` command registered. Returns `{"resumed": true}` in data. | Unit test via ExecuteCommandJson |

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004 |
| Application | @docs/specs/applications/dia.md | AD-002, AD-003 |
| System | @docs/specs/systems/dia/diaautomation.md | SD-AUT-001, SD-AUT-005, SD-AUT-006, SD-AUT-007 |

## Binding Decisions Compliance

| Binding Decision | Source | How This Feature Honors It |
|-----------------|--------|---------------------------|
| PD-004 (No STL containers) | Platform | Registry uses `DynamicArrayC<PauseResumeEntry, 16>`; callbacks are `std::function<void()>` |
| SD-AUT-006 (Cooperative callbacks) | DiaAutomation | `Pause()`/`Resume()` invoke registered fns; no frame suspension |
| SD-AUT-005 (Consistent envelope) | DiaAutomation | Both commands use `{success, data/error}` envelope |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should Pause/Resume emit lifecycle events? | Yes — `kAutomationPaused` / `kAutomationResumed` on `$lifecycle` stream per SD-AUT-008. |
| 2 | What if a pause callback throws or asserts? | Normal `DIA_ASSERT` behaviour — break in debug, continue in release. No special error handling in the pause loop. |

## Status

`Approved` (2026-05-21)

# Feature Spec: DiaAutomation Navigation Hold

## Parent System
@docs/specs/applications/dia/systems/diaautomation/diaautomation.md

## Depends On
@docs/specs/applications/dia/systems/diaautomation/module-and-build.md

## Purpose

AutomationService holds all stage transitions while automation is active. The orchestrator sends `dia.automation.navigate_to` to release the hold for one transition. After the transition commits, the hold re-arms automatically.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `EnableNavigationHold()` registers a transition guard with the Application that returns `Hold`. `IsHolding()` returns true. | Unit test |
| AC2 | `EnableNavigationHold()` called a second time is a no-op (idempotent). | Unit test |
| AC3 | `ReleaseNavigationHold(target)` validates target against `GetAllStages()`; returns `{"success": false, "error": "unknown stage: '<target>'"}` for unknown stages. Guard remains held. | Unit test |
| AC4 | `ReleaseNavigationHold(target)` for a valid stage: sets `mHolding = false`, calls `app.TransitionTo(target)`. Guard returns Allow on next check. | Unit test |
| AC5 | After the transition commits (`kStageTransitionCommitted` on `$lifecycle` stream), `mHolding` is set back to true (re-arm). | Unit test |
| AC6 | `dia.automation.navigate_to` command registered; params `{"target": "<stage>"}`. Calls `ReleaseNavigationHold`. Returns `{}` on success. | Unit test via ExecuteCommandJson |
| AC7 | While holding, `app.Update()` does not advance to the pending stage (guard blocks the transition). | Unit test |

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001 |
| Application | @docs/specs/applications/dia/dia.md | AD-003 |
| System | @docs/specs/applications/dia/systems/diaautomation/diaautomation.md | SD-AUT-001, SD-AUT-003, SD-AUT-005, SD-AUT-006, SD-AUT-007 |

## Binding Decisions Compliance

| Binding Decision | Source | How This Feature Honors It |
|-----------------|--------|---------------------------|
| PD-001 (StringCRC for IDs) | Platform | Stage target is `StringCRC`; command name is StringCRC |
| SD-AUT-003 (Re-arm after each transition) | DiaAutomation | $lifecycle reader sets `mHolding = true` on `kStageTransitionCommitted` |
| SD-AUT-005 (Consistent envelope) | DiaAutomation | navigate_to uses `{success, data/error}` envelope |

## Implementation Notes

- `EnableNavigationHold()` subscribes to `$lifecycle` EventStreamReader to watch for `kStageTransitionCommitted` — used to re-arm after transition.
- The guard fn is a lambda: `[this]() -> GuardResult { return mHolding ? GuardResult::Hold : GuardResult::Allow; }`
- `AutomationService` must be subscribed to `$lifecycle` via an `EventStreamReader<LifecycleEvent>` stored as a member.

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | How does AutomationService subscribe to $lifecycle? It's not a Module, so no `OnConnectStreams`. | AutomationService is given an `EventStreamReader<LifecycleEvent>` by the AutomationModule in its constructor or via a `ConnectStreams(IStreamStore*)` call after the module connects its own reader handle. The AutomationModule owns the reader handle; the service uses it. Alternatively: AutomationService takes a raw `IStreamStore*` pointer (from `Application::FindStream`) and creates an inline reader. |
| 2 | What if `ReleaseNavigationHold` is called while no hold is active? | No-op with a debug log warning. Safe to call multiple times. |

## Status

`Approved` (2026-05-21)

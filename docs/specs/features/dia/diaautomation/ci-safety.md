# Feature Spec: DiaAutomation CI Safety

## Parent System
@docs/specs/systems/dia/diaautomation.md

## Depends On
@docs/specs/features/dia/diaautomation/module-and-build.md

## Purpose

Belt-and-suspenders CI protection: `OnDisconnect()` for immediate connection-loss response; heartbeat monitor as backup for zombie orchestrators. Both paths release the navigation hold and call `RequestShutdown()`.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `OnDisconnect()` releases navigation hold (`mHolding = false`) and calls `app.RequestShutdown()`. | Unit test |
| AC2 | `OnDisconnect()` while no hold active still calls `RequestShutdown()`. | Unit test |
| AC3 | `EnableHeartbeatMonitor(timeout)` starts the timer; `mHeartbeatEnabled = true`. | Unit test |
| AC4 | `DisableHeartbeatMonitor()` stops the timer; `mHeartbeatEnabled = false`. | Unit test |
| AC5 | `ResetHeartbeat()` sets elapsed to 0. | Unit test |
| AC6 | `TickHeartbeat(dt)` increments elapsed; when elapsed > timeout, calls `OnDisconnect()` and disables the monitor. | Unit test |
| AC7 | `TickHeartbeat(dt)` while `mHeartbeatEnabled = false` is a no-op. | Unit test |
| AC8 | Any call to `RegisterCommands()`-registered commands calls `ResetHeartbeat()` before invoking the command logic. | Unit test — call a command, fast-forward heartbeat to near-timeout, call command again, verify OnDisconnect not triggered |

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaautomation.md | SD-AUT-001, SD-AUT-004, SD-AUT-007, SD-AUT-008 |

## Binding Decisions Compliance

| Binding Decision | Source | How This Feature Honors It |
|-----------------|--------|---------------------------|
| SD-AUT-004 (Connection-loss + heartbeat) | DiaAutomation | `OnDisconnect()` is primary; heartbeat fires `OnDisconnect()` as backup |
| SD-AUT-007 (Any command resets heartbeat) | DiaAutomation | `RegisterCommands()` wraps each callback to call `ResetHeartbeat()` first |
| SD-AUT-008 (Events on $lifecycle) | DiaAutomation | `OnDisconnect()` emits `kAutomationDisconnect`; timeout emits `kAutomationHeartbeatTimeout` before calling `OnDisconnect()` |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should heartbeat use wall-clock or game-clock dt? | Game-clock dt passed from `Application::Update(deltaTime)` — same real-elapsed time, unaffected by game-time scaling. Design decisions §10 confirms wall-clock intent is served by the app's update dt. |
| 2 | Who calls `TickHeartbeat`? | `AutomationModule::DoUpdate(float dt)` calls `mService->TickHeartbeat(dt)`. |
| 3 | What if `OnDisconnect()` is called after the app is already shutting down? | Guard: `if (app.IsShuttingDown()) return;` at the top of `OnDisconnect()`. |

## Status

`Approved` (2026-05-21)

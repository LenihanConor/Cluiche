# Feature Spec: Live Transition Trigger

## Parent System
@docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia/dia.md | AD-003 |
| System | @docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md | ED-003, ED-007, ED-009 |
| System (upstream) | @docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md | SD-004, SD-005 |

## Purpose

Allow the developer to trigger stage transitions on a running game directly from the editor. Select a target stage and issue the `transition_to` command. Provides visual feedback during the transition (waiting for modules to stop/start) and reports success or failure.

## Acceptance Criteria

1. **Transition To control** — UI element (button + stage dropdown) available when live-connected. Disabled when disconnected.
2. **Stage selector** — Dropdown populated with stages from the manifest. Current stage shown (non-selectable as target).
3. **Trigger command** — Sends `transition_to` DiaAPI command to the running game via WebSocket.
4. **Wait-for-ready feedback** — After triggering, UI shows "Transitioning…" state. Uses `wait_stage_ready` command (or observes `app.state` topic) to know when transition completes.
5. **Success feedback** — On completion: brief "Transition complete" toast. Live State Overlay updates to show new stage.
6. **Failure feedback** — If transition fails (module kFailed, timeout): "Transition failed" error with reason. Live State Overlay shows failed module(s) in red.
7. **Shutdown command** — Separate "Shutdown" button sends `request_shutdown` to gracefully stop the game.
8. **Block during transition** — Disable the transition control while a transition is in progress (no double-fire).

## Design

### React Component Structure

```
LiveTransitionPanel (sidebar section or header area, visible when connected)
├── StageSelector (dropdown of manifest stages, current highlighted)
├── TransitionButton ("Transition To" — disabled if target == current or transitioning)
├── TransitionStatus ("Transitioning to <stage>..." with spinner, or hidden)
├── ShutdownButton ("Shutdown Game" — destructive style)
└── (toast) TransitionResult ("Complete" green / "Failed: <reason>" red)
```

### Placement

Visible when live-connected. Can be placed:
- In the header area near the Live Connection button
- Or as a collapsible section in the sidebar

Available regardless of active tab (Graph/Presence/Streams) since it's a global action.

### Command Flow

1. User selects target stage from dropdown
2. Clicks "Transition To"
3. Button disabled, status shows "Transitioning to DummyStage…"
4. C++ backend sends `transition_to { stage: "DummyStage" }` via WebSocket
5. Backend listens for `app.state` update (stage changed) or timeout
6. On success: toast "Transition complete", button re-enabled, Live State Overlay updates
7. On failure: toast "Transition failed: ModuleX timed out", button re-enabled, overlay shows red

### Shutdown Flow

1. User clicks "Shutdown Game"
2. Confirmation dialog: "Shut down CluicheTest?"
3. On confirm: send `request_shutdown` command
4. Connection will drop (game closing) → Live Connection handles disconnect gracefully

### Timeout

If transition doesn't complete within 30 seconds (configurable), editor stops waiting and reports "Transition timed out — game may be stuck." Live State Overlay will continue showing whatever state the modules are in.

### Frontend Communication

- `editor.live.transitionTo(stageName)` → triggers transition
- `editor.live.shutdown()` → sends shutdown command
- Events: `onTransitionStarted(targetStage)`, `onTransitionComplete(stage)`, `onTransitionFailed(reason)`

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | `transition_to` command dispatch via WebSocket | Integration test: command sent, response received | Todo | |
| 2 | Transition state tracking (waiting, complete, failed) | Unit test: state machine transitions correctly | Todo | |
| 3 | Timeout handling (30s default) | Unit test: timeout fires, reports stuck | Todo | |
| 4 | `request_shutdown` command | Integration test: sends, handles disconnect | Todo | |
| 5 | CEF message handlers for transition/shutdown | Integration test: round-trip | Todo | |
| 6 | React `LiveTransitionPanel` with stage selector | Manual: dropdown populated, button works | Todo | |
| 7 | Transition status feedback (spinner, toast) | Manual: visual feedback correct | Todo | |
| 8 | Shutdown confirmation dialog | Manual: dialog appears, shutdown triggers | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Stage names matched by CRC between editor manifest and game runtime. |
| PD-007 | C++20 required | Uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | In `Dia::ApplicationFlow::Editor::`. |
| ED-003 | Live mode is overlay | Transition trigger is part of the live overlay UX, not a separate mode. |
| ED-007 | React + CEF frontend | UI in React; command dispatch in C++ backend. |
| ED-009 | Live button in header | Transition controls visible only when live button is green (connected). |
| SD-004 | TransitionTo is app-wide | Editor triggers app-wide transition — all PUs affected simultaneously. |
| SD-005 | Transitions async, queued | Transition command is queued; editor waits for completion asynchronously. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Safety | Should there be a confirmation before triggering a transition? | No — transitions are safe (the framework handles them gracefully). The stage selector already requires deliberate choice. Confirmation would add friction to a common debug workflow. |
| 2 | Stages | Should the dropdown include stages from the live game (which might differ from manifest)? | Use manifest stages. If the game has stages not in the manifest (stale editor), user needs to reload the manifest. Keep the source of truth clear. |
| 3 | Hot reload | Should "Transition To current stage" be available (triggers hot reload per SD-012)? | Yes — useful for hot reload. Show current stage in dropdown but label it "(reload)" instead of blocking selection. |
| 4 | Timeout | Is 30s the right timeout? | Yes for default. Module start_timeout_ms values in the manifest determine actual runtime timeout — editor's 30s is a generous ceiling for the UI to stop waiting. |

## Status

`Approved` — 2026-05-19

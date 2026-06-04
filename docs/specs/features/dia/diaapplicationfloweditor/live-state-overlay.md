# Feature Spec: Live State Overlay

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-007 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-003, ED-007, ED-008, ED-014 |
| System (upstream) | @docs/specs/systems/dia/diaapplicationflow.md | SD-016 |

## Purpose

When connected to a running game (Live Connection established), overlay runtime state information onto all editor views. Shows current stage, per-module state (running/loading/stopped/failed), transition progress, and stream throughput. Uses the `.tl` traffic-light dot primitive to indicate state across all views consistently (ED-008).

## Acceptance Criteria

1. **Overlay, not separate mode** — Runtime state is overlaid on the existing static views (ED-003). No mode switch; live data appears alongside config data.
2. **Current stage highlight** — The active stage is visually highlighted in all views where stages appear (Presence Grid column header, Stage Configuration list).
3. **Module state badges** — Each module's `.tl` dot reflects runtime state:
   - Green: running (DoStart returned kReady, update loop active)
   - Amber + pulse: loading (DoStart returning kLoading)
   - Red: failed (DoStart returned kFailed)
   - Grey: stopped (not in active stage)
4. **Transition progress** — During a stage transition, show which modules are stopping (red → grey) and which are starting (grey → amber → green). Animated sequence visible.
5. **PU state on Graph** — Graph View PU nodes show overall PU state via their `.tl` dot (green if all modules running, amber if any loading, red if any failed).
6. **Presence Grid live mode (ED-014)** — Active-stage column shows runtime dots. Inactive columns show outline-green for assigned (config truth). Active column header highlighted.
7. **Stream throughput** — Stream Inspector shows messages/sec when live (data from `get_stream_info` command).
8. **Graceful deactivation** — On disconnect, all overlays revert to offline state (grey dots, no highlights) immediately. No stale data shown.
9. **Topic reception** — Editor receives `live.connectionStatus`, `live.state`, `live.modules`, `live.streams` from the shared WebUIBridge topic bus. It does NOT subscribe to game topics directly.
10. **No Inspector dependency** — Editor renders correctly (offline mode) whether or not the Inspector plugin is loaded. Missing topics = offline state, not error state.
11. **Immediate transition** — When `live.connectionStatus` changes from connected→disconnected, all overlays revert to offline within the same render frame. No stale data shown for even one frame.

## Design

### Data Source (post-split)

Editor receives data via the shared WebUIBridge topic bus — **not** via direct WebSocket subscriptions:
- `live.connectionStatus` → {connected, host, port}
- `live.state` → current stage, transition state
- `live.modules` → per-PU module state list (instance_id → state enum)
- `live.streams` → per-stream throughput metrics

These topics are published by the sibling DiaApplicationFlowInspector plugin which owns the connection lifecycle. The Editor is a passive consumer.

### Topic Reception Model

On topic `live.connectionStatus` received with `connected: true`:
1. Mark live state active
2. Begin rendering overlays as subsequent `live.state`/`live.modules`/`live.streams` topics arrive

On topic `live.connectionStatus` received with `connected: false` (or no topic received at all):
1. Clear all live state immediately
2. Revert all views to offline rendering (same render frame — AC 11)
3. No error state — grey dots are the default

### Live State (Editor-side, read-only)

Post-split, the Editor no longer owns `LiveStateStore` (moved to Inspector). The Editor's TypeScript `useLiveStoreV2` is trimmed to a read-only subscriber:

```typescript
// useLiveStoreV2.ts (Editor version — trimmed)
interface LiveStoreState {
    connectionState: 'disconnected' | 'connected'
    activeStage: string | null
    modules: LiveModuleState[]
    streams: LiveStreamState[]
    // Setters only — called by topic push handlers in AppV2.tsx
    setConnectionState(state): void
    setActiveStage(stage): void
    updateModuleStates(modules): void
    updateStreamStates(streams): void
    clearLiveState(): void
}
// NO connect()/disconnect() actions — those live in Inspector
```

The C++ plugin receives topics via `WebUIBridge::SubscribeToTopic()` and forwards JSON payloads to the React UI.

### Frontend Rendering

React components check `liveState.isActive`:
- If active: render `.tl` dots with runtime colors + pulse animations
- If inactive: render grey dots (offline default)

The state is passed via React context so all views can access it without prop drilling.

### Traffic-Light Dot Mapping (ED-008)

| Runtime State | Dot Color | Pulse |
|--------------|-----------|-------|
| Stopped | Grey | No |
| Loading | Amber | Yes |
| Running | Green | Yes (subtle) |
| Failed | Red | No |

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | `LiveStateStore` class | Unit test: update/clear/query states | Todo | |
| 2 | WebSocket topic subscriptions on connect | Integration test: subscribes, receives updates | Todo | Depends on Live Connection |
| 3 | CEF bridge for live state updates to React | Integration test: state propagated to UI | Todo | |
| 4 | Graph View live dots (PU-level state) | Manual: PU dots reflect runtime | Todo | |
| 5 | Presence Grid live mode (ED-014) | Manual: active column shows runtime, inactive shows outline | Todo | |
| 6 | Module Inspector live dot | Manual: selected module shows runtime state | Todo | |
| 7 | Stream throughput display in Stream Inspector | Manual: shows msg/sec when live | Todo | |
| 8 | Transition animation (stopping/starting modules) | Manual: dots animate during transition | Todo | |
| 9 | Graceful deactivation on disconnect | Integration test: all state cleared, views revert | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | Module/PU/stream state matched by CRC to manifest model. |
| PD-007 | C++20 required | Uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | In `Dia::ApplicationFlow::Editor::`. |
| ED-003 | Live mode is overlay | State overlaid on static view. No separate mode. |
| ED-007 | React + CEF frontend | UI in React; state store in C++ backend. |
| ED-008 | Traffic-light dot primitive | All runtime state shown via .tl dots (grey/amber/green/red + pulse). |
| ED-014 | Presence Grid two modes | Active-stage column uses runtime dots; inactive uses outline-green. |
| SD-016 | IApplicationInspectable | Live data comes from IApplicationInspectable exposed via DiaAPI. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Update rate | How frequently does live state update? | Push-based: DiaDebugServer sends updates on state change (not polled). Module state changes are event-driven — only fires when a module transitions. Stream throughput is sampled every 1s server-side. |
| 2 | Mismatch | What if live game has modules not in the loaded manifest? | Ignore them — overlay only applies to modules in the editor's manifest model. Unknown module IDs from live data are silently dropped. |
| 3 | Stale | What if the manifest has been edited since connecting (e.g., module added but game not restarted)? | Overlay shows state for modules the game knows about. New manifest-only modules show grey (no live data). Visual cue that game is running stale config. |
| 4 | Performance | Could rapid state updates cause UI jank? | Throttle React re-renders to 10 Hz max (100ms debounce on state propagation). Module state changes are infrequent in practice. |

## Status

`Approved` — 2026-05-19

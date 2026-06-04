# Feature Spec: Live Connection

## Parent System
@docs/specs/systems/dia/diaapplicationfloweditor.md

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-007 |
| Application | @docs/specs/applications/dia.md | AD-003 |
| System | @docs/specs/systems/dia/diaapplicationfloweditor.md | ED-003, ED-007, ED-009 |

## Purpose

Provide WebSocket connection management between the editor and a running game instance. This is the communication foundation for all live mode features (Live State Overlay, Live Transition Trigger). Uses DiaEditor's existing GameConnectionManager infrastructure.

## Acceptance Criteria

1. **Connect button** — 3-state button in the editor header (ED-009):
   - Grey: "Connect Live" (disconnected)
   - Amber + pulse: "Connecting…" (handshake in progress)
   - Green + pulse: "Live: \<AppName\>" (connected, shows game name)
2. **Manual connect** — User must explicitly click to connect (system AI Review Q2: no auto-connect).
3. **Connection via GameConnectionManager** — Uses DiaEditor's existing WebSocket connection infrastructure. Does not create a new WebSocket client.
4. **Disconnect** — Clicking the button while connected shows "Disconnect?" confirmation, then disconnects cleanly.
5. **Connection loss handling** — If connection drops unexpectedly: button reverts to grey, toast notification "Connection lost", all live overlays deactivate gracefully.
6. **Connection status events** — Emits events for other features: `onConnected(appName)`, `onDisconnected`, `onConnectionLost`.
7. **Header-visible** — Connection state visible in header regardless of which tab (Graph/Presence/Streams) is active (ED-009).

## Design

### Integration with DiaEditor Framework

The DiaApplicationFlowEditor plugin uses `IEditorContext::GetGameConnectionManager()` to access the shared connection. The GameConnectionManager handles:
- WebSocket lifecycle (connect, heartbeat, disconnect)
- Message routing to registered plugins
- Connection discovery (localhost:port from game's DiaDebugServer)

This feature registers the plugin as a connection listener and exposes connection state to the React UI.

### Connection Flow

1. User clicks "Connect Live"
2. Button → amber + pulse ("Connecting…")
3. C++ backend: `GameConnectionManager::Connect(endpoint)`
4. On success: query `get_app_state` for app name
5. Button → green + pulse ("Live: CluicheTest")
6. Emit `onConnected("CluicheTest")` → other features activate live overlays

### Disconnect Flow

1. User clicks connected button
2. Confirmation: "Disconnect from CluicheTest?"
3. On confirm: `GameConnectionManager::Disconnect()`
4. Button → grey ("Connect Live")
5. Emit `onDisconnected` → all live overlays deactivate

### Connection Loss

1. GameConnectionManager detects heartbeat failure
2. Calls `OnConnectionLost()` callback
3. Button → grey
4. Toast: "Connection to CluicheTest lost"
5. Emit `onConnectionLost` → overlays deactivate gracefully (no error state, just revert to offline view)

### React Component

```
LiveConnectionButton (header widget)
├── StateIndicator (.tl dot: grey/amber/green with pulse)
├── Label ("Connect Live" / "Connecting…" / "Live: <AppName>")
└── (on click) ConnectAction or DisconnectDialog
```

### Frontend Communication

- `editor.live.connect()` → initiates connection
- `editor.live.disconnect()` → clean disconnect
- `editor.live.getStatus()` → returns `{ state: "disconnected"|"connecting"|"connected", appName? }`
- Events: `onLiveStatusChanged({ state, appName? })`

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | Register plugin with GameConnectionManager callbacks | Unit test: callbacks fire on connect/disconnect | Todo | |
| 2 | Connection lifecycle management (connect, query app name, disconnect) | Integration test: full connect/disconnect cycle | Todo | |
| 3 | Connection loss detection and graceful deactivation | Integration test: simulate loss, verify state revert | Todo | |
| 4 | CEF message handlers for connect/disconnect/status | Integration test: round-trip | Todo | |
| 5 | React `LiveConnectionButton` with 3-state rendering | Manual: button reflects all states | Todo | ED-009 |
| 6 | Disconnect confirmation dialog | Manual: dialog appears, buttons work | Todo | |
| 7 | Connection events emitted to other features | Integration test: subscriber receives events | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-007 | C++20 required | Uses C++20. |
| AD-003 | Namespace Dia::\<Module\>:: | In `Dia::ApplicationFlow::Editor::`. |
| ED-003 | Live mode is overlay on static view | Connection establishes live mode; other features overlay. No separate mode. |
| ED-007 | React + CEF frontend | Button in React; connection management in C++ via DiaEditor framework. |
| ED-009 | 3-state live button in header | Grey/amber+pulse/green+pulse as specified. |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Discovery | How does the editor know the game's WebSocket endpoint? | GameConnectionManager handles discovery — checks localhost on DiaDebugServer's default port (configurable). Editor plugin doesn't manage endpoints directly. |
| 2 | Multiple games | What if multiple games are running? | GameConnectionManager connects to one at a time. If multiple detected, shows a selection dialog (handled by DiaEditor framework, not this plugin). |
| 3 | Reconnect | Should there be an auto-reconnect on connection loss? | No — manual reconnect only. Auto-reconnect could mask underlying issues (game crashed, wrong instance). User clicks "Connect Live" again. |
| 4 | Performance | How often is live data polled? | Not polled — DiaDebugServer pushes state updates via WebSocket subscriptions. This feature establishes the connection; Live State Overlay subscribes to topics. |

## Status

`Moved` — 2026-06-04. Connection lifecycle (connect/disconnect button, callbacks, topic subscriptions) moved to [DiaApplicationFlowInspector](../../../systems/dia/diaapplicationflowinspector.md). Editor retains only a read-only connection status indicator — see [connection-status-indicator.md](connection-status-indicator.md).

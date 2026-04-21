# Feature Spec: Game Connection Panel

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Game Connection Panel** | (this document) |
| Related | WebSocket Client | @docs/specs/features/dia/diaeditor/websocket-client.md |

## Problem Statement

The editor needs a visible, first-class surface for managing its live connection to a running game. Without a dedicated panel, connection state is invisible: the user cannot tell whether they are connected, to which game, or what that game exposes. The existing `GameConnectionManager` module (see WebSocket Client feature) provides the transport but has no UI affordance — no way to type a host/port, no way to trigger a connect, no way to see "what game am I looking at?" after a connection succeeds.

This feature specifies the editor-side panel that surfaces `GameConnectionManager`'s state and drives its lifecycle. It is intentionally scoped to *just* the panel and its protocol with the C++ side; the actual game-server implementation inside CluicheTest (or any game) is a separate, downstream piece of work.

## Acceptance Criteria

**Disconnected view:**
- [ ] Panel always registered by `EditorView` (like Home and Output Console) so it is present whether or not a project is loaded
- [ ] Shows a clear "Disconnected" status with a dark-palette-consistent treatment
- [ ] Provides an input field for the game URL (default suggested: `ws://localhost:9002/`)
- [ ] Provides a "Connect" button that invokes the C++ side
- [ ] Displays the last-attempted URL and the reason for the last failure (if any)
- [ ] The last-used URL is persisted between editor runs (Decision 9: persistent URL)

**Connected view:**
- [ ] Shows a "Connected" status with the URL currently connected to
- [ ] Shows game-reported identity: game name, build version, processing-unit count, current phase (populated from a `game_info` payload the game publishes on connect)
- [ ] Shows a "Disconnect" button
- [ ] Shows heartbeat health (last heartbeat received; Decision 3: 10-second heartbeat)
- [ ] On unexpected disconnect, transitions back to the disconnected view and requires the user to click Connect again (Decision 5: no silent auto-reconnect)

**Protocol / behavior:**
- [ ] Connection is **user-initiated only** — the editor does not attempt to connect on boot (Decision 4)
- [ ] At most **one active connection** at a time (Decision 6); "game is the server, editor is the client" (Decision 7)
- [ ] Transport is **JSON over WebSocket** (Decision 1) as already specified by `GameConnectionManager`
- [ ] Protocol is **pure connect** — plain WebSocket frames, no Engine.IO / SignalR / socket.io envelope (Decision 10)
- [ ] Heartbeat: editor sends `ping` every 10s, game replies with `pong`; if no `pong` within 2x interval, treat as disconnected (Decision 3)

## Design

### Scope boundary — panel vs game server

This spec covers **only the editor-side panel and its C++ glue**. Writing a game-side `DiaDebugServer` that answers `connect` / `game_info` / `ping` is out of scope for this feature; Decision 8 acknowledged "we will have to add it." The panel is built against a stub/mock initially (responds with canned `game_info`) and wired to CluicheTest in a follow-up.

### Asset layout

```
Dia/DiaEditor/Plugin/Assets/gameconnection/
    index.html       (self-contained panel; listens to "game_connection" topic via postMessage)
```

Registered as a built-in panel in `EditorView::Initialize`:

```cpp
RegisterComponent("Game Connection", "dia://editor/gameconnection/index.html");
```

### UI states

Panel is a finite state machine with three visible states:

| State | Trigger | What it shows |
|-------|---------|---------------|
| `disconnected` | boot, explicit disconnect, transport drop | URL field + Connect button + last error |
| `connecting` | user clicks Connect, handshake in flight | "Connecting to `<url>`..." + Cancel |
| `connected` | handshake complete + first `game_info` received | Identity card + heartbeat indicator + Disconnect |

A dropped connection transitions `connected` → `disconnected` with a message (never silently retries — Decision 5).

### Topic + message shape

Panel is driven by two WebUIBridge surfaces:

**Request/response (panel → C++):**

| Request id | Payload | Response |
|------------|---------|----------|
| `game_connection.connect` | `{ "url": "ws://..." }` | `{ "ok": true }` or `{ "ok": false, "error": "..." }` |
| `game_connection.disconnect` | `{}` | `{ "ok": true }` |
| `game_connection.get_state` | `{}` | `{ "state": "disconnected" \| "connecting" \| "connected", "url": "...", "info": { ... } }` — used on panel load so new iframes pick up current state |

**Topic push (C++ → panel, over the existing `DiaEditor_onDataChanged` envelope + iframe `postMessage` re-broadcast):**

| Topic | Payload | When |
|-------|---------|------|
| `game_connection` | `{ "state": "...", "url": "...", "info": { ... }, "lastError": "..." }` | Every state transition; also on heartbeat timeout |
| `game_connection_heartbeat` | `{ "lastPongMs": 12345 }` | On each `pong` so the panel's "last heartbeat" indicator can tick |

Both topics reach the iframe via the same `window.postMessage({__dia:true, topic, data})` pattern used by Output Console.

### C++ glue

The panel does not talk to `GameConnectionManager` directly — it goes through a new `GameConnectionController` (or equivalent wiring on `EditorViewController`) that:

1. Registers the `game_connection.*` WebUIBridge handlers
2. Translates handler calls into `GameConnectionManager::ConnectRemote / Disconnect`
3. Observes connection-state changes and publishes `game_connection` topic pushes
4. Owns the heartbeat timer and publishes `game_connection_heartbeat` pushes
5. Persists the last-used URL to the same `Data/` folder used for layout persistence

The existing `GameConnectionManager` (WebSocket Client feature) is reused as-is. This spec adds the UI + the controller glue, not new transport.

### Persistent URL

Decision 9 — the URL field is pre-populated with the last successfully-used value. Stored alongside `editor-layout.json` (e.g., `Data/editor-connection.json`) using the same "project-adjacent for now, move later" strategy documented in the Docking Layout spec. If the file is absent, the panel suggests `ws://localhost:9002/` as the default.

### Heartbeat (Decision 3)

- Editor sends `{ "type": "ping", "ts": <ms> }` every 10s while in `connected` state
- Game replies with `{ "type": "pong", "ts": <same ts> }`
- If 20s pass without a `pong`, the controller tears down the socket and transitions to `disconnected` with `lastError = "Heartbeat timeout"`
- Panel shows "last heartbeat Ns ago" so the user has a live health signal

### What the panel does NOT do (scope)

- Does not expose raw WebSocket message logs — that belongs in Output Console
- Does not expose data subscriptions / `SubscribeToData` UI — that's a future "Data Explorer" panel
- Does not attempt multi-game or per-PU connections — single connection only (Decision 6)
- Does not attempt silent auto-reconnect (Decision 5)

## Implementation Files

- `Dia/DiaEditor/Plugin/Assets/gameconnection/index.html` — panel content; self-contained state machine
- `Dia/DiaEditor/MVC/EditorView.cpp` — register `"Game Connection"` built-in panel in `Initialize`
- `Dia/DiaEditor/LiveConnection/GameConnectionController.{h,cpp}` **(new)** — registers `game_connection.*` WebUIBridge handlers, drives `GameConnectionManager`, publishes topics
- `Dia/DiaEditor/LiveConnection/GameConnectionManager.{h,cpp}` — reused (no signature change required beyond exposing a connection-state callback, which the spec already includes as `SetConnectionCallback`)
- `Cluiche/CluicheEditor/CluicheEditor.vcxproj` — existing `Plugin/Assets/*` copy step picks up the new `gameconnection/` folder automatically

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — topic names / request ids hashed to StringCRC at dispatch |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — `GameConnectionController` is a Module composed under `EditorApplication` alongside `GameConnectionManager` |
| Platform | PD-004 | No STL in public APIs | **Compliant** — controller surface uses `const char*`, `Json::Value`, `StringCRC`, `DynamicArrayC`; leverages `Dia::Core::Functor` for callbacks |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — no new project; new files added to existing `DiaEditor.vcxproj` |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — all new code in `Dia::Editor::` |
| DiaEditor | SED-004 | WebSocket protocol uses JSON | **Compliant** — ping/pong/connect envelopes are JSON-over-WebSocket |
| DiaEditor | SED-005 | CEF replaces Awesomium | **Compliant** — panel is a CEF iframe |
| DiaEditor | SED-008 | Observer pattern (not polling) | **Compliant** — panel is driven by topic pushes, not by polling `get_state` on a timer |
| DiaEditor | SED-010 | Use DiaDebugProtocol for wire types | **Compliant** where applicable — `game_info` / `ping` / `pong` envelope shapes must be reused from / added to DiaDebugProtocol (the "add it" half of Decision 8) |

**All binding decisions: COMPLIANT**

## Open Questions

**Resolved (from interview, 2026-04-20):**
- **Decision 1:** Transport format — JSON over WebSocket (default)
- **Decision 2:** Handshake — `connect` request immediately followed by game sending `game_info` as its first payload
- **Decision 3:** Heartbeat interval — 10 seconds (ping/pong)
- **Decision 4:** Who initiates — user-initiated only; no connect-on-boot
- **Decision 5:** Disconnect recovery — require user to click Connect again; no silent auto-reconnect
- **Decision 6:** Connection cardinality — single active connection
- **Decision 7:** Roles — game is the server, editor is the client
- **Decision 8:** Where does `game_info` come from on the game side — does not exist yet; we will add it to DiaDebugProtocol / DiaDebugServer in a follow-up
- **Decision 9:** URL persistence — yes, persist last-used URL across editor sessions
- **Decision 10:** Protocol envelope — pure WebSocket frames; no socket.io / Engine.IO / SignalR layer

**Deferred:**
- Data subscription UI (future "Data Explorer" panel)
- Multi-game / multi-PU connections
- Live log streaming from the game into the editor's Output Console (requires wiring a subscription, not a panel change)

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Registration | Built-in panel or plugin? | Built-in | Built-in — connection is editor infrastructure, not plugin concern |
| 2 | State model | How many discrete UI states? | 3 | disconnected / connecting / connected — see state table |
| 3 | Initial connection | Auto-connect on boot? | No | No (Decision 4) — user-initiated only |
| 4 | Auto-reconnect | Silent retry on drop? | No | No (Decision 5) — show error, require manual reconnect |
| 5 | URL persistence | Remember last URL? | Yes | Yes (Decision 9) — stored in `Data/editor-connection.json` |
| 6 | Heartbeat | Interval and timeout? | 10s / 20s | 10s interval (Decision 3); 20s (2× interval) → disconnect |
| 7 | Cardinality | Multiple connections? | No | Single connection only (Decision 6) |
| 8 | Identity | What does "connected" show? | Game name/version/PU count/phase | Populated from first `game_info` payload after handshake |
| 9 | Transport | Raw WebSocket or framework? | Pure WebSocket | Pure (Decision 10) — plain JSON frames |
| 10 | Scope | Include data subscriptions? | No | No — separate "Data Explorer" panel later |
| 11 | Error surface | Where do connect failures go? | Inline in panel + Output Console | Both — panel shows `lastError`; also logged via `PushConsoleEntry("error", ...)` |
| 12 | Testing | How to validate without a real game? | Stub server | Build a minimal stub that answers `connect`/`game_info`/`ping` so the panel can be exercised before CluicheTest is wired |
| 13 | Reuse | Does `GameConnectionManager` need new methods? | Possibly `GetLastError()` | Yes — add `GetLastError()` returning a `const char*` so the controller can surface it in the `game_connection` topic payload |
| 14 | Topic naming | Should topic namespace distinguish state vs heartbeat? | Separate topics | Separate — `game_connection` vs `game_connection_heartbeat` so the panel can throttle only the heartbeat without losing state updates |
| 15 | Failure after initial `game_info` | What if `game_info` never arrives? | Timeout | If `game_info` not received within 5s of socket open, treat as failed handshake → `disconnected` with `lastError = "Handshake timeout"` |

## Implementation Plan (v0 slice)

Implemented in two slices per user preference (option "b"):

**Slice 1 — editor panel with stub server:**
1. Add `gameconnection/index.html` panel + register in `EditorView::Initialize`
2. Add `GameConnectionController` that wires `game_connection.*` WebUIBridge handlers and topic pushes
3. Stand up a minimal in-process mock that answers `connect` → `{ok:true}` + publishes a canned `game_info` topic — enough to walk every UI state without any real socket
4. Verify: panel shows disconnected → click Connect → connecting → connected with stub identity; click Disconnect → back to disconnected

**Slice 2 — wire CluicheTest as real game server:**
1. Add the `game_info` / `ping` / `pong` envelopes to `DiaDebugProtocol` (Decision 8's outstanding half)
2. Extend `DiaDebugServer` to respond to them with real PU/phase data
3. Swap the mock for a real `ws://localhost:9002/` connection into CluicheTest
4. Verify: launching CluicheTest and CluicheEditor side-by-side, the panel connects and shows CluicheTest's identity

## Status

`Draft` - ready for user approval to proceed with Slice 1 implementation

# System Spec: DiaViewLaunch

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** debug, tools

**Research:** @docs/research/editor_deep_link_launch/summary.md (claims audited against the code 2026-11-10; corrections are folded in below)

**Related:** @docs/specs/applications/dia/systems/diaeditor/editor-deep-link.md (the existing `OnNavigate(StringCRC)` catalogue deep link; this system adds a typed view-state hook next to it and does not change it)

## Purpose

DiaViewLaunch lets a script, an AI agent or a developer open **one view of a running game**, already connected and already showing the right thing, while opening as little as possible. When no one needs to look, it returns **the same data as JSON** instead. It does this through one command line:

```
dia query perf.window --game cow --args '{"seconds": 10}'      # numbers, no window
dia view entity --game cow --state '{"entity": "dragon_03"}'   # one small window, connected
```

Today the same thing takes seven manual steps in CluicheEditor (open the editor, load the `.diagame`, open Game Connection, type the port, connect, open the plugin, find the item), and none of them can be scripted. The system is generic: any Dia game with a DebugServer and any Inspector-tier plugin gets it.

## Responsibilities

- **Game discovery:** every running game with a DebugServer writes a discovery record (pid, port, game, `.diagame`, session id, token). Tools resolve `--game cow` from it, without a port, and ignore dead processes.
- **Connection token:** a per-launch secret the DebugServer requires on connect, published only in the discovery record.
- **Headless queries:** `dia query` sends any DebugServer query or command to a resolved game and prints the reply as JSON, with exit codes a script can branch on. It is built on a shared Python DebugServer client that the E2E harness also uses.
- **View request:** one typed `ViewRequest` document (plugin, game, connect target, view state). `dia view` builds it, validates it, resolves the game, picks the host and launches it, then returns JSON.
- **View-state contract:** plugins declare the view-state parameters they accept and receive them through a typed hook once their UI is ready.
- **Hosts:** the headless plugin host (`DiaPluginHost`) runs one unchanged editor plugin without CluicheEditor or CEF. Editor focus mode (`CluicheEditor --only <plugin>`) is the pre-agreed fallback.

## Non-Responsibilities

- **Steering an already-open CluicheEditor** (`editor.*` DiaAPI commands over a control socket). Deferred until someone needs scripted control of the full editor.
- **`cluiche://` links, saved view presets and the in-game "Inspect in Editor" button.** These are thin front ends that produce a `ViewRequest`, and they follow once a view launches reliably.
- **Editor-tier (offline file) plugins.** v1 targets Inspector-tier plugins, which attach to a live game (tool-tier rule: Editor = offline files, Inspector = live game over DiaDebugServer). Offline editors stay in CluicheEditor.
- **Remote connections.** The DebugServer binds 127.0.0.1 and only accepts localhost origins (fixed 2026-11-10). Nothing here reopens that.
- **New plugin UIs.** The host shows each plugin's existing UI unchanged.

## Design

### Shape

```
caller (shell / script / AI)
   │  dia query <name> ...           dia view <plugin> ...
   ▼                                  ▼
DiaCLI ── discovery reader ──► Cluiche/out/<App>/sessions/<id>/discovery.json
   │                                  │ (written by the game's DebugServer host)
   │ WebSocket + token                │ ViewRequest file
   ▼                                  ▼
DebugServer (game)  ◄──── token ──── DiaPluginHost  (or CluicheEditor --only)
                                       ├─ one IEditorPlugin, unchanged C++ half
                                       ├─ GameConnectionManager (PluginServiceLocator)
                                       └─ shell page + plugin UI over 127.0.0.1
```

### Discovery record

- **Where:** `Cluiche/out/<App>/sessions/<session-id>/discovery.json`, inside the session folder DiaObservation already creates (user decision 2026-11-10). This gives one session concept with the logs next to it. A file inside a session folder does not disturb the E2E harness, which picks the newest folder under `sessions/` by name.
- **Contents:** `format: "dia.discovery"`, `version: 1`, `pid`, `process_start` (process creation time, to defeat pid reuse), `port`, `game` (name, build), `diagame` (absolute path), `session_id`, `token`, `protocol_version`, `started_utc`, and `stopped_utc` (written on clean stop).
- **Liveness:** a record is live when `stopped_utc` is absent, the pid exists and its creation time matches `process_start`. Readers never delete records, so a crashed game's record simply reads as dead.
- **Plumbing:** the DebugServer does not know the session id or the out root today (the host module passes `""`). `DebugServerHostModule` gets both from the observation session. It writes the record after `StartServer` succeeds, so the port in the record is the bound port.
- **Ports:** CluicheTest and CoW both use 9002, so two games cannot run at once. `.diaapp` DebugServer config gains `"port": "auto"`, which binds a free port and reports it in the record. The default stays 9002 so existing editor setups keep working.

### Connection token

- The game generates 128 random bits per launch (hex) before `listen`, and writes them only into the discovery record.
- The client sends the token as a URL query parameter: `ws://127.0.0.1:<port>/?token=<hex>`. `DiaWebSocket::Server`'s existing validate handler (where the Origin check already runs) rejects a missing or wrong token with HTTP 403 before the socket opens. This needs no protocol message change and no "nothing before handshake" gate. The token is immutable once listening, so the WebSocket thread reads it without crossing a PU boundary.
- `.diaapp` DebugServer config: `"auth": "token" | "none"`, default `token`. `none` is for raw-socket unit tests only.
- **Every in-repo client adopts it in the same feature:** the editor's `GameConnectionManager`/`GameConnectionController` (which hard-code `ws://localhost:9002` in `editor-connection.json` and `ConnectionButton.tsx`) resolve the token from the discovery record whose port matches; the E2E `DiaClient`; `dia query`; and the host.
- Clients connect to `127.0.0.1`, not `localhost`, because the server binds IPv4 loopback only.

### Headless queries (`dia query`)

- `DiaClient` moves from `Cluiche/Tests/E2E/client.py` to `dia_cli/utils/debug_client.py`, and E2E imports it from there. It drains the unsolicited `HANDSHAKE_RESPONSE` and `GAME_INFO` messages and checks that `GAME_INFO.name` matches the resolved game.
- `dia query <name> [--args <json>] [--game <name> | --session <id> | --port <n>] [--timeout <s>]` sends one `command_request` (the DebugServer routes it through QueryRegistry, then sim-hosted, then DiaAPI) and prints `{ok, game, session_id, name, result | error}`.
- `dia query --list` prints the merged command catalogue (`list_commands`, including sim-hosted help).
- `dia query games` lists live discovery records.
- **Exit codes:** 0 ok; 2 no matching live game; 3 more than one match (the reply lists the candidates); 4 token or connection refused; 5 command error reply; 6 timeout; 7 launch failed (`dia view --launch` only).
- **Request ids (VL-011):** `CommandRequest` gains an optional `request_id`, which the server copies into `CommandResponse`. The shared client always sends one and matches replies by it, so concurrent queries are unambiguous. Clients that send none are matched by command name, as today.

### View request

- **`ViewRequest`** (`format: "dia.view_request"`, `version: 1`): `plugin` (plugin type id string), `game` (`.diagame` name or path, PD-010), `connect` (`auto` | `{session_id}` | `{port}`), `state` (object, validated against the plugin's view-state schema), and an optional `host` override (`headless` | `editor`).
- **Python side** (`dia_cli`): the authoritative builder and validator. `dia view <plugin> [--game] [--session] [--state <json>] [--request <file>]` writes the resolved request to `Cluiche/out/DiaViewLaunch/requests/<id>.json` and launches the host through `dia launch` machinery (the DiaCLI rule: never call the exe directly). `dia launch` gains argument passthrough for this. The command prints `{ok, plugin, host, pid, session_id, window}`.
- **`--launch` (VL-009):** without it `dia view` attaches only (exit 2 when no game is live). With it, `dia view` runs `dia launch <target>` for the request's game, waits up to 60 s for a *new* live discovery record for that game (`started_utc` after the launch), then continues as usual. A failed or timed-out launch exits 7 and includes the launch's own error.
- **C++ side** (DiaEditor `ViewLaunch/`): `ViewRequest` (Dia containers, PD-004; state held as a `Dia::Core::Json` value) and `ViewRequestLoader`, used by both hosts.
- **Host choice:** there is no per-plugin manifest file today (plugin metadata is compiled into `EditorPluginMetadata` and registered with `REGISTER_EDITOR_PLUGIN`). `EditorPluginMetadata` gains a `viewHost` field (`Headless` | `Editor`), plus a `tier` field (`Inspector` | `Editor`) for VL-008. The default is `Editor` until Feature 6 lands, then `Headless`. A request's `host` override wins. Callers never see which host ran.
- **How Python learns it:** both hosts support `--list-plugins`, which prints every registered plugin's id, name, tier, `viewHost` and whether its UI is deployed as JSON, then exits. `dia view list` runs it (cached per build). `dia view describe <plugin>` runs `--describe <plugin>` (Feature 5).

### View-state contract

- `IEditorPlugin` gains two default no-op virtuals (SED-001), next to the existing `OnNavigate(StringCRC)`:
  - `GetViewStateParams()` returns the accepted parameters as DiaValueSchema `ValueParam`s (name, type, range, default, description), so `describe` and validation reuse the existing value-description machinery.
  - `OnApplyViewState(const Dia::Core::Json&)` receives the validated state.
- **Delivery:** the host holds the state until the plugin UI sends `view.ready`, applies it then, and re-applies it if the UI reloads (CEF/iframe state is lost on unload, SED-018).
- **First adopters:** Entity Inspector (`entity`), ApplicationFlow Inspector (`stage`, `module`). Perf Inspector adopts when it is built (`unit`, `window_s`, `frame`, `module`).
- **Describe without a live host:** both hosts support `--describe <plugin>`, which prints the schema JSON and exits, so `dia view describe` works with no game running.

### Headless plugin host (`DiaPluginHost`)

- A new Dia application project (`Dia/DiaPluginHost/`, user decision 2026-11-10). It loads exactly one registered `IEditorPlugin` (from `EditorPluginRegistry`, the same `REGISTER_EDITOR_PLUGIN` static registration the editor uses) with an `EditorPluginContext` holding:
  - **a `WebUIBridge` backed by a new WebSocket `IJSBridge`** (DiaEditor), in place of CEF. `WebUIBridge` is CEF-free in its includes, but `Initialize()` asserts a non-null `EditorViewController`, so the host supplies a minimal controller. The bridge's `CallJSFunction("DiaEditor_onResponse" | "DiaEditor_onDataChanged", json)` calls become `{fn, args}` WebSocket frames that the shell page turns into the same `window.*` calls CEF makes.
  - **a minimal `EditorModel` with the request's `.diagame` loaded.** The AssetRuntime Inspector reads `GetModel()->GetDiagameProject()` and overrides `OnProjectChanged` (audit 2026-11-10), so a null model is not enough.
  - **a `PluginServiceLocator` with a `GameConnectionManager`** registered *before* the plugin loads (`LiveConnectionPluginBase` fetches it once, at load). The host calls `Initialize()`, pumps `Update()` every frame and calls `Connect(127.0.0.1, port)` with the token from the request (SED-016). These are the jobs `GameConnectionEditorPlugin` does in the editor. The host does not load that plugin, because it also pulls the view, `GameSwitchController` and action registry, and auto-connects to `ws://localhost:9002`.
  - a null view and plugin loader (no Inspector uses them).
- **Serving the UI.** CEF serves plugin UIs from a custom `dia://` scheme on disk (`dia://ui/…`, `dia://plugins/<name>/…`, `dia://theme/*.css`), injects the theme `<link>`s into HTML under `/plugins/`, and guards against `..` traversal. The host serves the same layout and does the same injection on 127.0.0.1, so relative paths such as `../dia-live-connection.js` keep working. It sits behind the same Origin check and token.
- **The shell page plays the CluicheEditor parent frame.** It iframes the plugin UI and relays the editor bridge protocol (`__diaFromFrame` in; `__diaResponse` to the originating iframe; `__dia` topics to all iframes; `editor.notify` handled locally). CEF delivers everything to the main frame only, so the shell keeps the request-to-iframe map, as `EditorBridge.ts` does. The relay is **extracted from `CluicheEditor/UI/src/bridge/EditorBridge.ts` (220 lines) into a shared module** used by both shells, not copied, so the two cannot drift.
- **Coverage today.** Of the seven Inspectors, the AI Inspector has no `UI/`. The Economy and ScalarField UIs are not deployed by `pipeline.toml`. AssetRuntime's `uiPath` points at a `dist/` the pipeline never builds. The host shows a clear "plugin UI not deployed" page for these rather than a blank window. Fixing their deployment is tracked in BACKLOG, not here.
- **Window (VL-010):** `dia view` opens `msedge --app=<url> --user-data-dir=Cluiche/out/DiaViewLaunch/edge-profile`. The isolated profile stops Edge from handing the window to a running Edge process and returning at once. If `msedge` is not found, the default browser opens a tab and the result says `"window": "browser"`.
- **Lifetime:** judged by the shell page's WebSocket, never the browser process. When the page disconnects and stays gone for about 5 s the host exits. It also exits when the game's connection ends and the request says `exit_on_disconnect`.
- **Task 1 is a spike, and it gates the rest.** The response-path question from the research is answered by the audit (see "Editor bridge prerequisite" below). The spike must still answer:
  1. **Static serving.** `DiaWebSocket::Server` is WebSocket-only (no HTTP handler is set). Either add a websocketpp HTTP handler for the host's own server (one port for files and socket), or serve the files another way. It must reproduce the `dia://` layout and theme injection.
  2. **Bridge and relay size.** How big are the WebSocket `IJSBridge`, the minimal `EditorViewController` and `EditorModel` setup, and the extracted shell relay? Prove it by loading the Entity Inspector end to end against a running CluicheTest.
- If the spike fails, Feature 7 is built instead and plugins default to `view_host: editor`.
- **Mockup gate:** the shell page's chrome (title strip with game, session and connection state; reconnect; the token-refused and game-gone states) gets an approved HTML mockup before the host is built.

### Editor bridge prerequisite (existing bugs, found by the audit 2026-11-10)

Request/response between plugin UIs and their C++ halves is broken today for most Inspectors, in two separate ways:

1. **Wrong response envelope.** The `bridge.ts` copies in ApplicationFlow Inspector, ApplicationFlow Editor v2 and Economy Inspector match `type === 'DiaEditor_onResponse'`. The shell sends `{__diaResponse, reqId, result}`, so every request silently times out after 5 s and resolves `{ok:false}`. (`@dia/editor-ui` `useBridge.ts` and the Pipeline Editor already match `__diaResponse` and work.)
2. **Null `reqId`.** Entity Inspector, AssetRuntime Inspector, Blackboard Inspector and `dia-live-connection.js` send `reqId: null`. `WebUIBridge` reads that as empty and takes the event path, but those handlers are registered as request handlers, so they never run (only "No event handler found" is logged). For example, the Entity Inspector's `schema_get` and `watch_add` are dead. A side effect is that `get_connection_state` priming never works, so an Inspector opened while already connected can show "disconnected" until the next state change. The iframe is also created after `OnLoad`, so the load-time push reaches no iframe.

**Rule for this system:** one envelope (`{__diaResponse, reqId, result}`) and a non-null `reqId` on every request. The fix (switch the three `bridge.ts` copies to `useBridge` or the shared envelope; give the null-`reqId` senders real ids) is a **bug-fix task outside this system**, tracked in BACKLOG. It must be done before Feature 5, because `view.ready` and view-state delivery rely on working request/response. Feature 5's ready handshake also **replays the current connection state on `view.ready`** in both hosts, which closes the missed-initial-push gap.

### Editor focus mode (fallback)

- `CluicheEditor --only <plugin> --view-request <path>` starts with one plugin fullscreen, with no dock, toolbar or splash. It loads the request's game, connects using the discovery token, and delivers the view state through the same `OnApplyViewState` path.
- The typed request replaces adding more flags to the hand-rolled `ParseCommandLine` (UI/business separation): `--view-request` is parsed into `ViewRequest`, and `--project=` keeps working.
- Built only if the spike fails, or when a plugin needs editor context the headless host does not provide.

### Logging and observation

- `VIEW` log channel. Logs cover: record written and stopped; token rejected (with remote origin, never the token itself); query sent and its outcome; host started, plugin loaded, state applied, UI ready, exit reason.
- Metrics: `dia.debugserver.auth_rejected` counter, `viewlaunch.host.ui_ready_ms` gauge.
- The host registers an `IHealthReporter` (degraded when the game connection drops).

## Features

| # | Feature | Notes | Status | Spec |
|---|---|---|---|---|
| 1 | Game discovery record | `discovery.json` in the observation session folder; liveness by pid and process start; session id and out root plumbed into `DebugServerHostModule`; `"port": "auto"`; Python reader + `dia query games` | Approved | [discovery-record.md](discovery-record.md) |
| 2 | Connection token | Per-launch token in the record; URL query checked in the WebSocket validate handler; `auth` config; editor, E2E, CLI clients adopt it in the same feature; AD-001 sign-off (DiaWebSocket, DiaDebugServer, DiaEditor) | Approved | [connection-token.md](connection-token.md) |
| 3 | Shared client + `dia query` | `DiaClient` moves into `dia_cli/utils`, E2E imports it; `dia query`, `--list`; JSON output; exit codes; optional echoed `request_id` (VL-011; AD-001 sign-off for DiaDebugProtocol and DiaDebugServer) | Approved | [debug-client-and-query.md](debug-client-and-query.md) |
| 4 | View request + `dia view` | `dia.view_request` v1; Python builder and validator; C++ `ViewRequest`/loader in DiaEditor; `EditorPluginMetadata` `viewHost` + `tier`, `--list-plugins`; `dia launch` arg passthrough; `dia view list`; `--launch` (VL-009, exit 7) | Draft (not written) | — |
| 5 | View-state contract | `GetViewStateParams()` (`ValueParam`s) + `OnApplyViewState`; deliver on `view.ready`, re-apply on reload, replay connection state on ready; `--describe`; Entity and ApplicationFlow Inspector adopt. **Needs the editor bridge fix first** | Draft (not written) | — |
| 6 | Headless plugin host | `Dia/DiaPluginHost` exe; WebSocket `IJSBridge`; minimal `EditorViewController` + `EditorModel`; host-owned `GameConnectionManager`; shell relay extracted from `EditorBridge.ts` and shared; `dia://` layout + theme injection served on 127.0.0.1; Edge `--app` window with an isolated profile and socket-based lifetime (VL-010); mockup gate; **Task 1 spike gates the feature** | Draft (not written) | — |
| 7 | Editor focus mode | `CluicheEditor --only <plugin> --view-request <path>`; fallback if Feature 6's spike fails | Draft (not written, conditional) | — |

**Build order:** 1 → 2 → 3 (no window: AI and scripts can read game data at this point), then 4 → 5 → 6 (spike first; on failure, 7 instead of the rest of 6).

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| DiaDebugServer | Command routing, catalogue (`list_commands`), host module writes the record | Done |
| DiaWebSocket | Validate handler (token check); possibly an HTTP handler for the host | Done |
| DiaObservation | Session id and session folder | Done |
| DiaEditor | `IEditorPlugin`, `WebUIBridge`/`IJSBridge`, `PluginServiceLocator`, `GameConnectionManager`, `LiveConnectionPluginBase` | Done |
| DiaValueSchema | `ValueParam` for view-state parameters | Done |
| DiaCLI | `dia query`, `dia view`, `dia launch` passthrough | Done (extended here) |
| Game-Scoped Editor Plugins | Plugin registry scope; the host loads a game-scoped plugin for its game | Done |
| Editor bridge response fix | One response envelope and non-null `reqId` across plugin UIs (see "Editor bridge prerequisite") | Not started: BACKLOG Loose End; needed before Feature 5 |

## Decisions

| ID | Decision | Status | Binding |
|---|---|---|---|
| VL-001 | Scriptable first: every view and query is reachable from `dia` with JSON output and non-zero exit codes; which host runs a view is hidden from callers | Accepted | Yes |
| VL-002 | One `ViewRequest` document is the only input to a host; CLI, links, presets and buttons are front ends that produce it | Accepted | Yes |
| VL-003 | The discovery record lives in the DiaObservation session folder (user decision 2026-11-10) | Accepted | Yes |
| VL-004 | The DebugServer requires a per-launch token, published only in the discovery record and checked in the WebSocket validate handler; built as part of this system (user decision 2026-11-10) | Accepted | Yes |
| VL-005 | Hosts load plugins unchanged; no plugin is ported to run headless | Accepted | Yes |
| VL-006 | The headless host is a new Dia application project, `DiaPluginHost`; DiaEditor stays a pure library (user decision 2026-11-10) | Accepted | Yes |
| VL-007 | View-state parameters are described with DiaValueSchema `ValueParam`s, not a new schema format | Accepted | Yes |
| VL-008 | v1 covers Inspector-tier plugins only | Accepted | Yes |
| VL-009 | `dia view` attaches by default; `--launch` starts the game and waits for its discovery record (60 s); launch failures exit 7 (user decision 2026-11-10) | Accepted | Yes |
| VL-010 | The headless host opens Edge `--app` with an isolated profile and judges its lifetime by the shell page's WebSocket; a browser tab is the fallback (user decision 2026-11-10) | Accepted | Yes |
| VL-011 | `CommandRequest`/`CommandResponse` gain an optional, echoed `request_id` in Feature 3; old clients are unaffected (user decision 2026-11-10) | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | How it constrains DiaViewLaunch |
|---|---|---|
| PD-001 | StringCRC for IDs | Plugin type ids and view-state keys resolve to `StringCRC` in C++; the request carries strings for display |
| PD-002 / AD-004 | ProcessingUnit/Phase/Module | `DiaPluginHost` is a DiaApplicationFlow app: request intake, plugin hosting and the UI socket are modules; socket-thread input reaches them through streams. The token check needs no cross-PU state (immutable before `listen`) |
| PD-004 / AD-002 | No STL in public APIs | `ViewRequest`, `GetViewStateParams`, `OnApplyViewState` use Dia containers and the Dia Json wrapper |
| PD-009 | Output under `Cluiche/out/<AppName>/` | Records in `out/<App>/sessions/`; request files in `out/DiaViewLaunch/requests/` |
| PD-010 | `.diagame` is the project root | `ViewRequest.game` names a `.diagame`; never a `.diaapp` path |
| AD-001 | Module docs; sign-off for engine API changes | New module docs (DiaPluginHost, DiaEditor `ViewLaunch/`); sign-off before building Features 1, 2, 5, 6 |
| SED-001 | `IEditorPlugin` minimal and stable | The two new virtuals are default no-ops; the new `EditorPluginMetadata` fields have defaults, so existing registrations compile unchanged |
| SED-010 | DiaDebugProtocol for editor-game wire types | No new wire message (the token rides the URL); the optional `request_id` field (VL-011) is added in DiaDebugProtocol |
| SED-015 | DiaEditor is a pure library | Bridge, `ViewRequest` and loader live in DiaEditor; modules live in `DiaPluginHost` and CluicheEditor |
| SED-016 | `Connect()` is explicit | Hosts call `Connect()` from the request; nothing auto-connects at boot |
| SED-022 / SED-023 | PluginServiceLocator; live plugins derive `LiveConnectionPluginBase` | The host registers `GameConnectionManager` in the locator; plugins keep their base class |
| DDS-004 | DebugServer is Debug-only | `dia query` and `dia view` work against Debug builds; a Release target reports "no DebugServer" (exit 2) |

## Test Strategy

- **GoogleTests:**
  - Discovery record write and stop.
  - Liveness (dead pid, pid reused with a different start time).
  - `port: auto` reports the bound port.
  - The token is accepted; missing or wrong tokens are refused with 403 (raw handshake, like `SLOW_WebSocketOrigin`); `auth: none`.
  - `ViewRequest` load and validation errors.
  - View state is held until `view.ready`, then re-applied on reload.
  - WebSocket `IJSBridge` round trip.
  - The host loads a stub plugin with its minimal model and controller; `GameConnectionManager` is in the locator before `OnLoad`.
  - Static serving reproduces the `dia://` layout, theme injection and the `..` traversal guard.
  - On `view.ready` the host replays connection state, then applies the view state.
- **pytest (DiaCLI):**
  - Discovery reader against fixture folders: live, dead, stopped, two matches.
  - `dia query` exit codes against a fake server.
  - Concurrent queries matched by `request_id`; a reply with no `request_id` still matches by name.
  - `dia view --launch`: waits for a new record, ignores an older live one, exits 7 on launch failure or timeout.
  - Window choice: Edge found → `--app` with the isolated profile; Edge missing → browser tab and `"window": "browser"`.
  - `ViewRequest` builder and validator.
- **E2E:**
  - `dia query` against a running CluicheTest returns the stage report.
  - Two games run at once with `port: auto` and both resolve by name.
  - The E2E harness passes with the token on.
- **Manual proof:**
  1. `dia run cow`.
  2. `dia view entity --game cow --state '{"entity": "<id>"}'` opens one window on that entity in under 3 s with no CluicheEditor running.
  3. Closing the window stops the host within about 5 s.
  4. `dia view entity --game cow --launch` with no game running starts CoW and opens the view.
  5. The same request with `host: editor` opens focus mode.

## Resolved Decisions

1. **`dia view` starts the game only with `--launch`** (user decision 2026-11-10, VL-009). Without the flag it attaches only and exits 2 when no game is live. With it, `dia view` runs `dia launch <target>`, waits up to 60 s for a new live discovery record for that game, then connects. Launch failures exit 7 and carry the launch's own error.
2. **The headless host's window is Edge `--app=<url>` with its own profile** (user decision 2026-11-10, VL-010). `--user-data-dir=Cluiche/out/DiaViewLaunch/edge-profile` keeps Edge from handing the window to a running Edge process (and from sharing the user's cookies and extensions). The host decides when to exit from the shell page's WebSocket: when the page disconnects and stays gone for about 5 s, the host shuts down. If `msedge` is not found, the default browser opens a tab and the JSON result says `"window": "browser"`. pywebview is ruled out by the Python toolchain constraints.
3. **Request ids land in Feature 3, optional and echoed** (user decision 2026-11-10, VL-011). `CommandRequest` gains an optional `request_id` that the server copies into `CommandResponse` (DiaDebugProtocol, SED-010). Clients that send none get replies as today, so the editor and existing tests do not change. The shared Python client always sends one and matches replies by it. Needs its own AD-001 sign-off (DiaDebugProtocol, DiaDebugServer) when Feature 3 starts.

## Open Design Questions

_None open at system level._ Feature-level questions go in each feature spec.

## Status

`Approved` (2026-11-10, user sign-off). Features 1–3 specs Approved 2026-11-10; Features 4–7 not written. Next: `/implement` Feature 1. AD-001 sign-offs are asked when each feature is built.

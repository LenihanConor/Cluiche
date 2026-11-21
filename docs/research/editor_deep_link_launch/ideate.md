# Research: Ideate — Editor Deep-Link Launch (revised)

**Input:** docs/research/editor_deep_link_launch/explore.md
**Revision note:** Reframed after discussion. The goal is *open as little as possible to see one specific thing* (a focused view, already connected), not steer the full CluicheEditor. Candidate 4 (headless plugin host) is the lead host option; its feasibility was checked against the code (see "Feasibility evidence").

## Feasibility evidence (code-checked 2026-11-09)

- `Dia::Editor::WebUIBridge` (DiaEditor/UI) is not CEF-bound: it takes a `Dia::Core::IJSBridge*`, a two-method interface in DiaCore (`RegisterJSHandler(name, handler)`, `CallJSFunction(fn, argsJson)`). Its only other coupling is an optional `EditorViewController*` used to forward UI events.
- Plugin TS UIs talk to the host through `bridgeRequest/bridgeEvent` in a per-plugin `bridge.ts`, using `window.parent.postMessage`. They never use WebSocket themselves.
- DebugServer wire format is JSON text frames (`SendText`); `GameConnectionManager` is a plain DiaEditor class over DiaWebSocket, usable without CEF.
- Searches of the seven Inspector plugins (ApplicationFlow, Entity, AssetRuntime, Blackboard, AI, Economy, ScalarField) and `LiveConnectionPluginBase` found no use of `EditorPluginContext::mView / mModel / mPluginLoader`.
- `EditorPluginBase::OnLoad` null-guards every context pointer (`mModel` is only used to subscribe to project changes, `mBridge` is guarded at each use). The one hard dependency is `LiveConnectionPluginBase::OnPluginLoad`, which fetches `GameConnectionManager` from `PluginServiceLocator`; the host must register a `GameConnectionManager` there. With a null model, plugins get no `OnProjectChanged` callbacks unless the host provides one.
- The shell relay (`CluicheEditor/UI/src/bridge/EditorBridge.ts`, ~100 lines) is simple: iframe `postMessage({__diaFromFrame, payload:{type, reqId, data}})` -> `dia.callCpp("DiaEditor_call", json)`; C++ responses -> `postMessage({__diaResponse, reqId, result})` to the originating iframe; pushed topics -> `postMessage({__dia, topic, data})` to all iframes. `editor.notify` is handled locally in the shell. **Open discrepancy:** `DiaApplicationFlowInspector/UI/src/bridge.ts` and `DiaApplicationFlowEditor/UI/src/v2/bridge.ts` match responses on `payload.type === 'DiaEditor_onResponse'`, which the shell's `__diaResponse` envelope does not contain, and no other Dia UI source consumes `__diaResponse`. Either another path delivers responses or request/response from those iframes times out today; must be checked against a running editor before the web shell copies the protocol.
- In Candidate 4 the browser never speaks the DebugServer protocol (proto-JSON `DebugMessage`: `data_update`, `command_request/response`); the host's unchanged `GameConnectionManager` does. The browser only talks to the host. A direct browser-to-DebugServer link is needed only for the dropped "web-only" variant.
- `DiaWebSocket::Server` (websocketpp) sets no validate/origin handler and calls `listen(port)`. As far as I know that overload binds all interfaces and accepts any Origin, so any web page can already open a socket to a running DebugServer. The headless host must bind 127.0.0.1 and check Origin plus a per-launch token.
- Thin plugins (Blackboard 43 lines, ScalarField 84, AI 129, Economy 129, ApplicationFlow 434) and thick ones (Entity 778, AssetRuntime 1502) all keep their C++ half unchanged under Candidate 4, because the C++ runs in the host process.

## Candidates

### Candidate 1: Typed View Request + `dia view` CLI (foundation)
**Home module/system:** DiaCLI (`dia view <plugin>`, `dia view list`) + a shared request schema (`ViewRequest`: plugin id, game `.diagame`, connect target, view-state JSON)
**Size:** S
**Description:** One typed request document and one command that builds it, validates it, resolves the game, picks the host (from the plugin's manifest) and launches. Returns structured JSON and a non-zero exit on failure, so scripts and AI can branch on it. The same request is what URIs, buttons and presets later produce. Host choice is an internal detail: `dia view entity` and `dia view perf` look identical to the caller.
**Primary value:** One scriptable entry point; every later front end and host plugs in behind it.

### Candidate 2: Game Discovery Record
**Home module/system:** DiaDebugServer (writes) + DiaCLI / host (reads)
**Size:** S
**Description:** On start, `DebugServer` writes a record under `Cluiche/out/<App>/sessions/` (pid, port, game name, `.diagame`, session id, start time) and removes it on clean stop; stale records are detected by pid liveness. `--game cow` or `--connect auto` resolves the running game with no port. `dia view games` lists them as JSON.
**Primary value:** "Right game, connected" without knowing a port, robust against orphan processes.

### Candidate 3: View-State Contract (`describe` + validate)
**Home module/system:** DiaEditor `Plugin/` (schema registration) + each plugin's metadata
**Size:** S-M
**Description:** Plugins declare the view-state params they accept (name, type, range, default), e.g. Perf Inspector: `unit`, `window_s`, `frame`, `module`, `paused`. `dia view describe perf` returns the schema; requests are validated against it; the state is delivered to the plugin through a new `OnApplyViewState(json)` hook (the existing `OnNavigate(StringCRC)` cannot carry it) once the plugin UI signals ready, queued across UI reloads.
**Primary value:** A link can land on a specific frame/unit/module, and scripts and AI can discover what a view accepts.

### Candidate 4: Headless Plugin Host (`DiaPluginHost`) with a Web Front End — LEAD
**Home module/system:** New Dia-level Application project (`DiaPluginHost`) reusing DiaEditor (plugin loading, `WebUIBridge`, `GameConnectionManager`); a small web shell page; `dia view` opens it in WebView2/pywebview or a browser tab
**Size:** M-L
**Description:** A small native process that loads one `IEditorPlugin` unchanged (C++ half included), supplies an `EditorPluginContext` whose `WebUIBridge` is backed by a new WebSocket-based `IJSBridge`, connects to the game with the existing `GameConnectionManager`, and serves the plugin's `GetUIPath()` UI over loopback HTTP. A shell page plays the editor's "parent frame" role: it hosts the plugin UI in an iframe and relays `postMessage` to/from the host over WebSocket, so existing plugin `bridge.ts` files need no change. No CEF, no docking, no splash, no editor model. Works for thin and thick plugins alike because the C++ half runs in the host. Layer note: reusable by any game, so it belongs in Dia; placement to be confirmed with the user before spec (escalation rule).
**Primary value:** Open exactly one view, for any Inspector plugin, already connected, without the editor and without porting plugins.

### Candidate 5: Headless Data Queries via CLI
**Home module/system:** DiaCLI (`dia perf window`, `dia query <channel>`) over DiaDebugServer queries
**Size:** S
**Description:** For cases where nobody needs to look, return the data a view would show as JSON from the command line (e.g. `perf.window`, `app.timings`). Uses the DebugServer's JSON wire directly from Python; no host, no UI. AI reads numbers; a human opens the view only when needed.
**Primary value:** The cheapest possible "see the thing": no window at all.

### Candidate 6: Editor Focus Mode (fallback host)
**Home module/system:** CluicheEditor (`--only <plugin>`, chromeless layout) + `EditorModelModule`
**Size:** S
**Description:** `CluicheEditor.exe --only entity --game ...` loads one plugin fullscreen with no dock or toolbar. Cheapest way to cover any plugin that genuinely needs editor context (`EditorModel`, `EditorView`, services) that the headless host does not provide. Still pays editor and CEF startup.
**Primary value:** A guaranteed-compatible fallback for plugins the headless host cannot run.

### Candidate 7: Native CEF Viewer Exe
**Home module/system:** New Dia-level viewer project sharing bootstrap extracted from CluicheEditor ApplicationFlow
**Size:** M
**Description:** A small exe hosting one `IEditorPlugin` inside CEF, fullscreen. Compatible with every plugin like Candidate 6 but with faster startup. Requires extracting the editor's bootstrap into a shared library to avoid two diverging hosts.
**Primary value:** Fast single-view window for any plugin, without a browser stack. Largely superseded by Candidate 4 if that works.

### Candidate 8: `editor.*` DiaAPI Commands on a Running Editor
**Home module/system:** DiaEditor / DiaAPI (`editor.open_plugin`, `editor.connect`, `editor.navigate`) with a loopback control endpoint
**Size:** M
**Description:** Lets scripts and AI drive an already-open editor with structured request/reply. Valuable when the user is already working in the full editor; unnecessary for the "open one small view" goal, since each view is its own small process.
**Primary value:** Scriptable control of the full editor, following the "every editor action is a DiaAPI command" rule.

### Candidate 9: In-Game "Inspect in Editor" Trigger
**Home module/system:** CluicheGameBaseline `DebugPanelPageModule` + DiaDebugServer
**Size:** S-M
**Description:** A debug-panel button (or a perf trip) raises a view request carrying the current frame, unit and offending module and asks `dia` to open it. The launch uses the same command as everything else.
**Primary value:** One click from seeing a spike in the game to the exact view of it.

### Candidate 10: `cluiche://` Links and Saved Presets
**Home module/system:** DiaCLI (URI handler registration, preset files under `Cluiche/Assets/<App>/editor-views/`)
**Size:** S
**Description:** A per-user (HKCU) URI scheme and named preset files that are both just serialised `ViewRequest` documents. Links work from logs, chat and test reports; presets give repeatable investigation starting points. A strict allow-list of verbs and plugins keeps links from executing anything.
**Primary value:** Shareable, repeatable views.

## Coverage Map

- **Stack vs decisions:** 1, 2 and 3 are the foundation every option shares. 9 and 10 are front ends on top. The real decision is the host: 4 (headless web), 6 (editor focus), 7 (native CEF), with 8 an optional extra for the full editor.
- **Headless-first:** 5 covers the "no window at all" case independent of the host.
- **Plugin coverage:** 4, 6 and 7 cover all plugins; 4 is the lightest if the unchecked pieces (`EditorPluginBase`, shell page relay) hold; 6 is the guaranteed fallback.
- **Callers:** shell (1, 5), AI (1, 3, 5), in-game button (9), external link (10).
- **Scope range:** S foundations and front ends, M-L for the host. No XL.

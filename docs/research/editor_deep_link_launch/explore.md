# Research: Explore — Editor Deep-Link Launch

**Session date:** 2026-11-09
**Folder:** docs/research/editor_deep_link_launch/

## Problem Space Overview

Today a developer who sees a performance problem in a running game has to: open CluicheEditor, load the right `.diagame`, open the Game Connection panel, connect to the right DebugServer, open the right plugin (e.g. the new Perf Inspector), and pick the right view state (window, unit, frame). Each step is manual, and none of it is addressable from outside the editor. The goal is a single *deep link*: one command, button press or AI tool call that names the editor/plugin, the target game, the connection and the view state, and lands the user (or agent) in that exact state.

This is the "open in..." / "go to" problem every mature tool has solved: VS Code's `code --goto file:line` and `vscode://` URIs, JetBrains Toolbox/`idea://` links, Unity's `-projectPath -executeMethod`, Perfetto's `ui.perfetto.dev/#!/?url=...`, Chrome DevTools' `--remote-debugging-port` attach. The shared shape is: (1) a stable *address* format, (2) a *router* that resolves the address to an app instance (start new or reuse running), (3) a *handler* inside the app that applies the state, and (4) a *state contract* each view agrees to accept.

For Cluiche this matters beyond convenience: the perf-inspector spec exists because a diagnosis took 20 minutes of hand-made A/B rebuilds. If an agent (or an in-game "Inspect this" button) can open the Perf Inspector already attached, on the offending unit and frame, the diagnose loop closes. It also fits the standing rule that every editor action is a DiaAPI command.

## Existing Approaches

- **Command-line flags on the app** (`code --goto`, Unity `-projectPath`): simplest; only works at cold start unless paired with single-instance forwarding.
- **Single-instance forwarding**: second launch detects the first (mutex / named pipe / `WM_COPYDATA`) and forwards its argv, then exits. VS Code, Chrome, Slack do this.
- **URI protocol handler** (`vscode://`, `idea://`, `steam://`): OS registry maps scheme to exe; enables links from browsers, chat, markdown, logs. Needs registry write.
- **Control channel on a running instance** (Chrome DevTools Protocol, Unity Editor `-batchmode` + IPC, VS Code CLI over IPC socket): a local socket the CLI uses to drive the editor; no OS registration.
- **Session/state files** (JetBrains `.idea`, Perfetto trace URL params): the link points at a file describing the state; the app loads it.
- **Agent-first command surface** (MCP / tool APIs): the same operations exposed as typed commands so an AI calls them, not a shell string.
- **Reverse direction: the game advertises itself** (discovery file, mDNS, well-known port) so the editor can find "the" game without being told a port.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Address format | CLI flags only / URI (`cluiche://`) / JSON "launch request" file / all three as one model | Flags and URI can be two spellings of one typed request |
| Entry transport | argv at cold start / single-instance forward / local control socket / OS URI handler | Cold start exists already (`--project=`); nothing else does |
| Instance policy | always new / reuse running / reuse if same game else new | Matters when the editor already has a different game loaded |
| Who resolves the target | `dia` CLI (python) / editor / game | CLI knows repo and builds; editor knows plugins; game knows its port |
| Game discovery | explicit port arg / well-known port / discovery file under `out/` / DiaCLI-started game registers itself | Decides whether "right game" is automatic |
| State contract | plugin-specific JSON blob / typed per-plugin params schema / generic "navigate to" verb | How the Perf Inspector receives unit, window, frame |
| Caller surface | shell / in-game button / AI tool / external browser link | Should all share one entry point |
| Failure model | fail loudly / open editor anyway with a banner / wait-and-retry connect | Game may still be starting when link fires |

## Known Tradeoffs

- Flags are trivial but cold-start only; fast iteration needs reuse of a running editor, which needs IPC.
- URI handlers give clickable links but require registry registration (per-user is fine, no admin) and a security stance: a link must not be able to run arbitrary commands.
- Typed per-plugin param schemas are safer and AI-friendly but add a registration burden per plugin; a generic blob is cheap but un-validatable.
- Putting resolution in `dia` (Python) is easy to build but editor-internal state (which plugins loaded) is invisible to it; putting it in the editor is richer but the editor must be running.
- Reusing an existing instance preserves layout and connections (Editor Memory), but state carried over can surprise.

## Known Pitfalls (C++ / game engine context)

- `ParseCommandLine` in `EditorModelModule.cpp` is a hand-rolled `GetCommandLineW` parser that knows only `--project=` and a bare path; adding flags there will not scale and mixes concerns (UI/business separation rule).
- CEF iframe state is lost on panel unload (SED-018); a deep link applied before the plugin UI is ready will be dropped. Needs a "pending request until panel ready" handoff.
- The editor is a game-scoped-plugin host: plugins are tagged out of scope until a game loads. Link order must be: load game, then activate plugin, then connect, then apply view state.
- Cross-PU rule: any request delivered to the editor from a socket thread must enter the editor via a stream, not a mutex-guarded shared structure.
- Windows foreground rules: a forwarded launch cannot always steal focus; `SetForegroundWindow` can fail silently.
- Stale ports / orphan `CluicheTest.exe` processes (known gotcha) can make "connect to the right game" connect to the wrong or dead one.
- Don't add a new singleton for the link router (no-singletons rule); use a module reachable via ModuleRef/streams.
- The editor must not bypass the DiaCLI rule: exes are launched through `dia`, so the shell entry point should be a `dia` command that composes game launch plus editor launch.

## Cluiche-Specific Opportunities

### Relevant Existing Modules
| Module | Relevance |
|--------|-----------|
| CluicheEditor `EditorModelModule` | Already parses `--project=<.diagame>` and a bare `.cluicheproj`; owns `LoadDiagameProject`; restores `last_project` and window state. Natural cold-start hook, but the parser should move to a typed request. |
| DiaEditor `Plugin/` (`IEditorPlugin`, `EditorPluginRegistry`, `GameConnectionEditorPlugin`) | Plugin activation and game-scope manifests; `GameConnectionManager/Controller` already connect to a DebugServer. Needs a programmatic "connect to host:port" and "activate plugin X" entry. |
| DiaDebugServer | `DebugServer::SetPort`, `SetGameInfo`, `SetDiagamePath` — the game already knows its port, name and `.diagame`. A discovery record can be published from here. Also hosts the Observation Bridge the Perf Inspector reads. |
| DiaCLI (`dia run/launch`, `launch.py`) | Owns process launch, path and working-dir resolution, `--dev-alias`. The right home for `dia editor open ...` that starts game + editor together. Click commands are reflectable into the Dia Console. |
| DiaAPI | Commands are the mandated action surface; the link router should be a DiaAPI command (`editor.open`), callable by CLI, AI and panel buttons alike. |
| DiaChatPlugin / editor script API | The AI caller; needs a typed tool that maps to the same command. |
| Perf Inspector / ApplicationFlow Inspector (specs) | First two consumers; define what view state (unit, window, frame, selected module) a link must carry. |
| Editor Memory (research `editor_workspac`) | Restores layout, plugins, connection targets on open; a deep link overrides it rather than replacing it. |
| In-game debug panel (`DebugPanelPageModule`) | Source of the "button press": panel knows game, port, current frame. |
| Dia Console (research `cli_launcher`) | Presets and typed commands; a deep link is a natural saved preset. |

### Platform Decision Constraints
| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Plugin ids, view ids and state keys in a request should be StringCRC-able names, not ad-hoc strings compared at runtime. |
| PD-002 ProcessingUnit/Phase/Module | The router is an EditorPU Module; request intake runs through a stream into the module, not a free-standing thread. |
| PD-004 No STL in public APIs | Any public request type in DiaEditor uses Dia containers/fixed buffers; the JSON blob lives behind the Dia Json wrapper. The Python side is unconstrained. |
| PD-005 / PD-007 | x64 Windows, C++20; named pipe / registry usage is Win32-only, acceptable. |
| PD-006 / PD-008 | New files need vcxproj entries (`dia docs vcxproj-add`); no output path overrides. |
| PD-010 `.diagame` is the project root | Targets are resolved from `.diagame`, never from `.diaapp` paths. The request's "game" field is a `.diagame` (or its name). |
| Layer rule (CLAUDE.md) | Generic request/router/handler is Dia (DiaEditor); the perf plugin's accepted params live with the plugin; wiring in CluicheEditor. Flag before putting new headers in Dia from a game project. |
| Tool tiers (memory) | Editor = offline files, Inspector = live game via DiaDebugServer; a link that "attaches to a live game" targets Inspector-tier plugins only. |
| UI/business separation (memory) | Build the typed launch-request model and executor first; CLI, URI and buttons are thin front ends. |
| Editor actions via DiaAPI (memory) | `editor.open` / `editor.navigate` are DiaAPI commands. |

## Open Questions for Ideation

- Is the minimum useful slice cold-start only (`dia editor open perf --game cow --attach`), or must it also steer an already-open editor?
- Who starts the game: does the link ever launch the game, or only attach to one already running (the perf-issue case is "already running")?
- How does the editor learn which running game is "the right one": discovery file written by DebugServer, or an explicit port?
- What is the view-state contract: per-plugin typed params registered by the plugin, or a generic navigation verb plus opaque JSON?
- Should a link be shareable (a URI in a chat or log line), or is shell/AI/button enough?
- How much of the state (frame number, selected module, window length) can survive until the panel's CEF UI is ready?
- Does an in-game button trigger the editor directly (game to OS process spawn), or through the DebugServer asking a connected editor, or through `dia`?
- What does an AI call: the shell command, a DiaAPI command via the DebugServer, or an MCP-style tool?

# Feature Spec: DiaConsole — Console Native Shell

## Parent System
@docs/specs/applications/dia/systems/diaconsole/diaconsole.md

**Status:** `Approved`

---

## Summary

Build the native-window UI shell that makes `console-command-model` usable by a human: a local FastAPI app serving a generic command browser + run form + live log, hosted inside a chromeless `pywebview`/WebView2 window via a new `dia console` command. The nav tree and forms are generated entirely from `CommandRegistry`/`CommandDescriptor` — no per-command hand-authored UI. Only the Log tab is functionally real in this feature; the Results tab, project/target breadcrumb, and Presets panel appear as static placeholders per the mockup's layout, since their actual logic belongs to `console-typed-results`, `console-project-context`, and `console-presets` respectively.

---

## Goals

1. `dia console` (new Click command in `dia_cli/cli/console.py`) launches a chromeless native window rendering DiaConsole's UI, backed by a local FastAPI app bound to `127.0.0.1` on an ephemeral port. Uvicorn runs in a daemon thread; the actual bound port is discovered via a `threading.Event` set from a FastAPI lifespan startup hook (not polled/guessed), then the webview window is created pointed at it.
2. Nav tree (`GET /api/commands`) renders every command from `CommandRegistry.from_click_app(cli_main.cli)`, grouped under friendly category labels per the mapping below, matching the mockup's collapsible-group layout (`mockups/console.html`).
3. Selecting a command renders a generically-generated form from its `ArgumentDescriptor`/`OptionDescriptor`s: `STRING`→text input, `INT`/`FLOAT`→number input, `BOOL` (`is_flag`)→checkbox, `CHOICE`→select populated from `choices`, `PATH`→text input. Required fields are marked. A live CLI-preview line re-renders as form values change, built from the same argv-serialization rules as `ExecutionService.build_argv` (mirrored in JS, not called over HTTP — see Open Design Questions).
4. Run button `POST`s an `ExecuteCommandRequest` to `/api/execute`, receives `{"execution_id": "..."}`, and the Log tab live-streams raw text via SSE from `GET /api/executions/{id}/logs`.
5. Cancel button `POST`s to `/api/executions/{id}/cancel`, wired directly to `ExecutionHandle.cancel()`. Because the window is frameless (no native title bar), the three glyph controls in the mockup's titlebar (minimize/maximize/close) are wired through pywebview's JS bridge (`window.pywebview.window.minimize()`/`toggleFullscreen()`/`close()`) — otherwise a frameless window would have no way to be moved/closed at all. No-ops harmlessly if opened in a plain browser tab during development.
6. `GET /api/executions/{id}/events` exposes the typed `ExecutionEvent` stream via SSE, independently of `/logs` — the shell's status line (step name, running/idle dot) consumes this; it is a separate connection from the Log tab's `/logs` stream, preserving `console-command-model`'s explicit non-merging of the two.
7. Results tab renders a static "No structured results yet" empty state; the breadcrumb renders a hardcoded, non-interactive `cluichetest / Debug / x64` label; the Presets panel is omitted entirely from the nav (not even non-functional chips) — none of these have real logic in this feature.
8. On window close, any in-flight `ExecutionHandle`s are cancelled before uvicorn shuts down (`server.should_exit = True`, thread joined with a timeout) — closing the window must never leave an orphaned `dia` subprocess tree running.
9. `dia console` is additive (SD-CONSOLE-008): every other `dia <command>` is completely unaffected by this feature's existence, and omitting `dia console` from a build leaves the rest of DiaCLI working.

---

## Data Contracts

**HTTP API (FastAPI, `dia_console/web/app.py`):**

```
GET  /                              → shell HTML (dia_console/web/static/index.html)
GET  /static/*                      → mounted static assets (JS/CSS)
GET  /api/commands                  → JSON array of CommandDescriptor (dataclasses.asdict, tuples → arrays)
POST /api/execute                   → body: ExecuteCommandRequest JSON → {"execution_id": str}
GET  /api/executions/{id}/events    → SSE stream of ExecutionEvent JSON (one `data:` frame per event)
GET  /api/executions/{id}/logs      → SSE stream of raw text lines (one `data:` frame per line)
POST /api/executions/{id}/cancel    → calls ExecutionHandle.cancel(); 204 No Content
```

**In-flight execution tracking.** `POST /api/execute` returns immediately after calling `ExecutionService.execute()`; the returned `ExecutionHandle` must be reachable by later `GET .../events`, `GET .../logs`, and `POST .../cancel` calls for the same `execution_id`. Kept as an in-memory `dict[str, ExecutionHandle]` on FastAPI app state — no persistence, no cross-process sharing (single-user, single-process tool per SD-CONSOLE's Out of Scope).

**Nav grouping — category → friendly label (resolved, not open):**

| Top-level command category | Group label |
|---|---|
| `run`, `launch`, `pipeline`, `fix`, `diagnose` | Run & Debug |
| `test`, `check` | Test & Quality |
| `asset`, `reflect` | Assets & Data |
| `scaffold`, `docs`, `codegen` | Create & Docs |
| `env` | Environment |
| anything else (`api`, `show`, `capture`, `agent`, `command`, `cli_validate`, `cli_check`, ...) | Advanced |

A category not in this table falls into **Advanced** rather than raising or being dropped — matches the closed-enum-plus-fallback pattern used throughout this system (see `console-command-model`'s `ArgumentType`).

**Corrected during implementation — this table lives server-side, not in JS.** `GET /api/commands` computes each command's `groupLabel` in Python (`dia_console/web/nav_grouping.py`, keyed on `CommandDescriptor.path[0]`) and ships it as an extra field alongside the raw descriptor JSON (not a `CommandDescriptor` dataclass field — `model.py` is untouched). `app.js` only ever buckets by this already-resolved label. This is a strictly better version of the original plan (a JS-side copy of the same table): it removes a drift risk this spec would otherwise have accepted with no corresponding upside, unlike Open Design Question 1's CLI-preview mirror, where a client-side copy is unavoidable because the preview must update live without a server round-trip per keystroke.

**New dependencies (none currently in `pyproject.toml`):** `fastapi`, `uvicorn`, `pywebview`.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | `dia_console/web/app.py` — FastAPI app: `GET /`, `GET /static/*` (via `StaticFiles`), `GET /api/commands` | `/api/commands` serializes the process-lifetime `CommandRegistry` (built once at server startup, not per-request — no re-reflection per SD-CONSOLE-010, but that's a *registry* rule, not a *server* one; building it once at startup is a normal server-lifecycle choice, not a violation). |
| 2 | `POST /api/execute` + in-memory `dict[str, ExecutionHandle]` on app state | Deserialize `ExecuteCommandRequest` from the request body; call `ExecutionService.execute()`; store the handle; return `execution_id`. |
| 3 | `GET /api/executions/{id}/events` and `GET /api/executions/{id}/logs` — two independent SSE endpoints | Each wraps the corresponding `ExecutionHandle` async iterator in FastAPI's `StreamingResponse` with `media_type="text/event-stream"`. Never merge them into one endpoint. 404 if `execution_id` is unknown. |
| 4 | `POST /api/executions/{id}/cancel` | Looks up the handle, calls `await handle.cancel()`, returns 204. 404 if unknown. |
| 5 | `dia_console/shell.py` — `launch()`: build `uvicorn.Config(app, host="127.0.0.1", port=0, log_level="warning")`, wrap in a `uvicorn.Server` subclass that overrides `install_signal_handlers` to a no-op, run `server.run()` in a daemon thread | **Corrected during implementation:** the port-readiness signal comes from overriding the `uvicorn.Server` subclass's own `startup()` method (calling `super().startup()` then reading `self.servers[0].sockets[0].getsockname()[1]` and setting a `threading.Event`) — NOT a FastAPI/ASGI `lifespan` "startup" hook on `app` itself. Uvicorn awaits its ASGI lifespan startup *before* creating the listen socket, so `self.servers` doesn't exist yet at that point — an app-level lifespan hook reading it raises `AttributeError` (confirmed in RED). Still purely event-driven, never polled — just signalled from the correct place. `launch()` waits on the event with a timeout (5s — fail loudly, `RuntimeError`, if the server never starts) before proceeding. |
| 6 | `dia_console/shell.py` — after port is known, `webview.create_window("Dia Console", url=f"http://127.0.0.1:{port}", frameless=True, ...)`, then `webview.start()` | On the window-closed event, before returning: cancel every in-flight `ExecutionHandle` in the app-state dict, then set `server.should_exit = True` and join the uvicorn thread with a timeout (e.g. 5s). |
| 7 | New `dia_cli/cli/console.py` — thin Click command `cli()` (group-less, single command) calling `dia_console.shell.launch()` | Same file-per-command pattern as every other `dia_cli/cli/*.py` module — auto-discovered, no `cli_main.py` changes needed. |
| 8 | `dia_console/web/static/index.html` + `app.js` + `styles.css` | Start from `docs/specs/applications/dia/systems/diaconsole/mockups/console.html`'s markup/CSS directly (per the plan's Implementation Patterns) rather than redesigning; strip the Presets panel, replace the breadcrumb with the hardcoded label, replace the Results-tab card content with the empty-state message. |
| 9 | `app.js` — nav tree rendering from `/api/commands`, generic form rendering keyed by `ArgumentType`, CLI-preview line, Run/Cancel wiring, Log-tab SSE consumption | The CLI-preview line mirrors `ExecutionService.build_argv`'s serialization rules in JS (flags omit-when-false, `multiple` repeats the flag, always-explicit values) purely for *display* — the authoritative argv is still built server-side in `build_argv` when `/api/execute` is called; the JS preview is cosmetic and must never be treated as what actually executes. |
| 10 | Test: `dia console` opens a window and the FastAPI app responds correctly on its ephemeral port, using FastAPI's `TestClient` for the HTTP-layer tests (no real window needed) plus one smoke test that `shell.launch()`'s server-startup handshake actually completes within its timeout | HTTP-layer tests don't need `pywebview`/a real window — that part is `CANNOT VERIFY PROGRAMMATICALLY` per the verify skill and needs a manual check (see Acceptance Criteria). |
| 11 | Test: `/api/execute` → `/api/executions/{id}/logs` end-to-end against a real `dia asset build --target cluichetest` execution, confirms SSE frames arrive and the stream terminates | Mark `@pytest.mark.integration`, mirroring `console-command-model`'s test conventions. |
| 12 | Test: window-close cancels in-flight executions | Simulate the close handler directly (call the same cancel-all-then-shutdown function `shell.py` registers) rather than driving a real GUI close event; assert every tracked handle's `cancel()` was invoked. |

---

## Binding Decisions

| Decision | Implication |
|----------|--------------|
| SD-CONSOLE-003 — Native window via `pywebview`/WebView2, not a browser tab or TUI | `dia_console/shell.py` uses `webview.create_window(frameless=True, ...)`; no browser-launch fallback. |
| SD-CONSOLE-004 — Free-only front-end stack, Webix excluded | Plain HTML/CSS/vanilla JS under `dia_console/web/static/`; no Webix, no paid component library. |
| SD-CONSOLE-005 — UI depends only on the command/execution/result model's interfaces | `app.js` only ever calls `/api/*`; it never constructs subprocess calls, never talks to Click, never bypasses `ExecutionService`. The FastAPI layer is the only thing that touches `dia_console.model`/`registry`/`execution` directly. |
| SD-CONSOLE-002 — Execution events wrap DiaCLI's existing NDJSON stream, not a new schema | `/api/executions/{id}/events` re-serializes `ExecutionEvent` (already translated by `console-command-model`) as SSE JSON frames; no new event vocabulary is introduced at the HTTP layer. |
| SD-CONSOLE-008 — DiaConsole is additive; every `dia <command>` is unaffected | `dia console` is one more file under `dia_cli/cli/`, auto-discovered the same way as every other command; nothing in `dia_cli/cli_main.py` changes. |
| No network-exposed API (system spec, Public Interfaces) | `uvicorn.Config(host="127.0.0.1", ...)` — never `0.0.0.0`, never a fixed well-known port that could be firewalled open by mistake. |

---

## Open Design Questions

1. **CLI-preview argv mirroring in JS is display-only, not authoritative — risk of drift.** Task 9 mirrors `build_argv`'s serialization rules in JavaScript purely so the preview line updates live as the user edits the form, without a round-trip to the server on every keystroke. If DiaCLI's argv rules ever change, the JS mirror could silently drift from the real Python implementation, showing a preview that doesn't match what actually runs (the real run is still correct either way, since `/api/execute` always calls the real `build_argv` server-side — only the *preview text* could go stale). Acceptable for v1; flagged so a future maintainer doesn't assume the JS copy is load-bearing.
2. **Ephemeral-port startup timeout (5s) is a judgment call, not measured.** If uvicorn genuinely takes longer to bind under some environment (slow disk, AV scanning the new process), `dia console` would fail to launch with a timeout error rather than just being slow. No evidence either way yet — treat as a tunable constant, not a hard architectural commitment, and revisit if it ever actually fires in practice.

---

## Acceptance Criteria

- `dia console` opens a chromeless native window with no visible browser chrome, backed by a FastAPI server bound to `127.0.0.1` on an ephemeral port (never a fixed port, never `0.0.0.0`).
- Nav tree shows every command from the real `CommandRegistry`, grouped under the friendly labels table above, with any unmapped category falling into "Advanced" rather than being dropped or crashing.
- Selecting a command renders a form whose field types match the mockup's input styles for each `ArgumentType` (text/number/checkbox/select), with required fields visually marked.
- Running `dia asset build --target cluichetest` from the UI (or an equivalent instrumented command) streams live log lines into the Log tab via `/api/executions/{id}/logs` and the stream terminates when the execution completes.
- Clicking Cancel while a run is in progress calls `/api/executions/{id}/cancel` and the run stops (subprocess tree killed, matching `console-command-model`'s `cancel()` behavior).
- Closing the window while a run is in progress cancels it first — no orphaned `dia` subprocess survives the window closing.
- Results tab shows the static empty-state message; breadcrumb shows the hardcoded label; no Presets UI is present anywhere in the shell.
- `/api/events` and `/api/logs` remain two separate SSE connections — no endpoint merges them.
- No existing `dia <command>` behavior changes; running any other DiaCLI command with `dia console` never having been run behaves identically to today.

**Note:** the actual native-window rendering (does it look right, is it genuinely chromeless, does WebView2 render the CSS correctly) is `CANNOT VERIFY PROGRAMMATICALLY` — needs a manual run-and-look check per the verify skill. Everything else above is automatable via `TestClient` + the real `ExecutionService`.

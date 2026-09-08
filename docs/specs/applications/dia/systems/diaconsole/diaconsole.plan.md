**Spec:** @docs/specs/applications/dia/systems/diaconsole/diaconsole.md
**Status:** In Progress

## Implementation Patterns

**Package location.** New top-level package `Dia/DiaCLI/dia_console/` — a sibling to the existing `Dia/DiaCLI/dia_cli/`, inside the *same* Poetry project/pyproject.toml (not a second package). It depends on `dia_cli` directly; splitting it into its own Poetry project would duplicate dependency/venv management for no benefit. Import path is `dia_console.*`, matching the spec's Public Interfaces section (`dia_console.model`, `dia_console.registry`, `dia_console.execution`).

**console-command-model:**
- `dia_console/model.py` — frozen `@dataclass` types exactly as listed in the spec's Data Contracts: `CommandDescriptor`, `ArgumentDescriptor`, `OptionDescriptor`, `CommandCapabilities`, `ExecuteCommandRequest`, `ExecutionEvent`, `ResultRecord`.
- `dia_console/registry.py` — `CommandRegistry.from_click_app(cli_main.cli)`. Walks Click's public API (`cli.list_commands(ctx)` / `cli.get_command(ctx, name)`), recursing into `click.Group` subcommands (`test`, `check`, `docs`, `env`, `scaffold`, `asset`, `reflect`, `capture`, `api`, `agent`) to build dotted-path IDs (`test.googletest`, `check.arch`). Reads each `click.Command`'s `.name`, `.help`, `.params` (`click.Argument`/`click.Option` → type/default/required/is_flag/multiple) into a `CommandDescriptor`. No import of `dia_cli.cli_main`'s private `_find_modules()` — use Click's public `MultiCommand` interface only, since that's the stable contract.
- `dia_console/execution.py` — `ExecutionService`/`ExecutionHandle` per SD-CONSOLE-009: **subprocess only**. Builds `["dia"] + descriptor.path + positional_args + option_flags` and spawns via `subprocess.Popen`. **Concurrency-safety detail to get right:** DiaCLI's `OutputContext` defaults to a fixed path `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson` — concurrent executions of the same command would collide on that file. `ExecutionService` must always pass `--log-json Cluiche/out/DiaCLI/logs/console/<execution_id>.ndjson` (DiaCLIWithOutput already supports this override flag) so every execution gets its own file. `ExecutionHandle.events()` tails that NDJSON file (not stdout parsing) for structured events, and separately streams raw stdout/stderr lines for the log pane.

**console-native-shell:**
- `dia_console/web/app.py` — FastAPI app (async fits the event-queue model above): `GET /` (shell), `GET /api/commands` (registry as JSON), `POST /api/execute` (returns `execution_id`), `GET /api/executions/{id}/events` (Server-Sent Events stream).
- `dia_console/shell.py` — runs uvicorn in a background thread bound to `127.0.0.1:<ephemeral port>`, then `webview.create_window(..., frameless=True, url=f"http://127.0.0.1:{port}")` / `webview.start()`.
- New `dia_cli/cli/console.py` — thin Click command (`dia console`) that calls into `dia_console.shell.launch()`. Follows the exact same file pattern as every other `dia_cli/cli/*.py` module (auto-discovered by the existing `cli/` folder convention — no changes to `cli_main.py` needed).
- Front end: plain HTML/CSS + vanilla JS (or htmx) under `dia_console/web/static/`. Start from `docs/specs/applications/dia/systems/diaconsole/mockups/console.html`'s layout/CSS classes directly rather than redesigning from scratch; wire its static content to the `/api/*` routes.

**console-typed-results:**
- `dia_console/results.py` — adapter registry keyed by `command_id`, each adapter converting a completed execution's output into `ResultRecord`s (e.g. a googletest adapter parsing the `--gtest_output=xml:` file DiaCLI's own `googletest_runner.py` already produces into `TestResult` records). Always register a generic fallback adapter (exit code + stderr → `GenericResult`) so every command produces *something* structured, per SD-CONSOLE — no command is left with only raw log text.

**console-project-context:**
- `dia_console/context.py` — `CurrentContext`/`ProjectRegistry`. Targets come from `dia_cli.commands.pipeline.pipeline_config.load_pipeline_config()` (already parses `pipeline.toml`) — do not re-parse `pipeline.toml` independently. Sibling-project discovery reuses the same tree-walk approach `cli_main.py` already uses for `cli/` folder discovery (same algorithm, don't reimplement) rather than a new discovery mechanism.

**console-presets:**
- `dia_console/presets.py` — loads `.dia/console-presets.yaml` (repo-committed) and `.dia/console-presets.local.yaml` (add to `.gitignore`), merges by `id` with local overlay winning collisions. New dependency: `pyyaml` (not currently a DiaCLI dependency — `toml` is used for `pyproject.toml` only).
- New `dia_cli/cli/preset.py` — `dia preset list` / `dia preset run <id>`, same file-per-command pattern as every other CLI module.

**console-desktop-install:**
- PyInstaller build step (`dia_console/packaging/DiaConsole.spec` or equivalent) producing `dist/DiaConsole.exe` — windowed (`--windowed`/`--noconsole`), one-file, with a bundled `.ico` (new asset under `dia_console/web/static/` or a dedicated `packaging/` folder). Required for v1, not optional — Poetry's `console_scripts` launcher (`dia.exe console`) always uses the console subsystem on Windows and flashes a terminal on double-click, so the desktop entry point must be this separately packaged exe, not the Poetry script.
- New `dia_cli/cli/console.py` subcommand `install-shortcut` — uses `pywin32`'s `win32com.client.Dispatch("WScript.Shell")` to create a `.lnk` at `Path.home() / "Desktop" / "Dia Console.lnk"`, `TargetPath` pointing at the built `DiaConsole.exe`, `IconLocation` set from the bundled `.ico`, `WorkingDirectory` set to the exe's own folder. New dependency: `pywin32` (not currently a DiaCLI dependency).
- Idempotent: re-running `install-shortcut` overwrites the existing `.lnk` rather than erroring or duplicating.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `/spec-feature` — console-command-model | Feature spec `Approved` | Not Started | sonnet | Foundation; blocks every other task |
| 2 | Implement console-command-model | Registry reflects all current DiaCLI commands; `ExecutionService` runs `dia show config` end-to-end via subprocess with per-execution `--log-json` path, no file collision under 2 concurrent runs | Not Started | opus | Highest architectural risk — everything else depends on getting this right |
| 3 | `/spec-feature` — console-native-shell | Feature spec `Approved` | Not Started | sonnet | Depends on Task 2 |
| 4 | Implement console-native-shell | `dia console` opens a chromeless native window; nav tree renders all reflected commands; `run` form matches `mockups/console.html` layout; Run button executes and streams live log | Not Started | sonnet | |
| 5 | `/spec-feature` — console-typed-results | Feature spec `Approved` | Not Started | sonnet | Depends on Task 4 |
| 6 | Implement console-typed-results | Results pane shows severity-grouped cards for a real `test googletest` run with at least one failure; generic fallback adapter covers a command with no specific adapter | Not Started | sonnet | |
| 7 | `/spec-feature` — console-project-context | Feature spec `Approved` | Not Started | sonnet | Can start once Task 4 lands; independent of Task 5/6 |
| 8 | Implement console-project-context | Switching project in the top bar changes nav/defaults without clobbering user-edited form fields; targets sourced from `pipeline.toml` via existing loader, not reimplemented | Not Started | sonnet | |
| 9 | `/spec-feature` — console-presets | Feature spec `Approved` | Not Started | sonnet | Independent of Task 7/8; can run concurrently |
| 10 | Implement console-presets | `dia preset run smoke-e2e` works from a repo-shared file; a user-local preset with a colliding ID overrides the shared one; "save current" UI action writes to the local file | Not Started | sonnet | |
| 11 | `/spec-feature` — console-desktop-install | Feature spec `Approved` | Not Started | sonnet | Depends on Task 4 (needs a working shell to package); independent of Task 5/6/7/8/9/10 |
| 12 | Implement console-desktop-install | PyInstaller produces a windowed `DiaConsole.exe` with a bundled icon; `dia console install-shortcut` creates/overwrites a Desktop `.lnk` targeting it; double-clicking the shortcut opens DiaConsole with no terminal window flash | Not Started | sonnet | |

## Notes

- Recommended sequencing: 1→2 must complete before anything else starts. 3→4 next (needs the model to render against). 5→6 needs the shell to display results in. 7/8, 9/10, and 11/12 can all run concurrently with each other and with 5/6 once 4 is done.
- Per the system spec, this system cannot move to `Done` until all 5 feature specs are `Approved` (CLAUDE.md spec workflow rule).

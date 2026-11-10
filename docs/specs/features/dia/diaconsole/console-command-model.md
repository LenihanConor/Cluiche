# Feature Spec: DiaConsole — Console Command Model

## Parent System
@docs/specs/applications/dia/systems/diaconsole/diaconsole.md

**Status:** `Approved`

---

## Summary

Build the UI-independent command/execution/result model that every other DiaConsole feature depends on: a `CommandRegistry` reflected from DiaCLI's existing Click command tree (`dia_console/registry.py`), typed dataclasses for describing commands/requests/events/results (`dia_console/model.py`), and an `ExecutionService`/`ExecutionHandle` that runs commands exclusively via subprocess and streams their progress (`dia_console/execution.py`). No UI, no results-pane rendering, no project context, no presets — just the model layer and proof that it can reflect the real command tree and run a real command end-to-end without collisions.

---

## Goals

1. `CommandRegistry.from_click_app(cli_main.cli)` reflects every current DiaCLI command/subcommand (including nested groups like `test`, `check`, `docs`, `env`, `scaffold`, `asset`, `reflect`, `capture`, `api`, `agent`) into `CommandDescriptor`s with correct dotted-path IDs (e.g. `test.googletest`, `check.arch`).
2. `ArgumentDescriptor`/`OptionDescriptor` capture each Click param's name, help, required/default/multiple flags, and a small closed `ArgumentType` enum (`STRING`, `INT`, `FLOAT`, `BOOL`, `PATH`, `CHOICE`) — any Click type outside that set degrades to `STRING` with the raw Click type name preserved for debugging, never a crash.
3. `ExecutionService.execute()` spawns `["dia", *descriptor.path, *args]` via subprocess only — no in-process Click invocation anywhere in this feature (SD-CONSOLE-009).
4. Every execution passes its own `--log-json Cluiche/out/DiaCLI/logs/console/<execution_id>.ndjson` path; two concurrent executions of the same command never collide on the same NDJSON file.
5. `ExecutionHandle` exposes two independent async streams: `events()` (typed `ExecutionEvent`s tailed from the NDJSON file) and `log_lines()` (raw stdout/stderr text) — not merged into one stream.
6. `ExecutionHandle.cancel()` kills the subprocess (and any child it spawned) and causes `events()` to terminate with an `execution.cancelled` event.
7. The registry is rebuilt by fresh reflection on every call — no persistent cache (SD-CONSOLE-010).

---

## Data Contracts

Dataclasses live in `dia_console/model.py`, exactly matching the system spec's Data Contracts section, with the `ArgumentType` enum added per Goal 2:

```python
class ArgumentType(str, Enum):
    STRING = "string"
    INT = "int"
    FLOAT = "float"
    BOOL = "bool"
    PATH = "path"
    CHOICE = "choice"

@dataclass(frozen=True)
class ArgumentDescriptor:
    name: str
    help: str
    type: ArgumentType
    required: bool
    default: Any | None
    multiple: bool
    raw_click_type: str          # debugging fallback, e.g. "IntRange" when type degrades to STRING

@dataclass(frozen=True)
class OptionDescriptor:
    name: str                      # Python param name, e.g. "module_filter" — the key used in ExecuteCommandRequest.options
    help: str
    type: ArgumentType
    required: bool
    default: Any | None
    multiple: bool
    is_flag: bool
    choices: tuple[str, ...] = ()   # populated only when type == CHOICE
    raw_click_type: str = ""
    cli_flag: str = ""              # the real flag as typed on the command line, e.g. "--module"
```

**Discovered during implementation — `cli_flag` field.** An audit of the real DiaCLI command tree found 28 options where the Click param name differs from its flag (`@click.option("--module", "module_filter", ...)` → param name `module_filter`, flag `--module`; likewise `--filter`→`filter_pattern`, `--json`→`output_json`, and more). `name` alone cannot round-trip into correct argv for these — a name-derived guess (`--module-filter`) would be wrong. `cli_flag` carries the actual declared flag string; argv serialization uses `cli_flag`, never a name-derived one.

**Discovered during implementation — `--log-json` argv placement.** `--log-json` is a *root* option on the `dia` app (`DiaCLIWithOutput`), not a subcommand option — it must precede `descriptor.path` in argv, not follow it as this spec's Public Interfaces snippet implied. Verified by hand: `dia --log-json <path> asset build --target cluichetest` → exit 0, 257 NDJSON lines written; putting it after `asset build` makes Click reject it as an unknown option on the `asset` group. Final argv shape: `[*argv_prefix, "--log-json", <path>, *descriptor.path, *option_flags, *positional_args]`.

`CommandDescriptor`, `ExecuteCommandRequest`, `ExecutionEvent`, `ResultRecord` are as already defined in the system spec — this feature does not change their shape, only implements the registry/service that produce and consume them.

**`ExecutionEvent.type` — resolved mapping from DiaCLI's real wire format.** `OutputContext` (`dia_cli/utils/dia_output.py`) writes flat dicts keyed by an open-ended `event` field (`OnRunStarted`, `OnStepFailed`, `OnAssetTransformed`, ...) — not the dotted `type` values the system spec's Data Contract assumed. `ExecutionService` translates on read, via this closed table plus a fallback:

| Real `event` value | `ExecutionEvent.type` |
|---|---|
| `OnRunStarted` | `execution.started` |
| `OnRunCompleted` | `execution.completed` |
| `OnRunFailed` | `execution.failed` |
| `OnStageStarted`, `OnStepStarted` | `step.started` |
| `OnStageCompleted`, `OnStepCompleted` | `step.completed` |
| `OnStageFailed`, `OnStepFailed` | `step.failed` |
| `OnStageSkipped` | `step.skipped` |
| `OnLogLine` | `log` |
| anything else (e.g. `OnAssetValidated`, `OnAssetTransformed`, `OnAssetDeployed`, `OnAssetFailed`, `OnBuildCompleted`, or any future system-specific event) | `step.progress`, with the original name preserved as `payload["rawEvent"]` |

`step.failed` and `step.skipped` extend the system spec's original 9-value enum (which only had `execution.started/completed/failed`, `step.started/progress/completed`, `log`, `result.produced`, `execution.cancelled`) — real commands emit failure/skip at the stage level and the enum needs to represent that. `execution.cancelled` is never read from NDJSON; `ExecutionHandle.cancel()` synthesizes it directly.

Field extraction from the raw dict: `step_id` = `payload.get("step") or payload.get("stage")` (`None` if neither present); `time` = `datetime.fromtimestamp(payload["ts"])` — `OutputContext._write_event` unconditionally overwrites `ts` with `time.time()` (float epoch) immediately before serializing, so this is reliable regardless of what a caller pre-set; `payload` on `ExecutionEvent` = the raw dict minus `event`/`ts` (plus `rawEvent` for the fallback case above); `seq` = an incrementing counter `ExecutionService` assigns per line read, starting at 0.

**Coverage caveat, not a bug to work around:** most DiaCLI commands (`show`, `check`, `test`, `env`, `scaffold`, etc.) never call `ctx.obj.output.run_started`/`emit` at all — they write via `logger.info`/`click.echo` directly and never open the NDJSON file, `--log-json` override or not. For these, `events()` legitimately times out per Goal 5/Open Design Question 2 and yields `execution.failed`. Only `dia asset build` (and future commands that adopt `OutputContext`) produce a real event stream today. `log_lines()` still works for every command regardless, since it reads the subprocess's stdout/stderr pipes directly.

**`ExecutionHandle` surface (new, not in the system spec's model snippet — elaborated here):**

```python
class ExecutionHandle:
    execution_id: str

    async def events(self) -> AsyncIterator[ExecutionEvent]: ...   # tailed from NDJSON
    async def log_lines(self) -> AsyncIterator[str]: ...           # raw stdout/stderr
    async def cancel(self) -> None: ...                            # kills subprocess + children
```

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | `dia_console/model.py` — add `ArgumentType` enum and all frozen dataclasses from the system spec plus this feature's `ArgumentDescriptor`/`OptionDescriptor` fields | No STL/Click types leak into the dataclasses — plain `str`/`int`/`float`/`bool`/`tuple`. |
| 2 | `dia_console/registry.py` — `CommandRegistry.from_click_app()` walking `click.MultiCommand.list_commands()`/`get_command()` recursively | Public Click API only — no `dia_cli.cli_main._find_modules()`. Build dotted-path `id` from the group/command name chain. |
| 3 | Map each `click.Argument`/`click.Option` to `ArgumentDescriptor`/`OptionDescriptor`, including the `ArgumentType` degrade-to-`STRING` fallback | `click.Choice` → `CHOICE` with `choices` populated; `click.Path` → `PATH`; `click.IntRange`/custom types → `STRING` + `raw_click_type` set. |
| 4 | `dia_console/execution.py` — `ExecutionService.execute()` building the subprocess argv per the resolved round-tripping rules below, spawning with `creationflags=subprocess.CREATE_NEW_PROCESS_GROUP`, and always injecting `--log-json Cluiche/out/DiaCLI/logs/console/<execution_id>.ndjson` | Create the `logs/console/` directory if missing. `execution_id` = `uuid4().hex`. Flags: `True`→`--flag`, `False`→omit. `multiple=True`→repeat option per value. Positional/`nargs=-1`→append in order. Always pass explicitly even if equal to default. All values `str()`-coerced. |
| 5 | `ExecutionHandle.events()` — poll for the NDJSON file every 100ms up to a 5s timeout, then tail it as it grows, parse each line as `ExecutionEvent`, stop when `execution.completed`/`failed`/`cancelled` is seen | If the file never appears within 5s, yield a terminal `execution.failed` event with message `"output stream never initialized"`. |
| 6 | `ExecutionHandle.log_lines()` — separately stream raw stdout/stderr lines from the subprocess pipes | Independent of `events()`; a consumer using only one must not block on the other. |
| 7 | `ExecutionHandle.cancel()` — force-kill the subprocess's whole process tree via `taskkill /F /T /PID <pid>`; ensure `events()` yields a terminal `execution.cancelled` event | Not `CTRL_BREAK_EVENT` — descendants (MSBuild, cppcheck, googletest.exe) aren't guaranteed to handle a cooperative signal, and graceful shutdown isn't required for a dev-tool cancel action. |
| 8 | Test: reflect the real DiaCLI command tree and assert every currently-registered command/subcommand appears with a correctly-formed dotted ID | Golden-list assertion, not a hardcoded count, so new commands don't silently break the test. |
| 9 | Test: run `dia asset build --target cluichetest` end-to-end via `ExecutionService`, assert `events()` yields `execution.started` → at least one `step.*`/`step.progress` → `execution.completed`, and `log_lines()` yields stdout | Chosen because it's the only command today that actually calls `OutputContext.run_started`/`emit` — proves the real happy path, not a command that would time out. Also add a small test confirming a non-instrumented command (e.g. `dia show config`) completes via `log_lines()` while `events()` times out to `execution.failed` per the coverage caveat above — this is expected behavior, not a defect, and should be asserted as such. |
| 10 | Test: launch 2 concurrent `dia asset build --target cluichetest` executions, assert both complete successfully with distinct `--log-json` files and no cross-contaminated events | Directly proves Goal 4. |
| 11 | Test: `cancel()` on a long-running execution (e.g. a command with a sleep/wait step, or a synthetic slow test command) terminates the subprocess and yields `execution.cancelled` | Directly proves Goal 6. |

---

## Binding Decisions

| Decision | Implication |
|----------|--------------|
| SD-CONSOLE-001 — Command descriptors are reflected from Click, not hand-authored | `CommandRegistry.from_click_app()` is the only construction path for `CommandDescriptor`; no parallel YAML descriptor file. |
| SD-CONSOLE-002 — Execution events wrap DiaCLI's existing `dia.output.v1` NDJSON stream | `ExecutionEvent` fields map 1:1 onto that schema; this feature does not add new wire event types (`log_lines()` stays a separate raw-text stream rather than a synthetic `ExecutionEvent.type == "log"`, precisely to avoid amending SD-CONSOLE-002's schema). |
| SD-CONSOLE-005 — UI depends only on the model's interfaces, never constructs business logic directly | This feature is exactly that model/interface layer; no UI code exists yet to violate it, but the API surface (`ExecutionService`, `ExecutionHandle`) must not leak subprocess/Click details that would tempt a future UI to bypass it. |
| SD-CONSOLE-009 — Execution is subprocess-only, never in-process | `ExecutionService.execute()` always spawns `dia` as a subprocess; no code path imports and calls Click command callbacks directly. |
| SD-CONSOLE-010 — No persistent registry cache in v1 | `from_click_app()` re-walks the Click tree on every call; no on-disk or in-memory cache surviving across calls. |

---

## Open Design Questions — Resolved

1. **Windows child-process termination.** Confirmed by grepping `Dia/DiaCLI`: `pipeline`, `check`, `env`, and `test` commands already shell out to further subprocesses (MSBuild compile stage, cppcheck, googletest runner, toolchain installers), so a plain `Popen.terminate()` on the immediate `dia` process would leave descendants running. **Decision:** spawn with `creationflags=subprocess.CREATE_NEW_PROCESS_GROUP`, and implement `cancel()` as `taskkill /F /T /PID <pid>` rather than `CTRL_BREAK_EVENT` — a forceful whole-tree kill instead of relying on every descendant (MSBuild, cppcheck, googletest.exe) to handle a cooperative signal gracefully, which isn't guaranteed and isn't needed for a dev-tool cancel action.
2. **NDJSON tail race at execution start.** **Decision:** poll for the `--log-json` file's existence every 100ms, up to a 5s timeout. `OutputContext` creates the file essentially immediately on command start, so 5s is generous headroom. If the file still doesn't exist after the timeout, `events()` yields a terminal `execution.failed` event with message `"output stream never initialized"` rather than hanging indefinitely.
3. **Argument value round-tripping.** **Decision — explicit serialization rules in `ExecutionService`:**
   - Flags (`is_flag=True`): `True` → emit `--flag`; `False` → omit entirely (no `--no-flag` handling in v1).
   - `multiple=True`: repeat the option once per value (`--filter x --filter y`).
   - Positional arguments (including Click `nargs=-1`): append values directly in declared order.
   - Always pass the value explicitly in argv, even when it equals the Click default — deterministic, and avoids silent drift if a command's default changes in a future DiaCLI version.
   - All values are `str()`-coerced before being appended to argv.

---

## Acceptance Criteria

- `CommandRegistry.from_click_app(cli_main.cli)` returns a `CommandDescriptor` for every currently-registered DiaCLI command and subcommand, with correctly nested dotted-path IDs.
- Every `ArgumentDescriptor`/`OptionDescriptor` produced has a valid `ArgumentType`; no exception is raised for any Click param type present in the current command tree.
- `ExecutionService.execute()` for `dia asset build --target cluichetest` completes end-to-end via subprocess, with `events()` yielding a terminal `execution.completed` event and `log_lines()` yielding the command's stdout.
- A non-instrumented command (e.g. `dia show config`) completes via `log_lines()` while `events()` times out to a terminal `execution.failed` — asserted as expected behavior per the coverage caveat, not treated as a failure.
- Two concurrent `dia asset build --target cluichetest` executions each get a distinct `--log-json` file and complete without cross-contaminated events.
- `ExecutionHandle.cancel()` force-kills a running execution's whole process tree (verified against a command that itself spawns a subprocess, e.g. `dia check cppcheck`) via `taskkill /F /T`, and `events()` yields a terminal `execution.cancelled` event.
- Flag/multiple/positional argument serialization matches the resolved round-tripping rules exactly (explicit `--flag`/omit, repeated `multiple` options, ordered positionals, always-explicit defaults).
- `events()` never hangs indefinitely if the NDJSON file fails to appear — it terminates with `execution.failed` after the 5s timeout.
- No code path in this feature invokes a Click command callback in-process.
- Registry reflection re-walks the Click tree on every `from_click_app()` call — no cache.

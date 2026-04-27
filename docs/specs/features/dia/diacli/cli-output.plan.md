# Plan: cli-output

**Spec:** @docs/specs/features/dia/diacli/cli-output.md  
**Status:** Done  
**Started:** 2026-04-26  
**Last Updated:** 2026-04-26

## Implementation Patterns

### `OutputContext` class
Single class in `dia_cli/utils/dia_output.py`. Owns two concerns: terminal rendering (via `rich.Console`) and NDJSON file writing. Thread-safe via `threading.Lock`. One instance per CLI invocation, stored on the Click context object as `ctx.obj.output`.

```python
class OutputContext:
    def __init__(self, log_dir: Path, no_color: bool = False, quiet: bool = False): ...
    def run_started(self, system: str, **kwargs) -> None: ...
    def stage_started(self, system: str, stage: str) -> None: ...
    def step_started(self, system: str, stage: str, step: str, detail: str = "") -> None: ...
    def step_completed(self, system: str, stage: str, step: str) -> None: ...
    def step_failed(self, system: str, stage: str, step: str, error: str) -> None: ...
    def stage_completed(self, system: str, stage: str) -> None: ...
    def stage_failed(self, system: str, stage: str, error: str) -> None: ...
    def run_completed(self, system: str, pass_count: int, fail_count: int) -> None: ...
    def run_failed(self, system: str, pass_count: int, fail_count: int) -> None: ...
    def log(self, system: str, level: str, message: str, stage: str = "") -> None: ...
```

### Timing pattern
Each `*_started` method records `time.time()` keyed by `(system, stage, step)`. Each `*_completed`/`*_failed` method computes `durationMs = int((time.time() - start_time) * 1000)`.

### NDJSON write pattern
`_write_event(payload: dict)` — adds `ts: time.time()`, serialises with `json.dumps`, writes + newline + immediate `flush()`. File opened in write mode at `run_started` (truncates previous run).

### Log path resolution
`log_dir` is `repo_root / "Cluiche/out/DiaCLI/logs"`. The `system` name (pipeline/env/test) becomes a subdirectory. `repo_root` resolved the same way `cli_main.py` does — walking up from cwd for `dia_cli_prime_config.json`.

### Terminal rendering
`rich.Console(highlight=False, markup=False)` to avoid rich's automatic markup parsing interfering with output. Use explicit `rich.style.Style` for colours. Timestamp prefix `[HH:MM:SS]` from `datetime.now().strftime`.

### `cli_main.py` integration
`OutputContext` instantiated after config is loaded, stored as `ctx.obj.output`. Global flags `--no-color`, `--quiet`, `--log-json` added to the top-level `DiaCLI` Click command.

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Add `rich >= 13.0` to `pyproject.toml` and install | Done | |
| 2 | Implement `dia_output.py` — `OutputContext` class with all event methods, NDJSON writer, terminal renderer | Done | |
| 3 | Wire `OutputContext` into `cli_main.py` — global flags, attach to Click context | Done | |
| 4 | Smoke test — run `dia` and verify no regressions, verify log file created | Done | |
| 5 | Update spec status to Done | Done | |

## Session Notes

### 2026-04-26
- Plan created. Starting with pyproject.toml dependency addition.

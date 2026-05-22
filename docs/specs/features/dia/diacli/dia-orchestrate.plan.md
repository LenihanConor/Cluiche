# Plan: dia-orchestrate

**Spec:** @docs/specs/features/dia/diacli/dia-orchestrate.md
**Status:** Done
**Started:** 2026-05-21

## Session Notes

Implements the `dia orchestrate` CLI command and pytest plugin. All Python — no C++ changes.

**Spec decisions summary:**
- AR-1: sync-native WebSocket via `websockets.sync.client` (no async ceremony, plain `def test_*`).
- AR-2: App readiness = retry WebSocket connect every 500ms up to 60s.
- AR-3: `assert_no_log_errors` reads DiaObservation session JSONL files at teardown; file-based, no new C++ commands.
- AR-4: `--suite=<app>/<name>` maps to `plans/<app>/<name>.json`.
- AR-5: Teardown = graceful quit via `dia.app.quit` first, then force-kill after 5s.
- SD-CLI-004: `orchestrate.py` in `dia_cli/cli/` — auto-discovered.
- SD-CLI-006: Click framework for all argument parsing.
- SD-CLI-008: Exit code maps directly to pytest exit code.

**Import pattern:** `conftest.py` uses `pytest_plugins = ["orchestrator.plugin"]` with `Tools/` on `sys.path`. `plugin.py` and `cli.py` use absolute imports (`orchestrator.client`, `orchestrator.cli`).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Tools/orchestrator/` directory structure, `pyproject.toml` | Directory exists | Done | haiku | |
| 2 | `client.py` — DiaClient sync | Helpers wrap send_command | Done | sonnet | |
| 3 | `plugin.py` — fixtures + implicit assertion | Log error grep, markers registered | Done | sonnet | |
| 4 | `cli.py` — plan loading + pytest invocation | load_plan + _DiaPlugin | Done | sonnet | |
| 5 | `Dia/DiaCLI/dia_cli/cli/orchestrate.py` — DiaCLI entry | `dia orchestrate --help` works | Done | haiku | |
| 6 | `conftest.py` + `plans/cluichetest/default.json` | pytest_plugins registered | Done | haiku | |
| 7 | `dia test cli` — plugin discovery passes | 6/6 pass | Done | sonnet | |
| 8 | Update specs + backlog, commit | Docs | Done | haiku | |

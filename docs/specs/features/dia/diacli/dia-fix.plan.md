# Plan: dia-fix

**Spec:** @docs/specs/features/dia/diacli/dia-fix.md  
**Status:** Done  
**Started:** 2026-05-05  
**Last Updated:** 2026-05-05

## Implementation Patterns

### Command skeleton (Tasks 1)
Follow the existing two-file pattern established by `asset` and `pipeline` commands:
- `dia_cli/cli/fix.py` — thin entry point, exports `cli`, adds subcommand via `cli.add_command(fix_cmd)`
- `dia_cli/commands/fix/group.py` — defines the `@click.command("fix")` with all options
- `dia_cli/commands/fix/__init__.py` — empty

### Handler (Task 2)
`fix_handler.py` builds a `list[str]` aider command and calls `subprocess.run()` with no `capture_output` (so aider streams directly to terminal per AI Review Q2). Propagate aider's exit code directly as the process exit code.

Aider invocation shape:
```python
cmd = ["aider", "--test-cmd", test_cmd, "--model", model, "--auto-test", "--yes"]
result = subprocess.run(cmd)
sys.exit(result.returncode)
```

`test_cmd` is assembled as `f"dia run {target}"` with optional `--filter` and `--config` appended.

### Env verify (Task 3)
Add a `_check_local_llm()` function to `verify_orchestrator.py` following the exact pattern of `_check_claude()`. Returns a list of `CheckResult` objects under category `"local-llm"`. Three checks:
1. `aider` — `shutil.which("aider")` → pass/fail, fix hint: `dia env setup`
2. `ollama` — `shutil.which("ollama")` → pass/fail, fix hint: install instructions
3. `qwen2.5-coder:14b` — `subprocess.run(["ollama", "list"], capture_output=True)`, check stdout for model name → pass/fail, fix hint: `ollama pull qwen2.5-coder:14b`

Wired into `run()` under `run_all` condition alongside existing checks.

### Env setup (Task 4)
Add a `_setup_local_llm()` function to `setup_orchestrator.py`. Auto-installs `aider-chat` via `subprocess.run(["pip", "install", "aider-chat"])`. Prints manual instructions for ollama install and `ollama pull qwen2.5-coder:14b`.

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 0 | Install prerequisites | Done | — | Ollama 0.23.0 via winget, qwen2.5-coder:14b pulled (9GB), aider 0.86.2 via 64-bit pip, Python Scripts added to user PATH |
| 1 | `dia fix` command skeleton | Done | haiku | `cli/fix.py` as direct `@click.command`; `commands/fix/__init__.py`, `commands/fix/fix_handler.py` |
| 2 | `fix_handler.py` | Done | sonnet | Aider invocation with `--yes --auto-test`; streams to terminal; propagates exit code; guards on aider not on PATH |
| 3 | `dia env verify` — local-llm checks | Done | haiku | `_check_local_llm()` in `verify_orchestrator.py`; checks aider, ollama, qwen2.5-coder:14b; `--local-llm` flag |
| 4 | `dia env setup` — local-llm guidance | Done | haiku | `_setup_local_llm()` in `setup_orchestrator.py`; auto-installs aider, adds PATH, prints ollama instructions |
| 5 | Tests | Done | sonnet | 15 tests all passing: fix_handler arg construction, CLI discovery, env verify checks |
| 6 | Commit | Done | haiku | |

## Session Notes

### 2026-05-05
- Spec approved after interview. Key decisions: `--yes` always forced, aider streams directly to terminal, repo-map used (no explicit file args), aider auto-installed via pip, ollama pull is manual only.

# Feature Spec: DiaConsole — Console Desktop Install

## Parent System
@docs/specs/applications/dia/systems/diaconsole/diaconsole.md

**Status:** `Approved`

---

## Summary

`dia console` (typing it in a terminal) already launches DiaConsole's native window, but Poetry's `console_scripts` launcher always uses the console subsystem on Windows — double-clicking a shortcut to it would flash a terminal window first. This feature packages DiaConsole as a real, windowed, double-clickable `DiaConsole.exe` via PyInstaller, and provides `dia console install-shortcut` to script creation of a Desktop `.lnk` pointing at it — so a developer can launch DiaConsole the same way they'd launch any other desktop app, without typing anything.

Two new subcommands land under the existing `dia console` group: `dia console build` (produces the packaged exe) and `dia console install-shortcut` (creates/overwrites the Desktop shortcut). Both are additive (SD-CONSOLE-008) — `dia console` on its own, with no subcommand, keeps launching the shell exactly as it does today.

---

## Goals

1. `dia_console/packaging/entrypoint.py` — a minimal script (`from dia_console.shell import launch; launch()` under `if __name__ == "__main__"`) that PyInstaller's `Analysis` targets. PyInstaller needs a plain Python entry script, not a Click command object, so this is a thin wrapper around the same `launch()` `dia console` (no subcommand) already calls.
2. `dia_console/packaging/DiaConsole.ico` — a simple placeholder icon (any valid multi-resolution `.ico`, generated once as part of this feature, e.g. via Pillow at build/dev time and committed as a binary asset — no external design tool needed). Explicitly a placeholder: swappable later for real DiaConsole branding without touching any build config, since the `.spec` file just references this filename.
3. `dia_console/packaging/DiaConsole.spec` — a PyInstaller spec producing a **windowed** (`console=False`), **one-file** build named `DiaConsole`, with `icon=DiaConsole.ico`, targeting `entrypoint.py`.
4. `dia_cli/cli/console.py`'s `cli` becomes a `click.group(invoke_without_command=True)`: invoking `dia console` with no subcommand keeps calling `launch()` exactly as today (regression-safe); `dia console build` and `dia console install-shortcut` are new subcommands under the same group.
5. `dia console build` runs PyInstaller against `DiaConsole.spec` as a subprocess (`sys.executable -m PyInstaller ...`, same "prefer installed launcher, fall back to `-m`" pattern `dia_console/execution.py`'s `default_argv_prefix` already uses — PyInstaller may not be on `PATH` in every dev env), with `--distpath <repo_root>/Cluiche/out/DiaCLI/DiaConsole` and `--workpath <repo_root>/Cluiche/out/DiaCLI/DiaConsole/build` (per this repo's `Cluiche/out/<System>/...` convention — confirmed via interview, not PyInstaller's own default `dist/`/`build/`), `--noconfirm` (idempotent re-runs, no interactive overwrite prompt).
6. `dia console install-shortcut` creates `%USERPROFILE%\Desktop\Dia Console.lnk` via `pywin32`'s `win32com.client.Dispatch("WScript.Shell")`, with `TargetPath` = the built `DiaConsole.exe`'s path, `IconLocation` = the same exe path (PyInstaller already embeds `DiaConsole.ico` into the exe, so no separate `.ico` reference is needed at shortcut-creation time), `WorkingDirectory` = the exe's parent folder. **If the exe doesn't exist yet** (per interview decision), fail with a clear `click.ClickException`: `"DiaConsole.exe not found at <path> — run 'dia console build' first."` — it does not auto-trigger a build.
7. Saving a `.lnk` to an existing path via `WScript.Shell`'s `CreateShortcut(path).Save()` naturally overwrites in place — re-running `install-shortcut` is idempotent with no special-case deletion logic needed.
8. New dependencies added to `Dia/DiaCLI/pyproject.toml`: `pyinstaller` (build-time packaging) and `pywin32` (Desktop `.lnk` creation via COM automation) — neither currently used anywhere in DiaCLI.

---

## Data Contracts

No new Python data model or HTTP contract — this feature is pure CLI/build tooling, no new dataclasses in `dia_console/model.py` and no new routes in `dia_console/web/app.py`.

**New CLI surface (`dia_cli/cli/console.py`):**
```bash
dia console                     # unchanged: launches DiaConsole's native window
dia console build                # packages DiaConsole.exe via PyInstaller
dia console install-shortcut     # creates/overwrites the Desktop .lnk
```

**Fixed paths:**
```python
EXE_PATH = repo_root / "Cluiche" / "out" / "DiaCLI" / "DiaConsole" / "DiaConsole.exe"
SHORTCUT_PATH = Path.home() / "Desktop" / "Dia Console.lnk"
```

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | `dia_console/packaging/entrypoint.py` | Just calls `dia_console.shell.launch()`. No CLI parsing, no Click. |
| 2 | `dia_console/packaging/DiaConsole.ico` | Placeholder, generated once (e.g. a small script using Pillow, run once during implementation — not a runtime dependency of DiaConsole itself) and committed as a binary asset. |
| 3 | `dia_console/packaging/DiaConsole.spec` | `console=False`, one-file, `icon="DiaConsole.ico"`, `Analysis(["entrypoint.py"], ...)`. |
| 4 | `dia_cli/cli/console.py` — convert `cli` to `click.group(invoke_without_command=True)`, preserving today's no-subcommand `launch()` behavior | Regression: `dia console` alone must still work exactly as before. |
| 5 | `dia console build` subcommand | Subprocess call to PyInstaller with the `--distpath`/`--workpath`/`--noconfirm` flags from Goal 5. Reports the final exe path on success; surfaces PyInstaller's stderr on failure rather than swallowing it. |
| 6 | `dia console install-shortcut` subcommand | Missing-exe check (Goal 6) before touching `win32com`. Idempotent overwrite (Goal 7). |
| 7 | `pyproject.toml` — add `pyinstaller`, `pywin32` | |
| 8 | Test: `dia console` with no subcommand still calls `launch()` (mocked) — regression guard for the group conversion | |
| 9 | Test: `dia console build`'s subprocess argv (mocked `subprocess.run`) includes the right `--distpath`/`--workpath`/`--noconfirm`/spec-file arguments — does not actually invoke PyInstaller in the unit test | Real PyInstaller builds take minutes and produce a 100MB+ binary — not something every test run should pay for. |
| 10 | Test: `dia console install-shortcut` errors clearly (via injectable/mocked exe-exists check) when the exe is missing, and (via mocked `win32com`) builds the right `TargetPath`/`IconLocation`/`WorkingDirectory` when it exists | `win32com` is Windows-COM-only — inject/mock it the same way `ExecutionService`/`run_preset` already take injectable dependencies for testability, rather than requiring a real COM environment in CI. |
| 11 | Test (marked `@pytest.mark.integration`, skipped by default — same convention as `console-command-model`'s one real-subprocess test): a real end-to-end `dia console build` run produces a real `DiaConsole.exe` at the expected path | Slow; opt-in only. |

---

## Binding Decisions

| Decision | Implication |
|----------|--------------|
| SD-CONSOLE-012 — desktop entry point is a PyInstaller windowed one-file exe plus a scripted `.lnk`, not a manual pythonw/Poetry-script setup | This feature is the direct implementation of that decision — nothing here is a new design choice at the system level, only the concrete file/task breakdown. |
| SD-CONSOLE-008 — DiaConsole is additive; every existing `dia <command>` invocation is unaffected | `dia console`'s no-subcommand behavior is preserved exactly (Task 4/8); `build`/`install-shortcut` are pure additions. |
| PD-005 — x64 is the only supported build target | PyInstaller packages whatever Python/venv architecture it's run from; this feature doesn't add a second architecture — the packaged exe is x64, consistent with the rest of the platform. |

---

## Open Design Questions

1. **One-file PyInstaller build size/time.** Bundling `fastapi`/`uvicorn`/`pywebview`/`pyyaml`/the interpreter itself into a single exe typically produces a 80-150MB binary and can take 1-3 minutes to build. `dia console build` is a manual, on-demand developer action in v1 — it is not wired into any CI pipeline or `dia pipeline` target. If that assumption changes (e.g. someone wants DiaConsole.exe published as a release artifact), revisit whether one-file is still the right PyInstaller mode versus one-folder (faster to build, slower to distribute as a single file).
2. **Placeholder icon lifecycle.** The `.ico` committed here is a placeholder, not real DiaConsole branding. Worth a follow-up (outside this feature) once the platform has an actual visual identity for its tooling.

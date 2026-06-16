**Spec:** N/A — refactor of existing DiaCLI command surface
**Status:** In Progress

## Summary

Cleanup pass over DiaCLI accumulated over ~2 months of growth. Nine phases: remove dead commands, consolidate orchestrate into test, kill editor_ui_test duplicate, strip legacy mdk branding + setup command, standardize docstring style, standardize error handling, fix check command surface, resolve reflect/types overlap, and optionally group help output.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Delete `cli/mycommand.py` + `commands/mycommand_cmd.py` | `--help` no longer lists `mycommand` | Not Started | sonnet | |
| 2 | Delete `cli/cli_prefixtest.py`; convert test assertion into pytest | `test_cli_prefix_stripped` still passes via pytest-only assertion | Not Started | sonnet | `test_plugin_discovery.py` references `prefixtest` in 3 tests — update those |
| 3 | Remove debug `print()` calls from `cli_main.py:181-183` | `OnlySetupCli` still loads without printing | Not Started | haiku | |
| 4 | Add `dia test e2e` subcommand to `cli/test.py` | `dia test e2e --help` works; options match current `dia orchestrate` | Not Started | sonnet | Copy all options: --suite, --filter, --scenario, --list, --port, --no-launch |
| 5 | Write tests for `dia test e2e` | `test_dia_test_e2e.py`: discovery, --help shows all options, mock `run_orchestration` verifies arg passthrough | Not Started | sonnet | New file `tests/test_dia_test_e2e.py` |
| 6 | Delete `cli/orchestrate.py` | `dia orchestrate` no longer in `--help` | Not Started | haiku | |
| 7 | Delete `cli/editor_ui_test.py` + `commands/editor_ui_test_cmd.py` | `dia editor_ui_test` gone from `--help`; `dia test editor-ui` still passes --watch and --coverage | Not Started | sonnet | `editor_ui_test_cmd.py` has --coverage; add `--coverage` to `cli/test.py` editor-ui subcommand first |
| 8 | Update `test_editor_ui_test_cmd.py` to point at `dia test editor-ui` | Tests pass via `dia test editor-ui` instead of `dia editor_ui_test` | Not Started | sonnet | Rewrite CLI discovery tests; keep implementation unit tests but re-import from new location |
| 9 | Delete `cli/setup.py` + `commands/setup_cmd.py` + `commands/setup/` | `dia setup` no longer appears in `--help` | Not Started | sonnet | Verify nothing imports setup_cmd; check if `commands/setup/executable_cmd.py` + `setpath_cmd.py` are dead |
| 10 | Strip mdk branding from `cli/command.py` + `cli/show.py` + `cli_main.py` | `--help` text contains no "mdk" references | Not Started | haiku | `command.py`: "mdk command create" → "dia command create"; `show.py`: "mdk show" → "dia show"; `cli_main.py`: class docstring |
| 11 | Rewrite docstrings in `cli/command.py`, `cli/show.py` | `dia command --help`, `dia show --help` show clean prose | Not Started | haiku | Remove `"""!`, `\f`, `@param`, `@defgroup`, `## @{`, `## @}` |
| 12 | Evaluate and remove `clean_doc_str()` from `cli_main.py` | `dia --help` and all subcommand `--help` unchanged | Not Started | sonnet | Only safe once all `!` prefixes are gone from every cli/ file |
| 13 | `cli/check.py` — replace `raise SystemExit` with `ctx.exit` | `dia check` exits cleanly; `ctx.exit` called consistently | Not Started | haiku | Add `@click.pass_context`, change all `raise SystemExit(N)` |
| 14 | `cli/cli_validate.py` — replace `raise SystemExit(1)` with `ctx.exit(1)` | `dia validate manifest` exits cleanly | Not Started | haiku | |
| 15 | Write tests for `dia validate manifest` | `test_dia_validate.py`: `--help` discovery, one valid file passes, one invalid file fails with exit 1 | Not Started | sonnet | New file `tests/test_dia_validate.py`; use CliRunner + tmp_path with fixture .diastage files |
| 16 | `cli/api.py` subcommands — replace `return exit_code` with `ctx.exit()` | `dia api exec` exits with correct code | Not Started | haiku | Add `@click.pass_context` to `list`, `exec`, `help` subcommands |
| 17 | Write tests for `dia api` CLI surface | `test_dia_api_cli.py`: `--help` discovery, `list --help`, `exec --help`; mock bridge for exit-code test | Not Started | sonnet | New file `tests/test_dia_api_cli.py` |
| 18 | Convert `dia check` to a group: `cppcheck`, `sanitizer`, `deps`, `sln-sync` | `dia check --help` shows 4 subcommands | Not Started | sonnet | `cppcheck` takes `--accept-baseline`; `deps` wires `arch_checker.py`; `sln-sync` wires `sln_sync.py` |
| 19 | Write tests for new `dia check` subcommand discovery | `test_cli_check_deps.py` updated + new assertions for `dia check cppcheck --help`, `dia check sanitizer --help`, `dia check sln-sync --help` | Not Started | sonnet | Extend existing test file or create `test_dia_check_surface.py` |
| 20 | Update `test_cli_check_deps.py` for new import path | Tests pass against restructured `dia check deps` | Not Started | sonnet | Import path changes from `dia_cli.cli.cli_check` to new location |
| 21 | Update CLAUDE.md to reflect new `check` surface | CLAUDE.md examples match reality | Not Started | haiku | `dia check deps`, `dia check cppcheck`, `dia check sanitizer`, `dia check sln-sync` |
| 22 | Move `types export` logic into `reflect` as `dia reflect export-types` | `dia reflect export-types --help` works; `dia types` gone | Not Started | sonnet | Move `_scan_build_dir` + `export` command into `commands/reflect/`; delete `cli/types.py` |
| 23 | Write test for `dia reflect export-types` discovery | `test_dia_reflect_surface.py`: `--help` shows `export-types` subcommand; `export-types --help` lists options | Not Started | sonnet | New file `tests/test_dia_reflect_surface.py` |
| 24 | Update CLAUDE.md for consolidated `reflect` surface | No `dia types` references in CLAUDE.md | Not Started | haiku | |
| 25 | Update `test_blue_cli.py` | Remove stale mdk-cli version test or repurpose | Not Started | haiku | Currently fully skipped; either delete or replace with a real dia version check |
| 26 | Regression test: final `dia --help` command list assertion | `test_dia_help_final.py`: assert exact set of commands (no `mycommand`, `prefixtest`, `orchestrate`, `editor_ui_test`, `setup`, `types`) | Not Started | sonnet | New file; run after all removals complete. Guards against re-introduction. |
| 27 | (Optional) Help grouping — implement custom Click formatter | `dia --help` shows commands grouped by function | Not Started | opus | Groups: Build (run/launch/pipeline), Test (test/fix), Generate (scaffold/docs/reflect), Env (env), Inspect (check/show/validate/api/command) |

## Notes

- Phases 1-3 are independent and can be done in any order.
- Phase 4 (tasks 4-6): add `test e2e`, write tests for it (task 5), then delete `orchestrate`.
- Phase 5 (tasks 7-8): add `--coverage` to `test editor-ui` before deleting `editor_ui_test`; update tests to new path.
- Phase 6 (task 9): verify `OnlySetupCli` still works standalone before deleting setup; verify no other imports.
- Phase 7 (tasks 10-12): do branding pass (10) before style pass (11), then evaluate clean_doc_str removal (12) last.
- Phase 8 (tasks 13-17): error-handling fixes + new test coverage for validate/api surfaces.
- Phase 9 (tasks 18-21): check restructure is the most invasive surface change; write discovery tests (19) alongside restructure.
- Phase 10 (tasks 22-24): types/reflect merge is self-contained; add surface test (23) to confirm.
- Phase 11 (tasks 25-27): final cleanup and regression guard; task 26 runs last as a gate.
- Task 27 is optional polish; tackle last when final command list is stable.

**Spec:** N/A — refactor of existing DiaCLI command surface
**Status:** Done

## Summary

Cleanup pass over DiaCLI accumulated over ~2 months of growth. Nine phases: remove dead commands, consolidate orchestrate into test, kill editor_ui_test duplicate, strip legacy mdk branding + setup command, standardize docstring style, standardize error handling, fix check command surface, resolve reflect/types overlap, and optionally group help output.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Delete `cli/mycommand.py` + `commands/mycommand_cmd.py` | `--help` no longer lists `mycommand` | Done | sonnet | |
| 2 | Delete `cli/cli_prefixtest.py`; convert test assertion into pytest | `test_cli_prefix_stripped` still passes via pytest-only assertion | Done | sonnet | Replaced with direct unit test of `_remove_cli_cmd_prefix()` |
| 3 | Remove debug `print()` calls from `cli_main.py:181-183` | `OnlySetupCli` still loads without printing | Done | haiku | |
| 4 | Add `dia test e2e` subcommand to `cli/test.py` | `dia test e2e --help` works; options match current `dia orchestrate` | Done | sonnet | |
| 5 | Write tests for `dia test e2e` | `test_dia_test_e2e.py`: discovery, --help shows all options, mock `run_orchestration` verifies arg passthrough | Done | sonnet | 5 tests; patch sys.modules trick for ImportError test |
| 6 | Delete `cli/orchestrate.py` | `dia orchestrate` no longer in `--help` | Done | haiku | |
| 7 | Delete `cli/editor_ui_test.py` + `commands/editor_ui_test_cmd.py` | `dia editor_ui_test` gone from `--help`; `dia test editor-ui` still passes --watch and --coverage | Done | sonnet | Added --coverage to ui_runner.run() first |
| 8 | Update `test_editor_ui_test_cmd.py` to point at `dia test editor-ui` | Tests pass via `dia test editor-ui` instead of `dia editor_ui_test` | Done | sonnet | 11 tests rewritten to test ui_runner directly |
| 9 | Delete `cli/setup.py` + `commands/setup_cmd.py` + `commands/setup/` | `dia setup` no longer appears in `--help` | Done | sonnet | No other files imported from setup modules |
| 10 | Strip mdk branding from `cli/command.py` + `cli/show.py` + `cli_main.py` | `--help` text contains no "mdk" references | Done | haiku | |
| 11 | Rewrite docstrings in `cli/command.py`, `cli/show.py` | `dia command --help`, `dia show --help` show clean prose | Done | haiku | Removed all Doxygen markers |
| 12 | Evaluate and remove `clean_doc_str()` from `cli_main.py` | `dia --help` and all subcommand `--help` unchanged | Done | sonnet | Removed — no `!` prefixes remain anywhere |
| 13 | `cli/check.py` — replace `raise SystemExit` with `ctx.exit` | `dia check` exits cleanly; `ctx.exit` called consistently | Done | haiku | 7 raise SystemExit calls converted |
| 14 | `cli/cli_validate.py` — replace `raise SystemExit(1)` with `ctx.exit(1)` | `dia validate manifest` exits cleanly | Done | haiku | |
| 15 | Write tests for `dia validate manifest` | `test_dia_validate.py`: `--help` discovery, one valid file passes, one invalid file fails with exit 1 | Done | sonnet | 5 tests; patched find_repo_root for tmp_path compat |
| 16 | `cli/api.py` subcommands — replace `return exit_code` with `ctx.exit()` | `dia api exec` exits with correct code | Done | haiku | |
| 17 | Write tests for `dia api` CLI surface | `test_dia_api_cli.py`: `--help` discovery, `list --help`, `exec --help`; mock bridge for exit-code test | Done | sonnet | 6 tests; patched at import site |
| 18 | Convert `dia check` to a group: `cppcheck`, `sanitizer`, `deps`, `sln-sync` | `dia check --help` shows 4 subcommands | Done | sonnet | Added cli_check.py shim for import back-compat |
| 19 | Write tests for new `dia check` subcommand discovery | 4 discovery tests added to `test_cli_check_deps.py` | Done | sonnet | |
| 20 | Update `test_cli_check_deps.py` for new import path | Tests pass against restructured `dia check deps` | Done | sonnet | Import updated from `cli_check` → `check` |
| 21 | Update CLAUDE.md to reflect new `check` surface | CLAUDE.md examples match reality | Done | haiku | Added cppcheck/sanitizer/sln-sync rows and examples |
| 22 | Move `types export` logic into `reflect` as `dia reflect export-types` | `dia reflect export-types --help` works; `dia types` gone | Done | sonnet | Created `commands/reflect/export_types_cmd.py` |
| 23 | Write test for `dia reflect export-types` discovery | `test_dia_reflect_surface.py`: 3 surface tests | Done | sonnet | Anchored regex to avoid false positive from "registered-types" in description |
| 24 | Update CLAUDE.md for consolidated `reflect` surface | No `dia types` references in CLAUDE.md | Done | haiku | Covered in same commit as task 21 |
| 25 | Update `test_blue_cli.py` | Remove stale mdk-cli version test | Done | haiku | Replaced with callable(main) smoke test |
| 26 | Regression test: final `dia --help` command list assertion | `test_dia_help_final.py`: exact command list + no mdk references | Done | sonnet | 2 tests; removed-command check uses anchored regex |
| 27 | (Optional) Help grouping — implement custom Click formatter | `dia --help` shows commands grouped by function | Not Started | opus | Deferred — command list now stable if desired later |

## Notes

- All 26 required tasks complete across 6 commits.
- 658 unit tests pass; 12 pre-existing failures (missing build-env scripts, unrelated env provisioning tests) are unchanged.
- Task 27 (help grouping) is optional polish and can be picked up independently.

**Spec:** @docs/specs/applications/dia/systems/diacli/dev-source-alias.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `launch.py`: `_resolve_dev_aliases` (computed `source` for app targets, `[]` otherwise) + `--source <abs path>` override on `dia launch` and `dia run` (must exist, absolute); insert `--dev-alias` args after `cmd = [exe]` | RED-GREEN: pytest AC1, AC3, AC6, `--source` override | Pending | sonnet | Tests flat in `Dia/DiaCLI/tests/` (`test_launch_dev_aliases.py`), not `tests/commands/` |
| 2 | `dia validate manifest` rejects `source` in `.diagame`/`.diastage` `path_aliases` (`cli_validate.py`) | RED-GREEN: pytest AC5 (validator half) | Pending | sonnet | |
| 3 | `RegisterDevAliases` in `Main.cpp.j2`, `CluicheTest/Main.cpp`, `CoW/Main.cpp`; `RegisterPathAliases` ignores a manifest `source` with an Error log; `main` takes `argc/argv` | Build `dia run cluichetest --build-only` + scaffold test update; manual AC1/AC4 | Pending | sonnet | Spec AC5 GoogleTest half: Main.cpp is not unit-testable; check if alias logic can be exercised, else report |

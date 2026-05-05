# Plan: DiaAssetPipeline

**Spec:** @docs/specs/systems/dia/diaassetpipeline.md  
**Status:** Done  
**Started:** 2026-05-05  
**Last Updated:** 2026-05-05

## Overview

Pure Python implementation inside `Dia/DiaCLI/dia_cli/commands/asset/`. No C++ code. All tests are pytest under `Dia/DiaCLI/tests/`. Build order follows the spec: Feature 1 → Feature 3 → Feature 2 → Feature 4.

## Tasks

| # | Task | Feature | Status | Model | Notes |
|---|------|---------|--------|-------|-------|
| 1 | Core types: `AssetError`, `TransformResult`, `DeployResult`, `AssetHandler` base class | F1 | Done | haiku | `handler.py` — dataclasses + base class with no-op defaults |
| 2 | `AssetHandlerRegistry` | F1 | Done | haiku | `registry.py` — register/get/duplicate error |
| 3 | `BuildContext` dataclass | F1 | Done | haiku | `context.py` — all fields: catalogue, config, platform, app_name, deploy_root, asset_stages, output |
| 4 | `BuildRunner` three-phase loop + NDJSON events | F1 | Done | sonnet | `runner.py` — validate→transform→deploy per asset, collect all errors (SD-APIPE-002), emit 5 event types |
| 5 | Stage filtering logic | F1 | Done | sonnet | `_should_include_asset`: build reverse map from RelationshipIndex contains edges; global-scoped always included; orphaned = warning + include |
| 6 | Tests for Feature 1 | F1 | Done | sonnet | `tests/test_asset_handler_registry.py`, `tests/test_build_runner.py` — 34 tests, all passing |
| 7 | `resolve_deploy_path` + `resolve_category` | F3 | Done | sonnet | `layout.py` — scope→global/stages/<name>, tag→category (ordered match, fallback misc) |
| 8 | `copy_asset` + `copy_asset_directory` + `create_deploy_directories` | F3 | Done | sonnet | `layout.py` — shutil.copy2 for files, shutil.copytree for folders (SD-CAT-013), mkdir parents |
| 9 | `RuntimeManifestGenerator` | F3 | Done | sonnet | `manifest_generator.py` — add_asset(is_folder), build_stages, generate → assets.runtime.json |
| 10 | Tests for Feature 3 | F3 | Done | sonnet | `tests/test_deploy_layout.py`, `tests/test_manifest_generator.py` — 31 tests passing |
| 11 | `DefaultAssetHandler` + 6 standard type handlers | F2 | Done | sonnet | `handlers/default.py` + texture/sprite/audio/config/entity/ui — thin subclasses with type_id and file_pattern; pattern mismatch = warning not error |
| 12 | `FolderHandler` | F2 | Done | sonnet | `handlers/folder.py` — validate checks is_dir, transform no-op, deploy shutil.copytree (SD-CAT-013) |
| 13 | `StageHandler` | F2 | Done | sonnet | `handlers/stage.py` — extends DefaultAssetHandler, validates name field is non-empty string; does NOT validate member assets (SD-CAT-012) |
| 14 | Auto-registration | F2 | Done | sonnet | `handlers/__init__.py` — `register_built_in_handlers(registry)` importing all 8 handlers |
| 15 | Tests for Feature 2 | F2 | Done | sonnet | `tests/test_built_in_handlers.py` — 21 tests passing |
| 16 | `pipeline.toml` config loader | F4 | Done | sonnet | `config_loader.py` — uses `toml` package (matches DiaCLI convention, not tomllib) |
| 17 | `dia asset build` command | F4 | Done | sonnet | `build_cmd.py` + `build_handler.py` — --target (required), --config (Debug/Release), --platform (x64), --force |
| 18 | `dia asset validate` command | F4 | Done | sonnet | `validate_cmd.py` + `validate_handler.py` — validate-only phases, no file writes |
| 19 | `dia asset deploy` command | F4 | Done | sonnet | `deploy_cmd.py` + `deploy_handler.py` — deploy-only phases |
| 20 | `dia asset` command group + plugin registration | F4 | Done | sonnet | `group.py` + `cli/asset.py` DiaCLI shim |
| 21 | Tests for Feature 4 | F4 | Done | sonnet | `tests/test_cli_asset_commands.py` — 19 tests passing |
| 22 | Wire `build-assets` stage in DiaPipeline | F4 | Done | sonnet | Replaced no-op stub (SD-PIPE-005) with run_asset_phases delegation |

## File Map

```
Dia/DiaCLI/dia_cli/commands/asset/
    __init__.py
    handler.py             # Task 1 — AssetError, TransformResult, DeployResult, AssetHandler
    registry.py            # Task 2 — AssetHandlerRegistry
    context.py             # Task 3 — BuildContext
    runner.py              # Task 4+5 — BuildRunner
    layout.py              # Task 7+8 — path resolution + copy utilities
    manifest_generator.py  # Task 9 — RuntimeManifestGenerator
    config_loader.py       # Task 16 — pipeline.toml reader
    group.py               # Task 20 — 'dia asset' Click group
    build_cmd.py           # Task 17
    build_handler.py       # Task 17
    validate_cmd.py        # Task 18
    validate_handler.py    # Task 18
    deploy_cmd.py          # Task 19
    deploy_handler.py      # Task 19
    handlers/
        __init__.py        # Task 14 — register_built_in_handlers
        default.py         # Task 11 — DefaultAssetHandler
        texture.py         # Task 11
        sprite.py          # Task 11
        audio.py           # Task 11
        config.py          # Task 11
        entity.py          # Task 11
        ui.py              # Task 11
        folder.py          # Task 12 — FolderHandler
        stage.py           # Task 13 — StageHandler

Dia/DiaCLI/tests/
    test_asset_handler_registry.py   # Task 6
    test_build_runner.py             # Task 6
    test_deploy_layout.py            # Task 10
    test_manifest_generator.py       # Task 10
    test_built_in_handlers.py        # Task 15
    test_cli_asset_commands.py       # Task 21
```

## Session Notes

### 2026-05-05
- Plan created. 22 tasks across 4 features, all Python (no C++ changes except wiring the DiaPipeline stub in task 22).
- Build order: F1 (tasks 1–6) → F3 (tasks 7–10) → F2 (tasks 11–15) → F4 (tasks 16–22).
- Key design notes carried over from specs:
  - Validate fail → skip transform + deploy for that asset; other assets continue (SD-APIPE-002, F1 AI Q2)
  - Pattern mismatch is a warning, not an error (F2 AI Q4)
  - Stage filtering pre-builds reverse map from RelationshipIndex contains edges (F1 AI Q5)
  - tomllib/tomli fallback pattern already exists in DiaCLI (F4 AI Q2)
- Feature 1 complete (tasks 1–6): handler.py, registry.py, context.py, runner.py created; 34 tests passing.
  - Added `OutputContext.emit()` public method to `dia_output.py` for custom NDJSON events.
  - Stage filtering reads `contains` edges directly from catalogue asset records (stage-type assets only).
  - DiaCLI uses the `toml` package (not tomllib); config_loader (task 16) should follow the same pattern.
- Feature 3 complete (tasks 7–10): layout.py, manifest_generator.py created; 31 tests passing.
  - Windows Path strips trailing slashes — folder trailing slash lives in manifest deploy_path string only.
  - add_asset() takes is_folder=True to append trailing slash to the manifest string (not the Path).
- Feature 2 complete (tasks 11–15): all 8 handlers + handlers/__init__.py; 21 tests passing.
  - Pattern mismatch emits OutputContext.warn(), never an AssetError.
  - StageHandler explicitly does not validate member assets (SD-CAT-012).
- Feature 4 complete (tasks 16–22): CLI commands + config loader + DiaPipeline stub wired; 19 tests passing.
  - DiaCLI uses `toml` package (not tomllib) — config_loader follows same pattern.
  - Shared _common_handler.py avoids duplication across build/validate/deploy handlers.
  - DiaPipeline asset_build_stage test updated: now expects exit 2 (no pipeline.toml in tmp) not 0.
  - 106 total tests passing across all 4 features.

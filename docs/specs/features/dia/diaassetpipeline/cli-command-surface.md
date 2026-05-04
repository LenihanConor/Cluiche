# Feature Spec: CLI Command Surface

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetPipeline | @docs/specs/systems/dia/diaassetpipeline.md |
| Feature | CLI Command Surface | (this document) |

## Summary

Three Click-based CLI commands — `dia asset build`, `dia asset validate`, and `dia asset deploy` — that provide the user-facing entry points to the DiaAssetPipeline. Each command reads target configuration from `pipeline.toml`, resolves the catalogue manifest path and target stage list, constructs a `BuildContext`, registers built-in handlers, and delegates to the build runner (Feature 1) with the appropriate phase selection. Commands follow DiaCLI's two-file plugin pattern (SD-CLI-001): a `*_cmd.py` file defining the Click command and a `*_handler.py` file containing the business logic.

## Problem

Without CLI commands, the asset pipeline has no entry point. Developers and CI systems need explicit commands to build, validate, or deploy assets for a specific target. The three commands map to common development workflows: full build (validate + transform + deploy), validation-only (check for errors without writing files), and deploy-only (copy already-processed assets). Each must read the correct target configuration from `pipeline.toml` and translate it into a `BuildContext` that the build runner understands.

## Acceptance Criteria

1. `dia asset build --target <name>` runs the full pipeline: validate, transform, deploy for the specified target
2. `dia asset validate --target <name>` runs validation only — no file writes, no transforms, no deploys
3. `dia asset deploy --target <name>` runs deploy only — skips validation and transform, copies assets directly
4. `--target` is required for all three commands
5. `--config` is optional, defaults to `Debug`. Accepts `Debug` or `Release`.
6. `--force` flag: when set, deletes the existing deploy directory before building (clean build)
7. All commands read `pipeline.toml` to resolve the target's `catalogue_manifest` path and `asset_stages` list
8. If `pipeline.toml` is missing or the target is not defined, exit with code 2 and a clear error message
9. If the catalogue manifest file referenced by `pipeline.toml` does not exist, exit with code 2
10. Exit code 0 on success, 1 on asset failures, 2 on invalid arguments or missing configuration (SD-CLI-008)
11. Each command follows the two-file plugin pattern: `*_cmd.py` (Click command) and `*_handler.py` (logic)
12. All commands produce NDJSON output via `OutputContext` to `Cluiche/out/<AppName>/logs/asset-pipeline/last-run.ndjson`
13. `--platform` is optional, defaults to `x64` (only supported value per PD-005)
14. Commands print a human-readable summary to stdout after completion (pass/fail counts, total duration)

## API Design

### CLI Commands

```bash
# Full pipeline: validate + transform + deploy
dia asset build --target cluichetest
dia asset build --target cluichetest --config Release
dia asset build --target cluichetest --config Debug --force

# Validate only: no file writes
dia asset validate --target cluichetest
dia asset validate --target cluichetest --config Release

# Deploy only: skip validate and transform
dia asset deploy --target cluichetest
dia asset deploy --target cluichetest --force
```

### Command Implementation (Two-File Pattern)

```python
# dia_cli/commands/asset/build_cmd.py

import click
from dia_cli.commands.asset.build_handler import handle_build

@click.command("build")
@click.option("--target", required=True, help="Target name from pipeline.toml")
@click.option("--config", default="Debug", type=click.Choice(["Debug", "Release"]))
@click.option("--platform", default="x64", type=click.Choice(["x64"]))
@click.option("--force", is_flag=True, default=False, help="Clean deploy directory before build")
@click.pass_context
def build(ctx, target, config, platform, force):
    """Run the full asset pipeline: validate, transform, deploy."""
    exit_code = handle_build(ctx, target, config, platform, force)
    ctx.exit(exit_code)
```

```python
# dia_cli/commands/asset/build_handler.py

import json
import sys
from pathlib import Path
from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.registry import AssetHandlerRegistry
from dia_cli.commands.asset.handlers import register_built_in_handlers
from dia_cli.commands.asset.runner import BuildRunner

def handle_build(ctx, target: str, config: str, platform: str, force: bool) -> int:
    """Execute the full asset pipeline for the given target."""
    # 1. Read pipeline.toml
    pipeline_config = load_pipeline_config()
    if pipeline_config is None:
        click.echo("Error: pipeline.toml not found", err=True)
        return 2

    # 2. Resolve target config
    target_config = pipeline_config.get("targets", {}).get(target)
    if target_config is None:
        click.echo(f"Error: target '{target}' not found in pipeline.toml", err=True)
        return 2

    # 3. Load catalogue manifest
    catalogue_path = Path(target_config["catalogue_manifest"])
    if not catalogue_path.exists():
        click.echo(f"Error: catalogue manifest not found: {catalogue_path}", err=True)
        return 2

    with open(catalogue_path) as f:
        catalogue = json.load(f)

    # 4. Build context
    deploy_root = Path(f"bin/{target_config.get('app_name', target)}/{config}/{platform}/assets")
    if force and deploy_root.exists():
        shutil.rmtree(deploy_root)

    context = BuildContext(
        catalogue=catalogue,
        config=config,
        platform=platform,
        app_name=target_config.get("app_name", target),
        deploy_root=deploy_root,
        asset_stages=target_config.get("asset_stages", []),
        output=ctx.obj.get("output_context"),
    )

    # 5. Register handlers and run
    registry = AssetHandlerRegistry()
    register_built_in_handlers(registry)

    runner = BuildRunner(registry, context)
    return runner.run(phases=["validate", "transform", "deploy"])
```

### pipeline.toml Target Configuration

```toml
[targets.cluichetest]
app_name = "CluicheTest"
catalogue_manifest = "Assets/CluicheTest/assets.catalogue.json"
asset_stages = ["stage.main_menu", "stage.level_1", "stage.level_2"]

[targets.googletests]
app_name = "GoogleTests"
catalogue_manifest = "Assets/GoogleTests/assets.catalogue.json"
asset_stages = []
```

### Exit Codes

| Code | Condition |
|------|-----------|
| 0 | All assets processed successfully |
| 1 | One or more assets failed validation, transform, or deploy |
| 2 | Invalid arguments, missing pipeline.toml, undefined target, or missing catalogue manifest |

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement `dia asset build` command | `build_cmd.py` (Click command with --target, --config, --platform, --force) and `build_handler.py` (load config, create context, run full pipeline) |
| 2 | Implement `dia asset validate` command | `validate_cmd.py` and `validate_handler.py` — runs `["validate"]` phases only, no file writes |
| 3 | Implement `dia asset deploy` command | `deploy_cmd.py` and `deploy_handler.py` — runs `["deploy"]` phases only |
| 4 | Implement pipeline.toml target config reading | Shared utility to load `pipeline.toml`, resolve target section, extract `catalogue_manifest` and `asset_stages` |
| 5 | Implement `dia asset` command group | Click group that parents `build`, `validate`, `deploy` subcommands, registered with DiaCLI plugin discovery |
| 6 | Tests | Unit tests for: target config loading (valid, missing target, missing pipeline.toml), exit code mapping, --force clean build, validate-only phase selection, deploy-only phase selection, human-readable summary output |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| Feature 1 — Handler Registry & Build Runner | AssetHandlerRegistry, BuildContext, BuildRunner |
| Feature 2 — Built-in Type Handlers | `register_built_in_handlers` for auto-registration |
| Feature 3 — Deploy Layout Engine | Invoked indirectly through handlers and the build runner |
| DiaCLI | Click framework, two-file plugin pattern (SD-CLI-001), OutputContext, plugin discovery |
| pipeline.toml | Target configuration: `catalogue_manifest`, `asset_stages`, `app_name` |
| Python stdlib | `json` (catalogue loading), `pathlib` (path resolution), `tomllib` / `tomli` (TOML parsing), `shutil` (--force clean) |

## Files

| File | Action |
|------|--------|
| `dia_cli/commands/asset/build_cmd.py` | Create — Click command for `dia asset build` |
| `dia_cli/commands/asset/build_handler.py` | Create — Business logic for build command |
| `dia_cli/commands/asset/validate_cmd.py` | Create — Click command for `dia asset validate` |
| `dia_cli/commands/asset/validate_handler.py` | Create — Business logic for validate command |
| `dia_cli/commands/asset/deploy_cmd.py` | Create — Click command for `dia asset deploy` |
| `dia_cli/commands/asset/deploy_handler.py` | Create — Business logic for deploy command |
| `dia_cli/commands/asset/config_loader.py` | Create — Shared pipeline.toml loading and target resolution |
| `dia_cli/commands/asset/group.py` | Create — `dia asset` Click group registration |
| `tests/test_cli_asset_commands.py` | Create — CLI command tests |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Asset IDs from the catalogue are `type.name` composite strings matching the StringCRC format. Target names in pipeline.toml are plain strings used for config lookup. |
| PD-005 | x64 Windows only | **Compliant.** `--platform` defaults to `x64` and only accepts `x64`. No cross-platform support. |
| PD-008 | Directory.Build.props owns OutDir | **Compliant.** Deploy root path (`bin/<App>/<Config>/<Platform>/assets/`) follows the `$(OutDir)` layout conventions from Directory.Build.props. |
| PD-009 | Generated output under Cluiche/out/ | **Compliant.** NDJSON logs written to `Cluiche/out/<AppName>/logs/asset-pipeline/last-run.ndjson` via OutputContext. Deployed assets go to `bin/` (binary output). |
| SD-CLI-001 | MDK CLI architecture — two-file plugin pattern | **Compliant.** Each command has a `*_cmd.py` (Click command definition) and `*_handler.py` (business logic). This is the core pattern this feature implements. |
| SD-CLI-002 | Python-based implementation | **Compliant.** All commands and handlers are Python. |
| SD-CLI-006 | Click framework | **Compliant.** All commands use Click decorators (`@click.command`, `@click.option`, `@click.group`, `@click.pass_context`). |
| SD-CLI-008 | Exit codes follow Unix conventions | **Compliant.** 0 = success, 1 = asset failures, 2 = invalid args or missing config. Matches the system spec exit code table. |
| SD-PIPE-002 | Stage order fixed: compile-code, build-assets, deploy | **Compliant.** `dia asset build` implements the `build-assets` stage. It does not interact with compile-code or the DiaPipeline deploy stage. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** Commands pass asset IDs through to the build runner unchanged. No ID manipulation. |
| SD-CAT-004 | Two manifests | **Compliant.** Commands read `assets.catalogue.json` from the path in pipeline.toml. The build runner generates `assets.runtime.json` via Feature 3. |
| SD-CAT-007 | Scope and tags drive deploy layout | **Compliant.** Commands do not override deploy layout. Layout is resolved by Feature 3 via the build runner. |
| SD-CAT-012 | Stage membership in RelationshipIndex | **Not directly applicable.** Commands pass the catalogue through to the build runner. Stage membership resolution happens in Features 1 and 3. |
| SD-CAT-013 | Folder assets deploy as directory trees | **Not directly applicable.** Folder handling is delegated to FolderHandler (Feature 2). |
| SD-APIPE-001 | Pipeline reads catalogue, generates runtime, never writes catalogue | **Compliant.** Commands open catalogue manifest as read-only via `json.load`. No writes to catalogue. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Config | Should pipeline.toml be located at a fixed path (repo root), or configurable via a CLI flag? | Fixed path at repo root (`pipeline.toml`). This matches DiaPipeline's existing convention. A `--pipeline-config` override could be added later but is not needed now. |
| 2 | TOML | Python 3.11+ has `tomllib` in stdlib. Earlier versions need `tomli`. Which should be used? | Use `tomllib` with a fallback import: `try: import tomllib; except ImportError: import tomli as tomllib`. DiaCLI already has this pattern for TOML parsing. |
| 3 | Validate | `dia asset validate` runs validation only with no file writes. Should it still read the full catalogue, or can it operate on a subset? | Full catalogue. Validation checks all assets (filtered by target stages). Subset validation is a future feature — the current scope is target-level validation. |
| 4 | Deploy | `dia asset deploy` skips validate and transform. What if assets haven't been transformed yet (no files to deploy)? | Deploy copies from source paths directly. Since the default transform is copy-as-is (SD-APIPE-003), most assets don't need a separate transform step. If a custom transform handler has not yet run, deploy copies the original source. This is documented behaviour — use `dia asset build` for the full pipeline. |
| 5 | Force | `--force` deletes the deploy directory before building. Should it also clear the NDJSON log? | No. NDJSON log files are managed by OutputContext and live in `Cluiche/out/`, not in the deploy directory. `--force` only affects the deploy tree in `bin/`. Log rotation is OutputContext's responsibility. |
| 6 | Groups | How is the `dia asset` command group discovered by DiaCLI? | Via DiaCLI's plugin discovery mechanism: `group.py` defines a Click group named `asset`, and DiaCLI's plugin loader discovers it from the `dia_cli/commands/asset/` package following the standard convention. |
| 7 | Summary | The spec says commands print a human-readable summary to stdout. What does this look like? | A single-line summary after the build completes: `Asset build complete: 42 passed, 2 failed, 44 total (3.2s)`. Errors are listed before the summary with asset ID and error message. This is written to stdout; the full event log goes to NDJSON. |

## Status

`Approved`

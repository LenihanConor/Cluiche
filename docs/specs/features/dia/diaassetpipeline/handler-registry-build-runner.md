# Feature Spec: Handler Registry & Build Runner

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetPipeline | @docs/specs/systems/dia/diaassetpipeline.md |
| Feature | Handler Registry & Build Runner | (this document) |

## Summary

Core infrastructure for the DiaAssetPipeline system: an `AssetHandlerRegistry` that maps asset type IDs to Python handler classes, a `BuildContext` carrying all per-build state, and a three-phase build runner that iterates all catalogue assets, filters by target stage membership, and executes validate, transform, deploy in sequence per asset. Errors are collected across all assets without early exit (SD-APIPE-002). Every significant event is emitted as an NDJSON line via DiaCLI's `OutputContext`.

This feature provides the skeleton that all other DiaAssetPipeline features plug into: type handlers register themselves here, the deploy layout engine is invoked from here, and the CLI commands delegate to the build runner.

## Problem

DiaPipeline's `build-assets` stage is currently a no-op stub (SD-PIPE-005). There is no infrastructure for dispatching per-type processing logic against the asset catalogue. Without a handler registry, every asset type would need bespoke orchestration code. Without a build context, each handler would need to independently resolve paths, read config, and manage output. Without a runner that collects errors across all assets, developers would face a fix-one-rerun cycle instead of seeing all problems at once.

## Acceptance Criteria

1. `AssetHandler` is a Python base class with `type_id: str`, `validate(record, context) -> list[AssetError]`, `transform(record, context) -> TransformResult`, and `deploy(record, context) -> DeployResult`
2. `AssetHandlerRegistry` supports `register(handler)` and `get(type_id) -> AssetHandler | None`
3. Registering a handler with a duplicate `type_id` raises a clear error
4. `BuildContext` carries: parsed catalogue dict, config (Debug/Release), platform (x64), app_name, deploy_root (Path), asset_stages (list from pipeline.toml), and an `OutputContext` for NDJSON logging
5. Build runner iterates all assets in the catalogue, filters out assets whose stage is not in the target's `asset_stages` list (global assets always included), and runs validate, transform, deploy per asset
6. All errors are collected — a failed asset does not prevent other assets from being processed (SD-APIPE-002)
7. If an asset's type has no registered handler, the runner uses a default copy-as-is path (SD-APIPE-003)
8. NDJSON events emitted: `OnAssetValidated`, `OnAssetTransformed`, `OnAssetDeployed`, `OnAssetFailed`, `OnBuildCompleted`
9. `OnBuildCompleted` includes pass count, fail count, and total duration in milliseconds
10. Build runner returns exit code 0 on full success, 1 if any asset failed
11. `TransformResult` and `DeployResult` are simple dataclasses carrying success/error status and optional output path
12. `AssetError` carries asset ID, phase (validate/transform/deploy), error message

## API Design

### Core Types

```python
# dia_cli/commands/asset/handler.py

from dataclasses import dataclass, field

@dataclass
class AssetError:
    asset_id: str          # type.name composite
    phase: str             # "validate" | "transform" | "deploy"
    message: str

@dataclass
class TransformResult:
    success: bool
    output_path: str | None = None
    errors: list[AssetError] = field(default_factory=list)

@dataclass
class DeployResult:
    success: bool
    deploy_path: str | None = None
    errors: list[AssetError] = field(default_factory=list)

class AssetHandler:
    type_id: str = ""

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        return []

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        return TransformResult(success=True)

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        return DeployResult(success=True)
```

### Registry

```python
# dia_cli/commands/asset/registry.py

class AssetHandlerRegistry:
    def __init__(self) -> None:
        self._handlers: dict[str, AssetHandler] = {}

    def register(self, handler: AssetHandler) -> None:
        if handler.type_id in self._handlers:
            raise ValueError(f"Duplicate handler for type '{handler.type_id}'")
        self._handlers[handler.type_id] = handler

    def get(self, type_id: str) -> AssetHandler | None:
        return self._handlers.get(type_id)
```

### Build Context

```python
# dia_cli/commands/asset/context.py

from pathlib import Path
from dataclasses import dataclass

@dataclass
class BuildContext:
    catalogue: dict            # parsed assets.catalogue.json
    config: str                # "Debug" or "Release"
    platform: str              # "x64"
    app_name: str              # e.g. "CluicheTest"
    deploy_root: Path          # bin/<App>/<Config>/<Platform>/assets/
    asset_stages: list[str]    # from pipeline.toml target config
    output: object             # OutputContext for NDJSON event logging
```

### Build Runner

```python
# dia_cli/commands/asset/runner.py

class BuildRunner:
    def __init__(self, registry: AssetHandlerRegistry, context: BuildContext) -> None: ...

    def run(self, phases: list[str] | None = None) -> int:
        """Run specified phases (default: all three). Returns exit code."""
        ...

    def _should_include_asset(self, record: dict) -> bool:
        """Filter by target stage membership. Global assets always included."""
        ...

    def _run_validate(self, record: dict, handler: AssetHandler) -> list[AssetError]: ...
    def _run_transform(self, record: dict, handler: AssetHandler) -> TransformResult: ...
    def _run_deploy(self, record: dict, handler: AssetHandler) -> DeployResult: ...
    def _emit_event(self, event_name: str, **data) -> None: ...
```

### Usage Pattern

```python
registry = AssetHandlerRegistry()
# Handlers auto-register at import (Feature 2)

context = BuildContext(
    catalogue=json.load(open(catalogue_path)),
    config="Debug",
    platform="x64",
    app_name="CluicheTest",
    deploy_root=Path("bin/CluicheTest/Debug/x64/assets"),
    asset_stages=["stage.main_menu", "stage.level_1"],
    output=output_context,
)

runner = BuildRunner(registry, context)
exit_code = runner.run()  # validate -> transform -> deploy
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement AssetError, TransformResult, DeployResult dataclasses | Core result types in `handler.py` |
| 2 | Implement AssetHandler base class | Base class with `type_id`, default no-op `validate`, `transform`, `deploy` methods |
| 3 | Implement AssetHandlerRegistry | `register` with duplicate rejection, `get` by type ID |
| 4 | Implement BuildContext dataclass | All fields needed by handlers and runner |
| 5 | Implement BuildRunner three-phase loop | Iterate catalogue assets, filter by stage membership, dispatch to handlers, collect errors |
| 6 | Implement NDJSON event emission | Emit `OnAssetValidated`, `OnAssetTransformed`, `OnAssetDeployed`, `OnAssetFailed`, `OnBuildCompleted` via OutputContext |
| 7 | Implement target stage filtering logic | `_should_include_asset`: include global-scoped assets always, include stage-scoped assets only if their stage is in `asset_stages` |
| 8 | Tests | Unit tests for: registry register/get/duplicate, build context creation, runner phase dispatch, error collection without early exit, stage filtering, NDJSON event output |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaCLI | OutputContext for NDJSON logging, Click framework, two-file plugin pattern |
| Python stdlib | `json` (catalogue parsing), `pathlib` (path resolution), `time` (duration tracking), `dataclasses` |
| DiaAssetCatalogue | `assets.catalogue.json` format — read via `json.load`, not C++ API |
| pipeline.toml | `catalogue_manifest` path and `asset_stages` list per target |

## Files

| File | Action |
|------|--------|
| `dia_cli/commands/asset/handler.py` | Create — AssetHandler base class, AssetError, TransformResult, DeployResult |
| `dia_cli/commands/asset/registry.py` | Create — AssetHandlerRegistry |
| `dia_cli/commands/asset/context.py` | Create — BuildContext dataclass |
| `dia_cli/commands/asset/runner.py` | Create — BuildRunner with three-phase loop and NDJSON events |
| `dia_cli/commands/asset/__init__.py` | Create — package init |
| `tests/test_asset_handler_registry.py` | Create — registry tests |
| `tests/test_build_runner.py` | Create — runner tests |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Asset IDs are `type.name` composite strings in Python, matching the StringCRC format used by C++ DiaAssetCatalogue. Handler dispatch keys on the type prefix. |
| PD-005 | x64 Windows only | **Compliant.** BuildContext defaults platform to `x64`. No cross-platform paths. |
| PD-008 | Directory.Build.props owns OutDir | **Compliant.** `deploy_root` is resolved from `bin/<App>/<Config>/<Platform>/assets/`, consistent with Directory.Build.props `$(OutDir)` conventions. |
| PD-009 | Generated output under Cluiche/out/ | **Compliant.** NDJSON event log written to `Cluiche/out/<AppName>/logs/` via OutputContext. Deployed assets go to `bin/` (binary output). |
| SD-CLI-001 | MDK CLI architecture | **Compliant.** Build runner is invoked by CLI commands (Feature 4) following the two-file plugin pattern. |
| SD-CLI-002 | Python-based implementation | **Compliant.** All code is Python. |
| SD-CLI-006 | Click framework | **Compliant.** CLI surface (Feature 4) uses Click. This feature provides the engine that Click commands delegate to. |
| SD-CLI-008 | Exit codes follow Unix conventions | **Compliant.** Runner returns 0 (success) or 1 (asset failures). Exit code 2 (invalid args) handled by CLI surface (Feature 4). |
| SD-PIPE-002 | Stage order fixed: compile-code, build-assets, deploy | **Compliant.** Build runner implements the `build-assets` stage logic. Does not reorder stages. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** All asset lookups use `type.name` format. Handler dispatch extracts type prefix from asset ID. |
| SD-CAT-004 | Two manifests | **Compliant.** Runner reads `assets.catalogue.json` (source). Runtime manifest generation is Feature 3's responsibility, invoked after deploy phase. |
| SD-CAT-007 | Scope and tags drive deploy layout | **Compliant.** Runner passes record scope and tags to deploy handler, which delegates to the deploy layout engine (Feature 3). |
| SD-CAT-012 | Stage membership in RelationshipIndex | **Compliant.** Stage filtering reads `contains` edges from the catalogue's RelationshipIndex to determine which assets belong to which stages. |
| SD-CAT-013 | Folder assets deploy as directory trees | **Compliant.** Runner dispatches folder-type assets to FolderHandler (Feature 2), which handles recursive directory copy. |
| SD-APIPE-001 | Pipeline reads catalogue, generates runtime, never writes catalogue | **Compliant.** Runner reads catalogue via `json.load`. Never modifies `assets.catalogue.json`. |
| SD-APIPE-002 | Collect all errors, no early exit | **Compliant.** Core design principle of the runner — all assets processed regardless of individual failures. |
| SD-APIPE-003 | Copy-as-is default transform | **Compliant.** Assets with no registered handler fall through to default copy-as-is behavior. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Runner | Should the runner process assets in catalogue order, or sort by type/stage for better locality? | Catalogue order. The catalogue manifest already groups assets logically. Sorting adds complexity with no clear benefit at current scale. Revisit if profiling shows I/O thrashing from random access patterns. |
| 2 | Errors | If a handler's `validate` returns errors, should the runner still call `transform` and `deploy` for that asset? | No. If validate fails, transform and deploy are skipped for that asset. The asset is marked failed and `OnAssetFailed` is emitted. Other assets continue normally. |
| 3 | Registry | Should the registry support unregistering handlers (e.g. for testing)? | No. Registration is one-way at startup. Tests construct fresh registry instances. Adding unregister adds complexity for no production use case. |
| 4 | Context | Should BuildContext be immutable after construction, or can handlers modify it? | Immutable by convention. BuildContext is a dataclass passed by reference, but handlers should treat it as read-only. No enforcement beyond documentation — Python's culture is "we're all consenting adults." |
| 5 | Filtering | How does the runner determine an asset's stage membership? The catalogue's RelationshipIndex has `contains` edges from stages to assets. | The runner pre-processes the RelationshipIndex at startup: builds a reverse map of asset ID to list of containing stage IDs. An asset is included if any of its containing stages is in `asset_stages`, or if it has `scope = global`. Assets contained by zero stages are included with a warning (orphaned asset fallback per system spec AI Q7). |
| 6 | Events | Should NDJSON events include a timestamp, or just duration? | Both. Each event includes an ISO 8601 timestamp and, where applicable, a duration in milliseconds. Timestamps enable log correlation across pipeline stages; durations enable per-asset performance analysis. |
| 7 | Phases | The `run` method accepts a `phases` parameter. What values does it accept? | A list of strings from `["validate", "transform", "deploy"]`. Default is all three in order. `dia asset validate` passes `["validate"]`. `dia asset deploy` passes `["deploy"]`. Invalid phase names raise ValueError. |

## Status

`Approved`

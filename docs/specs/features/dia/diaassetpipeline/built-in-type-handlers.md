# Feature Spec: Built-in Type Handlers

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetPipeline | @docs/specs/systems/dia/diaassetpipeline.md |
| Feature | Built-in Type Handlers | (this document) |

## Summary

Default Python handler implementations for all 8 taxonomy asset types: Texture, Sprite (Mesh/Sprite), Audio, Config, Entity Definition, Stage, UI Definition, and Folder. Each handler extends `AssetHandler` (Feature 1) and provides validate, transform, and deploy methods. Most handlers share a common default implementation: validate checks that the source file exists and matches the expected file pattern, transform is copy-as-is (SD-APIPE-003), and deploy delegates to the deploy layout engine (Feature 3). Two handlers have specialised behaviour: `FolderHandler` deploys an entire directory tree recursively (SD-CAT-013), and `StageHandler` validates that the stage JSON contains a required `name` field.

All handlers are auto-registered with `AssetHandlerRegistry` at import time, consistent with DiaCLI's plugin discovery pattern.

## Problem

Without concrete handler implementations, the build runner (Feature 1) has no type-specific logic to dispatch to. The pipeline would either skip all assets or require callers to manually register handlers before every build. Shipping default handlers for all 8 settled taxonomy types ensures the pipeline works out of the box for any catalogue-compliant project. The default validate/copy-as-is/deploy behaviour covers the majority of asset types, while Folder and Stage have known special requirements that must be handled at launch.

## Acceptance Criteria

1. All 8 taxonomy types have a registered handler: `texture`, `sprite`, `audio`, `config`, `entity`, `stage`, `ui`, `folder`
2. Default `validate` checks: source file/directory exists at the path specified in the asset record, file name matches the expected pattern for the type
3. Default `transform`: copy-as-is, no file modification (SD-APIPE-003)
4. Default `deploy`: delegates to the deploy layout engine (Feature 3) to resolve the output path from scope and tags, then copies the file
5. `FolderHandler.validate` checks that the source path is an existing directory
6. `FolderHandler.transform` is a no-op (directories are not transformed)
7. `FolderHandler.deploy` copies the entire directory tree recursively, preserving internal structure (SD-CAT-013)
8. `StageHandler.validate` additionally checks that `stage.json` contains a `name` field (required structural field)
9. `StageHandler` does NOT validate member assets — stage membership is defined by `contains` relationships in the RelationshipIndex (SD-CAT-012), not by fields in the stage file
10. All handlers are auto-registered when `dia_cli/commands/asset/handlers/__init__.py` is imported
11. Handlers can be subclassed by game-specific code to override validate/transform/deploy for custom behaviour
12. Each handler's `type_id` matches the corresponding DiaAssetCatalogue type ID (e.g. `"texture"`, `"folder"`)

## API Design

### Default Handler Base

```python
# dia_cli/commands/asset/handlers/default.py

from dia_cli.commands.asset.handler import AssetHandler, AssetError, TransformResult, DeployResult

class DefaultAssetHandler(AssetHandler):
    """Base implementation with standard validate/transform/deploy logic."""
    file_pattern: str = ""  # e.g. "*.texture.png"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        errors = []
        source_path = Path(record.get("source_path", ""))
        if not source_path.exists():
            errors.append(AssetError(
                asset_id=record["id"],
                phase="validate",
                message=f"Source file not found: {source_path}",
            ))
        if self.file_pattern and not source_path.match(self.file_pattern):
            errors.append(AssetError(
                asset_id=record["id"],
                phase="validate",
                message=f"File does not match pattern '{self.file_pattern}': {source_path.name}",
            ))
        return errors

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        # Copy-as-is: no transformation (SD-APIPE-003)
        return TransformResult(success=True, output_path=record.get("source_path"))

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        # Delegate to deploy layout engine (Feature 3)
        from dia_cli.commands.asset.layout import resolve_deploy_path, copy_asset
        deploy_path = resolve_deploy_path(record, context)
        copy_asset(record.get("source_path"), deploy_path)
        return DeployResult(success=True, deploy_path=str(deploy_path))
```

### Type-Specific Handlers

```python
# dia_cli/commands/asset/handlers/texture.py
class TextureHandler(DefaultAssetHandler):
    type_id = "texture"
    file_pattern = "*.texture.png"

# dia_cli/commands/asset/handlers/sprite.py
class SpriteHandler(DefaultAssetHandler):
    type_id = "sprite"
    file_pattern = "*.sprite.json"

# dia_cli/commands/asset/handlers/audio.py
class AudioHandler(DefaultAssetHandler):
    type_id = "audio"
    file_pattern = "*.audio.wav"

# dia_cli/commands/asset/handlers/config.py
class ConfigHandler(DefaultAssetHandler):
    type_id = "config"
    file_pattern = "*.config.json"

# dia_cli/commands/asset/handlers/entity.py
class EntityHandler(DefaultAssetHandler):
    type_id = "entity"
    file_pattern = "*.entity.json"

# dia_cli/commands/asset/handlers/ui.py
class UIHandler(DefaultAssetHandler):
    type_id = "ui"
    file_pattern = "*.ui.json"
```

### Folder Handler

```python
# dia_cli/commands/asset/handlers/folder.py

class FolderHandler(AssetHandler):
    type_id = "folder"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        errors = []
        source_path = Path(record.get("source_path", ""))
        if not source_path.is_dir():
            errors.append(AssetError(
                asset_id=record["id"],
                phase="validate",
                message=f"Folder asset source is not a directory: {source_path}",
            ))
        return errors

    def transform(self, record: dict, context: "BuildContext") -> TransformResult:
        # No-op: directories are not transformed
        return TransformResult(success=True, output_path=record.get("source_path"))

    def deploy(self, record: dict, context: "BuildContext") -> DeployResult:
        from dia_cli.commands.asset.layout import resolve_deploy_path
        deploy_path = resolve_deploy_path(record, context)
        # Recursive directory copy preserving internal structure (SD-CAT-013)
        shutil.copytree(record["source_path"], deploy_path, dirs_exist_ok=True)
        return DeployResult(success=True, deploy_path=str(deploy_path))
```

### Stage Handler

```python
# dia_cli/commands/asset/handlers/stage.py

class StageHandler(DefaultAssetHandler):
    type_id = "stage"
    file_pattern = "*.stage.json"

    def validate(self, record: dict, context: "BuildContext") -> list[AssetError]:
        errors = super().validate(record, context)
        # Validate required 'name' field in stage JSON
        source_path = Path(record.get("source_path", ""))
        if source_path.exists():
            with open(source_path) as f:
                stage_data = json.load(f)
            if "name" not in stage_data:
                errors.append(AssetError(
                    asset_id=record["id"],
                    phase="validate",
                    message="Stage JSON missing required 'name' field",
                ))
        return errors
```

### Auto-Registration

```python
# dia_cli/commands/asset/handlers/__init__.py

from dia_cli.commands.asset.registry import AssetHandlerRegistry
from dia_cli.commands.asset.handlers.texture import TextureHandler
from dia_cli.commands.asset.handlers.sprite import SpriteHandler
from dia_cli.commands.asset.handlers.audio import AudioHandler
from dia_cli.commands.asset.handlers.config import ConfigHandler
from dia_cli.commands.asset.handlers.entity import EntityHandler
from dia_cli.commands.asset.handlers.stage import StageHandler
from dia_cli.commands.asset.handlers.ui import UIHandler
from dia_cli.commands.asset.handlers.folder import FolderHandler

def register_built_in_handlers(registry: AssetHandlerRegistry) -> None:
    """Register all 8 built-in type handlers."""
    for handler_cls in [
        TextureHandler, SpriteHandler, AudioHandler, ConfigHandler,
        EntityHandler, StageHandler, UIHandler, FolderHandler,
    ]:
        registry.register(handler_cls())
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement DefaultAssetHandler base class | Shared validate (source exists, pattern match), copy-as-is transform, deploy via layout engine |
| 2 | Implement FolderHandler | Validate checks directory exists, transform is no-op, deploy copies entire directory tree recursively (SD-CAT-013) |
| 3 | Implement StageHandler | Extends DefaultAssetHandler, adds `name` field validation on stage JSON. Does NOT validate member assets (SD-CAT-012). |
| 4 | Implement remaining 6 handlers | TextureHandler, SpriteHandler, AudioHandler, ConfigHandler, EntityHandler, UIHandler — thin subclasses of DefaultAssetHandler with correct `type_id` and `file_pattern` |
| 5 | Implement auto-registration | `register_built_in_handlers` function called at pipeline startup |
| 6 | Tests | Unit tests for: default validate (source missing, pattern mismatch), FolderHandler (non-directory source, recursive copy), StageHandler (missing name), auto-registration of all 8 types, handler subclassing override |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| Feature 1 — Handler Registry & Build Runner | AssetHandler base class, AssetError, TransformResult, DeployResult, AssetHandlerRegistry, BuildContext |
| Feature 3 — Deploy Layout Engine | `resolve_deploy_path` for scope/tag-based output path resolution, `copy_asset` for single-file copy |
| Python stdlib | `pathlib` (path checks), `shutil` (directory copy), `json` (stage JSON parsing) |

## Files

| File | Action |
|------|--------|
| `dia_cli/commands/asset/handlers/__init__.py` | Create — auto-registration function, imports all handlers |
| `dia_cli/commands/asset/handlers/default.py` | Create — DefaultAssetHandler base with shared validate/transform/deploy |
| `dia_cli/commands/asset/handlers/folder.py` | Create — FolderHandler with recursive directory deploy |
| `dia_cli/commands/asset/handlers/stage.py` | Create — StageHandler with name field validation |
| `dia_cli/commands/asset/handlers/texture.py` | Create — TextureHandler |
| `dia_cli/commands/asset/handlers/sprite.py` | Create — SpriteHandler |
| `dia_cli/commands/asset/handlers/audio.py` | Create — AudioHandler |
| `dia_cli/commands/asset/handlers/config.py` | Create — ConfigHandler |
| `dia_cli/commands/asset/handlers/entity.py` | Create — EntityHandler |
| `dia_cli/commands/asset/handlers/ui.py` | Create — UIHandler |
| `tests/test_built_in_handlers.py` | Create — tests for all handlers |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Handler `type_id` strings match the DiaAssetCatalogue StringCRC type IDs (e.g. `"texture"`, `"stage"`). Asset IDs in records are `type.name` composites. |
| PD-005 | x64 Windows only | **Compliant.** All file paths use Windows conventions. No cross-platform considerations. |
| PD-008 | Directory.Build.props owns OutDir | **Compliant.** Deploy paths resolved via BuildContext's `deploy_root`, which follows `$(OutDir)` conventions. |
| PD-009 | Generated output under Cluiche/out/ | **Compliant.** Handlers emit NDJSON events via OutputContext (logs to `Cluiche/out/`). Deployed assets go to `bin/`. |
| SD-CLI-001 | MDK CLI architecture | **Compliant.** Handlers are internal implementation within the `dia_cli/commands/asset/` package. |
| SD-CLI-002 | Python-based implementation | **Compliant.** All handlers are pure Python. No C++ compilation required to add or modify handlers (SD-APIPE-006). |
| SD-CLI-006 | Click framework | **Not directly applicable.** Handlers are not Click commands. Click is used by the CLI surface (Feature 4). |
| SD-CLI-008 | Exit codes follow Unix conventions | **Not directly applicable.** Handlers return result objects. Exit code mapping is done by the build runner (Feature 1). |
| SD-PIPE-002 | Stage order fixed | **Compliant.** Handlers are invoked by the build runner during the `build-assets` stage. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** Handlers receive full `type.name` IDs from asset records. Handler dispatch uses the type prefix. |
| SD-CAT-004 | Two manifests | **Compliant.** Handlers read source paths from the catalogue manifest. They deploy to locations that the runtime manifest will reference. |
| SD-CAT-007 | Scope and tags drive deploy layout | **Compliant.** All handlers delegate deploy path resolution to the layout engine (Feature 3), which applies scope and tag rules. |
| SD-CAT-012 | Stage membership in RelationshipIndex | **Compliant.** StageHandler explicitly does NOT validate member assets. Stage membership is defined by `contains` edges in RelationshipIndex, not by stage file fields. |
| SD-CAT-013 | Folder assets deploy as directory trees | **Compliant.** FolderHandler uses `shutil.copytree` with `dirs_exist_ok=True` to recursively copy the entire directory, preserving internal structure. |
| SD-APIPE-001 | Pipeline reads catalogue, generates runtime, never writes catalogue | **Compliant.** Handlers read asset records from the parsed catalogue dict. No handler writes to `assets.catalogue.json`. |
| SD-APIPE-002 | Collect all errors, no early exit | **Compliant.** Each handler returns errors as a list. The build runner (Feature 1) collects errors across all assets. |
| SD-APIPE-003 | Copy-as-is default transform | **Compliant.** DefaultAssetHandler's `transform` returns the source path unchanged. All 6 standard type handlers inherit this behaviour. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Design | Should DefaultAssetHandler be an abstract base class, or a concrete class that handlers inherit from? | Concrete class. It provides working default behaviour (validate, copy-as-is transform, deploy). Type-specific handlers override only what they need. This is the Strategy pattern with sensible defaults. |
| 2 | Folder | `shutil.copytree` with `dirs_exist_ok=True` requires Python 3.8+. Is that acceptable? | Yes. DiaCLI already requires Python 3.10+ (for `match` statements and type union syntax `X | Y`). Python 3.8+ is well within the baseline. |
| 3 | Stage | Should StageHandler validate that the `name` field value is non-empty, or just that the key exists? | Both. The `name` key must exist AND be a non-empty string. A stage with `"name": ""` is structurally invalid. |
| 4 | Patterns | File patterns like `*.texture.png` are matched against the source file name. What about assets whose source path doesn't follow the convention? | Emit a validation warning (not an error) via NDJSON log. Pattern matching is a best-effort check for convention enforcement, not a hard gate. The asset is still processed. This should be configurable in a future strict-mode feature. |
| 5 | Registration | What happens if a game project wants to replace a built-in handler (e.g. custom TextureHandler with compression)? | The game project registers its handler after `register_built_in_handlers`. Since duplicate type IDs are rejected, the game code must either: (a) subclass the built-in handler and override methods, keeping the same `type_id`, or (b) the registry should support a `register(handler, replace=True)` overload. Option (a) is preferred — it preserves validation logic while adding transform steps. |
| 6 | Deploy | All handlers delegate deploy to the layout engine (Feature 3). Could the deploy logic live entirely in the build runner, removing it from handlers? | No. FolderHandler's deploy is fundamentally different (recursive directory copy vs single file copy). Keeping deploy in the handler allows type-specific deploy strategies. The layout engine resolves the path; the handler performs the copy. |

## Status

`Approved`

# Feature Spec: Deploy Layout Engine

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetPipeline | @docs/specs/systems/dia/diaassetpipeline.md |
| Feature | Deploy Layout Engine | (this document) |

## Summary

The deploy layout engine resolves each asset's output path from its scope (`global`/`stage`) and category tag, creates the necessary directory structure under `bin/<App>/<Config>/<Platform>/assets/`, copies processed assets to their final locations, and generates `assets.runtime.json` — the lean runtime manifest containing only asset IDs, scopes, deploy-relative paths, and stage membership arrays. The runtime manifest is consumed by DiaAssetRuntime's `RuntimeManifestLoader` at game startup; it contains no source paths, content hashes, tags, or type metadata.

This feature owns the "where does each asset end up" question and the "what does the game read at runtime" output.

## Problem

Without a layout engine, each handler would need to independently compute deploy paths, leading to inconsistent directory structures and duplicated path resolution logic. The deploy layout rules (scope-based top-level directory, tag-based subdirectory) are defined once in the DiaAssetCatalogue system spec (SD-CAT-007) but must be enforced consistently across all asset types. Additionally, the game needs a runtime manifest that maps asset IDs to their deployed paths — this is a distinct file from the source catalogue manifest and must be generated during the build.

## Acceptance Criteria

1. `resolve_deploy_path(record, context)` returns the full output path for an asset based on its scope and first matching category tag
2. Scope resolution: `scope = "global"` maps to `<deploy_root>/global/<category>/`, `scope = "stage"` maps to `<deploy_root>/stages/<stage_name>/<category>/`
3. Category resolution from tags: first matching tag wins from ordered list: `Presentation/UI`, `Presentation`, `characters`, `environments`, `gameplay`, `misc`. Assets with no matching tag map to `misc/`.
4. Folder asset deploy paths end with a trailing `/` (the path is a directory, not a file)
5. `create_deploy_directories(context)` creates the full directory tree under `deploy_root` as needed
6. `copy_asset(source, dest)` copies a single file to the deploy location, overwriting if it exists
7. `copy_asset_directory(source, dest)` copies an entire directory tree recursively for folder assets (SD-CAT-013), preserving internal structure
8. Runtime manifest is generated at `<deploy_root>/assets.runtime.json` after all assets are deployed
9. Runtime manifest `assets` array entries contain: `id` (string), `scope` (string), `deploy_path` (string, relative to deploy root)
10. Runtime manifest `stages` array entries contain: `id` (string), `assets` (array of asset ID strings) — built from `contains` edges in the catalogue's RelationshipIndex
11. Global assets referenced by a stage appear in that stage's `assets` array in the runtime manifest
12. Runtime manifest contains no source paths, content hashes, tags, or type metadata (SD-APIPE-001)

## API Design

### Path Resolution

```python
# dia_cli/commands/asset/layout.py

from pathlib import Path

# Ordered category resolution — first matching tag wins
CATEGORY_TAGS = [
    "Presentation/UI",
    "Presentation",
    "characters",
    "environments",
    "gameplay",
    "misc",
]

def resolve_deploy_path(record: dict, context: "BuildContext") -> Path:
    """Resolve full output path from scope and category tag.

    Scope rule:
        global -> <deploy_root>/global/<category>/
        stage  -> <deploy_root>/stages/<stage_name>/<category>/

    Category rule:
        First matching tag from CATEGORY_TAGS. Defaults to 'misc'.
    """
    ...

def resolve_category(tags: list[str]) -> str:
    """Return the first matching category from tags, or 'misc'."""
    for tag in tags:
        if tag in CATEGORY_TAGS:
            return tag
    return "misc"

def create_deploy_directories(deploy_root: Path) -> None:
    """Create the full deploy directory tree under deploy_root."""
    ...

def copy_asset(source: str | Path, dest: Path) -> None:
    """Copy a single file to dest, creating parent directories. Overwrites existing."""
    ...

def copy_asset_directory(source: str | Path, dest: Path) -> None:
    """Recursively copy a directory tree to dest, preserving structure (SD-CAT-013)."""
    ...
```

### Runtime Manifest Generator

```python
# dia_cli/commands/asset/manifest_generator.py

from pathlib import Path

@dataclass
class RuntimeAssetEntry:
    id: str               # type.name composite
    scope: str            # "global" or "stage"
    deploy_path: str      # relative to deploy_root

@dataclass
class RuntimeStageEntry:
    id: str               # stage asset ID
    assets: list[str]     # asset IDs contained by this stage

class RuntimeManifestGenerator:
    def __init__(self, context: "BuildContext") -> None:
        self._context = context
        self._assets: list[RuntimeAssetEntry] = []
        self._stages: list[RuntimeStageEntry] = []

    def add_asset(self, asset_id: str, scope: str, deploy_path: Path) -> None:
        """Record a deployed asset for manifest generation."""
        ...

    def build_stages(self) -> None:
        """Build stage entries from catalogue RelationshipIndex contains edges."""
        ...

    def generate(self) -> Path:
        """Write assets.runtime.json to deploy_root. Returns the manifest path."""
        ...
```

### Runtime Manifest Format

```json
{
    "version": 1,
    "app_name": "CluicheTest",
    "assets": [
        {
            "id": "texture.hero_sprite",
            "scope": "global",
            "deploy_path": "global/characters/hero_sprite.texture.png"
        },
        {
            "id": "config.main_menu_layout",
            "scope": "stage",
            "deploy_path": "stages/main_menu/Presentation/UI/main_menu_layout.config.json"
        }
    ],
    "stages": [
        {
            "id": "stage.main_menu",
            "assets": [
                "texture.hero_sprite",
                "config.main_menu_layout"
            ]
        }
    ]
}
```

### Usage Pattern

```python
# Inside build runner (Feature 1), after deploy phase:
generator = RuntimeManifestGenerator(context)

for record in deployed_assets:
    deploy_path = resolve_deploy_path(record, context)
    generator.add_asset(record["id"], record["scope"], deploy_path)

generator.build_stages()
manifest_path = generator.generate()
# -> bin/CluicheTest/Debug/x64/assets/assets.runtime.json
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement `resolve_deploy_path` | Scope + tag-based path resolution. Global assets to `global/<category>/`, stage assets to `stages/<stage_name>/<category>/`. Default to `misc/` for untagged assets. |
| 2 | Implement `resolve_category` | Ordered tag matching against `CATEGORY_TAGS`. First match wins, fallback to `misc`. |
| 3 | Implement `create_deploy_directories` | Create full directory tree under deploy root using `Path.mkdir(parents=True, exist_ok=True)`. |
| 4 | Implement `copy_asset` | Single file copy with `shutil.copy2`. Create parent directories if needed. Overwrite existing files. |
| 5 | Implement `copy_asset_directory` | Recursive directory copy with `shutil.copytree(dirs_exist_ok=True)` for folder assets (SD-CAT-013). |
| 6 | Implement `RuntimeManifestGenerator` | `add_asset` records deployed assets. `build_stages` reads `contains` edges from catalogue RelationshipIndex. `generate` writes `assets.runtime.json`. |
| 7 | Tests | Unit tests for: path resolution (global/stage, each category tag, untagged), folder path trailing slash, category resolution order, manifest generation (assets array, stages array, contains edges, global assets in stage lists), copy operations |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| Feature 1 — Handler Registry & Build Runner | BuildContext (deploy_root, catalogue, app_name), invoked after deploy phase |
| DiaAssetCatalogue | Catalogue manifest format — `assets` array (scope, tags, source_path), `relationship_index` (contains edges). Read via `json.load`. |
| Python stdlib | `pathlib` (path manipulation), `shutil` (file/directory copy), `json` (manifest write) |

## Files

| File | Action |
|------|--------|
| `dia_cli/commands/asset/layout.py` | Create — path resolution, category mapping, file/directory copy utilities |
| `dia_cli/commands/asset/manifest_generator.py` | Create — RuntimeManifestGenerator, runtime manifest JSON output |
| `tests/test_deploy_layout.py` | Create — path resolution and copy tests |
| `tests/test_manifest_generator.py` | Create — runtime manifest generation tests |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Asset IDs in the runtime manifest are `type.name` composite strings matching the StringCRC format. Stage IDs and asset references use the same format. |
| PD-005 | x64 Windows only | **Compliant.** All paths use Windows conventions. `deploy_root` resolves to a Windows path under `bin/`. |
| PD-008 | Directory.Build.props owns OutDir | **Compliant.** `deploy_root` (`bin/<App>/<Config>/<Platform>/assets/`) follows the `$(OutDir)` layout defined by Directory.Build.props. |
| PD-009 | Generated output under Cluiche/out/ | **Compliant.** Runtime manifest is deployed to `bin/` (binary output, not generated output). NDJSON logs go to `Cluiche/out/` via OutputContext. |
| SD-CLI-001 | MDK CLI architecture | **Compliant.** Layout engine is an internal module within `dia_cli/commands/asset/`, consumed by handlers and the build runner. |
| SD-CLI-002 | Python-based implementation | **Compliant.** All code is Python. |
| SD-CLI-006 | Click framework | **Not directly applicable.** Layout engine is not a Click command. |
| SD-CLI-008 | Exit codes follow Unix conventions | **Not directly applicable.** Layout engine does not set exit codes. Errors propagate via return values to the build runner. |
| SD-PIPE-002 | Stage order fixed | **Compliant.** Layout engine runs within the `build-assets` stage. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** All asset and stage IDs in the runtime manifest use `type.name` format. |
| SD-CAT-004 | Two manifests | **Compliant.** This feature generates the runtime manifest (`assets.runtime.json`) as distinct from the catalogue manifest (`assets.catalogue.json`). Runtime manifest contains only deploy paths, scopes, and stage membership — no source paths, content hashes, or build-time metadata. |
| SD-CAT-007 | Scope and tags drive deploy layout | **Compliant.** This is the feature that implements scope and tag-based layout resolution. Global/stage scope determines top-level directory. Category tag determines subdirectory. |
| SD-CAT-012 | Stage membership in RelationshipIndex | **Compliant.** `RuntimeManifestGenerator.build_stages` reads `contains` edges from the catalogue's RelationshipIndex to build stage-to-asset membership arrays. |
| SD-CAT-013 | Folder assets deploy as directory trees | **Compliant.** `copy_asset_directory` recursively copies folder assets. Folder deploy paths end with trailing `/`. |
| SD-APIPE-001 | Pipeline reads catalogue, generates runtime, never writes catalogue | **Compliant.** Layout engine reads catalogue data from BuildContext. Generates `assets.runtime.json`. Never modifies `assets.catalogue.json`. |
| SD-APIPE-004 | Deploy layout driven by scope and tags | **Compliant.** This is the core implementation of SD-APIPE-004. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Categories | The category list is fixed at 6 entries. Should it be configurable via pipeline.toml? | No, not for now. The 6 categories mirror the DiaAssetCatalogue tag taxonomy. Making it configurable adds complexity for a scenario that hasn't arisen. If a project needs custom categories, they can extend `CATEGORY_TAGS` in a fork or we add pipeline.toml support later. |
| 2 | Manifest | Should the runtime manifest include a `version` field for format evolution? | Yes. The manifest includes `"version": 1`. DiaAssetRuntime's `RuntimeManifestLoader` checks this field and can reject unsupported versions. This costs nothing now and prevents breaking changes later. |
| 3 | Stages | A global asset may be referenced by multiple stages. Should it appear in every stage's `assets` array, or only once at the top level? | Every stage that references it (via `contains` edges) includes it in its `assets` array. This makes stage loading self-contained — loading stage X gives the complete list of assets X needs, without requiring the loader to separately resolve globals. Duplication in the manifest is acceptable for a lean JSON file. |
| 4 | Paths | Should `deploy_path` in the runtime manifest be relative to the deploy root or absolute? | Relative to deploy root. The runtime resolves the full path by joining the binary directory with the relative deploy path. This makes the manifest portable across machines (e.g. build server vs developer workstation). |
| 5 | Overwrites | `copy_asset` overwrites existing files unconditionally. Should it check timestamps or content hashes? | No. Full overwrite, consistent with SD-APIPE-005 (incremental builds deferred). The pipeline always does a full pass. Hash-based skip logic is a future optimization. |
| 6 | Errors | What happens if `copy_asset` fails (e.g. permission denied, disk full)? | The function raises an exception. The calling handler catches it and returns a `DeployResult` with `success=False` and the error. The build runner (Feature 1) records the failure via `OnAssetFailed` and continues with other assets (SD-APIPE-002). |
| 7 | Folders | For folder assets, should the deploy path in the runtime manifest include the trailing `/`? | Yes. Trailing `/` signals to DiaAssetRuntime that this is a directory, not a file. The runtime's asset loader can then use directory-based loading logic. |

## Status

`Approved`

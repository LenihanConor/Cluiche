# System Spec: DiaAssetPipeline

## Parent Application
@docs/specs/applications/dia.md

## Asset System Context
@docs/specs/systems/dia/asset-system-overview.md

## Summary

DiaAssetPipeline is the build-time asset processing system. It reads `assets.catalogue.json` (the source-side catalogue manifest in `Assets/<AppName>/`) via Python `json.load`, runs per-type handlers (validate, transform, deploy) against each asset record, copies processed assets to the correct location in `bin/<App>/<Config>/<Platform>/assets/`, and generates `assets.runtime.json` (the deployed manifest with deploy-relative paths, stripped of build-time fields like content hashes and source paths). It is invoked by DiaPipeline's `build-assets` stage.

DiaAssetPipeline answers: "given the catalogue of what assets exist, how do I process and deploy each one to make the game runnable?"

## Responsibilities

**Owns:**
- Per-type handler registry — each registered asset type can provide a Python handler for validate, transform, and deploy
- Validation pass — runs each asset through its type handler's validation logic; collects all errors before reporting (no early exit)
- Transform pass — invokes type-specific transform logic (e.g. JSON config → binary, texture compression). Copy-as-is is the default transform for types with no registered transform
- Deploy pass — copies processed assets from source paths to `bin/<App>/<Config>/<Platform>/assets/` using scope and tag rules from AssetRecord
- Runtime manifest generation — transforms `assets.catalogue.json` into `assets.runtime.json` (a distinct, lean format: asset ID → deploy-relative path, scope, and stage membership — no source paths, content hashes, tags, or type metadata). Deploys it at `bin/<App>/<Config>/<Platform>/assets/assets.runtime.json`. DiaAssetRuntime deserializes this with its own `RuntimeManifestLoader` — no DiaAssetCatalogue dependency at runtime.
- Deploy layout enforcement — `global/` for shared assets (referenced by multiple Stages), `stages/<StageName>/` for Stage-scoped assets; subdirectory within each driven by AssetRecord tags
- NDJSON event logging for all pipeline events (per-asset results, stage timings, error summaries)
- `dia asset build` CLI command surface

**Does NOT own:**
- The catalogue manifest (`assets.catalogue.json`) — authored by DiaAssetCatalogueEditor via DiaAssetCatalogue
- Pipeline orchestration (compile-code, deploy stage) — owned by DiaPipeline
- Incremental builds (content hash comparison to skip unchanged assets) — future feature
- Asset authoring or content creation tools
- Binary format design for transformed assets

## Deploy Layout

Assets are deployed to:

```
bin/<App>/<Config>/<Platform>/assets/
    ├── global/                        # Assets referenced by >1 Stage
    │   ├── Presentation/
    │   │   └── UI/                    # tag: Presentation/UI
    │   ├── characters/                # tag: characters
    │   ├── environments/              # tag: environments
    │   ├── gameplay/                  # tag: gameplay
    │   └── misc/                      # tag: misc (or untagged)
    └── stages/
        └── <StageName>/               # Assets referenced by exactly 1 Stage
            ├── Presentation/
            │   └── UI/
            ├── characters/
            ├── environments/
            ├── gameplay/
            └── misc/
```

**Scope rule:** An asset with `scope = global` deploys under `global/`; `scope = stage` deploys under `stages/<StageName>/`. Scope is computed by DiaAssetCatalogue during manifest authoring based on how many Stages reference the asset.

**Category rule:** The deploy subdirectory within the scope is determined by the asset's tags. First matching tag wins: `Presentation/UI`, `Presentation`, `characters`, `environments`, `gameplay`, `misc`. Assets with no matching category tag go to `misc/`.

**Target filtering rule:** Each target in `pipeline.toml` declares `asset_stages` — the list of Stage asset IDs it uses. The pipeline deploys only: (a) assets scoped to those Stages, and (b) all global-scoped assets. Assets scoped to Stages not in the target's list are skipped. An empty `asset_stages` list means no game assets are deployed.

## Public Interfaces

### CLI Commands

```bash
# Run the full asset pipeline (validate + transform + deploy) for a target
dia asset build --target cluichetest
dia asset build --target cluichetest --config Debug
dia asset build --target cluichetest --config Release

# Validate only — no file writes
dia asset validate --target cluichetest

# Deploy only — skip transform, copy existing processed assets
dia asset deploy --target cluichetest
```

### Handler Registration API

```python
# dia_cli/commands/asset/handler_registry.py

class AssetHandler:
    """Base class for per-type asset handlers."""
    type_id: str                        # matches AssetTypeDescriptor type ID, e.g. "texture"

    def validate(self, record: AssetRecord, context: BuildContext) -> list[AssetError]:
        """Return list of errors. Empty = valid."""
        ...

    def transform(self, record: AssetRecord, context: BuildContext) -> TransformResult:
        """Process source asset. Default: copy as-is."""
        ...

    def deploy(self, record: AssetRecord, context: BuildContext) -> DeployResult:
        """Copy processed asset to correct bin location per scope/tag rules."""
        ...

class AssetHandlerRegistry:
    def register(self, handler: AssetHandler) -> None: ...
    def get(self, type_id: str) -> AssetHandler | None: ...
```

### Events Emitted (NDJSON via OutputContext)

| Event | When |
|-------|------|
| `OnAssetValidated(assetId, typeId, errors)` | After each asset's validation handler completes |
| `OnAssetTransformed(assetId, typeId, durationMs)` | After each asset's transform handler completes |
| `OnAssetDeployed(assetId, destPath, durationMs)` | After each asset is copied to bin |
| `OnAssetFailed(assetId, typeId, phase, errors)` | When any handler returns errors |
| `OnBuildCompleted(passCount, failCount, totalDurationMs)` | After all assets processed |

### Exit Codes

| Code | Meaning |
|------|---------|
| `0` | All assets validated, transformed, deployed successfully |
| `1` | One or more assets failed validation or transform |
| `2` | Invalid arguments or missing/unreadable manifest |

## Dependencies

| Dependency | What DiaAssetPipeline uses from it |
|------------|-----------------------------------|
| DiaAssetCatalogue | Reads `assets.catalogue.json` (source manifest in `Assets/<AppName>/`) via Python `json.load` — not through C++ ManifestSerializer. Gets asset IDs, types, source paths, tags, scope, stage name, content hashes. |
| DiaPipeline | Calls `dia asset build` as the `build-assets` stage implementation |
| DiaCLI | All commands are DiaCLI plugins; uses Click, OutputContext, plugin discovery |
| DiaLogger | NDJSON event log written to `Cluiche/out/<AppName>/logs/asset-pipeline/last-run.ndjson` |

## Features

| # | Feature | Size | Description | Spec | Status |
|---|---------|------|-------------|------|--------|
| 1 | Handler Registry & Build Runner | S | Core infrastructure: `AssetHandlerRegistry`, `BuildContext`, per-phase runner (validate → transform → deploy), error collection, NDJSON output | @docs/specs/features/dia/diaassetpipeline/handler-registry-build-runner.md | Approved |
| 2 | Built-in Type Handlers | S-M | Default handlers for all 8 taxonomy types — validate required fields, copy-as-is transform, deploy per scope/tag rules. Folder assets (`*.folder/`) deploy the entire directory tree recursively, preserving internal structure (SD-CAT-013). | @docs/specs/features/dia/diaassetpipeline/built-in-type-handlers.md | Approved |
| 3 | Deploy Layout Engine | S | Resolve scope (`global`/`stage`) and category tag to output path; create directories; copy files | @docs/specs/features/dia/diaassetpipeline/deploy-layout-engine.md | Approved |
| 4 | CLI Command Surface | S | `dia asset build`, `dia asset validate`, `dia asset deploy` with `--target`, `--config`, `--force` flags | @docs/specs/features/dia/diaassetpipeline/cli-command-surface.md | Approved |

**Build order:** 1 → 3 → 2 → 4 (registry and layout engine first; handlers depend on both; CLI wraps all)

## Design Constraints

- **Collect all errors, no early exit.** Validation runs across all assets before reporting. A single broken asset does not stop other assets from being processed.
- **Copy-as-is is the default.** A type with no registered transform handler just copies source to deploy path. No handler registration required for simple asset types.
- **Handler registration is pure Python.** No C++ compilation required to add a new asset type handler.
- **Incremental builds deferred.** Content hashes are available on AssetRecord but DiaAssetPipeline always does a full pass. Hash-based skip logic is a future optimisation.
- **No STL in public APIs (PD-004, AD-002).** Python only — not applicable to Python code, but any future C++ transform tool invoked as a subprocess must not leak STL types across its boundary.
- **Catalogue manifest is read-only.** DiaAssetPipeline reads `assets.catalogue.json` but never writes it. Catalogue authoring is owned by DiaAssetCatalogueEditor. DiaAssetPipeline generates `assets.runtime.json` as a deploy output.
- **Python reads manifest directly.** DiaAssetPipeline uses Python `json.load` to read the catalogue manifest — it does not depend on DiaAssetCatalogue's C++ `ManifestSerializer` API.

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-APIPE-001 | DiaAssetPipeline reads `assets.catalogue.json` (source manifest); it generates `assets.runtime.json` (deployed manifest with deploy paths, stripped of build-time fields). It never writes to the catalogue manifest. | Catalogue manifest is authored content owned by DiaAssetCatalogueEditor; pipeline is a consumer of catalogue data and a producer of the runtime manifest | All features | Accepted | Yes |
| SD-APIPE-002 | Validation collects all errors before reporting — no early exit | Developer experience: see all broken assets in one run rather than fix-one-run-repeat | All features | Accepted | Yes |
| SD-APIPE-003 | Copy-as-is is the default transform for types with no registered handler | Enables pipeline to function for all asset types before custom transforms are written; no handler = no blocking | All features | Accepted | Yes |
| SD-APIPE-004 | Deploy layout is driven by AssetRecord scope and tags (SD-CAT-007) | Single source of truth for layout rules in DiaAssetCatalogue; pipeline enforces, catalogue defines | All features | Accepted | Yes |
| SD-APIPE-005 | Incremental builds (content hash skip) are a future feature | Full pass every run is correct and simple; optimise when build times justify it | All features | Accepted | Yes |
| SD-APIPE-006 | All handlers are pure Python | Build tooling benefits from Python flexibility; no compilation required to add a type handler | All features | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | Source | Implication for DiaAssetPipeline |
|----|----------|--------|----------------------------------|
| PD-001 | StringCRC for all entity/component IDs | Platform | Asset IDs read from manifest are StringCRC-keyed; Python represents them as strings matching the `type.name` format |
| PD-005 | x64 Windows only | Platform | No cross-platform considerations; all paths use Windows conventions |
| PD-006 | Visual Studio project files are source of truth | Platform | Python tooling only — no new `.vcxproj` introduced |
| PD-007 | C++20 required | Platform | Python tooling only — not applicable |
| PD-008 | Directory.Build.props owns OutDir | Platform | Deploy target path (`bin/<App>/<Config>/<Platform>/assets/`) resolved from Directory.Build.props conventions at runtime, consistent with DiaPipeline's `$(OutDir)` resolution |
| PD-009 | Generated output under Cluiche/out/ | Platform | NDJSON event log written to `Cluiche/out/<AppName>/logs/asset-pipeline/last-run.ndjson`. Deployed assets go to `bin/` (binary output), not `out/`. Compliant. |
| AD-001 | Module system with YAML frontmatter | Application | Python tooling only — no new C++ module introduced |
| AD-003 | Namespace: Dia::\<Module\>:: | Application | Python package: `dia_cli/commands/asset/`; namespace convention not applicable to Python |
| SD-CLI-001 | MDK CLI architecture | DiaCLI | All `dia asset` commands follow the two-file plugin pattern |
| SD-CLI-002 | Python-based implementation | DiaCLI | All pipeline commands are Python |
| SD-CLI-006 | Click framework | DiaCLI | All commands use Click for argument parsing |
| SD-CLI-008 | Exit codes follow Unix conventions | DiaCLI | Exit codes follow the table above |
| SD-PIPE-002 | Stage ordering fixed: compile-code → build-assets → deploy | DiaPipeline | DiaAssetPipeline implements the `build-assets` stage; it does not reorder or skip stages |
| SD-PIPE-005 | build-assets was a no-op stub until asset pipeline specced | DiaPipeline | This system replaces that stub |
| SD-CAT-001 | Asset IDs are strictly `type.name` composites | DiaAssetCatalogue | All asset lookups use `type.name` format; handler dispatch keyed on type prefix |
| SD-CAT-003 | Registry populated by callers, not self-scan | DiaAssetCatalogue | DiaAssetPipeline reads the manifest; it does not scan directories |
| SD-CAT-004 | Two manifests: `assets.catalogue.json` (source) and `assets.runtime.json` (deployed) | DiaAssetCatalogue | DiaAssetPipeline reads `assets.catalogue.json` from `Assets/<AppName>/`; generates `assets.runtime.json` with deploy paths; DiaAssetRuntime reads `assets.runtime.json` from `bin/` |
| SD-CAT-007 | AssetRecord carries scope and tags driving deploy layout | DiaAssetCatalogue | DiaAssetPipeline's deploy layout engine consumes scope and tags directly from AssetRecord |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Handler registry | Should built-in handlers (for the 9 taxonomy types) be auto-registered at startup, or explicitly registered by the `dia asset build` command? | Auto-registered at import time — consistent with DiaCLI plugin discovery pattern. No caller needs to explicitly register built-ins. |
| 2 | Error handling | If transform succeeds but deploy (file copy) fails, does the asset count as failed? | Yes — any phase failure (validate, transform, or deploy) marks the asset as failed and emits `OnAssetFailed`. All other assets continue. |
| 3 | Deploy | Should deploy overwrite existing files unconditionally, or skip if destination is newer? | Overwrite unconditionally for now. Matches current `xcopy /Y` behaviour in DiaPipeline's deploy stage. Incremental skip is deferred with SD-APIPE-005. |
| 4 | Manifest read | DiaAssetPipeline reads the catalogue manifest — where does it expect to find it? | Path is a required `catalogue_manifest` field in `pipeline.toml` per-target config (e.g. `catalogue_manifest = "Assets/CluicheTest/assets.catalogue.json"`). DiaAssetPipeline reads it via Python `json.load` (not C++ CatalogueManifestSerializer). Pipeline generates `assets.runtime.json` (deploy paths, no content hashes or source paths) and deploys it to `bin/<App>/<Config>/<Platform>/assets/assets.runtime.json`. |
| 5 | Multi-target | If `dia asset build` is run for two targets (CluicheTest and GoogleTests), do they share a catalogue manifest or have separate ones? | Each target declares its own `catalogue_manifest` in `pipeline.toml` — they may share a manifest or have separate ones. Each target also declares `asset_stages` — the list of Stage asset IDs it uses. The pipeline deploys only assets scoped to those Stages plus all global assets. Each target gets its own `assets.runtime.json` in its own bin path. |
| 6 | Tags | What happens to an asset with no category tag (or a tag that doesn't match any known category)? | Deploys to `misc/` within its scope. No error — untagged assets are valid, just uncategorised. |
| 7 | Scope | What happens if an asset is referenced by zero Stages (orphaned asset)? | Emit a warning in NDJSON log; deploy to `global/misc/` as a safe fallback. Orphaned assets are not an error — they may be used via direct path resolution without a Stage load. |

## Status

`Approved` — 4 features, all Approved. Plan: @docs/specs/systems/dia/diaassetpipeline.plan.md

# Asset System Overview

This document describes how the four asset systems connect end-to-end. It is a navigation aid — each system has its own spec; this doc shows the full picture and is the canonical reference for the flow.

---

## Two Manifests

The asset system uses two distinct manifest files:

| Manifest | Location | Owner | Format |
|----------|----------|-------|--------|
| `assets.catalogue.json` | `Assets/<AppName>/` (source tree) | DiaAssetCatalogueEditor (sole writer) | Source paths, content hashes, tags, scope, relationships — full build-time metadata |
| `assets.runtime.json` | `bin/<App>/<Config>/<Platform>/assets/` (deploy dir) | DiaAssetPipeline (generates during deploy) | Deploy-relative paths, scope, relationships — stripped of source paths and content hashes |

DiaAssetPipeline reads the catalogue manifest and generates the runtime manifest. DiaAssetRuntime reads only the runtime manifest.

---

## End-to-End Flow

```
Raw authored files (Assets/<AppName>/)
         │
         ▼
 ┌─────────────────────────────────────────────────────┐
 │  DiaAssetCatalogue                                  │
 │  Pure C++ data model: identity, type registry,      │
 │  asset registry, bidirectional relationships         │
 │  Namespace: Dia::AssetCatalogue::                   │
 │  Path: Dia/DiaAssetCatalogue/                       │
 └──────────────────────┬──────────────────────────────┘
                        │ Provides data model used by
                        │ editor (authoring) and
                        │ runtime (deserialization)
                        ▼
 ┌─────────────────────────────────────────────────────┐
 │  DiaAssetCatalogueEditor                            │
 │  IEditorPlugin — sole author of                     │
 │  Assets/<AppName>/assets.catalogue.json             │
 │  (record CRUD, file discovery, relationships,       │
 │  validation, OS-default asset open)                 │
 └──────────────────────┬──────────────────────────────┘
                        │ assets.catalogue.json
                        │ (source paths, content hashes,
                        │  tags, scope, relationships)
                        ▼
 ┌─────────────────────────────────────────────────────┐
 │  DiaPipeline  (Done — dia pipeline CLI)             │
 │  Orchestrates: compile-code → build-assets → deploy │
 │                                                     │
 │   build-assets stage calls DiaAssetPipeline         │
 │   ├── Reads assets.catalogue.json (Python json.load)│
 │   ├── Per-type handlers: validate, transform, deploy│
 │   ├── Generates assets.runtime.json (deploy paths)  │
 │   └── Deploys assets + runtime manifest to bin/     │
 │                                                     │
 │   Deploy layout:                                    │
 │   bin/<App>/<Config>/<Platform>/assets/              │
 │   ├── assets.runtime.json                           │
 │   ├── global/{category}/     (scope = global)       │
 │   └── stages/<Name>/{category}/ (scope = stage)     │
 └──────────────────────┬──────────────────────────────┘
                        │ assets.runtime.json
                        │ (deploy paths, no content hashes,
                        │  no source paths)
                        ▼
 ┌─────────────────────────────────────────────────────┐
 │  DiaAssetRuntime                                    │
 │  Pure C++ library. No DiaApplication dependency.    │
 │  No DiaAssetCatalogue dependency.                   │
 │  Own types: RuntimeAssetEntry, RuntimeStageEntry.   │
 │  Own deserializer: RuntimeManifestLoader.           │
 │  ID → absolute path resolution.                     │
 │  Stage → Asset lifecycle management.                │
 │  Ref counting for global assets.                    │
 │  Namespace: Dia::AssetRuntime::                     │
 └──────────────────────┬──────────────────────────────┘
                        │ Absolute paths + state events
                        ▼
                  Game systems consume assets
                  (DiaGraphics, DiaRig2D, etc.)
```

---

## System Responsibilities

| System | Layer | Owns | Does NOT own |
|--------|-------|------|--------------|
| **DiaAssetCatalogue** | Data model | Asset identity (StringCRC `type.name`), AssetTypeDescriptor registry, AssetRegistry, RelationshipIndex, `CatalogueManifestSerializer`, JsonDefinitionLoader | Build steps, runtime loading, editor UI, file watching |
| **DiaAssetCatalogueEditor** | Editor UI | Sole author of `assets.catalogue.json` in `Assets/<AppName>/`. Record CRUD, file discovery, relationship editing, validation display, asset-type routing, catalogue rules engine (`assets.rules.json` — auto-applies relationships, tags, scope, stage membership from configurable rules) | Any business logic — calls DiaAssetCatalogue's API only |
| **DiaAssetPipeline** | Build-time | Per-asset-type Python handlers (validate, transform, deploy rules). Reads `assets.catalogue.json` via `json.load`. Generates `assets.runtime.json`. Called by DiaPipeline's `build-assets` stage. | Pipeline orchestration (that's DiaPipeline), runtime loading, editor UI, catalogue manifest authoring |
| **DiaPipeline** | Build orchestrator | `dia pipeline` CLI: compile-code → build-assets → deploy. Calls DiaAssetPipeline during `build-assets`. | Per-type asset logic (that's DiaAssetPipeline), runtime loading |
| **DiaAssetRuntime** | Runtime | Deserializes `assets.runtime.json` via own `RuntimeManifestLoader` (no DiaAssetCatalogue dependency). Own data types: `RuntimeAssetEntry`, `RuntimeStageEntry`. Absolute path resolution (ID→FilePath), Stage load/unload lifecycle, asset state machine, ref counting for shared assets, event notification to content systems. DiaAPI debug commands for editor queries. | Content loading (DiaGraphics/DiaAudio own that), build pipeline, editor UI, catalogue manifest, DiaAssetCatalogue types |
| **DiaAssetRuntimeEditor** | Editor UI | Live runtime asset inspector — connects to running game via DiaDebugServer WebSocket, displays asset states (Registered/Staged/Loaded/Unloading), Stage/Asset tree, global asset ref counts, state transition log. Read-only observer. | State mutation, manifest authoring, content loading. No DiaAssetCatalogue dependency. |

---

## Content Hash Ownership

`AssetRecord.mContentHash` is a CRC32 of the source file bytes:
- **Computed by DiaAssetCatalogue's `ContentHasher`** (Feature 4) — callers (e.g. the editor) invoke it when creating or updating a record
- **Hand-authored manifests** may omit it (zero = "not computed") — `HasContentHash()` distinguishes the two states
- Content hashes exist **only in `assets.catalogue.json`** — they are stripped when DiaAssetPipeline generates `assets.runtime.json`
- DiaAssetPipeline reads content hashes for future incremental builds (skip re-cooking unchanged assets)

---

## Key Design Rules

1. **Two manifests, not one.** `assets.catalogue.json` (source, full metadata) and `assets.runtime.json` (deployed, deploy paths only). Pipeline reads one and generates the other.
2. **DiaAssetCatalogue is pure data** — no I/O beyond manifest read/write, no build steps, no rendering
3. **DiaAssetCatalogueEditor is sole catalogue author** — the only system that writes `assets.catalogue.json`
4. **DiaAssetPipeline is plugins, DiaPipeline is orchestrator** — DiaPipeline's `build-assets` stage calls DiaAssetPipeline handlers; they are not the same system
5. **DiaAssetPipeline reads catalogue, generates runtime** — it never writes to the catalogue manifest (SD-APIPE-001)
6. **DiaAssetRuntime wraps, doesn't replace** — DiaCore FilePath still works; DiaAssetRuntime adds asset-aware resolution on top. All resolved paths are absolute.
7. **DiaAssetRuntime is fully independent** — no DiaApplication dependency, no DiaAssetCatalogue dependency. Owns its own types (`RuntimeAssetEntry`, `RuntimeStageEntry`) and deserializer (`RuntimeManifestLoader`). Game binary ships without build-time type framework or authoring machinery.
8. **Pipeline is the bridge** — DiaAssetPipeline (Python) is the only system that reads the catalogue format and writes the runtime format. The two manifest schemas are independent; neither C++ system knows the other's format.
9. **Root asset pattern** — in-game, call `RequestStageLoad(stageId)` and DiaAssetRuntime resolves the full chain: Stage → Assets. You don't load individual assets by path in gameplay code.
10. **Editor is UI only** — DiaAssetCatalogueEditor has no business logic; it calls DiaAssetCatalogue's query API and renders results

---

## Specs

| System | Spec | Status |
|--------|------|--------|
| DiaAssetCatalogue | [diaassetcatalogue.md](diaassetcatalogue.md) | Approved — 3 features Approved + 1 Draft (catalogue automation), not yet built |
| DiaAssetCatalogueEditor | [diaassetcatalogueeditor.md](diaassetcatalogueeditor.md) | Approved — 8 features, needs feature specs. Blocked on DiaAssetCatalogue. |
| DiaAssetPipeline | [diaassetpipeline.md](diaassetpipeline.md) | Approved — 4 features, needs feature specs |
| DiaPipeline | [diapipeline.md](diapipeline.md) | Done — `build-assets` stage is a stub pending DiaAssetPipeline |
| DiaAssetRuntime | [diaassetruntime.md](diaassetruntime.md) | Approved — 6 features, needs feature specs. Stage → Asset lifecycle (no bundles). |
| DiaAssetRuntimeEditor | [diaassetruntimeeditor.md](diaassetruntimeeditor.md) | Approved — 4 features, needs feature specs. Depends on DiaAssetRuntime Feature 6. |

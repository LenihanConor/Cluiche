# System Spec: DiaAssetCatalogue

## Parent Application
@docs/specs/applications/dia.md

## Asset System Context
@docs/specs/systems/dia/asset-system-overview.md

## Research Context
@docs/research/engine_data_arch/summary.md

## Summary

DiaAssetCatalogue is the foundational data model layer for the Dia engine's asset architecture. It provides stable identity (StringCRC-based) for every meaningful game object, a type framework for defining and registering asset types, a central registry for cataloging and querying all project data with bidirectional relationship tracking, and a JSON definition loader for deserializing authored content into validated C++ objects.

DiaAssetCatalogue is pure infrastructure with no side effects — it does not load files at runtime, run build pipelines, or render UI. It answers: "what data exists, what type is it, how is it connected, and how do I read it into C++?"

Other systems build on top: DiaAssetPipeline (build-time), DiaAssetRuntime (runtime), and DiaAssetCatalogueEditor (tooling).

## Responsibilities

**Owns:**
- Asset identity model — every asset gets a StringCRC ID (composite `type.name`, e.g. `"texture.player_ship"`)
- Asset type framework — registered descriptors define schema, file patterns, validation rules per type
- Asset taxonomy — the canonical set of asset types: Texture, Mesh/Sprite, Audio, Config, Entity Definition, Stage, UI Definition, Folder
- Central asset registry — catalog of all known assets with metadata, tags, forward references, and computed reverse-reference index
- JSON definition loading — single-call API for JSON file → validated typed C++ object via TypeSystem
- Bidirectional relationship graph — forward deps stored per-record, reverse deps computed and indexed
- Catalogue automation — content hash computation (`ContentHasher`), scope auto-computation from Stage reference graph (`ScopeComputer`), relationship inference from typed fields (`RelationshipInferrer`), configurable rules engine (`CatalogueRulesEngine` reading `assets.rules.json`)

**Does NOT own:**
- Build/cook pipeline (DiaAssetPipeline)
- Runtime asset loading and Stage lifecycle (DiaAssetRuntime)
- Editor browsing and inspection UI (DiaAssetCatalogueEditor)
- Validation execution as a pipeline phase (DiaAssetPipeline — but DiaAssetCatalogue provides the validation primitives)
- File watching or hot reload
- Binary serialization of built assets

## Asset Taxonomy

An **asset** is authored content with a stable identity that goes through the pipeline and gets loaded at runtime.

| Asset Type | File Pattern | Description |
|------------|-------------|-------------|
| Texture | `*.texture.png` | Image assets — sprites, backgrounds, UI elements |
| Mesh/Sprite | `*.sprite.json` | Sprite definitions referencing textures |
| Audio | `*.audio.wav` | Sound effects and music |
| Config | `*.config.json` | Game configuration data (weapon stats, tuning values) |
| Entity Definition | `*.entity.json` | Component-based entity templates (which components, what params) |
| Stage | `*.stage.json` | Hard game transition boundary — metadata only (name, display name). Asset membership is defined by `contains` relationships in the catalogue, not listed in the stage file. |
| UI Definition | `*.ui.json` | UI layout and behavior definitions |
| Folder | `*.folder/` | Directory treated as a single asset — all contained files deploy as a unit, resolved to folder path |

New asset types are added by registering a descriptor. The taxonomy is extensible — game-specific types can be added without modifying DiaAssetCatalogue.

## Public API

### Core Classes

| Class | Responsibility |
|-------|---------------|
| `JsonDefinitionLoader` | Loads a JSON file, deserializes via TypeJsonSerializer into a typed C++ object, validates required fields. Single-call API: give it a path and a type, get back a validated object. |
| `AssetTypeDescriptor` | Defines an asset type: file extension pattern, schema constraints (required fields, value ranges, reference types), validation function. One descriptor per asset type. |
| `AssetTypeRegistry` | Central registry of AssetTypeDescriptors. Register a type, query by ID or file pattern. |
| `AssetRecord` | Canonical record for one asset: StringCRC ID, type, source path, metadata (tags, status, scope, stage name), forward references, content hash. Tags drive deploy category (e.g. `characters`, `environments`, `gameplay`, `misc`, `Presentation`, `Presentation/UI`). Scope is `global` (shared across Stages) or `stage` (belongs to one named Stage). |
| `AssetRegistry` | Central catalog of all AssetRecords. Populated by importers or manifest scan. Queries: by ID, by type, by tag. Maintains computed reverse-reference index. |
| `RelationshipIndex` | Bidirectional relationship graph. Forward refs stored per-record; reverse refs ("what uses this?") computed and queryable. Supports relationship types: `uses`, `contains`, `configured_by`, `depends_on`. |
| `CatalogueManifestSerializer` | Reads and writes `assets.catalogue.json` (the source-side catalogue manifest stored in `Assets/<AppName>/`). LoadManifest deserializes into an AssetRegistry (with single-level include support). SaveManifest serializes registry state to a flat JSON file. This is the rich authoring format — source paths, content hashes, tags, scope, relationships, type metadata. DiaAssetPipeline reads this manifest (via Python `json.load`) and generates a separate `assets.runtime.json` for deployment. |
| `ContentHasher` | Computes CRC32 of a source file given a FilePath. For Folder assets (`*.folder/`), computes a combined CRC32 over all contained files (sorted by relative path for determinism). Called by callers (e.g. editor) when creating or updating records. |
| `ScopeComputer` | Traverses Stage → Asset relationship chains in the RelationshipIndex. Returns computed scope (`global`/`stage`) per asset. `RecomputeAllScopes()` batch-updates all records in the registry. |
| `RelationshipInferrer` | Walks a source file's TypeDefinition, finds `TypeVariableAttributeAssetReference` fields, resolves target asset IDs, returns inferred `uses` relationship edges. |
| `CatalogueRulesEngine` | Loads and validates `assets.rules.json` schema on load (unknown action types, missing required fields, invalid match criteria → reported as load errors). Evaluates match-action rules against registry records in array order. Rule actions: assign tag, assign scope, assign stage membership, infer relationships. Rules are additive — never delete user-authored data. Manual overrides preserved and flagged. Supports dry-run mode (returns proposed changes without mutating registry). Detects and reports conflicts (two rules assigning different values to the same field on the same record). |

### Rules Engine Schema (`assets.rules.json`)

Per-app rules file stored in `Assets/<AppName>/assets.rules.json`. Schema-validated on load (SD-CAT-011).

```json
{
    "rules": [
        {
            "name": "characters-folder-tag",
            "match": { "source_path_glob": "Assets/*/Characters/**" },
            "action": "assign_tag",
            "tag": "characters"
        },
        {
            "name": "entity-texture-refs",
            "match": { "type": "entity" },
            "action": "infer_references",
            "source": "typed_fields"
        },
        {
            "name": "player-stage-membership",
            "match": { "source_path_glob": "Assets/*/Characters/player_*" },
            "action": "assign_stage",
            "stage_id": "stage.gameplay"
        },
        {
            "name": "scope-auto",
            "match": { "all": true },
            "action": "compute_scope"
        }
    ]
}
```

**Match criteria** (one required per rule):

| Key | Type | Description |
|-----|------|-------------|
| `type` | string | Match records whose asset type ID equals this value |
| `source_path_glob` | string | Match records whose source path matches this glob pattern |
| `tag` | string | Match records that already have this tag |
| `stage_ref_count` | `{ "min": N, "max": N }` | Match records referenced by N Stages (range, inclusive) |
| `all` | `true` | Match every record |

**Action types:**

| Action | Required fields | Description |
|--------|----------------|-------------|
| `assign_tag` | `tag` | Add a tag to matching records |
| `assign_scope` | `scope` (`"global"` or `"stage"`), optional `stage_name` | Set scope on matching records |
| `assign_stage` | `stage_id` | Add a `contains` relationship from the named Stage to matching records |
| `infer_references` | `source` (`"typed_fields"`) | Run `RelationshipInferrer` on matching records to discover `uses` edges |
| `compute_scope` | (none) | Run `ScopeComputer` on matching records |

**Evaluation:** Rules are evaluated in array order (SD-CAT-009). Each rule runs against the registry state as it was *before* this evaluation pass — rules do not see mutations from earlier rules in the same pass. Conflicts (two rules setting different values for the same field on the same record) are reported as warnings; last-rule-wins.

**Dry-run:** `EvaluateDryRun()` returns a `RuleChangeset` listing every proposed mutation (record ID, field, old value, new value, source rule name) without modifying the registry. Conflicts are included in the changeset. The editor displays this for user review before calling `Apply()`.

### Rules Engine API

```cpp
namespace Dia::AssetCatalogue
{
    struct RuleChange
    {
        Dia::Core::StringCRC mRecordId;
        Dia::Core::Containers::String64 mField;     // e.g. "scope", "tag", "relationship"
        Dia::Core::Containers::String64 mOldValue;
        Dia::Core::Containers::String64 mNewValue;
        Dia::Core::Containers::String64 mRuleName;
        bool mIsConflict;                            // true if another rule also sets this field
    };

    struct RuleChangeset
    {
        Dia::Core::Containers::DynamicArrayC<RuleChange, 256> mChanges;
        unsigned int mConflictCount;
    };

    class CatalogueRulesEngine
    {
    public:
        // Load and schema-validate rules file. Returns errors if invalid.
        LoadResult<void> LoadRules(const Dia::Core::FilePath& rulesPath,
                                   const AssetTypeRegistry& typeRegistry);

        // Dry-run: returns proposed changes without modifying registry
        RuleChangeset EvaluateDryRun(const AssetRegistry& registry,
                                     const RelationshipIndex& relationships) const;

        // Apply: evaluates rules and mutates registry
        RuleChangeset Apply(AssetRegistry& registry,
                            RelationshipIndex& relationships) const;

        unsigned int GetRuleCount() const;
    };
}
```

### Namespace

`Dia::AssetCatalogue::`

All public headers under `Dia/DiaAssetCatalogue/`.

### Include Pattern

```cpp
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaAssetCatalogue/AssetTypeDescriptor.h>
#include <DiaAssetCatalogue/JsonDefinitionLoader.h>
#include <DiaAssetCatalogue/RelationshipIndex.h>
```

## Dependencies

| Dependency | What DiaAssetCatalogue uses from it |
|------------|--------------------------|
| DiaCore — TypeSystem | Runtime RTTI, field reflection, TypeJsonSerializer, TypeVariableAttributes for schema constraints |
| DiaCore — CRC/StringCRC | Asset identity (composite `type.name` IDs) |
| DiaCore — Containers | HashTableC (registry backing store), DynamicArrayC (reference lists), Handle<T> (safe asset references) |
| DiaCore — FilePath/PathStore | Path aliasing for source locations, deferred resolution |
| DiaCore — JSON | jsoncpp wrapper for reading/writing authored JSON |

No dependencies on DiaAPI, DiaApplication, DiaEditor, or any rendering/windowing module.

## Features

| # | Feature | Size | Description | Spec |
|---|---------|------|-------------|------|
| 1 | JSON Definition Loader | S | Single-call API: JSON file → validated typed C++ object via TypeSystem | [json-definition-loader.md](../../features/dia/diaassetcatalogue/json-definition-loader.md) |
| 2 | Asset Type Framework | S-M | Type descriptor registration, file pattern matching, schema definition via TypeSystem reflection | [asset-type-framework.md](../../features/dia/diaassetcatalogue/asset-type-framework.md) |
| 3 | Identity & Relationship Backbone | M | Asset registry with StringCRC identity, metadata, bidirectional relationship tracking (forward + reverse deps) | [identity-relationship-backbone.md](../../features/dia/diaassetcatalogue/identity-relationship-backbone.md) |
| 4 | Catalogue Automation | M-L | Content hash computation (including directory hashing for Folder assets), scope auto-computation (Stage reference graph traversal), relationship inference from `TypeVariableAttributeAssetReference` fields, configurable rules engine (`assets.rules.json`) with schema validation on load, dry-run mode (proposed changes without mutation), conflict detection (two rules targeting same field on same record), tag / stage membership / scope / relationship assignment based on match criteria (type, path glob, stage ref count). Rules evaluated in array order. Pure C++ logic — no UI. | [catalogue-automation.md](../../features/dia/diaassetcatalogue/catalogue-automation.md) |

**Build order:** 1 → 2 → 3 → 4 (automation depends on registry, type framework, and relationship index)

## Design Constraints

- **Content hashing from the start.** Every AssetRecord stores a CRC32 content hash of its source file. DiaAssetCatalogue provides `ContentHasher` to compute hashes — callers (e.g. the editor) call it when creating or updating records. Content hashes live only in `assets.catalogue.json` — DiaAssetPipeline reads them for future incremental build decisions but does not propagate them to `assets.runtime.json`.
- **Reverse dependencies are first-class.** The RelationshipIndex computes and maintains reverse refs — not a query-time scan. This is an architectural commitment in the data model.
- **Validation primitives, not validation execution.** DiaAssetCatalogue provides the schema constraints (via AssetTypeDescriptor) and referential integrity checks (via RelationshipIndex). DiaAssetPipeline orchestrates when validation runs.
- **No STL containers in public APIs (PD-004, AD-002).** All public interfaces use DiaCore containers.
- **StringCRC identity (PD-001).** Asset IDs are StringCRC values, not raw strings.

## Downstream Systems

| System | How it uses DiaAssetCatalogue | Status |
|--------|-------------------|--------|
| DiaAssetPipeline | Reads `assets.catalogue.json` via Python `json.load`. Runs per-type handlers (validate, transform, deploy). Generates `assets.runtime.json` with deploy paths and deploys it to `bin/<App>/<Config>/<Platform>/assets/`. Content hashes used only by pipeline for incremental builds — not written to runtime manifest. | Planned |
| DiaAssetRuntime | Deserializes `assets.runtime.json` at startup using its own `RuntimeManifestLoader` (lightweight, no dependency on DiaAssetCatalogue). Resolves absolute paths under `bin/` deploy root. Manages Stage → Asset lifecycle (content loaded per Stage on Stage transition). DiaAssetRuntime does NOT depend on DiaAssetCatalogue — the two manifests have different formats and different serializers. | Planned |
| DiaAssetCatalogueEditor | Sole author of `assets.catalogue.json` in `Assets/<AppName>/`. Reads AssetRegistry and RelationshipIndex to present browse, inspect, and relationship views. | Future |

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-CAT-001 | Asset IDs are strictly `type.name` composites | Type prefix enables efficient per-type registry queries without a secondary index; enforced at registration time | All features | Accepted | Yes |
| SD-CAT-002 | Validation primitives live in DiaAssetCatalogue; validation execution lives in DiaAssetPipeline | DiaAssetCatalogue is pure data model — it provides schema constraints and referential integrity checks; DiaAssetPipeline orchestrates when validation runs and owns structured error reporting | All features | Accepted | Yes |
| SD-CAT-003 | Registry is populated by callers, not by self-scan | DiaAssetCatalogue has no knowledge of the filesystem layout; discover is DiaAssetPipeline's job; manifest deserialization is the only self-directed population path | All features | Accepted | Yes |
| SD-CAT-004 | Two manifests: `assets.catalogue.json` (source, in `Assets/<AppName>/`) and `assets.runtime.json` (deployed, in `bin/<App>/<Config>/<Platform>/assets/`). Catalogue manifest is authored by DiaAssetCatalogueEditor and read by DiaAssetPipeline. Runtime manifest is generated by DiaAssetPipeline (deploy paths, stripped of build-time fields) and deserialized by DiaAssetRuntime at startup. | Clear separation of authored vs deployed data; content hashes and source paths stay build-side only; runtime manifest is self-contained with deploy-relative paths | All features | Accepted | Yes |
| SD-CAT-005 | One global manifest per app, lazy content loading per Stage | Manifest is metadata only (no asset bytes) — cheap to load at startup; content laziness happens in DiaAssetRuntime when Stages load; per-Stage manifest splitting deferred until project scale justifies it | All features | Accepted | Yes |
| SD-CAT-006 | Relationship vocabulary is StringCRC-typed and extensible | Ships with starter set (uses, contains, configured_by, depends_on); game-specific relationship types can be registered without modifying DiaAssetCatalogue | All features | Accepted | Yes |
| SD-CAT-007 | AssetRecord carries scope (`global`/`stage`) and stage name; tags drive deploy category | Scope determines whether an asset lands under `assets/global/` or `assets/stages/<StageName>/` in bin; tags (e.g. `characters`, `Presentation/UI`) determine the subdirectory within that scope. Rule: assets referenced by exactly one Stage are `stage`-scoped; assets referenced by multiple Stages are `global`. Authored in the manifest, enforced at pipeline deploy time. | All features | Accepted | Yes |
| SD-CAT-008 | Catalogue automation logic (hash, scope, relationship inference, rules engine) lives in DiaAssetCatalogue, not the editor | Editor is UI only (SD-ACE design constraint). Business logic — graph traversal, type system introspection, file hashing, rule evaluation — belongs in the data model layer. Editor calls these APIs and presents results. | Catalogue Automation | Accepted | Yes |
| SD-CAT-009 | Rules engine evaluates rules in array order; no separate priority field | Array order is simple, predictable, and debuggable. If rule A must run before rule B, put it first in the array. Priority numbers add indirection without value at this scale. | Catalogue Automation | Accepted | Yes |
| SD-CAT-010 | Rules engine supports dry-run mode and conflict detection | Dry-run returns a changeset (proposed mutations) without modifying the registry — critical for user trust and auditability. Conflict detection flags when two rules assign different values to the same field on the same record (e.g. two rules set different scopes). Conflicts are reported as warnings, not errors — last-rule-wins is the resolution. | Catalogue Automation | Accepted | Yes |
| SD-CAT-011 | `assets.rules.json` is per-app, hand-authored, schema-validated on load | Each app in `Assets/<AppName>/` has its own rules file. Schema validation on load rejects unknown action types, missing required fields, and invalid match criteria with clear error messages. Rules files are human-editable JSON — no binary format. | Catalogue Automation | Accepted | Yes |
| SD-CAT-012 | Stage→Asset membership lives in the RelationshipIndex, not in the `*.stage.json` file | Stage files contain only metadata (name, display name). "What assets belong to this Stage?" is answered by querying `contains` edges from the Stage record in the RelationshipIndex. This keeps the rules engine, editor, and ScopeComputer operating on a single source of truth. Stage files do not list their member assets. | All features | Accepted | Yes |
| SD-CAT-013 | Folder assets deploy as directory trees, not archives | Pipeline copies the entire `*.folder/` directory to the deploy location, preserving internal structure. Runtime manifest entry points to the folder path (trailing `/`). Consumer knows the internal layout. No archive format or extraction step. | Catalogue Automation, Pipeline | Accepted | Yes |

## Inherited Binding Decisions

| ID | Decision | Source | Implication for DiaAssetCatalogue |
|----|----------|--------|------------------------|
| PD-001 | StringCRC for all entity/component IDs | Platform | Asset IDs are StringCRC values (composite `type.name`). All registry lookups are CRC-keyed. |
| PD-002 | ProcessingUnit/Phase/Module architecture | Platform | DiaAssetCatalogue itself does not use ProcessingUnit (it's a data model, not an app). Downstream systems (DiaAssetRuntime) integrate with this pattern. |
| PD-003 | Component-based entities (IComponent/IComponentObject) | Platform | Entity Definition assets define component compositions. DiaAssetCatalogue stores the definition; DiaAssetRuntime instantiates via ComponentFactoryRegistry. |
| PD-004 | No STL containers in public APIs | Platform | All DiaAssetCatalogue public interfaces use HashTableC, DynamicArrayC, Handle<T>. |
| PD-005 | x64 Windows only | Platform | No cross-platform considerations in DiaAssetCatalogue. |
| PD-006 | Visual Studio project files are source of truth | Platform | DiaAssetCatalogue.vcxproj is maintained manually; no build system generates or modifies it. |
| PD-007 | C++20 required | Platform | Can use concepts for type constraints on AssetTypeDescriptor, std::span for zero-copy views, constexpr for compile-time validation. |
| PD-008 | Directory.Build.props owns build settings | Platform | DiaAssetCatalogue.vcxproj follows centralized build configuration. |
| PD-009 | Generated output under Cluiche/out/ | Platform | DiaAssetCatalogue does not generate output itself, but defines the record structure that DiaAssetPipeline writes to Cluiche/out/. |
| AD-001 | Module system with YAML frontmatter | Application | DiaAssetCatalogue gets a `dia.assetcatalogue.architecture.module.md` with YAML frontmatter. Child modules get their own architecture docs. |
| AD-002 | No STL in public APIs | Application | Same as PD-004, reinforced at application level. |
| AD-003 | Namespace: Dia::\<Module\>:: | Application | All DiaAssetCatalogue code under `Dia::AssetCatalogue::` namespace. |
| AD-005 | Component-based entities | Application | Entity Definition assets are component-aware — they reference IComponent types by StringCRC. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Identity | Should asset IDs be strictly `type.name` composites, or should the format be flexible? | Strict `type.name` — the type prefix enables efficient per-type queries on the registry without a secondary index. Enforced at registration time. Promoted to SD-CAT-001. |
| 2 | Taxonomy | Should new asset types require C++ code (registering a descriptor), or should types be definable in data? | C++ registration for now. TypeSystem reflection requires compiled types. Data-driven type definitions could be a future extension but add significant complexity. |
| 3 | Relationships | Should the relationship vocabulary (uses, contains, etc.) be fixed or extensible? | Extensible — relationships are StringCRC-typed. DiaAssetCatalogue ships with a starter set (uses, contains, configured_by, depends_on) but game-specific types can be registered. Promoted to SD-CAT-006. |
| 4 | Registry | Should the registry persist to disk (JSON manifest) or be rebuilt in-memory each time? | Both. The registry can be populated from a manifest file (for tools/editor) or built in-memory by importers (for the pipeline). The manifest is a serialized snapshot of registry state. |
| 5 | Hashing | CRC32 has non-trivial collision probability at scale. Is this acceptable? | Yes for a small game. StringCRC already stores the original string alongside the hash for debug comparison. If collisions become a problem, the identity model can be upgraded to CRC64 without changing the registry's API — it's keyed by StringCRC, not by raw uint32. |
| 6 | Threading | Does the AssetRegistry need to be thread-safe? | Not in DiaAssetCatalogue itself. Thread safety is the responsibility of the system that owns the lifecycle (DiaAssetPipeline at build time, DiaAssetRuntime at runtime). DiaAssetCatalogue's API should be reentrant but not internally synchronized. |
| 7 | Scope | Where is the line between DiaAssetCatalogue's validation primitives and DiaAssetPipeline's validation execution? | DiaAssetCatalogue provides: schema constraints on AssetTypeDescriptor and referential integrity queries on RelationshipIndex. DiaAssetPipeline provides: orchestration, structured error reporting, and the pass/reject decision. Promoted to SD-CAT-002. |
| 8 | Pipeline lifecycle | Does the same AssetRegistry instance live at both build time and runtime, or is the manifest the hand-off? | Two manifests are the hand-off (SD-CAT-004). `assets.catalogue.json` (source, in `Assets/<AppName>/`) is authored by DiaAssetCatalogueEditor. DiaAssetPipeline reads it via Python `json.load`, processes assets, and generates `assets.runtime.json` (deploy paths, no content hashes or source paths) in `bin/<App>/<Config>/<Platform>/assets/`. DiaAssetRuntime deserializes `assets.runtime.json` into a fresh registry at startup. Build and runtime are separate processes — instance sharing is not possible. |
| 9 | Manifest granularity | One global manifest or one per Stage? | One global manifest per app (SD-CAT-005). Manifest is metadata only — no asset bytes — so loading it once at startup is cheap. Lazy loading applies to asset *content* inside DiaAssetRuntime when Stages load. Per-Stage manifest splitting deferred as a future optimization. |
| 10 | Content hash | Who computes the content hash on AssetRecord, and is zero a valid value? | DiaAssetCatalogue provides `ContentHasher` (Feature 4) which computes CRC32 of the source file. Callers (e.g. the editor) call it when creating or updating records. Zero means "not yet computed" (valid for hand-authored manifests or records added before the source file exists). `HasContentHash()` helper distinguishes hashed from unhashed records. DiaAssetCatalogue does not reject zero-hash records. Content hashes live only in `assets.catalogue.json` — they are stripped when DiaAssetPipeline generates `assets.runtime.json`. |
| 11 | Rules engine | How does dry-run mode work? | `CatalogueRulesEngine::EvaluateDryRun()` returns a `RuleChangeset` — a list of proposed mutations (record ID, field, old value, new value, source rule name) without modifying the registry. The editor displays this changeset for user review before calling `Apply()`. Dry-run and apply use the same evaluation logic — dry-run just skips the mutation step. |
| 12 | Rules engine | What counts as a conflict? | Two rules assigning *different* values to the *same* field on the *same* record in a single evaluation pass. Example: rule A sets `scope = kGlobal` and rule B sets `scope = kStage` on `texture.player_ship`. Conflicts are reported as warnings in the changeset with both rule names and values. Resolution: last-rule-wins (array order), but the warning makes it visible so the author can reorder or adjust match criteria. |
| 13 | Rules engine | Should the rules file support conditional composition (e.g. "only if tag X is already present")? | Not in v1. Match criteria are evaluated against the record's state *before* any rules run in this pass. Chaining (rule A adds a tag, rule B matches on that tag) would require multi-pass evaluation, which adds complexity. Deferred — single-pass evaluation is sufficient for the current use cases. |
| 14 | Rules engine | How is `assets.rules.json` validated? | On load, `CatalogueRulesEngine::LoadRules()` validates: (a) JSON schema — required fields per action type, known action type strings, valid match criteria keys; (b) referential — asset type IDs in match criteria exist in AssetTypeRegistry; Stage IDs in `assign_stage` actions are valid `type.name` format. Validation errors are returned as a list — all checked, no early exit. Invalid rules files are rejected entirely (not partially loaded). |

## Status

`Done` — all 4 features implemented 2026-05-04; 92 tests (34 feature + 58 exhaustive)  
**Plan:** @docs/specs/systems/dia/diaassetcatalogue.plan.md

# Feature Spec: Catalogue Automation

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetCatalogue | @docs/specs/systems/dia/diaassetcatalogue.md |
| Feature | Catalogue Automation | (this document) |

## Summary

Four automation classes that operate on the AssetRegistry and RelationshipIndex to compute content hashes, determine asset scope from Stage reference graphs, infer relationships from typed fields, and evaluate configurable match-action rules. All logic is pure C++ with no UI — the editor calls these APIs and presents results.

`ContentHasher` computes CRC32 hashes of source files (including combined directory hashing for Folder assets). `ScopeComputer` traverses Stage-to-Asset `contains` relationships to determine whether each asset is global or stage-scoped. `RelationshipInferrer` walks a TypeDefinition's fields looking for `TypeVariableAttributeAssetReference` markers and returns inferred `uses` edges. `CatalogueRulesEngine` loads and schema-validates a per-app `assets.rules.json`, evaluates match-action rules in array order, supports dry-run mode (proposed changes without mutation), and detects conflicts when two rules target the same field on the same record.

## Problem

Without automation, every asset's content hash must be manually tracked, scope must be hand-assigned (and re-assigned when Stage membership changes), relationships must be manually authored (even when they're derivable from typed fields), and bulk operations like "tag everything under Characters/" require per-record edits. This is error-prone at any scale and blocks the editor from offering one-click "recompute everything." A rules engine adds repeatability — the same rules file produces the same catalogue state regardless of who runs it or when.

## Acceptance Criteria

1. `ContentHasher::ComputeHash(filePath)` returns CRC32 of the source file's bytes
2. For Folder assets (`*.folder/` directories), `ContentHasher::ComputeDirectoryHash(folderPath)` enumerates all contained files, sorts them by relative path for determinism, computes CRC32 of each file's bytes, and combines them into a single CRC32
3. `ScopeComputer` queries `contains` relationships from Stage records in the RelationshipIndex. An asset referenced by exactly 1 Stage gets `kStage` scope with the stage name set. An asset referenced by 0 or >1 Stages gets `kGlobal` scope.
4. `ScopeComputer::RecomputeAllScopes(registry, relationships)` batch-updates every record's scope and stage name in the registry
5. `RelationshipInferrer::InferRelationships(record, typeRegistry)` walks the record's TypeDefinition, finds all fields marked with `TypeVariableAttributeAssetReference`, resolves each to a target asset ID, and returns a list of `uses` RelationshipEdge values
6. `CatalogueRulesEngine::LoadRules(path, typeRegistry)` reads `assets.rules.json`, validates the schema (unknown action types, missing required fields per action, invalid match criteria keys, type IDs not in AssetTypeRegistry, stage IDs not in `type.name` format), and returns errors if invalid. Invalid rules files are rejected entirely — not partially loaded.
7. Match criteria supported: `type` (asset type ID equals value), `source_path_glob` (source path matches glob), `tag` (record already has this tag), `stage_ref_count` (min/max range of Stage references, inclusive), `all` (matches every record). Exactly one match criterion per rule.
8. Action types supported: `assign_tag` (adds a tag), `assign_scope` (sets scope, optionally with stage name), `assign_stage` (adds a `contains` relationship from the named Stage), `infer_references` (runs RelationshipInferrer on matching records), `compute_scope` (runs ScopeComputer on matching records)
9. Rules are evaluated in array order (SD-CAT-009). Each rule runs against the registry state as it was before this evaluation pass — rules do not see mutations from earlier rules in the same pass.
10. `EvaluateDryRun(registry, relationships)` returns a `RuleChangeset` listing every proposed mutation (record ID, field, old value, new value, source rule name) without modifying the registry or relationship index
11. `Apply(registry, relationships)` evaluates rules and mutates the registry and relationship index, returning the same `RuleChangeset` that dry-run would have produced
12. Conflict detection: when two rules assign different values to the same field on the same record in one pass, `RuleChange::mIsConflict` is set to `true` on both entries and `RuleChangeset::mConflictCount` is incremented. Resolution is last-rule-wins.
13. Rules are additive — they never delete user-authored data. Manual overrides are preserved.
14. Pure C++ logic — no UI, no file watching, no editor integration in this feature

## API Design

### ContentHasher

```cpp
namespace Dia::AssetCatalogue
{
    class ContentHasher
    {
    public:
        // CRC32 of a single source file
        unsigned int ComputeHash(const Dia::Core::FilePath& filePath) const;

        // Combined CRC32 for a Folder asset (*.folder/ directory)
        // Enumerates files, sorts by relative path, combines individual CRC32s
        unsigned int ComputeDirectoryHash(const Dia::Core::FilePath& folderPath) const;
    };
}
```

### ScopeComputer

```cpp
namespace Dia::AssetCatalogue
{
    class ScopeComputer
    {
    public:
        // Compute scope for a single asset based on Stage reference count
        // 1 Stage = kStage (with stage name), 0 or >1 = kGlobal
        AssetScope ComputeScope(const Dia::Core::StringCRC& assetId,
                                const RelationshipIndex& relationships,
                                Dia::Core::StringCRC& outStageName) const;

        // Batch-update all records in the registry
        unsigned int RecomputeAllScopes(AssetRegistry& registry,
                                        RelationshipIndex& relationships) const;
    };
}
```

### RelationshipInferrer

```cpp
namespace Dia::AssetCatalogue
{
    class RelationshipInferrer
    {
    public:
        // Walk the record's TypeDefinition, find TypeVariableAttributeAssetReference fields,
        // return inferred "uses" edges
        void InferRelationships(const AssetRecord& record,
                                const AssetTypeRegistry& typeRegistry,
                                Dia::Core::Containers::DynamicArrayC<RelationshipEdge, 16>& outEdges) const;
    };
}
```

### CatalogueRulesEngine

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

### Rules File Schema (`assets.rules.json`)

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

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement ContentHasher | CRC32 computation for single files via DiaCore CRC. `ComputeDirectoryHash` for Folder assets: enumerate files in directory, sort by relative path, compute per-file CRC32, combine into single hash. |
| 2 | Implement ScopeComputer | Query RelationshipIndex for `contains` edges where target is each asset. Count distinct Stage sources. 1 Stage = `kStage` + stage name; 0 or >1 = `kGlobal`. `RecomputeAllScopes` batch iterates all records and updates scope/stage name. |
| 3 | Implement RelationshipInferrer | Walk TypeDefinition of a record's asset type. Find fields with `TypeVariableAttributeAssetReference` (Feature 2). Read StringCRC values from the source data, resolve to target asset IDs, emit `uses` RelationshipEdge entries. |
| 4 | Implement Rule matching engine | Internal `Rule` struct (name, match criterion, action type, action params). Match evaluation: `type`, `source_path_glob`, `tag`, `stage_ref_count` (min/max), `all`. Glob matching for source paths. |
| 5 | Implement CatalogueRulesEngine::LoadRules | Parse JSON via DiaCore JSON. Schema validation: required fields per action type, known action strings, valid match criteria keys. Referential validation: type IDs against AssetTypeRegistry, stage IDs in `type.name` format. Return LoadResult with all errors (no early exit). Reject entirely if invalid. |
| 6 | Implement CatalogueRulesEngine::EvaluateDryRun | Snapshot-based evaluation: iterate rules in array order, match against records, compute proposed changes. Build RuleChangeset with record ID, field, old/new values, rule name. No registry mutation. |
| 7 | Implement conflict detection | During changeset construction, detect when two rules assign different values to the same field on the same record. Flag both RuleChange entries with `mIsConflict = true`, increment `mConflictCount`. Last-rule-wins for Apply resolution. |
| 8 | Implement CatalogueRulesEngine::Apply | Same evaluation logic as dry-run but applies mutations to registry and relationship index after changeset is built. Returns the changeset. |
| 9 | Wire action types to automation classes | `infer_references` action delegates to RelationshipInferrer. `compute_scope` action delegates to ScopeComputer. `assign_tag`, `assign_scope`, `assign_stage` directly mutate records/relationships. |
| 10 | Add GoogleTest coverage | Tests for: ContentHasher single file and directory hash, ScopeComputer single and batch, RelationshipInferrer with typed fields, LoadRules valid and invalid schemas, EvaluateDryRun each match criterion, EvaluateDryRun each action type, conflict detection, Apply mutates registry, rules additive (no deletion), empty rules file, snapshot semantics (rules don't see same-pass mutations) |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetCatalogue -- JSON Definition Loader (Feature 1) | LoadResult/LoadError types for rules engine load errors |
| DiaAssetCatalogue -- Asset Type Framework (Feature 2) | AssetTypeRegistry for type ID validation in rules, AssetTypeDescriptor for TypeDefinition access, TypeVariableAttributeAssetReference for relationship inference |
| DiaAssetCatalogue -- Identity & Relationship Backbone (Feature 3) | AssetRegistry for record storage and queries, RelationshipIndex for forward/reverse ref queries and mutation, AssetRecord/RelationshipEdge/AssetScope types |
| DiaCore TypeSystem | TypeDefinition for field introspection, TypeVariable for attribute walking |
| DiaCore CRC/StringCRC | Content hashing (CRC32), asset IDs, rule name IDs |
| DiaCore Containers | DynamicArrayC (changesets, edge lists, match results), String64 (rule names, field names) |
| DiaCore FilePath | Source file paths for hashing, directory enumeration for Folder assets |
| DiaCore JSON | Reading and parsing assets.rules.json |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetCatalogue/ContentHasher.h` | Create -- content hash computation header |
| `Dia/DiaAssetCatalogue/ContentHasher.cpp` | Create -- single file and directory hash implementation |
| `Dia/DiaAssetCatalogue/ScopeComputer.h` | Create -- scope computation header |
| `Dia/DiaAssetCatalogue/ScopeComputer.cpp` | Create -- scope computation implementation |
| `Dia/DiaAssetCatalogue/RelationshipInferrer.h` | Create -- relationship inference header |
| `Dia/DiaAssetCatalogue/RelationshipInferrer.cpp` | Create -- relationship inference implementation |
| `Dia/DiaAssetCatalogue/CatalogueRulesEngine.h` | Create -- rules engine header (RuleChange, RuleChangeset, CatalogueRulesEngine) |
| `Dia/DiaAssetCatalogue/CatalogueRulesEngine.cpp` | Create -- rules engine implementation (load, validate, dry-run, apply, conflict detection) |
| `Dia/DiaAssetCatalogue/DiaAssetCatalogue.vcxproj` | Modify -- add new files |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Record IDs, rule names, tag IDs, stage IDs, relationship types, and field paths in RuleChange are all StringCRC or StringCRC-derived. Content hashing uses CRC32 via the same CRC infrastructure. |
| PD-002 | ProcessingUnit/Phase/Module | **Not applicable.** Pure automation logic with no lifecycle involvement. Called by editor or pipeline on demand. |
| PD-003 | Component-based entities | **Compliant.** RelationshipInferrer discovers component references via TypeVariableAttributeAssetReference on Entity Definition types. Component instantiation is DiaAssetRuntime's concern. |
| PD-004 | No STL in public APIs | **Compliant.** DynamicArrayC for changesets and edge lists, String64 for field/rule names. No STL in any public interface. |
| PD-005 | x64 Windows only | **Compliant.** File enumeration for directory hashing uses Win64 APIs via DiaCore FilePath. No cross-platform considerations. |
| PD-007 | C++20 required | **Compliant.** Available but no specific C++20 features required by this feature. |
| PD-008 | Directory.Build.props | **Compliant.** DiaAssetCatalogue.vcxproj inherits centralized settings. No overrides. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** This feature does not generate output files. Rules files are hand-authored input. |
| AD-001 | YAML frontmatter module docs | **Compliant.** DiaAssetCatalogue module doc already created in Feature 1. No new module created. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::\<Module\>:: | **Compliant.** All code under `Dia::AssetCatalogue::` namespace. |
| AD-005 | Component-based entities | **Compliant.** Same as PD-003. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | ContentHasher | Should `ComputeHash` read the entire file into memory or stream in chunks? | Read entire file. Source assets are small (textures, JSON configs) — streaming adds complexity without benefit at this scale. If profiling shows memory pressure from large files, chunked streaming can be added as an internal optimization without API changes. |
| 2 | ContentHasher | How does `ComputeDirectoryHash` combine per-file CRC32 values into a single hash? | Feed each file's CRC32 (as 4 bytes, little-endian) into a running CRC32 computation over the concatenated per-file hashes. Sorting by relative path ensures determinism regardless of filesystem enumeration order. |
| 3 | ScopeComputer | Should ScopeComputer skip Stage records themselves when computing scope? | Yes. Stage records are containers, not contained assets. A Stage referencing another Stage via `contains` is a structural relationship between Stages — the child Stage's scope is not meaningful in the global/stage sense. ScopeComputer operates on non-Stage records only. |
| 4 | ScopeComputer | An asset referenced by 0 Stages gets `kGlobal`. Should it instead get a separate "unassigned" scope? | No. `kGlobal` is correct for unassigned assets — they are available everywhere by default. Adding a third scope state adds complexity to every consumer for a distinction that doesn't affect deployment behavior. The editor can flag 0-Stage assets separately using a RelationshipIndex query. |
| 5 | RelationshipInferrer | Does RelationshipInferrer need the source file's deserialized data, or just its TypeDefinition? | It needs the deserialized data to read the actual StringCRC values from reference fields. The TypeDefinition tells it which fields are references; the instance data tells it what those references point to. The caller must provide a way to access field values — either via the AssetRecord's source path (load and deserialize) or via a pre-loaded TypeInstance. For v1, the caller passes a loaded object and its TypeDefinition. |
| 6 | Rules Engine | Should rules support multiple match criteria (AND composition), or strictly one per rule? | One per rule for v1. AND composition can be achieved by narrowing with path globs. Keeping match criteria singular makes rules simple to reason about, debug, and serialize. Multi-criteria matching can be added later if needed without breaking the schema (add a `"match_all"` array key). |
| 7 | Rules Engine | What happens if `assign_stage` references a Stage ID that doesn't exist in the registry? | The rule evaluation skips that record with a warning in the changeset (a RuleChange with `mField = "relationship"`, `mNewValue = <stage_id>`, and `mIsConflict = false`). The changeset includes a human-readable note. Missing Stages are not hard errors — the Stage may be added later. LoadRules validates format (`type.name`) but not existence, since the registry may not be fully populated at load time. |
| 8 | Rules Engine | The spec says rules evaluate against pre-pass state (snapshot semantics). How is this implemented? | Before evaluation, the engine takes a lightweight snapshot of the fields that rules can modify (scope, tags, stage membership). Each rule's match criteria and old-value lookups use this snapshot. Mutations accumulate in the changeset. After all rules have been evaluated, Apply writes the final values (last-rule-wins for conflicts) to the live registry. This avoids rule ordering surprises where early rules change state that later rules match on. |
| 9 | Rules Engine | RuleChangeset has a fixed capacity of 256 changes. Is that sufficient? | For a small game with ~100 assets and ~10 rules, 256 is generous. If the changeset fills up, evaluation stops and the changeset includes a truncation flag so the caller knows results are incomplete. A future version can use a larger capacity or a heap-backed container if needed. |
| 10 | Conflict Detection | Should conflicts block Apply, or just warn? | Warn only (SD-CAT-010). Last-rule-wins is the resolution. The conflict count in the changeset and per-change `mIsConflict` flags give the caller (editor) enough information to surface warnings to the user. Blocking would make rule authoring frustrating — the user may intentionally override an earlier rule with a later one. |

## Status

`Approved`

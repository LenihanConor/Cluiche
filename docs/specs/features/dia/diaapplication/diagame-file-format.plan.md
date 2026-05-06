# Plan: DiaGame File Format and Typed Imports

**Spec:** @docs/specs/features/dia/diaapplication/diagame-file-format.md  
**Status:** Done  
**Started:** 2026-05-06  
**Last Updated:** 2026-05-06

## Implementation Patterns

### TypedImport struct
New enum + struct in a new header `Dia/DiaApplication/Manifest/TypedImport.h`. The enum uses `kManifest` and `kStage` values. The struct holds `String256 path` and `ImportType type`. This is a simple POD used in both `ApplicationManifest::imports` and `DiaGameManifest::imports`.

### DiaGameManifest / DiaStageManifest structs
New header `Dia/DiaApplication/Manifest/DiaGameManifest.h` defines both structs. `DiaGameManifest` has `name`, `version` (String256), typed imports array, and a `DiaGameConfig` sub-struct. `DiaStageManifest` has `name` (String256) and `manifestPath` (String256).

### DiaGameManifestLoader
New class in `Dia/DiaApplication/Manifest/DiaGameManifestLoader.h/.cpp`. Static methods: `LoadGameFile(path, outGameManifest)` and `LoadStageFile(path, outStageManifest)`. Uses jsoncpp for parsing. Returns `ManifestValidationResult`. Follows the same error accumulation pattern as `ApplicationManifestLoader`.

### ApplicationManifest imports migration
`ApplicationManifest.h` changes `DynamicArrayC<const char*, 16> imports` → `DynamicArrayC<TypedImport, 16> imports`. This is a breaking internal change. All sites reading/writing imports must be updated: `JsonApplicationManifestSerializer`, `ManifestComposer`, `ManifestSerializer` (editor), `HandleAddImport`/`HandleRemoveImport` (editor).

### ManifestComposer extensions
Add `ComposeFromGameFile(diagamePath, outManifest)` which: (1) parses .diagame, (2) iterates typed imports, (3) routes type=manifest to existing `ResolveImportsRecursive`, (4) routes type=stage through `.diastage` → load stage `.diaapp` → Phase C merge algorithm. Also update existing import resolution to handle `TypedImport` objects.

### ApplicationLoader extensions
Add `LoadFromGameFile(registry, diagamePath, outResult)` which calls composer's `ComposeFromGameFile`, validates, and instantiates the root PU. Keep existing `LoadApplication` for backward compat.

### Editor changes
`DiaApplicationEditor::OpenManifest` detects `.diagame` extension → routes to new `OpenDiaGame` method. Composed manifest serialized and pushed to UI as before. TreeView already handles the hierarchy; stage badges use `_source` field and stage name from metadata.

### Data migration
Update `cluiche_main.diaapp` imports to typed format. Create `cluichetest.diagame`, `stages/dummy_stage.diastage`, and `stages/dummy_stage.diaapp`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `TypedImport.h` with TypedImport struct and ImportType enum | — | Done | haiku | Simple struct, no test needed |
| 2 | Create `DiaGameManifest.h` with DiaGameManifest, DiaStageManifest, DiaGameConfig structs | — | Done | haiku | Simple structs |
| 3 | Migrate `ApplicationManifest.imports` from `const char*` to `TypedImport` | TestManifestSerializer (existing tests still pass) | Done | sonnet | Breaking change: update all readers/writers |
| 4 | Update `JsonApplicationManifestSerializer` to parse/write typed imports | TestManifestSerializer | Done | sonnet | Parse object array + backward-compat flat strings |
| 5 | Update `ManifestComposer` import resolution to use `TypedImport.path` | Existing compose tests still pass | Done | sonnet | Internal change — type routing comes in task 8 |
| 6 | Update editor `ManifestSerializer` to serialize/deserialize typed imports | TestManifestSerializer (editor) | Done | sonnet | Editor serializer writes `{path, type}` |
| 7 | Update editor `HandleAddImport`/`HandleRemoveImport` for TypedImport | — | Done | sonnet | String comparison on path field |
| 8 | Create `DiaGameManifestLoader.h/.cpp` — parse .diagame and .diastage files | TestDiaGameFormat tests 1-4 | Done | sonnet | New class, static Load methods |
| 9 | Add `ComposeFromGameFile` to ManifestComposer — typed import routing + stage merge | TestDiaGameFormat tests 5-7 | Done | opus | Core composition logic, stage routing |
| 10 | Add stage merge validation (duplicate IDs, missing target PU) | TestDiaGameFormat tests 8-9 | Done | sonnet | Implemented in MergeStageManifest |
| 11 | Add `LoadFromGameFile` to ApplicationLoader | TestDiaGameFormat test 13 | Done | sonnet | Thin wrapper over composer + instantiate |
| 12 | Update DiaApplicationEditor to detect and open .diagame files | — | Done | sonnet | Extension check in OpenManifest, routes to ComposeFromGameFile |
| 13 | Create CluicheTest data files: `cluichetest.diagame`, `stages/dummy_stage.diastage`, `stages/dummy_stage.diaapp` | — | Done | haiku | Data files per spec examples |
| 14 | Migrate `cluiche_main.diaapp` imports to typed format | — | Done | haiku | Replace flat strings with objects |
| 15 | Create `TestDiaGameFormat.cpp` — unit tests for new loaders and composition | All AC unit tests | Done | sonnet | 16 tests all passing |
| 16 | Update vcxproj files (DiaApplication, GoogleTests) with new source files | Build succeeds | Done | haiku | Add headers and .cpp files |
| 17 | Build + run GoogleTests to verify all pass | dia run googletest | Done | haiku | 58 manifest tests pass, full suite 4272/4273 (1 flaky pre-existing) |
| 18 | TreeView stage badge display | Visual verification | Done | sonnet | Green badge with stage filename on phases/PUs with _source |

## Session Notes

### 2026-05-06
- Spec approved, plan created
- Key risk: task 3 (imports migration) is a breaking change with wide blast radius — touches serializer, composer, editor, and all test fixtures that reference imports
- Task 9 (ComposeFromGameFile) depends on Phase C's stage merge algorithm existing or being stubbed
- Tasks 1-17 completed. Added backward-compat for flat-string imports in parser (existing tests pass unchanged). Fixed stage merge — stage_phases/transitions/modules are top-level JSON keys folded into metadata during Load. Pipeline updated to deploy .diagame/.diastage files. 16 new DiaGameFormat tests all pass.
- Task 18 (TreeView stage badge) completed — green badge shows source filename on phases/PUs from external files. All 18 tasks done.

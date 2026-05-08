# Feature Spec: DiaGame File Format and Typed Imports

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplication | @docs/specs/systems/dia/diaapplication.md |
| Feature | **DiaGame File Format and Typed Imports** | (this document) |

**Status:** `Done`

**Depends on:** @docs/specs/features/dia/diaapplication/manifest-imports.md (Phase A — import resolution)
**Supersedes (partial):** The import format in @docs/specs/features/dia/diaapplication/stage-manifests.md — flat string imports replaced by typed import objects; `.diastage` wrapper introduced.

---

## Problem Statement

The application has no single project root file — stages are injected via code, imports are flat strings with no type discrimination, and game-level config (asset root, default level) has nowhere to live. The manifest loader cannot distinguish between a PU-adding import and a stage-injecting import without inspecting file contents, and there is no natural place for game-wide metadata.

---

## Solution Overview

Introduce three file format changes:

1. **`.diagame`** — a top-level project root file declaring game metadata, typed imports (manifests and stages), and game-wide config. This is the entry point for both the editor and runtime.
2. **`.diastage`** — a thin metadata wrapper pointing to a `.diaapp` that contains the stage's phase/module/transition injections. Stage-specific config (future: file loading, activation conditions) lives here.
3. **Typed imports** — both `.diagame` and `.diaapp` use `{ "path": "...", "type": "manifest" | "stage" }` objects instead of flat strings. The `type` field tells the composer how to process the import (add PUs vs. merge into existing PUs).

### Key Design Points

1. **`.diagame` is the project root** — ApplicationLoader and DiaApplicationEditor both accept `.diagame` as their entry point. The `.diagame` references a root `.diaapp` (type: manifest) and any number of stages (type: stage).
2. **`.diastage` is metadata only** — it names the stage and points to a `.diaapp`. Entry/exit phase connections are derived from the transitions declared in that `.diaapp`. Future fields (file loading config, activation conditions) go here without polluting the `.diaapp` format.
3. **`.diaapp` format stays pure** — always describes PU/phase/module/transition structure. Stage `.diaapp` files use the same schema as Phase C (`stage_phases`, `stage_transitions`, `stage_modules`, `metadata.type == "stage"`). The only change is the import field format.
4. **Import format unified** — `{ "path": "...", "type": "manifest" }` for PU-adding imports, `{ "path": "...", "type": "stage" }` for stage imports. Used identically in `.diagame` and `.diaapp`.
5. **Backward compatible** — `.diaapp`-only workflow (no `.diagame`) still works for editor and runtime. The loader detects whether the entry file is `.diagame` or `.diaapp` and adjusts accordingly.

---

## File Format Definitions

### `.diagame` Schema

```json
{
  "name": "CluicheTest",
  "version": "0.1",
  "imports": [
    { "path": "cluiche_main.diaapp", "type": "manifest" },
    { "path": "dummy_stage.diastage", "type": "stage" }
  ],
  "config": {
    "asset_root": "Data/Assets",
    "default_level": "dummy_stage"
  }
}
```

### `.diastage` Schema

```json
{
  "name": "DummyStage",
  "manifest": "stages/dummy_stage.diaapp"
}
```

### `.diaapp` Typed Imports (updated format)

```json
{
  "version": 1,
  "imports": [
    { "path": "cluiche_sim.diaapp", "type": "manifest" },
    { "path": "cluiche_render.diaapp", "type": "manifest" }
  ],
  "processing_units": [ ... ]
}
```

### Hierarchy

```
.diagame              — game config, typed imports (manifests + stages)
  ├── .diaapp         — root manifest (PU definitions, may import child .diaapps)
  │     └── .diaapp   — child PU manifests (RenderPU, SimPU)
  └── .diastage       — stage metadata wrapper
        └── .diaapp   — stage content (phases/modules to merge into existing PUs)
```

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `.diagame` file format defined with `name`, `version`, typed `imports` array, and `config` object | Unit test: parse valid `.diagame`, verify all fields accessible |
| AC2 | Typed imports use `{ "path": "...", "type": "manifest" \| "stage" }` shape in both `.diagame` and `.diaapp` | Unit test: parse imports array, verify path and type fields present |
| AC3 | `.diaapp` imports field migrated from flat string array to typed import format | Unit test: load updated `.diaapp` with typed imports; old flat-string format accepted with deprecation warning |
| AC4 | `.diastage` file format defined with `name` and `manifest` pointer to a `.diaapp` | Unit test: parse valid `.diastage`, verify name and manifest path |
| AC5 | Stage `.diaapp` merges phases/modules/transitions into existing PUs by matching `instance_id` (per Phase C merge algorithm) | Unit test: root manifest + stage import → merged result has stage phases in target PU |
| AC6 | Entry/exit phase connections derived from transitions declared in the stage's `.diaapp` | Unit test: stage `.diaapp` declares transition from MainBootStrapPhase → StagePhase; after merge, transition exists in target PU |
| AC7 | `ApplicationLoader` can load a `.diagame` as entry point — resolves root manifest + all stages | Integration test: load `.diagame`, verify full composed manifest includes root PUs and stage phases |
| AC8 | `DiaApplicationEditor` can open a `.diagame` file as entry point | Integration test: editor opens `.diagame`, tree view shows full composed hierarchy |
| AC9 | Editor composes full PU tree: root manifest imports + stage merges | Unit test: editor receives composed manifest with PU hierarchy and stage-contributed phases |
| AC10 | Stage-contributed phases/modules appear in tree view with stage source attribution | Visual test: tree shows stage phases with `.diastage` name badge |
| AC11 | Existing `.diaapp`-only workflow still works (editor and runtime — no `.diagame` required) | Integration test: open `.diaapp` directly in editor; load `.diaapp` via ApplicationLoader; both work |
| AC12 | Manifest validation rejects: duplicate phase instance IDs after stage merge | Unit test: stage and root both define phase with same ID → error |
| AC13 | Manifest validation rejects: stage targeting non-existent PU | Unit test: stage targets "NonExistentPU" → error with clear message |
| AC14 | CluicheTest manifests migrated to new format (`.diagame` + `.diastage` for DummyStage) | Files exist; `dia run cluichetest` succeeds |

---

## Public API

### New: DiaGameManifest struct (Manifest/DiaGameManifest.h)

```cpp
namespace Dia::Application {

    struct TypedImport {
        Dia::Core::Containers::String256 path;
        enum class ImportType { kManifest, kStage } type;
    };

    struct DiaGameConfig {
        Dia::Core::Containers::String256 assetRoot;
        Dia::Core::Containers::String256 defaultLevel;
    };

    struct DiaGameManifest {
        Dia::Core::Containers::String256 name;
        Dia::Core::Containers::String256 version;
        Dia::Core::Containers::DynamicArrayC<TypedImport, 16> imports;
        DiaGameConfig config;
    };
}
```

### New: DiaStageManifest struct (Manifest/DiaStageManifest.h)

```cpp
namespace Dia::Application {

    struct DiaStageManifest {
        Dia::Core::Containers::String256 name;
        Dia::Core::Containers::String256 manifestPath;  // points to the .diaapp
    };
}
```

### Modified: ApplicationManifest imports (Manifest/ApplicationManifest.h)

```cpp
// Before:
// DynamicArrayC<const char*, 8> imports;

// After:
DynamicArrayC<TypedImport, 8> imports;
```

### Modified: ApplicationLoader (Loader/ApplicationLoader.h)

```cpp
namespace Dia::Application {

class ApplicationLoader {
public:
    // Existing: load from .diaapp (backward compat)
    static ProcessingUnit* LoadApplication(ApplicationTypeRegistry& registry,
                                           const char* manifestPath);

    // New: load from .diagame — resolves root manifest + stages, builds full topology
    static ProcessingUnit* LoadFromGameFile(ApplicationTypeRegistry& registry,
                                            const char* diagamePath,
                                            ManifestValidationResult& outResult);

    // New: parse .diagame file only (no PU construction — for editor use)
    static ManifestValidationResult LoadGameManifest(const char* diagamePath,
                                                     DiaGameManifest& outManifest);

    // New: parse .diastage file
    static ManifestValidationResult LoadStageManifest(const char* diastagePath,
                                                      DiaStageManifest& outStage);
};

}
```

### Modified: ManifestComposer (Manifest/ManifestComposer.h)

```cpp
namespace Dia::Application {

class ManifestComposer {
public:
    // Existing: compose from .diaapp with old flat imports
    ManifestValidationResult ComposeSingleManifest(const char* path, ApplicationManifest& out);

    // New: compose from .diagame — resolves typed imports, merges stages
    ManifestValidationResult ComposeFromGameFile(const char* diagamePath, ApplicationManifest& out);

    // New: compose from typed import list (shared logic for .diagame and .diaapp)
    ManifestValidationResult ComposeTypedImports(const DynamicArrayC<TypedImport, 16>& imports,
                                                  const char* basePath,
                                                  ApplicationManifest& out);
};

}
```

---

## Implementation Notes

### Load Flow: `.diagame` Entry Point

```
LoadFromGameFile(registry, "cluichetest.diagame"):
1. Parse .diagame → DiaGameManifest (name, version, imports, config)
2. For each import in DiaGameManifest.imports:
   a. If type == "manifest":
      - Resolve path relative to .diagame
      - Load .diaapp via existing manifest loader (recursively resolves its own typed imports)
      - Add PUs to composed manifest
   b. If type == "stage":
      - Resolve path to .diastage file
      - Parse .diastage → DiaStageManifest (name, manifest path)
      - Load the stage's .diaapp (contains stage_phases, stage_transitions, etc.)
      - Merge stage content into composed manifest (Phase C merge algorithm)
      - Set sourceManifestPath and stage name on merged entries
3. Validate composed manifest (duplicate IDs, missing targets, requires_phases)
4. Build PU tree from composed manifest (Phase B)
5. Return root PU
```

### Editor Flow: `.diagame` Entry Point

```
DiaApplicationEditor::OpenDiaGame(path):
1. Parse .diagame → DiaGameManifest
2. Resolve and compose all imports (same as loader step 2)
3. Serialize composed manifest to JSON (ManifestSerializer::Serialize)
4. Push to UI as "manifest_loaded" with additional game metadata
5. UI TreeView renders full hierarchy with stage badges
```

### Migration: Flat Strings → Typed Imports

The existing flat-string import format:
```json
{ "imports": ["cluiche_sim.diaapp", "cluiche_render.diaapp"] }
```

Must be migrated to:
```json
{ "imports": [
    { "path": "cluiche_sim.diaapp", "type": "manifest" },
    { "path": "cluiche_render.diaapp", "type": "manifest" }
] }
```

The loader accepts old-format imports with a deprecation warning logged, directing the user to update the format. This preserves backward compatibility during migration.

### CluicheTest Migration

New files:
- `Cluiche/CluicheTest/Data/Manifests/cluichetest.diagame`
- `Cluiche/CluicheTest/Data/Manifests/stages/dummy_stage.diastage`
- `Cluiche/CluicheTest/Data/Manifests/stages/dummy_stage.diaapp` (stage content — from Phase C)

Updated files:
- `Cluiche/CluicheTest/Data/Manifests/cluiche_main.diaapp` — imports changed to typed format
- `Cluiche/CluicheTest/Data/Manifests/cluiche_sim.diaapp` — no changes (no imports)
- `Cluiche/CluicheTest/Data/Manifests/cluiche_render.diaapp` — no changes (no imports)

---

## Dependencies

### Required Modules
- **DiaApplication/Manifest** — manifest loader, composer, validator, serializer
- **DiaCore/Json** — JSON parsing for new file formats
- **DiaCore/Containers** — String256, DynamicArrayC for struct storage

### Required Features
- **Manifest Imports (Phase A)** — import resolution and sourceManifestPath provenance
- **Stage Manifests (Phase C)** — stage merge algorithm (stage_phases, stage_transitions, stage_modules, requires_phases)

### Dependent Features
- **PU Parent-Child Tree (Phase B)** — composed manifest feeds into tree construction
- **Editor Tree View** — displays composed hierarchy with stage badges

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestDiaGameFormat.cpp)

1. **Parse .diagame** — valid file parsed, all fields accessible
2. **Parse .diastage** — valid file parsed, name and manifest path extracted
3. **Typed imports in .diaapp** — imports array parsed with path and type fields
4. **Deprecate old flat-string imports** — old format accepted with deprecation warning (backward compat)
5. **ComposeFromGameFile** — .diagame with 1 manifest + 1 stage import → composed result has PUs from manifest and stage phases merged
6. **Stage merge via typed import** — type=stage triggers stage merge (not PU addition)
7. **Manifest import via typed import** — type=manifest adds PUs (not merge)
8. **Duplicate phase ID across stage and root** — rejected with error
9. **Stage targets non-existent PU** — rejected with error
10. **Missing .diastage manifest field** — parse error
11. **Missing .diagame imports field** — parse succeeds (no imports = empty app)
12. **Relative path resolution** — paths resolved relative to .diagame / .diaapp location
13. **Backward compat: LoadApplication with .diaapp** — still works, no .diagame required

### Integration Tests

14. **CluicheTest end-to-end** — load via .diagame, application runs, DummyStage accessible
15. **Editor opens .diagame** — tree view shows full composed hierarchy
16. **Editor opens .diaapp directly** — backward compat, works as before

---

## Files Affected

### New Headers
- `Dia/DiaApplication/Manifest/DiaGameManifest.h` — DiaGameManifest, DiaStageManifest, TypedImport structs
- `Dia/DiaApplication/Manifest/DiaGameManifestLoader.h` — parse .diagame and .diastage files

### New Implementation
- `Dia/DiaApplication/Manifest/DiaGameManifestLoader.cpp`

### Modified
- `Dia/DiaApplication/Manifest/ApplicationManifest.h` — imports field type change (flat string → TypedImport)
- `Dia/DiaApplication/Manifest/ApplicationManifestLoader.h/.cpp` — support typed imports during resolution
- `Dia/DiaApplication/Manifest/ManifestComposer.h/.cpp` — ComposeFromGameFile, ComposeTypedImports
- `Dia/DiaApplication/Loader/ApplicationLoader.h/.cpp` — LoadFromGameFile, LoadGameManifest, LoadStageManifest
- `Dia/DiaApplicationEditor/DiaApplicationEditor.h/.cpp` — OpenDiaGame entry point, detect file type on open
- `Dia/DiaApplicationEditor/ManifestSerializer.h/.cpp` — serialize typed imports
- `Dia/DiaApplicationEditor/UI/src/TreeView.tsx` — stage badge display
- `Dia/DiaApplicationEditor/UI/src/types.ts` — stage metadata types
- `Dia/DiaApplicationEditor/UI/src/ManifestStore.ts` — game manifest state

### New Data Files
- `Cluiche/CluicheTest/Data/Manifests/cluichetest.diagame`
- `Cluiche/CluicheTest/Data/Manifests/stages/dummy_stage.diastage`
- `Cluiche/CluicheTest/Data/Manifests/stages/dummy_stage.diaapp`

### Updated Data Files
- `Cluiche/CluicheTest/Data/Manifests/cluiche_main.diaapp` — typed imports

### New Tests
- `Cluiche/Tests/GoogleTests/Application/TestDiaGameFormat.cpp`

### Modified Project Files
- `Dia/DiaApplication/DiaApplication.vcxproj` + filters — new headers/impl
- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` + filters — new test file

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all IDs | **Compliant** — stage names, PU references, phase IDs all stored as StringCRC internally. JSON uses string representation parsed into StringCRC. |
| PD-002 | Platform | PU/Phase/Module architecture | **Compliant** — .diagame sits above the PU hierarchy as a project-level container. It does not introduce a new architectural level — it's a composition file that feeds into the existing PU/Phase/Module system. |
| PD-004 | Platform | No STL in public APIs | **Compliant** — DiaGameManifest, DiaStageManifest, TypedImport all use Dia containers (String256, DynamicArrayC). LoadFromGameFile takes `const char*`. |
| PD-005 | Platform | x64 only | **Compliant** — no platform-specific code. |
| PD-006 | Platform | VS project files source of truth | **Compliant** — all new files added to vcxproj. |
| PD-007 | Platform | C++20 required | **Compliant** — no special language features required. |
| PD-008 | Platform | Directory.Build.props owns build settings | **Compliant** — no build setting changes. |
| PD-009 | Platform | Output under Cluiche/out/ | **Compliant** — no output path changes. |
| AD-001 | Dia App | Module docs with YAML frontmatter | **Compliant** — update DiaApplication architecture doc to reflect new file formats. |
| AD-002 | Dia App | No STL in public APIs | **Compliant** — see PD-004. |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | **Compliant** — all new code in Dia::Application:: namespace. |
| SD-001 | DiaApplication | PU/Phase/Module three-level hierarchy | **Compliant** — .diagame composes into the three-level hierarchy. No new runtime level. |
| SD-002 | DiaApplication | StateObject base with state machine | **Compliant** — no changes to state machine. |
| SD-004 | DiaApplication | Modules identified by StringCRC | **Compliant** — unchanged. |
| SD-006 | DiaApplication | Raw pointer and UniquePtr ownership | **Compliant** — loader returns raw pointer (caller owns); internal structs use value types. |
| SD-010 | DiaApplication | Explicit dependencies via AddDependancy() | **Compliant** — no change to dependency model. |
| DAED-001 | DiaApplicationEditor | Reuse ApplicationManifestValidator | **Compliant** — validation runs on the composed manifest using existing validator. |
| DAED-008 | DiaApplicationEditor | Manual save workflow | **Compliant** — opening .diagame follows same dirty/save pattern as .diaapp. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Format | Should `.diagame` support typed imports of type "stage" directly, or should stages only be reachable through a `.diaapp` that imports them? | `.diagame` supports stages directly. Stages are a game-level concept, not a PU-structure concept, so they belong at the `.diagame` level. A `.diaapp` can also import stages if needed (e.g., a sub-manifest that brings its own stages). |
| 2 | Backward Compat | Should the loader support old flat-string imports during a migration period, or hard-reject them immediately? | Accept with deprecation warning. Hard-reject would break existing test infrastructure and in-progress manifests. The warning logs a clear migration message. Flat strings are treated as type=manifest. |
| 3 | Relative Paths | How are import paths resolved — relative to the importing file, or relative to a project root? | Relative to the importing file. This matches filesystem conventions and allows manifests to be relocated as a group. |
| 4 | Config Extensibility | Should `.diagame` config be a fixed struct or freeform JSON? | Fixed struct with known fields (asset_root, default_level). Unknown fields are preserved but not interpreted. This allows forward compatibility without runtime schema validation overhead. |
| 5 | Editor Detection | How does the editor know whether the user opened a `.diagame` vs a `.diaapp`? | File extension. OpenManifest checks extension: `.diagame` routes to OpenDiaGame; `.diaapp` routes to existing flow. The Open File dialog shows both types. |
| 6 | Phase C Relationship | This spec introduces `.diastage` but Phase C uses `.diaapp` with metadata.type="stage". How do these reconcile? | `.diastage` is a thin metadata pointer. It references a `.diaapp` that uses Phase C's stage schema (metadata.type="stage", stage_phases, etc.). Phase C's merge algorithm is unchanged — this spec only adds the wrapper and the typed import system that routes to it. |
| 7 | Multiple Root Manifests | Can a `.diagame` import multiple manifests of type "manifest"? | Yes — they're composed in import order. Each adds its PUs. Duplicate PU instance IDs across manifests are rejected by the validator. Typically only one is the root (has `root: true`). |
| 8 | Stage without .diastage | Can a stage `.diaapp` still be imported directly (without a `.diastage` wrapper) via `{ "type": "stage" }` pointing to the `.diaapp`? | No — type="stage" must point to a `.diastage` file. This keeps the indirection clean and ensures stage metadata always has a home. If you need a quick stage, the `.diastage` is two lines of JSON. |

---

## Examples

### Example 1: Full CluicheTest Setup

```json
// cluichetest.diagame
{
  "name": "CluicheTest",
  "version": "0.1",
  "imports": [
    { "path": "cluiche_main.diaapp", "type": "manifest" },
    { "path": "stages/dummy_stage.diastage", "type": "stage" }
  ],
  "config": {
    "asset_root": "Data/Assets",
    "default_level": "dummy_stage"
  }
}
```

```json
// cluiche_main.diaapp
{
  "version": 1,
  "imports": [
    { "path": "cluiche_sim.diaapp", "type": "manifest" },
    { "path": "cluiche_render.diaapp", "type": "manifest" }
  ],
  "processing_units": [
    {
      "type_id": "MainProcessingUnit",
      "instance_id": "MainProcessingUnit",
      "root": true,
      "frequency_hz": 30.0,
      "dedicated_thread": false,
      "phases": [
        { "type_id": "MainBootPhase", "instance_id": "MainBootPhase" },
        { "type_id": "MainBootStrapPhase", "instance_id": "MainBootStrapPhase" }
      ],
      "transitions": [
        { "from": "MainBootPhase", "to": "MainBootStrapPhase" }
      ],
      "initial_phase": "MainBootPhase",
      "modules": [ ... ]
    }
  ]
}
```

```json
// stages/dummy_stage.diastage
{
  "name": "DummyStage",
  "manifest": "dummy_stage.diaapp"
}
```

```json
// stages/dummy_stage.diaapp
{
  "version": 1,
  "metadata": {
    "type": "stage",
    "name": "DummyStage",
    "requires_phases": ["MainBootStrapPhase"]
  },
  "stage_phases": [
    {
      "type_id": "DummyStage::MainLoadPhase",
      "instance_id": "DummyStage::MainLoadPhase",
      "target_processing_unit": "MainProcessingUnit"
    },
    {
      "type_id": "DummyStage::MainFEPhase",
      "instance_id": "DummyStage::MainFEPhase",
      "target_processing_unit": "MainProcessingUnit"
    }
  ],
  "stage_transitions": [
    { "from": "MainBootStrapPhase", "to": "DummyStage::MainLoadPhase", "target_processing_unit": "MainProcessingUnit" },
    { "from": "DummyStage::MainLoadPhase", "to": "DummyStage::MainFEPhase", "target_processing_unit": "MainProcessingUnit" },
    { "from": "DummyStage::MainFEPhase", "to": "MainBootStrapPhase", "target_processing_unit": "MainProcessingUnit" }
  ],
  "stage_modules": []
}
```

### Example 2: Composed Result (after loader processes .diagame)

```
PU Tree:
  MainProcessingUnit (root: true, from cluiche_main.diaapp)
    ├── SimProcessingUnit (from cluiche_sim.diaapp)
    └── RenderProcessingUnit (from cluiche_render.diaapp)

MainProcessingUnit phases (after stage merge):
  - MainBootPhase            (from cluiche_main.diaapp)
  - MainBootStrapPhase       (from cluiche_main.diaapp)
  - DummyStage::MainLoadPhase   (from stages/dummy_stage.diaapp, stage: DummyStage)
  - DummyStage::MainFEPhase     (from stages/dummy_stage.diaapp, stage: DummyStage)

MainProcessingUnit transitions (after stage merge):
  - MainBootPhase → MainBootStrapPhase
  - MainBootStrapPhase → DummyStage::MainLoadPhase   (stage entry)
  - DummyStage::MainLoadPhase → DummyStage::MainFEPhase
  - DummyStage::MainFEPhase → MainBootStrapPhase     (stage exit)
```

### Example 3: Editor Tree View Rendering

```
CluicheTest (cluichetest.diagame)
└── MainProcessingUnit
    ├── RenderProcessingUnit [cluiche_render.diaapp]
    │   └── RenderRunningPhase
    ├── SimProcessingUnit [cluiche_sim.diaapp]
    │   └── SimBootPhase, SimBootStrapPhase
    ├── MainBootPhase
    ├── MainBootStrapPhase
    ├── DummyStage::MainLoadPhase [DummyStage]
    └── DummyStage::MainFEPhase [DummyStage]
```

---

## Status

`Approved`

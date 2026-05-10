# Feature Spec: Manifest Imports

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplication.md |
| Feature | **Manifest Imports** | (this document) |

**Status:** `Done`

**Research:** @docs/research/diappl_flow_tree/summary.md (Phase A)

---

## Problem Statement

The `ApplicationManifest` struct has an `imports` field (`DynamicArrayC<const char*, 16>`) that is completely unused. Applications like CluicheTest must manually load and wire multiple .diaapp manifests in code (MainPU hard-codes `ApplicationLoader::LoadApplication()` calls for sim and render manifests in `PostPhaseStart()`). This makes the manifest topology invisible to the editor and prevents generic discovery of the full application structure.

---

## Solution Overview

Implement recursive import resolution in `ApplicationManifestLoader`. When loading a .diaapp file, the loader reads the `imports` array, recursively loads each referenced manifest, and merges all ProcessingUnit/Phase/Module entries into a single composed `ApplicationManifest`. Circular imports are detected via path tracking. Duplicate instance IDs across manifests are rejected. Each merged entry carries provenance metadata (source manifest path) for downstream editor visualization.

### Key Design Points

1. **Recursive resolution** — imports can reference files that themselves have imports (nested)
2. **Relative path resolution** — import paths resolved relative to the importing manifest's directory
3. **Flat merge** — imported PU entries are appended to the root manifest's `processingUnits` array; phase/module entries within a PU stay within that PU's entry
4. **Provenance tracking** — each merged entry stores which .diaapp file it originated from
5. **Backward compatible** — manifests with no imports load identically to current behavior

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | ApplicationManifestLoader recursively resolves `imports` entries when loading a .diaapp file | Unit test: manifest A imports B which imports C; all three PUs appear in result |
| AC2 | Imported manifests' ProcessingUnitEntries are merged into the parent manifest's `processingUnits` array | Unit test: root has 1 PU, import has 1 PU; merged result has 2 PUs |
| AC3 | Imported manifests' phase and module entries remain within their PU entry (not cross-merged) | Unit test: imported PU's phases stay in that PU's entry after merge |
| AC4 | Circular imports are detected and produce `kImportCycle` error (not crash/infinite loop) | Unit test: A imports B, B imports A; LoadFromFile returns kImportCycle with clear error message |
| AC5 | Duplicate PU instance IDs across manifests are detected and rejected at load time (`kDuplicateInstanceId`) | Unit test: root and import both define PU with same instance_id; load fails with error |
| AC6 | Import paths are resolved relative to the importing manifest's directory | Unit test: manifest at `Data/Manifests/main.diaapp` imports `sub/sim.diaapp`; loader resolves to `Data/Manifests/sub/sim.diaapp` |
| AC7 | CluicheTest's `cluiche_main.diaapp` is updated to import `cluiche_sim.diaapp` and `cluiche_render.diaapp` | Manual verification: load cluiche_main.diaapp, verify all 3 PUs present in merged manifest |
| AC8 | Existing single-file loading (no imports) continues to work unchanged | Unit test: manifest with empty/missing imports field loads identically to current behavior |
| AC9 | Merged entries preserve source manifest path (provenance) via `sourceManifestPath` field | Unit test: after merge, each PUEntry's `sourceManifestPath` matches the file it was loaded from |

---

## Public API

### Changes to ApplicationManifest (Manifest/ApplicationManifest.h)

```cpp
// Add to ProcessingUnitEntry:
struct ProcessingUnitEntry
{
    // ... existing fields ...
    Dia::Core::Containers::String256 sourceManifestPath;  // Path of .diaapp this entry was loaded from (empty if root)
    // Note: String256 (value semantics) is used instead of const char* to avoid lifetime/ownership
    // hazards when DynamicArrayC bitwise-copies entries during array growth.

    // ... existing methods ...
};

// Add to PhaseEntry:
struct PhaseEntry
{
    // ... existing fields ...
    Dia::Core::Containers::String256 sourceManifestPath;  // Provenance tracking
    // ...
};

// Add to ModuleEntry:
struct ModuleEntry
{
    // ... existing fields ...
    Dia::Core::Containers::String256 sourceManifestPath;  // Provenance tracking
    // ...
};
```

### Changes to ApplicationManifestLoader (Manifest/ApplicationManifestLoader.h)

```cpp
class ApplicationManifestLoader
{
public:
    // Existing method — enhanced to resolve imports
    // After parsing the root manifest's JSON, recursively loads all imports,
    // merges their entries, and validates the composed result.
    ManifestValidationResult LoadFromFile(const char* filePath, ApplicationManifest& outManifest);

    // Existing method — unchanged (no import resolution for string-based loading)
    ManifestValidationResult LoadFromString(const char* jsonString, ApplicationManifest& outManifest);

    // Existing method — now used internally; also available for explicit multi-file composition
    ManifestValidationResult ComposeManifests(
        const Dia::Core::Containers::DynamicArrayC<const char*, 16>& filePaths,
        ApplicationManifest& outComposedManifest);

private:
    // New: recursive import resolution
    ManifestValidationResult ResolveImports(
        const char* manifestPath,
        ApplicationManifest& outManifest,
        Dia::Core::Containers::DynamicArrayC<const char*, 32>& visitedPaths);

    // New: merge one manifest into another
    ManifestValidationResult MergeManifest(
        const ApplicationManifest& source,
        ApplicationManifest& target);

    // New: check for duplicate IDs across all PU entries
    bool HasDuplicateInstanceIds(const ApplicationManifest& manifest);
};
```

### Changes to JsonApplicationManifestSerializer

```cpp
// Load: parse "imports" array from JSON (already in schema, just unused)
// Save: write "imports" array and sourceManifestPath fields to JSON
// sourceManifestPath is written to metadata section (not core schema) for round-trip
```

### Changes to ManifestValidator

```cpp
// Existing kImportNotFound and kImportCycle result codes are now actively used.
// Add validation for cross-manifest duplicate instance IDs during composed validation.
```

---

## Implementation Notes

### Import Resolution Algorithm

```
ResolveImports(manifestPath, outManifest, visitedPaths):
    1. Check if manifestPath is in visitedPaths → kImportCycle error
    2. Add manifestPath to visitedPaths
    3. Parse JSON from manifestPath into localManifest
    4. Tag all localManifest entries with sourceManifestPath = manifestPath
    5. For each import in localManifest.imports:
        a. Resolve import path relative to manifestPath's directory
        b. ResolveImports(resolvedPath, importedManifest, visitedPaths)  // recurse
        c. MergeManifest(importedManifest, localManifest)
    6. Remove manifestPath from visitedPaths  // allow diamond imports (A→B→D, A→C→D)
    7. Copy localManifest into outManifest
```

### Diamond Imports

A imports B and C; both B and C import D. This is **not** a cycle — it's a diamond. The algorithm removes paths from `visitedPaths` after processing (step 6), so D is loaded once via B and once via C. The duplicate PU instance ID check (AC5) catches if both loads produce the same IDs. If D defines PU "SharedPU", the second merge attempt fails with `kDuplicateInstanceId`.

To support true diamond imports (D loaded only once), a `loadedManifests` cache keyed by resolved absolute path can skip re-loading. This is an optimization — not required for AC1-AC9 but noted for future work.

### Path Resolution

```cpp
// Given: importing manifest at "C:/Game/Data/Manifests/main.diaapp"
//        import entry: "sub/sim.diaapp"
// Resolved: "C:/Game/Data/Manifests/sub/sim.diaapp"
//
// Uses DiaCore FilePath utilities for directory extraction and path joining.
```

### CluicheTest Migration (AC7)

`cluiche_main.diaapp` gains:
```json
{
  "version": 1,
  "imports": ["cluiche_sim.diaapp", "cluiche_render.diaapp"],
  "processing_units": [ /* ... existing MainPU definition ... */ ]
}
```

MainProcessingUnit's `PostPhaseStart()` currently calls `ApplicationLoader::LoadApplication()` for sim and render manifests. After this feature, the merged manifest already contains all three PUs. The `ApplicationLoader::LoadApplication()` call on the root manifest returns the main PU, and `ComposeManifests` (or the import-aware `LoadFromFile`) makes sim/render PUs available. MainProcessingUnit migration to use the merged manifest is part of Phase B (PU parent-child tree) — this feature only ensures the merge works.

---

## Dependencies

### Required Modules
- **DiaCore/FilePath** — directory extraction, path joining for relative resolution
- **DiaCore/Containers** — DynamicArrayC for visited paths, HashTable for duplicate detection
- **DiaCore/CRC** — StringCRC for instance ID comparison
- **DiaSerializer** — JsonApplicationManifestSerializer for parsing imported files

### Dependent Features
- **Phase B: PU Parent-Child Tree** — consumes the merged manifest to build the runtime PU tree
- **Phase C: Stage Manifests** — stages use the same import mechanism to inject phases
- **Phase D: Editor Graph View** — reads sourceManifestPath to draw file boundaries

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestManifestImports.cpp)

1. **Single import** — root imports one file; merged manifest has both PUs
2. **Multiple imports** — root imports two files; all three PUs in result
3. **Nested imports** — A imports B which imports C; all entries merged
4. **Circular import detection** — A imports B, B imports A; returns kImportCycle
5. **Self-import detection** — A imports A; returns kImportCycle
6. **Duplicate instance ID rejection** — two manifests define same PU id; returns kDuplicateInstanceId
7. **Relative path resolution** — imports in subdirectory resolved correctly
8. **No imports (backward compat)** — manifest with empty imports array loads unchanged
9. **Missing import file** — imports non-existent file; returns kImportNotFound
10. **Provenance tracking** — after merge, each entry's sourceManifestPath matches origin file
11. **Import with no PUs** — imported file has phases/modules but no PUs; merge handles gracefully
12. **Deep nesting** — 4+ levels of nested imports all resolve correctly

### Integration Test

13. **CluicheTest manifest** — load cluiche_main.diaapp with imports; verify 3 PUs (Main, Sim, Render) present with correct phases and modules

---

## Files Affected

### Headers (Modified)
- `Dia/DiaApplicationFlow/Manifest/ApplicationManifest.h` — add `sourceManifestPath` to entry structs
- `Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoader.h` — add ResolveImports, MergeManifest, HasDuplicateInstanceIds
- `Dia/DiaApplicationFlow/Manifest/JsonApplicationManifestSerializer.cpp` — parse/write imports array and provenance

### Implementation (Modified)
- `Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoader.cpp` — import resolution logic in LoadFromFile, implement ComposeManifests

### Data (Modified)
- `Cluiche/CluicheTest/Data/Manifests/cluiche_main.diaapp` — add imports array

### Tests (New)
- `Cluiche/Tests/GoogleTests/Application/TestManifestImports.cpp`
- Test manifest fixtures (small .diaapp files for import testing)

### Project Files (Modified)
- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` — add test file
- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj.filters` — add test file

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | **Compliant** — duplicate ID detection uses StringCRC comparison. No new ID types introduced. |
| PD-002 | Platform | PU/Phase/Module architecture | **Compliant** — manifest imports merge PU/Phase/Module entries without changing the three-level hierarchy. |
| PD-004 | Platform | No STL containers in public APIs | **Compliant** — visitedPaths uses DynamicArrayC; sourceManifestPath is const char*. No STL in public API. |
| PD-005 | Platform | x64 only | **Compliant** — no platform-specific code. |
| PD-006 | Platform | VS project files source of truth | **Compliant** — new test file added to vcxproj. |
| PD-007 | Platform | C++20 required | **Compliant** — no language version constraints. |
| PD-008 | Platform | Directory.Build.props owns build settings | **Compliant** — no build setting changes. |
| PD-009 | Platform | Output under Cluiche/out/ | **Compliant** — no output path changes. |
| AD-001 | Dia App | Module system with YAML frontmatter | **Compliant** — no new modules created; existing architecture docs unaffected. |
| AD-002 | Dia App | No STL in public APIs | **Compliant** — see PD-004. |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | **Compliant** — all new code in Dia::Application::. |
| SD-001 | DiaApplicationFlow | PU/Phase/Module three-level hierarchy | **Compliant** — import merging respects the three levels; PU entries contain their phases/modules. |
| SD-004 | DiaApplicationFlow | Modules identified by StringCRC | **Compliant** — duplicate detection uses StringCRC matching. |
| SD-006 | DiaApplicationFlow | Support raw pointer and UniquePtr ownership | **Compliant** — provenance field is const char* (owned by manifest memory); no ownership model change. |
| SD-010 | DiaApplicationFlow | Explicit dependencies via AddDependancy() | **Compliant** — module dependencies within imported manifests preserved as-is during merge. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Diamond Imports | Should diamond imports (A→B→D, A→C→D) load D once (cached) or fail on duplicate IDs? | Load once via path cache. If D is reached via two paths, recognize it's the same file (by resolved absolute path) and skip the second load. This avoids forcing users to restructure imports to avoid diamonds. |
| 2 | Memory Ownership | Who owns the `sourceManifestPath` strings? Are they copied or pointing into transient parse buffers? | Strings are allocated by the manifest loader and owned by the ApplicationManifest. Use the same allocation pattern as existing `imports` entries (strdup-style, freed in manifest destructor). |
| 3 | LoadFromString | Should `LoadFromString()` support imports? It has no file path context for relative resolution. | No — LoadFromString has no base directory for resolving relative paths. Import resolution is file-only. Document that imports are ignored when loading from string. |
| 4 | Merge Semantics | Can an imported manifest's PU have the same type_id as the root's PU (different instance_id)? | Yes — type_id can repeat (e.g., two SimProcessingUnit instances with different instance_ids). Only instance_id uniqueness is enforced. |
| 5 | Import Depth Limit | Should there be a max recursion depth to prevent stack overflow on deeply nested imports? | Yes — cap at 16 levels. Any real application exceeding 16 levels of manifest nesting has a structural problem. Return kImportCycle with a "max depth exceeded" message. |
| 6 | Provenance in JSON | Should sourceManifestPath be serialized when saving a manifest back to JSON? | Yes — in the metadata section (not the core schema), so it round-trips through the editor without polluting the manifest format. Manifests loaded fresh from disk always get provenance re-tagged from the actual file path. |
| 7 | ComposeManifests | The existing ComposeManifests method takes explicit file paths. Should it also resolve imports within those files? | Yes — ComposeManifests should call ResolveImports for each file path. This makes it a superset of LoadFromFile (multiple roots, each with their own imports). |
| 8 | CluicheTest Migration | Should MainProcessingUnit.cpp be updated to stop manually loading sim/render manifests in this phase? | No — that migration belongs to Phase B (PU parent-child tree). This phase only ensures the merge works and updates the .diaapp file. Manual loading continues as fallback. |

---

## Examples

### Example 1: Root manifest with imports

**cluiche_main.diaapp:**
```json
{
  "version": 1,
  "imports": ["cluiche_sim.diaapp", "cluiche_render.diaapp"],
  "processing_units": [
    {
      "type_id": "MainProcessingUnit",
      "instance_id": "MainProcessingUnit",
      "root": true,
      "frequency_hz": 30.0,
      "dedicated_thread": false,
      "phases": [ ... ],
      "transitions": [ ... ],
      "initial_phase": "MainBootPhase",
      "modules": [ ... ]
    }
  ]
}
```

**After LoadFromFile("cluiche_main.diaapp", manifest):**
```
manifest.processingUnits.Size() == 3
manifest.processingUnits[0].instanceId == "MainProcessingUnit"
manifest.processingUnits[0].sourceManifestPath == "cluiche_main.diaapp"
manifest.processingUnits[1].instanceId == "SimProcessingUnit"
manifest.processingUnits[1].sourceManifestPath == "cluiche_sim.diaapp"
manifest.processingUnits[2].instanceId == "RenderProcessingUnit"
manifest.processingUnits[2].sourceManifestPath == "cluiche_render.diaapp"
```

---

## Status

`Approved`

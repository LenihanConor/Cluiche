# Feature Spec: Stage Manifests

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplication | @docs/specs/systems/dia/diaapplication.md |
| Feature | **Stage Manifests** | (this document) |

**Status:** `Approved`

**Research:** @docs/research/diappl_flow_tree/summary.md (Phase C)
**Depends on:** @docs/specs/features/dia/diaapplication/manifest-imports.md (Phase A — import resolution used to link stage manifests)

---

## Problem Statement

Game content like levels (e.g., DummyLevel) injects phases and modules into ProcessingUnits purely through code. These injections are invisible to the manifest system and the editor — the editor can show the application's PU/phase/module structure from .diaapp files, but has no way to see or edit what a level contributes. Levels also have confusing naming ("level" implies spatial, but DummyLevel is really a gameplay stage that defines execution flow).

---

## Solution Overview

Rename the "level" concept to **stage**. Each stage gets its own `.diaapp` manifest that declares the phases, modules, and phase transitions it contributes. Stages do not create PUs — they declare phase/module injections that the manifest loader merges into existing parent PUs at load time. The stage manifest is linked via the `imports` field (Phase A), making stage contributions visible to the editor.

### Key Design Points

1. **Stages inject, don't own** — a stage declares phases and modules tagged with a target PU instance ID. The loader merges these into the target PU's entry. Stages never create new PUs or threads (SD-012).
2. **Rename Level -> Stage** — DummyLevel becomes DummyStage throughout CluicheTest. ILevel becomes IStage. LevelFactory becomes StageFactory.
3. **Stage .diaapp format** — uses the same .diaapp JSON schema, with a `stage` metadata field marking it as a stage manifest and a `target_processing_unit` field on each phase/module entry indicating which PU to inject into.
4. **Auto-generated manifests from introspection** — the Stage class constructs its phases in C++ (source of truth for runtime). After code-based construction, `ApplicationIntrospector` auto-generates the stage manifest from the runtime topology. The editor reads this derived manifest. This eliminates code/manifest divergence — there is one source of truth (code), and the manifest is always a faithful reflection of it.
5. **Backward compatible** — stages can still be constructed purely in code (no manifest required). The manifest is opt-in for editor visibility.
6. **Explicit phase contracts** — stage manifests declare a `requires_phases` field listing phases from parent PUs that the stage's transitions depend on. The loader validates these contracts before merging, catching cross-boundary rename breakage at load time rather than at runtime.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | ILevel is renamed to IStage; LevelFactory renamed to StageFactory; all references updated | Build succeeds with no ILevel/LevelFactory references remaining |
| AC2 | DummyLevel namespace/class renamed to DummyStage throughout CluicheTest | Build succeeds; grep for "DummyLevel" returns zero hits |
| AC3 | DummyStage has a .diaapp manifest (`stages/dummy_stage.diaapp`) declaring its phases and transitions | File exists with correct JSON structure |
| AC4 | Stage manifest declares `target_processing_unit` on each phase entry, indicating which PU to inject into | Unit test: parse stage manifest, verify target PU field present on all phase entries |
| AC5 | ApplicationManifestLoader merges stage manifest phases into the target PU when the stage is imported | Unit test: root manifest imports stage; merged result has stage's phases in the target PU's entry |
| AC6 | Stage manifest phase transitions are merged into the target PU's transition table | Unit test: stage declares transition A->B; after merge, target PU's transitions include A->B |
| AC7 | Stage manifest modules are merged into the target PU's module list with correct phase affinity | Unit test: stage declares module M for phase A; after merge, M appears in target PU with phase_ids containing A |
| AC8 | Duplicate phase/module instance IDs between stage and parent manifest are detected and rejected | Unit test: stage and root both define phase with same ID; load fails with kDuplicateInstanceId |
| AC9 | Stage manifest provenance tracked (sourceManifestPath set to stage .diaapp path) | Unit test: after merge, stage phases have sourceManifestPath == "stages/dummy_stage.diaapp" |
| AC10 | Stage manifest has `stage` metadata identifying it as a stage (not a standalone application manifest) | Unit test: parse stage manifest, verify metadata.type == "stage" |
| AC11 | CluicheTest `cluiche_main.diaapp` imports `stages/dummy_stage.diaapp` | File updated; load produces merged manifest with DummyStage phases in MainPU |
| AC12 | Purely code-based stage construction (no manifest) continues to work | Integration test: construct stage in code without any manifest, verify phases injected correctly |
| AC13 | CluicheTest builds and runs correctly after the Level->Stage rename | Manual verification: application starts, DummyStage loads and runs |
| AC14 | `ApplicationIntrospector` can auto-generate a stage manifest from a code-constructed stage's runtime topology | Unit test: construct DummyStage in code, call introspector, verify generated manifest matches expected phases/transitions/modules |
| AC15 | Auto-generated manifest round-trips: generate → save → load → merge produces identical result to code-based construction | Integration test: generate manifest from DummyStage, load it, compare merged PU topology to code-built topology |
| AC16 | Stage manifest `requires_phases` field lists external phases that stage transitions depend on | Unit test: parse stage manifest, verify requires_phases contains "MainBootStrapPhase" |
| AC17 | Loader validates `requires_phases` — if a required phase is missing from the target PU, load fails with clear error | Unit test: stage requires "NonExistentPhase"; load returns kMissingRequiredField with message naming the missing phase |

---

## Public API

### Renamed Interfaces (CluicheKernel)

```cpp
// Before: ILevel, LevelFactory, LevelRegistryModule
// After:  IStage, StageFactory, StageRegistryModule

namespace Cluiche::Kernel {

    class IStage {
    public:
        virtual ~IStage() = default;
        virtual const Dia::Core::StringCRC& GetUniqueId() const = 0;
        virtual const Dia::Core::StringCRC& GetEntryPhaseUniqueId() const = 0;
        virtual const Dia::Core::StringCRC& GetExitPhaseUniqueId() const = 0;
    };

    class StageFactory {
    public:
        using CreateFn = IStage* (*)(Dia::Application::Phase* currentPhase,
                                     Dia::Application::ProcessingUnit* mainPU,
                                     Dia::Application::ProcessingUnit* simPU,
                                     Dia::Application::ProcessingUnit* renderPU);

        void Register(const Dia::Core::StringCRC& stageId, CreateFn factory);
        IStage* Create(const Dia::Core::StringCRC& stageId,
                       Dia::Application::Phase* currentPhase,
                       Dia::Application::ProcessingUnit* mainPU,
                       Dia::Application::ProcessingUnit* simPU,
                       Dia::Application::ProcessingUnit* renderPU);
    };
}
```

### Stage Manifest JSON Schema

```json
{
  "version": 1,
  "metadata": {
    "type": "stage",
    "name": "DummyStage",
    "description": "Demo gameplay stage for CluicheTest",
    "requires_phases": ["MainBootStrapPhase"]
  },
  "stage_phases": [
    {
      "type_id": "DummyStage::MainLoadPhase",
      "instance_id": "DummyStage::MainLoadPhase",
      "target_processing_unit": "MainProcessingUnit",
      "config": {}
    },
    {
      "type_id": "DummyStage::MainFEPhase",
      "instance_id": "DummyStage::MainFEPhase",
      "target_processing_unit": "MainProcessingUnit",
      "config": {}
    }
  ],
  "stage_transitions": [
    {
      "from": "MainBootStrapPhase",
      "to": "DummyStage::MainLoadPhase",
      "target_processing_unit": "MainProcessingUnit"
    },
    {
      "from": "DummyStage::MainLoadPhase",
      "to": "DummyStage::MainFEPhase",
      "target_processing_unit": "MainProcessingUnit"
    },
    {
      "from": "DummyStage::MainFEPhase",
      "to": "MainBootStrapPhase",
      "target_processing_unit": "MainProcessingUnit"
    }
  ],
  "stage_modules": []
}
```

### Changes to ApplicationManifestLoader

```cpp
class ApplicationManifestLoader {
    // ...existing...

private:
    // New: merge stage manifest into target PU entries
    // Called during import resolution when a manifest has metadata.type == "stage"
    ManifestValidationResult MergeStageManifest(
        const ApplicationManifest& stageManifest,
        ApplicationManifest& targetManifest);

    // New: resolve target_processing_unit references in stage phases/modules
    // Finds the target PU entry in the merged manifest and injects the stage's contributions
    bool ResolveStageTargets(
        const ApplicationManifest& stageManifest,
        ApplicationManifest& targetManifest);
};
```

---

## Implementation Notes

### Stage Manifest Merge Algorithm

When the loader encounters an imported manifest with `metadata.type == "stage"`:

```
MergeStageManifest(stageManifest, targetManifest):
1. For each phase in stageManifest.stage_phases:
   a. Find target PU in targetManifest by target_processing_unit ID
   b. If target PU not found: error kMissingRequiredField ("target PU not in manifest")
   c. Check phase instance ID not duplicate within target PU
   d. Append phase to target PU's phases array
   e. Set sourceManifestPath on the phase entry

2. For each transition in stageManifest.stage_transitions:
   a. Find target PU by target_processing_unit ID
   b. Append transition to target PU's transitions array

3. For each module in stageManifest.stage_modules:
   a. Find target PU by target_processing_unit ID
   b. Check module instance ID not duplicate
   c. Append module to target PU's modules array
   d. Set sourceManifestPath on the module entry
```

### Level -> Stage Rename Scope

Files affected in CluicheTest:
- `CluicheKernel/ILevel.h` → `CluicheKernel/IStage.h`
- `CluicheKernel/LevelFactory.h/.cpp` → `CluicheKernel/StageFactory.h/.cpp`
- `CluicheKernel/LevelRegistryModule.h/.cpp` → `CluicheKernel/StageRegistryModule.h/.cpp`
- `Levels/DummyLevel/` → `Stages/DummyStage/`
- All phase files within DummyLevel rename namespace
- `ApplicationFlow/` references to levels → stages
- vcxproj and vcxproj.filters for CluicheTest and CluicheKernel

### Auto-Generation via ApplicationIntrospector

After a stage constructs itself in code (phases, transitions, modules wired up):

```
1. ApplicationIntrospector inspects the stage's owning PU:
   - Enumerate phases added by the stage (identified by stage namespace prefix or explicit registration)
   - Enumerate transitions involving those phases
   - Enumerate modules added by the stage
2. Build a stage manifest JSON from the inspected topology
3. Populate requires_phases by scanning transitions for external phase references
   (phases not owned by this stage but referenced in from/to transitions)
4. Write to Cluiche/out/<AppName>/manifests/stages/<stage_id>.diaapp
```

The editor reads the generated manifest. Developers never hand-author stage manifests — they are always derived from the code. If a stage's code changes, regeneration produces an updated manifest automatically.

### Requires-Phases Validation

Before merging a stage manifest, the loader validates `requires_phases`:

```
For each phase in requires_phases:
    Find phase in target PU's existing phases (from root + previously merged imports)
    If not found: error kMissingRequiredField with message:
        "Stage 'DummyStage' requires phase 'MainBootStrapPhase' on PU 'MainProcessingUnit' but it was not found"
```

This catches cross-boundary rename breakage at load time.

### Stage Schema vs Application Schema

Stage manifests use the same `.diaapp` extension and version field, but differ structurally:
- Application manifests have `processing_units` (top-level PU definitions)
- Stage manifests have `stage_phases`, `stage_transitions`, `stage_modules` (injections into existing PUs)
- The `metadata.type` field distinguishes them: `"application"` (default/absent) vs `"stage"`
- The loader detects the type during import resolution and routes to the appropriate merge logic

### Backward Compatibility

The IStage interface keeps the same shape as ILevel — constructor still receives currentPhase, mainPU, simPU, renderPU. Code-based stage construction works identically. The manifest is a derived artifact auto-generated from the code-constructed topology via `ApplicationIntrospector`, making it always consistent with the code.

---

## Dependencies

### Required Modules
- **DiaApplication/Manifest** — manifest loader, serializer, validator
- **DiaCore/CRC** — StringCRC for stage and phase IDs
- **DiaCore/Containers** — DynamicArrayC for stage phases/modules

### Required Features
- **Manifest Imports (Phase A)** — import resolution is the mechanism for linking stage manifests

### Dependent Features
- **Editor Connected Graph View (Phase D)** — visualizes stage boundaries in the PU tree using sourceManifestPath + metadata.type

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestStageManifests.cpp)

1. **Parse stage manifest** — load stage .diaapp, verify metadata.type == "stage"
2. **Phase injection** — stage declares 2 phases for MainPU; after merge, MainPU has those phases
3. **Transition injection** — stage declares transitions; after merge, target PU transitions include them
4. **Module injection** — stage declares module with phase affinity; after merge, module in target PU
5. **Target PU not found** — stage targets non-existent PU; returns error
6. **Duplicate phase ID** — stage phase has same ID as existing PU phase; rejected
7. **Provenance** — merged stage phases have correct sourceManifestPath
8. **Multi-stage import** — root imports two stage manifests; both merged correctly
9. **Stage + application import** — root imports both a stage and an application manifest; both merge correctly
10. **Empty stage** — stage with no phases/modules/transitions; merge succeeds as no-op
11. **Stage metadata preserved** — stage name and description accessible after merge

### Integration Tests

12. **CluicheTest end-to-end** — application starts, DummyStage loads and transitions through MainLoadPhase -> MainFEPhase
13. **Code-only stage** — stage without manifest constructs and runs identically to current DummyLevel
14. **Manifest round-trip** — save merged manifest, reload, verify stage phases still present

---

## Files Affected

### Renamed (CluicheKernel)
- `CluicheKernel/ILevel.h` → `CluicheKernel/IStage.h`
- `CluicheKernel/LevelFactory.h/.cpp` → `CluicheKernel/StageFactory.h/.cpp`
- `CluicheKernel/LevelRegistryModule.h/.cpp` → `CluicheKernel/StageRegistryModule.h/.cpp`

### Renamed (CluicheTest)
- `Cluiche/CluicheTest/Levels/DummyLevel/` → `Cluiche/CluicheTest/Stages/DummyStage/`
- All files within: Level.h/.cpp, phase files, namespace references

### Modified (DiaApplication)
- `Dia/DiaApplication/Manifest/ApplicationManifestLoader.h/.cpp` — stage manifest merge logic, requires_phases validation
- `Dia/DiaApplication/Manifest/JsonApplicationManifestSerializer.cpp` — parse/write stage schema fields including requires_phases
- `Dia/DiaApplication/Manifest/ManifestValidator.h/.cpp` — validate stage manifest structure, requires_phases contracts
- `Dia/DiaApplication/Introspection/ApplicationIntrospector.h/.cpp` — auto-generate stage manifests from runtime topology

### New (Data)
- `Cluiche/CluicheTest/Data/Manifests/stages/dummy_stage.diaapp` — DummyStage manifest

### New (Tests)
- `Cluiche/Tests/GoogleTests/Application/TestStageManifests.cpp`

### Modified (Project Files)
- CluicheTest vcxproj + filters — renamed files
- CluicheKernel vcxproj + filters — renamed files
- GoogleTests vcxproj + filters — new test file

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all IDs | **Compliant** — stage IDs, phase IDs, target PU references all StringCRC. |
| PD-002 | Platform | PU/Phase/Module architecture | **Compliant** — stages inject into the existing PU/Phase/Module hierarchy. No new architectural level introduced. |
| PD-004 | Platform | No STL in public APIs | **Compliant** — IStage interface uses StringCRC references only. Internal arrays use DynamicArrayC. |
| PD-005 | Platform | x64 only | **Compliant** — no platform-specific code. |
| PD-006 | Platform | VS project files source of truth | **Compliant** — all renamed/new files updated in vcxproj. |
| PD-007 | Platform | C++20 required | **Compliant** — no language version constraints. |
| PD-008 | Platform | Directory.Build.props owns build settings | **Compliant** — no build setting changes. |
| PD-009 | Platform | Output under Cluiche/out/ | **Compliant** — no output path changes. |
| AD-002 | Dia App | No STL in public APIs | **Compliant** — see PD-004. |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | **Compliant** — DiaApplication code stays in Dia::Application::. CluicheTest code in Cluiche::DummyStage::. |
| SD-001 | DiaApplication | PU/Phase/Module three-level hierarchy | **Compliant** — stage phases are Phase objects injected into PUs. The three levels are unchanged. |
| SD-004 | DiaApplication | Modules identified by StringCRC | **Compliant** — stage modules use StringCRC instance IDs. |
| SD-011 | DiaApplication | PU parent-child tree | **Compliant** — stages inject into PUs in the tree. Stage manifests are imported by the root or any PU's manifest. |
| SD-012 | DiaApplication | Stages inject phases, don't own PUs | **Compliant** — this feature implements SD-012. Stage manifests declare phase/module injections only. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Schema | Should stage manifests use a different file extension (e.g., .diastage) instead of .diaapp? | No — pre-spec commitment says .diaapp is the only manifest format. The `metadata.type` field distinguishes stage manifests from application manifests. This keeps tooling simple. |
| 2 | Target Resolution | What happens if a stage targets a PU that hasn't been loaded yet (import order issue)? | Stage manifests are always imported by a parent manifest that defines (or imports) the target PUs. The merge happens after all imports are resolved, so target PUs are guaranteed to exist. If they don't, it's a validation error. |
| 3 | Transition Bridging | Stage transitions reference phases from both the stage and the parent PU (e.g., MainBootStrapPhase -> DummyStage::MainLoadPhase). How does the loader validate these cross-boundary transitions? | Two layers of validation: (1) `requires_phases` is validated before merge — if the stage declares it requires "MainBootStrapPhase" and that phase doesn't exist in the target PU, load fails immediately with a clear error. (2) After merge, standard phase transition validation catches any remaining invalid references. The requires_phases contract makes cross-boundary dependencies explicit rather than buried in transition edges. |
| 4 | Multiple Stages | Can multiple stages inject into the same PU? | Yes — each stage's phases are appended to the PU. Instance ID uniqueness across stages is enforced. Transition bridging between stages is the application's responsibility (stage A's exit -> stage B's entry). |
| 5 | Stage Loading Order | Does the order of stage imports matter? | For the merge: no — phases are appended and transitions are additive. For runtime behavior: the application controls which stage's entry phase to transition to. Import order doesn't affect execution order. |
| 6 | Rename Blast Radius | Is the Level->Stage rename limited to CluicheTest, or does it affect DiaApplication? | CluicheTest and CluicheKernel only. DiaApplication has no "level" or "stage" concept — it deals with PUs, phases, and modules. The stage concept lives in the application layer (CluicheKernel), not the engine. |
| 7 | Manifest Parity | What if the code-based stage structure diverges from the manifest? | It can't — the manifest is auto-generated from the code-constructed topology via ApplicationIntrospector. Code is the single source of truth. The manifest is a derived artifact regenerated whenever the stage is constructed. No hand-authored stage manifests means no divergence. |
| 8 | Stage Metadata in Merged Manifest | After merge, how does the editor know which phases came from which stage? | Via sourceManifestPath (Phase A provenance) + metadata.type == "stage". The editor groups phases by sourceManifestPath and renders stage boundaries. Stage name from metadata provides the display label. |

---

## Examples

### Example 1: Stage Manifest (dummy_stage.diaapp)

```json
{
  "version": 1,
  "metadata": {
    "type": "stage",
    "name": "DummyStage",
    "description": "Demo gameplay stage for CluicheTest",
    "requires_phases": ["MainBootStrapPhase"]
  },
  "stage_phases": [
    {
      "type_id": "DummyStage::MainLoadPhase",
      "instance_id": "DummyStage::MainLoadPhase",
      "target_processing_unit": "MainProcessingUnit",
      "config": {}
    },
    {
      "type_id": "DummyStage::MainFEPhase",
      "instance_id": "DummyStage::MainFEPhase",
      "target_processing_unit": "MainProcessingUnit",
      "config": {}
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

### Example 2: Root manifest importing stage

```json
{
  "version": 1,
  "imports": ["cluiche_sim.diaapp", "cluiche_render.diaapp", "stages/dummy_stage.diaapp"],
  "processing_units": [ ... ]
}
```

### Example 3: After merge — MainPU phases

```
MainPU phases (after merge):
  - MainBootPhase          (from cluiche_main.diaapp)
  - MainBootStrapPhase     (from cluiche_main.diaapp)
  - DummyStage::MainLoadPhase   (from stages/dummy_stage.diaapp)
  - DummyStage::MainFEPhase     (from stages/dummy_stage.diaapp)

MainPU transitions (after merge):
  - MainBootPhase -> MainBootStrapPhase
  - MainBootStrapPhase -> DummyStage::MainLoadPhase   (stage entry)
  - DummyStage::MainLoadPhase -> DummyStage::MainFEPhase
  - DummyStage::MainFEPhase -> MainBootStrapPhase     (stage exit)
```

---

## Status

`Approved`

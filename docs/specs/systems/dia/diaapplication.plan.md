# Plan: DiaApplicationFlow — Flow Tree

**Spec:** @docs/specs/systems/dia/diaapplication.md  
**Status:** Not Started  
**Started:** 2026-05-06  
**Last Updated:** 2026-05-06

## Context

Three Approved features to implement in build order A → B → C.

| Feature | Spec |
|---------|------|
| A — Manifest Imports | @docs/specs/features/dia/diaapplication/manifest-imports.md |
| B — PU Parent-Child Tree | @docs/specs/features/dia/diaapplication/pu-parent-child-tree.md |
| C — Stage Manifests | @docs/specs/features/dia/diaapplication/stage-manifests.md |

### Pre-implementation audit findings

- `ManifestComposer` (merge + cycle detection + recursive resolve) is **fully implemented** — only `LoadManifestFromFile()` is a stub.
- `ApplicationManifest.imports` array **already exists** in the struct (line 74) — not parsed from JSON yet.
- `kImportNotFound` / `kImportCycle` error codes **already defined** in `ManifestValidator.h`.
- `ProcessingUnitEntry` has no `root` flag, no `sourceManifestPath`, no `parentInstanceId` — Phase A & B additions needed.
- `ApplicationProcessingUnit` has no parent-pointer, no child table, no child thread management.
- Level→Stage rename already completed (recent commit renames DummyLevel to DummyStage and Levels/ to Stages/).
- `ApplicationIntrospector` exists but needs `GenerateStageManifest()` added for Phase C auto-generation.
- `cluiche_main.diaapp` has no `imports` field and no `"root": true` on MainPU.

---

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| A1 | Parse `imports` array in `JsonApplicationManifestSerializer::Load()` | Not Started | haiku | Add loop over `root["imports"]` array, populate `outManifest.imports` |
| A2 | Write `imports` array in `JsonApplicationManifestSerializer::Save()` | Not Started | haiku | Serialize non-empty imports array back to JSON |
| A3 | Add `sourceManifestPath` (const char*) to `ProcessingUnitEntry`, `PhaseEntry`, `ModuleEntry` | Not Started | haiku | ApplicationManifest.h only; default nullptr |
| A4 | Implement `ManifestComposer::LoadManifestFromFile()` stub | Not Started | sonnet | Replace TODO stub with real load via `JsonApplicationManifestSerializer`; tag all entries with `sourceManifestPath` after load |
| A5 | Wire `ApplicationManifestLoader::LoadFromFile()` through `ManifestComposer` | Not Started | sonnet | After parsing root JSON, call `ManifestComposer::ComposeSingleManifest()`; add relative-path resolution for import entries |
| A6 | Add `root` flag to `ProcessingUnitEntry`; parse from JSON (`"root": true`) | Not Started | haiku | Needed by Phase B; parse in `ParseProcessingUnit()`; serialize in Save |
| A7 | Update `cluiche_main.diaapp` to import sim and render; add `"root": true` to MainPU | Not Started | haiku | Manifests at `Cluiche/CluicheTest/Data/Manifests/` |
| A8 | Write tests: `TestManifestImports.cpp` (13 ACs from spec) | Not Started | sonnet | New test file; use temp fixture .diaapp files written per-test |
| A9 | Build + run Phase A tests; fix failures | Not Started | sonnet | `dia run googletest --filter="ManifestImports*"` |
| B1 | Add tree API to `ApplicationProcessingUnit.h` | Not Started | sonnet | `AddChildProcessingUnit`, `RemoveChildProcessingUnit`, `GetParent`, `FindChildProcessingUnit`, `GetChildren`, `IsRoot`, `FindProcessingUnitInTree`; add `mParent`, `mOwnedChildPUs`, `mChildThreads` members |
| B2 | Implement tree methods in `ApplicationProcessingUnit.cpp` | Not Started | sonnet | All 7 new methods; unique-ID check walks full subtree |
| B3 | Auto thread lifecycle in `DoStart()` / `DoStop()` | Not Started | sonnet | DoStart: spawn threads for dedicated children after PostPhaseStart; DoStop: join children bottom-up before PrePhaseStop |
| B4 | Add `LoadApplicationTree()` to `ApplicationLoader` | Not Started | sonnet | Calls LoadFromFile (import-aware after A5), validates exactly one root PU, builds PU tree via `AddChildProcessingUnit`; returns root |
| B5 | Migrate CluicheTest to use `LoadApplicationTree()` | Not Started | sonnet | Remove manual SimPU/RenderPU creation from `MainProcessingUnit::PostPhaseStart/PrePhaseStop`; use tree; keep old code commented as reference |
| B6 | Write tests: `TestPUTree.cpp` (10 unit ACs) + `TestPUTreeIntegration.cpp` (6 integration ACs) | Not Started | sonnet | Per spec; use lightweight fake PU types |
| B7 | Build + run Phase B tests; fix failures | Not Started | sonnet | `dia run googletest --filter="PUTree*"` |
| C1 | Add stage manifest schema fields to `ApplicationManifest.h` | Not Started | sonnet | Add `StagePhaseEntry` (PhaseEntry + target_processing_unit), `StageTransitionEntry`, `StageModuleEntry`; add `stagePhases`, `stageTransitions`, `stageModules` arrays; add `metadata.type` detection |
| C2 | Parse/save stage manifest format in `JsonApplicationManifestSerializer` | Not Started | sonnet | Detect `metadata.type == "stage"`; parse `stage_phases`, `stage_transitions`, `stage_modules`, `requires_phases`; serialize symmetrically |
| C3 | Implement `MergeStageManifest()` in `ManifestComposer` | Not Started | sonnet | AC5-AC9 from stage-manifests spec: inject stage phases/transitions/modules into target PU; validate requires_phases before merge; enforce duplicate ID check |
| C4 | Route stage manifests through import resolution | Not Started | sonnet | In `ResolveImportsRecursive`: detect `metadata.type == "stage"`, call `MergeStageManifest()` instead of standard `MergeManifests()` |
| C5 | Write `dummy_stage.diaapp` | Not Started | haiku | At `Cluiche/CluicheTest/Data/Manifests/stages/dummy_stage.diaapp`; phases: MainLoadPhase, MainFEPhase; transitions include MainBootStrapPhase bridge |
| C6 | Add stage import to `cluiche_main.diaapp` | Not Started | haiku | Add `"stages/dummy_stage.diaapp"` to imports array |
| C7 | Add `GenerateStageManifest()` to `ApplicationIntrospector` | Not Started | sonnet | Inspect PU for phases tagged by stage namespace prefix; emit stage manifest JSON; write to `out/<AppName>/manifests/stages/` |
| C8 | Write tests: `TestStageManifests.cpp` (14 ACs from spec) | Not Started | sonnet | New test file; fixture manifests per-test |
| C9 | Build + run Phase C tests; fix failures | Not Started | sonnet | `dia run googletest --filter="StageManifests*"` |
| D1 | Update test-completeness-registry for all new test files | Not Started | haiku | Add rows for TestManifestImports, TestPUTree, TestPUTreeIntegration, TestStageManifests |
| D2 | Mark feature specs A/B/C Done; mark system spec Done | Not Started | haiku | Update status fields in spec files |
| D3 | Update BACKLOG.md / BACKLOG-HISTORY.md | Not Started | haiku | Move DiaApplicationFlow Flow Tree to history |

---

## Sub-task detail: Phase A

### A4 — ManifestComposer::LoadManifestFromFile
Replace stub with:
1. Call `JsonApplicationManifestSerializer::LoadFromFile(filePath, outManifest)`
2. After successful load, iterate all `processingUnits[i].phases[j]`, `modules[j]` and set `.sourceManifestPath = filePath` on each
3. Set `.sourceManifestPath` on each ProcessingUnitEntry too

Relative-path resolution lives in A5 (the caller passes resolved absolute path to this function).

### A5 — Wire import resolution into LoadFromFile
1. Parse root manifest JSON directly first (existing logic)
2. Resolve each `imports[i]` entry relative to the manifest's directory (use `DiaCore::FilePath` or `strrchr`-based approach — check what's available)
3. Call `ManifestComposer::ResolveImportsRecursive()` for each import
4. Merge results into outManifest
5. `LoadFromString()` stays unchanged (no base path available)

---

## Sub-task detail: Phase B

### B1/B2 — PU tree members
```cpp
// New private members in ApplicationProcessingUnit:
ProcessingUnit* mParent;  // non-owning back-pointer, nullptr = root
Dia::Core::Containers::DynamicArrayC<Dia::Core::UniquePtr<ProcessingUnit>, 4> mOwnedChildPUs;
Dia::Core::Containers::DynamicArrayC<std::thread, 4> mChildThreads;
```
`ProcessingUnitTable` is a `HashTable<StringCRC, ProcessingUnit*>` — check if it already exists as a typedef in the header before adding `mChildPUTable`.

### B3 — Thread lifecycle timing
- `DoStart()`: after `PostPhaseStart()` returns, iterate `mOwnedChildPUs`, call `child->Initialize()` + `child->Start()`, then spawn `std::thread` if `dedicatedThread`
- `DoStop()`: before `PrePhaseStop()`, iterate in reverse, signal stop + join thread + call `child->Stop()`

### B5 — CluicheTest migration
`MainProcessingUnit::PostPhaseStart()` currently creates SimPU and RenderPU manually. After B4 is done, `LoadApplicationTree()` builds the tree. The entry point (`main.cpp` or equivalent) switches from `LoadApplication()` to `LoadApplicationTree()`. Comment out (don't delete) the manual wiring block.

---

## Sub-task detail: Phase C

### C3 — Stage merge validation order
1. Find target PU in merged manifest (error if missing)
2. Validate `requires_phases` against target PU's current phases (error if any missing)
3. Check for duplicate phase/module instance IDs (error if collision)
4. Append phases, transitions, modules; set `sourceManifestPath` on each

### C7 — ApplicationIntrospector::GenerateStageManifest
- Stage phases identified by namespace prefix (e.g., `"DummyStage::"`) or explicit registration tag
- Scan target PU's `mPhases` / `mTransitions` / `mModules` for entries matching the stage prefix
- Build JSON using existing `SerialisePhase` / `SerialiseModule` / `SerialiseTransition` helpers
- Populate `requires_phases` by finding transitions that reference phases *not* owned by the stage

---

## Session Notes

### 2026-05-06
- Plan created. Pre-implementation audit: ManifestComposer fully written, only LoadManifestFromFile stub + JSON serializer missing imports parsing + struct missing sourceManifestPath / root flag.
- Level→Stage rename already done in a previous commit — Phase C scope is smaller than spec implied.
- Phase A is the lightest (3–5 files, mostly mechanical). Phase B is medium (new API on ProcessingUnit, thread management, CluicheTest migration). Phase C is heaviest (new schema branch, stage merge logic, introspector).
- Model guide: haiku for A1/A2/A3/A6/A7/C5/C6/D1/D2/D3 (mechanical edits); sonnet for everything else.

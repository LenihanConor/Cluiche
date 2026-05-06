# Research: Ideate -- DiaApplication Flow Tree

**Input:** docs/research/diappl_flow_tree/explore.md

## Candidates

### Candidate 1: ApplicationFlow Root Class (No Module Rename)

**Home module/system:** DiaApplication (new class in existing module)
**Size:** M (1-3 weeks)

Add a new `ApplicationFlow` class inside DiaApplication that acts as the root of a PU tree. ApplicationFlow owns the root PU, discovers child PUs via the manifest import chain, and provides tree-traversal APIs (e.g., `GetAllProcessingUnits()`, `FindProcessingUnit(StringCRC)`). ProcessingUnit gains a `mParentFlow` back-pointer and `mChildPUs` table (using the already-defined `ProcessingUnitTable` typedef).

The DiaApplication module itself is **not renamed** -- the naming confusion is solved by giving the tree-root a distinct, descriptive name (`ApplicationFlow`) that users interact with. CluicheTest's MainProcessingUnit would be constructed under an ApplicationFlow instance instead of standalone.

**Primary value:** The engine and editor get a single entry point to discover the full PU topology without any include-path or namespace churn.

---

### Candidate 2: Full DiaApplication -> DiaApplicationFlow Rename

**Home module/system:** DiaApplication (renamed to DiaApplicationFlow)
**Size:** XL (>2 months)

Rename the entire module: directory `Dia/DiaApplicationFlow/`, namespace `Dia::ApplicationFlow::`, all include paths, vcxproj, architecture docs. Every file that includes any DiaApplication header changes.

**Primary value:** Eliminates naming ambiguity permanently at the module level -- "ApplicationFlow" cannot be confused with "the application itself."

---

### Candidate 3: PU Parent-Child Tree in ProcessingUnit

**Home module/system:** DiaApplication (ProcessingUnit class)
**Size:** M (1-3 weeks)

Extend ProcessingUnit to support children directly. Add `AddChildProcessingUnit(UniquePtr<ProcessingUnit>)`, `GetChildren()`, `GetParent()`. Parent PU automatically spawns/joins child threads based on `dedicatedThread` flag. Teardown walks the tree bottom-up (children stop before parent). Uses the existing `ProcessingUnitTable` typedef for child storage.

This is the runtime core that Candidates 1, 5, and 7 build on. Without it, the tree is only a manifest concept with no runtime representation.

**Primary value:** The PU topology becomes a first-class runtime data structure, enabling generic tree traversal, automatic thread lifecycle, and introspection by the editor.

---

### Candidate 4: Orchestrator Manifest (.diaflow)

**Home module/system:** DiaApplication/Manifest (new format + loader)
**Size:** M (1-3 weeks)

Introduce a new `.diaflow` manifest format that describes the complete application tree. A .diaflow file references .diaapp files by path and declares the parent-child relationships and data connections (e.g., FrameStreams) between PUs:

```json
{
  "version": 1,
  "root": "cluiche_main.diaapp",
  "children": {
    "MainProcessingUnit": [
      { "manifest": "cluiche_sim.diaapp", "data_links": ["InputToSimFrameStream"] },
      { "manifest": "cluiche_render.diaapp", "data_links": ["SimToRenderFrameStream"] }
    ]
  },
  "stages": [
    { "name": "DummyStage", "manifest": "stages/dummy_stage.diaapp" }
  ]
}
```

The editor opens a .diaflow to see the entire application. Individual .diaapp files remain editable standalone.

**Primary value:** One file gives a complete, declarative picture of the application topology without changing any runtime PU code.

---

### Candidate 5: Stage Manifests (Levels -> Stages with .diaapp)

**Home module/system:** DiaApplication + CluicheTest
**Size:** M (1-3 weeks)

Rename DummyStage to DummyStage. Each stage gets its own `.diaapp` manifest(s) describing the phases and modules it contributes. Stages don't create PUs -- they declare phase/module injections that the loader merges into parent PUs at load time. The manifest `imports` field links stage manifests into the application tree.

Example: `stages/dummy_stage.diaapp` declares `MainLoadPhase`, `MainFEPhase`, and their transitions, tagged with the target PU instance ID. The loader injects these into the already-constructed MainPU.

**Primary value:** Stages become visible in the editor as first-class manifest-backed entities instead of invisible code-only injections.

---

### Candidate 6: Activate Manifest Imports (Minimal)

**Home module/system:** DiaApplication/Manifest + DiaApplication/Loader
**Size:** S (<=1 week)

The `ApplicationManifest` already has `DynamicArrayC<const char*, 16> imports`. Implement import resolution in `ApplicationManifestLoader`: when loading a manifest, recursively load all imports, merge their PU/phase/module entries into the parent manifest, and detect cycles. No runtime PU changes -- this is purely a manifest-loading enhancement.

CluicheTest's `cluiche_main.diaapp` would gain `"imports": ["cluiche_sim.diaapp", "cluiche_render.diaapp"]` and the loader would produce a single merged `ApplicationManifest` containing all three PUs.

**Primary value:** Cheapest path to connecting manifests -- reuses existing data structures with no new formats or runtime changes.

---

### Candidate 7: Editor Connected Graph View

**Home module/system:** DiaApplicationEditor (spec + implementation)
**Size:** L (1-2 months)

Build the editor visualization that shows the full PU tree as a connected graph. The editor loads a root manifest (or .diaflow), follows imports, and renders:
- PU nodes (colored by thread: main=blue, dedicated=green)
- Phase flows within each PU (state machine arrows)
- Data links between PUs (FrameStreams)
- Stage boundaries (grouped overlay showing which stages contribute which phases)

This is the user-facing payoff of the runtime tree work. Depends on at least one of Candidates 3, 4, or 6 being done first so there's a connected structure to visualize.

**Primary value:** The developer sees the entire application thread topology and stage composition at a glance in the editor.

---

### Candidate 8: Convention-Only (No Runtime Changes)

**Home module/system:** Documentation + CluicheTest patterns
**Size:** S (<=1 week)

Don't change the runtime model. Instead:
- Establish a naming convention: the root PU is always named `<App>MainProcessingUnit`
- Organize manifests in a standard directory: `Data/Manifests/<app>.diaapp` (root), `Data/Manifests/stages/<stage>.diaapp`
- Rename DummyStage to DummyStage in CluicheTest
- Document the convention in the platform spec

The editor lists all .diaapp files in the manifest directory and lets you open any one. No tree view -- just a flat file browser with conventions.

**Primary value:** Zero runtime risk; solves the DummyStage -> DummyStage naming immediately; buys time to evaluate whether a full tree model is needed.

---

### Candidate 9: PU Data Links (Formalize FrameStreams)

**Home module/system:** DiaApplication (new DataLink/Channel abstraction)
**Size:** M (1-3 weeks)

Currently PU-to-PU communication happens via ad-hoc shared pointers (FrameStreams passed through StartData). Introduce a formal `DataLink` concept: typed, named channels registered on PUs that the tree model (or orchestrator manifest) connects. Each DataLink has a producer PU and consumer PU, making the data flow visible in the manifest and editor.

This is orthogonal to the tree structure but highly synergistic -- the editor can show not just ownership but data flow between PUs.

**Primary value:** PU communication becomes discoverable and manifest-driven instead of buried in application-specific StartData structs.

---

### Candidate 10: Incremental Bundle (3+6+5 -> 4 -> 7)

**Home module/system:** DiaApplication (phased delivery)
**Size:** L (1-2 months)

Not a single feature but a sequenced delivery plan combining candidates:

1. **Phase A (S):** Candidate 6 -- activate imports in manifest loader
2. **Phase B (M):** Candidate 3 -- PU parent-child tree in runtime
3. **Phase C (M):** Candidate 5 -- stage manifests (DummyStage -> DummyStage)
4. **Phase D (S):** Candidate 4 -- .diaflow orchestrator manifest (optional, may be unnecessary if imports suffice)
5. **Phase E (L):** Candidate 7 -- editor connected graph view

Each phase is independently shippable. Candidate 1 (ApplicationFlow root class) is folded into Phase B as the tree-root type. Candidate 2 (full rename) is explicitly deferred -- the root class solves naming without the churn.

**Primary value:** Delivers incremental value at each phase while building toward the full vision of a connected, visualizable application flow tree.

## Coverage Map

The candidates span the design axes as follows:

- **PU hierarchy**: Candidate 8 (keep flat) through Candidate 3 (full tree) with Candidate 6 (manifest-only linking) as middle ground
- **Naming**: Candidate 2 (full rename) vs. Candidate 1 (wrapper class) vs. Candidate 8 (convention only)
- **Stage manifests**: Candidate 5 (dedicated) vs. Candidate 8 (convention) vs. current (none)
- **Editor**: Candidate 7 (full graph) vs. Candidate 8 (file browser) vs. current (single file)
- **Scope**: S (Candidates 6, 8) through XL (Candidate 2), with Candidate 10 showing a phased path through the middle
- **Data flow**: Candidate 9 uniquely addresses PU-to-PU communication formalization

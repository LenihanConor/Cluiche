# Research: Explore -- DiaApplication Flow Tree

**Session date:** 2026-05-03
**Folder:** docs/research/diappl_flow_tree/

## Problem Space Overview

DiaApplication currently treats ProcessingUnits as flat, independent peers. CluicheTest manually creates three PUs (Main, Sim, Render), each described by a separate `.diaapp` manifest. MainPU hard-codes creation of SimPU and RenderPU in `PostPhaseStart()`, spawns threads, and joins them in `PrePhaseStop()`. There is no formal parent/child relationship, no root concept, and no way to see the complete thread topology as a connected tree.

This creates three interrelated problems. First, the name "DiaApplication" is ambiguous -- it could mean the running application, the framework, or the module itself, leading to confusion about what `ProcessingUnit` actually represents in the execution flow. Second, the PU hierarchy is implicit: MainPU knows about SimPU and RenderPU only because it manually creates them in application-specific code, making it impossible for the engine or editor to discover the full topology generically. Third, the editor can only open one `.diaapp` file at a time, with no way to show how multiple manifests connect into a complete application flow -- and game "levels" (like DummyStage) that inject phases into existing PUs have no manifest representation at all.

The goal is to research whether DiaApplication should evolve into a tree-structured flow model where PUs have explicit parent/child relationships, levels become "stages" with their own manifests, and the editor can visualize the entire connected graph.

## Existing Approaches

- **Unreal Engine**: World → Level → Sublevel hierarchy. Each level is a self-contained unit that can be loaded/unloaded. The "World Composition" view shows all levels and their spatial/logical relationships.
- **Unity**: Scene hierarchy with additive scene loading. Multiple scenes can be loaded simultaneously; a "master scene" bootstraps others.
- **Godot**: Scene tree where every node can contain child nodes. The entire game is a tree rooted at `/root`. SceneTree is the single authority for all execution.
- **Custom ECS engines**: Often use a "World" as the root container. Multiple worlds can run in parallel (e.g., Bevy's World), but there's usually an explicit top-level coordinator.
- **Application frameworks (Qt, WPF)**: Application → Window → Widget tree. The application object is always the root; windows are children with their own event loops.
- **Process supervisors (Erlang OTP)**: Supervision trees where a root supervisor owns child workers/supervisors. Crash isolation and restart policies are per-subtree. Relevant because PUs are thread containers.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| PU hierarchy model | Flat peers (current) / Parent-child tree / DAG / Hub-and-spoke | Tree is simplest extension; DAG adds complexity with little benefit |
| Root ownership | Implicit (MainPU by convention) / Explicit root node / Manifest-declared root | Explicit root lets the engine/editor discover topology without app-specific knowledge |
| Stage/Level manifests | No manifests (code-only, current) / Own .diaapp that imports into parent / Separate .diastage format | Reusing .diaapp keeps one format; separate format adds clarity |
| Manifest linking | None (current) / Import field (already in ApplicationManifest) / Separate orchestrator file | Import field exists but is unused -- cheapest path |
| Thread spawning | Manual per-PU (current) / Automatic by tree walker / Declarative in manifest | Declarative keeps app code clean; auto-spawn removes boilerplate |
| Naming | DiaApplication (current) / DiaApplicationFlow / DiaFlow / DiaRuntime | "ApplicationFlow" is most descriptive but longest |
| Editor visualization | Single .diaapp at a time (current) / Connected tree view / Full application graph | Tree view follows naturally from PU tree model |

## Known Tradeoffs

- **Explicit tree vs. flexibility**: A formal parent/child tree is easy to visualize but constrains PU topologies. If a future application needs a PU that communicates with two "parents" (fan-in), a strict tree won't support it. A DAG would, but adds complexity.
- **Naming disruption**: Renaming DiaApplication to DiaApplicationFlow touches every include path, namespace, vcxproj, and architecture doc. Large blast radius for a naming improvement.
- **Stage manifests**: Giving DummyStage (DummyStage) its own .diaapp means the level/stage contributes PU structure, not just phases. This is a shift in ownership -- currently levels inject into PUs they don't own.
- **Import resolution order**: If stages import PU definitions, the engine needs to resolve imports before constructing the PU tree. Circular imports must be prevented.
- **Editor complexity**: Showing a tree of connected .diaapp files requires the editor to load multiple manifests, resolve references, and render a graph. More work than single-file editing.
- **Backward compatibility**: CluicheTest's manual PU wiring must continue to work during migration. A big-bang rewrite of application startup is risky.

## Known Pitfalls (C++ / game engine context)

- **Thread ownership ambiguity**: If a parent PU "owns" children, does destroying the parent join child threads? Ownership semantics must be clear (Erlang-style supervision vs. fire-and-forget).
- **Circular manifest imports**: Must be detected at load time. The existing `imports` array in `ApplicationManifest` has no cycle detection.
- **Startup ordering**: Parent PU may need children running before it enters certain phases (e.g., MainPU needs RenderPU ready before rendering). Tree construction must support ordering constraints.
- **Memory lifetime**: PU tree nodes that hold raw pointers to children create dangling-pointer risks if children are destroyed out of order. Use UniquePtr or explicit ownership semantics.
- **StringCRC collisions across manifests**: When merging multiple .diaapp files into one tree, instance IDs must be unique across the entire application, not just per-manifest.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaApplication (ProcessingUnit, Phase, Module) | Direct target -- PU is the node type for the tree |
| DiaApplication/Manifest | Already has `imports` field and `ApplicationManifest::ProcessingUnitEntry` |
| DiaApplication/Loader | `ApplicationLoader::LoadApplication()` returns a single PU; needs to return a tree |
| DiaCore/Containers | DynamicArrayC, HashTable for tree node storage |
| DiaCore/Memory/UniquePtr | Ownership of child PUs |
| DiaEditor | Will visualize the PU tree; needs generic graph rendering |
| DiaApplicationEditor (specs) | 15 features approved but not implemented; manifest-load-save is the starting point |

### Existing Hooks

The `ApplicationManifest` struct already contains:
- `DynamicArrayC<const char*, 16> imports` -- unused but designed for linking manifests
- `DynamicArrayC<ProcessingUnitEntry, 4> processingUnits` -- a manifest can contain multiple PUs

`ProcessingUnit` already has:
- `ProcessingUnitTable` typedef (HashTable of StringCRC -> ProcessingUnit*) -- defined but seemingly unused
- `AddPhaseWithOwnership()` / `AddModuleWithOwnership()` -- UniquePtr ownership pattern ready to extend to child PUs

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | All PU, Phase, Module, and Stage instance IDs must be StringCRC. Cross-manifest uniqueness must be enforced. |
| PD-002 PU/Phase/Module hierarchy | Adding a PU-owns-PU layer extends but doesn't violate PD-002. The three-level internal structure stays intact. |
| PD-003 Component system | Stages (formerly levels) could become IComponentObjects if they need runtime composition. Or they stay outside the component system as application-flow constructs. |
| PD-004 No STL in public APIs | PU tree and stage APIs must use DiaCore containers. The existing `ProcessingUnitTable` typedef already does this. |
| PD-005 x64 only | No impact. |
| PD-007 C++20 | Enables concepts for PU tree traversal interfaces; std::span for child PU views. |
| PD-008 Directory.Build.props | No vcxproj output path changes needed unless a new project is created. |
| PD-009 Output under Cluiche/out/ | Stage manifests at runtime could be resolved relative to out/<AppName>/. |

## Open Questions for Ideation

- Should the PU tree be a compile-time construct (class hierarchy) or a runtime construct (data-driven from manifests)?
- Is renaming DiaApplication to DiaApplicationFlow worth the codebase churn, or can the confusion be solved with better documentation and a thin wrapper type (e.g., `ApplicationFlow` as a tree-root class)?
- Should stages (currently levels) own their own PUs, or only inject phases/modules into parent PUs? Owning PUs means stages can bring their own threads.
- How should the editor represent the tree -- a single "application flow" document that references .diaapp files, or a live multi-file view?
- Should the `imports` field in ApplicationManifest be the mechanism for connecting stages, or should a separate orchestrator manifest (e.g., `.diaflow`) describe the tree structure?
- What happens to DummyStage's constructor pattern (receives MainPU, SimPU, RenderPU as args)? Can it be replaced with manifest-driven discovery?
- Should PU-to-PU communication (currently via shared FrameStreams passed in StartData) be formalized as part of the tree model, or remain ad-hoc?

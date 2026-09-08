# Research Summary -- DiaApplicationFlow Flow Tree

**Session folder:** docs/research/diappl_flow_tree/
**Date:** 2026-05-03

## One-Line Answer

Evolve DiaApplicationFlow's flat PU model into a parent-child tree with manifest imports, stage manifests for game content, and an editor graph view -- delivered in four independent phases without renaming the module.

## Journey

1. **Explored:** DiaApplicationFlow treats ProcessingUnits as flat peers wired manually in app code. The `ApplicationManifest` already has an unused `imports` field and `ProcessingUnitTable` typedef. Industry precedent (Godot, Unity, Erlang OTP) overwhelmingly favors tree-structured execution hierarchies.
2. **Ideated:** 10 candidates generated, ranging from convention-only (S) through full module rename (XL), including a phased incremental bundle. Candidates spanned naming, runtime hierarchy, manifest formats, stage representation, editor visualization, and data links.
3. **Evaluated:** The incremental bundle scored highest (4.05) by capturing maximum engine and game value through phased delivery. PU parent-child tree (4.00) was the highest-scoring standalone feature. Full module rename scored lowest (2.00).
4. **Chose:** Modified bundle of C6 + C3 + C5 + C7. Naming rename dropped entirely. Stages inject phases, don't own PUs. No new file formats -- .diaapp only with imports.

## Chosen Work Item

**Name:** DiaApplicationFlow Flow Tree (Modified Incremental Bundle)
**Home module:** DiaApplicationFlow (existing)
**Suggested spec type:** System spec (with 4 child feature specs, one per phase)
**Estimated size:** L (1-2 months total across 4 phases)

### Delivery Phases

| Phase | Feature | Size | Dependencies |
|-------|---------|------|--------------|
| A | Activate Manifest Imports | S | None |
| B | PU Parent-Child Tree | M | Phase A |
| C | Stage Manifests (DummyStage -> DummyStage) | M | Phase A |
| D | Editor Connected Graph View | L | Phases A + B + C |

## Key Insights from Exploration

- **The `imports` field already exists** in `ApplicationManifest` but is completely unused. Activating it is the cheapest possible first step and immediately connects the three separate .diaapp files.
- **`ProcessingUnitTable` typedef is defined but unused** in ProcessingUnit.h -- it was designed for exactly this use case (HashTable of StringCRC -> ProcessingUnit*).
- **CluicheTest's manual PU wiring is fragile** -- MainPU hard-codes SimPU and RenderPU creation in `PostPhaseStart()`, passing FrameStreams through StartData. This pattern doesn't scale to stages or editor discovery.
- **Stages as phase injectors (not PU owners) matches DummyStage's current pattern** and avoids the complexity of stages bringing their own threads. This can evolve later if needed.
- **The full module rename (DiaApplicationFlow -> DiaApplicationFlow) is not worth the cost.** The naming confusion is real but can be managed without touching every include path in the codebase.
- **PU data links (formalizing FrameStreams) are valuable but orthogonal.** They can be added as a follow-up once the tree structure exists, and the editor graph view would benefit from them, but they're not a prerequisite.

## Pre-Spec Commitments

1. Stages inject phases into parent PUs; they do not create PUs or threads
2. Backward compatibility: CluicheTest's manual wiring works alongside the new tree during migration
3. .diaapp is the only manifest format (no .diaflow, no .diastage)
4. Cross-manifest ID uniqueness enforced at load time

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C1: ApplicationFlow Root Class | Unnecessary wrapper now that naming rename is dropped; root PU is the tree root directly |
| C2: Full Module Rename | XL effort, worst cost/risk ratio (2.00), naming solved cheaper by not solving it |
| C4: Orchestrator Manifest (.diaflow) | New format adds maintenance burden; imports + PU tree cover the same ground |
| C8: Convention-Only | Too shallow; DummyStage rename absorbed into C5 |
| C9: PU Data Links | Valuable but orthogonal; add as follow-up after tree exists |

## References

- docs/research/diappl_flow_tree/explore.md
- docs/research/diappl_flow_tree/ideate.md
- docs/research/diappl_flow_tree/evaluate.md
- docs/research/diappl_flow_tree/choose.md

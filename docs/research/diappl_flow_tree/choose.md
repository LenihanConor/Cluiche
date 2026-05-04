# Research: Choice -- DiaApplication Flow Tree

**Date:** 2026-05-03
**Chosen candidate:** Modified Incremental Bundle (C6 + C3 + C5 + C7)

## Rationale

The user's three original problems map to a clear set of candidates:

- **Problem 1 (naming):** Dropped entirely. No rename, no ApplicationFlow root class. The DiaApplication module name stays as-is. Revisit if confusion resurfaces.
- **Problem 2 (PU tree):** Activate manifest imports (C6) + PU parent-child tree (C3). Manifests connect declaratively via imports; runtime PUs form a real tree with parent/child ownership and automatic thread lifecycle.
- **Problem 3 (stages + editor):** Stage manifests (C5) + editor graph view (C7) + DummyLevel renamed to DummyStage (convention from C8). Stages become manifest-backed, visible entities; the editor shows the full connected tree.

The full module rename (C2) and orchestrator manifest (C4) are rejected. The ApplicationFlow root class (C1) is dropped since the root PU itself serves as the tree root without needing a wrapper.

## Delivery Sequence

| Phase | Candidate | Size | What ships |
|-------|-----------|------|------------|
| A | C6: Activate Manifest Imports | S (~1 week) | Manifest loader resolves imports, merges PUs, detects cycles. cluiche_main.diaapp imports sim + render. |
| B | C3: PU Parent-Child Tree | M (1-3 weeks) | ProcessingUnit gains AddChildPU/GetParent/GetChildren. Auto thread spawn/join. Root PU owns the tree. |
| C | C5: Stage Manifests | M (1-3 weeks) | DummyLevel -> DummyStage. Stages get .diaapp manifests declaring phase/module injections. Loader merges into parent PUs. |
| D | C7: Editor Connected Graph View | L (1-2 months) | Full tree visualization: PU nodes, phase flows, stage boundaries. Depends on A+B+C. |

Each phase is independently shippable.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| C1: ApplicationFlow Root Class | Dropped -- adds a wrapper class for naming clarity that isn't needed if we're not renaming. Root PU is the tree root. |
| C2: Full Module Rename | Worst cost/risk ratio (score 2.00). XL effort for a naming fix. |
| C4: Orchestrator Manifest (.diaflow) | Likely unnecessary -- manifest imports (C6) plus PU tree (C3) cover the same ground without a new format. |
| C8: Convention-Only | Too shallow on its own; the DummyLevel -> DummyStage rename is absorbed into C5. |
| C9: PU Data Links | Valuable but orthogonal. Can be added later to formalize FrameStreams; not needed for the tree or editor. |

## Pre-Spec Commitments

1. **Stages inject phases, not own PUs.** Stages declare phase/module contributions that the loader merges into parent PUs. Stages do not create new ProcessingUnits or threads. This matches the current DummyLevel pattern and keeps stage manifests lightweight. Can evolve later if a game needs stage-owned threads.
2. **Backward compatibility during Phase B.** CluicheTest's manual PU wiring (PostPhaseStart thread spawn) must continue working alongside the new parent-child tree. Migration is opt-in, not forced.
3. **No new file formats.** .diaapp is the only manifest format. No .diaflow, no .diastage. The imports field in ApplicationManifest is the linking mechanism.
4. **Cross-manifest ID uniqueness enforced.** When merging imports, the loader must reject duplicate PU/phase/module instance IDs across manifests.

## Next Step

Run /spec-system to create the system spec, then /spec-feature for each phase (A through D).
Suggested parent system: DiaApplication (existing system spec at docs/specs/systems/dia/diaapplication.md)

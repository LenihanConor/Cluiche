# Research: Evaluate -- DiaApplicationFlow Flow Tree

**Input:** docs/research/diappl_flow_tree/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability, capability, or architectural clarity for any application built on the engine
- **Game Value (0.20):** Improves CluicheTest as a demo/testbed, or makes game development workflows meaningfully better
- **Implementation Cost (0.25):** Inverse of effort -- 5 = very cheap (<1 week), 1 = very expensive (>2 months)
- **Risk (0.15):** Inverse of uncertainty -- 5 = well-understood with clear path, 1 = highly uncertain or breakage-prone
- **Cluiche Fit (0.15):** Aligns with existing module structure, PD-001 through PD-009, and the spec-driven workflow

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| 1. ApplicationFlow Root Class | 4 | 3 | 4 | 4 | 5 | 3.95 |
| 2. Full Module Rename | 3 | 1 | 1 | 2 | 3 | 2.00 |
| 3. PU Parent-Child Tree | 5 | 4 | 3 | 3 | 5 | 4.00 |
| 4. Orchestrator Manifest (.diaflow) | 4 | 3 | 3 | 3 | 4 | 3.40 |
| 5. Stage Manifests | 4 | 5 | 3 | 3 | 4 | 3.80 |
| 6. Activate Manifest Imports | 3 | 2 | 5 | 5 | 5 | 3.90 |
| 7. Editor Connected Graph View | 4 | 5 | 2 | 2 | 4 | 3.30 |
| 8. Convention-Only | 1 | 2 | 5 | 5 | 3 | 3.10 |
| 9. PU Data Links | 4 | 3 | 3 | 3 | 4 | 3.40 |
| 10. Incremental Bundle | 5 | 5 | 2 | 4 | 5 | 4.05 |

## Top 3 Candidates

### Rank 1: Incremental Bundle (score: 4.05)

**Why:** This isn't a single feature but a sequenced delivery of the most valuable pieces (Candidates 6, 3, 1, 5, optionally 4, then 7). It scores highest because it captures the full engine value (5) and game value (5) of the combined vision while managing risk through phased delivery (4) -- each phase ships independently, so if priorities shift mid-stream, you have real value at every stopping point. It also aligns perfectly with the spec-driven workflow (Fit: 5): each phase maps to a feature spec under a system spec.

**Watch out for:** The cost score (2) reflects total effort, not per-phase effort. The risk is that Phase B (PU tree in runtime) touches ProcessingUnit's core lifecycle -- thread spawning, teardown ordering, and backward compatibility with CluicheTest's manual wiring all need careful handling. The bundle also requires discipline to not gold-plate early phases.

### Rank 2: PU Parent-Child Tree (score: 4.00)

**Why:** This is the highest-scoring single feature. It's the keystone change that makes the PU topology a first-class runtime concept. Everything else -- stage manifests, editor visualization, data links -- becomes easier once PUs can discover their parents and children. Engine value is maximum (5) because this is a reusable architectural primitive, not CluicheTest-specific. Fit is perfect (5) -- it extends PD-002's PU/Phase/Module hierarchy with a PU-owns-PU layer that uses existing DiaCore containers (PD-004) and StringCRC IDs (PD-001).

**Watch out for:** The main risk is thread lifecycle management. If parent PU destruction auto-joins child threads, the teardown order must be deterministic. CluicheTest currently joins threads in `PrePhaseStop()` manually -- the migration path from manual to automatic thread management needs to be backward-compatible (support both patterns during transition).

### Rank 3: ApplicationFlow Root Class (score: 3.95)

**Why:** A thin, focused class that solves the naming confusion without any module rename. It gives the engine a single discoverable entry point (`ApplicationFlow`) that wraps the PU tree, which the editor and introspection tools query. The cost (4) is low because it's a new class addition, not a refactor. Risk (4) is low because it doesn't change existing PU behavior -- it layers on top. Perfect Cluiche Fit (5) since it lives in DiaApplicationFlow, uses StringCRC for lookup, and follows the existing UniquePtr ownership patterns.

**Watch out for:** On its own, this class is just a container. Its value depends on Candidate 3 (PU tree) being implemented -- without parent/child PUs, the ApplicationFlow root is just a wrapper around a single PU with no tree to traverse. Should be built alongside or after Candidate 3.

## Recommendation

**The Incremental Bundle (Candidate 10)** is the clear winner, scoring 4.05, because it captures the full value of the PU tree vision while mitigating risk through phased delivery. It directly addresses all three of the user's original concerns: naming (ApplicationFlow root class in Phase B), PU connectivity (parent-child tree in Phase B, imports in Phase A), and editor visualization of connected .diaapp files (Phase E).

The runner-up, PU Parent-Child Tree (Candidate 3, score 4.00), is effectively the core of the bundle -- it's the single most valuable standalone feature. The bundle wins by a thin margin because it sequences the cheap quick win (Candidate 6, activate imports) before the heavier runtime change, and includes stage manifests (Candidate 5) which the standalone PU tree doesn't.

The full module rename (Candidate 2, score 2.00) is decisively rejected -- it has the worst cost/risk ratio and the naming problem is solved more cheaply by the ApplicationFlow root class.

Platform constraint PD-002 (PU/Phase/Module hierarchy) is the key decision: the bundle extends PD-002 with a PU-owns-PU layer rather than replacing it, honoring the existing architecture while evolving it. This is consistent with how PD-002 was written -- it defines the three internal levels but says nothing about PU-to-PU relationships, leaving room for exactly this kind of extension.

# Research: Evaluate — DiaApplicationFlow / CluicheTest Simplification

**Input:** docs/research/diapp_simplif/ideate.md + discussion session (2026-05-08)

## Context

Through discussion, the 10 original candidates converged into a single composite approach: **Config-driven PU/Stage/Module architecture with unified streams**, implemented as a clean-break phased rewrite. The evaluation below scores the **implementation phases** to validate ordering and confirm the approach is sound.

## Scoring Criteria

- **Value (0.25):** How much confusion/complexity does this phase eliminate?
- **Dependency (0.25):** How much does other work depend on this being done first?
- **Risk (0.20):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Effort (0.15):** Inverse of cost — 5 = small, 1 = very large
- **Testability (0.15):** Can this phase be validated independently before moving to the next?

## Phases Under Evaluation

| Phase | Description | Size |
|-------|-------------|------|
| P1 | Framework core: Stage replaces Phase, new Module lifecycle (DoStart→kLoading/kReady/kFailed), app-wide TransitionTo, timeout/error handling | M (3 weeks) |
| P2 | Streams-in-config: StreamReader/StreamWriter handles, framework-owned streams, remove MessageBus/Observer from PU framework | M (2 weeks) |
| P3 | Module access & registration: ModuleRef<T> only, one-liner registration macro, remove GetModule/FindModule/DoBuildDependancies | S (1 week) |
| P4 | Config format v2: Merge .diaapp files, new schema (stages at root, streams section, module stage membership), validation at load | S (1 week) |
| P5 | CluicheTest migration: Rewrite all PU/Phase/Module subclasses to new API, delete old Phase classes, update all manifests | M (2-3 weeks) |
| P6 | Debug & E2E: IApplicationInspectable interface, DebugServerModule adapter, DiaAPI commands (get_app_state, transition_to, wait_stage_ready) | M (2 weeks) |
| P7 | Editor: Application Flow panel in CluicheEditor (graph view, stage timeline, module inspector, stream editor, validation, live mode) | L (4-5 weeks) |
| P8 | Test suite: New GoogleTests for framework (stage transitions, timeouts, rollback, validation, streams) | M (2 weeks) |

## Scores

| Phase | Value (0.25) | Dependency (0.25) | Risk (0.20) | Effort (0.15) | Testability (0.15) | Total |
|-------|-------------|-------------------|-------------|---------------|-------------------|-------|
| P1: Framework core | 5 | 5 | 3 | 3 | 4 | 4.20 |
| P2: Streams | 4 | 4 | 4 | 4 | 4 | 4.00 |
| P3: Module access | 4 | 3 | 5 | 5 | 5 | 4.20 |
| P4: Config format | 4 | 4 | 4 | 5 | 4 | 4.15 |
| P5: CluicheTest migration | 5 | 2 | 3 | 3 | 5 | 3.60 |
| P6: Debug & E2E | 3 | 2 | 4 | 4 | 5 | 3.35 |
| P7: Editor | 3 | 1 | 3 | 2 | 3 | 2.50 |
| P8: Test suite | 4 | 3 | 5 | 4 | 5 | 4.05 |

## Recommended Phase Order

### Tier 1: Foundation (must be first)

**P1: Framework Core (score: 4.20)** — Everything depends on this. The stage system, new Module lifecycle, TransitionTo, timeout/error/rollback. This IS the new DiaApplicationFlow. Highest dependency score.

**P3: Module Access & Registration (score: 4.20)** — Can be done in parallel with P1 or immediately after. Simplifies the API surface that P5 will consume. Low risk, high value.

**P4: Config Format v2 (score: 4.15)** — Defines the data contract that everything reads. Must exist before P5 (migration), P7 (editor), and P8 (tests need fixture manifests).

### Tier 2: Wiring (needs Tier 1)

**P2: Streams (score: 4.00)** — Needs P1 (framework owns streams) and P4 (config declares them). Well-scoped, testable independently.

**P8: Test Suite (score: 4.05)** — Write alongside Tier 1-2. Tests validate the framework before migrating CluicheTest onto it. Catches bugs early.

### Tier 3: Migration (needs Tier 1+2)

**P5: CluicheTest Migration (score: 3.60)** — The real proof. Rewrites all game code to new API. Depends on P1-P4 being stable. Highest effort after P7 but highest validation signal — if CluicheTest runs, the framework works.

**P6: Debug & E2E (score: 3.35)** — Can start alongside P5 or after. IApplicationInspectable needs the framework running. DiaAPI commands need CluicheTest as a test target.

### Tier 4: Tooling (needs everything)

**P7: Editor (score: 2.50)** — Lowest score because it depends on everything else being stable (format, framework, runtime to connect to). Highest effort. But also lowest risk to the engine — it's a separate app reading JSON.

## Recommended Execution Order

```
Week 1-3:   P1 (Framework Core) + P3 (Module Access) — parallel where possible
Week 3-4:   P4 (Config Format v2) + P8 begins (test fixtures)
Week 4-6:   P2 (Streams) + P8 continues (stream tests)
Week 6-9:   P5 (CluicheTest Migration) + P8 completes
Week 9-11:  P6 (Debug & E2E)
Week 11-16: P7 (Editor)
```

**Total estimated duration: 14-16 weeks** (3.5-4 months)

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|-----------|
| P1 takes longer than 3 weeks (new lifecycle is subtle) | Delays everything | Spec P1 thoroughly before coding; test-drive the lifecycle state machine |
| P5 reveals framework gaps (edge cases in real usage) | Rework P1-P4 | Start P5 with simplest PU (RenderPU) first; iterate |
| Config format needs revision mid-implementation | Ripple to P5, P7, P8 | Freeze format after P4; revision requires explicit decision |
| Editor scope creep (live mode, undo/redo, etc.) | P7 balloons to 8+ weeks | MVP: static editor only. Live mode is P7b (separate phase) |
| Old specs/code confusion during transition | Wrong API used in new code | Delete old code immediately in P5; don't leave both in tree |

## Critical Success Factors

1. **P1 spec must be written before implementation** — the lifecycle (kLoading/kReady/kFailed, timeout, rollback) has subtle edge cases. Spec them.
2. **P8 runs continuously from Week 3** — don't save all testing for the end.
3. **P5 is the proof** — if CluicheTest doesn't feel simpler to work with, the redesign failed its goal.
4. **P7 MVP is read-only** — validate the editor can display a manifest correctly before adding editing.

## Recommendation

The phased approach is sound. P1 is the critical path — it's the highest-risk phase with the most downstream dependencies. Invest spec time in P1 before writing code. Everything after P1 is well-understood (lower risk scores of 4-5).

The total 14-16 week estimate is realistic for a solo developer with AI assistance. The clean-break decision (no compatibility shim) saves ~2-3 weeks of shim code but requires P5 to be done in one push without a usable CluicheTest in between.

**Key trade-off accepted:** CluicheTest will be broken from the start of P5 until its end (~3 weeks). Plan for this.

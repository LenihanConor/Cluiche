# Implementation Plan: F-1 Harness (system)

**Spec:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md
**Status:** Not Started

System plan: tasks are the feature specs and the cross-cutting tooling work. Each feature has its own plan with the detailed tasks.

## Rules for every task

- Each task is a separate subagent with all context inlined (dispatch skill). Two-stage check after DONE: spec compliance, then code quality.
- Commit after each task, with the plan row updated in the same commit (`dia docs plan`).
- Fresh quoted run output for every code task (verify skill). New public API and behaviour follow TDD (RED quoted before GREEN).
- Build and run only through `dia run` / `dia launch`. Report pass/fail, failing tests and errors only.
- All edits outside `Cluiche/CoW/**`, `Cluiche/Assets/CoW/**` and `docs/specs/applications/cow/**` were signed off in the spec's Change Sign-off table (AD-001). Anything not in that table needs a new sign-off before it is edited.

## Phases

| Phase | Work | Spec / plan | Depends on |
|-------|------|-------------|------------|
| A | BootMenuModule promotion + `StageStatusModule` | bootmenu-promotion.md / .plan.md (7 tasks) | none |
| B | CoW app skeleton (boots to the Boot menu) | cow-app-skeleton.md / .plan.md | A for the menu; B1 and B2 can start immediately |
| C | Boot choice: Play entry only | boot-choice.md (1 task, folded into B6) | B |
| D | Tooling: `scaffold stage --app`, `new-cow-stage` skill, `dia scaffold app` (spec first) | see tasks below | B (the hand-built skeleton is the reference) |
| E | DiaMessageBus module doc status fix | below | none |
| Deferred | Bus live verification, bus traffic recorder, debug command host | features TBD | B |

## Tasks

| # | Task | Phase | Status | Model | Notes |
|---|------|-------|--------|-------|-------|
| 1 | Execute the promotion plan (its 7 tasks) | A | Not Started | sonnet | Sign-off: baseline + CluicheTest (approved) |
| 2 | Execute skeleton tasks B1 and B2 (DiaCLI `cow` target, `sln-add` path option) | B | Not Started | sonnet | DiaCLI only; independent of A, can run in parallel with task 1 (different files) |
| 3 | Execute skeleton tasks B3 to B5 (project and solution, manifests and `FrontEnd` stub, `Main.cpp`) | B | Not Started | sonnet | Needs task 1 complete (menu in baseline) and task 2 |
| 4 | Execute skeleton task B6 (verify end to end, including the boot-choice check and Visual Studio startup) | B, C | Not Started | haiku | Visual Studio F5 part is done by the user |
| 5 | Mark boot-choice spec Done; update system spec feature rows | C | Not Started | haiku | `dia docs spec-done` |
| 6 | **RED/GREEN:** `dia scaffold stage --app` (parameterise CluicheTest-specific names, paths and the 9 touch points) with tests | D | Not Started | sonnet | Uses `FrontEnd` as the reference. Existing CluicheTest scaffold output must be unchanged |
| 7 | Implement the `new-cow-stage` skill per [new-cow-stage-skill.md](new-cow-stage-skill.md) | D | Not Started | haiku | After task 6. Spec Draft; trial run needs a locked testbed design |
| 8 | `dia scaffold app` feature spec: [scaffold-app.md](../../../dia/systems/diacli/scaffold-app.md) | D | Draft written, awaiting approval | opus | Written by interview. Includes the data-driven target registry |
| 9 | Implement `dia scaffold app` per its spec (its 5 tasks); prove by generating `ScratchApp` outside the committed tree and booting it | D | Not Started | sonnet | Needs task 8 approved and the registry refactor signed off. Its task 1 may run before the skeleton and make skeleton B1 toml-only. Do not commit the throwaway app |
| 10 | Verify the DiaMessageBus tests pass, then correct `status: stub` in `dia.messagebus.architecture.module.md` | E | Not Started | haiku | Doc only; confirm with a fresh `dia run googletest --filter="*MessageBus*"` first |
| 11 | Observation opportunity scan across all touched files; log suggestions in the notes | A to D | Not Started | haiku | Does not block Done |
| 12 | Mark the F-1 system spec Done only when all child features are Approved or Done; record the deferred features | all | Not Started | haiku | System spec rule |

## Order and parallelism

```
Task 1 (promotion) ──────────────┐
Task 2 (DiaCLI target, sln-add) ─┴─> Task 3 (skeleton) -> Task 4 (verify) -> Task 5
                                               │
                                               └─> Task 6 -> Task 7
                                               └─> Task 8 -> Task 9
Task 10 any time
Task 11, 12 last
```

Tasks 1 and 2 touch different files (baseline and CluicheTest versus DiaCLI and `pipeline.toml`) and may run in parallel. Everything else is sequential.

## Risks

- Module `config` may not reach `BootMenuModule` (promotion plan, task 4). If not, use a default and tell the user; do not change Dia.
- Trimming `cluiche_main.diaapp` and `assets.catalogue.json` for CoW is trial and error. Each trim is validated with `dia validate manifest` and a launch before the next.
- `dia scaffold app` is only as good as the hand-built skeleton, so it waits for it.
- The user does the Visual Studio F5 check.

## Deferred (not in this plan)

Bus live verification, the bus traffic recorder (Dia or CoW-Shared still to decide), the debug command host, and shipping gating (dropped).

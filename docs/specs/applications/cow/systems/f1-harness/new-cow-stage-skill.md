# Feature Spec: new-cow-stage Skill

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md
**Mirrors:** `.claude/skills/new-cluichetest-stage.md`
**Depends on:** `dia scaffold stage --app` (system plan task 6) and the CoW app skeleton
**Status:** `Draft`

## Summary

A skill that creates a new CoW testbed stage. It differs from the CluicheTest skill in that CoW stages are driven by the GDD, so the skill reads the testbed's design entry first, then runs `dia scaffold stage <Name> --app CoW`, then adds testbed-specific C++.

## Goals

1. `/new-cow-stage <testbed id or name>` (for example `S-2` or `Dragon`) scaffolds the stage, wires it into the Boot menu, and builds.
2. Design intent comes from the GDD, never invented in the skill (AD-003).
3. Modes and presets become panels inside the one stage (AD-005), never separate stages.

## Non-Goals

1. Writing the testbed's simulation or visual content.
2. Editing the GDD (it lives in the DesignDocs repo, owned by the design session).
3. A generic multi-app stage skill; this one is CoW-specific.

## Design

Instructions the skill gives Claude, in order:

1. Resolve the testbed from `docs/specs/applications/cow/cow.md` Systems table (id, name, track). If the id is not listed, stop and ask.
2. Read the testbed summary in the GDD `10_PrototypeStrategy/TestbedRoster.md`, and its own testbed file if one exists (for example `F1_Harness.md`). If the design is not locked (not struck in `cow-backlog.md`'s "Features to Be Designed"), warn the user and ask whether to scaffold a skeleton stage anyway.
3. Derive the PascalCase stage name (`<Name>TestbedStage`) and choose modules from the testbed's systems, using the baseline module patterns. Unknown systems go to `dia-needs.md` per AD-002, not into the stage.
4. Run `dia scaffold stage <Name> --app CoW [--modules ...] [--budget ...]`.
5. Add a panel stub for modes and presets named in the testbed summary (for example isolated and live for S-2).
6. Add the stage to `cow-backlog.md` notes, build and launch with `dia run cow`, confirm the stage appears in the Boot menu, launch it, and return.
7. Update the relevant system spec or plan row; do not touch the GDD.

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | The skill file `.claude/skills/new-cow-stage.md` and command file `.claude/commands/new-cow-stage.md` exist with the usage and instructions above. |
| AC2 | The skill refuses to scaffold a testbed that is not in the CoW Systems table. |
| AC3 | The skill reads the GDD roster entry before scaffolding and warns when the design is not locked. |
| AC4 | Running it for a real testbed produces a stage listed in the Boot menu that launches and returns. |
| AC5 | Modes and presets appear as a panel in the one stage, not as extra stages. |

## Tasks

| # | Task | Test | Status | Model |
|---|------|------|--------|-------|
| 1 | Write the skill and command files from `new-cluichetest-stage.md`, replacing the domain patterns with the GDD lookup and CoW paths | AC1, AC2, AC3 | Draft | haiku |
| 2 | Trial run on the first real testbed stage after `scaffold stage --app` exists (the stub stage cannot prove AC5); fix the skill from what it shows | AC4, AC5 | Draft | sonnet |

## Binding Decisions

| Source | Constraint |
|--------|------------|
| AD-003 | The GDD owns design; the skill reads it and never rewrites it. |
| AD-005 | One stage per testbed, modes as panels. |
| AD-001 | Any needed Dia or baseline change is proposed, not made. |

## Open Design Questions

1. The GDD repo is a sibling checkout at `C:\GitHub\DesignDocs\COW`. The skill hard-codes that path; if it is elsewhere on another machine the lookup fails. Option: read the path from an environment variable with this as default.
2. Task 2 is blocked until a real testbed design is locked, so the skill ships unproven until then.

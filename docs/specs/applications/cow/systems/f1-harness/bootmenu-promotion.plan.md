# Implementation Plan: BootMenuModule Promotion

**Spec:** @docs/specs/applications/cow/systems/f1-harness/bootmenu-promotion.md
**Status:** Not Started

## Gate

Tasks 1 to 3 edit CluicheGameBaseline and CluicheTest. Per AD-001 each needs explicit human sign-off before it is dispatched. Commit after each task; the plan row is updated in the same commit (`dia docs plan`).

## Implementation Patterns

- Module shape: follow `DebugUIModule` (baseline, RenderPU): `kTypeId`, `kAllowedPUs`, `kDescription`, `DIA_MODULE`/`DIA_DESCRIBE` via the namespace-alias pattern.
- Optional sibling access: `ModuleRef<StageStatusModule>`; `Get()` returns null unless the target is `kActive`.
- Test helpers ship with the library under `<Module>/Testing/`, not in GoogleTests.
- Project files: use `dia docs vcxproj-add <Project> <File>`; move (do not copy) `BootMenuModule` and update CluicheTest's vcxproj to drop it.
- Scaffold module docs where the baseline requires them (`dia check deps` after).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **RED:** write tests for `StageStatusModule` (`Set`/`Get`, unknown stage returns null, overwrite, capacity limit). Prove RED with quoted output. | AC2 | Done | sonnet | TDD. Needs sign-off (baseline).; RED proven: 4/5 StageStatus tests fail on assertions (stub Set/Get). Signed off (baseline) |
| 2 | **GREEN:** add `StageStatusModule` (header, cpp, `Severity`/`StageStatus`, vcxproj, filters, module doc). Refactor. | AC2 | Done | sonnet | Needs sign-off (baseline).; 5/5 StageStatus tests pass; baseline has no module docs |
| 3 | **RED:** write tests for the promoted menu's pure logic (visited bitfield, severity counts, config defaults, no-status behaviour) via an extracted testable helper in `Testing/`. Prove RED. | AC3, AC4, AC5, AC6 | Done | sonnet | Menu draw code itself is covered by task 6.; RED: 8/10 BootMenuLogic tests fail on assertions; helper BootMenuLogic in Modules/ |
| 4 | **GREEN:** `git mv` `BootMenuModule` to `Cluiche/CluicheGameBaseline/Modules/`; remove all `TestResultsRegistry` use; add optional `ModuleRef<StageStatusModule>`; read `title`/`project` from module `config`; footer only when a status module exists; generalise the `DIA_DESCRIBE` text; mark visited on launch; fix includes and vcxproj in both projects. | AC1, AC3, AC4, AC5, AC6 | Done | sonnet | Needs sign-off (baseline + CluicheTest). Confirm module `config` is delivered to `BootMenuModule` (spec open point).; Moved via git mv; config via OnConfigure JSON (no Dia change); 15 tests pass; cluichetest compiles. StageStatus instance id must be 'StageStatus' |
| 5 | Add `TestStatusBridgeModule` to CluicheTest (RenderPU, Boot stage): maps `TestResultsRegistry` to `StageStatusModule` on start and each update with the spec's label/severity/visited mapping. Update `cluiche_main.diaapp`: add `StageStatus` and bridge instances, make `BootMenu` depend on the bridge, set title/project config to the existing strings. Update `CluicheTest.vcxproj`. | AC7 | Not Started | sonnet | Needs sign-off (CluicheTest). Run `dia validate manifest`. |
| 6 | Verify: `dia run googletest`, then `dia run cluichetest`; launch and return through every Boot stage; check title, badge text and colour, visited markers, counts, and `navigate_to` automation. Compare against the parity table. | AC7, AC8, AC9 | Not Started | haiku | Quote fresh output (verify gate). Report pass/fail only per build-output rule. |
| 7 | Run `dia check deps`, `dia docs precommit`; observation opportunity scan; mark the feature spec Done with `dia docs spec-done`; update the F-1 system spec feature row. | All | Not Started | haiku | |

## Dependencies

1 then 2 then 3 then 4 then 5 then 6 then 7. Strictly sequential: tasks 4 and 5 touch the same vcxproj and diaapp, and 3 needs task 2's types.

## Risks

- Module `config` may not reach `BootMenuModule` (the existing diaapp `config` block at line ~529 belongs to another module); if unsupported, flag it to the user before changing Dia, since it would be a new Dia capability.
- Start order: the menu restores visited markers in `DoStart`, so the bridge dependency in the diaapp must be declared or markers are lost after returning to Boot.
- The running-stage footer edge case differs from today by design (see the spec's parity table).

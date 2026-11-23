# Plan: DiaModulePlacement

**Spec:** @docs/specs/applications/dia/systems/diamoduleplacement/diamoduleplacement.md
**Status:** Not Started (spec is Draft; start only once it is Approved)

Moves 11 module classes out of `Dia/`, deletes 1 (`MetricsCollectorModule`) and renames 1 base class (`ObservationModuleBase`); spec table has the detail. One task per module so each commit builds and tests on its own. Models are sonnet or haiku; **ask before using opus** (feedback_confirm_opus).

## Speed-ups (decided before starting)

- **Narrow filters per task**, one full sharded GoogleTest run and one Release build at close-out (task 14), `--shards >= 2`. No sanitizer, no mutation checks.
- **Batch by risk, not by size.** Task 1 clears the arch violation that triggered this; do it first so the gate improves immediately. Tests-only modules (tasks 2-4) are the cheapest and prove the recipe before the app-facing ones.
- **The recipe is the same every time** (Implementation Pattern below), so tasks 2+ are mostly mechanical and can be dispatched to sonnet with the recipe inlined.
- Tasks that touch different packages and different app files can run in parallel, but they all edit the same `.vcxproj` files, so run them one at a time unless two touch disjoint projects (for example task 8 CluicheTest and task 6 Baseline are fine together; two Baseline tasks are not).

## Implementation Pattern (every move task)

1. Locate every consumer: `#include` of the module header in `Cluiche/` and `Cluiche/Tests/`, every subclass, and every `.diaapp`/manifest that lists the module's `kTypeId` string.
2. Move `<Module>.h/.cpp` to the destination (Baseline `Modules/` or the one app). Keep the class name, namespace, `kTypeId` string and `DIA_DESCRIBE` text byte-identical (MP-001). Fold into an existing host/subclass where one exists.
3. Update includes, `.vcxproj` and `.filters` with `dia docs vcxproj-add` (and remove from the Dia project). Never leave a file in two projects.
4. Move module-lifecycle tests beside the module; leave library tests in the package (MP-004). Add a test only if a moved test lost its setup.
5. Update the package's `dia.*.architecture.module.md`: drop `dia.application` if now unused, re-review `layer:`.
6. Verify, with quoted output: `dia run googletest --filter="<scoped>" --shards 2`, `dia check deps`, `dia check arch`.
7. Commit: `refactor(<Package>): move <Module> to <destination> (PD-012)`. Update this plan row in the same commit.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Audit: for every module in the spec table, confirm consumers, subclasses and manifest listings (`.diaapp`, `.diastage`, test manifests). Questions 2 and 3 are partly settled in the spec (ValueSchema live, MetricsCollector dead, Sensor unused); re-check against `.diastage` and test manifests too. Output: a short table in this plan's notes | none (read-only) | Not Started | sonnet | Gates task 3 |
| 1 | `MessageBusModule` -> Baseline. Fold into `MessageBusHostModule`; remove `MessageBusModule.h/.cpp` from `DiaMessageBus`; fix CluicheTest `TestableMessageBusModule`; move the 3 lifecycle tests, keep Bus tests. Check CluicheTest and GoogleTests can reach Baseline (both reference it today) | `*MessageBus*`; `dia check arch` drops the messagebus violation | Not Started | sonnet | **Clears the arch violation that started this.** Risk: Dia-side tests lose lifecycle coverage, so port them, do not delete |
| 2 | `ValueSchemaModule` -> Baseline (live: loaded from `cluiche_main` and `cow_main` by type id; 7 tests). It self-registers with `DIA_MODULE`, so the registration moves with it and the manifests need no edit | `*ValueSchema*`; a CluicheTest and a CoW start-up | Not Started | sonnet | Prove manifests still resolve `ValueSchemaModule` |
| 3 | `SensorModule` -> Baseline (decided: keep; no app or manifest uses it today, so no manifest edit). Keep the `kInstanceId` string. Move the module-driving tests in `TestDiaSensor.cpp` to a Baseline test folder; component tests stay with the package. Drop `dia.application` from the package manifest | `*Sensor*` | Not Started | sonnet | Unused code in Baseline until a game loads it |
| 4 | `DiaMainTime`, `DiaRenderTime` -> Baseline | `*SimTime*` | Not Started | sonnet | Move together with task 5 if they share headers |
| 5 | `DiaSimTimeModule` -> Baseline (CluicheTest, CoW, 4 tests). CoW `SaveLoadModule` looks it up by `kTypeId`, so MP-001 matters most here | `*SimTime*`, `*SaveLoad*` | Not Started | sonnet | Re-review `dia.simtime` layer afterwards (relabelled gameplay on 2026-11-23) |
| 6 | `AIBudgetModule` -> Baseline (Baseline already includes it) | `*AIBudget*` | Not Started | sonnet | |
| 7 | `CellLayersModule` -> Baseline (CluicheTest, CoW, 3 tests; both read its `kServiceStreamId`) | `*CellLayers*`; CellLayersViewer stage | Not Started | sonnet | Keep `kServiceStreamId` value unchanged. Re-review `dia.celllayers` layer |
| 8 | `DeferredStepModule` -> Baseline (CluicheTest, CoW, 3 tests) | `*DeferredStep*`, `*WorldStep*` | Not Started | sonnet | Sequence with the `DiaParallelExecutor` backlog row: do not run both at once, they touch the same package |
| 9 | `CellHistoryModule` -> CluicheTest (the only app user) | `*CellHistory*` | Not Started | sonnet | |
| 10 | `EntitySpawnerModule` -> CluicheTest; fold the existing subclass in `SpawnerTestStageModule`; move the 2 tests | `*EntitySpawner*`, spawner stage | Not Started | sonnet | |
| 11 | `TriggerScriptModule` -> CluicheTest | `*TriggerScript*` | Not Started | sonnet | |
| 12a | Delete `MetricsCollectorModule`: `.h/.cpp`, vcxproj entries, the `known_types.json` entry in `DiaApplicationFlowEditor/Assets` (the built copy under `bin/` regenerates) | build of CluicheEditor, CluicheTest, CoW | Not Started | haiku | Confirm nothing references it first (task 0 found none) |
| 12b | Rename the Dia base `ObservationModule` -> `ObservationModuleBase` (files, class, log strings, vcxproj). Update the two subclasses in `CluicheGameBaseline` and `CluicheEditor` to derive from it. Their own class name `ObservationModule` and type id `"ObservationModule"` stay unchanged (MP-001) | build of CluicheEditor, CluicheTest, CoW; `*Observation*`; start CluicheTest and the editor and confirm the observation session starts | Not Started | sonnet | Touches the framework package every module depends on; ask before escalating to opus |
| 13 | Guard (exempt the framework base classes in `DiaApplicationFlow`, including `ObservationModuleBase`): fail `dia docs precommit` (or add an arch rule) when a class under `Dia/` outside the `DiaApplicationFlow` base files derives from a `*Module`. Tests for the guard. Re-review every affected package layer, re-run `dia check arch`/`deps`, regenerate the registry | pytest for the guard; `dia check arch`, `dia check deps`, `dia docs precommit` | Not Started | sonnet | Also reconsider `dia.celllayers`/`cellhistory`/`simtime`/`valueschema` layers: several may relabel lower once the wrapper is gone |
| 14 | Close-out: gap audit (anything still referencing a removed header, docs that name the old locations, spec tables), full sharded GoogleTest run once, one Release build, the E2E stages for moved modules, update PD-012 text if wording needs "as with Dia" confirmed true | full suite sharded; E2E: MessageBus, Spawner, CellLayers, DeferredStep stages | Not Started | sonnet | Each remaining gap ends as fix-now, promote to a backlog row, or drop |

## Close-out checklist

- `grep -rn "public.*Module" Dia/` finds only the framework base classes.
- `docs/specs/applications/dia/systems/diamoduleplacement/diamoduleplacement.md` Status updated; `dia docs spec-done` when finished.
- Backlog: the "CI architecture gate is red" row is updated (the messagebus violation is gone), and a row exists for the remaining arch violation (`celllayers` -> `pathfinding`).

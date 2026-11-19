# Plan: Boot Menu View (Stage Catalogue Feature 4)

**Spec:** @docs/specs/applications/dia/systems/stagecatalogue/boot-menu-view.md
**Status:** Done
**Prereq:** Features 1, 2, 3 Done. Mockup gate (AC 1) cleared; `mockups/boot-menu.html` is the visual reference.

## Findings (Task 1 audit, 2026-11-19)

- Readers of `StageStatusModule`: only `BootMenuLogic` and `BootMenuModule` (and `TestBootMenuLogic.cpp`, `TestStageStatusModule.cpp`). No HUD or other reader exists, so AC 14 has nothing to migrate.
- `TestStatusBridgeModule` is referenced only by `cluiche_main.diaapp`, `registeredtypes.diaschema`, the CluicheTest vcxproj and its own files. `TestResultReporterModule` (F3) already sends `StageResultEvent`.
- `StageCatalogueSnapshot` FrameStream (MainPU to RenderPU) is already declared in `cluiche_main.diaapp` and `cow_main.diaapp`; BootMenu needs a `reads` channel entry in each.
- Open Design Questions: no `expanded_groups` config; keyboard navigation steps by row (spec default). Revisit only if the user asks.

## Implementation Patterns

- Namespace `Cluiche::AppFlow`. Fixed-size containers, no STL in public headers. Never `str = cstr` on `String<N>`.
- `BootMenuLogic`: no ImGui, no module machinery. Input is `const StageCatalogueSnapshot&` plus a flat fallback list. Rows are built per kind group (`StageCatalogue::GroupByKind` order Showcase, Tools, Tests, Other); state (selected, visited, result) keyed on the stage.
- Keep the existing preset sub-row API from F2 (`Row`, `PresetInfo`) working over snapshot presets.
- Test helpers: `Testing/StageCatalogueFixture.h` exists for fixture snapshots; tests in `Cluiche/Tests/GoogleTests/CluicheGameBaseline/`.
- Add/remove files with `dia docs vcxproj-add` (PowerShell or literal paths); removals edit vcxproj + filters by hand.
- Observation (AC 15) is part of the module task.
- Scoped `--filter` runs with `--shards 2`; one full run at close-out. `dia run|launch` only. Kill leftover `CluicheTest.exe`/`CoW` before E2E.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Audit readers of `StageStatusModule`/`TestStatusBridgeModule` (AC 14) | grep | Done | haiku | See Findings: no HUD reader, nothing to migrate |
| 2 | **RED → GREEN:** rebuild `BootMenuLogic` over `StageCatalogueSnapshot`: groups in order with hidden empty groups and counts, N-kind stage = N rows, per-stage selection/visited/result, tag chips AND + case-insensitive name/description search, chip list (kinds first, then alphabetical), badges only on `test` stages, Tests rollup counting unique stages (visited without result = pending), session collapse state (Tests collapsed), revision change keeps selection/filters/collapse, flat fallback list; remove the `StageStatusModule` dependency | AC 2-8: unit tests per bullet in Test Plan | Done | sonnet | RED-GREEN; rewrite `TestBootMenuLogic.cpp`; RED not run separately (new API, compile-only failure); 18 tests pass x2 shards. Warn counts as passed in rollup. Logic ~90 KB: hold off-stack. Baseline does not build until task 3 |
| 3 | **RED → GREEN:** `BootMenuModule` view: FrameStream reader of `StageCatalogueSnapshot`, grouped collapsible table, chips, search box, description second line + tooltip, badges, preset sub-rows, 2 s fallback flat list with one Warning, launch sends `BootMenuNavRequest` (with preset where F2 supports it), logs + `boot_menu.stage_count` unique gauge; add `reads` channel to `cluiche_main.diaapp` and `cow_main.diaapp`; compile-code for cluichetest and cow | AC 9-12, 15: compile + `dia validate manifest`; view logic covered by task 2 tests | Done | sonnet | Match the mockup; module drops `StageStatus` ModuleRef; Compile cluichetest+cow pass; validate manifest 80/0; 9 tests/shard; UI not yet seen visually |
| 4 | Retire `StageStatusModule` and `TestStatusBridgeModule`: delete files, `.diaapp` entries and dependencies, `registeredtypes.diaschema` entries, vcxproj + filters (Baseline, CluicheTest, GoogleTests), `TestStageStatusModule.cpp`; update module docs | AC 13: build clean, `dia validate manifest` passes, grep shows no references | Done | sonnet | Depends on 2 and 3; Validate 80/0; 39 tests pass; cluichetest+cow compile; grep clean. registeredtypes.diaschema commit also carries earlier uncommitted F1/F3 type entries (generated) |
| 5 | E2E: update the existing boot-menu E2E to the grouped layout; `stages.list` row count equals sum over stages of max(1, kinds) | AC E2E in Test Plan | Done | sonnet | Kill leftover exes; `dia test e2e` does not build, compile-code first; 1 pass (stage_count gauge == catalogue stages), 1 other test (now passing). Gap fixed after close-out: stages.list now replies {"stages":[...]}; log-row check replaced by boot_menu.row_count gauge (log.jsonl is locked during the run).|
| 6 | Close-out gap audit and edge tests (no mutation checks). One full `dia run googletest --shards 2`. Observation scan, spec + plan Done, backlog updated, user manual visual sign-off items listed | Full suite pass | Done | sonnet | Sanitizer only on request; Full googletest --shards 2: 16496 passed, 0 failed (11 skipped, pre-existing). Observation: AC 15 logs/gauge present, no High findings; Low: no health reporter for snapshot staleness. Gaps: stages.list wire bug (needs decision), built-log E2E skipped (log locked), manual visual sign-off pending |

## Notes

- Report results only (pass/fail, failing names, errors).
- Commit after each task; update this plan in the same commit.
- Task status: Pending → In Progress → Done (`dia docs plan` rejects Pending → Done).

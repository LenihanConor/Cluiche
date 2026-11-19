# Feature Spec: Boot Menu View

**Parent:** @docs/specs/applications/dia/systems/stagecatalogue/stagecatalogue.md (Feature 4)
**Status:** Approved (2026-11-18)

## Summary

`BootMenuModule` (CluicheGameBaseline, RenderPU) is rebuilt as a view over the `StageCatalogueSnapshot` from Feature 1:

- **Kind groups:** Showcase, Tools, Tests, Other.
- **Multi-group rows:** a stage with several kinds appears under each.
- **Filtering:** tag chips and a text search.
- **Descriptions** on each row.
- **Badges:** pass/fail on `test` stages only.
- **Tests group:** collapsed by default, with a passed/failed/pending rollup in its header.

`StageStatusModule` and CluicheTest's `TestStatusBridgeModule` are retired, because the catalogue's results replace them.

**Gate:** an approved HTML mockup of the menu comes before any code.

## Goals

- At a glance you can tell a showcase from a tool from a test.
- Finding one stage among 30+ takes a chip click or a few typed characters.
- CoW and CluicheTest use one menu, with badges in both.
- The menu holds no business logic: it reads the snapshot and sends requests.

## Acceptance Criteria

### Mockup gate

1. Before any code, `docs/specs/applications/dia/systems/stagecatalogue/mockups/boot-menu.html` shows the following, and the user approves it:
   - the four groups, with Tests collapsed and its rollup;
   - a stage listed under two groups;
   - tag chips with one active;
   - the search box;
   - descriptions;
   - badges;
   - preset sub-rows, laid out for Feature 2;
   - the empty and fallback states.

### Pure logic (`BootMenuLogic`, rebuilt)

2. `BootMenuLogic::SetSnapshot(const StageCatalogueSnapshot&)` builds rows from `GroupByKind` order. It has no ImGui and no module machinery, so it is unit-testable.
3. **Groups and rows:**
   - Group order is Showcase, Tools, Tests, Other, and empty groups are hidden.
   - Each group header shows its stage count.
   - A stage with N kinds has N rows.
4. **Per-stage state:** selection, visited and the result belong to the stage.
   - Selecting or launching from any row marks every row of that stage.
   - "N stages available" and the `boot_menu.stage_count` gauge count unique stages.
5. **Filters:** active tag chips are AND-ed. Search is a case-insensitive substring match on the name and description. A group whose rows are all filtered out is hidden.
   - The chip list is every tag in the catalogue: kinds first, then the rest alphabetically.
6. **Badges:** shown only on stages with the `test` kind, on every row of the stage. The label and colour come from the result: ok green, warn amber, fail red, and none or missing grey "PENDING".
7. **Tests header rollup:** `Tests (P passed, F failed, X pending)`, counting unique test stages. A stage that was visited but has no result counts as pending.
8. Collapse state is session-only. Tests starts collapsed and the other groups start expanded. The state survives a return to Boot within the process.

### Module

9. `BootMenuModule` reads `StageCatalogueSnapshot` through a FrameStream reader. A new revision rebuilds the rows and keeps the selection, collapse and filter state.
10. **Launch:** clicking a stage row sends `BootMenuNavRequest {target}`, as today. Feature 2 adds the preset field.
11. **Description:** shown as a dimmed second line under the stage name (truncated to the row width), with the full text in a hover tooltip.
12. **Fallback:** with no snapshot within 2 s of `DoStart` (the app has no `StageCatalogueModule`), the menu shows today's flat list from `GetStageTransitions(Boot)` under one "Stages" heading. It logs one Warning, and chips, search and badges are hidden.

### Retirement

13. `StageStatusModule` and CluicheTest's `TestStatusBridgeModule` are deleted, along with their `.diaapp` entries, vcxproj entries and tests. Their behaviour is covered by Feature 1's results and AC 6-7 here.
14. CluicheTest's HUD and any other reader of `StageStatusModule` are moved to the snapshot or `stages.results`, or listed as a blocker. Task 1 of the plan greps for every reader.

### Observation

15. Logs:
    - menu built (stages, rows, groups);
    - fallback used;
    - filter or search change, at Debug only;
    - launch (stage, group it was launched from).

    The `boot_menu.stage_count` gauge counts unique stages.

## Files Touched

- **CluicheGameBaseline:**
  - `Modules/BootMenuLogic.*` and `Modules/BootMenuModule.*`
  - `StageStatusModule.*` (deleted)
  - vcxproj and filters
- **CluicheTest:**
  - `Modules/TestStatusBridgeModule.*` (deleted)
  - `cluiche_main.diaapp`
  - any `StageStatusModule` readers
- **CoW:** `cow_main.diaapp`, if the snapshot reader needs a stream entry.
- **Tests:** `BootMenuLogic` tests rewritten; `StageStatusModule` tests removed.
- **Docs:** the mockup HTML.

## Binding Decisions

| Decision | How this feature complies |
|---|---|
| SC-004: multi-group rows, state keyed on the stage | AC 3-4, 7 |
| SC-005: E2E stages launchable from the menu | Test rows launch like any other (AC 10). Collapsing the Tests group hides nothing permanently |
| SC-010: menu is a view, actions are commands | The menu reads the snapshot and sends the same navigation request `stages.launch` uses in Feature 2. All grouping and filter logic is in the pure `BootMenuLogic` |
| PD-002 / AD-004: PU boundaries are streams | The menu (RenderPU) reads only the FrameStream snapshot. The cross-PU `TestResultsRegistry` read in `TestStatusBridgeModule` is removed |
| Editor mockup rule | AC 1 gates all code |

## Test Plan

- **GoogleTests** (pure `BootMenuLogic`, using fixture snapshots):
  - group order and hidden empty groups;
  - two-kind stage: two rows, one stage count;
  - visited and selection shared across rows;
  - chip AND and search match on name and description;
  - badge only on test stages;
  - rollup counts unique test stages, and visited with no result counts as pending;
  - new revision keeps the selection and filters;
  - fallback flat list when there is no snapshot.
- **E2E:** CluicheTest boots and `stages.list` still matches what the menu shows (row count = sum over stages of max(1, kinds)). The existing boot-menu E2E is updated to the grouped layout.
- **Manual (user visual sign-off against the mockup):**
  1. `dia run cow` shows Showcase / Tools / Tests.
  2. Tagging a stage `tool` and `showcase` shows it in both groups.
  3. A chip filter and a search work.
  4. After a test run in CluicheTest, the Tests header rollup updates on return to Boot.

## Open Design Questions

1. **Group order per game.** Showcase-first suits CoW. For CluicheTest, nearly every stage is `test`, so the menu opens on a collapsed group and you need one click every time.
   - Option: a `.diaapp` `BootMenuModule` config `expanded_groups` (default `[showcase, tool, other]`), so CluicheTest can expand Tests.
   - Settle this in the mockup review.
2. **Keyboard navigation over duplicate rows.** Up/down moves by row, so a two-kind stage is visited twice when you step through the list.
   - Alternative: step by stage and skip rows already passed. Decide in the mockup review.

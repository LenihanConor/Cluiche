# Feature Spec: T-2 Validation and Game Link

**Parent:** @docs/specs/applications/cow/systems/t2-worlddataeditor/t2-worlddataeditor.md
**Builds on:** @docs/specs/applications/cow/systems/t2-worlddataeditor/plugin-shell-files-and-schema.md (Feature 1), @docs/specs/applications/cow/systems/t2-worlddataeditor/table-and-form-editors.md (Feature 2), @docs/specs/applications/cow/systems/t2-worlddataeditor/packs-and-bundles.md (Feature 3), @docs/specs/applications/dia/systems/diavalueschema/validate-schema.md
**Mockup:** @docs/specs/applications/cow/systems/t2-worlddataeditor/mockups/world-data-editor.html

**Gameplay Domains:** debug, tools

## Summary

Closes the loop around saving. It delivers:

- the **save flow** that wraps Feature 1's basic save: confirm on errors, write, catalogue, run the validator, offer to reload in the game;
- **`ValidateService`**, which runs the real `dia validate schema --json` as a child process and parses the findings;
- the **validation panel** (live findings plus the last CLI run) with click-to-jump;
- the **game link**: when a running CoW is connected, **Reload in game**, **Open in testbed** and **Validate in game** send `cow.*` commands and show the results.

The editor never decides validity itself beyond Feature 2's live checks: the CLI is the authority (VS-004), and the game is the authority for world guarantees.

## Problem

Saving a file is only half the job. The author needs to know whether the saved result is valid by the same rules the build uses, to see the problems in one place and jump to them, and to see the change in the running game without restarting it. Without this, validity is found at the next `dia pipeline`, and a tuned file is checked by relaunching.

## Goals

1. **One authority.** After a save, the real validator runs and its findings are shown; the editor and the CLI cannot disagree.
2. **One place for problems.** A panel lists live findings and the last validator run, grouped by file, with click-to-jump to the cell or field.
3. **Save with eyes open.** Errors are confirmed before saving; warnings are not blocking.
4. **See it in the game.** With CoW connected, one click reloads the saved file in the running game, loads a bundle, or validates a world.
5. **Offline-first.** Everything except the game link works with no game and no CLI; missing pieces say so and never block editing.

## Non-Goals

- Authoring the validator's rules (Feature 2 of DiaValueSchema) or the world guarantees (S-1).
- Editing cells of a world (Feature 6), scenarios (Feature 5).
- Running or regenerating the simulation.
- Merging files, or reloading assets other than the ones listed.
- Version bumps: T-2 never changes a file's `version` (that belongs to a loader change); only Upgrade changes a pack's `base.version` (Feature 3).

## Architecture

```
C++ (plugin, LiveConnectionPluginBase)
  ValidateService   spawn `dia` CLI on a worker thread, timeout, parse JSON, cache per file
  GameLink          connection state, game identity check, command send/receive via GameConnectionManager
TypeScript
  model/   saveFlow.ts · findings.ts (merge live + CLI) · reloadMap.ts · gameLinkState.ts
  views/   ValidationPanel · SaveConfirmDialog · GameLinkBar · LinkLog
```

### Save flow

The step list Feature 1 defines becomes:

1. **Live checks.** If the document has errors, `SaveConfirmDialog` shows "Save with N errors?" with the first few listed; Save anyway or Cancel. Warnings never prompt.
2. **Serialise** with the stable writer for the kind (no version change).
3. **Write** (`WriteDocument`). A file changed on disk since it was read follows Feature 1's changed-on-disk flow.
4. **Catalogue** upsert if the record is absent.
5. **Validate** the saved file with `ValidateService`, asynchronously; the tab shows "validating…" and the result lands in the panel.
6. **Offer reload** if connected: a toast "Saved. Reload in game?" with **Reload** and **Not now**. A plugin setting **Reload automatically after save** (default off, remembered in editor memory) skips the toast.

**Save all** runs steps 1 to 5 for every dirty document in turn with one confirmation that sums the errors, then at most one reload toast per file kind.

### ValidateService

- **Command:** `dia validate schema <path> --target cow --json`, run from the repository root derived from the loaded `.diagame` path. A **Validate all** button runs it with no path.
- **Process:** a child process on a worker thread; the UI stays responsive. Timeout 30 seconds for one file and 120 seconds for all; a new run for the same file cancels the previous one.
- **Result:** the JSON array of findings from the CLI (`severity`, `rule`, `file`, `key`, `message`, `suggestion?`, plus `path` and `record`, below). An exit code of 2, output that is not JSON, or a timeout becomes a single "validator error" entry showing the first 500 characters of stderr.
- **Missing CLI.** If `dia` cannot be started (not on the path, no Python environment) the panel says "Validation unavailable: `<reason>`" and the save flow skips step 5; live checks still work. The command is a setting (default `dia`).
- **Caching.** The last result per file is kept with the file's modified time; a result for an older version of the file is shown dimmed and marked out of date.
- **`dia validate schema --json` shape (DiaValueSchema Feature 2 amended):** each finding also carries `path`, the location inside the file with record indexes (for example `species[2].growth_rate`), and `record`, the id of the record when the file is a table (or null). The editor uses them for click-to-jump.

### Validation panel

A dock panel with two tabs:
- **Live:** the findings from Feature 2's checks for every open document, grouped by file.
- **Last run:** the validator's findings, grouped by file, with the run time, the command, and whether the result is current.

Both show severity, rule, message and suggestion; filters for severity and text; counts in the tab titles. **Clicking a finding opens the document and selects the cell, row or field** named by `path` (the table scrolls to the row, the form expands the group and focuses the field, the raw view highlights the key). A suggestion button applies the fix as an operation when the finding is in an open, editable document. The panel also lists the **In-game checks** tab results (below).

### Game link

`CoWWorldDataEditorPlugin` derives from `LiveConnectionPluginBase` (Feature 1, corrected), which owns connection state and the connection callbacks. `GameLink` adds:

- **Identity.** The link is enabled only when the connected game reports a `diagame_path` equal to the loaded `cow.diagame`; for another game the bar says "connected to a different game" and every action is disabled.
- **Sending.** Commands go through `GameConnectionManager::SendCommandWithResponse` with a JSON object of named arguments and a 10-second timeout (60 seconds for validate). The result `{ ok, message, ... }` is shown in the **link log** under the bar; a refusal or timeout shows its text and changes nothing in the editor.
- **Game side.** The commands T-2 sends must be registered as **JSON commands** (`RegisterCommandJson`) with `ValueParam` lists, so the debug server returns structured results rather than only "success" or "failed". Console use still works: positional arguments map onto the same list. This is recorded as a requirement on the CoW commands (F-3 amended).

**Reload in game** by document kind:

| Document | Command sent | Notes |
|---|---|---|
| weather pack, ecology pack | `cow.world.pack.load <kind> <id>` | Applies live; the game's status becomes `custom` |
| `weather_parameters.json` | `cow.weather.reload` | Re-reads between steps |
| `ecology_parameters.json`, `species.json`, `biomes.json`, `succession_rules.json` | `cow.ecology.reload_data` | |
| world bundle | `cow.world.bundle.load <id>` | Resets the world; confirm first |
| baked world | `cow.world.terrain.load <id>` | Resets the world; a start state with a different hash is cleared; confirm first |
| ecology start state | `cow.world.startstate.load <id>` | A live edit; refused by the game until it is ready |
| `terrain_rules.json`, world-shape preset, scenario | none | Disabled with the reason ("no reload in S-1; regenerate there", "scenarios load with the testbed") |

**Open in testbed** (bundle composer, Feature 3) sends `cow.world.bundle.load <id>`.

**Validate in game** (baked worlds): after saving a baked world, an **In-game checks** button sends `cow.worldgen.validate <file>`. The reply `{ pass, violations: [ { cell: {q, r}, rule, message } ] }` fills the **In-game checks** tab. On a pass the game marks the world validated (its F-3 hook updates the catalogue record's `validated` flag) and T-2 refreshes the tree when it sees the file change; on a failure the flag stays `false`, and clicking a violation opens the world at that cell in Feature 6's cell editor when it exists. Until the game has validated a saved baked world, the tree and the composer show it as an **unvalidated edit** (Feature 3).

### Game link bar

A bar above the document area shows: a connection pill (`not connected`, `connected to CoW`, `connected to <other game>`), the **Reload automatically after save** switch, and the link log's last line. Actions that need the game are disabled with a tooltip when it is absent.

### Logging and observation

- **Channel:** `[EDITOR] T2`.
- **Logged:** each validator run (file, duration, counts, exit code), a validator error, each command sent (name, arguments) and its outcome, connection and identity changes, auto-reload setting changes.
- **Profiling:** `performance.mark` for the save flow; validator duration in the log.
- **Health:** a failing validator shows "Validation unavailable" and never prevents editing or saving.

## Acceptance Criteria

1. Saving a document with errors asks to confirm and lists the first errors; saving with only warnings does not ask; Cancel writes nothing.
2. A save writes the file with the stable writer, upserts the catalogue record if absent, never changes the file's `version`, and then starts validation without blocking the UI.
3. `ValidateService` runs `dia validate schema <path> --target cow --json` from the repository root on a worker thread and parses the findings; Validate all runs it without a path.
4. A nonzero exit code, non-JSON output or a timeout appears as one validator-error entry with the first 500 characters of stderr; a missing CLI shows "Validation unavailable" and does not stop editing or saving.
5. A new run for the same file cancels the previous run; a result older than the file's modified time is shown dimmed as out of date.
6. The validation panel shows Live and Last run tabs grouped by file, with filters and counts; clicking a finding opens the document and selects the exact cell, row or field from `path` and `record`; a suggestion button applies the fix as one undo step.
7. The `--json` findings carry `path` and `record` (DiaValueSchema Feature 2 amended) and the jump works for table rows, form fields and raw keys.
8. After a successful save with the game connected, the toast offers Reload, and Reload sends the command in the table for that document kind; with Reload automatically after save on, the command is sent without the toast.
9. Reload for a bundle or baked world asks for confirmation first; reload for `terrain_rules.json`, a preset or a scenario is disabled with its reason.
10. The link is enabled only when the connected game reports the loaded `cow.diagame` path; for another game every action is disabled and the bar says so.
11. Commands use `SendCommandWithResponse` with a 10-second timeout (60 seconds for validate); results and refusals are shown in the link log and change nothing in the editor.
12. Validate in game sends `cow.worldgen.validate <file>`, shows the violations in the In-game checks tab, and a passing world becomes validated (the catalogue flag is cleared of the unvalidated state when the file change is seen); a failing world stays an unvalidated edit.
13. Open in testbed sends `cow.world.bundle.load <id>` and is disabled when the link is disabled.
14. Save all asks once, saves each dirty document in turn, validates each, and shows at most one reload toast per kind.
15. Every command T-2 sends is registered on the game side as a JSON command with a `ValueParam` list.
16. With no game connected and no CLI, all editing and saving works and only the link and validation features show their reasons.

## Test Strategy

- **GoogleTests (C++):**
  - `ValidateService` with a fake process runner: success with findings, success with none, nonzero exit, non-JSON, timeout, cancellation by a new run, missing CLI, out-of-date marking.
  - Finding parsing including `path` and `record`.
  - `GameLink` over a fake `GameConnectionManager`: identity gating, each reload mapping, timeouts, refusal text, disconnection with commands pending.
  - Save flow ordering with fakes (confirm, write, catalogue, validate, offer).
- **pytest:** `dia validate schema --json` includes `path` and `record` on the shared conformance fixtures (DiaValueSchema Feature 2).
- **Vitest (model/):** save flow state machine, finding merge and grouping, jump targets from `path`, reload map per kind, link state.
- **React Testing Library:** `ValidationPanel`, `SaveConfirmDialog`, `GameLinkBar`, jump behaviour, disabled reasons.
- **E2E:** (1) open `species.json` from the Asset Catalogue, enter "Forest" for a biome and see the inline error with its suggestion; (2) apply it and save, and the validator reports 0 errors; (3) with a stub game connected, Reload sends `cow.ecology.reload_data`; (4) with a stub game for another `.diagame`, the actions are disabled.
- **Visual gate:** against the mockup's validation panel and link bar (extended with the link bar and In-game checks tab).

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| VS-004 | One validator; the editor never disagrees | The CLI runs after every save; live checks are held to it by Feature 2's fixtures |
| DiaEditor rule | Any plugin that needs a game connection derives from `LiveConnectionPluginBase` | The plugin does (Feature 1, corrected) |
| Editor actions via DiaAPI | Every action is a command | Save, reload, validate and open-in-testbed are registered editor actions and send DiaAPI commands |
| T-2 Resolved Decision 2 | Unvalidated world edits warn; only the default world is blocked | Validate in game clears the unvalidated state; the composer refuses the default (Feature 3) |
| Tool tiers | Editor tier: offline files; the game link only asks the game to reload or check | No simulation control beyond load and validate commands |
| CoW AD-002 | Four-tier placement | Editor code in `Cluiche/CoW/Editor/`; commands stay in their owning CoW modules |
| DiaCLI rule | Never run executables directly; use `dia` | The editor invokes only the `dia` CLI |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1 to 3 | Shell, editors, packs and bundles | Approved |
| `dia validate schema --json` (DiaValueSchema Feature 2) | The validator; amended with `path` and `record` | Approved, not built |
| `LiveConnectionPluginBase`, `GameConnectionManager` | Connection and commands | Done |
| F-3 commands (`pack.load`, `bundle.load`, `terrain.load`, `startstate.load`) | Reload targets; amended: registered as JSON commands | Approved, not built |
| `cow.weather.reload` (A4), `cow.ecology.reload_data` (A1), `cow.worldgen.validate` (S-1) | Reload and validate targets | Specced, not built |
| F-3 `SetWorldValidated` hook | Marks a world validated after a pass | Approved (consumer adoption) |
| Feature 6 | Jump to a violating cell | Draft |

## Resolved Decisions

1. **The CLI runs as a child process** (2026-10-20), on a worker thread with a timeout. It is exactly what a developer runs by hand, isolated from the editor and cancelable; a missing CLI degrades to "validation unavailable".
2. **T-2 never bumps `version`** (2026-10-20). A file's version changes when its loader changes, which is a code decision; only Upgrade changes a pack's base version.
3. **Errors are confirmed, not forbidden** (2026-10-20). A half-finished edit can be saved with its flags, as the system spec says; the validator's result is shown, and the build gate is `dia validate`.
4. **Reload is offered, not forced** (2026-10-20), with an opt-in automatic setting. Worlds and bundles always ask first because loading them resets the running world.
5. **Commands are JSON commands** (2026-10-20), so the editor receives reasons and structured results from the game, not only success or failure.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Needs the CoW commands registered as JSON commands (F-3 amended) and `dia validate schema --json` with `path` and `record`.

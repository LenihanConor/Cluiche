# Feature Spec: T-2 Scenario Editor

**Parent:** @docs/specs/applications/cow/systems/t2-worlddataeditor/t2-worlddataeditor.md
**Builds on:** @docs/specs/applications/cow/systems/t2-worlddataeditor/plugin-shell-files-and-schema.md (Feature 1), @docs/specs/applications/cow/systems/t2-worlddataeditor/table-and-form-editors.md (Feature 2, cell editors and checks), @docs/specs/applications/cow/systems/t2-worlddataeditor/validation-and-game-link.md (Feature 4, save flow)
**Mockup:** @docs/specs/applications/cow/systems/t2-worlddataeditor/mockups/world-data-editor.html (to be extended, see Mockup gate)

**Gameplay Domains:** debug, tools

## Summary

The editor for guided scenarios (`cow.scenario`, `.scenario.json`), the data files that S-2.5 and S-3 use to set up a world, state what to watch and name the checks that decide pass or fail. A scenario is an ordered list of DiaAPI **setup commands**, a **watch** text, a list of **checks**, and (S-2.5) a **walkthrough** of steps that switch overlays on and show text. The editor presents each part as an ordered list with the right picker, completes command names and arguments from the game's own schema export, and rejects the same mistakes the testbed's loader rejects: unknown commands, unknown checks, unknown overlays.

The validation rules are the loader's, shared as one pure C++ validator called over the bridge, not a TypeScript copy.

## Problem

A scenario that names a check that does not exist, or a command with a mistyped argument, fails only when the testbed loads it ("rejected with the file and line"). Authors are often not programmers, and the command and check names live in C++ they do not read. The editor has to show what exists, explain it, and report mistakes while the scenario is being written.

## Goals

1. **Author each part naturally.** Header fields, a setup command list, a checks list, a walkthrough list.
2. **Know what exists.** Command names and arguments from the game's schema export; check names, overlays and modes from the testbed's vocabulary file.
3. **Same verdict as the loader.** The scenario validator the testbed's loader uses is the one the editor calls.
4. **Explain everything.** Commands, arguments, checks and overlays have tooltips from the schema export and the vocabulary.
5. **Safe structure edits.** Reorder, duplicate and delete steps, each as one undo step.
6. **Degrade, don't block.** Without an export or vocabulary the editor still edits, with clear warnings.

## Non-Goals

- Running a scenario in the game (the testbeds do; Feature 4's reload map lists scenarios as having no reload).
- Defining checks or overlays (testbed C++ code does; the vocabulary file mirrors them).
- A scripting language: setup is a flat list of commands.
- Editing worlds (Feature 6) or packs (Feature 3).

## Architecture

```
C++ (CoWEditor, links CoWWorldAssetsCore)
  ScenarioService   validate(scenarioJson, stageFolder) · commandCatalogue(stage) · vocabulary(stage) · newScenario(stage)
TypeScript
  model/   scenarioModel.ts · commandLine.ts (tokenise / render) · scenarioFindings.ts
  views/   ScenarioEditor · SetupList · ChecksList · WalkthroughList · CommandInsertPalette
```

### The file

```json
{ "format": "cow.scenario", "version": 1,
  "id": "s2_breeding_seasons", "mode": "Controlled", "world": "sandbox_small_temperate",
  "setup":  [ "cow.s3.climate.set rain=1.0 year=GOOD", "cow.ecology.isolate_cell cell=412 on" ],
  "watch":  "Step one season at a time. Deer breed in Spring only; elk in Spring and Fall.",
  "checks": [ "births_only_in_breeding_season" ],
  "walkthrough": [ { "overlays": ["wind"], "text": "Find the prevailing wind.", "check": "…" } ] }
```

`mode` is used by S-3 (Controlled or Living), `walkthrough` by S-2.5; both are optional and the editor shows a section only when the stage's vocabulary or the file uses it.

### Vocabulary

The checks and overlays a testbed registers are C++ and cannot be discovered offline, so each testbed keeps a **vocabulary file** in its scenarios folder, found by looking in the scenario's own folder:

`Stages/<Stage>/scenarios/vocabulary.json`

```json
{ "format": "cow.scenario_vocabulary", "version": 1,
  "modes":    [ { "id": "Controlled", "label": "Controlled", "description": "Pinned climate, no weather or succession." } ],
  "checks":   [ { "id": "births_only_in_breeding_season", "label": "Births only in breeding season", "description": "…" } ],
  "overlays": [ { "id": "wind", "label": "Wind arrows", "description": "…" } ] }
```

- Hand-authored by the testbed's owner. A **GoogleTest in each testbed** fails if a registered check or overlay is missing from its vocabulary file or the file names one that is not registered, so the file cannot drift from the code. (S-2.5 and S-3 are amended to add that test and the file.)
- `ScenarioService.vocabulary(stage)` loads it; with no file the editor shows free text for checks, overlays and mode with a warning ("no vocabulary for this stage; names cannot be checked").

### Command catalogue

The game's code-side schema export lists every registered DiaAPI command (one `command` entry each, with its owner, description and example) and its arguments (`arg` entries with type, unit, range, enum values and `required`, in declaration order). Stage-specific commands exist only while their stage runs, so the export is made per stage: `dia schema export --target cow --stage <Stage>` writes `Cluiche/out/CoW/valueschema.<Stage>.json` (DiaValueSchema Feature 6 and 4 amended with this feature). `ScenarioService.commandCatalogue(stage)` reads that file for the scenario's stage folder, or the base export when there is no stage file.

- The editor shows the export's commit (header `git_commit`) and warns when it differs from HEAD, as the validator does.
- With no export the setup list is free text, and commands are not checked ("command list unavailable: run `dia schema export`").

### Command lines

A setup entry is a string: a command name, then arguments as `key=value` or positional values (`cow.s3.climate.set rain=1.0 year=GOOD`, `cow.weather.load_preset rain_shadow`). Values with spaces are in double quotes. `commandLine.ts` tokenises a string into `{ command, args: [{name?, value}] }` and renders it back, so the editor can show a structured row and a raw text row and switch between them without changing the text.

### The shared validator

`ScenarioValidator` is a pure class in `CoW/Shared/WorldAssets/Core/` (the `CoWWorldAssetsCore` library, alongside the F-3 code):

```cpp
Findings Validate(const Json::Value& scenario, const ICommandCatalogue&, const IScenarioVocabulary&, const IWorldIds&);
```

The testbed loader (S-2.5 Feature 4) calls it with the live command registry and the registered checks; the editor calls it, over the bridge, with the export and the vocabulary file. Its rules, each with the JSON path and, for text, the line:

| Rule | Severity |
|---|---|
| Envelope wrong, or `id` missing | Error |
| `id` differs from the file name (without `.scenario.json`), or duplicates another scenario in the folder | Error |
| `mode` not in the vocabulary's modes (when a mode list exists) | Error |
| `world` not found among bundles, baked worlds and the stage's named worlds | Warning |
| Setup command unknown (catalogue available) | Error |
| Setup argument: unknown name, missing required, wrong type, outside the range, not an enum value, too many positional values | Error |
| Setup command not available in this stage's export but present in the base export | Warning |
| Check not in the vocabulary (when it exists) | Error |
| Duplicate check | Warning |
| No checks at all | Warning |
| Walkthrough overlay not in the vocabulary (when it exists) | Error |
| Walkthrough `check` not in the scenario's own `checks` | Error |
| Empty `watch` | Warning |

When the catalogue or vocabulary is missing, the rules that need it are skipped and one Warning says which.

### Editor views

- **Header:** id (the file name; renaming renames the file and the catalogue record), mode (when the vocabulary has modes), world (a picker over bundles, baked worlds and the stage's named worlds, free text allowed with a warning), watch (multi-line text).
- **Setup list:** ordered rows. Each row is a structured editor (a command field with completion and one input per argument, typed from the catalogue: number with unit and range, enum dropdown, bool, text) with a toggle to raw text. Rows can be reordered, duplicated and deleted. **Insert command…** opens `CommandInsertPalette`: a searchable list of commands grouped by owner, showing the description, the argument signature and the example; choosing one inserts a row with required arguments empty and flagged.
- **Checks list:** ordered rows with a picker over the vocabulary's checks (id, label, description); duplicates are flagged.
- **Walkthrough list** (when present or when the stage has overlays): steps with overlay chips (picker over the vocabulary), text, and an optional check chosen from the scenario's own checks; a read-only **preview** shows the steps as the panel would read them.
- **Findings** appear inline on the row with the message, and in the validation panel (Feature 4) with click-to-jump.
- **New scenario** asks for the stage and an id and creates a valid file from a template (envelope, id, empty lists, a mode when the stage uses modes). **Duplicate** copies a scenario with a new id.

Every change is an operation (`set`, `insert`, `remove`, `move`) from Feature 1; a text field commits one operation when it loses focus or on Enter.

### Logging and observation

- **Channel:** `[EDITOR] T2`.
- **Logged:** which export and vocabulary were used (path, commit), validation summary per scenario, new and duplicated scenarios, a missing export or vocabulary (once per stage).
- **Profiling:** `performance.mark` around validate calls; a call over 50 ms is logged.

## Acceptance Criteria

1. A `cow.scenario` file opens in the scenario editor, which shows the header, setup list, checks list and, when applicable, the walkthrough list; a stage whose vocabulary has no modes hides the mode field.
2. Setup rows tokenise into a command and arguments (named, positional, quoted) and render back to the same text; the structured and raw views stay in step.
3. With a stage export present, command names complete from the catalogue and each argument input is typed and range-checked from the catalogue; the insert palette lists commands by owner with description, signature and example.
4. With the vocabulary present, checks, overlays and modes are pick lists with descriptions; unknown names are errors; a missing vocabulary degrades to free text with a warning.
5. `ScenarioValidator` produces every finding in the rule table with the JSON path and, for text, the line, and skips rules whose input is missing with one warning.
6. The editor and the testbed loader return the same findings for the same scenario, catalogue and vocabulary (shared core test with fixtures).
7. Reordering, duplicating and deleting steps each commit one undo step; a text field commits one operation on blur or Enter.
8. Renaming the id renames the file and updates the catalogue record; an id that differs from the file name or duplicates another is an error.
9. New scenario creates a valid file from the template for the chosen stage; Duplicate copies with a new id.
10. The walkthrough preview shows the steps in order with their overlays, text and pinned check.
11. The editor shows the commit of the export it used and warns when it differs from HEAD; a missing export shows "command list unavailable" and does not stop editing.
12. Each testbed's vocabulary test fails when a registered check or overlay is absent from its vocabulary file, or when the file names one that is not registered.
13. The visual result matches the scenario editor in the extended mockup.

## Mockup gate

The mockup has no scenario editor. Before this feature is built it is extended and approved with: the header form, the setup list with a structured row and a raw row, the argument inputs with a range error, the insert-command palette, the checks list with the vocabulary picker, the walkthrough list and preview, and the inline findings.

## Test Strategy

- **GoogleTests (C++):**
  - `ScenarioValidator` over fixture scenarios, catalogues and vocabularies: one case per rule, missing-input degradation, line numbers.
  - The same fixtures run through the testbed loader's call (S-2.5 and S-3 fakes) to prove both give one result.
  - `ScenarioService`: catalogue loading per stage with fallback, vocabulary lookup in the scenario's folder, new-scenario template.
  - Each testbed's vocabulary-sync test (in the testbed, once it exists).
- **Vitest (model/):** command-line tokenise and render (named, positional, quotes, round trip), scenario operations with inverses, findings placement on rows, new and duplicate.
- **React Testing Library:** `ScenarioEditor` and its lists from fixtures: structured and raw rows, completion, range errors, palette, pickers, walkthrough preview, tooltips.
- **pytest:** `dia schema export --stage` and the `command` entries (DiaValueSchema Features 4 and 6) using a stub launcher.
- **E2E:** open a scenario, mistype a check name and see the error, pick the right one, add a setup command from the palette with a bad argument and fix it, save; the validator reports no errors.
- **Visual gate:** against the extended mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| S-2.5 / S-3 scenario format | One CoW-Shared format; setup is DiaAPI commands; checks are testbed names; the loader rejects unknown commands, checks and overlays | The editor edits that format and calls the loader's own validator |
| VS-004 | One rule set | The shared `ScenarioValidator`; no TypeScript copy |
| Editor actions via DiaAPI | Setup commands are DiaAPI commands | The catalogue is the DiaAPI command list from the export |
| UI/business separation | A typed model first | `model/` and the C++ service |
| CoW AD-002 | Four-tier placement | Editor code in `Cluiche/CoW/Editor/`; the validator in CoW-Shared `Core/` |
| AD-001 | Ask before Dia edits | The DiaValueSchema `command` entries and `--stage` option are Dia edits needing sign-off at build time |
| Mockup-first rule | A mockup before any editor feature is built | Mockup gate |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1, 2 and 4 | Shell, cell editors and checks, save flow and validation panel | Approved |
| DiaValueSchema Features 4 and 6 | `command` entries and the `--stage` export; amended with this feature | Approved; amended |
| S-2.5 Feature 4 (scenario format and loader) | The loader calls `ScenarioValidator`; adds a vocabulary file and sync test | Approved; amended |
| S-3 Feature 3 (guided scenarios) | The same | Approved; amended |
| F-3 | Scenario files and the catalogue type `cow.scenario` | Approved |

## Resolved Decisions

1. **A vocabulary file per testbed** (2026-10-20), kept in step with the code by a testbed test. It works offline, needs no new mechanism in Dia, and makes check and overlay names pick lists.
2. **The scenario validator is shared C++** (2026-10-20), called by the loader and by the editor, so the editor cannot accept a scenario the loader rejects.
3. **The command list comes from a per-stage schema export** (2026-10-20), with a `command` entry for every command, because stage commands exist only while their stage runs and commands without arguments would otherwise be invisible.
4. **Unknown worlds are a warning** (2026-10-20): a world can be a stage-named world defined in the `.diastage` file, which the editor cannot always see.
5. **No "run in game"** (2026-10-20): the testbeds own scenario running; the editor stops at a valid file.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. A scenario-editor mockup must be approved before it is built (Mockup gate); the DiaValueSchema `command` entries and `--stage` export need AD-001 sign-off.

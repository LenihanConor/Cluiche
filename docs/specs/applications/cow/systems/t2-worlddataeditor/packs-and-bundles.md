# Feature Spec: T-2 Packs and Bundles

**Parent:** @docs/specs/applications/cow/systems/t2-worlddataeditor/t2-worlddataeditor.md
**Builds on:** @docs/specs/applications/cow/systems/t2-worlddataeditor/plugin-shell-files-and-schema.md (Feature 1), @docs/specs/applications/cow/systems/t2-worlddataeditor/table-and-form-editors.md (Feature 2), @docs/specs/applications/cow/systems/f3-worldassets/envelope-and-override-merge.md (F-3 Feature 1), @docs/specs/applications/cow/systems/f3-worldassets/save-as.md (F-3 Feature 3)
**Mockup:** @docs/specs/applications/cow/systems/t2-worlddataeditor/mockups/world-data-editor.html

**Gameplay Domains:** debug, tools

## Summary

The editors for the files that sit on top of the defaults: **packs** (overrides against a defaults file) and **world bundles** (a terrain plus optional weather pack, ecology pack and start state).

- **Pack diff** shows a pack as overrides against its defaults, lets you edit, add, reset and remove overrides, and upgrades a stale pack.
- **Pack compare** puts two packs (or a pack and the defaults) side by side with copy-across.
- **Bundle composer** builds a bundle from pickers that show each part's state, lists "used by" in both directions, and sets the default world.
- **`PackOverrideIndex`** answers "which packs override this key?" for Feature 2's form editor.

All merge, upgrade and bundle-validity rules come from the same C++ code the game uses (F-3's `CoWWorldAssetsCore`), called over the bridge; the TypeScript only presents the results.

## Problem

Tuning is saved as packs, and worlds as bundles, but reading a pack as a raw list of keys says nothing about what it changes, whether it is still compatible with the defaults, or who uses it. Composing a bundle by hand means writing paths that must exist, agree on a terrain hash and be valid. A designer needs to see the overrides as differences, move tuning from one pack to another, and build a bundle that is known to load.

## Goals

1. **Packs as differences.** Key, default, pack value and change, overrides first, all keys on request.
2. **Edit safely.** Add an override by picking a key, edit a value with the right editor and live checks, reset it to the default, or remove it.
3. **Upgrade stale packs** in one reviewed step.
4. **Compare and copy across** between two packs or a pack and the defaults.
5. **Compose bundles that load.** Each part shows whether it exists, is stale, is valid or is an unvalidated edit, and a start state shows whether it matches the terrain.
6. **Know the impact.** "Used by" in both directions, and a warning on a defaults key that packs override.
7. **No second rule set.** Merge, upgrade and bundle checks are F-3's code, not a TypeScript copy.

## Non-Goals

- Saving, reloading in the game and validation runs (Feature 4 wraps the save flow).
- Editing species or biome overrides (packs cover the parameter files only).
- Cell-level editing of worlds and start states (Feature 6); the composer only picks them.
- Running the simulation or loading the bundle (Feature 4's "Open in testbed").
- Merging concurrent edits.

## Architecture

Views in `Cluiche/CoW/Editor/WorldDataEditor/UI/views/`, a C++ `PackService` and `BundleService` in the plugin's FileService, logic in `model/`.

```
C++ (CoWEditor, links CoWWorldAssetsCore)
  PackService     analyzePack(pack, defaults) · upgradePack(pack, defaults) · buildPackText(...) · newPack(kind)
  BundleService   checkBundle(bundle) · setDefault(id) · bundleText(...)
TypeScript
  model/   packRows.ts · packCompare.ts · packOverrideIndex.ts · bundleModel.ts · usedBy.ts
  views/   PackDiff · PackCompare · BundleComposer · NewPackDialog · UpgradeDialog · UsedByList
```

### Shared core additions (F-3 `Core/`, amended with this spec)

So the editor and the game agree on every rule, three pieces of F-3 live in `CoWWorldAssetsCore`:
- **`CheckBundle(bundle, IAssetSource, IDefaultsProvider) → BundleCheckResult`**: the Prepare step of F-3 Feature 2: envelope checks, referenced files exist, packs resolved and classified against their defaults (stale, unknown keys, type mismatch, clamped), the start state's terrain hash compared with the baked terrain, a start state on non-baked terrain rejected. The game's loader calls it with its participants' defaults; T-2 calls it with defaults read from disk.
- **`DiagameEditor`** and **`CanSetDefault(bundle)`**: F-3 Feature 3's minimal `cow.diagame` text edit and its refusal rules (bundle exists, baked terrain not an unvalidated edit).
- **`WorldContentHash(Json)`**: the FNV-1a 64 hash of the canonical JSON text of a `.wld.json` document (sorted keys, no whitespace), so S-1, F-3 and T-2 compute one value. (F-3 Feature 6 is amended to this wording.)

### Pack diff

`analyzePack(pack, defaults)` returns, per override: path, the default value, the pack value, status (`ok`, `unknownKey`, `typeMismatch`, `outOfRange`) and the F-3 `MergeReport` summary (stale, `baseVersionDelta`, `newerBase`). `packRows.ts` lays out rows:

| Column | Content |
|---|---|
| Key | dotted path with the schema label and tooltip |
| Default | the defaults file's value |
| Pack | the override, or empty for a default |
| Δ | the numeric difference (pack minus default) and the percentage, or `—` |
| Status | stale, unknown, mistyped, out of range |

- **Overrides only** by default; **Show all keys** adds every defaults key as a row with an empty Pack cell.
- **Edit:** the Pack cell uses the same cell editors and live checks as Feature 2. Editing an empty cell adds an override; **Reset** removes the override (the value returns to the default); each is a `set` or `remove` operation on `overrides`.
- **Add override:** a searchable list of the defaults keys (label, unit, default) that adds the chosen key with its default as the starting value.
- **Stale banner:** "Pack based on `v3`, defaults are `v4`" with **Upgrade**. `upgradePack` returns the plan (keys it will drop, mistyped values it will drop, the new base version); `UpgradeDialog` shows the plan, and confirming applies one `batch` (remove the listed keys, set `base.version`), so one undo reverts it. A `newerBase` pack shows a different banner and cannot be "upgraded".
- Numbers are shown as stored; the file writer (below) keeps 9 significant digits and sorted keys.

### New pack and duplicate

**New pack** asks for the kind (weather or ecology), an id (`a-z 0-9 _`, unique among that kind's packs) and a display name, and creates a document with no overrides and `base` set to the defaults' current format and version. **Duplicate** copies a pack with a new id. Both create the file and its catalogue record without `author` (hand-authored), like all T-2 records.

### Pack compare

Two packs of the same kind, or a pack and **Defaults**, in two pickers. Rows are the union of their keys: key, default, A, B, B minus A, with differences highlighted and a filter for "differs only". **Copy across** (per row, or for all differing rows) sets the value, or removes the override when the source has none, in the target pack as an operation (the target becomes dirty). Packs with different `base` versions show a banner naming the stale one. Packs of different kinds cannot be compared.

### `PackOverrideIndex`

Reads every pack of a kind (through `FileService`, cached and refreshed on `Poll`) and records which keys each overrides. `overridesOf(defaultsFormat, key)` returns the pack ids. It is the provider Feature 2's form editor asks for its warning icon: "Overridden by packs: lush, arid". It reads in-memory values for open documents.

### Bundle composer

A form over the bundle document:

| Part | Picker | Shows |
|---|---|---|
| Terrain | a baked world (list of `Global/WorldGen/Baked/*.wld.json`), or a preset (list of `Presets/*`) with seed and size | exists, `unvalidated edit` warning, hash prefix |
| Weather pack | packs of kind weather, or none | stale / unknown-key counts |
| Ecology pack | packs of kind ecology, or none | stale / unknown-key counts |
| Start state | start states whose world matches the chosen baked terrain, or none | cell count; **broken** when the hash differs from the terrain |

- Each picker entry carries its state from `checkBundle`, run after every change. A start state is offered only for baked terrain; choosing preset terrain clears it with a note.
- **Used by (both directions).** For a bundle: the files it references, each with a link. For any pack, baked world or start state: the bundles that reference it and, for a baked world, the start states keyed to it (`UsedByList`, from `usedBy.ts`, built on Feature 2's refs index and a bundle index).
- **Display name** and **id** edit in place; the id is the file name, so renaming a bundle renames the file, updates the catalogue record and shows the references (the default world, if it names this bundle).
- **Set as default world:** `CanSetDefault` decides; it is refused for a bundle that does not exist or whose baked terrain is an unvalidated edit (the message names the world and says to run **Validate in game**, Feature 4). On success `setDefault` edits `config.default_world` in `cow.diagame` with `DiagameEditor`.
- **Open in testbed** is shown disabled with "connect to the game" until Feature 4 wires it.

### Writing

Packs and bundles are serialised by the C++ writers (`buildPackText`, `bundleText`): fixed key order, sorted overrides, 9-significant-digit numbers, trailing newline, so equal content gives equal bytes. Save is Feature 1's flow (Feature 4 adds validation and reload).

### Logging and observation

- **Channel:** `[EDITOR] T2`.
- **Logged:** pack upgrade applied (keys dropped), copy-across, bundle check summary, `default_world` change (old and new), new pack created.
- **Profiling:** `performance.mark` around pack row layout and `checkBundle` calls; bridge call time at Debug.

## Acceptance Criteria

1. A pack opens in the diff view with Key, Default, Pack, Δ and Status columns; overrides-only is the default and Show all keys adds the rest; labels and tooltips come from the schema.
2. Editing a Pack cell adds or changes an override with the right cell editor and live checks; Reset removes it; Add override lists the defaults keys with label, unit and default; each is one undo step.
3. `analyzePack` classifies each override as ok, unknown key, type mismatch or out of range, and reports stale, version delta and newer base, exactly as F-3's merge does (same code).
4. A stale pack shows the banner; Upgrade shows the plan (dropped keys, dropped mistyped values, new base version) and applies it as one undo step; a newer-base pack cannot be upgraded.
5. New pack creates a valid pack with the defaults' current base and no overrides; Duplicate copies with a new id; both add a catalogue record without `author`.
6. Pack compare shows the union of keys with differences highlighted and a differs-only filter; copy across edits the target pack as an operation; packs of different kinds are refused and a stale one is flagged.
7. Compare accepts Defaults as either side.
8. `PackOverrideIndex.overridesOf` returns the packs that override a key, uses open documents' in-memory values, and drives the warning icon in the form editor.
9. The composer's pickers list the right candidates (baked worlds, presets with seed and size, packs by kind, start states matching the terrain) and show each part's state from `checkBundle`.
10. A start state whose terrain hash differs from the chosen baked world is marked broken; choosing preset terrain clears the start state; the hash is F-3's `WorldContentHash`.
11. "Used by" lists a bundle's referenced files and, for a pack, baked world or start state, the bundles (and for a world, the start states) that reference it, each as a link that opens the file.
12. Renaming a bundle renames the file, updates the catalogue record and reports a `default_world` that named it.
13. Set as default world uses `CanSetDefault` and `DiagameEditor`, refuses an unvalidated-edit terrain with a message naming the world, and changes no byte of `cow.diagame` other than `config.default_world`.
14. `checkBundle` called by the editor and by the game's loader return the same result for the same files (shared core test).
15. Packs and bundles are written with sorted overrides, fixed key order and 9-significant-digit numbers; equal content gives byte-identical files.
16. The visual result matches the mockup's pack diff, compare and used-by regions, extended for the bundle composer (see Mockup gate).

## Mockup gate

The mockup has the pack diff, the stale banner, a compare stub and a used-by list; it has no bundle composer. Before the composer is built the mockup is extended and re-approved with: the four part pickers and their state badges, the broken-start-state marker, the unvalidated-edit warning, the used-by lists, and the Set as default world action with its refusal message. The pack compare stub is completed in the same pass.

## Test Strategy

- **GoogleTests (C++):**
  - `PackService` over fixture pack and defaults files for each status, stale, newer base, upgrade plans.
  - `CheckBundle` with fake sources: every Prepare failure and warning from F-3 Feature 2; the same fixtures run through the game loader's Prepare to prove both give one result.
  - `WorldContentHash` stability and sensitivity; `DiagameEditor` and `CanSetDefault` over copies of the real `cow.diagame` (key present, absent, CRLF preserved, unvalidated refusal).
  - Writers: byte-identical output, key order, precision.
- **Vitest (model/):** pack row layout (overrides-only, all keys, Δ and percentage), edit and reset ops, upgrade batch with inverse, compare union and copy-across ops, `PackOverrideIndex`, bundle model state, `usedBy` in both directions, rename plan for a bundle.
- **React Testing Library:** `PackDiff`, `PackCompare`, `BundleComposer`, `UpgradeDialog`, `UsedByList` from fixtures; tooltips present.
- **E2E:** open a stale pack, upgrade, save; compose a bundle from a baked world and a start state, change the world and see the broken marker, fix, set as default; with a stub game, nothing else is sent (Feature 4).
- **Visual gate:** against the extended mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| F-3 | Envelope, packs as overrides only, stale rules, start states only on baked terrain, hash mismatch refused | All through F-3's own code |
| VS-004 | One rule set | No second merge or bundle implementation; live value checks reuse Feature 2's |
| T-2 Resolved Decision 2 | Unvalidated world edits warn; only the default world is blocked | The composer warns, and `CanSetDefault` refuses |
| UI/business separation | A typed model first; views depend only on it | `model/` and the C++ services |
| CoW AD-002 | Four-tier placement | Editor code in `Cluiche/CoW/Editor/`; shared rules in `CoW/Shared/WorldAssets/Core/` |
| Editor actions via DiaAPI | Every action is a command | New pack, upgrade, copy across and set default are registered editor actions |
| AD-001 | Ask before Dia / shared build edits | No Dia edits here; the `CoWWorldAssetsCore` library change is CoW-side |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1 and 2 | Shell, ops, schema, cell editors, refs index, live checks | Approved |
| F-3 Features 1 to 3 | Envelope, merge, pack file, catalogue writer, `set_default` rules; amended to put `CheckBundle`, `DiagameEditor`, `WorldContentHash` in `Core/` | Approved; amended |
| F-3 Feature 6 / S-1 | Content hash definition and baked worlds | Approved; amended |
| Feature 4 | Save flow wrapper, validate in game, open in testbed | Draft |
| `@dia/editor-ui` `ValueLabel`, `ValueTooltip` | Labels | Approved, not built |

## Resolved Decisions

1. **Merge, upgrade and bundle checks are called over the bridge, not re-implemented** (2026-10-20). These rules (tolerance, type kinds, hash comparison, refusal rules) are subtle and F-3 already owns them; packs and bundles are small, so a bridge call per change is cheap. (Feature 2's live checks are the exception: they run per keystroke and are held to the CLI by shared fixtures.)
2. **`CheckBundle` is shared by the game and the editor** (2026-10-20), so a bundle the composer calls valid is a bundle the loader accepts.
3. **"Set as default world" is in the composer** (2026-10-20), as the system's Resolved Decision 2 expects, using F-3's `set_default` rules and text edit.
4. **Compare accepts Defaults as a side** (2026-10-20), so "what does this pack change?" and "how do these two differ?" are one view.
5. **Pack and bundle creation adds records without `author`** (2026-10-20), consistent with Feature 1: they are hand-authored, so a testbed will not overwrite them.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. The bundle composer needs the extended mockup approved first (Mockup gate).

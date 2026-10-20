# Feature Spec: F-3 Save As

**Parent:** @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md
**Builds on:** @docs/specs/applications/cow/systems/f3-worldassets/envelope-and-override-merge.md (Feature 1), @docs/specs/applications/cow/systems/f3-worldassets/world-assets-module-and-bundle-load.md (Feature 2), @docs/specs/applications/dia/systems/diacli/dev-source-alias.md (dia-needs #16)

**Gameplay Domains:** debug

## Summary

Lets a testbed author content and keep it. Five DiaAPI commands write the current state of the world as source assets, add them to the asset catalogue, and (for one command) point the game at a default world:

| Command | Writes |
|---|---|
| `cow.world.pack.save_as <weather\|ecology> <id> [displayName] [overwrite]` | An overrides-only pack of the current tuning |
| `cow.world.terrain.bake <id> [overwrite]` | The current S-1 world as a baked `.wld.json` |
| `cow.world.startstate.save_as <id> [overwrite]` | The edited ecology cells as a sparse start state, keyed to the baked terrain |
| `cow.world.bundle.save_as <id> [displayName] [overwrite]` | A bundle naming the current terrain, packs and start state |
| `cow.world.bundle.set_default <id>` | `default_world` in `cow.diagame` |

Writes go to the `source` alias (dev launches only, dia-needs #16) and the catalogue records carry `"author": "game"` so the asset pipeline never removes them. The panel card (Feature 5) and T-2 call the same commands.

## Problem

Tuning, painting and baking in a testbed is lost when it closes unless it becomes files the game loads. Those files must be small and reviewable in git (only overrides, only edited cells), they must reference each other correctly, they must not overwrite hand-authored content, and the catalogue the build reads must stay valid and unchanged in everything the game did not touch.

## Goals

1. **Save what you see.** Each command writes the current state as the right asset type and path, using Feature 1's envelope and pack builder.
2. **Reviewable diffs.** Files and the catalogue are written in exactly the formatting the repository already uses, so a save changes only the lines it should.
3. **Never lose hand-authored work.** The game creates or updates only records it owns; an existing id or file is refused unless `overwrite`, and a hand-authored record is never overwritten even with it.
4. **No half-saved state.** An asset file and its catalogue record are both written or neither is.
5. **Reproducible bundles.** A bundle cannot be saved while any part has unsaved changes, so loading it later gives the world that was saved.
6. **Clear failure.** Without the `source` alias, every command says saving is unavailable and why.

## Non-Goals

- Loading, applying and listing assets (Feature 2).
- `cow.ecology.tuning.save` and other writes to defaults files (A1's own command; it reuses the file-writing primitive only).
- The `source` alias itself, the `world` alias and deploy (Feature 4, dia-needs #16 and #17).
- The panel card (Feature 5) and consumer participants (Feature 6).
- Deleting or renaming assets, and catalogue pruning.
- Shipping-build authoring.

## Architecture

`Cluiche/CoW/Shared/WorldAssets/` (CoW-Shared), additions to the Feature 2 module.

```
SourceWriter       resolve the target path under `source`, write a file atomically, refuse overwrite
CatalogueWriter    read-modify-write of assets.catalogue.json in the repository's exact format
DiagameEditor      minimal text edit of cow.diagame for default_world
SaveCommands       the five commands, built on the above and the participants
```

### Preview (added 2026-10-20, Feature 5)

`cow.world.save.preview <pack weather|pack ecology|bake|startstate|bundle> <id>` is a read-only command that reports what the matching save would do without writing: `{ path, exists, ownedByGame, blockedReason?, overrides?, cellCount?, parts?, unsaved? }`. It runs the same checks as the save command and returns the same refusal text, so the panel card and the console show one reason. It writes nothing and never touches the catalogue.

### Asset kinds

One table in the module, matching the parent Asset Table and shared with Feature 2's listing:

| Kind | Folder under `Global/` | Extension | Format id |
|---|---|---|---|
| weather pack | `Weather/Packs/` | `.weather.json` | `cow.weather_pack` |
| ecology pack | `Ecology/Packs/` | `.ecology.json` | `cow.ecology_pack` |
| baked world | `WorldGen/Baked/` | `.wld.json` | `cow.world` |
| start state | `Ecology/StartStates/` | `.ecostate.json` | `cow.ecology_start` |
| bundle | `Worlds/` | `.cowworld` | `cow.world_bundle` |

An **id** is 1 to 48 characters of `a-z 0-9 _`. It becomes the file name and part of the catalogue id, so nothing else is accepted.

### Writing a file

`SourceWriter::Write(kind, id, text, overwrite)`:
1. Requires the `source` alias; otherwise it fails with "saving needs a dev launch (`dia launch`): the `source` alias is missing" (Error, nothing written).
2. Resolves `<source>/Global/<folder>/<id><ext>`, creating folders as needed. The path never leaves `<source>/Global/`.
3. Refuses when the file exists unless `overwrite`, and refuses even with `overwrite` when the catalogue record for that id exists and is not game-owned (see Catalogue).
4. Writes to a temporary file in the same folder, then renames it into place, so a failed write leaves the old file or nothing.

Asset text comes from Feature 1's `BuildPack` for packs; baked worlds come from S-1's `ExportWorld`; start states and bundles are built with the same stable key order and indentation (two spaces, trailing newline).

### The catalogue

`assets.catalogue.json` (`Cluiche/Assets/CoW/`) is `{ "assets": [ … ] }`. It is written by Python with `indent=4`, sorted keys, ASCII escapes and LF line endings plus a final newline; re-serialising the current file with those options reproduces it byte for byte. `CatalogueWriter` produces exactly that format, so rewriting an unchanged catalogue changes nothing in git. The reader, the writer and the upsert rules are a pure class, `CatalogueFile`, in `WorldAssets/Core/` (the `CoWWorldAssetsCore` library), which the module calls and which T-2's `FileService` links too (amended 2026-10-20, T-2 Feature 1). The kind table (`WorldAssetKinds`, Feature 4) lives in `Core/` for the same reason.

**Record written by the game:**

```json
{ "author": "game", "content_hash": 0, "id": "cow.ecology_pack.lush",
  "references": [], "scope": "global", "source_path": "Global/Ecology/Packs/lush.ecology.json",
  "stage_name": "", "status": "active", "tags": [ "world" ], "type": "cow.ecology_pack" }
```

- `id` is `<format id>.<asset id>`; `type` is the format id (parent rule); `tags` is `["world"]`, which deploys it under `global/world/…` (Feature 4); `content_hash` is 0 like the existing records.
- A **bundle** record lists a `contains` reference to each part's record id; a **baked world** record also carries `"validated": true` (see Validated flag).
- **Upsert:** an absent id is added; an existing record with `"author": "game"` is updated in place; an existing record without `author` is hand-authored and is **never** changed, even with `overwrite` (the command fails and names the record).
- **Safe read-modify-write:** the file is read immediately before writing (never cached), the change applied, written to a temporary file and renamed. If the file's size and modified time changed between the read and the rename, the read-modify-write is repeated once; a second conflict fails with "the catalogue changed while saving" and nothing is replaced.
- **Order of operations:** asset file first, then catalogue. If the catalogue step fails, a newly created asset file is removed (an overwritten one is left, with the Error stating that the catalogue record was not updated).
- The pipeline never prunes records (`author: game` is protected by design, parent Resolved Decision 3); an integration test proves a `dia pipeline` run between two saves loses nothing.

### `pack.save_as`

1. The participant for `<kind>` supplies `Capture()` (current values) and `Defaults()` and `DefaultsSpec()`.
2. `Diff(defaults, current)` (Feature 1) gives the overrides; `BuildPack` writes them with `base` set to the defaults' **current** format version, which clears the stale flag of a re-saved stale pack.
3. A pack with no overrides is saved with an Info message ("pack equals the defaults"); it is a valid named empty pack.
4. After a successful save the loaded pack for that kind becomes the saved id, and the part is no longer unsaved.

### `terrain.bake`

1. Requires a world accepted by S-1 (`GetCurrentWorld()` non-null); otherwise Error.
2. Calls S-1's `ExportWorld` with the resolved target path. The file is the S-1 `.wld.json` (`cow.world`).
3. The catalogue record carries `"validated": true`, because only worlds that passed S-1's guarantees are accepted.
4. The loaded terrain becomes `baked:<id>` with that world's content hash, so a start state can now be painted and saved against it.

### `startstate.save_as`

1. Requires the loaded terrain to be baked; otherwise Error "bake the terrain first" (parent version table).
2. `ecology_start.Capture()` returns the cells edited since the world was initialised or the last start state was loaded (A1's edit set). An empty set is an Error ("no edited cells").
3. The file records `world: { baked: <id>, hash: <content hash> }` and only the edited cells, with A1's `set_field` field names.

### `bundle.save_as`

Writes a bundle naming the **loaded parts**:
- **terrain:** `{ "baked": "WorldGen/Baked/<id>.wld.json" }` for baked terrain; `{ "preset", "seed", "size" }` for unedited preset-plus-seed terrain; an edited, un-baked world is an Error ("bake first").
- **weather**, **ecology:** the loaded pack's path, or omitted when the defaults are in use.
- **startState:** the loaded start state's path, or omitted.

**Unsaved changes block the save.** For each part, the current `Capture()` is compared (Feature 1 `Diff`) with what the loaded pack, or the defaults, would resolve to. If they differ, or start-state cells were edited since load, the command fails and names the part ("weather has unsaved changes: `pack.save_as weather <id>` first, or `pack.clear weather`"). This is what makes a bundle reproduce the world that was saved.

### `bundle.set_default`

1. The bundle must exist and its terrain must not be an unvalidated edit: if the terrain is baked and its catalogue record has `"validated": false`, the command is refused with the world's name and "run `cow.worldgen.validate`".
2. `DiagameEditor` sets `config.default_world` in `cow.diagame` with a **minimal text edit** that keeps every other byte: the file uses CRLF line endings and its `config` keys are not sorted, so re-serialising it would reorder lines. If `"default_world"` exists its string value is replaced; otherwise a new member is inserted after the last member of `config`, with the same indentation and line ending, adding the comma the previous member needs.
3. The result is parsed and checked: only `config.default_world` may differ from the original; otherwise the edit is discarded and an Error is reported.
4. It does not load the bundle: the new default applies at the next New Game (Feature 2's initial bundle).

### Validated flag

A baked world's catalogue record may carry `"validated": true | false`; absent means validated. `terrain.bake` writes `true`. T-2 writes `false` when it saves a hand-edited baked world; `cow.worldgen.validate <file>` sets it back to `true` when it passes. The flag lives on the record, not in the `.wld.json`, so it never changes the world's content hash or invalidates start states.

### Events and logging

- **Event:** `WorldAssetSaved{ kind, id }` (small POD) after both file and catalogue are written.
- **Channel `WAS`.** Info: each save with its path, a no-override pack. Warning: an overwrite, a stale pack re-saved. Error: missing `source` alias, refused overwrite, refused hand-authored record, catalogue conflict, unvalidated default, write failure.
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on file write and on the catalogue read-modify-write.
- **Metrics:** saves by kind, refusals, catalogue retries, save milliseconds.
- **Health:** none beyond Feature 2's.

## Acceptance Criteria

1. Each command writes its asset at the Asset Table path under `<source>/Global/…` using the envelope and stable formatting, and returns the path.
2. Without the `source` alias, every command fails with the "saving needs a dev launch" message and writes nothing.
3. An id outside `a-z 0-9 _` or over 48 characters is rejected; a path can never leave `<source>/Global/`.
4. An existing file or record is refused unless `overwrite`; a record with no `author` is refused even with `overwrite`; a record with `"author": "game"` is updated in place.
5. File writes are atomic (temporary file then rename); a failed write leaves the previous file or none.
6. The catalogue is re-serialised in the repository format: loading the real `assets.catalogue.json` and writing it unchanged produces byte-identical output (golden test on the checked-in file).
7. A new record has the fields above, `type` equal to the format id, tag `world` and `author` `game`; a bundle record lists `contains` references; a baked world record has `validated` set.
8. The catalogue is read fresh before every write; a change between read and rename triggers one retry and then an Error with nothing replaced.
9. If the catalogue step fails after the asset file was newly created, the file is removed; after an overwrite, the old content is kept and the Error says the record was not updated.
10. `pack.save_as` writes the overrides from `Diff(Defaults, Capture)`, with `base.version` equal to the defaults' current version; re-saving a stale pack clears stale; an empty pack saves with an Info message.
11. `terrain.bake` fails without an accepted world, writes the S-1 world file, records `validated: true`, and marks the loaded terrain as `baked:<id>` with its content hash.
12. `startstate.save_as` fails on non-baked terrain and on an empty edit set, and writes only edited cells keyed to the baked world and its hash.
13. `bundle.save_as` writes the loaded parts, omits unused packs and start state, fails with a message naming the part when any part has unsaved changes, and fails for an un-baked edited terrain.
14. `bundle.set_default` refuses an unknown bundle and a bundle whose baked terrain record has `validated: false`; otherwise it sets `config.default_world` in `cow.diagame` changing no other byte (CRLF and key order preserved), whether the key existed or not; an edit that would change anything else is discarded with an Error.
15. After every successful save, `WorldAssetSaved{kind, id}` is published and the saved id appears in `cow.world.status`.
16. A `dia pipeline --target cow` run between two saves keeps every `author: game` record.

## Test Strategy

- **GoogleTests (TDD)** with a temporary folder as `source` and the Feature 2 fake participants:
  - Path resolution, id rules, traversal attempts, missing alias.
  - Atomic write and overwrite rules (existing file, game record, hand-authored record).
  - Catalogue round trip against copies of the real file and a handful of edge fixtures (empty `references`, non-ASCII display name, nested arrays); upsert add and update; retry on a simulated concurrent change; cleanup after catalogue failure.
  - Each command's output against golden files (pack with overrides, empty pack, stale re-save, baked world record, start state, bundle with and without optional parts).
  - Bundle unsaved-change detection for each part and for edited start-state cells.
  - `DiagameEditor` on copies of the real `cow.diagame` (key present, key absent, CRLF preserved, only one member differs after parsing) and on malformed inputs.
- **pytest (integration):** copy the CoW assets to a scratch folder, run two saves with a `dia pipeline` run between them, assert the game records survive and `dia validate manifest` still passes.
- **Test utilities** in `CoW/Shared/WorldAssets/Testing/`: temp-source builder, catalogue fixtures, golden-file helper.
- No Vitest or E2E: the card is Feature 5.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| AD-001 | Ask before Dia edits | Needs dia-needs #16 (`source` alias), already signed off; this feature edits only CoW files |
| AD-002 | Four-tier placement | CoW-Shared; writes only under `Assets/CoW/` |
| PD-009 | Authored files are source assets in `Assets/` | All writes go to `<source>/Global/…` |
| PD-010 | `.diagame` is the root | `default_world` is set in `cow.diagame` |
| Parent Resolved Decision 3 | The game adds or updates only its own records, tagged `world` with `author: game` | Catalogue upsert rules |
| Parent Resolved Decision 2 | Start states only on baked terrain | `startstate.save_as` refuses otherwise |
| Editor actions via DiaAPI | Every action is a command | The five commands; the card and T-2 call them |
| No singletons | Use module references | Participants through the Feature 2 module |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1 and 2 | Envelope, diff, module, participants | Approved |
| dia-needs #16 (`source` alias) | Where saves are written | Approved, build signed off, not built |
| Feature 4 | `world` tag deploy and the `world` alias | Draft |
| S-1 | `ExportWorld`, `GetCurrentWorld`, a world content hash, `cow.worldgen.validate` | Specced, not built (Feature 6 / S-1 Feature 4) |
| A1 | `Capture()` of edited cells for start states | Specced, not built (Feature 6) |
| T-2 | Writes `validated: false` on hand-edited worlds | Approved (system), features TBD |

## Resolved Decisions

1. **`set_default` belongs here** (2026-10-20). It was in the parent's command table and the S-3.5 spec but in no feature; it writes through the `source` alias like the other saves.
2. **The validated flag lives on the catalogue record** (2026-10-20). Not in the `.wld.json`, so a validate pass never changes the world's content hash and never invalidates start states.
3. **`cow.diagame` gets a text edit, not a rewrite** (2026-10-20). The file has CRLF endings and unsorted `config` keys, so only a minimal edit keeps the diff to one line.
4. **Bundles refuse unsaved parts** (2026-10-20). A bundle that silently dropped a live tweak would load as a different world than the one saved.

## Open Design Questions

_None remaining._

## Status

`Approved` (2026-10-20). Plan not yet created. Needs Features 1 and 2 and dia-needs #16 (build signed off, not started).

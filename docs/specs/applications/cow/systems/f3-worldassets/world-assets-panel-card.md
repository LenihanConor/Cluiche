# Feature Spec: F-3 World Assets Panel Card

**Parent:** @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md
**Builds on:** @docs/specs/applications/cow/systems/f3-worldassets/world-assets-module-and-bundle-load.md (Feature 2), @docs/specs/applications/cow/systems/f3-worldassets/save-as.md (Feature 3), @docs/specs/applications/cow/systems/f3-worldassets/deploy-and-aliases.md (Feature 4)
**Mockup:** @docs/specs/applications/cow/systems/f3-worldassets/mockups/world-assets-card.html (to be extended, see Mockup gate)

**Gameplay Domains:** debug

## Summary

The in-game "World Assets" card: what world is loaded, which pack and start state it uses, what has unsaved changes, and the buttons to load, save and clear each part. It follows the pattern CoW's F-2 test-runner panel already uses: a pure **`WorldAssetsPanelModel`** turns module state into everything the card shows, a panel-only **`WorldAssetsDebugDomain`** (an `IDebugDomain`) carries that state out and the card's requests in, and the card itself is the `world_assets` custom card in CoW's `debug-panel.html`.

Every button runs a `cow.world.*` DiaAPI command; the card never calls module code. Two small read-only commands are added so the card can show previews and reasons.

## Problem

The commands in Features 2 and 3 are enough to author a world from the console, but a tester tuning a world needs to see the state at a glance: which parts are loaded, which are stale or unsaved, whether saving is possible and, if not, why. A card that reads and presses the same commands the console uses gives that without a second code path, and it lets T-2 and the tests rely on the same commands.

## Goals

1. **See the world.** The loaded bundle (or `custom`), each part, stale and unsaved markers, and whether `source` is present.
2. **Act in one click.** Load a bundle, pack, terrain or start state from a list; save a pack, bake terrain, save a start state or bundle through a dialog with a preview; clear a part.
3. **Know why not.** A disabled action shows the reason, such as "Bake terrain first (start states need baked terrain)".
4. **Preview before writing.** The save dialog shows the overrides, the target path and the exact command before anything is written.
5. **Same commands as everything else.** The card, the console and T-2 call the same DiaAPI commands; UI and business logic stay separate.
6. **Honest load state.** A running load shows its step; a partial or failed load shows the error.

## Non-Goals

- The loading, saving and asset logic (Features 2 to 4).
- Editing parameter values (each system's own sliders).
- A file browser, or deleting or renaming assets.
- Editor-side UI (T-2 is the offline editor).
- Shipping builds: the card is `DIA_DEBUG` only.

## Architecture

```
WorldAssetsPanelModel   pure: ModuleState + UiState → CardView ; Intent → command line(s)      (no UI, bus or module deps)
WorldAssetsDebugDomain  IDebugDomain, panel-only: GetJSONState = CardView ; OnCommand = Intent → DiaAPI command
world_assets card       debug-panel.html (CoW copy): renders CardView, posts intents
```

Files: `Cluiche/CoW/Shared/WorldAssets/WorldAssetsPanelModel.*` and `WorldAssetsDebugDomain.*` (registered by `WorldAssetsModule` under `DIA_DEBUG`); the card is added to `Cluiche/Assets/CoW/Global/Presentation/UI/debug-panel.html` as `domain.custom === 'world_assets'`, like `f2_test_runner`. Placement: CoW-Shared (AD-002).

### Panel model

`WorldAssetsPanelModel::Build(const WorldAssetsPanelInput&, const WorldAssetsUiState&) → CardView` is a pure function over plain data:

- **Input** (a snapshot of `cow.world.status` plus the save-availability block, below).
- **UI state** (what is open): which list is open, which dialog is open and its fields (kind, id, display name, overwrite), the cached lists, and the preview result.
- **`CardView`:** header (bundle name or `custom` / `none`, source pill), a row per part (Terrain, Weather Pack, Ecology Pack, Start State) with value, meta chips (baked, size, hash prefix, override count, cell count), badges (`stale vN < vM`, `unsaved: N`, `edited`), per-action enable state and reason, the load-state strip, the footer log line, and the dialog contents.
- **`Intent → command`:** `Intent::LoadBundle(id)` becomes `cow.world.bundle.load <id>`, and so on; the model emits the full command line, which is also what the dialog shows as the command preview.

No UI, bus or module types appear in the model, so it is unit-tested without a running application.

### Commands added for the card

Both are read-only. They are part of the F-3 command surface (Features 2 and 3 are amended to list them), so the console and T-2 can use them too.

| Command | Returns |
|---|---|
| `cow.world.save.preview <pack weather\|pack ecology\|bake\|startstate\|bundle> <id>` | `{ path, exists, ownedByGame, blockedReason?, overrides?: [{key, from, to}], cellCount?, parts?, unsaved?: [...] }`: what the matching save would write, without writing. For a pack it is the `Diff`; for a start state the edited-cell count; for a bundle the parts and any unsaved ones |
| `cow.world.terrain.list` / `terrain.load <id>` | The baked worlds (origin and size), and loading one: the terrain step and the ready step of the bundle loader with the current parameters applied; a start state whose terrain hash differs is cleared with a Warning |

And `cow.world.status` (Feature 2) gains an `actions` block: for each of `terrain.load`, `terrain.bake`, `pack.save_as.weather`, `pack.save_as.ecology`, `startstate.save_as`, `startstate.load`, `bundle.save_as`, `bundle.set_default`, `{ enabled, reason? }`. The reasons are the same strings the commands return when refused, produced by one function so the card and the command can never disagree.

### Domain

`WorldAssetsDebugDomain` is panel-only (no world drawers), like `F2TestRunnerDebugDomain`:
- **`GetJSONState`:** `{ "panel": "world_assets", "view": <CardView> }`. The status snapshot is refreshed at most twice a second and on every `WorldBundleLoaded`, `WorldBundleFailed`, `WorldAssetSaved` and `WorldGenerationComplete`, so the unsaved-count diffs cost almost nothing.
- **`OnCommand`:** `open_list {kind}`, `close_list`, `open_dialog {kind}`, `set_field {name, value}`, `close_dialog`, `confirm`, plus the direct actions `load {kind, id}`, `clear {kind}`. List and dialog commands change only UI state (and run the read-only list and preview commands); `confirm`, `load` and `clear` run the matching `cow.world.*` command and show its result line in the footer.
- Manual actions are refused while a bundle load is running (the card shows why).

### Card behaviour (from the mockup)

- **Header:** bundle display name (`custom` after a live pack or terrain change, `none` before any load), a source pill: green "source: Cluiche/Assets/CoW" when saving is possible, grey "read-only: no source alias" in a packaged or direct launch.
- **Part rows:** Terrain (`Load ▾`, `Bake as…`), Weather Pack, Ecology Pack and Start State (`Load ▾`, `Save as…`, `Clear`). The terrain row has no Clear: a world always has terrain.
- **Load dropdown:** the matching `list` result, each entry with its display name, origin (`source` or `deployed`) and stale badge; choosing one runs the load command.
- **Badges:** `stale vN < vM` with a tooltip listing the dropped keys; `unsaved: N` (the override count the next save would write); `edited` for hand-painted ecology cells.
- **Save dialog:** ID (lower-case letters, digits, `_`), display name, an overwrite checkbox, then the preview box: the overrides diff, the target path, and the command line that Save will run. Save is disabled while the preview reports a block (existing id without overwrite, a hand-authored record, unsaved parts for a bundle) and shows the reason.
- **Disabled actions** show their reason inline, for example "Start State Save Unavailable: bake the terrain first (current terrain: preset + seed)".
- **Load strip:** the current step while a load runs, a red "partial" or "failed" banner with the error afterwards (Feature 2 states), and a Dismiss button.
- **Footer:** the last `[WAS]` line.

### Mockup gate

The mockup exists and is the visual reference. Before the card is built it is **extended and re-approved** to add what Features 2 to 4 introduced: the load-step strip and the partial/failed banner, the open Load dropdown with origin and stale badges, a disabled button with its reason, the `custom` / `none` bundle states, the read-only source pill, and the terrain row without Clear. Building the card starts only after that mockup is approved.

### Page tests and the CoW panel copy

CoW's `debug-panel.html` is a copy of CluicheTest's. The existing panel page tests load the CluicheTest copy; this feature makes the loader take the page path, adds CoW's copy to the set, and adds the `world_assets` card tests. A change to the shared panel script still has to be re-synced into CoW's copy (the F-1 follow-up), now covered by the page tests running against both.

### Logging and observation

- **Channel `WAS`** (the card adds no channel): Info when a dialog confirms a command (the command line); Warning when a card request is refused.
- **Metrics:** card commands accepted and refused (like the F-2 domain's counters).
- **Health:** none beyond Feature 2's.

## Acceptance Criteria

1. `WorldAssetsPanelModel::Build` produces the header, the four part rows with values, chips and badges, the action enable states with reasons, the load strip, the footer and the dialog contents from plain-data input, with no UI, bus or module dependency.
2. Every intent maps to exactly one `cow.world.*` command line, and the dialog's command preview equals the line `confirm` runs.
3. `cow.world.save.preview` returns the path, existence, ownership, the overrides diff (packs), the edited cell count (start state), or the parts and unsaved parts (bundle), writes nothing, and reports the same block reason the save command would.
4. `cow.world.status.actions` gives `{enabled, reason}` for each action, and each reason string is the one the command returns when refused (one source).
5. `cow.world.terrain.list` and `terrain.load <id>` list the baked worlds with origin and size and load one with the current parameters applied; a start state with a different terrain hash is cleared with a Warning.
6. The domain's state is the card view; its list and dialog commands change only UI state and run only read-only commands; `confirm`, `load` and `clear` run the matching command; all are refused with a visible reason while a load runs.
7. The card renders: bundle name or `custom` / `none`, the source pill in both states, the part rows with chips and `stale` / `unsaved` / `edited` badges, the Load dropdown with origin and stale badges, the disabled-with-reason state, the load strip and the partial/failed banner, the save dialog with diff preview, target path and command preview, and the footer line.
8. The terrain row has Load and Bake but no Clear; the other parts have Load, Save as and Clear.
9. In a packaged or direct launch (no `source` alias) all save actions are disabled with the "saving needs a dev launch" reason and loading still works.
10. The status snapshot is refreshed at most twice a second and on the four events listed; the unsaved counts match `Diff` against the loaded pack or defaults.
11. The state JSON matches a checked-in golden file and the page tests render the same file (the existing panel contract pattern).
12. The extended mockup is approved before card code starts, and the finished card matches it.
13. The card exists only under `DIA_DEBUG`.

## Test Strategy

- **GoogleTests (TDD):**
  - `WorldAssetsPanelModel` over input fixtures: every badge, every action state and reason, load strip states, dialog contents, intent to command mapping.
  - `save.preview` for each kind and each block reason; `status.actions` reasons equal to the refusal messages from the save commands (a table-driven test over both).
  - `terrain.list` / `terrain.load` with the Feature 2 fake participants.
  - `WorldAssetsDebugDomain` state shape, UI-only commands, refusal while loading, counters.
  - Throttled refresh with a fake clock and event triggers.
- **Contract golden:** `Cluiche/Tests/Contracts/` entry for the `world_assets` state (the same pattern as the debug panel contracts), produced by the real builders.
- **Vitest (page tests):** load CoW's `debug-panel.html`, feed the golden and fixture states, assert the card DOM (rows, badges, dropdown, dialog, disabled reasons, banner); run the existing panel page tests against both copies of the page.
- **Visual gate:** the extended mockup, compared before the feature is marked Done.
- **Manual (until CoW has a headless launch):** in S-3 tune a value, Save as…, Load it back, Save bundle as…, relaunch and load the bundle.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| Parent / editor-actions rule | Every authoring action is a command and the panel calls the same commands | The card runs only `cow.world.*` commands; previews and lists are commands too |
| UI/business separation | A typed model first; the UI depends only on it | `WorldAssetsPanelModel` is pure; the card renders its output |
| Mockup-first rule | A visual mockup before any editor feature is built | The existing mockup is extended and approved first (Mockup gate) |
| AD-005 | One stage per testbed; modes in a panel | The card is a panel in existing stages, never a new stage |
| AD-002 | Four-tier placement | CoW-Shared model and domain; the card in CoW's page copy |
| F-1 pattern (F-2 panel) | Panel model + panel-only domain + custom card | Same shape as `F2PanelModel` / `F2TestRunnerDebugDomain` |
| No singletons | Use module references | The domain holds the module by reference |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 2, 3, 4 | State, commands, locator | Approved |
| Amendments to Features 2 and 3 | `terrain.list` / `terrain.load`, `status.actions`, `save.preview` | Made with this spec (see those specs) |
| F-1 debug host: `DiaVisualDebugger` panel, `IDebugDomain` | Hosting | Done |
| CoW `debug-panel.html` copy and the panel page tests | The card and its tests | Done (extended) |
| DiaValueSchema Feature 3 `renderValueTooltip` | Optional: stale-key tooltips could use it later | Approved; not required here |

## Open Design Questions

_None remaining._ Resolved here: terrain gets Load but no Clear; the read-only commands the card needs are added to the F-3 surface instead of reading module internals; the status is refreshed twice a second and on events rather than every frame.

## Status

`Approved` (2026-10-20). Plan not yet created. Card code starts only after the extended mockup is approved (Mockup gate).

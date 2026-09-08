# Research: Ideate — Plugin Browser Scaling

**Input:** docs/research/plugin_browser/explore.md

## Candidates

### Candidate 1: Project-Scoped Filtering (C++ side only)
**Home module/system:** DiaEditor — `EditorPluginRegistry` + `.diaapp` manifest schema
**Size:** S
**Description:** The C++ `EditorPluginRegistry::GetPlugins()` response already drives the Plugin Browser. This candidate adds a single filter: only return plugins whose `.diaapp` manifest is referenced by the currently loaded `.cluicheproj`. No UI changes at all — CoW plugins simply never appear in the CluicheTest browser because their manifest isn't loaded. The plugin browser iframe gets a shorter list and the problem evaporates for the common case.

There is one caveat: if no project is loaded (editor cold-start, before any `.cluicheproj` is opened), all plugins are shown. A "no project open" state is expected; the filter only activates when a project is active.

**Primary value:** CoW plugins stop appearing in CluicheTest workspaces with zero UI work — the natural manifest boundary does the filtering.

---

### Candidate 2: Search Bar in Plugin Browser
**Home module/system:** CluicheEditor UI — Plugin Browser panel iframe
**Size:** S
**Description:** Add a text input at the top of the Plugin Browser iframe panel that fuzzy-filters the visible plugin list as the user types. fuse.js is already a dependency in the project (used by `CommandPalette.tsx`). The bridge response doesn't change; search is purely client-side over the list returned by `getPlugins()`. Works immediately on lists of 20–40 items.

Does not solve context contamination on its own, but dramatically improves discovery. Pairs naturally with Candidate 1 (filter first to project-relevant plugins, then search within them).

**Primary value:** Any developer can find any plugin in under 2 keystrokes regardless of list length.

---

### Candidate 3: Category Grouping via `.diaapp` Metadata
**Home module/system:** DiaEditor — `IEditorPlugin` interface + `.diaapp` schema + Plugin Browser UI
**Size:** M
**Description:** Add a `category` field to the `editor.plugins[]` entry in `.diaapp` manifests (e.g. `"category": "Engine"`, `"category": "Assets"`, `"category": "Game/CoW"`). The C++ plugin registry reads this field and includes it in the `getPlugins()` bridge response. The Plugin Browser UI renders plugins grouped by category — collapsible accordion sections, one per category. Uncategorised plugins fall into an "Other" bucket.

Categories are editor-defined string labels, not an enum — new games can introduce new category strings without changing C++ code. The UI groups by string equality; ordering of categories is declaration order in the response (or alphabetical as a fallback).

**Primary value:** Engine developers see "Engine / Assets / Pipeline" groups; CoW developers see a "Game / CoW" section below. Clear ownership at a glance.

---

### Candidate 4: `game_id` Affinity Field + Context Badge
**Home module/system:** DiaEditor — `.diaapp` schema + Plugin Browser UI
**Size:** M
**Description:** Add an optional `game_id` field to each `editor.plugins[]` entry in `.diaapp` (e.g. `"game_id": "cow"`). The currently loaded `.cluicheproj` exposes its target game ID (derived from the `.diagame` root file per PD-010). The Plugin Browser requests this context via a new `get_project_context` bridge call and renders:
- **In-project plugins** — normal display
- **Out-of-project plugins** — greyed row, labelled "Not in this project", with a "Load anyway" button

This is the "show all, contextualise" model rather than the "hide irrelevant" model. It prioritises discovery (you can always see that CoW plugins exist) while making context obvious. Pinned out-of-project plugins are shown normally so the user can unpin them.

**Primary value:** Developers can discover plugins from other games without switching projects; context is explicit not invisible.

---

### Candidate 5: Filter Chips (Status + Category)
**Home module/system:** CluicheEditor UI — Plugin Browser panel iframe
**Size:** S
**Description:** Add a row of filter chip buttons below the (optionally added) search bar: `All | Loaded | Available | Pinned` (status chips) and one chip per category (once category metadata exists). Clicking a chip narrows the list. Multiple chips can be active simultaneously (AND logic). No C++ changes needed for the status chips — `loaded`/`available`/`pinned` are already in the bridge response. Category chips require Candidate 3's metadata.

On its own (without category metadata), this candidate gives a status filter that's useful today. With Candidate 3 it becomes a full faceted filter.

**Primary value:** Power users can quickly narrow to "Loaded" or a specific category without typing; works as a complement to search.

---

### Candidate 6: Pinned/Favourites Section at Top
**Home module/system:** CluicheEditor UI — Plugin Browser panel iframe (+ C++ persist)
**Size:** S
**Description:** Hoist all PINNED plugins into a sticky "Pinned" section at the top of the browser, separated from the general list. The current PINNED badge is already rendered inline — this simply reorders the list so pinned plugins are always visible without scrolling. Persistence of pin state is already handled by C++ (PINNED is already a state). No schema changes; purely a UI reordering.

Works with the flat list today and survives any later categorisation — the "Pinned" section sits above whatever grouping exists below it.

**Primary value:** Frequently used plugins are one-click accessible regardless of list length.

---

### Candidate 7: Manifest-Scoped Plugin Packages ("Plugin Packs")
**Home module/system:** DiaEditor — `EditorPluginRegistry` + `.cluicheproj` schema
**Size:** L
**Description:** Formalise the idea that a `.cluicheproj` references named "plugin packs" — each pack is a `.diaapp` manifest with a declared `pack_name`. The Plugin Browser renders one collapsible section per pack: "Dia Engine Pack", "CluicheTest Pack", "CoW Pack". Loading/unloading a pack loads/unloads all its plugins atomically. A developer can enable the CoW pack temporarily while working on CluicheTest (e.g. to test cross-game compatibility), then collapse and ignore it.

This is the most structured model. It requires extending `.cluicheproj` to record enabled packs, a new C++ pack-load concept in `EditorPluginRegistry`, and non-trivial UI. It is the right long-term architecture if packs grow to 5+.

**Primary value:** Pack-level enable/disable is a single click; cross-game plugin experimentation is first-class without permanent contamination.

---

### Candidate 8: Command Palette Plugin Actions (No Browser Changes)
**Home module/system:** CluicheEditor UI — `CommandPalette.tsx`
**Size:** S
**Description:** Rather than redesigning the Plugin Browser, surface plugin load/unload/pin as commands in the existing command palette (`Ctrl+Shift+P`). The palette already does fuzzy search. Add `plugin.load.<name>`, `plugin.unload.<name>`, `plugin.pin.<name>` commands to the C++ command registry. Developers who know what they want never need to open the Plugin Browser at all.

This is an orthogonal improvement that does not replace the Plugin Browser — it adds a keyboard-driven shortcut path for power users who know the plugin name. Not a substitute for browsing/discovery.

**Primary value:** Zero-friction load/unload for developers who already know what they want; complements any browser redesign.

---

## Coverage Map

The 8 candidates cover the full range of the design axes from explore.md:

| Design Axis | Candidates covering it |
|-------------|----------------------|
| Organisation (grouping) | C3 (category), C7 (packs) |
| Discovery (search/filter) | C2 (search), C5 (chips), C8 (palette) |
| Context filtering | C1 (project-scoped, invisible), C4 (affinity, visible) |
| Pinning UX | C6 (pinned section) |
| Keyboard / power users | C8 (palette) |
| Backend schema | C1, C3, C4, C7 (all require C++ changes of varying depth) |

**Scope range:** S (C1, C2, C5, C6, C8) through M (C3, C4) to L (C7). At least one candidate in every size tier.

**Composability note:** Most candidates are additive and stack well. A natural progression is C1 → C2 → C6 → C5 (project filter, then search, then pinned section, then status chips) — all buildable incrementally. C3 and C7 require schema work and should be sequenced deliberately.

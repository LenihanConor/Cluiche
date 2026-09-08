# Research: Explore — Plugin Browser Scaling

**Session date:** 2026-05-22
**Folder:** docs/research/plugin_browser/

## Problem Space Overview

The Plugin Browser is a panel in CluicheEditor that lists all available editor plugins, showing their name, version, description, and load/unload status. Today it renders a flat scrollable list (see screenshot). With ~4 plugins it is manageable. The user projects 10s of plugins as the platform matures — generic engine plugins (DiaApplicationEditor, DiaAssetCatalogueEditor, DiaAssetRuntimeEditor, Pipeline Editor) plus per-game plugins (future CoW asset editors, creature editors, dialogue editors) that have no business appearing in a CluicheTest workspace.

Two problems are converging:
- **Discovery at scale**: A flat list of 20–40 items is hard to scan. Users need grouping, filtering, or search to find what they want. The current "AVAILABLE / LOADED / PINNED" badge system is a start, but the list has no structure.
- **Context contamination**: CoW-specific plugins are irrelevant noise when a developer is working on CluicheTest, and vice-versa. The plugin browser must understand what project is open and surface only relevant plugins — or at minimum allow the user to suppress irrelevant ones.

The two problems are linked. Any redesign that solves discovery must also have a first-class answer to context: how does the browser know which plugins are relevant to the current project?

## Existing Approaches

Industry patterns for plugin/extension browsing in comparable tools:

- **VS Code Extensions Panel** — Sidebar with search bar at top, tabs for "Installed / Available / Recommended". Extensions are grouped by publisher. Recommended extensions are surfaced based on open workspace type (e.g. `.py` files → Python extension).
- **JetBrains Plugin Marketplace** — Category tree on the left, search + filtering on the right. Plugins tagged with `bundled`, `installed`, `marketplace`. Filter by category (Languages, Frameworks, Build Tools, etc.).
- **Unreal Engine Plugin Browser** — Plugins categorized by built-in / project / enabled. Checkboxes for enable/disable per category row. No search.
- **Unity Package Manager** — Left-side list of packages by registry (Unity / My Assets / In Project). Search bar. Tag chips for filtering. "In Project" vs "Unity Registry" is the context filter.
- **Blender Add-ons** — Categorised accordion list. Search box. Enabled/disabled toggle per row. Category is a first-class attribute on each add-on.
- **Sublime Text Packages** — Command palette-based (`Install Package`, fuzzy search). No visual browser.
- **Figma Plugins** — Search + category filter. "Used in this file" context surfacing. Tag-based filtering.

Common patterns emerging from the above:
- Search/fuzzy filter is near-universal for >20 items
- Categories/tags are the primary organisational unit
- Context-aware surfacing ("in this project", "relevant to workspace") is a differentiator
- Load/unload (enable/disable) is an in-row action, not a separate step
- Pinning / favourites reduces repeated searching

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Organisation** | Flat list, Grouped by category, Grouped by owner/game, Tag cloud | Flat already exists; categories are the minimum viable next step |
| **Discovery** | Scroll only, Search bar, Fuzzy search, Filter chips | fuse.js is already in the project; fuzzy search is nearly free |
| **Context filtering** | None (show all), Project-scoped (filter by .cluicheproj), Tag-based, Manifest-declared game affinity | Project-scoped is the principled answer; tag-based is the quick answer |
| **Category authority** | Hardcoded in UI, Manifest-declared by plugin, Inferred from namespace (Dia* vs CoW*) | Manifest-declared is the correct long-term approach; namespace inference works as a stopgap |
| **Surface area** | Same panel (Plugin Browser iframe), Sidebar panel, Command palette only | Plugin browser is already a panel; keeping it a panel is the smallest change |
| **Pinning** | Already partially present (PINNED badge visible) | Infrastructure exists; UX could be made more intentional |
| **Load state display** | AVAILABLE / LOADED / PINNED (current) | Works well; should be preserved in any redesign |

## Known Tradeoffs

- **Category authority in manifest vs inferred**: Declaring categories in `.diaapp` manifests is clean but requires a schema extension and the C++ plugin bridge to pass category metadata through. Namespace-based inference (plugins starting with "CoW" get tagged "CoW Game") works today with zero backend changes but is fragile.
- **Project-scoped filtering vs global**: Filtering by project requires the Plugin Browser to know which `.cluicheproj` is open and which plugin manifests it references. The `ProjectContextButton` already has project context — sharing it with the Plugin Browser needs a bridge event or a new request type.
- **Search in a panel vs command palette**: fuse.js fuzzy search in the Plugin Browser panel is the natural choice. The command palette already handles command search; mixing plugin browsing into it would blur responsibilities.
- **Flat categories vs hierarchical**: A two-level hierarchy (category → plugins) is sufficient. Three levels (game → category → plugins) would future-proof CoW/CluicheTest separation but is premature.
- **React state vs C++ state**: The Plugin Browser is a panel iframe; it communicates via the EditorBridge. Any metadata (category, game affinity) must be provided by C++ through the bridge response — the UI cannot invent it.

## Known Pitfalls (C++ / game engine context)

- **Bridge schema creep**: Adding `category`, `tags`, `gameAffinity` fields to the plugin registry response is additive and non-breaking, but it requires a deliberate C++ schema change. Don't assume the UI can carry category logic alone.
- **Plugin count growth is not gradual**: The step from 4 to 20 plugins may happen in a single sprint if a new game (CoW) is started. Design for 30+ now.
- **Pinning semantics**: "PINNED" currently means "load on startup". Context filtering must not remove pinned plugins from the list — a pinned CoW plugin in a CluicheTest project should be visible (possibly greyed) so the user can unpin it.
- **iframe isolation**: The Plugin Browser is a standalone iframe. It calls the bridge via `window.parent.postMessage`. Any new bridge request type must be routed through the iframe relay already in `EditorBridge.ts`.
- **Per-game plugin packages**: CoW plugins will live in a CoW-specific `.diaapp` manifest, not in the shared `editor-plugins.diaapp`. Context filtering can therefore be implemented by the C++ side simply omitting plugins whose manifests aren't referenced by the current `.cluicheproj` — no UI logic required for the core case.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module / File | Relevance |
|---------------|-----------|
| `EditorBridge.ts` — `getPanels()` / `panels_changed` | Plugin browser likely uses a similar `getPlugins()` request; model can follow the same pattern |
| `ProjectContextButton.tsx` | Already holds project context (current `.cluicheproj` path); project-scoped filtering needs this context shared |
| `CommandPalette.tsx` | fuse.js is already wired; reuse the search pattern |
| `DiaEditor` (C++) — `EditorPluginRegistry` | The backend that owns the plugin list; category/gameAffinity metadata must be added here |
| `.cluicheproj` project file | References the set of `.diaapp` manifests for the current project; this is the natural project-scope filter boundary |
| `AED-002` — plugins declared in `.diaapp` `editor` section | Any new metadata fields (category, tags) live here; schema extension is well-defined |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Plugin IDs on the C++ side are StringCRC; the JSON bridge already converts them to strings — no change needed for search/filter |
| PD-004 No STL in public APIs | Category/tag data passed over the bridge is JSON; the C++ `EditorPluginRegistry` must use DiaCore containers internally |
| AED-002 Plugins in `.diaapp` manifest | Category and gameAffinity fields should be added to the `editor.plugins[]` schema, not invented in the UI |
| AED-003 Each system owns its editor as `<System>/Editor/` | Namespace-based category inference maps cleanly to this ownership rule |
| PD-010 `.diagame` is the project root | Game-affinity filtering ultimately traces back to which `.diagame` is active; the `.cluicheproj` → `.diaapp` chain already encodes this |

## Open Questions for Ideation

- Should **category** be a first-class C++ concept (added to `IEditorPlugin::GetCategory()` virtual) or a metadata-only field in `.diaapp` with no C++ interface change?
- Should **game affinity** be declared explicitly (a `game_id` field in the manifest) or inferred from which `.diaapp` file the plugin lives in?
- Should the Plugin Browser show plugins **not** in the current project (greyed out, from a global registry) or **only** project-relevant plugins? The former gives discoverability; the latter gives focus.
- Should **search** be in the Plugin Browser iframe or in the main toolbar (command palette)?
- Can **pinning** survive context filtering — i.e. should pinned CoW plugins remain visible in a CluicheTest project?
- Is the Plugin Browser always a dockable panel, or could it also be a floating modal (like a marketplace)?
- Should categories be user-customisable or editor-defined only?

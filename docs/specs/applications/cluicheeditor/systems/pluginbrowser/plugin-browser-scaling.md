# Feature Spec: Plugin Browser Scaling

**Status:** Approved
**Application:** CluicheEditor
**System:** CluicheEditor Plugin Browser
**Research:** docs/research/plugin_browser/summary.md

## Parent Specs

- Platform: @docs/specs/platform/Cluiche.md
- Application: @docs/specs/applications/cluicheeditor/cluicheeditor.md
- System: @docs/specs/applications/cluicheeditor/systems/pluginbrowser/pluginbrowser.md

## Summary

The Plugin Browser panel currently renders a flat scrollable list of all plugins. As the platform grows to 20–40 plugins across engine and per-game domains, two problems converge: the flat list doesn't scale for discovery, and CoW-specific plugins contaminate the CluicheTest workspace. This feature fixes both with five targeted changes: project-scoped C++ filtering, a compact master-detail layout, client-side fuse.js search, status filter chips, and command palette load/unload actions.

## Problem

`PluginBrowser` renders every registered plugin in a flat scrollable list. With 4 plugins today it is manageable. At 20–40 (engine plugins + per-game plugins), two distinct problems emerge:

1. **Discovery at scale** — a flat list of 40 items with no grouping, search, or filtering makes it hard to find any specific plugin.
2. **Context contamination** — CoW-specific plugins registered via a CoW `.diaapp` manifest appear in CluicheTest workspaces where they have no relevance.

## Goals

1. Eliminate context contamination by filtering the plugin list to the current project's manifests at the C++ layer
2. Solve discovery at scale with a compact master-detail layout that renders 40+ plugins without scrolling walls
3. Add search and status filtering to let users narrow to a specific plugin in ≤2 keystrokes
4. Add keyboard-first load/unload via command palette for developers who know the plugin name

## Acceptance Criteria

| ID | Criterion |
|----|-----------|
| AC1 | When a `.cluicheproj` is loaded, `EditorPluginRegistry::GetPlugins()` returns only plugins whose `.diaapp` manifest is in the project's manifest list; plugins from other manifests are omitted |
| AC2 | When no project is loaded (cold-start), all registered plugins are returned (no filter applied) |
| AC3 | The Plugin Browser renders plugins as compact single-line rows: plugin name on the left, load/unload button on the right; row height ≤ 32px |
| AC4 | A persistent detail pane is anchored at the bottom of the Plugin Browser panel; selecting a row populates the detail pane with the plugin's name, description, version, and status |
| AC5 | A search input at the top of the Plugin Browser filters the displayed list via fuse.js over the plugin `name` and `description` fields; matching is client-side, no new bridge calls |
| AC6 | Three status filter chips are displayed below the search input: `All`, `Loaded`, `Available`; exactly one is active at a time; `All` is the default |
| AC7 | `plugin.load.<plugin_id>` and `plugin.unload.<plugin_id>` commands are registered in the C++ command registry and appear in the command palette; `<plugin_id>` is the plugin's StringCRC identifier in string form |
| AC8 | The load/unload button in each row calls the existing `EditorBridge.loadPlugin` / `EditorBridge.unloadPlugin` bridge events — no new bridge contract |
| AC9 | Filter chips and search compose: if `Loaded` is active and the user types a search term, only loaded plugins matching the search are shown |
| AC10 | The detail pane has a fixed default height of 120px; it is always visible with a "Select a plugin" placeholder when nothing is selected |

## Out of Scope

- Category grouping (C3) — deferred until 10+ plugins exist; requires `.diaapp` schema extension
- Plugin Packs (C7) — deferred; requires `.cluicheproj` schema extension and pack-load C++ concept
- Pin/favourite actions — deferred to profiles pass
- Greyed-out display of out-of-scope plugins — C1 omits them silently; "show greyed" model is C4 (not chosen)
- Resizable detail pane — fixed at 120px for this feature
- Category filter chips — deferred to C3; only status chips in this feature
- `plugin.pin` / `plugin.unpin` palette commands — deferred with pin UX

## Tasks

| # | Task | Notes |
|---|------|-------|
| T1 | Add project-scope filter to `EditorPluginRegistry::GetPlugins()` | C++ (DiaEditor). Add `SetActiveManifests(const Dia::Core::DynamicArrayC<Dia::Core::StringCRC>&)`. `GetPlugins()` filters by manifest when list is non-empty; empty = show all. |
| T2 | Wire active manifests into registry on project load | C++ (CluicheEditor). `PluginLoaderModule::DoStart` calls `EditorPluginRegistry::SetActiveManifests()` with StringCRC IDs of loaded `.diaapp` paths after the manifest load loop; built-ins (empty manifestId) always pass the filter. `EditorModel` is not involved — it has no typeId→manifest mapping. |
| T3 | Implement compact list + detail pane layout in Plugin Browser | React/TSX (CluicheEditor UI). Replace existing flat-list render with tight single-line rows + fixed 120px detail pane anchored at bottom. |
| T4 | Add fuse.js search input | React/TSX. Search input at top of panel; fuse.js over `name` + `description`; no bridge calls. |
| T5 | Add `All / Loaded / Available` status filter chips | React/TSX. Single-select chip row below search; chips compose with search filter. |
| T6 | Register `plugin.load.<id>` and `plugin.unload.<id>` commands | C++ (DiaEditor). New `PluginCommandRegistrar` helper (~40 lines) iterates the registry at startup and registers one load + one unload command per plugin in `CommandRegistry`. |
| T7 | Wire command handlers to existing load/unload API | C++ command handlers call `EditorPluginRegistry::LoadPlugin()` / `UnloadPlugin()` — same path as the row button. |

## Traceability

| Level | Spec | Key Constraint |
|-------|------|---------------|
| Platform | Cluiche.md | PD-001: StringCRC for all IDs — plugin command IDs use StringCRC; PD-004: No STL in public APIs — `SetActiveManifests` uses `DynamicArrayC<StringCRC>` |
| Application | cluicheeditor.md | AED-002: Plugins specified in `.diaapp` manifest — C1 filter boundary respects manifest ownership; AED-005: React + DiaUICEF — UI changes are React-only; AED-006: `.cluicheproj` is top-level project file — filter uses its manifest list |
| System | pluginbrowser.md | SCPB-001: Filtering is C++-side only; SCPB-002: Filter is display-only, doesn't unload; SCPB-003: Categories deferred |

## Binding Decisions Compliance

| ID | Decision (plain language) | How this feature complies |
|----|--------------------------|--------------------------|
| PD-001 | Use StringCRC for all entity/component IDs | Plugin IDs in C++ are `StringCRC`; palette commands registered as `plugin.load.<stringcrc_str>`; `SetActiveManifests` takes `DynamicArrayC<StringCRC>` |
| PD-002 | ProcessingUnit/Phase/Module architecture for app structure | T2 wiring runs through `PluginLoaderModule::DoStart`; no new modules or processing units introduced |
| PD-003 | Component-based entities | Not applicable — editor UI panel, not a game entity |
| PD-004 | No STL containers in public APIs | `SetActiveManifests` uses `Dia::Core::DynamicArrayC<Dia::Core::StringCRC>`; `GetPlugins()` return type uses DiaCore containers |
| PD-005 | x64 Windows only | Compliant — no platform-specific code added |
| PD-006 | Visual Studio project files are source of truth | DiaEditor.vcxproj updated for T1/T6 new files; no per-project override of build paths |
| PD-007 | C++20 required | All new C++ uses `/std:c++20`; `constexpr StringCRC` in plugin IDs |
| PD-008 | Directory.Build.props owns OutDir/IntDir/toolchain | No per-project overrides added |
| PD-009 | Generated non-binary output under `Cluiche/out/<AppName>/` | Not applicable — no generated file output |
| PD-010 | `.diagame` is the project root; systems resolve from `.diagame` | C1 filter uses `.cluicheproj` manifest list (editor project scope); `.cluicheproj` is the editor's root per AED-006 — not in conflict with PD-010 which governs runtime/pipeline discovery |
| AED-001 | DiaEditor is a pure library; CluicheEditor owns application flow | T1/T2/T6/T7 add capability to DiaEditor library; `PluginLoaderModule` in CluicheEditor wires the manifest list in — library has no ApplicationFlow dependency |
| AED-002 | Plugins specified in `.diaapp` manifest `editor` section | C1 filter boundary is the `.diaapp` manifest list from `.cluicheproj` — consistent with this decision |
| AED-005 | UI built with React + DiaUICEF + react-mosaic | T3/T4/T5 are React/TSX only; no DiaUICEF or mosaic internals touched |
| AED-006 | `.cluicheproj` is the top-level project file | T2 reads manifest paths from the loaded `.cluicheproj` via `EditorModel`; compliant |
| SCPB-001 | Project-scope filtering is C++-side only | T1 implements filter in `EditorPluginRegistry`; UI receives an already-filtered list |
| SCPB-002 | Filter is display-only; does not unload out-of-scope plugins | `SetActiveManifests` affects `GetPlugins()` return value only; no unload side-effects |
| SCPB-003 | Category and pack grouping deferred | No category fields, no pack concept in this feature |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|------------------|--------|
| 1 | T1/T2 | How does `EditorPluginRegistry` receive the active manifest list — via `SetActiveManifests()` called from `EditorModel`, or via a query at `GetPlugins()` time? | `SetActiveManifests()` called once on project load/close | `SetActiveManifests()` called once on project load/close |
| 2 | AC2 | What does the Plugin Browser show during cold-start (no project loaded)? | All plugins unfiltered — empty manifest list = no filter | All plugins unfiltered |
| 3 | AC1 | If a plugin from an out-of-scope manifest is already loaded, does the filter hide it from the panel? | Yes — filter is display-only; it hides but does not unload | Yes — hidden from panel but not unloaded |
| 4 | T6 | Should `plugin.load.<id>` commands be registered at startup (for all known plugins) or dynamically as plugins are discovered? | At startup, for all plugins registered in the registry | At startup for all known plugins |
| 5 | T6 | Should `plugin.load.<id>` be a no-op if the plugin is already loaded, or report an error? | Silent no-op — idempotent; consistent with `loadPlugin` bridge behavior | Guard early-return if already loaded; emit `DIA_LOG_INFO` ("plugin already loaded: <id>"); do not load twice. Same guard applies to the load/unload button path. |
| 6 | AC5 | Should fuse.js search over `name` only or `name` + `description`? | Both — fuse.js threshold 0.4 | Both `name` and `description`; threshold 0.4 |
| 7 | AC10 | Is the detail pane always visible (placeholder when nothing selected), or hidden until a row is clicked? | Always visible with "Select a plugin" placeholder | Always visible with placeholder |

## Open Questions

None — all pre-spec commitments resolved in research choose.md.

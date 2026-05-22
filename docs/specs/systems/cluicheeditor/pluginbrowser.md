# System Spec: CluicheEditor Plugin Browser

## Parent Application
@docs/specs/applications/cluicheeditor.md

## Purpose

The Plugin Browser is the panel in CluicheEditor that lets developers discover, load, and unload editor plugins. It owns the full lifecycle of the plugin browsing surface: the panel UI (React), the bridge contract between panel and C++ (`getPlugins`, `loadPlugin`, `unloadPlugin`), and the C++ `EditorPluginRegistry` query API that backs the panel.

## Responsibilities

- Render the list of available and loaded editor plugins in a browsable panel
- Filter the plugin list to the current project's manifests (context scoping)
- Provide search and status filtering within the panel UI
- Register plugin load/unload as command palette commands
- Own the `getPlugins` bridge response shape and the `EditorPluginRegistry` query API

## Non-Responsibilities

- **Plugin implementation** — each plugin lives with its system (AED-003)
- **Plugin loading lifecycle** — owned by `PluginLoaderModule` / `EditorModel`; the browser calls existing load/unload APIs
- **`.diaapp` manifest schema** — owned by the DiaEditor system
- **Category or pack metadata** — deferred to future system specs (C3 / C7 from research)

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Plugin Browser Scaling | Project-scope filter + compact master-detail layout + search + filter chips + palette commands | [plugin-browser-scaling.md](../../features/cluicheeditor/pluginbrowser/plugin-browser-scaling.md) | Approved |

## Dependencies

**Required:**
- **Dia.DiaEditor** — `EditorPluginRegistry`, `IEditorPlugin`, `CommandRegistry`
- **CluicheEditor UI** — React panel iframe, `EditorBridge.ts`
- **fuse.js** — Client-side fuzzy search (already a dependency)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SCPB-001 | Project-scope filtering is C++-side only; UI receives an already-filtered list | Keeps the UI stateless with respect to project context; the C++ registry owns the authoritative list | Plugin list filtering | Accepted | Yes |
| SCPB-002 | Filter is display-only; it does not unload plugins already loaded from out-of-scope manifests | Loading is a side-effect action; filter is a view concern — unloading out-of-scope plugins on project switch is destructive and out of scope | Filtering behaviour | Accepted | Yes |
| SCPB-003 | Category and pack grouping are deferred; flat list is the baseline | Premature until 10+ plugins exist; schema changes (C3/C7) should be driven by a dedicated feature | Category/pack | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | StringCRC for all IDs | Plugin IDs on the C++ side are StringCRC; bridge converts to strings |
| PD-004 | Platform | No STL containers in public APIs | `EditorPluginRegistry` query API uses DiaCore containers |
| AED-002 | CluicheEditor App | Plugins specified in `.diaapp` manifest | Project-scope filter boundary is the manifest list from `.cluicheproj` |
| AED-005 | CluicheEditor App | React + DiaUICEF UI stack | Panel UI is React/TSX; bridge contract is JSON over CEF |
| AED-006 | CluicheEditor App | `.cluicheproj` is the top-level project file | Active manifest list comes from the loaded `.cluicheproj` |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Scope | Should the Plugin Browser system spec also own the panel docking/registration, or just the browsing surface? | Browsing surface only — docking/registration is owned by DiaEditor's panel system |

## Status

`Draft`

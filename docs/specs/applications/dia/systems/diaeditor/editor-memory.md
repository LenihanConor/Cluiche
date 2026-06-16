# Feature Spec: Editor Memory

**Parent:** @docs/specs/applications/dia/systems/diaeditor/diaeditor.md
**Research:** @docs/research/editor_workspac/summary.md
**Status:** Done
**Plan:** @docs/specs/applications/dia/systems/diaeditor/editor-memory.plan.md

## Summary

The editor invisibly remembers its state — layout, loaded plugins, and per-plugin data — and restores everything on open. No UI, no named profiles; it just works. Per-plugin state is project-scoped so switching between games (CluicheTest, CoW) shows the correct data per plugin without manual reconfiguration.

## Problem

The editor loses all state on close. Every launch requires manually re-opening plugins, re-arranging panels, and navigating back to where you were. Per-plugin data (last pipeline run, last open stage, connection target) is also lost or not project-aware, meaning context bleeds across projects.

## Goals

1. Eliminate "where was I?" friction on editor restart
2. Let plugins persist project-scoped state without each inventing their own persistence
3. Keep the mechanism invisible — no new UI concepts, no user configuration

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | On editor close, current layout tree and loaded plugin list are persisted to disk |
| AC2 | On editor open, layout and plugins are restored from the last session (no user action required) |
| AC3 | If a saved plugin is no longer registered (removed from build), it is skipped gracefully — no crash, no error dialog |
| AC4 | Per-plugin state (via `.context.json`) includes a `project` key so plugins can scope their memory per `.diagame` |
| AC5 | All memory files live under `out/CluicheEditor/` (gitignored, user-local) |

## Design

### Framework Memory (editor-global)

On close, the framework serializes to `out/CluicheEditor/.memory.json`:

```json
{
  "version": 1,
  "layout": { /* react-mosaic tree — same shape DockingLayout already serializes */ },
  "plugins": [
    { "type": "SceneEditorPlugin", "instance_id": "scene_main" },
    { "type": "EntityTemplateEditorPlugin", "instance_id": "bp_main" }
  ],
  "last_project": "C:/Games/CoW/cow.diagame"
}
```

On open: if `.memory.json` exists, `PluginLoaderModule` loads the listed plugins and applies the layout. Built-in plugins (Home, Output, PluginBrowser, GameConnection) are always loaded regardless of memory.

### Per-Plugin Memory (project-scoped)

Extends existing SED-021 `.context.json` sidecar. The framework passes the active project path to plugins via `EditorPluginContext`. Plugins that want project-scoped state key their `.context.json` entries by project:

```json
{
  "session_id": "2026-06-03T10:00:00Z",
  "project": "C:/Games/CoW/cow.diagame",
  "plugin_state": {
    "last_open_file": "scenes/forest.diastage",
    "last_pipeline_run": "2026-06-03T09:55:00Z"
  }
}
```

When the active project changes (via Project Context Bar), plugins receive `OnProjectChanged()` and can reload their scoped state.

### Restore Flow

```
Editor launch
  → PluginLoaderModule::DoStart()
    → LoadBuiltInPlugins() (always)
    → ReadMemoryFile("out/CluicheEditor/.memory.json")
    → For each plugin in memory.plugins:
        → If registered in EditorPluginRegistry: LoadPlugin(typeId, instanceId)
        → Else: skip (AC3)
    → ApplyLayout(memory.layout)
    → If memory.last_project exists: LoadProject(memory.last_project)
      → Plugins receive OnLoad + OnProjectChanged with project context
      → Plugins restore their .context.json state for this project
```

### Save Flow

```
Editor close (Application::RequestShutdown)
  → PluginLoaderModule::DoStop()
    → Collect loaded plugin list (excluding built-ins)
    → Serialize current layout via DockingLayout::Serialize()
    → Write .memory.json to out/CluicheEditor/
    → Each plugin's OnUnload() → plugin saves its own .context.json
```

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | Add `EditorMemory` class to DiaEditor — load/save `.memory.json` (layout tree + plugin list + last project) | S |
| 2 | Extend `PluginLoaderModule::DoStart` to call `EditorMemory::Load()` and restore plugins + layout from it | S |
| 3 | Extend `PluginLoaderModule::DoStop` to call `EditorMemory::Save()` with current state | S |
| 4 | Add `project` field to SED-021 `.context.json` schema; update `EditorPluginContext` to expose active project path | S |
| 5 | Add graceful skip for unregistered plugins during restore (log warning, continue) | S |
| 6 | Tests: unit test EditorMemory serialize/deserialize, test missing-plugin skip, test empty-memory first-launch | S |

## Binding Decisions

| Source | ID | Decision | How this feature complies |
|--------|----|----------|---------------------------|
| Platform | PD-004 | No STL containers in public APIs | `EditorMemory` public API uses `const char*` paths and DiaCore containers for plugin list |
| Platform | PD-009 | Generated output under `out/` | `.memory.json` stored at `out/CluicheEditor/.memory.json` |
| DiaEditor | SED-015 | DiaEditor is a pure library | `EditorMemory` is a plain class (no Module/Phase); `PluginLoaderModule` in CluicheEditor drives it |
| DiaEditor | SED-021 | Per-plugin `.context.json` sidecar | Per-plugin project-scoped state extends this existing mechanism; no new persistence path |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should `.memory.json` store window position/size? | Deferred — could add later as a field. Not needed for v1. |
| 2 | What happens on corrupt `.memory.json`? | Delete it and start fresh (built-ins only). Log a warning. |
| 3 | Should the framework auto-save periodically (not just on close)? | No for v1 — on-close is sufficient. Crash recovery is a future enhancement. |

# Research: Explore — Editor Workspace / Persona

**Session date:** 2026-06-03
**Folder:** docs/research/editor_workspac/

## Problem Space Overview

When developing with CluicheEditor, the user frequently context-switches between different workflows (engine development, game content editing, debugging) each requiring different plugin sets, layouts, open files, and connection targets. Currently the editor has:

- **Layout persistence** (`editor-layout.json`) — saves which panels are visible and their arrangement
- **Project file** (`.cluicheproj`) — references manifests and stores `editor_state` (layout, open files, last connection)
- **Per-plugin session context** (SED-021: `.context.json` sidecar + `.sessions/` archive)

What's missing is a cohesive mechanism that ties all these together into a switchable, restorable "workspace" concept. Today, if you switch from editing CoW to developing the editor itself, you manually close/open plugins, lose your place in files, and reconfigure connections. The per-plugin `.context.json` (SED-021) already anticipated "extensible to personas" — this research explores how to fulfil that.

The term "persona" here refers to a named configuration of editor state — not a user identity system. Think of it as a workspace profile.

## Existing Approaches

- **VS Code workspaces** — `.code-workspace` file stores folder list, settings, extensions. Separate per-workspace settings override user settings. Recently-used list for quick switch.
- **JetBrains projects** — `.idea/` directory stores layout, active tool windows, run configurations, VCS mapping. Multiple projects can be open simultaneously (separate windows).
- **Unity layouts** — Named layout presets saved/restored via menu. Per-project `Library/` folder caches editor state. Layout is separate from project settings.
- **Unreal Slate tabs** — Layout saved per user, not per project. Plugin sets fixed by project module dependencies.
- **Vim/Neovim sessions** — `:mksession` saves buffer list, window splits, cursor positions, registers to a `.vim` file. Restorable via `:source`.
- **tmux/screen** — Named sessions with window/pane layouts. Persist via plugins (tmux-resurrect). Session restore is separate from project identity.
- **Browser profiles (Chrome)** — Completely isolated state (extensions, bookmarks, history). Heavy — full process isolation.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Scope** | Per-project only / Per-user only / Per-project + per-user override | VS Code does per-project + per-user. Unity is per-project only. |
| **Multiplicity** | Single auto-save / Named profiles | Auto-save is cheapest. Named profiles add switch UI. |
| **Storage** | In `.cluicheproj` / Sidecar file / `out/` directory / User-global path | Version-control friendliness varies. |
| **Granularity** | Layout only / Layout + plugins / Layout + plugins + per-plugin state / Full (incl. open files, camera, connection) | Each level adds complexity but more complete restore. |
| **Switch mechanism** | Implicit (project open) / Explicit (UI dropdown) / CLI flag | Implicit is simplest UX. Explicit allows multiple per project. |
| **Plugin participation** | Framework handles all / Plugins opt-in to save/restore | SED-021 already has per-plugin `.context.json` — could extend that. |

## Known Tradeoffs

- **More state saved = better restore BUT harder to keep consistent** — if a saved workspace references a plugin that no longer exists, need graceful fallback
- **Per-project vs per-user** — Per-project is version-controllable (team shares layout); per-user avoids merge conflicts but is local-only
- **Named profiles add complexity** — Need UI for create/delete/rename/switch; need to decide when to auto-save vs manual save
- **Plugin state coupling** — If plugins store state in their own `.context.json`, workspace switch needs to coordinate with every plugin's save/restore cycle
- **Migration cost** — Any solution must not break existing `editor-layout.json` + `.cluicheproj` paths; needs to be additive

## Known Pitfalls (C++ / game engine context)

- **STL-free public API (PD-004)** — Workspace data structures exposed between DiaEditor and plugins must use Dia types, not std::map/vector
- **Singleton lifecycle** — EditorPluginRegistry is a singleton; workspace switch that unloads/reloads plugins must respect static-init ordering
- **CEF iframe state** — Panel iframes lose all in-page state when destroyed (SED-018). Workspace switch that destroys panels will lose unsaved JS-side state
- **File locks** — Switching workspace while a plugin has a file open (or a build running) can cause data loss
- **Manifest coupling** — `.diaapp` manifests declare plugins. If workspace A uses different manifests than workspace B, switch implies manifest load/unload — that's heavier than just layout swap
- **out/ directory growth** — Per-workspace snapshots in `out/CluicheEditor/` could accumulate without cleanup policy

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaEditor (EditorModel) | Already stores `editor_state` in `.cluicheproj` (layout, open files, last connection). Natural owner of workspace concept. |
| DiaEditor (DockingLayout) | Already serializes/deserializes panel tree to JSON. Layout is one axis of workspace. |
| DiaEditor (EditorPluginRegistry) | Knows all registered plugins. Workspace specifies which subset to load. |
| PluginLoaderModule | Already does `RestoreLayoutPlugins()` — reads saved layout, loads matching plugins. Could be extended to restore from workspace. |
| SED-021 `.context.json` | Per-plugin session state with archive mechanism. Already mentions "extensible to personas" — this is where plugin state would be coordinated. |
| EditorManifestLoader | Loads plugins from .diaapp. Workspace could reference different manifests. |
| DiaCLI | Could provide `dia editor switch-workspace <name>` for headless/scripted switching. |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Workspace IDs, profile names → should be StringCRC-indexed |
| PD-004 No STL in public APIs | Workspace data model exposed to plugins must use DiaCore containers |
| PD-009 Generated output under `out/` | Workspace state files that are generated (not source-controlled) belong in `out/CluicheEditor/` |
| SED-014 `.cluicheproj` is top-level | Workspace concept must interop with or extend `.cluicheproj`, not replace it |
| SED-015 DiaEditor is pure library | Workspace save/restore logic lives in DiaEditor as library code; CluicheEditor's Modules drive it |
| SED-021 `.context.json` sidecar | Already has the mechanism for per-plugin state archiving; workspace switch extends this |
| AED-002 Plugins in .diaapp manifests | Plugin sets tied to manifests — workspace that changes plugin sets implies manifest change or overlay |

## Open Questions for Ideation

- Should workspace switch unload/reload plugins, or just hide/show them (preserving their in-memory state)?
- Should workspaces be stored inside the project (version-controllable) or outside (user-local, no merge conflicts)?
- Is "workspace = named profile" the right abstraction, or is "workspace = project + implicit auto-save" sufficient?
- How does workspace interact with the live game connection? (Different projects connect to different targets.)
- Should plugins be notified of workspace switch (so they can save/restore their own state), or should the framework handle it opaquely?
- What's the minimum viable version that solves "reopen what I had last time" without the overhead of named profiles?

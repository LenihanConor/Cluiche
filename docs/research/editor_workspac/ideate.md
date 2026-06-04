# Research: Ideate — Editor Workspace / Persona

**Input:** docs/research/editor_workspac/explore.md

## Candidates

### Candidate 1: Auto-Save Session (No Named Profiles)

**Home module/system:** DiaEditor (EditorModel + DockingLayout)
**Size:** S (≤1 week)

**Description:** The editor automatically saves its full state on close and restores it on open. No UI, no named profiles — just "reopen what I had last time." State saved: loaded plugins, layout tree, open files, last connection info, per-plugin `.context.json`. Stored in `out/CluicheEditor/.session.json` (user-local, not version controlled).

On open: if `.session.json` exists and references the same `.cluicheproj`, restore from it. If the project file changed (e.g. manifests removed), fall back to manifest-declared plugins only. On close: serialize current state to `.session.json`.

**Primary value:** Eliminates the "where was I?" friction on editor restart with zero new UI or concepts.

---

### Candidate 2: Named Workspace Profiles (Project-Local)

**Home module/system:** DiaEditor (new `WorkspaceManager` class) + CluicheEditor UI
**Size:** M (1–3 weeks)

**Description:** Users can create, name, and switch between workspace profiles stored alongside the project. Each profile is a `.diaworkspace` JSON file in a `workspaces/` folder next to `.cluicheproj`. A profile stores: plugin set (type IDs), layout tree, per-plugin context snapshot, connection target, open files.

UI: dropdown in the toolbar (next to project name) listing available workspaces. "Save as..." creates a new profile from current state. Switch triggers: save current → unload extras → load target → restore layout → notify plugins.

`.cluicheproj` gains a `"last_workspace"` field for auto-open on launch.

**Primary value:** Enables "Editor Development" vs "Build CoW" context switching with one click. Profiles are version-controllable (team can share standard setups).

---

### Candidate 3: Per-Project Auto-Save + User-Global Named Presets

**Home module/system:** DiaEditor (EditorModel) + new `WorkspacePresetRegistry`
**Size:** M (1–3 weeks)

**Description:** Combines Candidate 1 (auto-save per project) with a separate concept of "presets" — user-global named layout+plugin templates stored in a user-local directory (e.g. `%APPDATA%/Cluiche/workspaces/`). Presets are project-independent templates. Applying a preset to a project overrides its current state.

Flow: Open project → auto-restores last session. Switch preset → applies template, but project manifests still determine available plugins (preset only loads plugins that are registered). Presets can be imported/exported as JSON files.

**Primary value:** "Develop editor" preset works across projects (it's not tied to CoW's `.cluicheproj`). Auto-save handles the common case; presets handle the power-user workflow switching.

---

### Candidate 4: Manifest Overlays (Plugin Set as First-Class Workspace)

**Home module/system:** DiaEditor (EditorManifestLoader) + CluicheEditor PluginLoaderModule
**Size:** M (1–3 weeks)

**Description:** Instead of a new workspace concept, treat plugin sets as the workspace dimension. A `.cluicheproj` can reference multiple `.diaapp` manifests and designate one as "active." Switching workspace = switching which manifest's plugins are loaded. Layout auto-saves per active manifest.

```json
{
  "manifests": [
    { "path": "config/editor-dev.diaapp", "label": "Editor Dev" },
    { "path": "config/cow-game.diaapp", "label": "Build CoW" }
  ],
  "active_manifest": "config/editor-dev.diaapp"
}
```

Each manifest already defines an `editor.plugins[]` section. Switching active manifest triggers: save current layout to `out/CluicheEditor/layouts/<manifest-slug>.json` → unload current plugins → load new manifest's plugins → restore layout for new manifest.

**Primary value:** Reuses the existing manifest system with minimal new concepts. Plugin sets ARE the workspace. No new file format needed.

---

### Candidate 5: Lightweight Plugin Pinning + Layout Snapshots

**Home module/system:** DiaEditor (DockingLayout) + PluginLoaderModule
**Size:** S (≤1 week)

**Description:** Adds two small features without introducing a "workspace" concept:

1. **Plugin pinning** — Mark plugins as "always loaded" vs "session-only." Pinned plugins survive across sessions. Session-only plugins are unloaded on close but remembered in layout (restored on next open).
2. **Layout snapshots** — Save/restore named layout arrangements. Just panel tree positions, not which plugins are loaded. Exposed as a menu item: "Layouts → Save Current / Restore → [list]."

Together these give: plugins you always want (Home, Output) stay pinned; session plugins (debugger, scene editor) come back from last session; layout is separately saveable.

**Primary value:** Two orthogonal, small features that compose to cover 80% of the workspace problem without the weight of a profile system.

---

### Candidate 6: Plugin Groups (Tagged Plugin Sets)

**Home module/system:** DiaEditor (EditorPluginRegistry) + CluicheEditor UI
**Size:** S–M (1–2 weeks)

**Description:** Plugins are tagged with groups (e.g. "debug", "content", "engine-dev"). Users can activate/deactivate groups from a toolbar menu. Group definitions live in a `plugin-groups.json` in the project. Each group specifies which plugin types to load and optionally a layout.

```json
{
  "groups": [
    { "name": "Editor Dev", "plugins": ["OutputConsole", "PluginBrowser", "BlueprintEditor"], "layout": "editor-dev.layout.json" },
    { "name": "CoW Build", "plugins": ["SceneEditor", "AssetBrowser", "GameConnection"], "layout": "cow.layout.json" }
  ]
}
```

Multiple groups can be active simultaneously (their plugin sets merge). Toggle a group on/off from the toolbar — additive mental model.

**Primary value:** Lighter than named workspaces but addresses the core "different plugin sets for different tasks" need. Additive activation avoids the "destroy everything, rebuild" switch cost.

---

### Candidate 7: Full Persona System (User Identity + Project + State)

**Home module/system:** New `DiaEditorPersona` subsystem within DiaEditor
**Size:** L (1–2 months)

**Description:** A full identity-based persona system where each persona is a combination of: user preferences (keybinds, theme), project binding (which `.cluicheproj`), workflow state (plugins, layout, connection, open files, per-plugin data), and display name/icon. Personas are stored user-globally with project overrides.

Includes: persona switcher UI, import/export, CLI integration (`dia editor --persona "Engine Dev"`), per-persona output isolation in `out/`, per-persona `.context.json` scoping (SED-021's "extensible to personas" vision fully realized).

**Primary value:** Maximum expressiveness — completely isolated editor "identities" that encapsulate everything. The "always right" setup regardless of what you're doing.

---

### Candidate 8: `.cluicheproj` Editor State Extension (Minimal Path)

**Home module/system:** DiaEditor (EditorModel)
**Size:** S (≤1 week)

**Description:** The `.cluicheproj` already has an `editor_state` section. Extend it to include: active plugin list (type IDs), full layout tree (not just a file reference), and recent-files list. Auto-save this section on editor close. On open, restore from it.

No named profiles. No new files. Just make the existing `editor_state` block actually drive the full restore. Currently `editor-layout.json` lives separately and the `.cluicheproj` barely uses its `editor_state` — unify them.

Adds a `.cluicheproj.user` sidecar (gitignored) for user-local state that shouldn't be version-controlled (window position, last connection).

**Primary value:** Minimal conceptual overhead — no new abstractions. The project file already claims to store this; just make it real.

## Coverage Map

The candidates span three scope levels:
- **S (minimal):** Candidates 1, 5, 8 — solve "restore last session" with no/minimal new concepts
- **M (workspace switching):** Candidates 2, 3, 4, 6 — address named/switchable configurations
- **L (full system):** Candidate 7 — maximum isolation and identity

They also vary on the key design axes:
- **Storage location:** project-local (2, 4, 6, 8), user-global (3, 7), gitignored out/ (1, 5)
- **Plugin participation:** framework-opaque (1, 5, 8) vs plugin-notified (2, 3, 7)
- **Switch mechanism:** implicit/auto (1, 8) vs explicit UI (2, 3, 4, 6, 7)

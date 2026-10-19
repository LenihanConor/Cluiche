# Feature Spec: Game-Scoped Editor Plugins

**Status:** Approved
**Application:** CluicheEditor
**System:** CluicheEditor Plugin Browser
**Origin:** 2026-10-19 world-sim product review. The CoW World Data Editor and World Sim Inspector are the first editor plugins that belong to one game.

## Parent Specs

- Platform: @docs/specs/platform/Cluiche.md
- Application: @docs/specs/applications/cluicheeditor/cluicheeditor.md
- System: @docs/specs/applications/cluicheeditor/systems/pluginbrowser/pluginbrowser.md

## Summary

Some editor plugins belong to one game. CoW's World Data Editor and World Sim Inspector make no sense in a CluicheTest workspace. This feature lets a game's `.diagame` name its own editor manifest. The plugins in that manifest are **game-scoped**: they exist in the editor only while that `.diagame` is loaded.

Generic Dia editors and inspectors (Scene Editor, Entity Inspector, Asset Catalogue Editor, Scalar Field Inspector and so on) are **not** game-scoped. They stay available in every game (user decision 2026-10-19).

## Problem

- `EditorPluginRegistry` already has a scope filter (`SetActiveManifests`, `IsInScopeFilter`; Plugin Browser Scaling, Done). It has three problems:
  - It is keyed to the `.cluicheproj` manifest list, not to the game being worked on.
  - Loading a `.diagame` never changes it.
  - It only hides plugins in the Plugin Browser (SCPB-002); they can still be created from a layout or memory file.
- The editor picks its `.diagame` in four ways: `--project=`, the restored `last_project`, File > Open, or a live game connection (`get_app_state` → `diagame_path`, which takes precedence). None of them changes which plugins are available.
- Nothing uses the filter yet.

## Goals

1. **A game owns its editor plugins.** `cow.diagame` names `cow_editor.diaapp`, and that manifest lists CoW's plugins.
2. **Fully unavailable outside their game.** A game-scoped plugin whose game isn't loaded is never created and doesn't appear in the Plugin Browser. Its layout and memory entries are skipped, it gets no `open_asset` routes, and it has no palette commands.
3. **Follows the loaded game.** Whenever a `.diagame` loads (by any of the four paths) or closes, the game scope is recomputed. Game-scoped plugins of the previous game close; plugins of the new game become available.
4. **Generic editors are untouched.** Built-in plugins, plugins compiled from Dia modules and plugins from `.cluicheproj` manifests behave exactly as today.

## Non-Goals

- **DLL plugins or hot-loading.** Plugins stay compiled into `CluicheEditor.exe`; scoping is a runtime decision.
- **Scoping by stage or testbed.** The scope is the game, not which stage is running.
- **Changing `.cluicheproj` manifest scoping.** That stays display-only (SCPB-002).
- **A new manifest format.** The existing `.diaapp` `"editor"` section is reused (`EditorManifestLoader`).

## Design

### Data

```json
// Cluiche/Assets/CoW/cow.diagame
{
  "...": "...",
  "editor": { "manifests": ["cow_editor.diaapp"] }
}

// Cluiche/Assets/CoW/cow_editor.diaapp  (existing EditorManifestLoader format)
{
  "editor": {
    "enabled": true,
    "plugins": [
      { "type": "CoWWorldDataEditor" },
      { "type": "CoWWorldSimInspector" }
    ]
  }
}
```

- Manifest paths are relative to the `.diagame`.
- A `.diagame` without `"editor"` has no game-scoped plugins.

### Scope rules

Every registry entry already has a `manifestId`; empty means built-in. This feature adds an origin to each entry:

| Origin | Example | Available |
|---|---|---|
| Built-in / Dia module (no manifest) | Home, Scene Editor, Scalar Field Inspector | Always |
| `.cluicheproj` manifest | today's project manifests | As today. SCPB-002 still applies: the Plugin Browser hides it, nothing else changes |
| `.diagame` manifest (**game-scoped**) | `CoWWorldSimInspector` | Only while that `.diagame` is loaded |

- **One registration path.** A plugin type listed in any `.diagame` manifest is game-scoped. Its factory still self-registers (`REGISTER_EDITOR_PLUGIN`), and the registry marks it game-scoped when the manifest is read. No code macro names a game; the data decides.
- **Both origins.** If a type appears in both a `.cluicheproj` manifest and a `.diagame` manifest, the `.diagame` wins: it is game-scoped.

### Runtime

- **When a `.diagame` loads:** `EditorModel::LoadDiagameProject` reads `editor.manifests` and calls `EditorPluginRegistry::SetGameScope(gameId, manifests)`, which marks those plugin types game-scoped for that game.
- **When the game changes or closes:** the registry raises `GameScopeChanged`. `PluginLoaderModule` then:
  1. Closes any open game-scoped plugin that is now out of scope (`OnUnload` is called and its layout slot is kept as empty).
  2. Logs `[EDITOR] INFO closing <type> (out of scope: <game>)`.
- **Creating a plugin:** `PluginLoaderModule` refuses to create an out-of-scope game-scoped plugin from a layout, `.memory.json` or the palette. It logs one Info line and skips the entry.
- **`open_asset`:** game-scoped plugins register their asset-type routes in `OnLoad` and remove them in `OnUnload`. So `open_asset lush.ecology.json` routes to the CoW World Data Editor only while CoW is loaded; otherwise it falls back to the OS default app.
- **Plugin Browser:** the C++-side list (SCPB-001) leaves out out-of-scope game-scoped plugins entirely.
- **Live connection:** when a game connects and reports its `diagame_path`, that path loads through the same `LoadDiagameProject` call, so connecting a running CoW makes CoW's plugins available.

### Where CoW plugins live

- **Code:** `Cluiche/CoW/Editor/` as a `CoWEditor` static library, using the same pattern as Dia editor modules.
  - `CluicheEditor.vcxproj` references it, so it is compiled in.
  - It is available only when CoW is loaded, because of this feature.
  - CoW runtime code never depends on `CoWEditor`.
- **UI:** `Cluiche/CoW/Editor/<Plugin>/UI/`.

**Mockup:** [mockups/game-scoped-plugins.html](mockups/game-scoped-plugins.html)

## Acceptance Criteria

| # | Criterion |
|---|---|
| AC1 | With `cow.diagame` loaded, `CoWWorldSimInspector` is listed in the Plugin Browser and can be opened |
| AC2 | With `cluichetest.diagame` loaded or no game loaded, CoW plugins are not listed, not openable from the palette, and a saved layout naming one opens without it (one Info log line) |
| AC3 | Switching from CoW to CluicheTest closes open CoW plugins and logs each closure |
| AC4 | `open_asset` on a CoW asset type routes to the CoW editor only while CoW is loaded |
| AC5 | Connecting a running CoW game (live `diagame_path`) makes CoW plugins available without a restart |
| AC6 | Generic Dia plugins (e.g. Scene Editor, Asset Catalogue Editor) are available in every case above |
| AC7 | A `.diagame` with no `"editor"` key behaves exactly as today |
| AC8 | Switching game with a dirty game-scoped plugin prompts Save / Discard / Cancel; Cancel keeps the current game |
| AC9 | A live connection from a different game asks before switching; declining keeps the loaded game and its plugins |

## Test Strategy

- **GoogleTests on `EditorPluginRegistry`:**
  - Set the game scope and query which plugins are in scope for each origin.
  - Switch games and check the scope changes.
  - A type listed in both a `.cluicheproj` and a `.diagame` manifest resolves as game-scoped.
  - With an empty game scope, nothing that is game-scoped is in scope.
- **Integration (editor E2E):** load CoW, then CluicheTest, then CoW, and check AC1–AC4 and AC6 with a stub game-scoped test plugin. Real CoW plugins are not needed.

## Binding Decisions

| Source | Decision | Constraint |
|---|---|---|
| PD-010 | `.diagame` is the root; typed imports | The game's editor manifest is named from the `.diagame` |
| AED-002 | Plugins are specified in a `.diaapp` `"editor"` section | Reused unchanged for `cow_editor.diaapp` |
| AED-006 | `.cluicheproj` is the top-level project file | Unchanged. The game scope is added on top; it doesn't replace the project manifests |
| SCPB-001 | Scope filtering happens in C++ | Out-of-scope game plugins are removed C++-side |
| SCPB-002 | The filter is display-only | **Narrowed** (2026-10-19): it still holds for `.cluicheproj` manifests. Game-scoped plugins are fully unavailable out of scope, by user decision |
| CoW AD-001 | Ask before editing CluicheEditor / Dia | Building this changes `Dia/DiaEditor` and `CluicheEditor`, so it needs sign-off before implementation |

## Resolved Decisions

1. **Unsaved edits on a game switch: prompt** (2026-10-19). If any game-scoped plugin is dirty, the switch shows Save / Discard / Cancel before closing it. Cancel keeps the current game loaded. This uses the existing dirty-state pattern if `IEditorPlugin` has one; otherwise the feature adds `IsDirty()` / `Save()` to the plugin interface.
2. **A different game connecting: ask before switching** (2026-10-19). When a live connection reports a `diagame_path` different from the loaded one, the editor asks "Switch to <game>?" instead of switching straight away (this narrows "takes precedence" in `GameConnectionController`). Declining keeps the current game; the connection stays up but game-scoped plugins don't change. If the same game connects, nothing is asked.

## Status

`Approved` (2026-10-19). Build needs AD-001 sign-off for the `Dia/DiaEditor` and `CluicheEditor` changes.

# Feature Spec: Plugin Lifecycle Toolbar

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Plugin Lifecycle Toolbar** | (this document) |

## Problem Statement

Users have no way to discover, load, unload, or toggle visibility of editor plugins at runtime. The panel set is fixed at startup, and closing a panel from the mosaic layout provides no way to reopen it. This feature adds a persistent toolbar strip, a Plugin Browser panel, and full runtime plugin lifecycle management.

## Acceptance Criteria

- [ ] A persistent toolbar strip is always visible (top or bottom of the editor window)
- [ ] Each loaded plugin has a corresponding icon/button in the toolbar
- [ ] Clicking a toolbar button toggles that plugin's panel visibility in the mosaic layout (show/hide)
- [ ] A Plugin Browser panel lists all registered plugin types with name, version, description, and loaded/available status
- [ ] Users can load an available (unloaded) plugin from the Plugin Browser — it appears in the mosaic layout and toolbar
- [ ] Users can unload a non-pinned plugin from the Plugin Browser — it is removed from the layout and toolbar
- [ ] Pinned plugins (Output Console, Plugin Browser) can be hidden via toolbar but never unloaded
- [ ] The Plugin Browser always has a dedicated toolbar icon so it can be reopened after being hidden
- [ ] Newly loaded plugins auto-split into the mosaic layout at a reasonable position
- [ ] The mosaic X button on panel tabs hides the panel (minimizes to toolbar), not unloads it
- [ ] A game connection status indicator is visible in the toolbar (green/red dot)
- [ ] The toolbar is extensible — plugins can contribute toolbar items via a well-defined interface
- [ ] When a panel is hidden, its toolbar icon appears visually distinct (dimmed/outlined) from visible panels
- [ ] C++ notifies the React frontend when the panel list changes (load/unload) so the UI updates dynamically

## Design

### Toolbar Architecture

The toolbar is a React component rendered outside the mosaic area — a thin horizontal strip at the bottom of the editor. It has three zones:

```
┌──────────────────────────────────────────────────────────────────┐
│                        Mosaic Layout                             │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
┌──────────────────────────────────────────────────────────────────┐
│ [Plugin Icons...] │ [Spacer] │ [Status Indicators] │
│ 🏠 📋 🔌 🎮 ...  │          │  ● Connected         │
└──────────────────────────────────────────────────────────────────┘
```

Each toolbar button shows `iconChar` (first letter of plugin name by default) and displays the full plugin name as a tooltip on mouse hover.

- **Left zone**: One button per loaded plugin, toggles panel visibility
- **Center**: Flexible spacer (future: layout preset selector, command palette trigger)
- **Right zone**: Status indicators (game connection state)

### Toolbar Item Interface (C++ Side)

Plugins contribute toolbar items via an extension to `IEditorPlugin`:

```cpp
// Dia/DiaEditor/Plugin/IEditorPlugin.h
struct EditorToolbarItem
{
    char label[32];       // Short label (used as tooltip)
    char iconChar[4];     // Single UTF-8 character or short symbol for the button
    bool pinned;          // If true, plugin cannot be unloaded (only hidden)
};

class IEditorPlugin
{
public:
    // Existing methods...
    
    // Toolbar integration — default returns item based on GetName()
    virtual EditorToolbarItem GetToolbarItem() const;
};
```

Default implementation builds a toolbar item from `GetName()` with `pinned = false`. Plugins override to customize icon or set `pinned = true`.

### Panel Visibility vs Plugin Lifecycle

Two distinct states:
- **Loaded/Unloaded**: Whether the plugin instance exists (C++ object alive, OnLoad called, receiving OnUpdate)
- **Visible/Hidden**: Whether the plugin's panel is shown in the mosaic layout

State transitions:
```
                  Load (from Plugin Browser)
   [Available] ─────────────────────────────> [Loaded + Visible]
                                                    │
                          Toolbar toggle / X button  │
                                                    ▼
                                              [Loaded + Hidden]
                                                    │
                          Toolbar toggle             │
                                                    ▼
                                              [Loaded + Visible]

                  Unload (from Plugin Browser, non-pinned only)
   [Loaded] ────────────────────────────────> [Available]
```

### Notification System (C++ → React)

When plugins are loaded/unloaded or visibility changes, C++ pushes updates to the React shell:

```cpp
// EditorView or PluginLoaderModule notifies after load/unload:
mWebUIBridge->NotifyUIDataChanged("panels_changed", buildPanelListJson());
```

The React `DockingManager` subscribes to the `"panels_changed"` topic and re-renders the toolbar and mosaic layout accordingly.

### DockingLayout Changes

`DockingLayout` gains panel removal and visibility toggling:

```cpp
// Dia/DiaEditor/Layout/DockingLayout.h
void RemovePanel(const char* name);
void SetPanelVisible(const char* name, bool visible);
bool IsPanelVisible(const char* name) const;
```

### PluginLoaderModule Changes

`PluginLoaderModule` gains plugin unloading via the `IPluginLoader` interface:

```cpp
// Dia/DiaEditor/Plugin/IPluginLoader.h
class IPluginLoader
{
public:
    virtual void LoadPlugin(const StringCRC& typeId, const StringCRC& instanceId) = 0;
    virtual void UnloadPlugin(const StringCRC& typeId) = 0;
    virtual bool IsPluginTypeLoaded(const StringCRC& typeId) const = 0;
    virtual bool IsPluginPinned(const StringCRC& typeId) const = 0;
};
```

Pinned plugins (Console, Plugin Browser) are tracked in the loader and refuse unload requests.

### Plugin Browser Enhancements

The Plugin Browser panel adds:
- Unload button per plugin (disabled for pinned plugins, shows lock icon)
- Pinned badge on pinned plugins
- `plugin_browser.unload` request handler

### React Toolbar Component

```
Cluiche/CluicheEditor/UI/src/layout/Toolbar.tsx
```

New React component:
- Subscribes to `"panels_changed"` topic via `EditorBridge.subscribe()`
- Renders a button per loaded plugin (icon + tooltip)
- Active (visible) buttons have full opacity; hidden panels are dimmed
- Click sends `"toggle_panel_visibility"` event to C++
- Right zone shows connection status via `"game_connection"` topic subscription

### Game Connection Status Indicator

The toolbar's right zone subscribes to the existing `"game_connection"` topic:
- Green dot + "Connected" when game connection is active
- Red dot + "Disconnected" when not connected
- No new C++ code needed — reuses existing `GameConnectionController` data pushes

### Request Handlers

| Handler | Type | Description |
|---------|------|-------------|
| `plugin_browser.get_available` | Request | Returns all registered plugins with loaded/pinned status |
| `plugin_browser.load` | Request | Loads a plugin by typeId |
| `plugin_browser.unload` | Request | Unloads a non-pinned plugin by typeId |
| `toggle_panel_visibility` | Event | Toggles a loaded plugin's panel visibility |
| `panels_changed` | Topic (push) | C++ → JS notification when panel list changes |

## Implementation Files

### New Files
- `Dia/DiaEditor/Plugin/IPluginLoader.h` — Abstract plugin loader interface
- `Dia/DiaEditor/Plugin/PluginBrowserEditorPlugin.h/cpp` — Plugin Browser plugin
- `Dia/DiaEditor/Plugin/Assets/pluginbrowser/index.html` — Plugin Browser frontend
- `Cluiche/CluicheEditor/UI/src/layout/Toolbar.tsx` — React toolbar component

### Modified Files
- `Dia/DiaEditor/Plugin/IEditorPlugin.h` — Add `EditorPluginInfo`, `EditorToolbarItem`, extend `IEditorPluginFactory`
- `Dia/DiaEditor/Plugin/EditorPluginRegistrationMacros.h` — Implement `GetPluginInfo()` in macro
- `Dia/DiaEditor/Plugin/EditorPluginRegistry.h/cpp` — Add `GetFactory()` accessor
- `Dia/DiaEditor/Plugin/EditorPluginContext.h` — Add `IPluginLoader*`
- `Dia/DiaEditor/Layout/DockingLayout.h/cpp` — Add `RemovePanel()`, `SetPanelVisible()`, `IsPanelVisible()`
- `Dia/DiaEditor/MVC/EditorView.h/cpp` — Add `UnregisterComponent()`, panel change notification, visibility toggle handler
- `Cluiche/CluicheEditor/ApplicationFlow/Modules/PluginLoaderModule.h/cpp` — Implement `IPluginLoader` with unload + pinning
- `Cluiche/CluicheEditor/UI/src/layout/DockingManager.tsx` — Subscribe to `panels_changed`, integrate toolbar
- `Cluiche/CluicheEditor/UI/src/bridge/EditorBridge.ts` — Add visibility toggle API
- `Dia/DiaEditor/DiaEditor.vcxproj` + `.filters` — Add new headers
- `Cluiche/CluicheEditor/CluicheEditor.vcxproj` + `.filters` — Add new .cpp

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for all IDs | **Compliant** — Plugin type IDs, instance IDs, toolbar item IDs all use StringCRC |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — PluginLoaderModule is a Module within CluicheEditorProcessingUnit; DiaEditor library classes have no Module dependency (AED-001) |
| Platform | PD-003 | Component-based entities | **N/A** — Toolbar and plugin lifecycle are not entity-related |
| Platform | PD-004 | No STL in public APIs | **Compliant** — All public interfaces use `const char*`, `StringCRC`, `DynamicArrayC`; `std::string` used only in .cpp internals |
| Platform | PD-005 | x64 only | **Compliant** — No 32-bit considerations |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — New files added to `.vcxproj` and `.vcxproj.filters` |
| Platform | PD-007 | C++20 required | **Compliant** — No C++20-specific features needed but compiles under `/std:c++20` |
| Platform | PD-008 | Directory.Build.props owns build settings | **Compliant** — No per-project overrides added |
| Application | AED-001 | CluicheEditor owns app flow; DiaEditor is pure library | **Compliant** — `IPluginLoader` interface lives in DiaEditor; `PluginLoaderModule` implementation lives in CluicheEditor. Toolbar is React (shell), not library code |
| Application | AED-002 | Plugins from .diaapp manifest | **Compliant** — Manifest-loaded plugins appear in Plugin Browser alongside built-ins; no manifest changes required |
| Application | AED-003 | Each system owns its editor subdirectory | **Compliant** — PluginBrowserEditorPlugin lives in `Dia/DiaEditor/Plugin/` (DiaEditor owns it) |
| Application | AED-004 | WebSocket network-first connection | **N/A** — Connection status indicator reuses existing GameConnectionManager; no new networking |
| Application | AED-005 | React + CEF + react-mosaic | **Compliant** — Toolbar is a React component; mosaic integration for panel visibility; runs in CEF |
| Application | AED-006 | .cluicheproj is top-level project file | **N/A** — No project file changes |
| System | SED-001 | IEditorPlugin interface minimal and stable | **Compliant** — Only adds optional `GetToolbarItem()` with a default implementation; existing plugins unaffected |
| System | SED-002 | Plugins register via macro | **Compliant** — PluginBrowserEditorPlugin uses REGISTER_EDITOR_PLUGIN macro; `GetPluginInfo()` added to macro-generated factory |
| System | SED-003 | Systems own their editors | **Compliant** — Plugin Browser is a DiaEditor-owned built-in, not a system plugin |
| System | SED-004 | WebSocket JSON protocol | **N/A** — No new wire protocol; connection indicator reads existing state |
| System | SED-005 | CEF replaces Awesomium | **Compliant** — All UI in CEF/React |
| System | SED-006 | Docking managed by JS (react-mosaic) | **Compliant** — Toolbar is JS; panel visibility toggling updates mosaic layout from JS side |
| System | SED-007 | CommandDispatcher embeds Python | **N/A** — No command dispatch changes |
| System | SED-008 | Observer pattern (not polling) | **Compliant** — `panels_changed` topic push follows observer pattern; no polling for panel state |
| System | SED-009 | Undo/redo via IEditorCommand | **N/A** — Load/unload/visibility are not undoable operations |
| System | SED-010 | DiaDebugProtocol for wire types | **N/A** — No editor-game protocol changes |
| System | SED-011 | Two-tier observer paths | **N/A** — Plugin Browser uses request/response, not observer paths |
| System | SED-012 | StringCRC constants in DataPath | **Compliant** — Request handler IDs are static `StringCRC` constants |
| System | SED-013 | Plugin data structs kPluginDataTypeId | **N/A** — Plugin Browser stores no custom data in EditorModel |
| System | SED-014 | .cluicheproj references .diaapp | **N/A** — No project file changes |
| System | SED-015 | DiaEditor is pure library, no DiaApplication dependency | **Compliant** — `IPluginLoader` is a pure interface in DiaEditor; `PluginLoaderModule` (DiaApplication subclass) is in CluicheEditor |
| System | SED-016 | GameConnectionManager boots clean, Connect() explicit | **Compliant** — Connection indicator is read-only; doesn't affect connection lifecycle |
| System | SED-017 | EditorManifestLoader static utility with callback | **N/A** — No manifest loading changes |

**All binding decisions: COMPLIANT — no conflicts.**

## Open Questions

| # | Question | Status | Resolution |
|---|----------|--------|------------|
| 1 | Where does the toolbar render — top or bottom? | Resolved | Bottom of editor, below the mosaic area |
| 2 | What happens when mosaic X is clicked? | Resolved | Hides panel to toolbar (not unload); re-toggle from toolbar |
| 3 | Which plugins are pinned? | Resolved | Output Console and Plugin Browser are hardcoded pinned; all others can be unloaded |
| 4 | How does a newly loaded plugin find a position in the mosaic? | Resolved | Auto-split the last/rightmost leaf in the mosaic tree |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Toolbar | Should toolbar position (top/bottom) be configurable? | No — bottom is standard; keep it simple | Bottom only, not configurable |
| 2 | Toolbar Icons | How are plugin icons determined? Plugins currently have no icon metadata. | Use first letter of plugin name as icon; add `iconChar` field to `EditorToolbarItem` for override | `EditorToolbarItem::iconChar` — first letter default, plugins can override. Mouse hover shows full plugin name as tooltip |
| 3 | Unload Safety | What if a plugin has unsaved state when unloaded? | Call `OnUnload()` which is the plugin's chance to clean up; no save prompt for v1 | Call `OnUnload()` — plugin is responsible for its own cleanup |
| 4 | Toolbar Persistence | Should which plugins are hidden/visible persist across editor restarts? | Yes — save visibility state alongside layout in `editor-layout.json` | Yes — visibility state saved in layout persistence |
| 5 | Max Toolbar Items | What if many plugins are loaded and toolbar overflows? | Overflow into a "more" dropdown | Overflow dropdown for v1; revisit if toolbar gets crowded |
| 6 | Connection Indicator | Should clicking the connection indicator open the Game Connection panel? | Yes — natural shortcut | Yes — toggles Game Connection panel visibility |
| 7 | Plugin Load Order | When loading at runtime, should the new panel get focus? | Yes — user explicitly requested it | Yes — newly loaded panel gets focus in mosaic |
| 8 | Extensibility Scope | For v1, should third-party toolbar items (beyond panel toggles) be supported? | No — design the interface but only implement panel toggles and connection indicator | Interface designed, only panel toggles + connection status implemented |

## Status

`Approved`

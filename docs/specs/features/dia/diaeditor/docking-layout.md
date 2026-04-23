# Feature Spec: Docking Layout

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Docking Layout** | (this document) |

## Problem Statement

Manages flexible workspace layout using react-mosaic docking library, allowing users to arrange panels (plugin views, console, command palette) via drag-and-drop, split horizontally/vertically, and persist their layout to disk for restoration on next startup.

## Acceptance Criteria

- [ ] Integrate react-mosaic JavaScript library
- [ ] Default IDE-style layout (sidebar + main + bottom panel)
- [ ] Automatic plugin panel mounting via IEditorPlugin::GetUIPath()
- [ ] Drag-and-drop panel rearrangement
- [ ] Horizontal/vertical panel splitting
- [ ] Tab groups for multiple panels in same area
- [ ] Persist layout to `~/.cluiche/editor-layout.json`
- [ ] Load layout on editor startup
- [ ] Ignore missing panels gracefully (if plugin removed)
- [ ] Reset to default layout option

## Design

### react-mosaic Integration

**Package Installation:**
```json
// Cluiche/CluicheEditor/UI/package.json
{
  "dependencies": {
    "react": "^18.2.0",
    "react-dom": "^18.2.0",
    "react-mosaic-component": "^6.1.0",
    "typescript": "^5.0.0"
  }
}
```

**DockingManager Component:**
```typescript
// Cluiche/CluicheEditor/UI/src/layout/DockingManager.tsx
import React, { useState, useEffect } from 'react';
import { Mosaic, MosaicWindow, MosaicNode } from 'react-mosaic-component';
import 'react-mosaic-component/react-mosaic-component.css';

type PanelId = string;

interface PanelConfig {
    id: PanelId;
    title: string;
    url: string;  // dia:// URL or iframe src
    closable: boolean;
}

export const DockingManager: React.FC = () => {
    const [layout, setLayout] = useState<MosaicNode<PanelId> | null>(null);
    const [panels, setPanels] = useState<Map<PanelId, PanelConfig>>(new Map());
    
    useEffect(() => {
        // Load layout from C++ on mount
        window.CluicheEditor.loadLayout().then((savedLayout) => {
            if (savedLayout) {
                setLayout(savedLayout);
            } else {
                // Use default layout (Decision 25: IDE-style)
                setLayout(getDefaultLayout());
            }
        });
        
        // Load registered panels
        window.CluicheEditor.getPanels().then((panelList) => {
            const panelMap = new Map();
            panelList.forEach((panel: PanelConfig) => {
                panelMap.set(panel.id, panel);
            });
            setPanels(panelMap);
        });
    }, []);
    
    const handleLayoutChange = (newLayout: MosaicNode<PanelId> | null) => {
        setLayout(newLayout);
        // Save to C++ (persists to ~/.cluiche/editor-layout.json)
        window.CluicheEditor.saveLayout(newLayout);
    };
    
    const renderPanel = (panelId: PanelId) => {
        const panel = panels.get(panelId);
        if (!panel) {
            // Decision 27: Ignore missing panels gracefully
            return <div>Panel "{panelId}" not found</div>;
        }
        
        return (
            <MosaicWindow<PanelId>
                path={[]}
                title={panel.title}
                createNode={() => panelId}
                toolbarControls={panel.closable ? undefined : []}
            >
                <iframe
                    src={panel.url}
                    style={{ width: '100%', height: '100%', border: 'none' }}
                    title={panel.title}
                />
            </MosaicWindow>
        );
    };
    
    return (
        <Mosaic<PanelId>
            renderTile={renderPanel}
            value={layout}
            onChange={handleLayoutChange}
            className="mosaic-blueprint-theme"
        />
    );
};

// Decision 25: IDE-style default layout
function getDefaultLayout(): MosaicNode<PanelId> {
    return {
        direction: 'row',
        first: 'navigation',  // Left sidebar (20% width)
        second: {
            direction: 'column',
            first: 'plugin_main',  // Main editor area (70% height)
            second: 'output_console',  // Bottom console (30% height)
            splitPercentage: 70
        },
        splitPercentage: 20
    };
}
```

### Automatic Plugin Panel Mounting

**EditorApplication (C++ Side):**
```cpp
void EditorApplication::LoadPlugin(const StringCRC& pluginTypeId) {
    IEditorPlugin* plugin = EditorPluginRegistry::Instance().CreatePlugin(pluginTypeId);
    
    if (!plugin) {
        DIA_LOG_ERROR("Plugin not found: %s", pluginTypeId.GetString());
        return;
    }
    
    // Decision 26: Automatic mounting via GetUIPath()
    const char* uiPath = plugin->GetUIPath();
    const char* name = plugin->GetName();
    LayoutMode layoutMode = plugin->GetLayoutMode();
    
    // Register panel with docking system
    Json::Value panelConfig;
    panelConfig["id"] = name;
    panelConfig["title"] = name;
    panelConfig["url"] = uiPath;  // e.g., "dia://editor/diaapplicationeditor/index.html"
    panelConfig["closable"] = (layoutMode == LayoutMode::kDockable);
    
    // Send to UI via JavaScript bridge
    CallJavaScript("registerPanel", panelConfig);
    
    // Initialize plugin
    plugin->OnLoad(mModel);
    mLoadedPlugins.Add(plugin);
    
    DIA_LOG("Loaded plugin: %s", name);
}
```

**JavaScript Bridge Handler:**
```typescript
// Cluiche/CluicheEditor/UI/src/bridge/EditorBridge.ts
window.CluicheEditor = {
    registerPanel: (config: PanelConfig) => {
        // Called from C++ when plugin loads
        // Add panel to DockingManager state
        dispatchEvent(new CustomEvent('panel-registered', { detail: config }));
    },
    
    loadLayout: async (): Promise<MosaicNode<string> | null> => {
        // Request saved layout from C++
        return new Promise((resolve) => {
            window.cefQuery({
                request: JSON.stringify({ type: 'load_layout' }),
                onSuccess: (response: string) => {
                    const layout = JSON.parse(response);
                    resolve(layout);
                },
                onFailure: () => resolve(null)
            });
        });
    },
    
    saveLayout: (layout: MosaicNode<string>) => {
        // Save layout to C++
        window.cefQuery({
            request: JSON.stringify({
                type: 'save_layout',
                layout: layout
            }),
            onSuccess: () => console.log('Layout saved'),
            onFailure: (err: string) => console.error('Layout save failed:', err)
        });
    },
    
    getPanels: async (): Promise<PanelConfig[]> => {
        // Get list of registered panels from C++
        return new Promise((resolve) => {
            window.cefQuery({
                request: JSON.stringify({ type: 'get_panels' }),
                onSuccess: (response: string) => {
                    const panels = JSON.parse(response);
                    resolve(panels);
                }
            });
        });
    }
};
```

### Layout Persistence

**DockingLayout Class (C++ Side):**
```cpp
namespace Dia::Editor {
    class DockingLayout {
    public:
        DockingLayout();
        
        // Load/save layout to ~/.cluiche/editor-layout.json (Decision 24)
        bool LoadFromDisk();
        bool SaveToDisk();
        
        // Serialization
        Json::Value Serialize() const;
        void Deserialize(const Json::Value& layout);
        
        // Panel registry
        void RegisterPanel(const char* panelId, const char* title, const char* url, bool closable);
        void UnregisterPanel(const char* panelId);
        Json::Value GetPanels() const;
        
        // Layout access
        Json::Value GetCurrentLayout() const { return mCurrentLayout; }
        void SetCurrentLayout(const Json::Value& layout);
        
    private:
        Json::Value mCurrentLayout;
        
        struct PanelInfo {
            StringCRC id;
            char title[64];
            char url[256];
            bool closable;
        };
        DynamicArrayC<PanelInfo, 32> mRegisteredPanels;
        
        Dia::Core::FilePath GetLayoutFilePath() const;  // ~/.cluiche/editor-layout.json
    };
}
```

**LoadFromDisk:**
```cpp
bool DockingLayout::LoadFromDisk() {
    Dia::Core::FilePath layoutPath = GetLayoutFilePath();  // ~/.cluiche/editor-layout.json
    
    if (!FileExists(layoutPath.AsCString())) {
        DIA_LOG("No saved layout found, using default");
        return false;
    }
    
    const char* layoutJson = ReadFileContents(layoutPath.AsCString());
    
    Json::Value layout;
    Json::Reader reader;
    if (!reader.parse(layoutJson, layout)) {
        DIA_LOG_ERROR("Failed to parse layout JSON");
        return false;
    }
    
    // Decision 27: Ignore missing panels gracefully
    // Validate that referenced panels exist, but don't fail if some missing
    ValidateLayout(layout);
    
    mCurrentLayout = layout;
    DIA_LOG("Loaded layout from %s", layoutPath.AsCString());
    return true;
}

void DockingLayout::ValidateLayout(Json::Value& layout) {
    // Recursively walk layout tree, remove references to non-existent panels
    if (layout.isString()) {
        StringCRC panelId(layout.asCString());
        bool exists = false;
        for (int i = 0; i < mRegisteredPanels.Size(); ++i) {
            if (mRegisteredPanels[i].id == panelId) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            DIA_LOG_WARNING("Panel '%s' not found, will be ignored", layout.asCString());
        }
    } else if (layout.isObject()) {
        if (layout.isMember("first")) {
            ValidateLayout(layout["first"]);
        }
        if (layout.isMember("second")) {
            ValidateLayout(layout["second"]);
        }
    }
}
```

**SaveToDisk:**
```cpp
bool DockingLayout::SaveToDisk() {
    Dia::Core::FilePath layoutPath = GetLayoutFilePath();
    
    Json::StyledWriter writer;
    const char* layoutJson = writer.write(mCurrentLayout).c_str();
    
    bool success = WriteFileContents(layoutPath.AsCString(), layoutJson);
    
    if (success) {
        DIA_LOG("Saved layout to %s", layoutPath.AsCString());
    } else {
        DIA_LOG_ERROR("Failed to save layout to %s", layoutPath.AsCString());
    }
    
    return success;
}
```

### Reset to Default Layout

**EditorApplication::ResetLayout:**
```cpp
void EditorApplication::ResetLayout() {
    DIA_LOG("Resetting layout to default...");
    
    // Clear current layout
    mDockingLayout->SetCurrentLayout(Json::Value::null);
    
    // Tell UI to use default
    CallJavaScript("resetToDefaultLayout", Json::Value());
    
    // Delete saved layout file
    Dia::Core::FilePath layoutPath = mDockingLayout->GetLayoutFilePath();
    if (FileExists(layoutPath.AsCString())) {
        DeleteFile(layoutPath.AsCString());
    }
}
```

## Implementation Files

- `Dia/DiaEditor/Layout/DockingLayout.h/cpp` - C++ layout persistence
- `Cluiche/CluicheEditor/UI/src/layout/DockingManager.tsx` - React docking component
- `Cluiche/CluicheEditor/UI/src/bridge/EditorBridge.ts` - JavaScript ↔ C++ bridge
- `Cluiche/CluicheEditor/UI/package.json` - react-mosaic dependency

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — panel IDs use StringCRC in PanelInfo |
| Platform | PD-002 | PU/Phase/Module architecture | **N/A** — DockingLayout is a utility class, not a Module |
| Platform | PD-004 | No STL in public APIs | **Compliant** — uses `const char*`, DynamicArrayC, Dia::Core::FilePath; no std::string/std::vector |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — built within DiaEditor .vcxproj |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — uses `Dia::Editor::` namespace |
| DiaEditor | SED-002 | Plugins register via macro | **Compliant** — automatic panel mounting via IEditorPlugin::GetUIPath() |
| DiaEditor | SED-005 | CEF replaces Awesomium | **Compliant** — react-mosaic runs in CEF browser |
| DiaEditor | SED-006 | Docking managed by JavaScript library | **Compliant** — react-mosaic handles drag-and-drop, splitting |
| DiaEditor | SED-008 | Observer pattern (not polling) | **N/A** — layout changes driven by UI events, not model observation |

**All binding decisions: COMPLIANT**

## Open Questions

**Resolved:**
- **Decision 23:** Use react-mosaic library (modern, React-native, no jQuery)
- **Decision 24:** Persist layout to user home directory (`~/.cluiche/editor-layout.json`)
  - **NOTE:** Need better data management strategy across all editor data (config, cache, logs, layouts)
- **Decision 25:** Default IDE-style layout (sidebar 20% + main 70% + console 30%)
- **Decision 26:** Automatic plugin mounting via IEditorPlugin::GetUIPath()
- **Decision 27:** Ignore missing panels gracefully (log warning, skip panel, render rest)
- **Decision 28:** Fullscreen collapses the Mosaic layout to a single leaf node rather than swapping to a separate render tree (SED-018). Entering fullscreen saves the current layout tree and sets `layout` to the panel id string; exiting restores it. The Mosaic component stays mounted throughout, so no iframe is destroyed and no panel loses its in-page state.

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Library | Which docking library? | react-mosaic (modern React) | ✅ react-mosaic (Decision 23) |
| 2 | Persistence | Where to save layout? | ~/.cluiche/editor-layout.json | ✅ User home (Decision 24) |
| 3 | Default Layout | Initial panel arrangement? | IDE-style (sidebar + main + bottom) | ✅ IDE-style (Decision 25) |
| 4 | Plugin Mounting | Automatic or explicit registration? | Automatic via GetUIPath() | ✅ Automatic (Decision 26) |
| 5 | Versioning | Handle missing panels? | Ignore gracefully with warning | ✅ Ignore (Decision 27) |

## Implementation Status

**v0 Shipped** (as of 2026-04-20):

| Area | What's implemented | Notes |
|------|--------------------|-------|
| Docking library | `react-mosaic-component` v6.x | Wired in `Cluiche/CluicheEditor/UI/package.json`, styles imported in `main.tsx` |
| DockingManager | `Cluiche/CluicheEditor/UI/src/layout/DockingManager.tsx` — fetches panels via `EditorBridge.getPanels()`, builds Mosaic tree via `buildTree()` | All panels rendered as iframes to their `dia://` URL |
| Built-in panels | `Home` (`dia://editor/home/index.html`) and `Output Console` (`dia://editor/outputconsole/index.html`) | Registered in `EditorView::Initialize` so the docking area is never empty, even with no project loaded |
| Plugin panels | `EditorView::RegisterComponent(name, uiPath)` called by `EditorApplication::LoadPlugin` | Mirrors Decision 26 — automatic mounting via `IEditorPlugin::GetUIPath()` |
| Persistence path | `Data/editor-layout.json` (alongside the exe / project data) | **Not** `~/.cluiche/editor-layout.json` as originally specified — see below |
| Load on startup | `EditorView::LoadLayoutFromDisk()` called in `CluicheEditorRunningPhase::AfterModulesStart` after project is loaded | |
| Save on shutdown | `EditorView::SaveLayoutToDisk()` called in `CluicheEditorShutdownPhase::BeforeModulesStop` | |
| Save on change | `EditorBridge.saveLayout()` fires on every Mosaic `onChange` | Fires the `save_layout` event handler |
| Missing-panel tolerance | Mosaic render falls back to empty tile if panel id not in registered set | Decision 27 satisfied; panels registered after layout load still work |

**Fullscreen (fixed 2026-04-23):**
Fullscreen previously swapped to a separate `<div>` + `<iframe>` render tree, destroying the Mosaic instance and every panel's iframe on each toggle. Fixed by implementing Decision 28 (SED-018): fullscreen now saves the layout tree, collapses it to the single panel id, and restores on exit. The Mosaic component stays mounted throughout. `key={id}` also added to the tile iframe to prevent remounting on drag/resize.

**Deferred / diverged from spec:**

- **Layout path** — shipped at `Data/editor-layout.json` (project-adjacent) rather than `~/.cluiche/editor-layout.json` (user-home). Decision 24 explicitly flagged "need better data management strategy" — that strategy still isn't built. Revisit when a global editor-preferences story exists.
- **`DockingLayout` C++ utility class** — not shipped as a standalone class; its responsibilities live inside `EditorView` (`SetLayoutPath`, `LoadLayoutFromDisk`, `SaveLayoutToDisk`, the `load_layout`/`save_layout` WebUIBridge handlers). Revisit if the editor grows multiple layouts per project or per user.
- **Reset-to-default layout command** — no UI affordance; the layout file can be deleted manually to reset
- **Panel "closable" flag** — every panel renders with default toolbar controls; no per-panel closable override yet

**Revised call graph (what's actually in the tree):**

```cpp
// C++ boot
view.SetLayoutPath("Data/editor-layout.json");
view.LoadLayoutFromDisk();     // pulls JSON, stashed for "load_layout" request

// C++ shutdown
view.SaveLayoutToDisk();       // writes current layout blob to disk

// WebUIBridge request/event handlers
"get_panels"   -> JSON array of {id, title, url}
"load_layout"  -> cached JSON from disk
"save_layout"  -> overwrite in-memory blob (persisted on shutdown + on change)
```

## Status

`Approved` - v0 implemented; deferred features tracked above

# Feature Spec: Debug Data Panel Registry

> **Superseded 2026-10-13.** Dropped. The existing DiaDebugPanel renders stats and array tables from each `IDebugDomain`, and `MessageBusDebugDomain` already provides the bus-traffic Schema/Live/History tabs. A second panel registry (and its singleton) would duplicate that. See `docs/specs/applications/dia/systems/diadebugdomain/panel-console.md` for what replaced the console part.

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md  
**Mockup:** @docs/specs/applications/cow/systems/f1-harness/mockups/bus-traffic-panel.html  
**Status:** Superseded

## Summary

A registry system for in-game data panels (tables, logs, stats) distinct from visual debug draw (shapes). Provides `IDebugDataPanel` interface, panel registration/discovery, and Ultralight rendering. Proves CoW F-1 with a Bus Traffic Viewer panel showing live message table (tick, type, router, pass, count).

## Problem Statement

Existing debug infrastructure covers:
- **Visual debug draw** - `IDebugDomain` for world-space shapes (physics, AI, entities)
- **Remote inspection** - `DebugServer` + `IInspectorDataSource` for CluicheEditor queries
- **Command execution** - `DiaAPI` + Debug Console for text commands

Gap: No system for **in-game data panels** (tables, logs, stats). Examples needed:
- Live message bus traffic (type, router, count, payload size)
- Blackboard key/value viewer
- Entity list with filtering
- Asset runtime state table
- Performance metrics dashboard

Current workaround: Each CluicheTest stage builds custom ImGui HUDs (`TestStageHUDModule`, `AssetRuntimeHUDModule`). These are:
1. Scattered (no central registry)
2. Not reusable (stage-specific, duplicated code)
3. Not discoverable (no way to list available panels)

CoW testbeds need a unified panel system where any module can register a data panel, and users can toggle/dock them on demand.

## Goals

1. Define `IDebugDataPanel` interface (init, update, render via Ultralight)
2. Provide `DebugDataPanelRegistry` for registration/discovery
3. Create `BusTrafficViewerPanel` as proof (live message table for CoW F-1)
4. Panels can be toggled on/off, dragged, docked
5. Panel state persists across stage transitions
6. Panels are Debug-only (#ifdef DIA_DEBUG)

## Non-Goals

1. Editor-side panels (CluicheEditor plugins remain separate)
2. Panel inter-communication (panels are independent)
3. Custom layouts or saved workspaces (panels dock freely, no presets)
4. Panel-specific keybinds (use Debug Palette to launch panels)
5. Visual debug draw (that's `IDebugDomain`'s job)

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `IDebugDataPanel` interface exists with `GetPanelId()`, `GetDisplayName()`, `Render()` | Code review |
| AC-2 | `DebugDataPanelRegistry::Register(IDebugDataPanel*)` / `Unregister()` works | GoogleTest |
| AC-3 | `DebugDataPanelRegistry::GetPanelCount()` and `GetPanel(int index)` work | GoogleTest |
| AC-4 | `BusTrafficViewerPanel` registers itself and shows in panel list | Manual test in CoW F-1 |
| AC-5 | Bus Traffic Viewer shows live table of messages (tick, type, router, pass, count, payload size) | Manual test: post messages, see them in panel |
| AC-6 | Panel toolbar works (Live/Pause, Primary/Reaction filters, Clear button) | Manual test |
| AC-7 | Table is sortable by column (click header to sort) | Manual test |
| AC-8 | Panel stats bar shows total messages, msg/sec, unique types/routers | Manual test |
| AC-9 | Panel window can be dragged and docked | Manual test (DiaUI docking) |
| AC-10 | Panel visual layout matches mockup | Visual comparison test |
| AC-11 | Panel persists state across stage transition (Boot → FrontEnd → Boot keeps panel open if it was open) | Manual test |
| AC-12 | Recording indicator badge shows when `FileBusRecorder` is active | Manual test: start recorder, see badge pulse |

## Design

### Architecture

```
DiaDebugDataPanel (new Dia module)
  ├─ IDebugDataPanel.h                 // Abstract interface
  ├─ DebugDataPanelRegistry.h/.cpp     // Central registry
  ├─ DebugDataPanelManager.h/.cpp      // Lifecycle + per-frame updates
  └─ BusTrafficViewerPanel.h/.cpp      // Proof: Bus Traffic Viewer
      ├─ BusTrafficViewerPage.h/.cpp   // Ultralight page (C++ shell)
      └─ bus-traffic-viewer.html        // Ultralight HTML/CSS/JS
```

### IDebugDataPanel Interface

```cpp
// Dia/DiaDebugDataPanel/IDebugDataPanel.h
namespace Dia::DebugPanel {

class IDebugDataPanel {
public:
    virtual ~IDebugDataPanel() = default;

    // Identity
    virtual Dia::Core::StringCRC GetPanelId() const = 0;           // Unique ID (e.g. "bus_traffic_viewer")
    virtual const char* GetDisplayName() const = 0;                // Human label (e.g. "Bus Traffic Viewer")
    virtual const char* GetDescription() const = 0;                // One-line description
    virtual Dia::Core::StringCRC GetCategory() const = 0;          // Category (e.g. "messaging", "assets", "performance")

    // Lifecycle
    virtual void Init() = 0;                                       // Called once on registration
    virtual void Shutdown() = 0;                                   // Called once on unregistration
    virtual void Update(float deltaTime) = 0;                      // Called per-frame (gather data)

    // Visibility
    virtual bool IsVisible() const = 0;
    virtual void Show() = 0;
    virtual void Hide() = 0;

    // Ultralight page access
    virtual Dia::UI::Page* GetPage() = 0;                          // Returns nullptr if not an Ultralight panel
};

} // namespace Dia::DebugPanel
```

### DebugDataPanelManager

```cpp
// Dia/DiaDebugDataPanel/DebugDataPanelManager.h
namespace Dia::DebugPanel {

class DebugDataPanelManager : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Per-frame update manager for all registered data panels";
    
    explicit DebugDataPanelManager(const Dia::Core::StringCRC& instanceId);
    
protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    
private:
    // Calls Update(dt) on all visible panels
    void UpdatePanels(float deltaTime);
};

} // namespace Dia::DebugPanel
```

### DebugDataPanelRegistry

```cpp
// Dia/DiaDebugDataPanel/DebugDataPanelRegistry.h
namespace Dia::DebugPanel {

class DebugDataPanelRegistry {
public:
    static DebugDataPanelRegistry& Instance();

    void Register(IDebugDataPanel* panel);
    void Unregister(IDebugDataPanel* panel);

    int GetPanelCount() const;
    IDebugDataPanel* GetPanel(int index) const;
    IDebugDataPanel* FindPanel(Dia::Core::StringCRC panelId) const;

    // Visit all panels in a given category
    template<typename Fn>
    void VisitCategory(Dia::Core::StringCRC category, Fn&& fn) const;
};

} // namespace Dia::DebugPanel
```

### BusTrafficViewerPanel

**Data source:**
- Subscribes to `Bus::OnDispatch` events (via new hook in DiaMessageBus)
- Buffers last 500 messages in ring buffer
- Tracks stats: total count, per-second rate, unique types/routers

**HTML page:**
- Sortable table (tick, type, router, pass, count, payload size)
- Toolbar: Live/Pause toggle, filter buttons (Primary/Reaction), Clear button
- Stats bar: total messages, msg/sec, unique types/routers, buffer usage
- Recording badge: shows when `FileBusRecorder` is active

**Update flow:**
1. `BusTrafficViewerPanel::Update()` - drain message buffer, calculate stats
2. If panel visible, push updates to HTML via `CallJSFunction("updateTable", messagesJson)`
3. JS updates table rows, stats bar

### Panel Categories

Suggested categories (StringCRC keys):
- `"messaging"` - Bus Traffic Viewer, Message Ledger
- `"assets"` - Asset Runtime State, Loading Queue
- `"performance"` - Metrics Dashboard, Frame Time Graph
- `"simulation"` - Blackboard Inspector, Entity List
- `"ai"` - Behavior Tree Viewer, Pathfinding Debugger
- `"core"` - Console (already separate), Health Status

### Dependencies

| Dependency | Why |
|------------|-----|
| `DiaUI` | Ultralight page rendering |
| `DiaApplicationFlow` | Module lifecycle |
| `DiaMessageBus` | Bus Traffic Viewer data source |
| `DiaCore` | StringCRC, containers |

### Files Touched

**New files:**
- `Dia/DiaDebugDataPanel/IDebugDataPanel.h`
- `Dia/DiaDebugDataPanel/DebugDataPanelRegistry.h/.cpp`
- `Dia/DiaDebugDataPanel/DebugDataPanelManager.h/.cpp`
- `Dia/DiaDebugDataPanel/BusTrafficViewerPanel.h/.cpp`
- `Dia/DiaDebugDataPanel/BusTrafficViewerPage.h/.cpp`
- `Dia/DiaDebugDataPanel/Assets/bus-traffic-viewer.html` (copied from mockup)
- `Dia/DiaDebugDataPanel/DiaDebugDataPanel.vcxproj`
- `Dia/DiaDebugDataPanel/dia.debug.datapanel.architecture.module.md`
- `Cluiche/Tests/GoogleTests/DiaDebugDataPanel/TestPanelRegistry.cpp`
- `Cluiche/Tests/GoogleTests/DiaDebugDataPanel/TestBusTrafficPanel.cpp`

**Updated files:**
- `Dia/DiaMessageBus/Bus.h/.cpp` (add `OnDispatch` callback hook for panels to subscribe)
- `Cluiche/Assets/CoW/cow_main.diaapp` (add `DebugDataPanelManagerModule` instance)
- `Cluiche.sln` (add `DiaDebugDataPanel.vcxproj`)

## Usage Example (CoW F-1)

```cpp
// In CoW's Main.cpp or a CoW-specific module
#ifdef DIA_DEBUG
    auto* busPanel = new Dia::DebugPanel::BusTrafficViewerPanel();
    Dia::DebugPanel::DebugDataPanelRegistry::Instance().Register(busPanel);
#endif

// User opens Debug Palette (F1), types "bus", selects "Bus Traffic Viewer"
// Palette calls:
auto* panel = Dia::DebugPanel::DebugDataPanelRegistry::Instance().FindPanel("bus_traffic_viewer");
if (panel) panel->Show();
```

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| PD-004 | No STL in public APIs | `IDebugDataPanel` and `DebugDataPanelRegistry` use DiaCore types only |
| AD-001 | Module YAML frontmatter | `dia.debug.datapanel.architecture.module.md` provided |
| SD-F1-001 | Panels are Debug-only; #ifdef DIA_DEBUG guards | All panel code compiles out in Release |
| SD-F1-002 | Panels use Ultralight for consistency | All data panels use same rendering tech as console/debug panel |

## Open Design Questions

1. **Panel ownership.** Should `DebugDataPanelRegistry` own panel instances (delete on unregister), or does caller own? **Proposal:** Caller owns; registry holds non-owning pointers (like `IDebugDomain`).

2. **Multi-instance panels.** Can the same panel be registered multiple times (e.g., two Bus Traffic Viewers filtering different routers)? **Proposal:** Not in v1; panel IDs must be unique. Add multi-instance support later if needed.

3. **Panel persistence.** Should panel open/close state save to a config file (survives app restart)? **Proposal:** Not in v1; state persists in-memory only (across stage transitions). Add file persistence later.

4. **Panel z-order.** When multiple panels overlap, which is on top? **Proposal:** DiaUI's existing z-order system handles this; panels follow standard Ultralight stacking.

## Status

Draft

**Plan:** TBD (to be written after approval)

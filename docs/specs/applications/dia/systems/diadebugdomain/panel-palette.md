# Feature Spec: Debug Palette

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md  
**Mockup:** @docs/specs/applications/cow/systems/f1-harness/mockups/debug-palette.html  
**Status:** Approved

## Summary

A searchable, keyboard-driven discovery UI for all in-game debug tools: DiaAPI commands, data panels, and visual debug domains. Provides fuzzy search, categorized tabs, keyboard navigation (↑↓ to select, Enter to execute), and keybind display. Toggled with `F1` key. Proves CoW F-1 by letting users search "bus" and discover `bus_traffic.*` commands and Bus Traffic Viewer panel.

## Problem Statement

As the debug infrastructure grows, discoverability becomes a problem:

**Current state:**
- **47 DiaAPI commands** registered across 8 categories (asset_runtime, bus_traffic, debug.layer, simtime, help, etc.)
- **8+ data panels** (Bus Traffic Viewer, Blackboard Inspector, Entity List, Asset State, etc.)
- **7+ visual debug domains** (`IDebugDomain` implementations: Physics2D, Animation2D, Entity, Coord2D, etc.)

**Developers need to:**
1. Remember exact command names (is it `bus_traffic.start` or `bus.traffic.start`?)
2. Know what panels exist (is there a panel for X? what's it called?)
3. Find keybinds (how do I open the Bus Traffic Viewer?)

**No discovery mechanism exists.**

Contrast with CluicheEditor: command palette (`Ctrl+Shift+P`) shows all commands/actions searchable. CoW needs the same for in-game debug tools.

## Goals

1. Unified discovery: search across commands, panels, and domains
2. Fuzzy search (type "bus" → matches "bus_traffic" and "Bus Traffic Viewer")
3. Categorized tabs (All, Commands, Panels, Domains)
4. Keyboard navigation (↑↓ arrows, Enter to execute/launch)
5. Show keybinds and descriptions
6. Toggle with `F1` key
7. Executes commands, toggles panels, does NOT replace them (launches target)

## Non-Goals

1. Command history (that's Debug Console's job)
2. Command execution output (palette launches Console if needed)
3. Panel configuration (palette just shows/hides panels)
4. Custom keybind editing (keybinds are display-only)
5. Recent items / favorites (simple flat list for v1)

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | Palette toggles on/off with `F1` key | Manual test in CoW F-1 |
| AC-2 | Search box accepts text and filters results | Manual test: type "bus", see 4 results |
| AC-3 | Results grouped by type (Commands, Panels, Domains) | Manual test: see 3 groups in mockup |
| AC-4 | Fuzzy search works (type "bstrfc" matches "bus_traffic") | Manual test |
| AC-5 | ↑↓ arrow keys navigate results, Enter executes selected item | Manual test |
| AC-6 | Selecting a command opens Debug Console and pre-fills command | Manual test: select `bus_traffic.start`, console opens with command ready |
| AC-7 | Selecting a panel toggles panel visibility (shows if hidden, hides if shown) | Manual test: select Bus Traffic Viewer, panel opens |
| AC-8 | Selecting a domain toggles domain visibility in DiaDebugPanel | Manual test: select Physics2D domain |
| AC-9 | Category tabs filter results (Commands, Panels, Domains, All) | Manual test |
| AC-10 | Each result shows name, description, category badge, and keybind (if any) | Manual test: match mockup layout |
| AC-11 | Palette visual layout matches mockup | Visual comparison test |
| AC-12 | ESC key closes palette | Manual test |

## Design

### Architecture

```
DiaDebugPalette (new Dia module)
  ├─ DebugPalettePage.h/.cpp          // Ultralight page (C++ shell)
  ├─ DebugPaletteIndex.h/.cpp         // Unified index of all debug tools
  ├─ DebugPaletteExecutor.h/.cpp      // Launch target (command/panel/domain)
  └─ debug-palette.html                // Ultralight HTML/CSS/JS
```

### Components

**0. DebugPaletteModule**

```cpp
// Dia/DiaDebugPalette/DebugPaletteModule.h
namespace Dia::DebugPalette {

class DebugPaletteModule : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "Searchable discovery UI for all debug commands/panels/domains";
    
    explicit DebugPaletteModule(const Dia::Core::StringCRC& instanceId);
    ~DebugPaletteModule() override;
    
    void Toggle();
    void Show();
    void Hide();
    bool IsVisible() const { return mVisible; }
    
protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    
private:
    DebugPalettePage* mPage = nullptr;
    DebugPaletteIndex mIndex;
    DebugPaletteExecutor mExecutor;
    bool mVisible = false;
};

} // namespace Dia::DebugPalette
```

**1. DebugPaletteIndex**

Aggregates all discoverable debug tools:

```cpp
namespace Dia::DebugPalette {

enum class ItemType { Command, Panel, Domain };

struct PaletteItem {
    ItemType type;
    Dia::Core::StringCRC id;                // Unique ID
    const char* name;                        // Display name
    const char* description;                 // One-line description
    Dia::Core::StringCRC category;          // Category (e.g. "messaging", "physics")
    const char* keybind;                     // Optional keybind (e.g. "Ctrl+Shift+B")
};

class DebugPaletteIndex {
public:
    void Rebuild();                          // Scan registries and rebuild index
    int GetItemCount() const;
    const PaletteItem& GetItem(int index) const;
    
    // Search with fuzzy matching
    void Search(const char* query, Dia::Core::Containers::DynamicArrayC<int, 64>& outIndices) const;
};

} // namespace Dia::DebugPalette
```

**Index sources:**
- **Commands:** Query `Dia::API::GetCommandList()` (all registered commands)
- **Panels:** Query `Dia::DebugPanel::DebugDataPanelRegistry::Instance()` (all panels)
- **Domains:** Query `Dia::VisualDebugger::DebugDomainRegistry` (all `IDebugDomain` implementations)

**Rebuild trigger:** On `F1` keypress (palette show), rebuild index to catch any newly registered items.

**2. DebugPaletteExecutor**

Launches the selected item:

```cpp
namespace Dia::DebugPalette {

class DebugPaletteExecutor {
public:
    void Execute(const PaletteItem& item);
};

} // namespace Dia::DebugPalette
```

**Execute behavior:**
- **Command:** Open Debug Console and pre-fill command text (user presses Enter to execute)
- **Panel:** Toggle panel visibility (if hidden, show; if shown, hide)
- **Domain:** Toggle domain in DiaDebugPanel (enable/disable drawer)

**3. DebugPalettePage (Ultralight)**

- Loads `debug-palette.html`
- JS binding: `app.onItemSelected(itemIndex)`
- Receives selection from HTML, forwards to `DebugPaletteExecutor`
- Pushes search results to HTML via `CallJSFunction("updateResults", itemsJson)`

**4. HTML/CSS/JS**

See `debug-palette.html` mockup for full layout. Key features:

- **Search box** with autofocus
- **Category tabs** (All, Commands, Panels, Domains) with result counts
- **Results list** grouped by type, showing name/description/category/keybind
- **Keyboard navigation** (↑↓ to select, Enter to execute, ESC to close)
- **Empty state** ("No results found") when search has zero matches

### Search Algorithm

**Fuzzy matching:**
1. Exact prefix match (highest priority): "bus" matches "bus_traffic" before "blackboard"
2. Contains match: "traffic" matches "bus_traffic"
3. Fuzzy match: "bstrfc" matches "bus_traffic" (first letters of each word)

**Ranking:**
- Exact prefix > contains > fuzzy
- Within same rank, sort alphabetically

### Dependencies

| Dependency | Why |
|------------|-----|
| `DiaAPI` | Command list query |
| `DiaDebugDataPanel` | Panel list query |
| `DiaVisualDebugger` | Domain list query |
| `DiaDebugConsole` | Launch console with pre-filled command |
| `DiaUI` | Ultralight page rendering |
| `DiaApplicationFlow` | Module lifecycle |

### Files Touched

**New files:**
- `Dia/DiaDebugPalette/DebugPalettePage.h/.cpp`
- `Dia/DiaDebugPalette/DebugPaletteIndex.h/.cpp`
- `Dia/DiaDebugPalette/DebugPaletteExecutor.h/.cpp`
- `Dia/DiaDebugPalette/DebugPaletteModule.h/.cpp`
- `Dia/DiaDebugPalette/Assets/debug-palette.html` (copied from mockup)
- `Dia/DiaDebugPalette/DiaDebugPalette.vcxproj`
- `Dia/DiaDebugPalette/dia.debug.palette.architecture.module.md`
- `Cluiche/Tests/GoogleTests/DiaDebugPalette/TestPaletteIndex.cpp`
- `Cluiche/Tests/GoogleTests/DiaDebugPalette/TestFuzzySearch.cpp`

**Updated files:**
- `DiaAPI/CommandRegistry.h/.cpp` (add `GetCommandList()` if not present)
- `Cluiche/CluicheGameBaseline/Modules/InputModule.cpp` (bind `F1` key to palette toggle)
- `Cluiche/Assets/CoW/cow_main.diaapp` (add `DebugPaletteModule` instance)
- `Cluiche.sln` (add `DiaDebugPalette.vcxproj`)

## Usage Example (CoW F-1)

```cpp
// User presses F1
// Palette opens, search box focused

// User types "bus"
// JS calls app.onSearch("bus")
// C++ calls DebugPaletteIndex::Search("bus", results)
// Results: 4 items (3 commands + 1 panel)
// C++ calls page->CallJSFunction("updateResults", resultsJson)
// JS renders 4 results in HTML

// User presses ↓ arrow twice, Enter
// JS calls app.onItemSelected(1)  // index 1 = bus_traffic.stop
// C++ calls DebugPaletteExecutor::Execute(item)
// Executor opens Debug Console, pre-fills "bus_traffic.stop"
// Palette closes
```

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| PD-004 | No STL in public APIs | `DebugPaletteIndex` and `DebugPaletteExecutor` use DiaCore types only |
| AD-001 | Module YAML frontmatter | `dia.debug.palette.architecture.module.md` provided |
| SD-F1-001 | Palette is Debug-only; #ifdef DIA_DEBUG guards | All palette code compiles out in Release |
| SD-F1-002 | Palette uses Ultralight for consistency | All debug UI uses same rendering tech |

## Open Design Questions

1. **Command execution vs. pre-fill.** Should palette execute commands directly, or open Console with command pre-filled? **Proposal:** Pre-fill (gives user chance to add arguments, see output in Console).

2. **Panel toggle vs. show-only.** Should selecting a panel always toggle (hide if shown), or always show? **Proposal:** Toggle (matches VS Code command palette behavior).

3. **Recent items.** Should palette remember recently used items and rank them higher? **Proposal:** Not in v1; add later if needed.

4. **Keybind editing.** Should palette allow editing keybinds, or just display them? **Proposal:** Display only; keybinds are hardcoded or loaded from config (editing is a separate feature).

5. **Category icons.** Should each category have a visual icon (⚡ for commands, 📊 for panels, 🎨 for domains)? **Proposal:** Yes; mockup shows icons, improves scannability.

## Status

Draft

**Plan:** TBD (to be written after approval)

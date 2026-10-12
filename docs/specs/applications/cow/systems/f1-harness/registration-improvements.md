# Debug Tool Registration - Improvements & Cleanup

**Date:** 2026-10-11  
**Status:** Proposed

## Current State Analysis

### What's Good

✅ **DiaAPI `CommandCallback`** - Already uses `std::function`, supports lambdas/captures  
✅ **IDebugDomain interface** - Well-defined, 10 pure virtual methods  
✅ **Separate registries** - Commands, Panels, Domains have distinct purposes

### What Needs Improvement

❌ **Scattered registration** - 3 separate APIs, no grouping  
❌ **No keybind registration** - Keybinds hardcoded elsewhere  
❌ **Panel construction** - `Panel<T>()` assumes default constructor, no dependency injection  
❌ **Category inference** - Implicit from string prefix, not explicit  
❌ **No documentation helper** - Hard to know what's already registered

---

## Proposed Improvements

### 1. **ModuleDebugRegistry - Fluent API** (NEW)

The unified registration API we designed:

```cpp
mDebugTools
    .Command("bus_traffic.start", "Start recording", &StartRecording)
    .Panel<BusTrafficViewerPanel>()
    .Domain<PathfindingVisualDebugger>()
    .Activate();
```

**Problems to solve:**

#### Problem 1.1: Member Functions vs. Free Functions

Currently assumes free functions or static methods. Better:

```cpp
// OLD (requires static wrapper)
static int StartRecording(const CommandArgs& args) {
    return MyModule::Instance().DoStartRecording(args);
}
mDebugTools.Command("start", "...", &StartRecording);

// NEW (direct member function)
mDebugTools.Command("start", "...", 
    [this](const CommandArgs& args) { return DoStartRecording(args); });
```

**Solution:** Document that lambdas with `[this]` capture work out of the box (already supported via `std::function`).

---

#### Problem 1.2: Panel/Domain Construction

Currently assumes default constructor. What if dependencies needed?

```cpp
// Won't work if BusTrafficViewerPanel needs a Bus reference
mDebugTools.Panel<BusTrafficViewerPanel>();  // ERROR: no default ctor
```

**Solution:** Add variadic template overload:

```cpp
// Pass constructor arguments
mDebugTools.Panel<BusTrafficViewerPanel>(mBus, mRecorder);

// OR pass constructed instance
auto* panel = new BusTrafficViewerPanel(mBus, mRecorder);
mDebugTools.Panel(panel);  // Takes ownership
```

**Implementation:**

```cpp
template<typename T, typename... Args>
ModuleDebugRegistry& Panel(Args&&... args) {
    auto* panel = new T(std::forward<Args>(args)...);
    mPanels.PushBack(panel);
    return *this;
}

// Overload for already-constructed instance
ModuleDebugRegistry& Panel(IDebugDataPanel* panel) {
    mPanels.PushBack(panel);
    return *this;
}
```

---

#### Problem 1.3: Keybinds

Currently keybinds are hardcoded in `InputModule` or elsewhere. No connection to panels.

```cpp
// Somewhere in InputModule.cpp:
if (keyboard.WasKeyPressed(Key::kCtrlShiftB))
    OpenPanel("bus_traffic_viewer");  // String ID, brittle
```

**Solution:** Register keybind with panel:

```cpp
mDebugTools
    .Panel<BusTrafficViewerPanel>()
        .Keybind("Ctrl+Shift+B")  // Returns PanelBuilder&
    .Panel<BlackboardInspectorPanel>()
        .Keybind("Ctrl+Shift+K");
```

**Implementation:** Use builder pattern:

```cpp
class PanelBuilder {
public:
    PanelBuilder(ModuleDebugRegistry* owner, IDebugDataPanel* panel);
    
    PanelBuilder& Keybind(const char* keybind);
    ModuleDebugRegistry& End();  // Return to main builder
    
private:
    ModuleDebugRegistry* mOwner;
    IDebugDataPanel* mPanel;
};

// In ModuleDebugRegistry:
template<typename T, typename... Args>
PanelBuilder Panel(Args&&... args) {
    auto* panel = new T(std::forward<Args>(args)...);
    mPanels.PushBack(panel);
    return PanelBuilder(this, panel);
}
```

**Alternate (simpler):** Keybind as method parameter:

```cpp
mDebugTools
    .Panel<BusTrafficViewerPanel>("Ctrl+Shift+B")
    .Panel<BlackboardInspectorPanel>("Ctrl+Shift+K");
```

---

#### Problem 1.4: Category

Currently inferred from command prefix (`bus_traffic.*` → category "bus_traffic"). Better to be explicit:

```cpp
// Explicit category
ModuleDebugRegistry mDebugTools{"pathfinding", "Pathfinding", "navigation"};
//                                  ^id          ^displayName   ^category
```

**All tools inherit the category** - commands, panels, domains grouped under "Navigation" in Palette.

---

### 2. **Discovery Helper** (NEW)

Add runtime introspection:

```cpp
// List what's registered (debugging aid)
mDebugTools.PrintSummary();
// Output:
//   pathfinding [Navigation]
//     Commands: 4 (show_grid, clear_cache, find_path, profile_query)
//     Panels: 1 (Pathfinding Debug Panel)
//     Domains: 1 (Pathfinding Domain)

// Validate at startup (catch typos/duplicates)
mDebugTools.Validate();  // Throws if: duplicate IDs, missing names, null callbacks
```

---

### 3. **Cleanup Existing Registration Sites**

Many modules already register debug tools manually. Clean them up:

#### Before (DiaAssetRuntime):

```cpp
// In AssetRuntimeDebugCommands.cpp (separate file)
void RegisterAssetRuntimeCommands(AssetRuntime& runtime, ITransitionNotifier* notifier) {
    Dia::API::CommandInfo cmd;
    cmd.name = "asset-runtime-get-loaded";
    cmd.description = "List all loaded assets";
    cmd.category = "asset-runtime";
    cmd.callback = [&runtime](const CommandArgs& args) { return GetLoaded(runtime, args); };
    Dia::API::RegisterCommand(cmd);
    
    // ... repeat for 5 more commands ...
}
```

#### After:

```cpp
// In AssetRuntimeModule.cpp (with the module)
void AssetRuntimeModule::DoStart() {
#ifdef DIA_DEBUG
    mDebugTools{"asset_runtime", "Asset Runtime", "assets"}
        .Command("get_loaded", "List loaded assets", 
            [this](auto& a) { return GetLoaded(a); })
        .Command("get_staged", "List staged assets",
            [this](auto& a) { return GetStaged(a); })
        .Command("get_state", "Get asset state",
            [this](auto& a) { return GetState(a); })
        .Panel<AssetStatePanel>("Ctrl+Shift+A")
        .Activate();
#endif
}
```

**Benefits:**
- ✅ All debug tools in one place
- ✅ Scoped to module lifetime (auto-cleanup)
- ✅ No separate `RegisterXxxCommands()` function
- ✅ Keybind registered with panel

---

### 4. **Module Template Update**

When `dia scaffold module Pathfinding` runs, generate:

```cpp
// Dia/DiaPathfinding/PathfindingModule.h
class PathfindingModule : public SimModule {
    // ... normal module members ...
    
#ifdef DIA_DEBUG
    Dia::Debug::ModuleDebugRegistry mDebugTools{
        "pathfinding",        // ID (used in command prefix)
        "Pathfinding",        // Display name
        "navigation"          // Category (for grouping in Palette)
    };
#endif
};

// Dia/DiaPathfinding/PathfindingModule.cpp
void PathfindingModule::DoStart() {
    // ... normal startup ...
    
#ifdef DIA_DEBUG
    mDebugTools
        // Commands - use [this] to capture module context
        .Command("show_grid", "Show navigation grid overlay",
            [this](const Dia::API::CommandArgs& args) {
                // Command implementation here
                return 0;  // 0=success
            })
        
        // Panels - pass dependencies if needed
        .Panel<PathfindingDebugPanel>(/* dependencies */)
            .Keybind("Ctrl+Shift+P")  // Optional keybind
        
        // Visual debuggers - for world-space shapes
        .Domain<PathfindingVisualDebugger>(/* dependencies */)
        
        .Activate();  // Registers everything
#endif
    
    return StartResult::kReady;
}

void PathfindingModule::DoStop() {
#ifdef DIA_DEBUG
    mDebugTools.Deactivate();  // Auto-cleanup
#endif
    return StopResult::kReady;
}
```

---

## Recommended Changes to Specs

### Debug Console Spec
Add section: **"Command Registration - Member Functions"**
> Commands can use lambdas with `[this]` capture to access module context. `CommandCallback` is `std::function<int(const CommandArgs&)>`, which supports any callable.

### Debug Data Panel Spec
Add section: **"Panel Construction - Dependency Injection"**
> Panels can accept constructor arguments: `.Panel<T>(arg1, arg2)`. `ModuleDebugRegistry` forwards arguments to the panel constructor.

Add section: **"Keybind Registration"**
> Panels can register keybinds: `.Panel<T>().Keybind("Ctrl+Shift+B")`. The keybind appears in Debug Palette and is automatically wired to toggle the panel.

### Module Debug Registry Spec (NEW)
Create full spec for `ModuleDebugRegistry` API:
- Constructor: `(id, displayName, category)`
- `.Command(name, desc, callback)` - register command
- `.Panel<T>(args...)` - register panel with dependencies
- `.Panel<T>().Keybind(key)` - register panel with keybind
- `.Domain<T>(args...)` - register visual debugger
- `.Activate()` / `.Deactivate()` - lifecycle
- `.Validate()` - runtime checks
- `.PrintSummary()` - debugging aid

---

## Migration Strategy

### Phase 1: Core API (1-2 days)
- Implement `ModuleDebugRegistry` with improvements
- Variadic template for Panel/Domain construction
- Keybind registration
- Validate/PrintSummary helpers

### Phase 2: Update Specs (1 day)
- Add registration sections to Console/Panel/Palette specs
- Write full `module-debug-registry.md` spec

### Phase 3: Clean Up Existing Registrations (2-3 days)
Migrate these modules to new pattern:
- DiaAssetRuntime (5 commands → inline in module)
- DiaMessageBus (3 commands → inline)
- DiaVisualDebugger (debug domains → keep as-is, already good)

### Phase 4: Update Module Template (1 day)
- `dia scaffold module` generates `mDebugTools` member
- Comment blocks showing examples
- Add to module documentation

---

## Summary: What Gets Better

| Aspect | Before | After |
|--------|--------|-------|
| **Command registration** | Free functions or static wrappers | Lambdas with `[this]`, inline |
| **Panel construction** | Default constructor only | Pass dependencies via variadic template |
| **Keybinds** | Hardcoded in InputModule | Registered with panel |
| **Category** | Inferred from string prefix | Explicit in constructor |
| **Discovery** | No helper | `.PrintSummary()`, `.Validate()` |
| **Location** | Separate `RegisterXxxCommands()` files | Inline in module `DoStart()` |
| **Cleanup** | Manual `Unregister()` calls | Auto-cleanup in `Deactivate()` |

---

## Open Questions

1. **Builder pattern vs. method parameters for keybinds?**
   - Option A: `.Panel<T>().Keybind("Ctrl+B")` (fluent, but more complex)
   - Option B: `.Panel<T>("Ctrl+B")` (simpler, but positional arg)
   - **Recommendation:** Option B for simplicity

2. **Category inheritance - all tools share module's category?**
   - Yes (simpler, enforces grouping)
   - Or allow per-tool override?
   - **Recommendation:** Yes, share category. Override only if needed later.

3. **Validation - throw or log on errors?**
   - Throw: strict, fails fast (Debug builds only)
   - Log: lenient, shows warning but continues
   - **Recommendation:** Throw in Debug, no-op in Release

---

## Status

Proposed (ready for approval alongside feature specs)

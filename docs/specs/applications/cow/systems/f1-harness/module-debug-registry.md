# Feature Spec: Module Debug Registry

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md  
**Status:** Approved  
**Priority:** P0 - Foundation for Console, Panel Registry, and Palette

## Summary

`ModuleDebugRegistry` is a fluent API for modules to register all their debug tools (commands, panels, visual debuggers) in one place. Owns the tool instances, handles lifecycle (activate/deactivate), and forwards to global registries (`DiaAPI`, `DebugDataPanelRegistry`, `DebugDomainRegistry`).

## Problem Statement

Currently, modules register debug tools via 3 separate APIs:
1. `Dia::API::RegisterCommand()` - commands
2. `DebugDataPanelRegistry::Register()` - data panels
3. `DebugDomainRegistry::Register()` - visual debug domains

Problems:
- **Scattered** - debug tools spread across file, hard to see what's available
- **Manual cleanup** - must remember to unregister in DoStop()
- **Boilerplate** - repeat struct initialization for each command
- **No grouping** - commands/panels/domains aren't visibly related

## Goals

1. Unified registration - all tools in one fluent chain
2. Auto-cleanup - deactivation unregisters and deletes owned objects
3. Type-safe - templates enforce correct types
4. Dependency injection - panels/domains can take constructor args
5. Keybind registration - panels declare keybinds inline
6. Category grouping - all tools share module's category

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `ModuleDebugRegistry` constructor takes (id, displayName, category) | Code review |
| AC-2 | `.Command(name, desc, callback)` registers command, returns `*this` for chaining | Code review + test |
| AC-3 | `.Panel<T>(args...)` creates panel with dependencies, returns `*this` | Code review + test |
| AC-4 | `.Domain<T>(args...)` creates domain with dependencies, returns `*this` | Code review + test |
| AC-5 | `.Activate()` registers all tools with global registries | GoogleTest |
| AC-6 | `.Deactivate()` unregisters all tools and deletes owned instances | GoogleTest + Valgrind (no leaks) |
| AC-7 | Destructor calls `Deactivate()` if not already called | GoogleTest |
| AC-8 | `.Validate()` throws on duplicate IDs, null callbacks, empty names | GoogleTest |
| AC-9 | `.PrintSummary()` logs tool count to DIA_LOG_INFO | Manual test |
| AC-10 | Works with lambdas `[this]` capturing module context | Code review + example |

## Design

### Full API

```cpp
// Dia/DiaDebug/ModuleDebugRegistry.h
namespace Dia::Debug {

class ModuleDebugRegistry {
public:
    // Constructor: module ID, display name, category for Palette grouping
    ModuleDebugRegistry(const char* id, const char* displayName, const char* category);
    
    ~ModuleDebugRegistry();  // Calls Deactivate() if active
    
    // ──────────────────────────────────────────────────────────
    // Fluent API - Returns *this for chaining
    // ──────────────────────────────────────────────────────────
    
    // Register command with lambda callback (supports [this] capture)
    ModuleDebugRegistry& Command(const char* name, 
                                  const char* description, 
                                  Dia::API::CommandCallback callback);
    
    // Register panel with dependencies (variadic template)
    template<typename TPanelType, typename... Args>
    ModuleDebugRegistry& Panel(Args&&... args);
    
    // Register panel with dependencies + keybind
    template<typename TPanelType, typename... Args>
    ModuleDebugRegistry& Panel(const char* keybind, Args&&... args);
    
    // Register panel from already-constructed instance (takes ownership)
    ModuleDebugRegistry& Panel(Dia::DebugPanel::IDebugDataPanel* panel, 
                                const char* keybind = nullptr);
    
    // Register visual debug domain with dependencies
    template<typename TDomainType, typename... Args>
    ModuleDebugRegistry& Domain(Args&&... args);
    
    // Register domain from already-constructed instance (takes ownership)
    ModuleDebugRegistry& Domain(Dia::VisualDebugger::IDebugDomain* domain);
    
    // ──────────────────────────────────────────────────────────
    // Lifecycle
    // ──────────────────────────────────────────────────────────
    
    // Register all tools with global registries
    // Call in module's DoStart() after adding all tools
    void Activate();
    
    // Unregister all tools and delete owned instances
    // Call in module's DoStop(), or let destructor handle it
    void Deactivate();
    
    bool IsActive() const { return mActive; }
    
    // ──────────────────────────────────────────────────────────
    // Debug Helpers
    // ──────────────────────────────────────────────────────────
    
    // Validate all registered tools (throws on errors in Debug, no-op in Release)
    // Checks: duplicate IDs, null callbacks, empty names, null panel/domain pointers
    void Validate();
    
    // Log summary to DIA_LOG_INFO: "{id} [{category}] Commands: X, Panels: Y, Domains: Z"
    void PrintSummary() const;
    
    // ──────────────────────────────────────────────────────────
    // Accessors (for testing)
    // ──────────────────────────────────────────────────────────
    
    const char* GetId() const { return mId; }
    const char* GetDisplayName() const { return mDisplayName; }
    const char* GetCategory() const { return mCategory; }
    
    int GetCommandCount() const { return mCommands.Size(); }
    int GetPanelCount() const { return mPanels.Size(); }
    int GetDomainCount() const { return mDomains.Size(); }
    
private:
    const char* mId;
    const char* mDisplayName;
    const char* mCategory;
    bool mActive = false;
    
    // Owned objects - deleted in Deactivate()
    Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfo, 16> mCommands;
    Dia::Core::Containers::DynamicArrayC<Dia::DebugPanel::IDebugDataPanel*, 4> mPanels;
    Dia::Core::Containers::DynamicArrayC<const char*, 4> mPanelKeybinds;  // Parallel to mPanels
    Dia::Core::Containers::DynamicArrayC<Dia::VisualDebugger::IDebugDomain*, 4> mDomains;
};

// ──────────────────────────────────────────────────────────
// Template Implementations (in header)
// ──────────────────────────────────────────────────────────

template<typename TPanelType, typename... Args>
ModuleDebugRegistry& ModuleDebugRegistry::Panel(Args&&... args) {
    return Panel(nullptr, std::forward<Args>(args)...);  // No keybind
}

template<typename TPanelType, typename... Args>
ModuleDebugRegistry& ModuleDebugRegistry::Panel(const char* keybind, Args&&... args) {
    auto* panel = new TPanelType(std::forward<Args>(args)...);
    mPanels.PushBack(panel);
    mPanelKeybinds.PushBack(keybind);
    return *this;
}

template<typename TDomainType, typename... Args>
ModuleDebugRegistry& ModuleDebugRegistry::Domain(Args&&... args) {
    auto* domain = new TDomainType(std::forward<Args>(args)...);
    mDomains.PushBack(domain);
    return *this;
}

} // namespace Dia::Debug
```

---

## Implementation Details

### Activate() - Registration

```cpp
void ModuleDebugRegistry::Activate() {
    if (mActive) {
        DIA_LOG_WARNING("Debug", "ModuleDebugRegistry '%s' already active", mId);
        return;
    }
    
    // Register commands with DiaAPI
    for (int i = 0; i < mCommands.Size(); ++i) {
        const auto& cmd = mCommands[i];
        Dia::API::RegisterCommand(cmd);
    }
    
    // Register panels with DebugDataPanelRegistry
    for (int i = 0; i < mPanels.Size(); ++i) {
        auto* panel = mPanels[i];
        panel->Init();  // Call panel's init
        Dia::DebugPanel::DebugDataPanelRegistry::Instance().Register(panel);
        
        // Register keybind if provided (stored separately for now)
        const char* keybind = mPanelKeybinds[i];
        if (keybind != nullptr) {
            // TODO: Keybind registration API (Phase 2)
            // KeybindRegistry::Instance().Register(keybind, [panel]() { panel->Toggle(); });
        }
    }
    
    // Register domains with DebugDomainRegistry
    for (int i = 0; i < mDomains.Size(); ++i) {
        auto* domain = mDomains[i];
        // Domain registers its drawers internally via DebugLayerManager
        Dia::VisualDebugger::DebugDomainRegistry::Instance().Register(domain);
    }
    
    mActive = true;
}
```

### Deactivate() - Cleanup

```cpp
void ModuleDebugRegistry::Deactivate() {
    if (!mActive)
        return;
    
    // Unregister commands from DiaAPI
    for (int i = 0; i < mCommands.Size(); ++i) {
        Dia::API::UnregisterCommand(mCommands[i].name);
    }
    
    // Unregister and delete panels
    for (int i = 0; i < mPanels.Size(); ++i) {
        auto* panel = mPanels[i];
        Dia::DebugPanel::DebugDataPanelRegistry::Instance().Unregister(panel);
        panel->Shutdown();
        delete panel;
    }
    mPanels.Clear();
    mPanelKeybinds.Clear();
    
    // Unregister and delete domains
    for (int i = 0; i < mDomains.Size(); ++i) {
        auto* domain = mDomains[i];
        Dia::VisualDebugger::DebugDomainRegistry::Instance().Unregister(domain);
        delete domain;
    }
    mDomains.Clear();
    
    mActive = false;
}
```

---

## Usage Examples

### Example 1: Commands Only

```cpp
class MessageBusModule : public SimModule {
    Dia::Debug::ModuleDebugRegistry mDebugTools{"bus_traffic", "Bus Traffic", "messaging"};
};

void MessageBusModule::DoStart() {
#ifdef DIA_DEBUG
    mDebugTools
        .Command("start", "Start recording bus traffic",
            [this](const Dia::API::CommandArgs& args) { return StartRecording(args); })
        .Command("stop", "Stop recording",
            [this](const Dia::API::CommandArgs& args) { return StopRecording(args); })
        .Command("status", "Show recorder status",
            [this](const Dia::API::CommandArgs& args) { return ShowStatus(args); })
        .Activate();
#endif
    return StartResult::kReady;
}

void MessageBusModule::DoStop() {
#ifdef DIA_DEBUG
    mDebugTools.Deactivate();  // Auto-cleanup
#endif
    return StopResult::kReady;
}
```

### Example 2: Panel with Dependencies

```cpp
class PathfindingModule : public SimModule {
    Dia::Debug::ModuleDebugRegistry mDebugTools{"pathfinding", "Pathfinding", "navigation"};
    PathfindingGrid* mGrid = nullptr;
};

void PathfindingModule::DoStart() {
    mGrid = new PathfindingGrid();
    
#ifdef DIA_DEBUG
    mDebugTools
        .Panel<PathfindingDebugPanel>("Ctrl+Shift+P", mGrid)  // Pass grid as dependency
        .Domain<PathfindingVisualDebugger>(mGrid)
        .Activate();
#endif
    return StartResult::kReady;
}
```

### Example 3: Full Stack

```cpp
class EntityModule : public SimModule {
    Dia::Debug::ModuleDebugRegistry mDebugTools{"entity", "Entities", "core"};
};

void EntityModule::DoStart() {
#ifdef DIA_DEBUG
    mDebugTools
        .Command("list", "List all entities",
            [this](auto& a) { return ListEntities(a); })
        .Command("inspect", "Inspect entity by ID",
            [this](auto& a) { return InspectEntity(a); })
        .Panel<EntityListPanel>("Ctrl+Shift+E")
        .Domain<EntityVisualDebugger>()
        .Validate()      // Catch errors before activating
        .PrintSummary()  // Log "entity [core] Commands: 2, Panels: 1, Domains: 1"
        .Activate();
#endif
    return StartResult::kReady;
}
```

---

## Files Touched

**New files:**
- `Dia/DiaDebug/ModuleDebugRegistry.h` (~150 lines)
- `Dia/DiaDebug/ModuleDebugRegistry.cpp` (~200 lines)
- `Dia/DiaDebug/DiaDebug.vcxproj` (new static lib)
- `Dia/DiaDebug/dia.debug.architecture.module.md`
- `Cluiche/Tests/GoogleTests/DiaDebug/TestModuleDebugRegistry.cpp`

**Updated files:**
- `Cluiche.sln` (add DiaDebug.vcxproj)
- Module templates in `dia scaffold module` (add mDebugTools pattern)

---

## Dependencies

| Dependency | Why |
|------------|-----|
| `DiaAPI` | Command registration |
| `DiaDebugDataPanel` | Panel registry (built by F-1) |
| `DiaVisualDebugger` | Domain registry (already exists) |
| `DiaCore` | StringCRC, DynamicArrayC |
| `DiaObservation` | DIA_LOG_* for PrintSummary() |

---

## Open Design Questions

1. **Keybind registration** - How to wire keybinds to panels?
   - **Option A:** Store keybinds in ModuleDebugRegistry, Debug Palette reads them
   - **Option B:** Separate `KeybindRegistry` that maps keys to actions
   - **Recommendation:** Option A for v1 (simpler), add KeybindRegistry later if needed

2. **Command unregistration** - Does `Dia::API::UnregisterCommand()` exist?
   - Need to check DiaAPI - if not, add it
   - Alternative: Commands stay registered until app shutdown (acceptable for Debug-only)

3. **Ownership of command callbacks** - Lambda captures `[this]` - what if module deleted?
   - **Solution:** Deactivate() must be called in DoStop() before module destructor runs
   - Document in spec: calling order matters

---

## Status

Draft (buildable - ready for implementation)

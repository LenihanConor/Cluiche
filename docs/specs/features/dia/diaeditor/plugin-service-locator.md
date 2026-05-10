# Feature Spec: Plugin Service Locator

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Plugin Service Locator** | (this document) |

## Problem Statement

Editor plugins need to share services (e.g., GameConnectionManager) without tight coupling or raw pointer passing. Currently each plugin that needs a game connection creates its own `GameConnectionManager` instance, resulting in multiple independent WebSocket connections to the same game server. A type-safe service locator on `EditorPluginContext` allows plugins to register and consume shared services by type, decoupling providers from consumers while maintaining safe access patterns.

## Acceptance Criteria

- [x] `PluginServiceLocator` class with `RegisterService<T>`, `UnregisterService<T>`, `GetService<T>` template methods
- [x] Type resolution via `T::kUniqueId` (existing StringCRC pattern)
- [x] `EditorPluginContext` exposes `PluginServiceLocator* mServices`
- [x] Service locator instance owned by the application layer (PluginLoaderModule), not by DiaEditor library
- [x] `GameConnectionEditorPlugin` registers its `GameConnectionManager` as a service during `OnLoad`
- [x] Other plugins retrieve the shared manager via `context.mServices->GetService<GameConnectionManager>()`
- [x] `GetService<T>` returns `nullptr` if service not registered (graceful degradation)
- [x] No RTTI — type safety via `kUniqueId` StringCRC matching

## Design

### PluginServiceLocator

```cpp
namespace Dia::Editor {
    class PluginServiceLocator {
    public:
        template<typename T>
        void RegisterService(T* service);

        template<typename T>
        void UnregisterService();

        template<typename T>
        T* GetService() const;

    private:
        struct ServiceEntry {
            Dia::Core::StringCRC typeId;
            void* service;
        };

        static const unsigned int kMaxServices = 16;
        DynamicArrayC<ServiceEntry, kMaxServices> mServices;
    };
}
```

Type resolution uses `T::kUniqueId` — the same `static const StringCRC` pattern used throughout Dia for Modules, Components, and plugin data types.

### Context Integration

```cpp
struct EditorPluginContext {
    EditorModel* mModel;
    EditorView* mView;
    WebUIBridge* mBridge;
    IPluginLoader* mPluginLoader;
    PluginServiceLocator* mServices;  // new
};
```

### Ownership

The `PluginServiceLocator` instance is owned by `PluginLoaderModule` (CluicheEditor application layer). DiaEditor only defines the class — it does not instantiate or own it. This follows SED-015 (DiaEditor is a pure library).

### Provider Pattern

```cpp
// GameConnectionEditorPlugin::OnLoad
void GameConnectionEditorPlugin::OnLoad(const EditorPluginContext& context) {
    mManager.Initialize();
    // ... existing setup ...
    if (context.mServices)
        context.mServices->RegisterService(&mManager);
}
```

### Consumer Pattern

```cpp
// Any plugin that needs the shared connection
void SomePlugin::OnLoad(const EditorPluginContext& context) {
    GameConnectionManager* mgr = nullptr;
    if (context.mServices)
        mgr = context.mServices->GetService<GameConnectionManager>();
    // mgr may be nullptr if GameConnectionEditorPlugin not loaded
}
```

## Implementation Files

- `Dia/DiaEditor/Plugin/PluginServiceLocator.h` - Service locator class (header-only, template methods)
- `Dia/DiaEditor/Plugin/EditorPluginContext.h` - Added `mServices` field
- `Cluiche/CluicheEditor/ApplicationFlow/Modules/PluginLoaderModule.h` - Owns `PluginServiceLocator` instance
- `Cluiche/CluicheEditor/ApplicationFlow/Modules/PluginLoaderModule.cpp` - Wires instance into context
- `Dia/DiaEditor/Plugin/GameConnectionEditorPlugin.cpp` - Registers manager as service

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — service type resolution via `T::kUniqueId` StringCRC |
| Platform | PD-004 | No STL in public APIs | **Compliant** — uses DynamicArrayC, void*, templates; no std:: types in interface |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — added to DiaEditor.vcxproj |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — `Dia::Editor::PluginServiceLocator` |
| DiaEditor | SED-001 | Plugin interface minimal and stable | **Compliant** — service locator is additive; IEditorPlugin unchanged |
| DiaEditor | SED-013 | Plugin data structs declare kPluginDataTypeId | **Compliant** — same `kUniqueId` pattern for service type resolution |
| DiaEditor | SED-015 | DiaEditor is a pure library, no DiaApplicationFlow dependency | **Compliant** — PluginServiceLocator is a plain class; instance owned by consumer |

**All binding decisions: COMPLIANT**

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Lifetime | What guarantees that a service outlives its consumers? | Load ordering: built-in plugins (GameConnectionEditorPlugin) load before manifest plugins. Unload is reverse order. The provider outlives all consumers. |
| 2 | Thread Safety | Is PluginServiceLocator thread-safe? | Not needed — all plugin OnLoad/OnUnload/OnUpdate calls are on the same thread (PluginLoaderModule::DoUpdate). If threading is added later, a mutex can be added. |
| 3 | Collision | What if two plugins register the same service type? | Second registration would be added alongside the first; GetService returns the first match. In practice this won't happen — services are singletons by convention (one GameConnectionManager). |
| 4 | Alternatives | Why not use EditorModel's observer pattern for this? | Observers are for data notifications. Service locator is for accessing shared object instances (methods, subscriptions). Different use case. |

## Status

`Done` - Implemented and passing tests (4350/4351, pre-existing unrelated failure)

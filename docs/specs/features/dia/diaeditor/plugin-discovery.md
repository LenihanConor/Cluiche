# Feature Spec: Plugin Discovery

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Plugin Discovery** | (this document) |

## Problem Statement

Discovers and loads editor plugins via EditorPluginRegistry, enabling system-specific editors (like DiaApplicationEditor) to register themselves and be instantiated by the host application.

## Acceptance Criteria

- [x] EditorPluginRegistry with singleton pattern
- [x] REGISTER_EDITOR_PLUGIN macro for auto-registration at startup
- [x] Create plugin instances by StringCRC type ID
- [x] List all registered plugins
- [x] IEditorPlugin interface defines plugin contract
- [x] Plugin factories stored in registry
- [x] Thread-safe registration (static initialization)
- [x] Plugins register before EditorApplication::DoStart()

## Design

### IEditorPlugin Interface

```cpp
namespace Dia::Editor {
    enum class LayoutMode {
        kFullScreen,  // Takes entire window
        kDockable     // Can be docked with other panels
    };
    
    class IEditorPlugin {
    public:
        virtual ~IEditorPlugin() = default;
        
        // Metadata
        virtual const char* GetName() const = 0;
        virtual const char* GetVersion() const = 0;
        virtual const char* GetDescription() const = 0;
        
        // UI integration
        virtual const char* GetUIPath() const = 0;      // Path to web UI assets
        virtual LayoutMode GetLayoutMode() const = 0;
        
        // Lifecycle
        virtual void OnLoad(EditorModel* model) = 0;
        virtual void OnUnload() = 0;
        virtual void OnUpdate(float deltaTime) = 0;
    };
}
```

### Plugin Factory

```cpp
namespace Dia::Editor {
    class IEditorPluginFactory {
    public:
        virtual ~IEditorPluginFactory() = default;
        virtual IEditorPlugin* Create() = 0;
    };
}
```

### EditorPluginRegistry

```cpp
namespace Dia::Editor {
    class EditorPluginRegistry {
    public:
        static EditorPluginRegistry& Instance();
        
        void RegisterPlugin(const StringCRC& typeId, IEditorPluginFactory* factory);
        IEditorPlugin* CreatePlugin(const StringCRC& typeId);
        
        const DynamicArrayC<StringCRC, 32>& GetRegisteredPlugins() const;
        bool IsPluginRegistered(const StringCRC& typeId) const;
        
    private:
        EditorPluginRegistry() = default;
        
        struct PluginEntry {
            StringCRC typeId;
            IEditorPluginFactory* factory;
        };
        
        DynamicArrayC<PluginEntry, 32> mPlugins;
        Dia::Core::Mutex mMutex;
    };
}
```

### Registration Macro

```cpp
#define REGISTER_EDITOR_PLUGIN(ClassName, TypeName) \
    namespace { \
        struct ClassName##Factory : public Dia::Editor::IEditorPluginFactory { \
            Dia::Editor::IEditorPlugin* Create() override { \
                return new ClassName(); \
            } \
        }; \
        static ClassName##Factory g_##ClassName##Factory; \
        \
        struct ClassName##Registrar { \
            ClassName##Registrar() { \
                Dia::Editor::EditorPluginRegistry::Instance().RegisterPlugin( \
                    Dia::Core::StringCRC(TypeName), \
                    &g_##ClassName##Factory \
                ); \
            } \
        }; \
        static ClassName##Registrar g_##ClassName##Registrar; \
    }
```

### Usage Example

**DiaApplicationEditor.cpp:**
```cpp
#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>

namespace Dia::ApplicationEditor {
    class DiaApplicationEditor : public Dia::Editor::IEditorPlugin {
    public:
        const char* GetName() const override { return "DiaApplicationEditor"; }
        const char* GetVersion() const override { return "1.0.0"; }
        const char* GetDescription() const override { 
            return "Edit .diaapp manifest files"; 
        }
        
        const char* GetUIPath() const override { 
            return "dia://editor/diaapplicationeditor/index.html"; 
        }
        
        LayoutMode GetLayoutMode() const override { 
            return LayoutMode::kFullScreen; 
        }
        
        void OnLoad(EditorModel* model) override {
            // Initialize plugin
        }
        
        void OnUpdate(float deltaTime) override {
            // Update logic
        }
        
        void OnUnload() override {
            // Cleanup
        }
    };
}

// Auto-register on static initialization
REGISTER_EDITOR_PLUGIN(Dia::ApplicationEditor::DiaApplicationEditor, "DiaApplicationEditor");
```

**EditorApplication loads plugins:**
```cpp
void EditorApplication::LoadPluginsFromManifest(const char* manifestPath) {
    // Parse manifest
    Json::Value manifest = LoadManifest(manifestPath);
    
    // Load plugins specified in manifest
    for (const auto& pluginConfig : manifest["editor"]["plugins"]) {
        const char* typeStr = pluginConfig["type"].asCString();
        StringCRC typeId(typeStr);
        
        // Create plugin instance
        IEditorPlugin* plugin = EditorPluginRegistry::Instance().CreatePlugin(typeId);
        
        if (plugin) {
            plugin->OnLoad(mModel);
            mLoadedPlugins.Add(plugin);
            DIA_LOG("Loaded editor plugin: %s", plugin->GetName());
        } else {
            DIA_LOG_ERROR("Failed to load plugin: %s (not registered)", typeStr);
        }
    }
}
```

## Implementation Files

- `Dia/DiaEditor/Plugin/IEditorPlugin.h` - Plugin interface
- `Dia/DiaEditor/Plugin/EditorPluginRegistry.h` - Registry interface
- `Dia/DiaEditor/Plugin/EditorPluginRegistry.cpp` - Registry implementation

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — plugin type IDs, instance IDs are StringCRC |
| Platform | PD-002 | PU/Phase/Module architecture | **N/A** — registry is a singleton utility, not a Module |
| Platform | PD-003 | Component-based entities | **N/A** — plugin discovery, not entity management |
| Platform | PD-004 | No STL in public APIs | **Compliant** — IEditorPlugin uses `const char*`; registry uses DynamicArrayC, StringCRC |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — built within DiaEditor .vcxproj |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — all types in `Dia::Editor::` |
| Dia | AD-004 | PU/Phase/Module for apps | **N/A** — registry runs at static init, before PU lifecycle |
| DiaEditor | SED-001 | Plugin interface minimal and stable | **Compliant** — IEditorPlugin has 8 methods, no optional features |
| DiaEditor | SED-002 | Plugins register via macro | **Compliant** — REGISTER_EDITOR_PLUGIN macro matches ApplicationTypeRegistry pattern |
| DiaEditor | SED-003 | Systems own their editors | **Compliant** — usage example shows DiaApplicationEditor in its own namespace |
| DiaEditor | SED-013 | Plugin data structs declare kPluginDataTypeId | **N/A** — discovery only, data access is EditorModel's concern |

**All binding decisions: COMPLIANT**

## Open Questions

**Resolved:**
- **Decision 43:** Static linking for Phase 5 (simpler, no DLL boundary issues); DLL optional in Phase 7+ for hot-reload
- **Decision 44:** Plugin version is informational only (display/logging); version enforcement deferred to Phase 7+ with DLL loading
- **Decision 45:** No hard plugin-to-plugin dependencies; cross-plugin coordination via observer paths (SED-011)
- **Decision 46:** Load order is manifest order for predictability; correctness must not depend on order

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Loading | DLL or static? | Static for Phase 5 (simpler); DLL in Phase 7+ for hot-reload | ✅ Static (Decision 43) |
| 2 | Versioning | How to check plugin compatibility? | Plugin reports version; editor checks against supported range | ✅ Informational only (Decision 44) — static linking guarantees build-time compatibility; GetVersion() for display/logging only. Version enforcement deferred to Phase 7+ with DLL loading |
| 3 | Dependencies | Plugin-to-plugin deps? | No for Phase 5; all plugins independent | ✅ No hard deps (Decision 45) — plugins are independent; cross-plugin coordination via observer paths (SED-011) |
| 4 | Order | Plugin load order matter? | No - plugins should be independent; load in manifest order | ✅ No (Decision 46) — load in manifest order for predictability, but correctness must not depend on order |

## Status

`Done` - All acceptance criteria met. Home panel migrated to IEditorPlugin as proof-of-concept.

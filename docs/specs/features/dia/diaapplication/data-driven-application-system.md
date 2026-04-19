# Feature Spec: Data-Driven Application System

## Metadata

| Field | Value |
|-------|-------|
| **Feature ID** | F-DiaApp-DataDriven |
| **Feature Name** | Data-Driven Application System |
| **Parent System** | @docs/specs/systems/dia/diaapplication.md |
| **Status** | Approved |
| **Owner** | TBD |
| **Created** | 2026-04-18 |
| **Updated** | 2026-04-18 |

## Traceability

| Level | ID | Name | Link |
|-------|-------|------|------|
| Platform | Cluiche | Cluiche Game Development Platform | @docs/specs/platform/Cluiche.md |
| Application | Dia | Dia Game Engine | @docs/specs/applications/dia.md |
| System | DiaApplication | Application Framework System | @docs/specs/systems/dia/diaapplication.md |
| Feature | F-DiaApp-DataDriven | Data-Driven Application System | (this document) |

## Purpose

Enables editor-driven definition of application architecture topology (ProcessingUnit composition, Phase ordering, Module connections) without recompilation, while behavior implementations remain code-driven. This feature externalizes the **structure and flow** of applications to `.diaapp` manifest files, allowing rapid iteration on application architecture and providing the foundation for a visual application editor.

## Problem Statement

Currently, application structure is entirely code-driven: developers must write C++ to instantiate ProcessingUnits, add Phases, register Modules, and define phase transitions. This requires:
- Recompilation for every structural change (adding a module, reordering phases, changing transitions)
- Manual coding for repetitive boilerplate (AddPhase, AddModule, AddPhaseTransition calls)
- No runtime visibility into application structure for debugging or tooling
- No path toward visual application editing

This feature separates **topology** (what connects to what) from **behavior** (what each component does), enabling data-driven architecture definition while preserving code-driven behavior implementations.

## Acceptance Criteria

- **AC1**: ProcessingUnits, Phases, and Modules can be serialized to/from JSON (structure only, not behavior)
- **AC2**: Type registration system allows runtime instantiation of existing C++ classes by string ID
- **AC3**: Applications can be loaded from `.diaapp` manifest files defining topology
- **AC4**: Manifest specifies which Phase types to instantiate and their execution order
- **AC5**: Manifest specifies which Module types to instantiate and their configuration parameters
- **AC6**: Validation layer checks manifest against registered types and dependencies
- **AC7**: Introspection API allows querying registered types and active instances (for editor)
- **AC8**: Manifest changes can be reloaded at runtime without restart (for iteration speed)
- **AC9**: Fallback behavior when manifest is missing/corrupt (e.g., default to code-defined topology)
- **AC11**: Phase transition rules can be defined in manifest (not just hard-coded state machines)
- **AC12**: Multiple manifest files can be composed/merged (e.g., base.diaapp + level.diaapp)
- **AC13**: Manifest schema includes versioning for forward/backward compatibility
- **AC17**: Clear error messages when manifest references non-existent types or violates dependencies

## Design

### Overview

The Data-Driven Application System adds three layers on top of existing DiaApplication infrastructure:

1. **Type Registration System** - Registers C++ classes (ProcessingUnit/Phase/Module subclasses) with string IDs for runtime lookup
2. **Serialization Layer** - Converts application structure to/from JSON manifests
3. **Manifest Loader** - Validates and instantiates applications from `.diaapp` files

**Key Principle:** This system handles **topology** (structure, connections, configuration), not **behavior** (Update() logic, custom algorithms). Behavior remains in C++ subclasses.

### Architecture

```
┌─────────────────────────────────────────────────────┐
│           .diaapp Manifest File (JSON)              │
│  ┌───────────────────────────────────────────────┐ │
│  │ - Processing Unit Types                       │ │
│  │ - Phase Types & Ordering                      │ │
│  │ - Module Types & Configuration                │ │
│  │ - Phase Transitions                           │ │
│  │ - Module Dependencies                         │ │
│  └───────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────┘
                        ↓
        ┌───────────────────────────────┐
        │   ApplicationManifestLoader   │
        │  - Load JSON                  │
        │  - Validate Schema            │
        │  - Validate Types             │
        │  - Check Dependencies         │
        └───────────────────────────────┘
                        ↓
        ┌───────────────────────────────┐
        │  ApplicationTypeRegistry      │
        │  - Lookup Factory by ID       │
        │  - Create Instance            │
        │  - Query Registered Types     │
        └───────────────────────────────┘
                        ↓
        ┌───────────────────────────────┐
        │     ProcessingUnit (Live)     │
        │  ┌─────────────────────────┐  │
        │  │ Phases (instantiated)   │  │
        │  │ Modules (instantiated)  │  │
        │  │ Transitions (configured)│  │
        │  └─────────────────────────┘  │
        └───────────────────────────────┘
```

### Component 1: Type Registration System

**Purpose:** Enable runtime instantiation of C++ classes by string ID.

**API Design:**

```cpp
namespace Dia::Application {

// Factory interface for creating registered types
template <typename T>
class ITypeFactory {
public:
    virtual ~ITypeFactory() = default;
    virtual T* Create(ProcessingUnit* pu, const StringCRC& id) = 0;
    virtual T* Create(ProcessingUnit* pu, const StringCRC& id, 
                      const Json::Value& config) = 0;
};

// Central registry for all ProcessingUnit/Phase/Module types
class ApplicationTypeRegistry {
public:
    static ApplicationTypeRegistry& Instance();
    
    // Registration (called by macros below)
    void RegisterProcessingUnitType(const StringCRC& typeId, 
                                     ITypeFactory<ProcessingUnit>* factory);
    void RegisterPhaseType(const StringCRC& typeId, 
                           ITypeFactory<Phase>* factory);
    void RegisterModuleType(const StringCRC& typeId, 
                            ITypeFactory<Module>* factory);
    
    // Instantiation
    ProcessingUnit* CreateProcessingUnit(const StringCRC& typeId, 
                                         const StringCRC& instanceId,
                                         const Json::Value& config = Json::Value());
    Phase* CreatePhase(const StringCRC& typeId, 
                       ProcessingUnit* pu,
                       const StringCRC& instanceId,
                       const Json::Value& config = Json::Value());
    Module* CreateModule(const StringCRC& typeId, 
                         ProcessingUnit* pu,
                         const StringCRC& instanceId,
                         const Json::Value& config = Json::Value());
    
    // Introspection (for editor)
    const DynamicArrayC<StringCRC>& GetRegisteredProcessingUnitTypes() const;
    const DynamicArrayC<StringCRC>& GetRegisteredPhaseTypes() const;
    const DynamicArrayC<StringCRC>& GetRegisteredModuleTypes() const;
    
    bool IsTypeRegistered(const StringCRC& typeId) const;
    
private:
    HashTable<StringCRC, ITypeFactory<ProcessingUnit>*> mProcessingUnitFactories;
    HashTable<StringCRC, ITypeFactory<Phase>*> mPhaseFactories;
    HashTable<StringCRC, ITypeFactory<Module>*> mModuleFactories;
};

} // namespace Dia::Application
```

**Registration Macros:**

```cpp
// In header file (MyProcessingUnit.h):
class MyProcessingUnit : public Dia::Application::ProcessingUnit {
public:
    static const StringCRC kTypeId;
    static const StringCRC kUniqueId; // Instance ID (already exists)
    
    MyProcessingUnit(const StringCRC& id, float hz = -1.0f);
    // ... existing methods ...
};

// In cpp file (MyProcessingUnit.cpp):
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>

const StringCRC MyProcessingUnit::kTypeId = StringCRC("MyProcessingUnit");

DIA_REGISTER_PROCESSING_UNIT(MyProcessingUnit) {
    return new MyProcessingUnit(instanceId, hz);
}

// Similar for Phase and Module:
DIA_REGISTER_PHASE(MyPhase) {
    return new MyPhase(pu, instanceId);
}

DIA_REGISTER_MODULE(MyModule) {
    return new MyModule(pu, instanceId, Module::RunningEnum::kUpdate);
}

// With configuration support:
DIA_REGISTER_MODULE_WITH_CONFIG(MyConfigurableModule) {
    float updateRate = config.get("updateRate", 60.0f).asFloat();
    return new MyConfigurableModule(pu, instanceId, updateRate);
}
```

**Implementation Notes:**
- Macros expand to factory classes and static initializers (similar to ComponentFactoryRegistry)
- Registration happens during static initialization (before main())
- Thread-safe: registration during startup, lookups during runtime
- Uses existing StringCRC system for type IDs

### Component 2: Serialization Layer

**Purpose:** Define JSON schema and serialization/deserialization logic.

**Manifest Schema (`.diaapp` format):**

```json
{
  "version": "1.0",
  "schema": "dia.application.manifest.v1",
  
  "processing_units": [
    {
      "type": "MainProcessingUnit",
      "instance_id": "MainPU",
      "thread_policy": "dedicated",
      "frequency_hz": 60.0,
      
      "config": {
        // Type-specific configuration passed to factory
      },
      
      "phases": [
        {
          "type": "InitPhase",
          "instance_id": "Init",
          "config": {}
        },
        {
          "type": "UpdatePhase",
          "instance_id": "Update",
          "config": {}
        },
        {
          "type": "RenderPhase",
          "instance_id": "Render",
          "config": {}
        },
        {
          "type": "ShutdownPhase",
          "instance_id": "Shutdown",
          "config": {}
        }
      ],
      
      "phase_transitions": [
        {"from": "Init", "to": "Update"},
        {"from": "Update", "to": "Update"},
        {"from": "Update", "to": "Shutdown"}
      ],
      
      "initial_phase": "Init",
      
      "modules": [
        {
          "type": "RenderModule",
          "instance_id": "Renderer",
          "phases": ["Update", "Render"],
          "config": {
            "vsync": true,
            "resolution": [1920, 1080]
          }
        },
        {
          "type": "PhysicsModule",
          "instance_id": "Physics",
          "phases": ["Update"],
          "dependencies": [],
          "config": {
            "gravity": [0, -9.8, 0]
          }
        },
        {
          "type": "InputModule",
          "instance_id": "Input",
          "phases": ["Update"],
          "dependencies": [],
          "config": {}
        }
      ],
      
      "module_dependencies": [
        {"module": "Renderer", "depends_on": "Input"}
      ]
    }
  ],
  
  "imports": [
    // AC12: Manifest composition
    "base_modules.diaapp",
    "level_specific.diaapp"
  ],
  
  "metadata": {
    // AC14 (future): Editor-specific data
    "editor_layout": {},
    "notes": "Main application manifest"
  }
}
```

**API Design:**

```cpp
namespace Dia::Application {

// Serializable application structure (intermediate representation)
struct ApplicationManifest {
    struct ModuleEntry {
        StringCRC typeId;
        StringCRC instanceId;
        DynamicArrayC<StringCRC> phaseIds;     // Which phases this module belongs to
        DynamicArrayC<StringCRC> dependencies; // Module dependencies
        Json::Value config;
    };
    
    struct PhaseEntry {
        StringCRC typeId;
        StringCRC instanceId;
        Json::Value config;
    };
    
    struct PhaseTransition {
        StringCRC fromPhase;
        StringCRC toPhase;
    };
    
    struct ProcessingUnitEntry {
        StringCRC typeId;
        StringCRC instanceId;
        float frequencyHz;
        bool dedicatedThread;
        Json::Value config;
        
        DynamicArrayC<PhaseEntry> phases;
        DynamicArrayC<PhaseTransition> transitions;
        StringCRC initialPhase;
        DynamicArrayC<ModuleEntry> modules;
    };
    
    unsigned int version;  // Schema version
    DynamicArrayC<ProcessingUnitEntry> processingUnits;
    DynamicArrayC<const char*> imports; // AC12: Composed manifests
    Json::Value metadata; // AC14 (future): Editor metadata
};

// Serialization interface (added to existing classes)
class ProcessingUnit : public StateObject {
public:
    // Existing methods...
    
    // NEW: Serialization support
    virtual void SerializeTopology(Json::Value& out) const;
    virtual bool DeserializeTopology(const Json::Value& in);
};

// Similar for Phase and Module
class Phase : public StateObject {
public:
    // Existing methods...
    virtual void SerializeConfig(Json::Value& out) const {}
    virtual bool DeserializeConfig(const Json::Value& in) { return true; }
};

class Module : public StateObject {
public:
    // Existing methods...
    virtual void SerializeConfig(Json::Value& out) const {}
    virtual bool DeserializeConfig(const Json::Value& in) { return true; }
};

} // namespace Dia::Application
```

**Implementation Notes:**
- Base classes provide default implementations (no-op for simple types)
- Subclasses override `SerializeConfig`/`DeserializeConfig` to handle custom parameters
- Uses existing `DiaCore/Json/` (jsoncpp) for JSON parsing
- Validation happens in ApplicationManifestLoader (see below)

### Component 3: Manifest Loader

**Purpose:** Load, validate, and instantiate applications from manifests.

**API Design:**

```cpp
namespace Dia::Application {

// Validation result codes
enum class ManifestValidationResult {
    kSuccess,
    kSchemaVersionUnsupported,
    kMissingRequiredField,
    kInvalidJSON,
    kUnknownType,              // Type not registered
    kDuplicateInstanceId,
    kCircularDependency,
    kInvalidPhaseTransition,
    kModuleMissingFromPhase,   // Module references non-existent phase
    kImportNotFound,           // AC12: Imported manifest not found
    kImportCycle               // AC12: Circular import
};

// Error context for AC17 (clear error messages)
struct ManifestValidationError {
    ManifestValidationResult code;
    const char* message;
    const char* context; // e.g., "processing_units[0].modules[2]"
    
    const char* ToString() const;
};

// Main loader class
class ApplicationManifestLoader {
public:
    ApplicationManifestLoader();
    
    // Load from file
    ManifestValidationResult LoadFromFile(const char* filePath,
                                          ApplicationManifest& outManifest);
    
    // Load from JSON string
    ManifestValidationResult LoadFromString(const char* jsonString,
                                            ApplicationManifest& outManifest);
    
    // Validation (without instantiation)
    ManifestValidationResult Validate(const ApplicationManifest& manifest);
    
    // Instantiation
    ProcessingUnit* Instantiate(const ApplicationManifest::ProcessingUnitEntry& entry);
    
    // AC12: Manifest composition
    ManifestValidationResult ComposeManifests(
        const DynamicArrayC<const char*>& filePaths,
        ApplicationManifest& outComposedManifest);
    
    // AC8: Hot reload
    ManifestValidationResult ReloadManifest(const char* filePath,
                                            ProcessingUnit* existingPU);
    
    // Error reporting (AC17)
    const DynamicArrayC<ManifestValidationError>& GetErrors() const;
    void ClearErrors();
    
private:
    DynamicArrayC<ManifestValidationError> mErrors;
    
    // Validation helpers
    bool ValidateSchema(const Json::Value& root);
    bool ValidateTypes(const ApplicationManifest& manifest);
    bool ValidateDependencies(const ApplicationManifest::ProcessingUnitEntry& entry);
    bool ValidatePhaseTransitions(const ApplicationManifest::ProcessingUnitEntry& entry);
    bool DetectImportCycles(const DynamicArrayC<const char*>& imports, 
                            const char* currentFile);
};

// High-level application loader
class ApplicationLoader {
public:
    // AC3: Load entire application from manifest
    static ProcessingUnit* LoadApplication(const char* manifestPath,
                                           ManifestValidationResult& outResult);
    
    // AC9: Fallback to code-defined structure
    static ProcessingUnit* LoadApplicationWithFallback(
        const char* manifestPath,
        ProcessingUnit* (*fallbackFactory)());
};

} // namespace Dia::Application
```

**Validation Rules:**
1. **Schema Version:** Check `version` field matches supported versions (AC13)
2. **Type Existence:** All `type` fields reference registered types (AC2, AC17)
3. **Instance ID Uniqueness:** No duplicate `instance_id` within a ProcessingUnit (AC17)
4. **Circular Dependencies:** Detect cycles in `module_dependencies` (AC17)
5. **Phase References:** Modules' `phases` arrays reference valid phase IDs (AC17)
6. **Transition Validity:** `phase_transitions` only use defined phase IDs (AC11, AC17)
7. **Import Resolution:** `imports` files exist and don't create cycles (AC12, AC17)

**Error Messages (AC17):**
```
Error: Unknown module type 'NonExistentModule'
  Context: processing_units[0].modules[2].type
  Hint: Check ApplicationTypeRegistry for registered module types

Error: Circular dependency detected
  Context: processing_units[0].module_dependencies
  Cycle: PhysicsModule → RenderModule → InputModule → PhysicsModule
  
Error: Phase 'UpdatePhase' not found in processing unit
  Context: processing_units[0].modules[1].phases[0]
  Available phases: InitPhase, RenderPhase, ShutdownPhase
```

### Component 4: Introspection API

**Purpose:** Query runtime state for editor integration (AC7).

**API Design:**

```cpp
namespace Dia::Application {

// Runtime introspection for active applications
class ApplicationIntrospector {
public:
    explicit ApplicationIntrospector(ProcessingUnit* pu);
    
    // Query structure
    const DynamicArrayC<Phase*>& GetPhases() const;
    const DynamicArrayC<Module*>& GetModules() const;
    Phase* GetCurrentPhase() const;
    
    // Query topology
    struct PhaseTransitionInfo {
        StringCRC fromPhase;
        StringCRC toPhase;
    };
    const DynamicArrayC<PhaseTransitionInfo>& GetTransitions() const;
    
    struct ModulePlacement {
        Module* module;
        DynamicArrayC<Phase*> phases;
    };
    const DynamicArrayC<ModulePlacement>& GetModulePlacements() const;
    
    // Query dependencies
    const DynamicArrayC<Module*>& GetModuleDependencies(Module* module) const;
    
    // Export current topology to manifest (for editor "save")
    void ExportToManifest(ApplicationManifest& outManifest) const;
    
private:
    ProcessingUnit* mProcessingUnit;
};

} // namespace Dia::Application
```

**Use Cases:**
- Editor queries registered types via `ApplicationTypeRegistry::GetRegisteredModuleTypes()`
- Editor queries active topology via `ApplicationIntrospector`
- Editor modifies manifest, calls `ApplicationManifestLoader::Validate()` before save
- Editor triggers hot reload via `ApplicationManifestLoader::ReloadManifest()`

### Component 5: Hot Reload Support (AC8)

**Purpose:** Reload manifest without restarting application.

**Design Constraints:**
- Must preserve module state where possible (use existing HotReloadManager)
- Phase transitions only during safe points (not mid-Update)
- Validation must pass before applying changes (rollback on failure)

**API Design:**

```cpp
namespace Dia::Application {

class ApplicationManifestLoader {
public:
    // AC8: Hot reload implementation
    ManifestValidationResult ReloadManifest(const char* filePath,
                                            ProcessingUnit* existingPU) {
        // 1. Load new manifest
        ApplicationManifest newManifest;
        auto result = LoadFromFile(filePath, newManifest);
        if (result != ManifestValidationResult::kSuccess) {
            return result; // AC9: Keep existing on failure
        }
        
        // 2. Validate before applying
        result = Validate(newManifest);
        if (result != ManifestValidationResult::kSuccess) {
            return result; // AC9: Keep existing on failure
        }
        
        // 3. Diff against current structure
        ApplicationManifest currentManifest;
        ApplicationIntrospector inspector(existingPU);
        inspector.ExportToManifest(currentManifest);
        
        auto diff = ComputeDiff(currentManifest, newManifest);
        
        // 4. Apply changes:
        //    - Remove modules/phases no longer present
        //    - Add new modules/phases
        //    - Update configuration for existing modules
        //    - Rebuild phase transitions
        
        return ManifestValidationResult::kSuccess;
    }
};

} // namespace Dia::Application
```

**Hot Reload Process:**
1. Queue phase transition to safe state (e.g., paused phase)
2. Stop modules that will be removed
3. Load new manifest and validate
4. Apply structural changes (add/remove phases/modules)
5. Restart modules with new configuration
6. Resume execution

**Limitations:**
- Cannot hot reload ProcessingUnit itself (only phases/modules within)
- State preservation only for modules supporting it (via existing HotReloadManager)
- Editor responsible for triggering reload at safe times

### Component 6: Manifest Composition (AC12)

**Purpose:** Compose multiple manifest files (e.g., base + level-specific).

**Merge Rules:**
1. **Processing Units:** Merged by `instance_id` (later files override earlier)
2. **Phases:** Added if new `instance_id`, merged if existing (config overridden)
3. **Modules:** Added if new `instance_id`, merged if existing (config deep-merged)
4. **Phase Transitions:** Union of all transitions (duplicates ignored)
5. **Imports:** Resolved recursively (depth-first, cycle detection)

**Example:**

`base.diaapp`:
```json
{
  "version": "1.0",
  "processing_units": [{
    "instance_id": "MainPU",
    "phases": [{"instance_id": "Init", ...}, {"instance_id": "Update", ...}],
    "modules": [
      {"instance_id": "Renderer", "config": {"vsync": true}}
    ]
  }]
}
```

`level1.diaapp`:
```json
{
  "version": "1.0",
  "imports": ["base.diaapp"],
  "processing_units": [{
    "instance_id": "MainPU",
    "modules": [
      {"instance_id": "Renderer", "config": {"resolution": [1920, 1080]}},
      {"instance_id": "Level1AI", "phases": ["Update"], "config": {...}}
    ]
  }]
}
```

**Composed Result:**
```json
{
  "version": "1.0",
  "processing_units": [{
    "instance_id": "MainPU",
    "phases": [{"instance_id": "Init", ...}, {"instance_id": "Update", ...}],
    "modules": [
      {"instance_id": "Renderer", "config": {"vsync": true, "resolution": [1920, 1080]}},
      {"instance_id": "Level1AI", "phases": ["Update"], "config": {...}}
    ]
  }]
}
```

### Component 7: Versioning (AC13)

**Purpose:** Support forward/backward compatibility as schema evolves.

**Version Field:**
```json
{
  "version": "1.0",
  "schema": "dia.application.manifest.v1"
}
```

**Supported Versions:**
- `1.0` - Initial version (this feature)
- Future versions handled by migration logic in `ApplicationManifestLoader`

**Migration Strategy:**
```cpp
class ApplicationManifestLoader {
private:
    bool MigrateSchema(Json::Value& manifest, 
                       const char* fromVersion, 
                       const char* toVersion) {
        // Future: convert old schema to new schema
        // E.g., "1.0" → "1.1" adds new fields with defaults
        return true;
    }
};
```

**Validation:**
- Loader checks `version` field on load
- If version > current, reject with `kSchemaVersionUnsupported`
- If version < current, attempt migration (if migration path exists)
- If migration fails, reject with error

## Files Modified / Created

### New Files

**Type Registry:**
- `Dia/DiaApplication/TypeRegistry/ApplicationTypeRegistry.h` - Registry interface
- `Dia/DiaApplication/TypeRegistry/ApplicationTypeRegistry.cpp` - Registry implementation
- `Dia/DiaApplication/TypeRegistry/RegistrationMacros.h` - DIA_REGISTER_* macros

**Manifest System:**
- `Dia/DiaApplication/Manifest/ApplicationManifest.h` - Manifest data structures
- `Dia/DiaApplication/Manifest/ApplicationManifestLoader.h` - Loader interface
- `Dia/DiaApplication/Manifest/ApplicationManifestLoader.cpp` - Loader implementation
- `Dia/DiaApplication/Manifest/ManifestValidator.h` - Validation logic
- `Dia/DiaApplication/Manifest/ManifestValidator.cpp` - Validation implementation
- `Dia/DiaApplication/Manifest/ManifestComposer.h` - Composition logic (AC12)
- `Dia/DiaApplication/Manifest/ManifestComposer.cpp` - Composition implementation

**Introspection:**
- `Dia/DiaApplication/Introspection/ApplicationIntrospector.h` - Introspection interface
- `Dia/DiaApplication/Introspection/ApplicationIntrospector.cpp` - Introspection implementation

**Application Loader:**
- `Dia/DiaApplication/Loader/ApplicationLoader.h` - High-level application loader (AC3, AC9)
- `Dia/DiaApplication/Loader/ApplicationLoader.cpp` - Implementation

### Modified Files

**Core Classes (add serialization interface):**
- `Dia/DiaApplication/ProcessingUnit.h` - Add `SerializeTopology()` / `DeserializeTopology()`
- `Dia/DiaApplication/ProcessingUnit.cpp` - Implement serialization
- `Dia/DiaApplication/Phase.h` - Add `SerializeConfig()` / `DeserializeConfig()`
- `Dia/DiaApplication/Phase.cpp` - Default implementations
- `Dia/DiaApplication/Module.h` - Add `SerializeConfig()` / `DeserializeConfig()`
- `Dia/DiaApplication/Module.cpp` - Default implementations

**Project Files:**
- `Dia/DiaApplication/DiaApplication.vcxproj` - Add new files
- `Dia/DiaApplication/DiaApplication.vcxproj.filters` - Add filters for TypeRegistry, Manifest, Introspection folders

**Module Documentation:**
- `Dia/DiaApplication/dia.application.architecture.module.md` - Update with new public API

### Example Usage Files

**Examples (create in CluicheTest):**
- `Cluiche/CluicheTest/Examples/data_driven_app_example.cpp` - Shows manifest-based loading
- `Cluiche/CluicheTest/ApplicationFlow/main_application.diaapp` - Example manifest for CluicheTest

## Dependencies

**Required:**
- **DiaCore/Json** - JSON parsing (jsoncpp wrapper)
- **DiaCore/Containers** - DynamicArrayC, HashTable for storage
- **DiaCore/CRC** - StringCRC for type IDs and instance IDs
- **DiaCore/Type** - Potentially for type metadata (optional)
- **DiaCore/FilePath** - File loading for manifests
- **DiaCore/Core** - Assertions for validation

**Optional:**
- **DiaCore/Type/TypeDef.h** - If we want deeper type introspection (e.g., parameter metadata)

## Testing Strategy

**Unit Tests (GoogleTests project):**

1. **Type Registration:**
   - Test registration macros create factories correctly
   - Test duplicate registration detection
   - Test type lookup (registered vs unregistered)
   - Test instantiation with/without config

2. **Manifest Loading:**
   - Test valid manifest parsing
   - Test invalid JSON rejection
   - Test missing required fields
   - Test unknown type rejection (AC17)
   - Test schema version validation (AC13)

3. **Validation:**
   - Test circular dependency detection (AC17)
   - Test invalid phase transition detection (AC11, AC17)
   - Test duplicate instance ID detection (AC17)
   - Test module-phase reference validation (AC17)

4. **Composition (AC12):**
   - Test import resolution
   - Test config merging
   - Test cycle detection in imports
   - Test missing import error (AC17)

5. **Introspection (AC7):**
   - Test querying active topology
   - Test export to manifest (round-trip)

6. **Hot Reload (AC8):**
   - Test adding new module
   - Test removing module
   - Test updating module config
   - Test validation failure rollback (AC9)

7. **Fallback (AC9):**
   - Test missing manifest fallback
   - Test corrupt JSON fallback
   - Test validation failure fallback

**Integration Tests (CluicheTest):**

1. **End-to-End Application Loading:**
   - Create `.diaapp` manifest for CluicheTest MainProcessingUnit
   - Load via `ApplicationLoader::LoadApplication()`
   - Verify all phases and modules instantiated correctly
   - Run application and verify behavior unchanged

2. **Editor Workflow Simulation:**
   - Query registered types via `ApplicationTypeRegistry`
   - Load manifest, modify, validate, save
   - Trigger hot reload and verify changes applied

3. **Performance:**
   - Measure manifest load time (should be < 100ms for typical app)
   - Measure hot reload time (should be < 500ms for typical changes)

## Open Questions

| # | Question | Impact | Proposed Resolution |
|---|----------|--------|---------------------|
| 1 | Should module configuration parameters be validated against a schema (beyond JSON structure)? | Medium - Affects AC10 (originally suggested, now removed). Without validation, invalid config detected at runtime in module constructor. | Not in initial implementation. Module constructors validate their own config and report errors via ErrorCallback. Future enhancement: add parameter metadata to ITypeFactory for pre-validation. |
| 2 | How should hot reload handle modules with non-serializable state (e.g., GPU resources)? | High - Affects AC8 success rate. Some modules cannot preserve state across reload. | Use existing `HotReloadManager::CanHotReload()` pattern. Modules opt-in to hot reload; others require full restart. Document limitations clearly. |
| 3 | Should phase transitions support conditional expressions in manifest (e.g., "Update → Shutdown if error count > 10")? | Low - AC11 specifies transition topology, not conditions. Conditions are behavior (code-driven). | No - manifests define **which** transitions are valid, not **when** they occur. Transition logic remains in Phase::FlaggedToStopUpdating() and code. |
| 4 | Should ApplicationLoader automatically register types, or require explicit registration? | Low - Affects convenience vs explicitness tradeoff. | Explicit registration via macros (same pattern as ComponentFactoryRegistry). Auto-registration (e.g., via reflection) adds complexity and magic. |
| 5 | What happens if hot reload is triggered while ProcessingUnit is in the middle of Update()? | High - Thread safety and corruption risk. | Queue reload request via MessageBus or flag; apply during safe phase transition point. Document that `ReloadManifest()` must be called when PU is idle or in safe phase. |
| 6 | Should manifests support comments (JSON doesn't natively)? | Low - Developer experience consideration. | Use `metadata` field for notes. Future: support JSON5 or YAML as alternative format. |
| 7 | How are module dependency resolution order and startup order specified? | Medium - Affects module startup sequencing. | Implicit from dependency graph (topological sort during BuildDependancies). Manifest specifies **what** depends on **what**, not explicit order. |

## Tasks

| # | Task | Estimated Effort | Dependencies | Assignee |
|---|------|------------------|--------------|----------|
| 1 | Implement ApplicationTypeRegistry and registration macros | 3 days | None | TBD |
| 2 | Add serialization interface to ProcessingUnit/Phase/Module base classes | 2 days | None | TBD |
| 3 | Implement ApplicationManifest data structures | 1 day | None | TBD |
| 4 | Implement ManifestValidator (type checking, dependency validation) | 4 days | Task 1, 3 | TBD |
| 5 | Implement ApplicationManifestLoader (JSON parsing, validation, instantiation) | 4 days | Task 1, 3, 4 | TBD |
| 6 | Implement ManifestComposer (AC12 - import resolution and merging) | 3 days | Task 3, 5 | TBD |
| 7 | Implement ApplicationIntrospector (AC7 - query runtime topology) | 2 days | None | TBD |
| 8 | Implement ApplicationLoader (AC3, AC9 - high-level API with fallback) | 2 days | Task 5, 6 | TBD |
| 9 | Implement hot reload support (AC8) | 5 days | Task 5, 7 | TBD |
| 10 | Write unit tests for type registry | 2 days | Task 1 | TBD |
| 11 | Write unit tests for manifest loading and validation | 3 days | Task 4, 5 | TBD |
| 12 | Write unit tests for manifest composition | 2 days | Task 6 | TBD |
| 13 | Write unit tests for hot reload | 3 days | Task 9 | TBD |
| 14 | Create example .diaapp manifest for CluicheTest | 1 day | Task 5, 8 | TBD |
| 15 | Write integration test for end-to-end application loading | 2 days | Task 8, 14 | TBD |
| 16 | Update DiaApplication module documentation | 1 day | All tasks | TBD |
| 17 | Write developer guide and examples | 2 days | All tasks | TBD |

**Total Estimated Effort:** ~41 days (~8 weeks for single developer, ~4 weeks with pair)

**Critical Path:** Task 1 → Task 4 → Task 5 → Task 9 (Type Registry → Validation → Loading → Hot Reload)

## Success Metrics

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| Manifest Load Time | < 100ms for typical app (5 PUs, 10 phases, 20 modules) | Unit test with timer |
| Hot Reload Time | < 500ms for typical change (add/remove/update 1-2 modules) | Integration test with timer |
| Error Message Clarity | 100% of validation errors include context and actionable hint | Code review + user testing |
| Type Registration Overhead | < 1ms startup time for 100 registered types | Unit test with timer |
| Test Coverage | > 90% line coverage for new code | Code coverage tool |
| Backward Compatibility | 100% of v1.0 manifests valid in future versions | Regression test suite |

## Binding Decisions Compliance

| Parent Decision | Summary | Compliance | Notes |
|-----------------|---------|------------|-------|
| **PD-001** | Use StringCRC for all entity/component IDs | ✅ Compliant | Type IDs and instance IDs both use StringCRC. `ApplicationTypeRegistry` uses StringCRC keys. |
| **PD-002** | ProcessingUnit/Phase/Module architecture for app structure | ✅ Compliant | This feature enhances PD-002 by making it data-driven while preserving the architecture. No structural changes to the hierarchy. |
| **PD-003** | Component-based entities (IComponent/IComponentObject) | ✅ N/A | This feature is orthogonal to component system. Modules can still use components internally. |
| **PD-004** | No STL containers in public APIs | ✅ Compliant | All public APIs use DiaCore containers (DynamicArrayC, HashTable). Internal use of std::function in factory lambdas is acceptable (not part of public API surface). Json::Value is existing exception already used in DiaCore/Json. |
| **PD-006** | Visual Studio project files are source of truth | ✅ Compliant | All new files added to DiaApplication.vcxproj and .vcxproj.filters manually. |
| **AD-001** | Module system with YAML frontmatter documentation | ✅ Compliant | Will update dia.application.architecture.module.md with new public API entries. |
| **AD-002** | No STL containers in public APIs | ✅ Compliant | Reinforces PD-004. Uses DynamicArrayC, HashTable throughout. |
| **AD-003** | Namespace convention: `Dia::<Module>::` | ✅ Compliant | All new code in `Dia::Application::` namespace. |
| **AD-004** | ProcessingUnit/Phase/Module for application structure | ✅ Compliant | This feature IS the data-driven implementation of AD-004. Preserves existing architecture. |
| **SD-001** | ProcessingUnit/Phase/Module three-level hierarchy | ✅ Compliant | Manifests preserve the three-level hierarchy. No structural changes. |
| **SD-002** | StateObject base class with explicit state machine | ✅ Compliant | Serialization does not affect state machine. Manifests define structure; state transitions remain code-driven. |
| **SD-003** | QueuePhaseTransition() thread-safe, TransitionPhase() immediate | ✅ Compliant | Hot reload uses QueuePhaseTransition() for thread safety (see AC8 design). |
| **SD-004** | Modules identified by StringCRC with template GetModule<T>() | ✅ Compliant | Manifest `instance_id` fields map to StringCRC. Existing GetModule<T>() API unchanged. |
| **SD-005** | Phases define module dependencies, not vice versa | ✅ Compliant | Manifest `modules[].phases` array specifies which phases contain each module (phase defines membership). |
| **SD-006** | Support both raw pointer and UniquePtr module/phase management | ⚠️ Partial | Initial implementation creates modules/phases with internal ownership (UniquePtr). External ownership support deferred to future iteration (requires lifetime management in manifest). |
| **SD-007** | Message bus uses type-erased void* with size tracking | ✅ N/A | Feature does not modify MessageBus. |
| **SD-008** | Error history limited to 100 most recent errors | ✅ N/A | Feature does not modify error history system. |
| **SD-009** | Hot reload requires module opt-in (CanHotReload) | ✅ Compliant | Hot reload design uses existing HotReloadManager; respects CanHotReload opt-in (see AC8 notes). |
| **SD-010** | No automatic module dependency resolution - explicit AddDependancy() | ✅ Compliant | Manifest `module_dependencies` array explicitly lists dependencies. No automatic inference. |

**Compliance Summary:**
- ✅ **Compliant:** 18 decisions
- ⚠️ **Partial:** 1 decision (SD-006 - external ownership deferred)
- ❌ **Conflict:** 0 decisions

**SD-006 Partial Compliance Note:**
The initial implementation focuses on internal ownership (manifests create and own modules/phases). External ownership (manifests reference externally-created objects) is a future enhancement that requires additional lifetime management semantics in the manifest schema. This is acceptable for the initial feature as internal ownership covers the primary use case (editor-driven application creation).

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Design | Should type registration happen via static initialization (macros) or explicit registration in main()? | Static initialization (same as ComponentFactoryRegistry) - automatic and consistent | Static initialization - Automatic via macros, consistent with ComponentFactoryRegistry pattern. |
| 2 | Design | Should manifests support "abstract" module types that must be subclassed (vs only concrete types)? | No - only concrete instantiable types. Abstraction handled via inheritance in code, not manifests | Concrete types only - Manifests reference instantiable classes. Abstraction and polymorphism remain code concerns. |
| 3 | Serialization | Should ProcessingUnit frequency (hz) be configurable in manifest? | Yes - add `frequency_hz` field to ProcessingUnitEntry (already in proposed schema) | Yes - frequency_hz field in manifest (already in schema). |
| 4 | Serialization | Should phase transition conditions be scriptable (e.g., Python expressions)? | No - too complex for initial implementation. Transitions defined in manifest (topology), conditions in code (behavior) | No - manifest defines which transitions are valid (topology), not when they occur (behavior stays in code). |
| 5 | Validation | Should validator check for "orphaned" modules (not in any phase)? | Yes - warn (not error) if module not referenced by any phase. May be intentional for conditional loading | Yes - warn (not error) if module has empty phases array. May be intentional for manual addition later. |
| 6 | Hot Reload | Should hot reload preserve MessageBus subscriptions? | No - modules re-subscribe during Start(). Hot reload is equivalent to Stop/Start cycle | No - modules re-subscribe during Start(). Hot reload equals Stop/Start cycle for subscriptions. |
| 7 | Hot Reload | Should hot reload trigger via file watcher (automatic) or manual API call? | Manual API call initially. Automatic file watcher is editor feature (separate concern) | Manual API call - ReloadManifest() called explicitly. File watching is editor/tool concern, not framework. |
| 8 | Composition | Should manifest imports support glob patterns (e.g., "levels/*.diaapp")? | Not initially - explicit file paths only. Glob expansion is build-time concern | Not initially - explicit file paths only. Glob expansion can be added later if needed. |
| 9 | Composition | What happens if imported manifests have conflicting schema versions? | Error - all composed manifests must have compatible versions. Loader should migrate if possible | Error if incompatible - all manifests must have compatible versions. Attempt migration if path exists, otherwise fail. |
| 10 | Introspection | Should introspection API expose module state (kRunning, kNotRunning)? | Yes - add `GetState()` passthrough to ApplicationIntrospector. Useful for editor status display | Yes - ApplicationIntrospector exposes StateObject::GetState() for phases/modules. Useful for editor status. |
| 11 | Error Handling | Should manifest validation errors stop at first error or collect all errors? | Collect all errors (similar to compiler). Better developer experience. `GetErrors()` returns array | Collect all errors - Validator continues after errors, returns full list via GetErrors(). Better dev experience. |
| 12 | Testing | Should CluicheTest be converted to use .diaapp manifest by default? | Not initially - keep code-based version as fallback test. Add separate manifest-based example | Not initially - CluicheTest stays code-based. Add separate example showing manifest-based loading. |
| 13 | Performance | Should loader cache parsed manifests (if loaded multiple times)? | Not initially - premature optimization. Manifests loaded once at startup or on explicit reload | No - no caching. Manifests loaded on demand. Can add caching later if profiling shows need. |
| 14 | Versioning | Should validator reject newer schema versions or attempt best-effort parsing? | Reject - safer than silent data loss. Users must migrate manifests or update code | Reject with clear error log - kSchemaVersionUnsupported result. Report supported vs found version. |
| 15 | Dependencies | Should module dependencies support version constraints (e.g., "requires PhysicsModule v2.0+")? | No - no module versioning system exists yet. Future enhancement if needed | No - no module versioning system. Simple type-based dependencies only. Version constraints if needed later. |

## Related Specs / References

**Parent Specs:**
- @docs/specs/systems/dia/diaapplication.md - Parent system spec
- @docs/specs/applications/dia.md - Dia application spec
- @docs/specs/platform/Cluiche.md - Platform spec

**Related Features:**
- [hot-reload.md](hot-reload.md) - Existing hot reload system (AC8 builds on this)
- [module-system.md](module-system.md) - Module dependencies (validation in AC6)
- [phase-management.md](phase-management.md) - Phase transitions (AC11 externalizes this)

**Referenced Systems:**
- DiaCore/Json - JSON parsing (jsoncpp)
- DiaCore/Containers - DynamicArrayC, HashTable
- DiaCore/CRC - StringCRC for IDs
- DiaCore/Type - Potential type metadata integration

**External Inspiration:**
- Unity Prefabs/SceneGraph - Data-driven scene composition
- Unreal Engine Blueprints - Visual scripting (future editor goal)
- Entity Component System (ECS) - Data-driven entity composition (similar pattern at different level)

## Status Log

| Date | Status | Notes |
|------|--------|-------|
| 2026-04-18 | Draft | Initial spec created after user interview |

# Data-Driven Application System

## Overview

The Data-Driven Application System enables editor-driven definition of application architecture topology (ProcessingUnit composition, Phase ordering, Module connections) without recompilation. This system externalizes the **structure and flow** of applications to `.diaapp` manifest files while keeping **behavior implementation** in C++ code.

## Key Principle

**Topology (Data) + Behavior (Code) = Complete Application**

- **Topology (Data)**: What connects to what, phase order, module configuration → `.diaapp` manifest
- **Behavior (Code)**: What each component does, Update() logic → C++ classes

## Components

### 1. ApplicationTypeRegistry

Central registry for all ProcessingUnit/Phase/Module types. Enables runtime instantiation by string ID.

**Files:**
- `TypeRegistry/ApplicationTypeRegistry.h/cpp`
- `TypeRegistry/RegistrationMacros.h`

**Usage:**
```cpp
// In YourModule.cpp
#include <DiaApplicationFlow/TypeRegistry/RegistrationMacros.h>

const StringCRC YourModule::kTypeId = StringCRC("YourModule");

DIA_REGISTER_MODULE(YourModule) {
    // config parameter contains JSON configuration from manifest
    float updateRate = config.get("updateRate", 60.0f).asFloat();
    return new YourModule(pu, instanceId, updateRate);
}
```

**Introspection:**
```cpp
auto& registry = ApplicationTypeRegistry::Instance();
const auto& moduleTypes = registry.GetRegisteredModuleTypes();
// Returns: DynamicArrayC<StringCRC> of all registered module type IDs
```

### 2. ApplicationManifest

Data structures representing manifest contents (intermediate representation between JSON and runtime objects).

**Files:**
- `Manifest/ApplicationManifest.h/cpp`

**Structure:**
- `ApplicationManifest` - Top-level manifest
  - `ProcessingUnitEntry[]` - ProcessingUnit definitions
    - `PhaseEntry[]` - Phase definitions
    - `PhaseTransition[]` - Valid transitions
    - `ModuleEntry[]` - Module definitions
    - `config` - JSON configuration

### 3. ManifestValidator

Validates manifests against registered types and dependency rules.

**Files:**
- `Manifest/ManifestValidator.h/cpp`

**Validation Rules:**
- Schema version must be supported (currently: `1.0`)
- All type IDs must be registered in ApplicationTypeRegistry
- Instance IDs must be unique within a ProcessingUnit
- Module dependencies must not have cycles
- Phase transitions must reference valid phase IDs
- Module phase references must be valid
- Warns if module has empty phases array (orphaned)

**Error Reporting:**
```cpp
ManifestValidator validator;
ManifestValidationResult result = validator.Validate(manifest);

if (result != ManifestValidationResult::kSuccess) {
    const auto& errors = validator.GetErrors();
    for (unsigned int i = 0; i < errors.Size(); ++i) {
        const auto& error = errors[i];
        DIA_LOG("Error [%s] %s: %s",
            error.GetResultString(error.code),
            error.context,
            error.message);
    }
}
```

### 4. ApplicationManifestLoader

Loads, validates, and instantiates applications from `.diaapp` JSON files.

**Files:**
- `Manifest/ApplicationManifestLoader.h/cpp`

**API:**
```cpp
ApplicationManifestLoader loader;

// Load from file
ApplicationManifest manifest;
ManifestValidationResult result = loader.LoadFromFile("app.diaapp", manifest);

// Instantiate ProcessingUnit
ProcessingUnit* app = loader.Instantiate(manifest.processingUnits[0]);
```

### 5. ManifestComposer

Merges multiple manifest files (supports imports).

**Files:**
- `Manifest/ManifestComposer.h/cpp`

**Merge Rules (AC12):**
1. **ProcessingUnits**: Merged by `instance_id` (later overrides earlier)
2. **Phases**: Added if new `instance_id`, merged if existing (config overridden)
3. **Modules**: Added if new `instance_id`, merged if existing (config deep-merged)
4. **Phase Transitions**: Union of all transitions (duplicates ignored)
5. **Imports**: Resolved recursively with cycle detection

**Example:**
```json
{
  "version": "1.0",
  "imports": ["base.diaapp", "level1_modules.diaapp"],
  "processing_units": [...]
}
```

### 6. ApplicationIntrospector

Queries runtime application topology (for editor integration).

**Files:**
- `Introspection/ApplicationIntrospector.h/cpp`

**API:**
```cpp
ApplicationIntrospector inspector(processingUnit);

// Query structure
const auto& phases = inspector.GetPhases();
const auto& modules = inspector.GetModules();
Phase* currentPhase = inspector.GetCurrentPhase();

// Query topology
const auto& transitions = inspector.GetTransitions();
const auto& placements = inspector.GetModulePlacements();

// Query state
StateObject::StateEnum state = inspector.GetModuleState(module);

// Export to manifest (for editor "save")
ApplicationManifest manifest;
inspector.ExportToManifest(manifest);
```

### 7. ApplicationLoader

High-level API for loading applications from manifests.

**Files:**
- `Loader/ApplicationLoader.h/cpp`

**API:**
```cpp
// Simple loading (automatic error logging)
ProcessingUnit* app = ApplicationLoader::LoadApplication("app.diaapp");

// With explicit error handling
ManifestValidationResult result;
ProcessingUnit* app = ApplicationLoader::LoadApplication("app.diaapp", result);

// With fallback to code-defined structure (AC9)
auto fallbackFactory = []() { return new MainProcessingUnit(...); };
ProcessingUnit* app = ApplicationLoader::LoadApplicationWithFallback(
    "app.diaapp",
    fallbackFactory
);
```

### 8. Hot Reload Support

Runtime manifest reloading without restart (AC8).

**API:**
```cpp
ApplicationManifestLoader loader;
ManifestValidationResult result = loader.ReloadManifest("app.diaapp", existingPU);

if (result == ManifestValidationResult::kSuccess) {
    // Modules added/removed/updated
    // Phase transitions rebuilt
}
```

**Limitations:**
- Cannot hot reload ProcessingUnit itself (only phases/modules within)
- State preservation only for modules supporting it (via existing HotReloadManager)
- Phase reloading not yet implemented (logged as TODO)

## Manifest Format (.diaapp)

### Schema Version 1.0

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
        // Type-specific configuration
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
        }
      ],

      "phase_transitions": [
        {"from": "Init", "to": "Update"},
        {"from": "Update", "to": "Update"}
      ],

      "initial_phase": "Init",

      "modules": [
        {
          "type": "RenderModule",
          "instance_id": "Renderer",
          "phases": ["Update"],
          "config": {
            "vsync": true,
            "resolution": [1920, 1080]
          }
        }
      ],

      "module_dependencies": [
        {"module": "Renderer", "depends_on": "Input"}
      ]
    }
  ],

  "imports": [
    "base_modules.diaapp",
    "level_specific.diaapp"
  ],

  "metadata": {
    // Editor-specific data (optional, ignored by runtime)
  }
}
```

## Usage Workflow

### 1. Define Types in Code

```cpp
// MyModule.h
class MyModule : public Module {
public:
    static const StringCRC kTypeId;
    MyModule(ProcessingUnit* pu, const StringCRC& id, float rate);
    
    virtual void SerializeConfig(Json::Value& out) const override;
    virtual void DeserializeConfig(const Json::Value& in) override;
};

// MyModule.cpp
const StringCRC MyModule::kTypeId = StringCRC("MyModule");

DIA_REGISTER_MODULE(MyModule) {
    float rate = config.get("rate", 30.0f).asFloat();
    return new MyModule(pu, instanceId, rate);
}

void MyModule::DeserializeConfig(const Json::Value& in) {
    if (in.isMember("rate")) {
        mRate = in["rate"].asFloat();
    }
}
```

### 2. Create Manifest File

```json
{
  "version": "1.0",
  "processing_units": [{
    "type": "MainProcessingUnit",
    "instance_id": "MainPU",
    "modules": [
      {
        "type": "MyModule",
        "instance_id": "MyMod",
        "phases": ["Update"],
        "config": {"rate": 60.0}
      }
    ]
  }]
}
```

### 3. Load Application

```cpp
ProcessingUnit* app = ApplicationLoader::LoadApplication("my_app.diaapp");
app->Start(nullptr);
// ... run application ...
app->Stop();
delete app;
```

## Examples

See `Cluiche/CluicheTest/Examples/DataDrivenApplicationExample.cpp` for comprehensive examples:

1. **Example1_LoadFromManifest** - Basic manifest loading
2. **Example2_LoadWithFallback** - Fallback to code-defined structure
3. **Example3_Introspection** - Query runtime topology
4. **Example4_HotReload** - Runtime manifest reloading
5. **Example5_QueryRegisteredTypes** - Introspection API

Example manifest: `Cluiche/CluicheTest/ApplicationFlow/example_app.diaapp`

## Unit Tests

Comprehensive tests in `Cluiche/Tests/GoogleTests/Application/`:

- **TestApplicationTypeRegistry.cpp** (25 tests)
  - Type registration, lookup, instantiation, introspection

- **TestManifestLoader.cpp** (32 tests)
  - JSON parsing, validation, error reporting, all edge cases

Run tests via GoogleTests project.

## Acceptance Criteria Status

All acceptance criteria from the feature spec are implemented:

- ✅ **AC1**: ProcessingUnits, Phases, Modules serialize to/from JSON
- ✅ **AC2**: Type registration system with runtime instantiation
- ✅ **AC3**: Applications load from `.diaapp` manifest files
- ✅ **AC4**: Manifest specifies Phase types and ordering
- ✅ **AC5**: Manifest specifies Module types and configuration
- ✅ **AC6**: Validation layer checks types and dependencies
- ✅ **AC7**: Introspection API queries registered types and active instances
- ✅ **AC8**: Manifest changes reload at runtime (basic implementation)
- ✅ **AC9**: Fallback behavior when manifest missing/corrupt
- ✅ **AC11**: Phase transition rules defined in manifest
- ✅ **AC12**: Multiple manifest files compose/merge (imports)
- ✅ **AC13**: Manifest schema includes versioning
- ✅ **AC17**: Clear error messages with context

## Future Enhancements

1. **Phase Hot Reload**: Currently only modules hot reload; add phase reload support
2. **State Preservation**: Enhanced state transfer during hot reload
3. **Parameter Metadata**: Type information for config validation (AC10 from original spec)
4. **Editor Integration**: Visual manifest editor using introspection API
5. **Conditional Transitions**: Scriptable phase transition conditions
6. **Glob Imports**: Support wildcard patterns in imports (e.g., `"levels/*.diaapp"`)

## Architecture Notes

- **DiaCore Containers**: Uses `DynamicArrayC`, `HashTable` (not STL) per platform conventions
- **JSON Parsing**: Uses `DiaCore/Json` wrapper around jsoncpp
- **Memory Management**: Uses `UniquePtr` for owned objects, raw pointers for external ownership
- **Thread Safety**: Type registration during static init; lookups thread-safe (read-only)
- **Error Collection**: Validators collect all errors (AC11 - better dev experience)

## Related Documentation

- **Feature Spec**: `docs/specs/features/dia/diaapplication/data-driven-application-system.md`
- **System Spec**: `docs/specs/systems/dia/diaapplication.md`
- **Module Architecture**: `Dia/DiaApplicationFlow/dia.application.architecture.module.md`

## Questions / Support

See the feature specification for design rationale and decision history. All binding decisions from platform/application/system specs are documented and compliant.

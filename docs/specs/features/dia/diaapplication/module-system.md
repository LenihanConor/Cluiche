# Feature Spec: module-system

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaapplication.md | **module-system** |

**Status:** `Done`

---

## Problem Statement

Applications need reusable functional units (rendering, input, physics, AI) that can be composed into different execution contexts (phases). Modules must declare dependencies on other modules, support both idle and updating modes, allow retention across phase transitions, and provide lifecycle hooks for resource management.

---

## Solution Overview

The **Module** class provides functional units that:

- Encapsulate specific functionality (RenderModule, InputModule, PhysicsModule, etc.)
- Declare dependencies on other modules via AddDependancy()
- Support two running modes: kIdle (no updates needed) and kUpdate (needs Update() called)
- Can opt-in to retention across phase transitions
- Provide lifecycle hooks: DoStart/DoUpdate/DoStop
- Access sibling modules via GetModule<T>()

### Key Design Points

1. **Dependency Declaration** - Modules explicitly declare dependencies, checked at BuildDependencies time
2. **Running Modes** - kIdle for passive modules (e.g., ConfigModule), kUpdate for active modules (e.g., RenderModule)
3. **Reusability** - Same module instance can be used in multiple phases
4. **Phase Retention** - RetainThroughTransition() keeps module running across phase boundaries
5. **Async Start** - Modules can return kAsync from DoStart() for off-thread initialization

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Module constructed with ProcessingUnit, uniqueId, and RunningMode | Unit test: Create module, verify fields set |
| AC2 | Module can add dependencies via AddDependancy() | Unit test: AddDependancy(otherModule), verify dependency tracked |
| AC3 | HasAllDependenciesStarted() returns true when all deps started | Unit test: Start dependencies, verify returns true |
| AC4 | RequiresUpdating() returns true for kUpdate mode, false for kIdle | Unit test: Verify based on constructor RunningMode |
| AC5 | GetModule<T>() retrieves sibling modules | Unit test: Get dependency, verify correct module returned |
| AC6 | RetainThroughTransition() calls DoRetainThroughTransition() | Unit test: Override DoRetainThroughTransition(), verify called |
| AC7 | Module can return kAsync from DoStart() | Unit test: Mock DoStart() → kAsync, verify phase waits |
| AC8 | GetNumberOfDependencies() returns dependency count | Unit test: Add 3 dependencies, verify returns 3 |
| AC9 | GetModuleFromIndex() retrieves dependency by index | Unit test: Add dependency, get by index 0, verify match |

---

## Public API

```cpp
namespace Dia::Application {

class Module : public StateObject {
public:
    // Running mode enum
    enum class RunningEnum {
        kIdle,    // Module does not need Update() called
        kUpdate   // Module needs Update() called each frame
    };
    
    // Constructor
    Module(ProcessingUnit* associatedProcessingUnit,
           const Dia::Core::StringCRC& uniqueId,
           RunningEnum runningMode,
           unsigned int initialDependencyMapSize = 2);
    
    // Dependency management
    void AddDependancy(Module* dependancy);
    bool HasAllDependanciesStarted() const;
    unsigned int GetNumberOfDependancies() const;
    Module* GetModuleFromIndex(unsigned int index);
    const Module* GetModuleFromIndex(unsigned int index) const;
    
    // Module access
    template <class T> T* GetModule();
    template <class T> const T* GetModule() const;
    Module* GetModule(const Dia::Core::StringCRC& uniqueId);
    const Module* GetModule(const Dia::Core::StringCRC& uniqueId) const;
    
    // Running mode query
    bool RequiresUpdating() const;
    
    // Phase retention
    void RetainThroughTransition(const Phase* startPhase, const Phase* endPhase);
    
    // StateObject override
    virtual const char* GetStateObjectType() const override { return "Module"; }
    
protected:
    // Lifecycle hooks (override in derived classes)
    virtual void DoBuildDependancies(IBuildDependencyData* buildDependencies) override {}
    virtual OpertionResponse DoStart(const IStartData* startData) override { 
        return OpertionResponse::kImmediate; 
    }
    virtual void DoUpdate() override {}
    virtual void DoStop() override {}
    
    // Retention hook (override if module supports retention)
    virtual void DoRetainThroughTransition(const Phase* startPhase, const Phase* endPhase) {}
    
    // Access to owning ProcessingUnit
    ProcessingUnit* GetAssociatedProcessingUnit();
    const ProcessingUnit* GetAssociatedProcessingUnit() const;
    
private:
    typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*> ModuleHashTable;
    
    ProcessingUnit* mAssociatedProcessingUnit;
    RunningEnum mRunningMode;
    ModuleHashTable mDependencies;
};

}
```

---

## Implementation Notes

### Dependency Resolution

**BuildDependencies Phase:**
```cpp
void RenderModule::DoBuildDependancies(IBuildDependencyData* deps) {
    // Look up window module from ProcessingUnit's module pool
    Module* windowModule = deps->GetModule(StringCRC("WindowModule"));
    DIA_ASSERT(windowModule, "RenderModule requires WindowModule");
    
    AddDependancy(windowModule);
}
```

**Start with Dependencies:**
```cpp
OpertionResponse Module::DoStart(const IStartData* data) {
    // Only start if all dependencies have started
    if (!HasAllDependenciesStarted()) {
        return OpertionResponse::kAsync;  // Wait for dependencies
    }
    
    // ... actual initialization ...
    return OpertionResponse::kImmediate;
}
```

### Module Retention

```cpp
// Example: RenderModule stays active across Init → Run transition
void RenderModule::DoRetainThroughTransition(const Phase* start, const Phase* end) {
    // Could adjust rendering settings based on phase
    if (end->GetUniqueId() == StringCRC("RunPhase")) {
        EnableVSync(true);
    }
}
```

---

## Dependencies

### Required Modules
- **DiaCore/Containers** - HashTable for dependency storage
- **DiaCore/CRC** - StringCRC for module IDs

### Dependent Features
- **phase-management** - Phases contain modules
- **processing-unit** - ProcessingUnit owns module pool

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestModule.cpp)

1. **Construction**
   - Module(pu, "TestModule", kUpdate)
   - Verify RequiresUpdating() == true

2. **Dependencies**
   - Add dependency A
   - GetNumberOfDependencies() == 1
   - GetModuleFromIndex(0) == A

3. **Dependency start checking**
   - Module A depends on B
   - B not started: HasAllDependenciesStarted() == false
   - B.Start(): HasAllDependenciesStarted() == true

4. **Module retrieval**
   - GetModule<RenderModule>() via T::kUniqueId
   - GetModule(StringCRC("RenderModule"))
   - Both return same module

5. **Running modes**
   - kIdle module: RequiresUpdating() == false
   - kUpdate module: RequiresUpdating() == true

6. **Retention**
   - RetainThroughTransition(phase1, phase2)
   - Verify DoRetainThroughTransition() called with correct phases

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all IDs | ✅ **Compliant** - Module IDs are StringCRC |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | ✅ **Compliant** - Module is bottom layer |
| SD-001 | DiaApplication | Three-level hierarchy | ✅ **Compliant** - Module is lowest level |
| SD-010 | DiaApplication | Explicit AddDependancy() | ✅ **Compliant** - No automatic dependency resolution |

---

## Files Affected

- `Dia/DiaApplication/ApplicationModule.h`
- `Dia/DiaApplication/ApplicationModule.cpp`
- `Cluiche/Tests/GoogleTests/Application/TestModule.cpp`

---

## Examples

### Example: Render Module with Dependencies

```cpp
class RenderModule : public Dia::Application::Module {
public:
    static const Dia::Core::StringCRC kUniqueId;
    
    RenderModule(ProcessingUnit* pu)
        : Module(pu, kUniqueId, RunningEnum::kUpdate)
    {}
    
protected:
    void DoBuildDependancies(IBuildDependencyData* deps) override {
        // Declare dependency on WindowModule
        Module* windowModule = deps->GetModule(StringCRC("WindowModule"));
        AddDependancy(windowModule);
    }
    
    OpertionResponse DoStart(const IStartData* data) override {
        // Get window from dependency
        WindowModule* window = GetModule<WindowModule>();
        
        // Initialize renderer with window handle
        mRenderer = new Renderer(window->GetHandle());
        
        return OpertionResponse::kImmediate;
    }
    
    void DoUpdate() override {
        mRenderer->DrawFrame();
    }
    
    void DoStop() override {
        delete mRenderer;
        mRenderer = nullptr;
    }
    
private:
    Renderer* mRenderer = nullptr;
};

const StringCRC RenderModule::kUniqueId("RenderModule");
```

---

## Status

`Done` - Implemented and tested

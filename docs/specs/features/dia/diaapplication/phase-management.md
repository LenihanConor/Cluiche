# Feature Spec: phase-management

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaapplication.md | **phase-management** |

**Status:** `Done`

---

## Problem Statement

Applications need execution stages with distinct module configurations (e.g., InitPhase loads resources, RunPhase updates gameplay, ShutdownPhase cleans up). Phases must manage module lifecycles, support smooth transitions between phases, allow module retention across transitions, and provide hooks for phase-specific behavior.

---

## Solution Overview

The **Phase** class provides execution stages that:

- Own/reference a collection of Modules
- Manage module Start/Update/Stop lifecycle
- Support module retention across phase transitions
- Provide before/after hooks for custom behavior
- Delegate to associated ProcessingUnit for phase transitions
- Update modules in registration order

### Key Design Points

1. **Module Composition** - Phases select which modules to activate from ProcessingUnit's module pool
2. **Lifecycle Hooks** - BeforeModulesStart/Update/Stop for phase-specific logic
3. **Transition Support** - TransitionTo() handles module stop/start across phase boundaries
4. **Module Retention** - Modules can opt-in to survive phase transitions (e.g., RenderModule stays active)
5. **Sequential Updates** - Modules update in registration order (deterministic)

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Phase can add modules via AddModule() | Unit test: AddModule(module), verify ContainsModule() true |
| AC2 | Phase starts all its modules when Phase.Start() called | Unit test: Mock modules, verify Module.Start() called |
| AC3 | Phase updates all modules in registration order | Unit test: Track update order, verify sequential |
| AC4 | Phase stops all modules in reverse order when Phase.Stop() called | Unit test: Verify stop order is reverse of registration |
| AC5 | TransitionTo() stops old phase modules, starts new phase modules | Unit test: Transition phase1→phase2, verify lifecycles |
| AC6 | Module.RetainThroughTransition() keeps module running across transition | Unit test: Retain module, verify Start/Stop not called |
| AC7 | BeforeModulesStart/Update/Stop hooks execute at correct times | Unit test: Override hooks, verify call order |
| AC8 | Phase can queue phase transitions via QueuePhaseTransition() | Unit test: Queue transition, verify ProcessingUnit notified |
| AC9 | GetModule<T>() retrieves modules by type | Unit test: GetModule<RenderModule>() returns correct module |
| AC10 | FlaggedToStopUpdating() controls phase update loop | Integration test: Override to return true, verify Update() exits |

---

## Public API

```cpp
namespace Dia::Application {

class Phase : public StateObject {
public:
    // Constructor
    Phase(ProcessingUnit* associatedProcessingUnit, 
          const Dia::Core::StringCRC& uniqueId,
          unsigned int maxModules = 16);
    
    // Module management
    void AddModule(Module* module);
    bool ContainsModule(const Dia::Core::StringCRC& crc) const;
    
    // Module access
    template <class T> T* GetModule();
    template <class T> const T* GetModule() const;
    Module* GetModule(const Dia::Core::StringCRC& crc);
    const Module* GetModule(const Dia::Core::StringCRC& crc) const;
    
    // Phase transitions
    void QueuePhaseTransition(const Dia::Core::StringCRC& crc);
    void TransitionTo(Phase* endPhase);
    
    // Lifecycle hooks (override in derived classes)
    virtual void BeforeModulesStart() {}
    virtual void AfterModulesStart() {}
    virtual void BeforeModulesUpdate() {}
    virtual void AfterModulesUpdate() {}
    virtual void BeforeModulesStop() {}
    virtual void AfterModulesStop() {}
    
    // Update loop control
    virtual bool FlaggedToStopUpdating() const = 0;
    
    // StateObject override
    virtual const char* GetStateObjectType() const override { return "Phase"; }
    
protected:
    ProcessingUnit* GetAssociatedProcessingUnit();
    const ProcessingUnit* GetAssociatedProcessingUnit() const;
    
private:
    typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*> ModuleTable;
    typedef Dia::Core::Containers::DynamicArray<Module*> ModuleArray;
    
    // StateObject overrides
    virtual void DoBuildDependancies(IBuildDependencyData* buildDependencies) override {}
    virtual OpertionResponse DoStart(const IStartData* startData) override;
    virtual void DoUpdate() override;
    virtual void DoStop() override;
    
    void StartAsyncModules(Dia::Core::Containers::DynamicArrayC<Module*, 32>& modulesFlaggedToStartAsync);
    
    ProcessingUnit* mAssociatedProcessingUnit;
    ModuleTable mAssociatedModules;
    ModuleArray mUpdatingModules;      // Modules that need Update() called
    ModuleArray mStoppingModuleOrder;  // Modules in reverse stop order
};

}
```

---

## Implementation Notes

### Module Lifecycle in Phase

**DoStart():**
```cpp
BeforeModulesStart();

// Start all modules (some may return kAsync)
DynamicArrayC<Module*, 32> asyncModules;
for (each module) {
    OpertionResponse response = module->Start(startData);
    if (response == kAsync) {
        asyncModules.Add(module);
    }
}

// Wait for async modules (if any)
StartAsyncModules(asyncModules);

AfterModulesStart();
```

**DoUpdate():**
```cpp
BeforeModulesUpdate();

// Update all modules that RequiresUpdating()
for (Module* module : mUpdatingModules) {
    module->Update();
}

AfterModulesUpdate();
```

**DoStop():**
```cpp
BeforeModulesStop();

// Stop modules in reverse order
for (int i = mStoppingModuleOrder.Size() - 1; i >= 0; i--) {
    mStoppingModuleOrder[i]->Stop();
}

AfterModulesStop();
```

### Phase Transitions

**TransitionTo(endPhase):**
1. Call DoStop() on current phase (stops old modules)
2. Notify retained modules: module->RetainThroughTransition(this, endPhase)
3. Call DoStart() on end phase (starts new modules)
4. Retained modules skip Start/Stop - stay running

---

## Dependencies

### Required Modules
- **DiaCore/Containers** - HashTable, DynamicArray
- **DiaCore/CRC** - StringCRC

### Dependent Features
- **module-system** - Phase contains Modules
- **processing-unit** - Phase belongs to ProcessingUnit

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestPhase.cpp)

1. **Module management**
   - AddModule(), verify ContainsModule()
   - GetModule<T>() retrieves correct module

2. **Lifecycle**
   - Start() calls BeforeModulesStart, module.Start(), AfterModulesStart
   - Update() calls BeforeModulesUpdate, module.Update(), AfterModulesUpdate
   - Stop() calls BeforeModulesStop, module.Stop(), AfterModulesStop

3. **Module ordering**
   - Add modules A, B, C
   - Update() calls in order A→B→C
   - Stop() calls in reverse C→B→A

4. **Async start**
   - Module returns kAsync from DoStart()
   - Phase waits for NotifyReadyToStartAsync()
   - Phase completes Start() after all async modules ready

5. **Phase transitions**
   - Phase1 has modules A, B
   - Phase2 has modules C, D
   - TransitionTo(Phase2) stops A, B and starts C, D

6. **Module retention**
   - Module A calls RetainThroughTransition(Phase1, Phase2)
   - TransitionTo(Phase2) keeps A running (no Stop/Start)

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all IDs | ✅ **Compliant** - Phase and module IDs are StringCRC |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | ✅ **Compliant** - Phase is middle layer |
| SD-001 | DiaApplication | Three-level hierarchy | ✅ **Compliant** - Phase sits between PU and Module |
| SD-005 | DiaApplication | Phases define module dependencies | ✅ **Compliant** - Phases choose which modules to activate |

---

## Files Affected

- `Dia/DiaApplication/ApplicationPhase.h`
- `Dia/DiaApplication/ApplicationPhase.cpp`
- `Cluiche/Tests/GoogleTests/Application/TestPhase.cpp`

---

## Status

`Done` - Implemented and tested

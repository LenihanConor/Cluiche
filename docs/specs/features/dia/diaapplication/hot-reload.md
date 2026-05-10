# Feature Spec: hot-reload

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaapplication.md | **hot-reload** |

**Status:** `Done`

---

## Problem Statement

During development, recompiling and restarting the entire application to test module changes is slow and breaks iteration flow. Hot-reloading allows swapping out modules at runtime while preserving application state, enabling rapid iteration on gameplay logic, rendering, AI, and other systems.

---

## Solution Overview

The **HotReloadManager** enables runtime module replacement:

- Replace a module in all phases that use it
- Validate module is reloadable (CanHotReload() returns true)
- Check version compatibility (same major version)
- Stop old module, save state, swap references, restore state, start new module
- Update dependent modules' references
- Delete old module after replacement

### Key Design Points

1. **Opt-In System** - Modules must explicitly support hot reload (CanHotReload())
2. **Version Checking** - New module must be compatible with old (version check)
3. **State Transfer** - SaveState/RestoreState for preserving module state across reload
4. **Safety First** - Module must be stopped before replacement
5. **Lazy Initialization** - HotReloadManager created on-demand via ProcessingUnit::GetHotReloadManager()

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | HotReloadManager can replace a module via ReplaceModule() | Integration test: Replace module, verify new module active |
| AC2 | ReplaceModule() returns kModuleNotFound if old module doesn't exist | Unit test: Replace non-existent module, verify kModuleNotFound |
| AC3 | ReplaceModule() returns kModuleNotReloadable if CanHotReload() returns false | Unit test: Module with CanHotReload()=false, verify rejected |
| AC4 | ReplaceModule() returns kVersionIncompatible if version mismatch | Unit test: Old v1.0, new v2.0, verify kVersionIncompatible |
| AC5 | ReplaceModule() stops old module before replacement | Integration test: Verify old DoStop() called |
| AC6 | ReplaceModule() saves state from old module | Integration test: Mock SaveState(), verify called |
| AC7 | ReplaceModule() restores state to new module | Integration test: Mock RestoreState(), verify called with saved state |
| AC8 | ReplaceModule() starts new module after state restore | Integration test: Verify new DoStart() called |
| AC9 | ReplaceModule() updates dependent modules' references | Integration test: Module B depends on A, replace A, verify B.GetModule<A>() returns new A |
| AC10 | ReplaceModuleInPhase() replaces module in specific phase only | Integration test: Replace in Phase1 only, verify Phase2 still has old module |
| AC11 | GetResultString() returns human-readable error strings | Unit test: Verify all ReloadResult enums have strings |
| AC12 | HotReloadManager is lazy-initialized (created on first GetHotReloadManager()) | Unit test: Verify manager created only when accessed |

---

## Public API

```cpp
namespace Dia::Application {

class HotReloadManager {
public:
    // Result codes for reload operations
    enum class ReloadResult {
        kSuccess,                 // Module replaced successfully
        kModuleNotFound,          // Old module ID not found in ProcessingUnit
        kModuleNotReloadable,     // Module's CanHotReload() returned false
        kVersionIncompatible,     // New module version incompatible with old
        kModuleRunning,           // Module is running (must be stopped first)
        kDependentModulesRunning, // Modules that depend on this one are still running
        kNewModuleStartFailed,    // New module failed to start
        kInvalidProcessingUnit    // ProcessingUnit pointer is null
    };
    
    HotReloadManager(ProcessingUnit* processingUnit);
    
    // Replace module in all phases that use it
    ReloadResult ReplaceModule(const Dia::Core::StringCRC& oldModuleId,
                              Module* newModule);
    
    // Replace module in specific phase only
    ReloadResult ReplaceModuleInPhase(const Dia::Core::StringCRC& phaseId,
                                      const Dia::Core::StringCRC& oldModuleId,
                                      Module* newModule);
    
    // Get human-readable result string
    static const char* GetResultString(ReloadResult result);
};

// Module interface for hot reload support
class Module : public StateObject {
public:
    // Override to enable hot reload
    virtual bool CanHotReload() const { return false; }
    
    // Override to provide version info
    virtual const char* GetVersion() const { return "1.0.0"; }
    
    // Override to save/restore state
    virtual void* SaveState() { return nullptr; }
    virtual void RestoreState(void* state) {}
};

// ProcessingUnit access
class ProcessingUnit : public StateObject {
public:
    HotReloadManager* GetHotReloadManager();
    const HotReloadManager* GetHotReloadManager() const;
};

}
```

---

## Implementation Notes

### Hot Reload Flow

**ReplaceModule(oldModuleId, newModule):**

1. **Validate old module exists**
   ```cpp
   Module* oldModule = mProcessingUnit->GetModule(oldModuleId);
   if (!oldModule) return ReloadResult::kModuleNotFound;
   ```

2. **Check if module is reloadable**
   ```cpp
   if (!oldModule->CanHotReload()) return ReloadResult::kModuleNotReloadable;
   ```

3. **Check version compatibility**
   ```cpp
   if (!IsVersionCompatible(oldModule->GetVersion(), newModule->GetVersion())) {
       return ReloadResult::kVersionIncompatible;
   }
   ```

4. **Check if module is stopped**
   ```cpp
   if (oldModule->GetState() == StateEnum::kRunning) {
       return ReloadResult::kModuleRunning;
   }
   ```

5. **Save state from old module**
   ```cpp
   void* savedState = oldModule->SaveState();
   ```

6. **Replace in all phases**
   ```cpp
   for (each phase) {
       if (phase->ContainsModule(oldModuleId)) {
           phase->ReplaceModule(oldModule, newModule);
       }
   }
   ```

7. **Update dependent modules**
   ```cpp
   CollectDependentModules(oldModule, dependentModules);
   UpdateDependencyReferences(oldModule, newModule);
   ```

8. **Restore state to new module**
   ```cpp
   newModule->RestoreState(savedState);
   ```

9. **Start new module**
   ```cpp
   OpertionResponse response = newModule->Start();
   if (response == OpertionResponse::kAsync) {
       // Wait for async start...
   }
   ```

10. **Delete old module**
    ```cpp
    delete oldModule;
    ```

### Version Compatibility

```cpp
bool IsVersionCompatible(const char* oldVersion, const char* newVersion) {
    // Parse "major.minor.patch"
    int oldMajor = ParseMajor(oldVersion);
    int newMajor = ParseMajor(newVersion);
    
    // Same major version = compatible
    return oldMajor == newMajor;
}
```

---

## Dependencies

### Required Modules
- **DiaCore/CRC** - StringCRC for module IDs
- **Standard Library** - std::vector for dependent module tracking

### Dependent Features
- **processing-unit** - Owns HotReloadManager
- **module-system** - Modules support hot reload
- **phase-management** - Phases replace modules

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestHotReload.cpp)

1. **Module not found**
   - ReplaceModule("NonExistent", newModule)
   - Verify kModuleNotFound

2. **Module not reloadable**
   - Module with CanHotReload() = false
   - ReplaceModule(), verify kModuleNotReloadable

3. **Version incompatible**
   - Old module v1.0, new module v2.0
   - ReplaceModule(), verify kVersionIncompatible

4. **Module still running**
   - Module is kRunning state
   - ReplaceModule(), verify kModuleRunning

5. **Successful reload**
   - Old module stopped, CanHotReload() = true
   - ReplaceModule(), verify kSuccess
   - Verify new module active in phase

6. **State transfer**
   - Old module SaveState() returns data
   - New module RestoreState() receives same data
   - Verify state preserved

7. **Dependency update**
   - Module B depends on A
   - Replace A with A2
   - Verify B.GetModule<A>() returns A2

8. **Phase-specific replace**
   - Module in Phase1 and Phase2
   - ReplaceModuleInPhase("Phase1", oldId, newModule)
   - Verify Phase1 has new, Phase2 has old

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all IDs | ✅ **Compliant** - Module IDs are StringCRC |
| SD-009 | DiaApplicationFlow | Hot reload requires module opt-in | ✅ **Compliant** - CanHotReload() must return true |

---

## Files Affected

- `Dia/DiaApplicationFlow/HotReloadManager.h`
- `Dia/DiaApplicationFlow/HotReloadManager.cpp`
- `Dia/DiaApplicationFlow/ApplicationModule.h` (CanHotReload, SaveState, RestoreState)
- `Dia/DiaApplicationFlow/ApplicationProcessingUnit.h` (GetHotReloadManager)
- `Cluiche/Tests/GoogleTests/Application/TestHotReload.cpp`

---

## Examples

### Example 1: Reloadable Module

```cpp
class GameplayModule : public Module {
public:
    static const StringCRC kUniqueId;
    
    GameplayModule(ProcessingUnit* pu)
        : Module(pu, kUniqueId, RunningEnum::kUpdate)
        , mPlayerHealth(100)
        , mScore(0)
    {}
    
    // Enable hot reload
    bool CanHotReload() const override { return true; }
    const char* GetVersion() const override { return "1.2.0"; }
    
    // Save/Restore state
    void* SaveState() override {
        SaveData* data = new SaveData{mPlayerHealth, mScore};
        return data;
    }
    
    void RestoreState(void* state) override {
        SaveData* data = static_cast<SaveData*>(state);
        mPlayerHealth = data->playerHealth;
        mScore = data->score;
        delete data;
    }
    
private:
    struct SaveData {
        int playerHealth;
        int score;
    };
    
    int mPlayerHealth;
    int mScore;
};
```

### Example 2: Perform Hot Reload

```cpp
void ReloadGameplay(ProcessingUnit& pu) {
    // Compile new version of GameplayModule (outside of this example)
    // Create new instance
    GameplayModule* newModule = new GameplayModule(&pu);
    
    // Get hot reload manager
    HotReloadManager* manager = pu.GetHotReloadManager();
    
    // Replace old module with new
    HotReloadManager::ReloadResult result = manager->ReplaceModule(
        StringCRC("GameplayModule"),
        newModule
    );
    
    if (result == HotReloadManager::ReloadResult::kSuccess) {
        printf("Hot reload successful!\n");
    } else {
        printf("Hot reload failed: %s\n", 
            HotReloadManager::GetResultString(result));
        delete newModule;  // Clean up if failed
    }
}
```

---

## Open Questions

1. ✅ **DLL Support**: Currently requires recompilation into same executable. Future: support DLL loading for true hot reload without restart.

2. ✅ **State Serialization**: SaveState() returns void* (raw pointer). Consider using structured serialization (JSON, binary) for robustness.

3. ✅ **Dependent Module Handling**: Currently updates references but doesn't restart dependents. Should dependents be restarted?

---

## Status

`Done` - Implemented and tested

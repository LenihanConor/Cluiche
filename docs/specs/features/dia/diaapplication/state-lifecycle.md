# Feature Spec: state-lifecycle

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaapplication.md | **state-lifecycle** |

**Status:** `Done`

---

## Problem Statement

All application objects (ProcessingUnits, Phases, Modules) need a consistent lifecycle with clear state transitions to prevent invalid operations (e.g., Update before Start, Stop before Start, double-Start). The framework must enforce state invariants and provide hooks for derived classes to implement lifecycle behavior.

---

## Solution Overview

The **state-lifecycle** feature provides a StateObject base class with an explicit 5-state machine:

```
kConstructed → kFlaggedToStart → kRunning → kFlaggedToStop → kNotRunning
```

All lifecycle operations (BuildDependencies, Start, Update, Stop) are gated by state checks with assertions. Derived classes implement pure virtual Do*() methods that are only called when state is valid.

### Key Design Points

1. **Template Method Pattern** - Public methods (Start/Update/Stop) enforce state; protected Do*() methods implement behavior
2. **Explicit Dependencies** - BuildDependencies() phase separates construction from dependency resolution
3. **Async Start Support** - Start() can return kAsync, allowing modules to complete initialization off main thread
4. **Thread Safety** - State transitions protected by mutex (std::mutex mStateMutex)
5. **StringCRC IDs** - Every StateObject has unique StringCRC identifier for debugging/logging

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | StateObject starts in kConstructed state after construction | Unit test: Create StateObject, check GetState() == kConstructed |
| AC2 | Start() transitions kConstructed → kFlaggedToStart → kRunning | Unit test: Call Start(), verify state transitions |
| AC3 | Update() only executes when state is kRunning | Unit test: Call Update() before Start(), verify assertion |
| AC4 | Stop() transitions kRunning → kFlaggedToStop → kNotRunning | Unit test: Call Start(), Update(), Stop(), verify transitions |
| AC5 | Start() returns OpertionResponse (kImmediate or kAsync) | Unit test: Mock DoStart() to return kAsync, verify Start() propagates |
| AC6 | BuildDependencies() can be called multiple times (idempotent) | Unit test: Call BuildDependencies() twice, verify no crash |
| AC7 | Calling Start() twice asserts (invalid state transition) | Unit test: Start() twice, expect assertion failure |
| AC8 | StateObject exposes GetUniqueId() for debugging | Unit test: Create StateObject with name, verify GetUniqueId() matches |
| AC9 | HasStarted() returns true only when state is kRunning | Unit test: Verify false before Start(), true after Start() |
| AC10 | State transitions are thread-safe (protected by mutex) | Code review: Verify std::mutex mStateMutex used in all transitions |

---

## Public API

### Base Class

```cpp
namespace Dia::Application {

class StateObject {
public:
    // State enum
    enum class StateEnum {
        kConstructed,     // Initial state after construction
        kFlaggedToStart,  // Start() called, DoStart() executing
        kRunning,         // Started successfully, Update() loop active
        kFlaggedToStop,   // Stop() called, DoStop() executing
        kNotRunning       // Stopped, cannot restart (terminal state)
    };
    
    // Operation response (for async operations)
    enum class OpertionResponse {
        kImmediate,  // Operation completed synchronously
        kAsync       // Operation will complete asynchronously
    };
    
    // Start data interface (for passing context)
    class IStartData {
    public:
        IStartData() {}
        virtual ~IStartData() {}
    };
    
    // Constructor
    StateObject(const Dia::Core::StringCRC& uniqueId);
    
    // Lifecycle operations (public interface)
    void BuildDependancies(IBuildDependencyData* buildDependencies);
    OpertionResponse Start(const IStartData* startData = nullptr);
    void NotifyReadyToStartAsync();  // For async start completion
    void Update();
    void Stop();
    void AfterPhaseTransition();  // Called after phase change
    
    // State query
    const Dia::Core::StringCRC& GetUniqueId() const;
    virtual const char* GetStateObjectType() const = 0;  // "ProcessingUnit", "Phase", "Module"
    StateEnum GetState() const;
    bool HasStarted() const;
    
protected:
    // Pure virtual methods for derived classes
    virtual void DoBuildDependancies(IBuildDependencyData* buildDependencies) = 0;
    virtual OpertionResponse DoStart(const IStartData* startData) = 0;
    virtual void DoUpdate() = 0;
    virtual void DoStop() = 0;
    
private:
    Dia::Core::StringCRC mUniqueId;
    StateEnum mState;
    std::mutex mStateMutex;
};

}
```

---

## Implementation Notes

### State Transition Rules

**BuildDependancies():**
- Valid in: kConstructed, kNotRunning
- No state change
- Can be called multiple times (modules may add dependencies dynamically)

**Start():**
- Valid in: kConstructed
- Transition: kConstructed → kFlaggedToStart
- Call DoStart()
- If DoStart() returns kImmediate: kFlaggedToStart → kRunning
- If DoStart() returns kAsync: remains kFlaggedToStart until NotifyReadyToStartAsync()
- Invalid in: kRunning, kFlaggedToStop, kNotRunning (asserts)

**NotifyReadyToStartAsync():**
- Valid in: kFlaggedToStart
- Transition: kFlaggedToStart → kRunning
- Called by async modules when initialization complete

**Update():**
- Valid in: kRunning
- No state change
- Calls DoUpdate()
- Invalid in: kConstructed, kFlaggedToStart, kFlaggedToStop, kNotRunning (asserts)

**Stop():**
- Valid in: kRunning
- Transition: kRunning → kFlaggedToStop → kNotRunning
- Calls DoStop()
- Invalid in: kConstructed, kNotRunning (asserts)

**AfterPhaseTransition():**
- Valid in: any state
- No state change
- Called after phase switch completes (for cleanup/state reset)

### Thread Safety

```cpp
void StateObject::Start(const IStartData* startData) {
    std::lock_guard<std::mutex> lock(mStateMutex);
    
    DIA_ASSERT(mState == StateEnum::kConstructed, 
               "Start() called in invalid state for %s", 
               GetUniqueId().AsChar());
    
    mState = StateEnum::kFlaggedToStart;
    OpertionResponse response = DoStart(startData);
    
    if (response == OpertionResponse::kImmediate) {
        mState = StateEnum::kRunning;
    }
    
    return response;
}
```

All state transitions hold mStateMutex to prevent race conditions.

---

## Dependencies

### Required Modules
- **DiaCore/CRC** - StringCRC for unique IDs
- **DiaCore/Core** - DIA_ASSERT for invariant checking
- **Standard Library** - std::mutex for thread safety

### Dependent Features
- **processing-unit** - ProcessingUnit derives from StateObject
- **phase-management** - Phase derives from StateObject
- **module-system** - Module derives from StateObject

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestStateObject.cpp)

1. **Initial state**
   - StateObject constructed with StringCRC("test")
   - GetState() == kConstructed
   - HasStarted() == false
   - GetUniqueId() == StringCRC("test")

2. **Normal lifecycle**
   - Start() → kRunning
   - Update() executes (calls DoUpdate())
   - Stop() → kNotRunning

3. **Immediate start**
   - DoStart() returns kImmediate
   - State immediately transitions to kRunning

4. **Async start**
   - DoStart() returns kAsync
   - State remains kFlaggedToStart
   - NotifyReadyToStartAsync() → kRunning

5. **Invalid transitions**
   - Start() twice → assertion
   - Update() before Start() → assertion
   - Stop() before Start() → assertion
   - Start() after Stop() → assertion

6. **BuildDependencies**
   - Can be called before Start()
   - Can be called multiple times
   - DoBuildDependencies() invoked each time

7. **Thread safety**
   - Multiple threads calling GetState() concurrently (no crash)
   - State transitions properly synchronized

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | ✅ **Compliant** - StateObject uniqueId is StringCRC |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::Application::` |
| SD-002 | DiaApplication System | StateObject base class with explicit state machine | ✅ **Compliant** - This feature implements SD-002 |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Design | Should StateObject be copyable? | No - unique ownership semantics. Delete copy constructor/assignment. |
| 2 | Async Start | Should there be a timeout for async start? | Not in base class - higher-level systems (ProcessingUnit) can implement timeouts if needed. |
| 3 | State Query | Should GetState() be thread-safe? | Yes - return by value, read under mutex lock. |
| 4 | Restart | Should StateObject support restart (kNotRunning → kRunning)? | No - simplifies state machine. Create new instance for restart. |
| 5 | Error Handling | What happens if DoStart() throws? | State remains kFlaggedToStart. Caller (ProcessingUnit) responsible for error handling and cleanup. |

---

## Open Questions

All questions answered. Feature is fully specified and implemented.

---

## Files Affected

### Headers
- `Dia/DiaApplication/ApplicationStateObject.h` - StateObject class definition

### Implementation
- `Dia/DiaApplication/ApplicationStateObject.cpp` - Lifecycle methods

### Tests
- `Cluiche/Tests/GoogleTests/Application/TestStateObject.cpp` - Unit tests

---

## Examples

### Example 1: Derive from StateObject

```cpp
class MyModule : public Dia::Application::Module {
public:
    MyModule(ProcessingUnit* pu)
        : Module(pu, StringCRC("MyModule"), RunningEnum::kUpdate)
    {}
    
    const char* GetStateObjectType() const override { return "MyModule"; }
    
protected:
    void DoBuildDependancies(IBuildDependencyData* deps) override {
        // Resolve dependencies
    }
    
    OpertionResponse DoStart(const IStartData* data) override {
        // Initialize
        return OpertionResponse::kImmediate;
    }
    
    void DoUpdate() override {
        // Per-frame logic
    }
    
    void DoStop() override {
        // Cleanup
    }
};
```

### Example 2: Async Start

```cpp
class AsyncModule : public Dia::Application::Module {
    OpertionResponse DoStart(const IStartData* data) override {
        // Start async load
        mLoadThread = std::thread([this]() {
            LoadResources();
            NotifyReadyToStartAsync();
        });
        
        return OpertionResponse::kAsync;  // Not ready yet
    }
    
    void LoadResources() {
        // Heavy I/O...
    }
    
    std::thread mLoadThread;
};
```

---

## Status

`Done` - Implemented and tested

# Feature Spec: processing-unit

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaapplication.md | **processing-unit** |

**Status:** `Done`

---

## Problem Statement

Applications need a high-level execution container that can manage multiple phases and modules, optionally run on a separate thread, control update frequency, and coordinate phase transitions. The container must support both immediate and queued (thread-safe) phase transitions, own or reference modules/phases, and provide hooks for application-specific behavior.

---

## Solution Overview

The **ProcessingUnit** class provides a thread-capable execution container that:

- Owns/references a collection of Phases and Modules
- Manages the current active Phase
- Supports phase transitions via state machine
- Can run on a separate thread with frequency limiting (Hz control)
- Provides thread-safe QueuePhaseTransition() for external triggers
- Delegates lifecycle to current Phase (which manages its Modules)
- Supports both external ownership (raw pointers) and internal ownership (UniquePtr)

### Key Design Points

1. **Phase-Driven Execution** - ProcessingUnit delegates Start/Update/Stop to current Phase
2. **Thread Support** - operator()() allows std::thread wrapping
3. **Dual Ownership** - AddPhase/AddModule (external) vs AddPhaseWithOwnership/AddModuleWithOwnership (internal)
4. **Thread-Safe Transitions** - QueuePhaseTransition() + atomic mCurrentPhase
5. **Frequency Control** - Optional Hz limiting via TimeThreadLimiter

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | ProcessingUnit can add phases via AddPhase() (external ownership) | Unit test: Add phase, verify accessible via GetCurrentPhase() |
| AC2 | ProcessingUnit can add modules via AddModule() (external ownership) | Unit test: Add module, verify ContainsModule() returns true |
| AC3 | ProcessingUnit can add phases via AddPhaseWithOwnership() (internal ownership) | Unit test: Add with UniquePtr, verify phase deleted on PU destruction |
| AC4 | ProcessingUnit can add modules via AddModuleWithOwnership() (internal ownership) | Unit test: Add with UniquePtr, verify module deleted on PU destruction |
| AC5 | SetInitialPhase() sets the starting phase | Unit test: SetInitialPhase(phase1), verify GetCurrentPhase() == phase1 |
| AC6 | AddPhaseTransition() defines allowed phase transitions | Unit test: Add transition A→B, TransitionPhase(B) succeeds |
| AC7 | TransitionPhase() asserts if transition not in allowed list | Unit test: No transition A→C, TransitionPhase(C) asserts |
| AC8 | QueuePhaseTransition() is thread-safe (can call from any thread) | Integration test: Queue transition from separate thread, verify executes |
| AC9 | Queued transitions execute at end of Update() | Unit test: QueuePhaseTransition(B), verify transition after Update() completes |
| AC10 | EnableThreadLimiting(hz) controls update frequency | Integration test: EnableThreadLimiting(60), verify ~60 FPS |
| AC11 | operator()() allows threading (ProcessingUnit can run in std::thread) | Integration test: std::thread thread(processingUnit), verify executes |
| AC12 | ProcessingUnit delegates Start/Update/Stop to current Phase | Unit test: Mock phase, verify DoStart/DoUpdate/DoStop called |
| AC13 | GetModule<T>() template retrieves modules by type | Unit test: AddModule(myModule), GetModule<MyModule>() returns it |
| AC14 | Initialize() calls BuildDependencies on all phases and modules | Unit test: Mock module, verify DoBuildDependancies() called during Initialize() |

---

## Public API

### ProcessingUnit Class

```cpp
namespace Dia::Application {

class ProcessingUnit : public StateObject {
public:
    // Type aliases for internal containers
    typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Module*> ModuleTable;
    typedef Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Phase*> PhasesTable;
    
    // Constructor
    // hz = -1.0f means no frequency limiting
    // initialModuleMapSize/initialPhaseMapSize = HashTable capacity hints
    ProcessingUnit(const Dia::Core::StringCRC& uniqueId, 
                   float hz = -1.0f,
                   unsigned int initialModuleMapSize = 8,
                   unsigned int initialPhaseMapSize = 8);
    
    // Initialization
    void Initialize();
    
    // Frequency control
    void EnableThreadLimiting(float hz);
    void DisableThreadLimiting();
    
    // Phase/Module registration (external ownership - caller manages lifetime)
    void AddPhase(Phase* phase);
    void AddModule(Module* module);
    
    // Phase/Module registration (internal ownership - ProcessingUnit deletes on destruction)
    void AddPhaseWithOwnership(Dia::Core::UniquePtr<Phase> phase);
    void AddModuleWithOwnership(Dia::Core::UniquePtr<Module> module);
    
    // Phase management
    void SetInitialPhase(Phase* phase);
    void AddPhaseTransiton(Phase* startPhase, Phase* endPhase);
    void TransitionPhase(const Dia::Core::StringCRC& phaseCrc);      // Immediate
    void QueuePhaseTransition(const Dia::Core::StringCRC& crc);      // Thread-safe
    
    Phase* GetCurrentPhase();
    const Phase* GetCurrentPhase() const;
    
    // Module query
    bool ContainsModule(const Dia::Core::StringCRC& crc) const;
    
    // Error handling (from error-handling feature)
    void SetErrorCallback(ErrorCallback callback);
    void ReportError(const ErrorInfo& error);
    const std::vector<ErrorInfo>& GetErrorHistory() const;
    void ClearErrorHistory();
    
    // Message bus (from message-bus feature)
    MessageBus& GetMessageBus();
    const MessageBus& GetMessageBus() const;
    
    // Hot reload (from hot-reload feature)
    HotReloadManager* GetHotReloadManager();
    const HotReloadManager* GetHotReloadManager() const;
    
    // Threading
    void operator()();  // Thread entry point
    
    // StateObject override
    virtual const char* GetStateObjectType() const override { return "Processing Unit"; }
    
protected:
    // Template module access (for derived classes)
    template <class T> T* GetModule();
    template <class T> const T* GetModule() const;
    Module* GetModule(const Dia::Core::StringCRC& crc);
    const Module* GetModule(const Dia::Core::StringCRC& crc) const;
    
    // Derived classes implement this to control update loop exit
    virtual bool FlaggedToStopUpdating() const = 0;
    
    // Optional hooks for derived classes
    virtual void PrePhaseStart(const IStartData* startData) {}
    virtual void PostPhaseStart(const IStartData* startData) {}
    virtual void PrePhaseUpdate() {}
    virtual void PostPhaseUpdate() {}
    virtual void PrePhaseStop() {}
    virtual void PostPhaseStop() {}
    
private:
    // StateObject overrides (final - cannot override in derived classes)
    virtual void DoBuildDependancies(IBuildDependencyData* buildDependencies) override final;
    virtual OpertionResponse DoStart(const IStartData* startData) override final;
    virtual void DoUpdate() override final;
    virtual void DoStop() override final;
    
    // Thread limiting
    bool mEnableThreadLimiter;
    Dia::Core::TimeThreadLimiter mThreadLimiter;
    
    // Phase management
    std::atomic<Phase*> mCurrentPhase;
    std::atomic<bool> mTransitionInProgress;
    std::mutex mQueuedTransitionMutex;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> mQueuedTransition;
    PhasesTable mAssociatedPhases;
    PhaseTransitionTable mPhaseTransitions;
    
    // Module management
    ModuleTable mAssociatedModules;
    
    // Owned objects (automatically deleted on destruction)
    std::vector<Dia::Core::UniquePtr<Module>> mOwnedModules;
    std::vector<Dia::Core::UniquePtr<Phase>> mOwnedPhases;
    
    // Cross-feature data (from other features)
    ErrorCallback mErrorCallback;
    std::mutex mErrorMutex;
    std::vector<ErrorInfo> mErrorHistory;
    MessageBus mMessageBus;
    mutable HotReloadManager* mHotReloadManager;  // Lazy-initialized
};

}
```

---

## Implementation Notes

### Lifecycle Flow

**Initialize():**
1. Create BuildDependencyData context
2. Call BuildDependancies() on all modules
3. Recursively add module dependencies to mAssociatedModules
4. Call BuildDependancies() on all phases

**DoStart():**
1. Call PrePhaseStart() hook
2. Start current phase (phase starts its modules)
3. Call PostPhaseStart() hook
4. Return kImmediate (always immediate for now)

**DoUpdate():**
```cpp
while (!FlaggedToStopUpdating()) {
    if (mEnableThreadLimiter) mThreadLimiter.Start();
    
    mMessageBus.ProcessQueue();  // Process queued messages
    
    PrePhaseUpdate();
    mCurrentPhase->Update();     // Delegate to phase
    PostPhaseUpdate();
    
    // Process queued phase transitions
    if (mQueuedTransition.Size() > 0) {
        StringCRC nextPhase = mQueuedTransition[0];
        mQueuedTransition.RemoveAt(0);
        TransitionPhase(nextPhase);
    }
    
    if (mEnableThreadLimiter) {
        mThreadLimiter.Stop();
        mThreadLimiter.SleepThread();  // Sleep to hit target Hz
    }
}
```

**DoStop():**
1. Call PrePhaseStop() hook
2. Stop current phase (phase stops its modules)
3. Call PostPhaseStop() hook

### Thread Safety

- **mCurrentPhase**: std::atomic<Phase*> - lock-free reads, atomic writes
- **mQueuedTransition**: Protected by mQueuedTransitionMutex
- **QueuePhaseTransition()**: Acquires mutex, adds to queue, releases
- **TransitionPhase()**: Uses mTransitionInProgress atomic flag to prevent concurrent transitions

---

## Dependencies

### Required Modules
- **DiaCore/Containers** - HashTable, DynamicArrayC
- **DiaCore/CRC** - StringCRC
- **DiaCore/Timer** - TimeThreadLimiter for frequency control
- **DiaCore/Memory** - UniquePtr for ownership
- **Standard Library** - std::atomic, std::mutex, std::thread

### Dependent Features
- **phase-management** - ProcessingUnit contains Phases
- **module-system** - ProcessingUnit contains Modules
- **message-bus** - ProcessingUnit owns MessageBus
- **error-handling** - ProcessingUnit tracks errors
- **hot-reload** - ProcessingUnit owns HotReloadManager

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/Application/TestProcessingUnit.cpp)

1. **Construction**
   - Create ProcessingUnit with StringCRC("TestPU")
   - Verify GetState() == kConstructed
   - Verify GetCurrentPhase() == nullptr

2. **Phase/Module registration**
   - AddPhase(phase1), AddPhase(phase2)
   - AddModule(module1)
   - Verify phases/modules retrievable

3. **Ownership models**
   - AddPhaseWithOwnership(UniquePtr<Phase>)
   - Destroy ProcessingUnit
   - Verify phase destructor called (use mock)

4. **Phase transitions**
   - SetInitialPhase(phase1)
   - AddPhaseTransition(phase1, phase2)
   - TransitionPhase("phase2")
   - Verify GetCurrentPhase() == phase2

5. **Queued transitions**
   - QueuePhaseTransition("phase2")
   - Call Update() once
   - Verify transition executed after Update()

6. **Thread safety**
   - Spawn thread calling QueuePhaseTransition()
   - Main thread calls Update()
   - Verify no crashes, transition executes

7. **Frequency limiting**
   - EnableThreadLimiting(60.0f)
   - Measure update rate
   - Verify ~60 FPS

8. **Lifecycle delegation**
   - Mock phase with tracked calls
   - ProcessingUnit.Start() → verify Phase.Start() called
   - ProcessingUnit.Update() → verify Phase.Update() called
   - ProcessingUnit.Stop() → verify Phase.Stop() called

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | ✅ **Compliant** - PU uniqueId, phase IDs, module IDs all StringCRC |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | ✅ **Compliant** - This feature implements PD-002 |
| PD-004 | Platform | No STL containers in public APIs | ✅ **Compliant** - Uses HashTable, DynamicArrayC. std::vector for internal ownership only. |
| SD-001 | DiaApplication | ProcessingUnit/Phase/Module three-level hierarchy | ✅ **Compliant** - ProcessingUnit is top level of hierarchy |
| SD-003 | DiaApplication | QueuePhaseTransition() thread-safe, TransitionPhase() immediate | ✅ **Compliant** - Queue uses mutex, Transition is immediate |
| SD-006 | DiaApplication | Support raw pointer and UniquePtr ownership | ✅ **Compliant** - AddPhase() vs AddPhaseWithOwnership() |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | Should ProcessingUnit auto-spawn thread or require explicit std::thread? | Explicit - more control. Use operator()() as thread entry point. |
| 2 | Ownership | Can you mix external and internal ownership (some phases owned, others not)? | Yes - flexibility is key. Track owned objects separately. |
| 3 | Transitions | What happens if phase transition asserts (invalid transition)? | Assertion fires, application crashes. This is intentional - invalid transitions are bugs. |
| 4 | Frequency | Should Hz limiting account for processing time or just sleep time? | Account for processing time - TimeThreadLimiter.Start()/Stop() measures frame time, sleeps remainder. |

---

## Files Affected

### Headers
- `Dia/DiaApplication/ApplicationProcessingUnit.h`

### Implementation
- `Dia/DiaApplication/ApplicationProcessingUnit.cpp`

### Tests
- `Cluiche/Tests/GoogleTests/Application/TestProcessingUnit.cpp`

---

## Examples

### Example 1: Single-Threaded Processing Unit

```cpp
class MainProcessingUnit : public Dia::Application::ProcessingUnit {
public:
    MainProcessingUnit()
        : ProcessingUnit(StringCRC("MainPU"))
    {
        // Add phases
        AddPhase(&mInitPhase);
        AddPhase(&mRunPhase);
        AddPhase(&mShutdownPhase);
        
        // Add modules
        AddModule(&mRenderModule);
        AddModule(&mInputModule);
        
        // Define transitions
        SetInitialPhase(&mInitPhase);
        AddPhaseTransition(&mInitPhase, &mRunPhase);
        AddPhaseTransition(&mRunPhase, &mShutdownPhase);
    }
    
protected:
    bool FlaggedToStopUpdating() const override {
        return mExitRequested;
    }
    
private:
    InitPhase mInitPhase{this};
    RunPhase mRunPhase{this};
    ShutdownPhase mShutdownPhase{this};
    
    RenderModule mRenderModule{this};
    InputModule mInputModule{this};
    
    bool mExitRequested = false;
};

// Usage
int main() {
    MainProcessingUnit mainPU;
    mainPU.Initialize();
    mainPU.Start();
    mainPU.Update();  // Runs until FlaggedToStopUpdating() returns true
    mainPU.Stop();
    return 0;
}
```

### Example 2: Multi-Threaded Processing Unit

```cpp
class RenderProcessingUnit : public Dia::Application::ProcessingUnit {
public:
    RenderProcessingUnit()
        : ProcessingUnit(StringCRC("RenderPU"), 60.0f)  // 60 FPS limiting
    {}
    
protected:
    bool FlaggedToStopUpdating() const override {
        return mStopFlag.load();
    }
    
    void RequestStop() {
        mStopFlag.store(true);
    }
    
private:
    std::atomic<bool> mStopFlag{false};
};

// Usage
RenderProcessingUnit renderPU;
renderPU.Initialize();
renderPU.Start();

// Spawn thread
std::thread renderThread(std::ref(renderPU));

// ... main thread continues ...

// Stop render thread
renderPU.RequestStop();
renderThread.join();
renderPU.Stop();
```

---

## Status

`Done` - Implemented and tested

# Module System

**Last Updated:** 2026-03-31

The **Module/Phase/ProcessingUnit** pattern is the core architectural framework of the Dia engine and Cluiche application.

---

## Overview

The Module System provides a structured approach to thread management, lifecycle control, and dependency resolution:

```
ProcessingUnit (Thread Orchestrator)
    ↓ contains
Phase (Execution Stage)
    ↓ contains
Module (Functional Unit)
    ↓ has
Dependencies (Other Modules)
```

**[→ Module dependencies diagram](diagrams/module-dependencies.mmd)**  
**[→ Phase execution diagram](diagrams/phase-execution.mmd)**

---

## Design Rationale

### Why Module/Phase/ProcessingUnit?

**Problem:** Traditional game engines often have:
- Unclear initialization order
- Hidden dependencies between systems
- Difficult to test in isolation
- Thread management mixed with business logic

**Solution:** Explicit lifecycle management with dependency injection:

- **Module** - Self-contained unit with explicit dependencies
- **Phase** - Groups modules into execution stages
- **ProcessingUnit** - Orchestrates phases on a thread

**Benefits:**
- ✅ Explicit dependency declaration (no hidden coupling)
- ✅ Automatic dependency ordering (topological sort)
- ✅ Clear lifecycle (Start → Update → Stop)
- ✅ Thread-safe by design (queued phase transitions)
- ✅ Testable in isolation (inject mock dependencies)
- ✅ Reusable across phases (module retention)

**Trade-offs:**
- ❌ Initial complexity overhead
- ❌ Boilerplate code (DoStart/DoUpdate/DoStop)
- ❌ Learning curve for new developers

**[→ Design rationale details](../reference/design-rationale/why-module-phase-pu.md)**

---

## Core Concepts

### ProcessingUnit

**Purpose:** Thread orchestrator that runs phases

**File:** `Dia/DiaApplication/ApplicationProcessingUnit.h`

**Key Responsibilities:**
- Execute phases in sequence
- Queue and process phase transitions (thread-safe)
- Control update frequency via `TimeThreadLimiter`
- Can run on separate thread via `operator()()`

**Lifecycle:**
```cpp
ProcessingUnit pu;
pu.Start();    // Initialize first phase
pu.Update();   // Run update loop
pu.Stop();     // Shutdown
```

**Threading:**
```cpp
// Run on separate thread
std::thread thread(processingUnit);  // Calls operator()()
thread.join();
```

**Phase Transitions:**
```cpp
// Thread-safe (queued with mutex)
void ProcessingUnit::TransitionPhase(Phase* newPhase);
```

**Update Loop:**
```cpp
void ProcessingUnit::Update() {
    while (mRunning) {
        // Process queued phase transition (if any)
        ProcessQueuedTransition();
        
        // Update current phase
        if (mCurrentPhase) {
            mCurrentPhase->Update();
        }
        
        // Frequency control (optional)
        if (mThreadLimiter) {
            mThreadLimiter->Limit();
        }
    }
}
```

**Example:**
```cpp
class MainProcessingUnit : public Dia::Application::ProcessingUnit {
public:
    MainProcessingUnit() {
        // Initial phase
        TransitionPhase(&mBootPhase);
    }
private:
    MainBootPhase mBootPhase;
    MainBootStrapPhase mBootStrapPhase;
};
```

---

### Phase

**Purpose:** Execution stage containing modules

**File:** `Dia/DiaApplication/ApplicationPhase.h`

**Key Responsibilities:**
- Manage module lifecycle (Start → Update → Stop)
- Resolve module dependencies (topological sort)
- Provide lifecycle hooks (Before/After module operations)
- Support module retention across transitions

**Lifecycle:**
```cpp
Phase phase;
phase.Start();    // Start all modules (dependency order)
phase.Update();   // Update all modules
phase.Stop();     // Stop all modules (reverse order)
```

**Module Management:**
```cpp
void Phase::AddModule(Module* module);                    // Add module
void Phase::RetainModule(Module* module);                 // Retain across transition
void Phase::RemoveModule(Module* module);                 // Remove module
```

**Lifecycle Hooks:**
```cpp
class MyPhase : public Dia::Application::Phase {
protected:
    void BeforeModulesStart() override;     // Called before modules start
    void AfterModulesStart() override;      // Called after modules start
    void BeforeModulesUpdate() override;    // Called before modules update
    void AfterModulesUpdate() override;     // Called after modules update
    void BeforeModulesStop() override;      // Called before modules stop
    void AfterModulesStop() override;       // Called after modules stop
};
```

**Dependency Resolution:**
```cpp
// Phase automatically orders modules based on dependencies
phase.AddModule(&moduleA);  // Depends on nothing
phase.AddModule(&moduleB);  // Depends on moduleA
phase.AddModule(&moduleC);  // Depends on moduleB

// Start order: A → B → C (topological sort)
// Stop order: C → B → A (reverse)
```

**Example:**
```cpp
class MainBootPhase : public Dia::Application::Phase {
public:
    MainBootPhase() {
        // Add modules
        AddModule(&mMainKernelModule);
        AddModule(&mLevelFactoryModule);  // Depends on Kernel
    }
    
protected:
    void AfterModulesStart() override {
        // Register levels after modules started
        RegisterLevels();
    }

private:
    MainKernelModule mMainKernelModule;
    LevelFactoryModule mLevelFactoryModule;
};
```

---

### Module

**Purpose:** Functional unit with explicit dependencies

**File:** `Dia/DiaApplication/ApplicationModule.h`

**Key Responsibilities:**
- Implement Start/Update/Stop lifecycle
- Declare dependencies on other modules
- Maintain internal state
- Provide services to other modules

**Lifecycle:**
```cpp
Module module;
module.Start();    // Initialize (calls DoStart)
module.Update();   // Per-frame update (calls DoUpdate)
module.Stop();     // Cleanup (calls DoStop)
```

**Dependency Declaration:**
```cpp
class MyModule : public Dia::Application::Module {
public:
    MyModule() {
        // Declare dependencies in constructor
        RegisterDependency(&GetDependency<OtherModule>());
    }
};
```

**Lifecycle Implementation:**
```cpp
class MyModule : public Dia::Application::Module {
private:
    void DoStart() override {
        // Initialize module
        mState = InitializeState();
    }
    
    void DoUpdate() override {
        // Per-frame update
        mState->Update();
    }
    
    void DoStop() override {
        // Cleanup
        mState->Cleanup();
    }
    
    State* mState;
};
```

**Accessing Dependencies:**
```cpp
class MyModule : public Dia::Application::Module {
public:
    MyModule() {
        RegisterDependency(&GetDependency<TimeServerModule>());
    }
    
private:
    void DoUpdate() override {
        // Access dependency
        TimeServerModule* timeServer = GetDependency<TimeServerModule>();
        float deltaTime = timeServer->GetDeltaTime();
    }
};
```

**Module States:**
```cpp
enum ModuleState {
    Idle,           // Not started
    Starting,       // DoStart() executing
    Updating,       // DoUpdate() executing
    Stopped         // DoStop() executed
};
```

**Example:**
```cpp
class MainKernelModule : public Dia::Application::Module {
public:
    MainKernelModule();
    
    // Access methods (used by dependent modules)
    Dia::Core::TimeServer* GetTimeServer() { return &mTimeServer; }
    Dia::Input::InputSourceManager* GetInputManager() { return &mInputManager; }
    Dia::Window::IWindow* GetWindow() { return mWindow; }
    Dia::Graphics::ICanvas* GetCanvas() { return mCanvas; }

private:
    void DoStart() override {
        mTimeServer.Start();
        mInputManager.Initialize();
        mWindow = CreateWindow();
        mCanvas = CreateCanvas();
    }
    
    void DoUpdate() override {
        mTimeServer.Update();
        mInputManager.Update();
    }
    
    void DoStop() override {
        delete mCanvas;
        delete mWindow;
    }
    
    Dia::Core::TimeServer mTimeServer;
    Dia::Input::InputSourceManager mInputManager;
    Dia::Window::IWindow* mWindow;
    Dia::Graphics::ICanvas* mCanvas;
};
```

---

## Module Lifecycle Details

### Start Phase

**Order:** Dependency order (topological sort)

```
1. Phase::Start() called
2. For each module (in dependency order):
   a. Module::DoStart() called
   b. Module transitions to Updating state
3. Phase::AfterModulesStart() called
```

**Example:**
```
MainKernelModule → LevelFactoryModule → MainUIModule
(no dependencies)   (depends on Kernel)  (depends on Kernel)
```

### Update Phase

**Order:** Same as start order

```
1. Phase::BeforeModulesUpdate() called
2. For each module (in dependency order):
   a. Module::DoUpdate() called
3. Phase::AfterModulesUpdate() called
4. Repeat until phase transition
```

**Per-Frame:**
```cpp
// Every frame:
BeforeModulesUpdate()
    Module A Update
    Module B Update
    Module C Update
AfterModulesUpdate()
```

### Stop Phase

**Order:** Reverse dependency order

```
1. Phase::BeforeModulesStop() called
2. For each module (in reverse order):
   a. Module::DoStop() called
   b. Module transitions to Stopped state
3. Phase::AfterModulesStop() called
```

**Example:**
```
MainUIModule → LevelFactoryModule → MainKernelModule
(reverse of start order)
```

---

## Dependency Management

### Declaring Dependencies

**In Constructor:**
```cpp
class MyModule : public Dia::Application::Module {
public:
    MyModule() {
        // Declare all dependencies upfront
        RegisterDependency(&GetDependency<DependencyA>());
        RegisterDependency(&GetDependency<DependencyB>());
    }
};
```

**Multiple Dependencies:**
```cpp
class SimUIProxyModule : public Dia::Application::Module {
public:
    SimUIProxyModule() {
        // Depends on TimeServer and ObserverSubject
        RegisterDependency(&GetDependency<SimTimeServerModule>());
        RegisterDependency(&GetDependency<MainUIModule>());  // Cross-thread
    }
};
```

### Accessing Dependencies

**Via GetDependency<T>():**
```cpp
void MyModule::DoUpdate() {
    // Access dependency (assumes already declared)
    TimeServerModule* timeServer = GetDependency<TimeServerModule>();
    float deltaTime = timeServer->GetDeltaTime();
}
```

**Via Direct Reference (Cached):**
```cpp
class MyModule : public Dia::Application::Module {
public:
    MyModule() {
        mTimeServer = &GetDependency<TimeServerModule>();
        RegisterDependency(mTimeServer);
    }
    
private:
    void DoUpdate() override {
        // Use cached reference
        float deltaTime = mTimeServer->GetDeltaTime();
    }
    
    TimeServerModule* mTimeServer;
};
```

### Dependency Resolution Algorithm

**Topological Sort:**

1. Build dependency graph (directed acyclic graph)
2. Find modules with no dependencies (roots)
3. Start roots first
4. Remove roots from graph
5. Repeat until all modules started

**Cycle Detection:**
- If cycle detected → assertion failure (debug builds)
- Cycles indicate design error (circular dependencies)

**Example:**
```
Modules: A, B, C, D
Dependencies:
  B depends on A
  C depends on A, B
  D depends on C

Dependency graph:
    A
   / \
  B   |
   \ /
    C
    |
    D

Start order: A → B → C → D
```

---

## Phase Transitions

### Transitioning Between Phases

**Thread-Safe Queue:**
```cpp
void ProcessingUnit::TransitionPhase(Phase* newPhase) {
    std::lock_guard<std::mutex> lock(mQueuedTransitionMutex);
    mQueuedPhaseTransition = newPhase;
}
```

**Processing Queue:**
```cpp
void ProcessingUnit::ProcessQueuedTransition() {
    Phase* newPhase = nullptr;
    {
        std::lock_guard<std::mutex> lock(mQueuedTransitionMutex);
        if (mQueuedPhaseTransition) {
            newPhase = mQueuedPhaseTransition;
            mQueuedPhaseTransition = nullptr;
        }
    }
    
    if (newPhase) {
        // Stop current phase
        if (mCurrentPhase) {
            mCurrentPhase->Stop();
        }
        
        // Start new phase
        mCurrentPhase = newPhase;
        mCurrentPhase->Start();
    }
}
```

**Example:**
```cpp
// MainBootPhase transitions to MainBootStrapPhase
void MainBootPhase::AfterModulesStart() {
    // Transition after registration complete
    GetProcessingUnit()->TransitionPhase(&mBootStrapPhase);
}
```

### Module Retention

**Purpose:** Keep module alive across phase transitions

**Usage:**
```cpp
// In first phase
Phase phaseA;
phaseA.AddModule(&sharedModule);

// In second phase (retain from first)
Phase phaseB;
phaseB.RetainModule(&sharedModule);  // Don't Stop, keep running
```

**Lifecycle:**
```
Phase A: Start Module → Update Module → Don't Stop (retained)
Phase B: Don't Start (retained) → Update Module → Stop Module
```

**Example:**
```cpp
// MainKernelModule retained across all Main thread phases
class MainBootPhase : public Phase {
    MainBootPhase() {
        AddModule(&mMainKernelModule);  // Start in Boot
    }
};

class MainBootStrapPhase : public Phase {
    MainBootStrapPhase() {
        RetainModule(&mMainKernelModule);  // Retain (don't restart)
    }
};
```

---

## Threading Considerations

### Thread Safety

**ProcessingUnit:**
- Phase transition queue protected by mutex
- Safe to call `TransitionPhase()` from any thread
- Actual transition happens on ProcessingUnit's thread

**Phase:**
- Not thread-safe (accessed only by owning ProcessingUnit)
- Modules started/updated/stopped on single thread

**Module:**
- Not inherently thread-safe (depends on implementation)
- If shared across threads, module must use mutex

### Cross-Thread Modules

**Example:** `MainUIModule` observed by `SimUIProxyModule`

**MainUIModule (Main Thread):**
```cpp
class MainUIModule : public Module, public ObserverSubject {
    void DoStart() override {
        InitializeUI();
        Notify();  // Notify observers (thread-safe)
    }
};
```

**SimUIProxyModule (Sim Thread):**
```cpp
class SimUIProxyModule : public Module, public Observer {
public:
    SimUIProxyModule() {
        // Observe MainUIModule (cross-thread)
        RegisterDependency(&GetDependency<MainUIModule>());
    }
    
    void OnNotify() override {
        std::lock_guard<std::mutex> lock(mMutex);
        mUIReady = true;
    }
    
private:
    std::mutex mMutex;
    bool mUIReady = false;
};
```

**[→ Threading model details](threading-model.md)**

---

## Common Patterns

### Pattern 1: Kernel Module

**Purpose:** Provide core services to other modules

**Structure:**
```cpp
class KernelModule : public Module {
public:
    // Service accessors
    TimeServer* GetTimeServer() { return &mTimeServer; }
    InputManager* GetInputManager() { return &mInputManager; }
    Window* GetWindow() { return mWindow; }
    
private:
    // Core services
    TimeServer mTimeServer;
    InputManager mInputManager;
    Window* mWindow;
};
```

**Usage:**
```cpp
class DependentModule : public Module {
public:
    DependentModule() {
        RegisterDependency(&GetDependency<KernelModule>());
    }
    
private:
    void DoUpdate() override {
        KernelModule* kernel = GetDependency<KernelModule>();
        float deltaTime = kernel->GetTimeServer()->GetDeltaTime();
    }
};
```

### Pattern 2: Factory Module

**Purpose:** Provide access to factory/registry

**Structure:**
```cpp
class FactoryModule : public Module {
public:
    LevelFactory* GetLevelFactory() { return &LevelFactory::Instance(); }
};
```

**Usage:**
```cpp
void Phase::AfterModulesStart() {
    FactoryModule* factory = GetModule<FactoryModule>();
    ILevel* level = factory->GetLevelFactory()->Create("DummyLevel");
}
```

### Pattern 3: Proxy Module

**Purpose:** Bridge cross-thread communication

**Structure:**
```cpp
class ProxyModule : public Module, public Observer {
public:
    ProxyModule() {
        RegisterDependency(&GetDependency<RemoteModule>());
    }
    
    void SendMessage(const char* msg) {
        std::lock_guard<std::mutex> lock(mMutex);
        mRemote->ReceiveMessage(msg);
    }
    
private:
    void OnNotify() override {
        std::lock_guard<std::mutex> lock(mMutex);
        mRemoteReady = true;
    }
    
    RemoteModule* mRemote;
    std::mutex mMutex;
    bool mRemoteReady = false;
};
```

---

## Examples

### Example 1: Simple Module

```cpp
// MyModule.h
class MyModule : public Dia::Application::Module {
public:
    MyModule();
    
    int GetValue() const { return mValue; }

private:
    void DoStart() override;
    void DoUpdate() override;
    void DoStop() override;
    
    int mValue;
};

// MyModule.cpp
MyModule::MyModule() : mValue(0) {
    // No dependencies
}

void MyModule::DoStart() {
    mValue = 42;
    DIA_LOG("MyModule", "Started with value %d", mValue);
}

void MyModule::DoUpdate() {
    mValue++;
}

void MyModule::DoStop() {
    DIA_LOG("MyModule", "Stopped with value %d", mValue);
}
```

### Example 2: Module with Dependencies

```cpp
// DependentModule.h
class DependentModule : public Dia::Application::Module {
public:
    DependentModule();

private:
    void DoStart() override;
    void DoUpdate() override;
    void DoStop() override;
};

// DependentModule.cpp
DependentModule::DependentModule() {
    // Declare dependencies
    RegisterDependency(&GetDependency<TimeServerModule>());
    RegisterDependency(&GetDependency<InputModule>());
}

void DependentModule::DoStart() {
    TimeServerModule* timeServer = GetDependency<TimeServerModule>();
    DIA_LOG("DependentModule", "Starting at time %f", timeServer->GetTime());
}

void DependentModule::DoUpdate() {
    TimeServerModule* timeServer = GetDependency<TimeServerModule>();
    InputModule* input = GetDependency<InputModule>();
    
    float deltaTime = timeServer->GetDeltaTime();
    if (input->IsKeyPressed(Key::Space)) {
        DIA_LOG("DependentModule", "Space pressed at dt=%f", deltaTime);
    }
}

void DependentModule::DoStop() {
    DIA_LOG("DependentModule", "Stopping");
}
```

### Example 3: Phase with Multiple Modules

```cpp
// MyPhase.h
class MyPhase : public Dia::Application::Phase {
public:
    MyPhase();

protected:
    void AfterModulesStart() override;
    void BeforeModulesUpdate() override;

private:
    MyModule mModuleA;
    DependentModule mModuleB;
};

// MyPhase.cpp
MyPhase::MyPhase() {
    // Add modules (order doesn't matter, dependencies resolve automatically)
    AddModule(&mModuleB);  // Depends on nothing in this phase
    AddModule(&mModuleA);  // Depends on nothing
}

void MyPhase::AfterModulesStart() {
    DIA_LOG("MyPhase", "All modules started");
}

void MyPhase::BeforeModulesUpdate() {
    // Called before modules update each frame
}
```

---

## Best Practices

### ✅ Do:

1. **Declare all dependencies in constructor**
   ```cpp
   MyModule::MyModule() {
       RegisterDependency(&GetDependency<Dep>());
   }
   ```

2. **Keep modules focused (single responsibility)**
   - One module = one concern
   - Example: TimeServerModule only manages time

3. **Use lifecycle hooks appropriately**
   - `DoStart()` - Initialize state
   - `DoUpdate()` - Per-frame update
   - `DoStop()` - Cleanup resources

4. **Cache dependency references** (if accessed frequently)
   ```cpp
   MyModule::MyModule() {
       mTimeServer = &GetDependency<TimeServerModule>();
       RegisterDependency(mTimeServer);
   }
   ```

5. **Use phase hooks for cross-module operations**
   ```cpp
   void Phase::AfterModulesStart() {
       // All modules started, safe to interact
       RegisterLevels();
   }
   ```

### ❌ Don't:

1. **Access dependencies before Start()**
   ```cpp
   // BAD: Constructor access
   MyModule::MyModule() {
       mTimeServer = &GetDependency<TimeServerModule>();
       float time = mTimeServer->GetTime();  // CRASH (not started yet)
   }
   ```

2. **Create circular dependencies**
   ```cpp
   // BAD: A depends on B, B depends on A
   ModuleA depends on ModuleB
   ModuleB depends on ModuleA  // CYCLE (assertion failure)
   ```

3. **Store raw pointers without declaring dependency**
   ```cpp
   // BAD: No dependency declared
   MyModule::DoStart() {
       mTimeServer = SomeGlobal::GetTimeServer();  // Hidden dependency
   }
   ```

4. **Perform heavy work in constructor**
   ```cpp
   // BAD: Heavy work in constructor
   MyModule::MyModule() {
       LoadHugeFile();  // Should be in DoStart()
   }
   ```

5. **Transition phases from DoUpdate()**
   ```cpp
   // BAD: Immediate transition
   MyModule::DoUpdate() {
       GetProcessingUnit()->TransitionPhase(&newPhase);  // Queued, OK
       // Current phase still updating! Confusing.
   }
   // BETTER: Transition from Phase hook
   Phase::AfterModulesUpdate() {
       TransitionPhase(&newPhase);
   }
   ```

---

## Further Reading

### Architecture
- [Architecture Overview](architecture.md) - Complete system architecture
- [Threading Model](threading-model.md) - Multi-threaded design details
- [Module Dependencies Diagram](diagrams/module-dependencies.mmd) - Visual module relationships

### Design
- [Why Module/Phase/PU?](../reference/design-rationale/why-module-phase-pu.md) - Design rationale
- [Design Patterns](../reference/design-rationale/design-patterns.md) - Common patterns

### API Documentation
- [DiaApplication API](../reference/api/dia/application-api.md) - Module/Phase/PU API reference
- [Module Lifecycle](../reference/subsystems/dia-application/module-lifecycle.md) - Detailed lifecycle

### For AI Agents
- [Patterns Reference](../reference/ai-guides/patterns-reference.md) - Code patterns with examples
- [Entry Points](../reference/ai-guides/entry-points.md) - Where to start for common tasks

---

## Summary

The **Module/Phase/ProcessingUnit** pattern provides:
- ✅ Explicit dependency management
- ✅ Automatic dependency ordering
- ✅ Clear lifecycle (Start → Update → Stop)
- ✅ Thread-safe phase transitions
- ✅ Testable, reusable, composable modules

**Key Concepts:**
- **Module** - Functional unit with dependencies
- **Phase** - Execution stage containing modules
- **ProcessingUnit** - Thread orchestrator running phases

**Lifecycle:**
1. Declare dependencies in constructor
2. Phase starts modules (dependency order)
3. Phase updates modules (each frame)
4. Phase stops modules (reverse order)

**[→ Back to Architecture Overview](architecture.md)**
# DiaApplication API

**Last Updated:** 2026-04-01

Application framework API providing ProcessingUnit, Phase, and Module architecture.

---

## Overview

**DiaApplication** is the core framework layer that provides the Module/Phase/ProcessingUnit pattern for multi-threaded application architecture.

**Location:** `Dia/DiaApplication/`

**Namespace:** `Dia::Application::`

**Key Concepts:**
- **ProcessingUnit** - Thread container
- **Phase** - Execution stage within a ProcessingUnit
- **Module** - Functional unit that phases depend on
- **StateObject** - Base for stateful objects
- **ILevel** - Level interface
- **FrameStream** - Thread-safe queue

---

## Core Classes

### ProcessingUnit

**Header:** `Dia/DiaApplication/ApplicationProcessingUnit.h`

**Purpose:** Top-level container that runs on a thread and manages phases.

#### Key Methods

```cpp
class ProcessingUnit
{
public:
    ProcessingUnit(const StringCRC& id);
    virtual ~ProcessingUnit();
    
    // Lifecycle
    void Start();
    void Update();
    void Stop();
    
    // Phase management
    void TransitionPhase(Phase* newPhase);
    void QueuePhaseTransition(Phase* newPhase);  // Thread-safe
    
    // Query
    Phase* GetCurrentPhase() const;
    const StringCRC& GetId() const;
    bool IsRunning() const;
    
protected:
    // Override in subclass
    virtual void DoStart() = 0;
    virtual void DoUpdate() = 0;
    virtual void DoStop() = 0;
};
```

#### Usage Example

```cpp
class MainProcessingUnit : public Dia::Application::ProcessingUnit
{
public:
    static const Dia::Core::StringCRC kUniqueId;
    
    MainProcessingUnit()
        : ProcessingUnit(kUniqueId)
    {
    }
    
private:
    void DoStart() override
    {
        // Initial phase
        TransitionPhase(new MainBootPhase());
    }
    
    void DoUpdate() override
    {
        // Update current phase
        if (GetCurrentPhase())
        {
            GetCurrentPhase()->Update();
        }
    }
    
    void DoStop() override
    {
        // Cleanup
    }
};

const Dia::Core::StringCRC MainProcessingUnit::kUniqueId = 
    Dia::Core::StringCRC("MainProcessingUnit");
```

#### Thread Safety

- `TransitionPhase()` - **Same-thread only**
- `QueuePhaseTransition()` - **Thread-safe** (use for cross-thread transitions)

---

### Phase

**Header:** `Dia/DiaApplication/ApplicationPhase.h`

**Purpose:** Execution stage that contains modules.

#### Key Methods

```cpp
class Phase
{
public:
    Phase(const StringCRC& id);
    virtual ~Phase();
    
    // Lifecycle
    void Start();
    void Update();
    void Stop();
    
    // Module management
    void AddModule(Module* module);
    template<typename T>
    T* GetModule();
    
    // Query
    const StringCRC& GetId() const;
    ProcessingUnit* GetProcessingUnit() const;
    
protected:
    // Override in subclass
    virtual void DoStart() = 0;
    virtual void DoUpdate() = 0;
    virtual void DoStop() = 0;
    
    // Utility
    void UpdateAllModules();
};
```

#### Usage Example

```cpp
class MainBootPhase : public Dia::Application::Phase
{
public:
    static const Dia::Core::StringCRC kUniqueId;
    
    MainBootPhase()
        : Phase(kUniqueId)
    {
    }
    
private:
    void DoStart() override
    {
        // Add modules (automatic dependency resolution)
        AddModule(new MainKernelModule());
        AddModule(new MainUIModule());
        AddModule(new LevelFactoryModule());
    }
    
    void DoUpdate() override
    {
        // Update all modules
        UpdateAllModules();
        
        // Check for phase transition
        if (ShouldTransitionToBootStrap())
        {
            GetProcessingUnit()->TransitionPhase(new MainBootStrapPhase());
        }
    }
    
    void DoStop() override
    {
        // Modules cleaned up automatically
    }
    
    bool ShouldTransitionToBootStrap() const
    {
        // Transition logic
        return true;
    }
};

const Dia::Core::StringCRC MainBootPhase::kUniqueId = 
    Dia::Core::StringCRC("MainBootPhase");
```

#### Module Dependency Resolution

Modules are automatically started in correct order based on dependencies:

```cpp
// ModuleB depends on ModuleA
AddModule(new ModuleB());  // Declares dependency on ModuleA
AddModule(new ModuleA());  // Order doesn't matter

// Phase will start ModuleA before ModuleB automatically
```

---

### Module

**Header:** `Dia/DiaApplication/ApplicationModule.h`

**Purpose:** Functional unit with explicit dependencies and lifecycle.

#### Key Methods

```cpp
class Module
{
public:
    Module(const StringCRC& id);
    virtual ~Module();
    
    // Lifecycle
    void Start();
    void Update();
    void Stop();
    
    // Query
    const StringCRC& GetId() const;
    bool IsStarted() const;
    
protected:
    // Dependencies
    template<typename T>
    T& GetDependency();
    
    void RegisterDependency(Module* dependency);
    
    // Override in subclass
    virtual void DoStart() = 0;
    virtual void DoUpdate() = 0;
    virtual void DoStop() = 0;
};
```

#### Usage Example

```cpp
class SimTimeServerModule : public Dia::Application::Module
{
public:
    static const Dia::Core::StringCRC kUniqueId;
    
    SimTimeServerModule()
        : Module(kUniqueId)
        , mTimeServer(nullptr)
    {
        // No dependencies for this module
    }
    
    // Public API
    float GetDeltaTime() const
    {
        return mTimeServer ? mTimeServer->GetDeltaTime() : 0.0f;
    }
    
private:
    void DoStart() override
    {
        // Create TimeServer singleton
        Dia::Core::TimeServer::Create();
        mTimeServer = Dia::Core::TimeServer::Instance();
    }
    
    void DoUpdate() override
    {
        // Update time
        if (mTimeServer)
        {
            mTimeServer->Update();
        }
    }
    
    void DoStop() override
    {
        // Destroy TimeServer
        Dia::Core::TimeServer::Destroy();
        mTimeServer = nullptr;
    }
    
    Dia::Core::TimeServer* mTimeServer;
};

const Dia::Core::StringCRC SimTimeServerModule::kUniqueId = 
    Dia::Core::StringCRC("SimTimeServerModule");
```

#### With Dependencies

```cpp
class SimPhysicsModule : public Dia::Application::Module
{
public:
    SimPhysicsModule()
        : Module(kUniqueId)
        , mTimeServer(nullptr)
    {
        // Register dependency
        RegisterDependency(&GetDependency<SimTimeServerModule>());
    }
    
private:
    void DoStart() override
    {
        // Get dependency (guaranteed to be started first)
        mTimeServer = GetDependency<SimTimeServerModule>();
        DIA_ASSERT(mTimeServer != nullptr, "TimeServer missing");
    }
    
    void DoUpdate() override
    {
        // Use dependency
        float deltaTime = mTimeServer->GetDeltaTime();
        UpdatePhysics(deltaTime);
    }
    
    SimTimeServerModule* mTimeServer;
};
```

---

### StateObject

**Header:** `Dia/DiaApplication/ApplicationStateObject.h`

**Purpose:** Base class for objects with lifecycle (levels, game states).

#### Key Methods

```cpp
class StateObject
{
public:
    StateObject(const StringCRC& id);
    virtual ~StateObject();
    
    // Lifecycle
    void Start();
    void Update();
    void Stop();
    
    // Query
    const StringCRC& GetId() const;
    bool IsStarted() const;
    
protected:
    virtual void DoStart() = 0;
    virtual void DoUpdate() = 0;
    virtual void DoStop() = 0;
};
```

#### Usage Example (Level)

```cpp
class DummyLevel : public Dia::Application::StateObject
{
public:
    static const Dia::Core::StringCRC kUniqueId;
    
    DummyLevel()
        : StateObject(kUniqueId)
    {
    }
    
private:
    void DoStart() override
    {
        DIA_LOG("DummyLevel: Starting");
        // Initialize level
    }
    
    void DoUpdate() override
    {
        // Update game logic
    }
    
    void DoStop() override
    {
        DIA_LOG("DummyLevel: Stopping");
        // Cleanup level
    }
};
```

---

### ILevel (Interface)

**Header:** `Dia/DiaApplication/ApplicationStateObject.h`

**Purpose:** Level interface (alias for StateObject).

```cpp
typedef StateObject ILevel;
```

---

### LevelFactory

**Header:** `Dia/DiaApplication/LevelFactory.h`

**Purpose:** Create levels by name at runtime.

#### Key Methods

```cpp
class LevelFactory : public Dia::Core::Singleton<LevelFactory>
{
public:
    // Register level type
    template<typename T>
    void Register(const char* name);
    
    // Create level by name
    ILevel* Create(const char* name);
};
```

#### Usage Example

```cpp
// Registration (usually in LevelFactoryModule)
LevelFactory::Create();
LevelFactory::Instance()->Register<DummyLevel>("DummyLevel");
LevelFactory::Instance()->Register<UnitTestLevel>("UnitTestLevel");

// Creation
ILevel* level = LevelFactory::Instance()->Create("DummyLevel");
if (level)
{
    level->Start();
    // ...
    level->Stop();
    delete level;
}

// Cleanup
LevelFactory::Destroy();
```

---

### FrameStream<T>

**Header:** `Dia/DiaApplication/FrameStream.h`

**Purpose:** Thread-safe producer-consumer queue for cross-thread communication.

#### Key Methods

```cpp
template<typename T>
class FrameStream
{
public:
    FrameStream();
    ~FrameStream();
    
    // Thread-safe operations
    void Write(const T& data);
    bool Read(T& outData);
    
    // Query
    bool HasData() const;
};
```

#### Usage Example

```cpp
// Shared between Main and Sim threads
Dia::Application::FrameStream<InputEvent> mInputFrameStream;

// Producer (Main thread)
void MainThread::OnInput(const InputEvent& event)
{
    mInputFrameStream.Write(event);  // Thread-safe
}

// Consumer (Sim thread)
void SimThread::Update()
{
    InputEvent event;
    while (mInputFrameStream.Read(event))  // Thread-safe
    {
        ProcessInput(event);
    }
}
```

#### Thread Safety

- **All methods thread-safe** (uses `std::mutex` internally)
- **Non-blocking:** `Read()` returns `false` if empty
- **FIFO ordering**

---

## Common Patterns

### Creating a Processing Unit

1. Inherit from `ProcessingUnit`
2. Implement `DoStart()`, `DoUpdate()`, `DoStop()`
3. Create initial phase in `DoStart()`
4. Update current phase in `DoUpdate()`

```cpp
class MyProcessingUnit : public ProcessingUnit
{
public:
    MyProcessingUnit() : ProcessingUnit(kUniqueId) {}
    
private:
    void DoStart() override
    {
        TransitionPhase(new MyBootPhase());
    }
    
    void DoUpdate() override
    {
        if (GetCurrentPhase())
        {
            GetCurrentPhase()->Update();
        }
    }
    
    void DoStop() override { }
};
```

---

### Creating a Phase

1. Inherit from `Phase`
2. Add modules in `DoStart()`
3. Update modules in `DoUpdate()`
4. Handle phase transitions

```cpp
class MyPhase : public Phase
{
private:
    void DoStart() override
    {
        AddModule(new Module1());
        AddModule(new Module2());
    }
    
    void DoUpdate() override
    {
        UpdateAllModules();
        
        if (ShouldTransition())
        {
            GetProcessingUnit()->TransitionPhase(new NextPhase());
        }
    }
    
    void DoStop() override { }
};
```

---

### Creating a Module

1. Inherit from `Module`
2. Register dependencies in constructor
3. Get dependencies in `DoStart()`
4. Implement update logic

```cpp
class MyModule : public Module
{
public:
    MyModule()
        : Module(kUniqueId)
    {
        RegisterDependency(&GetDependency<OtherModule>());
    }
    
private:
    void DoStart() override
    {
        mDependency = GetDependency<OtherModule>();
    }
    
    void DoUpdate() override
    {
        mDependency->DoSomething();
    }
    
    void DoStop() override
    {
        mDependency = nullptr;
    }
    
    OtherModule* mDependency;
};
```

---

## Dependencies

**Required:**
- `Dia/DiaCore/` - Containers, StringCRC, Singleton, Observer

**Uses:**
- `std::thread`, `std::mutex` - Threading
- `std::queue` - FrameStream implementation

---

## Thread Safety

| Class/Method | Thread Safety |
|-------------|---------------|
| `ProcessingUnit::TransitionPhase()` | ❌ Same-thread only |
| `ProcessingUnit::QueuePhaseTransition()` | ✅ Thread-safe |
| `FrameStream::Write/Read()` | ✅ Thread-safe |
| `Module` methods | ❌ Call from owning thread only |

---

## Best Practices

### 1. Use QueuePhaseTransition for Cross-Thread

```cpp
// ✅ Good: Thread-safe
mainPU->QueuePhaseTransition(new NextPhase());

// ❌ Bad: Not thread-safe
mainPU->TransitionPhase(new NextPhase());  // From different thread
```

---

### 2. Declare Dependencies in Constructor

```cpp
// ✅ Good: Early declaration
MyModule::MyModule()
{
    RegisterDependency(&GetDependency<OtherModule>());
}

// ❌ Bad: Late declaration
void MyModule::DoStart()
{
    RegisterDependency(&GetDependency<OtherModule>());  // Too late!
}
```

---

### 3. Nullify Dependencies in DoStop

```cpp
void MyModule::DoStop()
{
    mDependency = nullptr;  // Dependency may be deleted
}
```

---

### 4. Use FrameStream for Cross-Thread Data

```cpp
// ✅ Good: Thread-safe queue
mFrameStream.Write(data);

// ❌ Bad: Direct shared state
sharedVariable = data;  // Race condition!
```

---

## Gotchas

### Gotcha 1: Module Destruction Order

Modules destroyed in **reverse dependency order**. Dependencies nullified in `DoStop()`.

---

### Gotcha 2: Phase Transition Timing

Phase transitions happen **after current update completes**.

```cpp
void MyPhase::DoUpdate()
{
    DoWork();
    GetProcessingUnit()->TransitionPhase(new NextPhase());
    // This code still runs (transition happens after DoUpdate)
    DoMoreWork();
}
```

---

### Gotcha 3: FrameStream Not Synchronized

`FrameStream` queues data, doesn't synchronize threads. Producer and consumer run independently.

---

## Summary

**Core Classes:**
- `ProcessingUnit` - Thread container
- `Phase` - Execution stage
- `Module` - Functional unit
- `StateObject`/`ILevel` - Stateful objects
- `LevelFactory` - Runtime level creation
- `FrameStream<T>` - Thread-safe queue

**Key Features:**
- Automatic module dependency resolution
- Explicit phase transitions
- Thread-safe cross-thread communication
- Type-safe module retrieval

**Thread Safety:**
- Use `QueuePhaseTransition()` for cross-thread
- Use `FrameStream<T>` for data transfer
- Module methods not thread-safe

**[→ API Overview](../api-overview.md)**  
**[→ DiaCore API](core-api.md)**  
**[→ Architecture Details](../../architecture/module-system.md)**

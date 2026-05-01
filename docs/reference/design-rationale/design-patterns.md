# Design Patterns

**Last Updated:** 2026-04-01

Common design patterns used throughout Cluiche and Dia engine.

---

## Core Patterns

### 1. Module Pattern (Custom)

**Purpose:** Reusable, testable component with explicit dependencies

**Structure:**
```cpp
class MyModule : public Dia::Application::Module {
public:
    MyModule() {
        RegisterDependency(&GetDependency<DependencyA>());
    }

private:
    void DoStart() override {
        // Initialize
    }
    
    void DoUpdate() override {
        // Per-frame update
    }
    
    void DoStop() override {
        // Cleanup
    }
};
```

**When to Use:**
- Need reusable functionality
- Have dependencies on other modules
- Want testable components

**Benefits:**
- ✅ Explicit dependencies
- ✅ Clear lifecycle
- ✅ Testable in isolation

**[→ Module System Details](../architecture/module-system.md)**

---

### 2. Factory Pattern

**Purpose:** Create objects by type name without knowing concrete class

**Structure:**
```cpp
class LevelFactory {
public:
    template<typename T>
    void Register(const char* name) {
        mRegistry[StringCRC(name)] = []() { return new T(); };
    }
    
    ILevel* Create(const char* name) {
        StringCRC crc(name);
        auto it = mRegistry.find(crc);
        return it != mRegistry.end() ? it->second() : nullptr;
    }

private:
    std::map<StringCRC, std::function<ILevel*()>> mRegistry;
};
```

**When to Use:**
- Create objects at runtime by name
- Pluggable architecture (levels, components)
- Avoid hard-coded dependencies

**Example:**
```cpp
LevelFactory::Instance().Register<DummyLevel>("DummyLevel");
ILevel* level = LevelFactory::Instance().Create("DummyLevel");
```

**Benefits:**
- ✅ Pluggable (register new types easily)
- ✅ Runtime creation
- ✅ Type-safe (template registration)

---

### 3. Observer Pattern

**Purpose:** Notify multiple observers when event occurs

**Structure:**
```cpp
class Observer {
public:
    virtual void OnNotify() = 0;
};

class ObserverSubject {
public:
    void Attach(Observer* observer) {
        mObservers.PushBack(observer);
    }
    
    void Detach(Observer* observer) {
        mObservers.Remove(observer);
    }

protected:
    void Notify() {
        std::lock_guard<std::mutex> lock(mMutex);
        for (Observer* obs : mObservers) {
            obs->OnNotify();
        }
    }

private:
    DynamicArray<Observer*> mObservers;
    std::mutex mMutex;  // Thread-safe
};
```

**When to Use:**
- One-to-many notifications
- Cross-thread communication
- Event-driven architecture

**Example:**
```cpp
// MainUIModule (Main thread) notifies SimUIProxyModule (Sim thread)
class MainUIModule : public ObserverSubject {
    void DoStart() override {
        InitializeUI();
        Notify();  // UI ready
    }
};

class SimUIProxyModule : public Observer {
    void OnNotify() override {
        // UI is ready, can now use proxy
        mUIReady = true;
    }
};
```

**Benefits:**
- ✅ Decoupled (subject doesn't know observers)
- ✅ Thread-safe (mutex-protected)
- ✅ Multiple observers

---

### 4. Singleton Pattern

**Purpose:** Single global instance

**Structure:**
```cpp
template<typename T>
class Singleton {
public:
    static void Create() {
        DIA_ASSERT(!sInstance);
        sInstance = new T();
    }
    
    static void Destroy() {
        delete sInstance;
        sInstance = nullptr;
    }
    
    static T* GetInstance() {
        DIA_ASSERT(sInstance);
        return sInstance;
    }
    
    static bool IsCreated() {
        return sInstance != nullptr;
    }

private:
    static T* sInstance;
};

template<typename T>
T* Singleton<T>::sInstance = nullptr;
```

**When to Use:**
- Truly global resource (TimeServer, TypeRegistry)
- Need single instance (LevelFactory)
- Avoid overuse (prefer dependency injection)

**Example:**
```cpp
class LevelFactory : public Singleton<LevelFactory> {
    // ...
};

// Usage
LevelFactory::Create();
LevelFactory::Instance()->Register<DummyLevel>("DummyLevel");
LevelFactory::Destroy();
```

**Benefits:**
- ✅ Global access
- ✅ Controlled creation/destruction
- ✅ Clear singleton status

**Drawbacks:**
- ❌ Global state (hard to test)
- ❌ Hidden dependencies
- ⚠️ Use sparingly!

---

### 5. State Pattern (Phases)

**Purpose:** Object changes behavior based on internal state

**Structure:**
```cpp
class Phase {  // State
public:
    virtual void Start() = 0;
    virtual void Update() = 0;
    virtual void Stop() = 0;
};

class ProcessingUnit {  // Context
public:
    void TransitionPhase(Phase* newPhase) {
        if (mCurrentPhase) {
            mCurrentPhase->Stop();
        }
        mCurrentPhase = newPhase;
        mCurrentPhase->Start();
    }
    
    void Update() {
        if (mCurrentPhase) {
            mCurrentPhase->Update();
        }
    }

private:
    Phase* mCurrentPhase;
};
```

**When to Use:**
- Object has distinct states
- State transitions are explicit
- Behavior changes based on state

**Example:**
```cpp
// Phases are states
MainBootPhase → MainBootStrapPhase → (Running)
```

**Benefits:**
- ✅ Clear state transitions
- ✅ State-specific behavior
- ✅ Easy to add new states

---

### 6. Proxy Pattern

**Purpose:** Control access to object (cross-thread in our case)

**Structure:**
```cpp
class SimUIProxyModule {
public:
    void SendMessage(const char* msg) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mUIReady) {
            mUISystem->SendMessage(msg);  // Proxy to Main thread UI
        }
    }

private:
    MainUIModule* mUISystem;  // Real object (on Main thread)
    std::mutex mMutex;        // Synchronization
    bool mUIReady;
};
```

**When to Use:**
- Control access to object
- Cross-thread communication
- Add synchronization

**Example:**
```cpp
// Sim thread uses proxy to access Main thread UI
simUIProxy->SendMessage("button_clicked");
```

**Benefits:**
- ✅ Thread-safe access
- ✅ Transparent to client
- ✅ Can add logic (caching, validation)

---

### 7. Template Method Pattern

**Purpose:** Define algorithm skeleton, let subclasses override steps

**Structure:**
```cpp
class Module {
public:
    void Start() {  // Template method
        DoStart();  // Subclass implements
    }
    
    void Update() {  // Template method
        DoUpdate();  // Subclass implements
    }
    
    void Stop() {  // Template method
        DoStop();  // Subclass implements
    }

protected:
    virtual void DoStart() = 0;   // Hook
    virtual void DoUpdate() = 0;  // Hook
    virtual void DoStop() = 0;    // Hook
};
```

**When to Use:**
- Common algorithm with variable steps
- Enforce lifecycle structure
- Framework design

**Example:**
```cpp
class MyModule : public Module {
private:
    void DoStart() override {
        // Custom initialization
    }
    
    void DoUpdate() override {
        // Custom update
    }
    
    void DoStop() override {
        // Custom cleanup
    }
};
```

**Benefits:**
- ✅ Code reuse (common structure)
- ✅ Enforced lifecycle
- ✅ Subclass customization

---

## Creational Patterns

### Object Pool (Component Factory)

**Purpose:** Reuse objects instead of allocating/deallocating

**Structure:**
```cpp
template<typename T, unsigned int Capacity>
class StaticPooledComponentFactory {
public:
    T* Create() {
        if (mFreeList.IsEmpty()) {
            return nullptr;  // Pool exhausted
        }
        T* obj = mFreeList.PopBack();
        obj->Create();  // Initialize
        return obj;
    }
    
    void Destroy(T* obj) {
        obj->Destroy();  // Cleanup
        mFreeList.PushBack(obj);  // Return to pool
    }

private:
    Array<T, Capacity> mPool;
    DynamicArray<T*> mFreeList;
};
```

**When to Use:**
- Frequent allocation/deallocation
- Predictable object count
- Performance critical (avoid heap fragmentation)

**Benefits:**
- ✅ No dynamic allocation (fast)
- ✅ Cache-friendly (contiguous memory)
- ✅ Predictable memory usage

---

## Structural Patterns

### Composite Pattern (Scene Graph - Future)

**Purpose:** Treat individual objects and compositions uniformly

**Structure:**
```cpp
class SceneNode {
public:
    virtual void Update() {
        for (SceneNode* child : mChildren) {
            child->Update();
        }
    }
    
    void AddChild(SceneNode* child) {
        mChildren.PushBack(child);
    }

private:
    DynamicArray<SceneNode*> mChildren;
};
```

**When to Use:**
- Tree structures (scene graph, UI hierarchy)
- Uniform treatment of leaf and composite objects

**Note:** Not fully implemented yet, planned for future

---

## Behavioral Patterns

### Visitor Pattern (Debug Rendering)

**Purpose:** Separate algorithm from data structure

**Structure:**
```cpp
class DebugFrameDataVisitor {
public:
    virtual void Visit(LineData* line) = 0;
    virtual void Visit(CircleData* circle) = 0;
    virtual void Visit(TextData* text) = 0;
};

class LineData {
public:
    void Accept(DebugFrameDataVisitor* visitor) {
        visitor->Visit(this);
    }
};

class RenderVisitor : public DebugFrameDataVisitor {
    void Visit(LineData* line) override {
        canvas->DrawLine(line->start, line->end, line->color);
    }
    
    void Visit(CircleData* circle) override {
        canvas->DrawCircle(circle->center, circle->radius, circle->color);
    }
};
```

**When to Use:**
- Multiple operations on same data structure
- Add new operations without modifying classes
- Separate concerns (data vs algorithm)

**Benefits:**
- ✅ Open/closed principle (add operations without modifying data)
- ✅ Single responsibility (each visitor does one thing)
- ✅ Type-safe dispatch

---

## Anti-Patterns Avoided

### God Object

**Anti-Pattern:**
```cpp
class GameManager {
    void UpdateInput();
    void UpdatePhysics();
    void UpdateGraphics();
    void UpdateAI();
    void UpdateUI();
    // 100 more methods...
};
```

**Solution: Module Decomposition**
```cpp
InputModule
PhysicsModule
GraphicsModule
AIModule
UIModule
```

---

### Singleton Overuse

**Anti-Pattern:**
```cpp
InputManager::Instance()
PhysicsManager::Instance()
GraphicsManager::Instance()
// Everything is singleton!
```

**Solution: Dependency Injection**
```cpp
class MyModule : public Module {
    MyModule() {
        RegisterDependency(&GetDependency<InputModule>());
    }
};
```

**When Singleton is OK:**
- LevelFactory (truly global registry)
- TimeServer (single time source)
- TypeRegistry (single type database)

---

### Stringly-Typed

**Anti-Pattern:**
```cpp
void* GetComponent(const char* name);  // Error-prone
```

**Solution: Type-Safe**
```cpp
template<typename T>
T* GetDependency();  // Compile-time safety
```

---

## Pattern Combinations

### Module + Factory

```cpp
// Factory creates modules
ModuleFactory::Register<PhysicsModule>("Physics");
Module* physics = ModuleFactory::Create("Physics");

// Phase adds module
phase.AddModule(physics);
```

---

### Observer + Proxy

```cpp
// MainUIModule is ObserverSubject
class MainUIModule : public ObserverSubject {
    void DoStart() override {
        Notify();  // Notify observers
    }
};

// SimUIProxyModule is Observer + Proxy
class SimUIProxyModule : public Observer {
    void OnNotify() override {
        // Proxy to UI (with mutex)
    }
};
```

---

### Singleton + Factory

```cpp
// LevelFactory is singleton
class LevelFactory : public Singleton<LevelFactory> {
public:
    template<typename T>
    void Register(const char* name) {
        // Factory pattern
    }
};
```

---

## Pattern Selection Guide

### When to Use Each Pattern

| Pattern | Use When | Example |
|---------|----------|---------|
| **Module** | Reusable component with dependencies | InputModule, PhysicsModule |
| **Factory** | Runtime object creation by name | LevelFactory, ComponentFactory |
| **Observer** | One-to-many notifications | UI events, cross-thread events |
| **Singleton** | Single global instance needed | LevelFactory, TimeServer |
| **State** | Object has distinct states | Phases (Boot, Running, etc.) |
| **Proxy** | Control access, synchronization | SimUIProxyModule |
| **Template Method** | Common algorithm, variable steps | Module lifecycle (Start/Update/Stop) |
| **Visitor** | Multiple operations on structure | Debug rendering |

---

## Summary

**Patterns Used in Cluiche/Dia:**

**Core Architecture:**
- ✅ Module (custom) - Reusable components
- ✅ State (Phases) - Application flow
- ✅ Template Method - Lifecycle hooks

**Creational:**
- ✅ Factory - Runtime object creation
- ✅ Singleton - Global instances (sparingly)
- ✅ Object Pool - Component allocation

**Structural:**
- ✅ Proxy - Cross-thread access

**Behavioral:**
- ✅ Observer - Event notifications
- ✅ Visitor - Debug rendering

**Anti-Patterns Avoided:**
- ❌ God Object
- ❌ Singleton Overuse
- ❌ Stringly-Typed

**Philosophy:** Use patterns to solve problems, not for their own sake

**[→ Back to Design Philosophy](design.md)**  
**[→ Module System Architecture](../architecture/module-system.md)**
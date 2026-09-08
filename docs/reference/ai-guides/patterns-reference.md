# Code Patterns Reference

**Last Updated:** 2026-04-01

Common code patterns used throughout Cluiche codebase with examples for AI agents.

---

## Pattern Index

**Core Patterns:**
1. [Module Pattern](#pattern-1-module)
2. [Phase Pattern](#pattern-2-phase)
3. [ProcessingUnit Pattern](#pattern-3-processingunit)
4. [Factory Pattern](#pattern-4-factory)
5. [Observer Pattern](#pattern-5-observer)
6. [Singleton Pattern](#pattern-6-singleton)

**Threading Patterns:**
7. [FrameStream (Cross-Thread Communication)](#pattern-7-framestream)
8. [Mutex Protection](#pattern-8-mutex-protection)

**Type System Patterns:**
9. [StringCRC Usage](#pattern-9-stringcrc)
10. [TypeRegistry Usage](#pattern-10-typeregistry)

**Container Patterns:**
11. [DynamicArray Usage](#pattern-11-dynamicarray)
12. [HashTable Usage](#pattern-12-hashtable)

**Component Patterns:**
13. [Component Creation](#pattern-13-component-creation)
14. [Component Factory](#pattern-14-component-factory)

---

## Pattern 1: Module

**Purpose:** Reusable functional unit with explicit dependencies and lifecycle

**When to Use:**
- Need functionality that runs every frame
- Have dependencies on other modules
- Want testable, isolated components

### Template

```cpp
// Header: MyModule.h
#pragma once

#include "DiaApplicationFlow/ApplicationModule.h"

namespace MyNamespace
{
    class MyModule : public Dia::Application::Module
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        MyModule();
        virtual ~MyModule() = default;

        // Public API
        void DoSomething();

    private:
        // Module lifecycle
        virtual void DoStart() override;
        virtual void DoUpdate() override;
        virtual void DoStop() override;

        // Dependencies
        SomeOtherModule* mDependency;

        // Internal state
        bool mInitialized;
    };
}
```

```cpp
// Implementation: MyModule.cpp
#include "MyModule.h"
#include "SomeOtherModule.h"
#include "DiaCore/Core/Log.h"

namespace MyNamespace
{
    const Dia::Core::StringCRC MyModule::kUniqueId = 
        Dia::Core::StringCRC("MyModule");

    MyModule::MyModule()
        : Dia::Application::Module(kUniqueId)
        , mDependency(nullptr)
        , mInitialized(false)
    {
        // Register dependencies
        RegisterDependency(&GetDependency<SomeOtherModule>());
    }

    void MyModule::DoStart()
    {
        DIA_LOG("MyModule: Starting");
        
        // Get dependencies
        mDependency = GetDependency<SomeOtherModule>();
        DIA_ASSERT(mDependency != nullptr, "Dependency missing");
        
        // Initialize
        mInitialized = true;
    }

    void MyModule::DoUpdate()
    {
        if (mInitialized)
        {
            // Per-frame update
            mDependency->DoSomethingElse();
        }
    }

    void MyModule::DoStop()
    {
        DIA_LOG("MyModule: Stopping");
        mInitialized = false;
        mDependency = nullptr;
    }

    void MyModule::DoSomething()
    {
        // Public API implementation
    }
}
```

### Key Points

- **Unique ID:** `static const StringCRC kUniqueId`
- **Constructor:** Register dependencies via `RegisterDependency()`
- **DoStart():** Get dependencies via `GetDependency<T>()`, initialize
- **DoUpdate():** Called every frame
- **DoStop():** Cleanup, nullify dependencies

---

## Pattern 2: Phase

**Purpose:** Execution stage within a ProcessingUnit with state transitions

**When to Use:**
- Need distinct application states (Boot, Running, Shutdown)
- Want to group modules by execution stage
- Need explicit state transitions

### Template

```cpp
// Header: MyPhase.h
#pragma once

#include "DiaApplicationFlow/ApplicationPhase.h"

namespace MyNamespace
{
    class MyPhase : public Dia::Application::Phase
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        MyPhase();
        virtual ~MyPhase() = default;

    private:
        virtual void DoStart() override;
        virtual void DoUpdate() override;
        virtual void DoStop() override;
        
        bool ShouldTransition() const;
    };
}
```

```cpp
// Implementation: MyPhase.cpp
#include "MyPhase.h"
#include "NextPhase.h"
#include "MyModule.h"
#include "DiaCore/Core/Log.h"

namespace MyNamespace
{
    const Dia::Core::StringCRC MyPhase::kUniqueId = 
        Dia::Core::StringCRC("MyPhase");

    MyPhase::MyPhase()
        : Dia::Application::Phase(kUniqueId)
    {
    }

    void MyPhase::DoStart()
    {
        DIA_LOG("MyPhase: Starting");
        
        // Add modules for this phase
        AddModule(new MyModule());
        AddModule(new AnotherModule());
    }

    void MyPhase::DoUpdate()
    {
        // Update all modules
        UpdateAllModules();
        
        // Check for phase transition
        if (ShouldTransition())
        {
            GetProcessingUnit()->TransitionPhase(new NextPhase());
        }
    }

    void MyPhase::DoStop()
    {
        DIA_LOG("MyPhase: Stopping");
        // Modules cleaned up automatically by Phase base class
    }

    bool MyPhase::ShouldTransition() const
    {
        // Transition logic
        return false;
    }
}
```

### Key Points

- **Add Modules:** `AddModule(new MyModule())` in `DoStart()`
- **Update Modules:** `UpdateAllModules()` in `DoUpdate()`
- **Transition:** `GetProcessingUnit()->TransitionPhase(new NextPhase())`
- **Cleanup:** Phase base class handles module destruction

---

## Pattern 3: ProcessingUnit

**Purpose:** Top-level thread container that manages phases

**When to Use:**
- Creating a new thread
- Need to orchestrate phases
- Want explicit thread lifecycle

### Template

```cpp
// Header: MyProcessingUnit.h
#pragma once

#include "DiaApplicationFlow/ApplicationProcessingUnit.h"
#include <thread>

namespace MyNamespace
{
    class MyProcessingUnit : public Dia::Application::ProcessingUnit
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        MyProcessingUnit();
        virtual ~MyProcessingUnit();

        // Thread control
        void StartThread();
        void StopThread();

    private:
        virtual void DoStart() override;
        virtual void DoUpdate() override;
        virtual void DoStop() override;

        std::thread mThread;
        bool mRunning;
    };
}
```

```cpp
// Implementation: MyProcessingUnit.cpp
#include "MyProcessingUnit.h"
#include "MyBootPhase.h"
#include "DiaCore/Core/Log.h"

namespace MyNamespace
{
    const Dia::Core::StringCRC MyProcessingUnit::kUniqueId = 
        Dia::Core::StringCRC("MyProcessingUnit");

    MyProcessingUnit::MyProcessingUnit()
        : Dia::Application::ProcessingUnit(kUniqueId)
        , mRunning(false)
    {
    }

    MyProcessingUnit::~MyProcessingUnit()
    {
        if (mThread.joinable())
        {
            StopThread();
        }
    }

    void MyProcessingUnit::DoStart()
    {
        DIA_LOG("MyProcessingUnit: Starting");
        
        // Transition to initial phase
        TransitionPhase(new MyBootPhase());
    }

    void MyProcessingUnit::DoUpdate()
    {
        // Update current phase
        if (GetCurrentPhase())
        {
            GetCurrentPhase()->Update();
        }
    }

    void MyProcessingUnit::DoStop()
    {
        DIA_LOG("MyProcessingUnit: Stopping");
    }

    void MyProcessingUnit::StartThread()
    {
        mRunning = true;
        mThread = std::thread([this]() {
            Start();
            while (mRunning)
            {
                Update();
            }
            Stop();
        });
    }

    void MyProcessingUnit::StopThread()
    {
        mRunning = false;
        if (mThread.joinable())
        {
            mThread.join();
        }
    }
}
```

### Key Points

- **Initial Phase:** `TransitionPhase(new MyBootPhase())` in `DoStart()`
- **Update Loop:** `GetCurrentPhase()->Update()` in `DoUpdate()`
- **Threading:** Wrap in `std::thread`, call `Start()`/`Update()`/`Stop()`

---

## Pattern 4: Factory

**Purpose:** Create objects by name at runtime

**When to Use:**
- Pluggable architecture (levels, components)
- Runtime object creation
- Avoid hard-coded dependencies

### Template

```cpp
// Using LevelFactory (existing)
#include "DiaApplicationFlow/LevelFactory.h"

// Register level
Dia::Application::LevelFactory::Instance()->Register<MyLevel>("MyLevel");

// Create level by name
Dia::Application::ILevel* level = 
    Dia::Application::LevelFactory::Instance()->Create("MyLevel");

if (level)
{
    level->Start();
}
```

### Custom Factory

```cpp
template<typename T>
class Factory
{
public:
    template<typename TDerived>
    void Register(const Dia::Core::StringCRC& id)
    {
        mRegistry[id] = []() { return new TDerived(); };
    }

    T* Create(const Dia::Core::StringCRC& id)
    {
        auto it = mRegistry.find(id);
        return it != mRegistry.end() ? it->second() : nullptr;
    }

private:
    std::map<Dia::Core::StringCRC, std::function<T*()>> mRegistry;
};

// Usage
Factory<IComponent> componentFactory;
componentFactory.Register<TransformComponent>(
    Dia::Core::StringCRC("Transform"));

IComponent* comp = componentFactory.Create(
    Dia::Core::StringCRC("Transform"));
```

---

## Pattern 5: Observer

**Purpose:** One-to-many event notifications (thread-safe)

**When to Use:**
- Cross-thread communication
- Event-driven architecture
- Decoupled notifications

### Template

```cpp
// Observer (listener)
class MyObserver : public Dia::Core::Observer
{
public:
    virtual void OnNotify() override
    {
        DIA_LOG("Event received");
        // Handle event
    }
};

// Subject (event source)
class MySubject : public Dia::Core::ObserverSubject
{
public:
    void SomethingHappened()
    {
        // Notify all observers (thread-safe)
        Notify();
    }
};

// Usage
MySubject subject;
MyObserver observer;

subject.Attach(&observer);
subject.SomethingHappened();  // Calls observer.OnNotify()
subject.Detach(&observer);
```

### Key Points

- **Thread-Safe:** `Notify()` uses `std::mutex`
- **Attach/Detach:** Manage observer list
- **One-to-Many:** Multiple observers can attach

---

## Pattern 6: Singleton

**Purpose:** Single global instance with controlled lifetime

**When to Use:**
- Truly global resource (TimeServer, LevelFactory)
- Need explicit creation/destruction
- **Use sparingly!** (prefer dependency injection)

### Template

```cpp
// Header: MyManager.h
#pragma once

#include "DiaCore/Architecture/Singleton/Singleton.h"

namespace MyNamespace
{
    class MyManager : public Dia::Core::Singleton<MyManager>
    {
    public:
        void DoSomething();

    private:
        friend class Dia::Core::Singleton<MyManager>;
        
        MyManager() = default;
        ~MyManager() = default;
    };
}
```

```cpp
// Usage
MyManager::Create();
MyManager::Instance()->DoSomething();
MyManager::Destroy();

// Check if created
if (MyManager::IsCreated())
{
    // Use instance
}
```

### Key Points

- **Explicit Creation:** `Create()` before use
- **Explicit Destruction:** `Destroy()` when done
- **Access:** `Instance()` returns pointer
- **Check:** `IsCreated()` returns bool

---

## Pattern 7: FrameStream

**Purpose:** Thread-safe cross-thread communication queue

**When to Use:**
- Send data from one thread to another
- Producer-consumer pattern
- Avoid complex mutex logic

### Template

```cpp
// Header: Define FrameStream
#include "DiaApplicationFlow/FrameStream.h"

class MainProcessingUnit : public Dia::Application::ProcessingUnit
{
private:
    Dia::Application::FrameStream<std::string> mMainToSimMessages;

public:
    Dia::Application::FrameStream<std::string>& GetMainToSimMessages() {
        return mMainToSimMessages;
    }
};
```

```cpp
// Producer (Main thread)
void MainUIModule::OnButtonClicked()
{
    mainPU->GetMainToSimMessages().Write("ButtonClicked");
}

// Consumer (Sim thread)
void SimInputModule::DoUpdate()
{
    std::string message;
    while (mainPU->GetMainToSimMessages().Read(message))
    {
        DIA_LOG("Sim received: %s", message.c_str());
        // Process message
    }
}
```

### Key Points

- **Thread-Safe:** Uses `std::mutex` internally
- **Write:** `Write(const T& data)` from producer thread
- **Read:** `Read(T& data)` from consumer thread (returns bool)
- **Non-Blocking:** `Read()` returns `false` if empty

---

## Pattern 8: Mutex Protection

**Purpose:** Protect shared data from race conditions

**When to Use:**
- Multiple threads access shared state
- Need to prevent race conditions
- Simple synchronization

### Template

```cpp
class SharedData
{
public:
    void SetValue(int value) {
        std::lock_guard<std::mutex> lock(mMutex);
        mValue = value;
    }

    int GetValue() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mValue;
    }

private:
    int mValue;
    mutable std::mutex mMutex;  // mutable for const methods
};
```

### Key Points

- **Lock Guard:** RAII-style locking (automatic unlock)
- **Mutable:** Mark `mMutex` as `mutable` for const methods
- **Keep Critical Section Small:** Lock only what's necessary

---

## Pattern 9: StringCRC

**Purpose:** Compile-time string hashing for fast comparison

**When to Use:**
- Need fast string comparison (O(1))
- Type IDs
- Hash map keys

### Template

```cpp
#include "DiaCore/CRC/CRC.h"

// Compile-time constant
static const Dia::Core::StringCRC kMyId = Dia::Core::StringCRC("MyId");

// Runtime construction
Dia::Core::StringCRC runtimeId("RuntimeId");

// Comparison (compares CRC values, not strings)
if (kMyId == runtimeId)
{
    // Equal
}

// Use as hash map key
Dia::Core::HashTable<Dia::Core::StringCRC, int> table;
table.Insert(kMyId, 42);

int* value = table.Find(kMyId);
if (value)
{
    DIA_LOG("Value: %d", *value);
}

// Get CRC value
unsigned int crc = kMyId.GetCRC();
```

### Key Points

- **Constexpr:** Can be evaluated at compile-time
- **Fast Comparison:** O(1) integer comparison
- **No String Storage:** Only stores CRC value (4 bytes)

---

## Pattern 10: TypeRegistry

**Purpose:** Runtime type reflection and metadata

**When to Use:**
- Need type information at runtime
- Serialization
- Polymorphic type identification

### Template

```cpp
#include "DiaCore/Type/TypeDefinition.h"
#include "DiaCore/Type/TypeRegistry.h"

// Define type
class MyClass
{
public:
    static const Dia::Core::StringCRC kTypeId;
};

const Dia::Core::StringCRC MyClass::kTypeId = 
    Dia::Core::StringCRC("MyClass");

// Register type
Dia::Core::TypeDefinition* typeDef = new Dia::Core::TypeDefinition();
typeDef->SetName("MyClass");
typeDef->SetId(MyClass::kTypeId);
Dia::Core::TypeRegistry::Instance()->Register(typeDef);

// Lookup type
const Dia::Core::TypeDefinition* foundType = 
    Dia::Core::TypeRegistry::Instance()->FindType(MyClass::kTypeId);

if (foundType)
{
    DIA_LOG("Found type: %s", foundType->GetName());
}
```

---

## Pattern 11: DynamicArray

**Purpose:** Dynamic array (like `std::vector`)

**When to Use:**
- Need growable array
- Most common container in codebase

### Template

```cpp
#include "DiaCore/Containers/Arrays/DynamicArray.h"

// Create array
Dia::Core::DynamicArray<int> arr;

// Add elements
arr.PushBack(1);
arr.PushBack(2);
arr.PushBack(3);

// Access elements
int value = arr[0];
int* ptr = arr.At(1);

// Size
unsigned int size = arr.Size();
bool empty = arr.IsEmpty();

// Remove
arr.PopBack();
arr.RemoveAt(0);
arr.Clear();

// Iteration
for (int val : arr)
{
    DIA_LOG("Value: %d", val);
}

// Iterator
for (auto it = arr.Begin(); it != arr.End(); ++it)
{
    DIA_LOG("Value: %d", *it);
}
```

### Key Points

- **No STL:** Custom implementation (explicit memory control)
- **Capacity:** Grows automatically (2x on overflow)
- **Random Access:** O(1) access by index

---

## Pattern 12: HashTable

**Purpose:** Hash map with StringCRC keys

**When to Use:**
- Need fast lookup by name
- String-based keys
- O(1) access

### Template

```cpp
#include "DiaCore/Containers/HashTables/HashTable.h"
#include "DiaCore/CRC/CRC.h"

// Create hash table
Dia::Core::HashTable<Dia::Core::StringCRC, int> table;

// Insert
Dia::Core::StringCRC key("MyKey");
table.Insert(key, 42);

// Find
int* value = table.Find(key);
if (value)
{
    DIA_LOG("Found: %d", *value);
}

// Remove
table.Remove(key);

// Iteration
for (auto it = table.Begin(); it != table.End(); ++it)
{
    Dia::Core::StringCRC key = it.Key();
    int value = it.Value();
    DIA_LOG("Key: %u, Value: %d", key.GetCRC(), value);
}
```

### Key Points

- **StringCRC Keys:** Optimized for string-based lookup
- **No Collision Info:** Uses CRC32 (collisions possible but rare)
- **O(1) Lookup:** Fast constant-time access

---

## Pattern 13: Component Creation

**Purpose:** Create components via factory

**When to Use:**
- Component-based architecture
- Runtime component creation

### Template

```cpp
#include "DiaCore/Architecture/Components/IComponent.h"
#include "DiaCore/Architecture/Components/ComponentFactoryRegistry.h"

// Define component
class TransformComponent : public Dia::Core::IComponent
{
public:
    static const Dia::Core::StringCRC kTypeId;
    
    TransformComponent() : IComponent(kTypeId) {}
    
    Dia::Maths::Vector2D position;
};

const Dia::Core::StringCRC TransformComponent::kTypeId = 
    Dia::Core::StringCRC("Transform");

// Register factory
Dia::Core::ComponentFactoryRegistry::Instance()->Register<TransformComponent>(
    TransformComponent::kTypeId);

// Create component
Dia::Core::IComponent* comp = 
    Dia::Core::ComponentFactoryRegistry::Instance()->Create(
        TransformComponent::kTypeId);

TransformComponent* transform = static_cast<TransformComponent*>(comp);
transform->position = Dia::Maths::Vector2D(10.0f, 20.0f);

// Destroy
Dia::Core::ComponentFactoryRegistry::Instance()->Destroy(
    TransformComponent::kTypeId, comp);
```

---

## Pattern 14: Component Factory

**Purpose:** Pooled component allocation (optional)

**When to Use:**
- Frequent component creation/destruction
- Predictable component count
- Avoid heap fragmentation

### Template

```cpp
#include "DiaCore/Architecture/Components/StaticPooledComponentFactory.h"

// Define pooled factory (capacity 100)
Dia::Core::StaticPooledComponentFactory<TransformComponent, 100> transformFactory;

// Create from pool
TransformComponent* comp1 = transformFactory.Create();
comp1->position = Dia::Maths::Vector2D(10.0f, 20.0f);

// Return to pool
transformFactory.Destroy(comp1);

// Create again (reuses memory)
TransformComponent* comp2 = transformFactory.Create();
```

### Key Points

- **Static Pool:** Fixed capacity at compile-time
- **No Allocation:** Pre-allocated memory
- **Fast:** O(1) creation and destruction

---

## Summary

**Most Common Patterns:**
1. **Module** - Core building block for functionality
2. **Phase** - Groups modules by execution stage
3. **ProcessingUnit** - Thread container
4. **FrameStream** - Cross-thread communication
5. **StringCRC** - Fast string comparison
6. **DynamicArray** - Most used container

**Thread Safety:**
- **FrameStream** - Thread-safe queue
- **Mutex** - Protect shared data
- **Observer** - Thread-safe notifications

**Type System:**
- **StringCRC** - Compile-time hashing
- **TypeRegistry** - Runtime reflection

**[→ System Boundaries](system-boundaries.md)**  
**[→ Entry Points](entry-points.md)**  
**[→ Back to AI Guide](AI-README.md)**

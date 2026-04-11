# DiaCore API

**Last Updated:** 2026-04-01

Foundation library API providing containers, patterns, type system, and utilities.

---

## Overview

**DiaCore** is the foundation layer providing custom containers, design patterns, type system, time tracking, and core utilities.

**Location:** `Dia/DiaCore/`

**Namespace:** `Dia::Core::`

**Key Components:**
- **Containers** - DynamicArray, HashTable, LinkList, Graph, etc.
- **Architecture** - Singleton, Factory, Observer, Component system
- **Type System** - StringCRC, TypeRegistry
- **Time** - TimeServer, TimeAbsolute, TimeRelative
- **Utilities** - Assert, Log, FilePath, JSON

---

## Containers

### DynamicArray<T>

**Header:** `Dia/DiaCore/Containers/Arrays/DynamicArray.h`

**Purpose:** Dynamic array (like `std::vector`)

#### Key Methods

```cpp
template<typename T>
class DynamicArray
{
public:
    DynamicArray();
    explicit DynamicArray(unsigned int capacity);
    ~DynamicArray();
    
    // Add/Remove
    void PushBack(const T& value);
    void PopBack();
    void RemoveAt(unsigned int index);
    void Clear();
    
    // Access
    T& operator[](unsigned int index);
    const T& operator[](unsigned int index) const;
    T* At(unsigned int index);
    
    // Query
    unsigned int Size() const;
    unsigned int Capacity() const;
    bool IsEmpty() const;
    
    // Iteration
    T* Begin();
    T* End();
    const T* Begin() const;
    const T* End() const;
};
```

#### Usage Example

```cpp
// Create array
Dia::Core::DynamicArray<int> arr;

// Add elements
arr.PushBack(1);
arr.PushBack(2);
arr.PushBack(3);

// Access
int value = arr[0];         // Unchecked
int* ptr = arr.At(1);       // Returns pointer

// Iterate
for (int val : arr)
{
    DIA_LOG("Value: %d", val);
}

// Remove
arr.PopBack();
arr.RemoveAt(0);
arr.Clear();
```

#### Thread Safety

❌ **Not thread-safe** - Must synchronize externally

---

### Array<T, N>

**Header:** `Dia/DiaCore/Containers/Arrays/Array.h`

**Purpose:** Fixed-size array

#### Key Methods

```cpp
template<typename T, unsigned int N>
class Array
{
public:
    Array();
    
    // Access
    T& operator[](unsigned int index);
    const T& operator[](unsigned int index) const;
    
    // Query
    constexpr unsigned int Size() const { return N; }
    
    // Iteration
    T* Begin();
    T* End();
};
```

#### Usage Example

```cpp
Dia::Core::Array<float, 10> fixedArray;
fixedArray[0] = 1.0f;
fixedArray[9] = 10.0f;

// Compile-time size
constexpr unsigned int size = fixedArray.Size();  // 10
```

---

### HashTable<K, V>

**Header:** `Dia/DiaCore/Containers/HashTables/HashTable.h`

**Purpose:** Hash map (optimized for StringCRC keys)

#### Key Methods

```cpp
template<typename Key, typename Value>
class HashTable
{
public:
    HashTable();
    ~HashTable();
    
    // Add/Remove
    void Insert(const Key& key, const Value& value);
    void Remove(const Key& key);
    void Clear();
    
    // Find
    Value* Find(const Key& key);
    const Value* Find(const Key& key) const;
    bool Contains(const Key& key) const;
    
    // Query
    unsigned int Size() const;
    bool IsEmpty() const;
    
    // Iteration
    Iterator Begin();
    Iterator End();
};
```

#### Usage Example

```cpp
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

// Check existence
if (table.Contains(key))
{
    // Key exists
}

// Iterate
for (auto it = table.Begin(); it != table.End(); ++it)
{
    Dia::Core::StringCRC k = it.Key();
    int v = it.Value();
}

// Remove
table.Remove(key);
```

#### Thread Safety

❌ **Not thread-safe** - Must synchronize externally

---

### LinkList<T>

**Header:** `Dia/DiaCore/Containers/LinkLists/LinkList.h`

**Purpose:** Doubly-linked list

#### Usage Example

```cpp
Dia::Core::LinkList<int> list;

list.PushFront(1);
list.PushBack(3);
list.Insert(1, 2);  // Insert at index

int* front = list.Front();
int* back = list.Back();

list.PopFront();
list.PopBack();
```

---

### Graph<T>

**Header:** `Dia/DiaCore/Containers/Graphs/Graph.h`

**Purpose:** Graph data structure with nodes and edges

---

### BitFlag<N>

**Header:** `Dia/DiaCore/Containers/BitFlag/BitFlag.h`

**Purpose:** Bit flags for compact boolean storage

---

## Architecture Patterns

### Singleton<T>

**Header:** `Dia/DiaCore/Architecture/Singleton/Singleton.h`

**Purpose:** Singleton pattern with explicit lifetime

#### Key Methods

```cpp
template<typename T>
class Singleton
{
public:
    static void Create();
    static void Destroy();
    static T* Instance();
    static bool IsCreated();
};
```

#### Usage Example

```cpp
class MyManager : public Dia::Core::Singleton<MyManager>
{
private:
    friend class Dia::Core::Singleton<MyManager>;
    MyManager() = default;  // Private constructor
    ~MyManager() = default;
    
public:
    void DoSomething();
};

// Usage
MyManager::Create();
MyManager::Instance()->DoSomething();
MyManager::Destroy();

// Check before use
if (MyManager::IsCreated())
{
    MyManager::Instance()->DoSomething();
}
```

#### Thread Safety

✅ **Thread-safe** - Creation/destruction protected

---

### Observer / ObserverSubject

**Header:** `Dia/DiaCore/Architecture/Observer/Observer.h`

**Purpose:** Observer pattern for event notifications

#### Key Classes

```cpp
class Observer
{
public:
    virtual void OnNotify() = 0;
};

class ObserverSubject
{
public:
    void Attach(Observer* observer);
    void Detach(Observer* observer);
    
protected:
    void Notify();
};
```

#### Usage Example

```cpp
// Subject
class EventSource : public Dia::Core::ObserverSubject
{
public:
    void FireEvent()
    {
        Notify();  // Notifies all observers
    }
};

// Observer
class EventListener : public Dia::Core::Observer
{
public:
    void OnNotify() override
    {
        DIA_LOG("Event received");
    }
};

// Usage
EventSource source;
EventListener listener;

source.Attach(&listener);
source.FireEvent();  // listener.OnNotify() called
source.Detach(&listener);
```

#### Thread Safety

✅ **Thread-safe** - Attach/Detach/Notify protected by mutex

⚠️ **Important:** `OnNotify()` called from Subject's thread

---

### Factory<T>

**Header:** `Dia/DiaCore/Architecture/Factory/Factory.h`

**Purpose:** Factory pattern for runtime object creation

---

### Component System

**Header:** `Dia/DiaCore/Architecture/Components/`

#### IComponent

```cpp
class IComponent
{
public:
    IComponent(const StringCRC& typeId);
    virtual ~IComponent() = default;
    
    const StringCRC& GetTypeId() const;
};
```

#### ComponentFactoryRegistry

```cpp
class ComponentFactoryRegistry : public Singleton<ComponentFactoryRegistry>
{
public:
    template<typename T>
    void Register(const StringCRC& typeId);
    
    IComponent* Create(const StringCRC& typeId);
    void Destroy(const StringCRC& typeId, IComponent* component);
};
```

#### Usage Example

```cpp
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

// Register
ComponentFactoryRegistry::Create();
ComponentFactoryRegistry::Instance()->Register<TransformComponent>(
    TransformComponent::kTypeId);

// Create
IComponent* comp = ComponentFactoryRegistry::Instance()->Create(
    TransformComponent::kTypeId);
TransformComponent* transform = static_cast<TransformComponent*>(comp);

// Destroy
ComponentFactoryRegistry::Instance()->Destroy(
    TransformComponent::kTypeId, comp);
ComponentFactoryRegistry::Destroy();
```

---

### StaticPooledComponentFactory<T, N>

**Header:** `Dia/DiaCore/Architecture/Components/StaticPooledComponentFactory.h`

**Purpose:** Pre-allocated component pool (no dynamic allocation)

#### Usage Example

```cpp
// Create pool (capacity 100)
Dia::Core::StaticPooledComponentFactory<TransformComponent, 100> pool;

// Allocate from pool
TransformComponent* comp1 = pool.Create();
comp1->position = Dia::Maths::Vector2D(10.0f, 20.0f);

// Return to pool
pool.Destroy(comp1);

// Reuses memory
TransformComponent* comp2 = pool.Create();
```

---

## Type System

### StringCRC

**Header:** `Dia/DiaCore/CRC/CRC.h`

**Purpose:** Compile-time string hashing for fast comparison

#### Key Methods

```cpp
class StringCRC
{
public:
    constexpr StringCRC(const char* str);
    
    unsigned int GetCRC() const;
    
    bool operator==(const StringCRC& other) const;
    bool operator!=(const StringCRC& other) const;
    bool operator<(const StringCRC& other) const;
};
```

#### Usage Example

```cpp
// Compile-time constant
static const Dia::Core::StringCRC kMyId = Dia::Core::StringCRC("MyId");

// Runtime construction
Dia::Core::StringCRC runtimeId("RuntimeId");

// Fast comparison (integer comparison, not string)
if (kMyId == runtimeId)
{
    // Equal
}

// Get CRC value
unsigned int crc = kMyId.GetCRC();

// Use as hash map key
Dia::Core::HashTable<Dia::Core::StringCRC, int> table;
table.Insert(kMyId, 42);
```

#### Key Features

- ✅ **Compile-time evaluation** (constexpr)
- ✅ **O(1) comparison** (integer comparison)
- ✅ **Small size** (4 bytes)
- ✅ **No string storage** (only CRC value)

---

### TypeDefinition

**Header:** `Dia/DiaCore/Type/TypeDefinition.h`

**Purpose:** Runtime type metadata

#### Key Methods

```cpp
class TypeDefinition
{
public:
    void SetName(const char* name);
    void SetId(const StringCRC& id);
    
    const char* GetName() const;
    const StringCRC& GetId() const;
};
```

---

### TypeRegistry

**Header:** `Dia/DiaCore/Type/TypeRegistry.h`

**Purpose:** Global type registry (singleton)

#### Usage Example

```cpp
// Create registry
Dia::Core::TypeRegistry::Create();

// Register type
Dia::Core::TypeDefinition* typeDef = new Dia::Core::TypeDefinition();
typeDef->SetName("MyClass");
typeDef->SetId(Dia::Core::StringCRC("MyClass"));
Dia::Core::TypeRegistry::Instance()->Register(typeDef);

// Lookup type
const Dia::Core::TypeDefinition* found = 
    Dia::Core::TypeRegistry::Instance()->FindType(
        Dia::Core::StringCRC("MyClass"));

if (found)
{
    DIA_LOG("Found type: %s", found->GetName());
}

// Destroy registry
Dia::Core::TypeRegistry::Destroy();
```

---

## Time

### TimeServer

**Header:** `Dia/DiaCore/Time/TimeServer.h`

**Purpose:** Global time source (singleton)

#### Key Methods

```cpp
class TimeServer : public Singleton<TimeServer>
{
public:
    void Update();
    
    float GetDeltaTime() const;
    float GetTotalTime() const;
    unsigned int GetFrameCount() const;
};
```

#### Usage Example

```cpp
// Create
Dia::Core::TimeServer::Create();

// Update each frame
Dia::Core::TimeServer::Instance()->Update();

// Get delta time
float dt = Dia::Core::TimeServer::Instance()->GetDeltaTime();

// Get total time
float totalTime = Dia::Core::TimeServer::Instance()->GetTotalTime();

// Destroy
Dia::Core::TimeServer::Destroy();
```

#### Thread Safety

✅ **Thread-safe** for reading (immutable after update)

---

### TimeAbsolute

**Header:** `Dia/DiaCore/Time/TimeAbsolute.h`

**Purpose:** Absolute time point

---

### TimeRelative

**Header:** `Dia/DiaCore/Timer/TimeRelative.h`

**Purpose:** Relative time, timers

---

## Utilities

### Assertions

**Header:** `Dia/DiaCore/Core/Assert.h`

#### Macros

```cpp
DIA_ASSERT(condition, message, ...);
```

#### Usage Example

```cpp
void ProcessData(Data* data)
{
    DIA_ASSERT(data != nullptr, "Data must not be null");
    DIA_ASSERT(data->IsValid(), "Data must be valid");
    DIA_ASSERT(data->GetSize() > 0, "Data size must be > 0, got %d", data->GetSize());
}
```

---

### Logging

**Header:** `Dia/DiaCore/Core/Log.h`

#### Macros

```cpp
DIA_LOG(format, ...);
```

#### Usage Example

```cpp
DIA_LOG("Starting module: %s", GetName());
DIA_LOG("Value: %d, Position: (%f, %f)", value, pos.x, pos.y);
```

---

### FilePath

**Header:** `Dia/DiaCore/FilePath/FilePath.h`

**Purpose:** File path manipulation and I/O

#### Usage Example

```cpp
Dia::Core::FilePath path("data/level.json");

if (path.Exists())
{
    std::string content = path.ReadAllText();
}

path.WriteAllText("new content");
```

---

### JSON

**Header:** `Dia/DiaCore/Json/JsonWrapper.h`

**Purpose:** JsonCpp wrapper

#### Usage Example

```cpp
// Parse JSON
Json::Value root;
Json::Reader reader;
if (reader.parse(jsonString, root))
{
    int value = root["key"].asInt();
    std::string name = root["name"].asString();
}

// Write JSON
Json::Value obj;
obj["key"] = 42;
obj["name"] = "Test";

Json::StyledWriter writer;
std::string output = writer.write(obj);
```

---

## Common Patterns

### Using DynamicArray

```cpp
Dia::Core::DynamicArray<MyObject> objects;

objects.PushBack(MyObject());
objects.PushBack(MyObject());

for (MyObject& obj : objects)
{
    obj.Update();
}
```

---

### Using HashTable with StringCRC

```cpp
Dia::Core::HashTable<Dia::Core::StringCRC, MyData*> lookup;

Dia::Core::StringCRC key("key1");
lookup.Insert(key, new MyData());

MyData** found = lookup.Find(key);
if (found)
{
    (*found)->DoSomething();
}
```

---

### Observer Pattern for Events

```cpp
class EventSource : public Dia::Core::ObserverSubject {
    void FireEvent() { Notify(); }
};

class Listener : public Dia::Core::Observer {
    void OnNotify() override { /* Handle */ }
};

source.Attach(&listener);
source.FireEvent();
```

---

## Dependencies

**External:**
- JsonCpp (wrapped in `DiaCore/Json/`)

**Standard Library:**
- `<cstdlib>`, `<cstring>`, `<cstdio>` - C utilities
- `<string>`, `<vector>`, `<map>` (minimal use)
- `<mutex>` - Thread synchronization

---

## Thread Safety

| Class | Thread Safety |
|-------|---------------|
| `DynamicArray` | ❌ Not thread-safe |
| `HashTable` | ❌ Not thread-safe |
| `Singleton` (creation) | ✅ Thread-safe |
| `ObserverSubject` | ✅ Thread-safe |
| `TimeServer` (read) | ✅ Thread-safe |
| `StringCRC` | ✅ Immutable (safe) |

---

## Best Practices

### 1. Prefer DynamicArray for Most Cases

```cpp
// ✅ Good: Use DynamicArray
Dia::Core::DynamicArray<int> arr;

// ❌ Bad: Use std::vector (breaks custom container policy)
std::vector<int> arr;
```

---

### 2. Use StringCRC for Type IDs

```cpp
// ✅ Good: Compile-time hash
static const StringCRC kUniqueId = StringCRC("MyClass");

// ❌ Bad: Runtime string comparison
const char* kUniqueId = "MyClass";
if (strcmp(id, kUniqueId) == 0) { }  // Slow!
```

---

### 3. Check Singleton Creation

```cpp
// ✅ Good: Check before use
if (TimeServer::IsCreated())
{
    TimeServer::Instance()->Update();
}

// ❌ Bad: Assume created
TimeServer::Instance()->Update();  // May crash!
```

---

### 4. Synchronize Container Access

```cpp
// ✅ Good: Protected access
std::mutex mMutex;
Dia::Core::DynamicArray<int> mSharedArray;

void AddValue(int value)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSharedArray.PushBack(value);
}

// ❌ Bad: Unprotected access (race condition)
mSharedArray.PushBack(value);
```

---

## Gotchas

### Gotcha 1: HashTable Key Ownership

HashTable does **not** copy keys. Use StringCRC (which stores CRC value) or ensure key lifetime.

---

### Gotcha 2: Observer Called from Subject's Thread

`Observer::OnNotify()` is called from the **Subject's thread**, not the Observer's thread.

---

### Gotcha 3: DynamicArray Invalidates Pointers on Resize

```cpp
DynamicArray<int> arr;
arr.PushBack(1);

int* ptr = &arr[0];
arr.PushBack(2);  // May resize, invalidating ptr
// ptr is now invalid!
```

---

## Summary

**Containers:**
- `DynamicArray<T>` - Dynamic array (most used)
- `HashTable<K,V>` - Hash map (StringCRC keys)
- `Array<T,N>` - Fixed-size array
- `LinkList<T>`, `Graph<T>`, `BitFlag<N>`

**Patterns:**
- `Singleton<T>` - Explicit lifetime singleton
- `Observer`/`ObserverSubject` - Event notifications
- Component system - Runtime components

**Type System:**
- `StringCRC` - Compile-time string hashing
- `TypeRegistry` - Runtime type metadata

**Utilities:**
- `DIA_ASSERT`, `DIA_LOG` - Debugging
- `TimeServer` - Global time
- `FilePath` - File I/O
- JSON wrapper

**Thread Safety:**
- Singleton creation: ✅
- ObserverSubject: ✅
- Containers: ❌ (synchronize externally)

**[→ API Overview](../api-overview.md)**  
**[→ DiaApplication API](application-api.md)**  
**[→ DiaMaths API](maths-api.md)**

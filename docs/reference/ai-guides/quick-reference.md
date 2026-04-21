# Quick Reference

**Last Updated:** 2026-04-01

Fast lookup tables for AI agents working with Cluiche codebase.

---

## File Locations

| What | Where |
|------|-------|
| **Entry point** | `Cluiche/CluicheTest/Main.cpp` |
| **Main thread** | `Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.h` |
| **Render thread** | `Cluiche/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h` |
| **Sim thread** | `Cluiche/ApplicationFlow/ProcessingUnits/SimProcessingUnit.h` |
| **Phases** | `Cluiche/ApplicationFlow/Phases/` |
| **Modules** | `Cluiche/CluicheKernel/ApplicationFlow/Modules/` |
| **Levels** | `Cluiche/Levels/` |
| **Containers** | `Dia/DiaCore/Containers/` |
| **Math** | `Dia/DiaMaths/` |
| **Graphics** | `Dia/DiaGraphics/Interface/ICanvas.h` |
| **Input** | `Dia/DiaInput/DiaInputEvent.h` |
| **UI** | `Dia/DiaUI/DiaUIIUISystem.h` |
| **SFML backend** | `Dia/DiaSFML/` |
| **Type system** | `Dia/DiaCore/Type/` + `Dia/DiaCore/CRC/` |
| **Patterns** | `Dia/DiaCore/Architecture/` |
| **Tests** | `Cluiche/Levels/UnitTestLevel/` |
| **Tools** | `Tools/dia_modules.py` |

---

## Namespaces

| Namespace | Purpose | Example |
|-----------|---------|---------|
| `Cluiche` | Application code | `Cluiche::MainProcessingUnit` |
| `Dia::Application` | Framework | `Dia::Application::Module` |
| `Dia::Core` | Foundation | `Dia::Core::DynamicArray<T>` |
| `Dia::Maths` | Math library | `Dia::Maths::Vector2D` |
| `Dia::Graphics` | Graphics abstraction | `Dia::Graphics::ICanvas` |
| `Dia::Window` | Window abstraction | `Dia::Window::IWindow` |
| `Dia::Input` | Input abstraction | `Dia::Input::InputEvent` |
| `Dia::UI` | UI abstraction | `Dia::UI::IUISystem` |
| `Dia::SFML` | SFML backend | `Dia::SFML::DiaSFMLRenderWindow` |
| `Dia::UIAwesomium` | Awesomium backend | `Dia::UIAwesomium::UISystem` |
| `Dia::IO` | File I/O | `Dia::IO::FilePath` |

---

## Naming Conventions

### Files

| Pattern | Example |
|---------|---------|
| Header | `ClassName.h` |
| Source | `ClassName.cpp` |
| Module architecture | `dia.parent.module.architecture.module.md` |

### Classes

| Pattern | Example |
|---------|---------|
| Class | `PascalCase` → `ProcessingUnit` |
| Interface | `IPascalCase` → `ICanvas` |
| Member variable | `mCamelCase` → `mCurrentPhase` |
| Constant | `kCamelCase` → `kUniqueId` |
| Static | `sCamelCase` → `sInstance` |

### Functions

| Pattern | Example |
|---------|---------|
| Public method | `PascalCase` → `Update()` |
| Private method | `PascalCase` or `Do` prefix → `DoStart()` |
| Getter | `GetX()` → `GetValue()` |
| Setter | `SetX()` → `SetValue()` |
| Boolean query | `IsX()` / `HasX()` → `IsReady()`, `HasData()` |

---

## Key Macros

| Macro | Purpose | Example |
|-------|---------|---------|
| `DIA_ASSERT(cond, msg)` | Runtime assertion | `DIA_ASSERT(ptr != nullptr, "Null pointer")` |
| `DIA_LOG(fmt, ...)` | Logging | `DIA_LOG("Value: %d", value)` |
| `DIA_STATIC_ASSERT(cond, msg)` | Compile-time assertion | `DIA_STATIC_ASSERT(sizeof(T) == 4, "Wrong size")` |

---

## Common Types

### Containers

| Type | Header | Purpose |
|------|--------|---------|
| `DynamicArray<T>` | `DiaCore/Containers/Arrays/DynamicArray.h` | Dynamic array (like `std::vector`) |
| `Array<T, N>` | `DiaCore/Containers/Arrays/Array.h` | Fixed-size array |
| `HashTable<K, V>` | `DiaCore/Containers/HashTables/HashTable.h` | Hash map (StringCRC keys) |
| `LinkList<T>` | `DiaCore/Containers/LinkLists/LinkList.h` | Doubly-linked list |
| `Graph<T>` | `DiaCore/Containers/Graphs/Graph.h` | Graph data structure |
| `BitFlag<N>` | `DiaCore/Containers/BitFlag/BitFlag.h` | Bit flags |

### Math

| Type | Header | Purpose |
|------|--------|---------|
| `Vector2D` | `DiaMaths/Vector/Vector2D.h` | 2D vector (x, y) |
| `Vector3D` | `DiaMaths/Vector/Vector3D.h` | 3D vector (x, y, z) |
| `Vector4D` | `DiaMaths/Vector/Vector4D.h` | 4D vector (x, y, z, w) |
| `Matrix33` | `DiaMaths/Matrix/Matrix33.h` | 3x3 matrix (2D transforms) |
| `Matrix44` | `DiaMaths/Matrix/Matrix44.h` | 4x4 matrix (3D transforms) |
| `Transform2D` | `DiaMaths/Transform/Transform2D.h` | 2D transform with hierarchy |
| `Circle` | `DiaMaths/Shape/Circle.h` | Circle shape |
| `AABB` | `DiaMaths/Shape/AABB.h` | Axis-aligned bounding box |

### Type System

| Type | Header | Purpose |
|------|--------|---------|
| `StringCRC` | `DiaCore/CRC/CRC.h` | Compile-time string hash |
| `TypeDefinition` | `DiaCore/Type/TypeDefinition.h` | Type metadata |
| `TypeRegistry` | `DiaCore/Type/TypeRegistry.h` | Type registry (singleton) |

### Time

| Type | Header | Purpose |
|------|--------|---------|
| `TimeAbsolute` | `DiaCore/Time/TimeAbsolute.h` | Absolute time |
| `TimeRelative` | `DiaCore/Timer/TimeRelative.h` | Relative time, timers |
| `TimeServer` | `DiaCore/Time/TimeServer.h` | Global time source (singleton) |

---

## Framework Classes

### Core Framework

| Class | Header | Purpose |
|-------|--------|---------|
| `ProcessingUnit` | `DiaApplication/ApplicationProcessingUnit.h` | Thread container |
| `Phase` | `DiaApplication/ApplicationPhase.h` | Execution stage |
| `Module` | `DiaApplication/ApplicationModule.h` | Functional unit |
| `StateObject` | `DiaApplication/ApplicationStateObject.h` | Stateful object base |
| `ILevel` | `DiaApplication/ApplicationStateObject.h` | Level interface |

### Patterns

| Class | Header | Purpose |
|-------|--------|---------|
| `Singleton<T>` | `DiaCore/Architecture/Singleton/Singleton.h` | Singleton pattern |
| `Factory<T>` | `DiaCore/Architecture/Factory/Factory.h` | Factory pattern |
| `Observer` | `DiaCore/Architecture/Observer/Observer.h` | Observer pattern |
| `ObserverSubject` | `DiaCore/Architecture/Observer/Observer.h` | Observer subject |
| `IComponent` | `DiaCore/Architecture/Components/IComponent.h` | Component interface |

### Threading

| Class | Header | Purpose |
|-------|--------|---------|
| `FrameStream<T>` | `DiaApplication/FrameStream.h` | Thread-safe queue |

---

## Key Constants

### Thread IDs

```cpp
static const StringCRC kMainThreadId = StringCRC("MainProcessingUnit");
static const StringCRC kRenderThreadId = StringCRC("RenderProcessingUnit");
static const StringCRC kSimThreadId = StringCRC("SimProcessingUnit");
```

### Module IDs

```cpp
// Main thread modules
static const StringCRC kMainKernelModuleId = StringCRC("MainKernelModule");
static const StringCRC kMainUIModuleId = StringCRC("MainUIModule");
static const StringCRC kLevelFactoryModuleId = StringCRC("LevelFactoryModule");

// Sim thread modules
static const StringCRC kSimTimeServerModuleId = StringCRC("SimTimeServerModule");
static const StringCRC kSimInputFrameStreamModuleId = StringCRC("SimInputFrameStreamModule");
static const StringCRC kSimUIProxyModuleId = StringCRC("SimUIProxyModule");
```

---

## Thread Ownership

| What | Thread | Notes |
|------|--------|-------|
| **UI** | Main | Awesomium requires main thread |
| **Input capture** | Main | SFML input events |
| **Rendering** | Render | 60 FPS |
| **Game logic** | Sim | Variable rate |
| **Physics** | Sim | (Stub) |
| **AI** | Sim | (Stub) |

---

## Thread Safety

### Thread-Safe

| Class/Function | Notes |
|----------------|-------|
| `FrameStream<T>::Read/Write()` | Uses mutex |
| `ObserverSubject::Attach/Detach/Notify()` | Uses mutex |
| `Random::RandomFloat()` | Uses mutex (fixed 2026-03) |
| `ProcessingUnit::QueuePhaseTransition()` | Uses mutex |
| `TimeServer` (read-only) | Immutable after init |
| `TypeRegistry` (read-only) | Immutable after registration |
| `LevelFactory` (read-only) | Immutable after registration |

### NOT Thread-Safe

| Class/Function | Notes |
|----------------|-------|
| `DynamicArray<T>` | Requires external synchronization |
| `HashTable<K, V>` | Requires external synchronization |
| `Transform2D` (hierarchy) | Traversal not thread-safe |
| `ProcessingUnit::TransitionPhase()` | Same-thread only |

---

## Common Operations

### Create Module

```cpp
class MyModule : public Dia::Application::Module
{
public:
    static const Dia::Core::StringCRC kUniqueId;
    MyModule() : Module(kUniqueId) {}
private:
    virtual void DoStart() override;
    virtual void DoUpdate() override;
    virtual void DoStop() override;
};

const Dia::Core::StringCRC MyModule::kUniqueId = StringCRC("MyModule");
```

### Add Dependency

```cpp
MyModule::MyModule()
{
    RegisterDependency(&GetDependency<OtherModule>());
}

void MyModule::DoStart()
{
    OtherModule* dep = GetDependency<OtherModule>();
}
```

### Create Level

```cpp
class MyLevel : public Dia::Application::StateObject
{
public:
    static const Dia::Core::StringCRC kUniqueId;
    MyLevel() : StateObject(kUniqueId) {}
private:
    virtual void DoStart() override;
    virtual void DoUpdate() override;
    virtual void DoStop() override;
};

// Register
LevelFactory::Instance()->Register<MyLevel>("MyLevel");
```

### Cross-Thread Communication

```cpp
// Producer (Main thread)
mFrameStream.Write(data);

// Consumer (Sim thread)
Data data;
while (mFrameStream.Read(data))
{
    Process(data);
}
```

### Observer Pattern

```cpp
// Subject
class Subject : public ObserverSubject
{
    void Event() { Notify(); }
};

// Observer
class Observer : public Observer
{
    void OnNotify() override { /* Handle event */ }
};

// Usage
subject.Attach(&observer);
subject.Event();
```

---

## Build Commands

### Visual Studio

```bash
# Open solution
start Cluiche/Cluiche.sln

# Build via MSBuild
msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64
msbuild Cluiche/Cluiche.sln /p:Configuration=Release /p:Platform=x64
```

### Configurations

| Configuration | Purpose |
|---------------|---------|
| `Debug\|x64` | Development (primary) |
| `Release\|x64` | Optimized build |

---

## Tools

### dia_modules.py

```bash
# Validate dependencies
python Tools/dia_modules.py --validate

# Generate dependency graph
python Tools/dia_modules.py --graph output.dot

# Check for cycles
python Tools/dia_modules.py --check-cycles

# List module dependencies
python Tools/dia_modules.py --list dia.core.containers
```

---

## External Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| **SFML** | 2.5.1 | Graphics, audio, input |
| **JsonCpp** | Master | JSON parsing |
| **Awesomium** | SDK | Web UI (deprecated) |
| **Webix** | - | Web UI components |
| **VisJS** | - | Graph visualization |

---

## Status Indicators

### Module Status

| Status | Meaning |
|--------|---------|
| `active` | In active development |
| `deprecated` | Being phased out |
| `stub` | Minimal implementation |

### Maturity

| Maturity | Meaning |
|----------|---------|
| `dev` | Development (unstable API) |
| `stable` | Stable (API unlikely to change) |
| `legacy` | Old code (maintenance mode) |

---

## Architecture File Schema

```yaml
schema: dia.module.v1
module_id: dia.parent.module
name: ModuleName
owner_team: TBD
layer: platform | framework | application
status: active | deprecated | stub
maturity: dev | stable | legacy

path: Relative/Path/To/Module
language: cpp
parent_module_id: dia.parent

summary: >
  One-line description

intent: >
  Detailed purpose

responsibilities:
  - What module owns

non_responsibilities:
  - What module doesn't own

dependent_modules:
  - dia.module.child1

public_api:
  headers:
    - Path/To/Header.h
  namespaces:
    - Namespace::Name
  entry_points:
    - ClassName

dependencies:
  required:
    - dia.core.containers
  forbidden:
    - dia.application
```

---

## Common Patterns

### Singleton

```cpp
class MyManager : public Dia::Core::Singleton<MyManager>
{
private:
    friend class Dia::Core::Singleton<MyManager>;
    MyManager() = default;
};

// Usage
MyManager::Create();
MyManager::Instance()->DoSomething();
MyManager::Destroy();
```

### Factory

```cpp
Factory<ILevel> factory;
factory.Register<MyLevel>(StringCRC("MyLevel"));
ILevel* level = factory.Create(StringCRC("MyLevel"));
```

### StringCRC

```cpp
static const StringCRC kMyId = StringCRC("MyId");

HashTable<StringCRC, int> table;
table.Insert(kMyId, 42);

int* value = table.Find(kMyId);
```

---

## Debugging

### Assertions

```cpp
DIA_ASSERT(ptr != nullptr, "Pointer must not be null");
DIA_ASSERT(count > 0, "Count must be positive: %d", count);
```

### Logging

```cpp
DIA_LOG("Starting module: %s", GetName());
DIA_LOG("Value: %d, Position: (%f, %f)", value, pos.x, pos.y);
```

### Thread ID

```cpp
std::thread::id threadId = std::this_thread::get_id();
DIA_LOG("Thread ID: %d", threadId);
```

---

## Summary Tables

### Most Used Containers

1. `DynamicArray<T>` - 90% of use cases
2. `HashTable<StringCRC, T>` - Fast lookup by name
3. `Array<T, N>` - Fixed size

### Most Used Math Types

1. `Vector2D` - 2D positions, velocities
2. `Matrix33` - 2D transformations
3. `Transform2D` - Hierarchical transforms

### Most Used Patterns

1. Module/Phase/ProcessingUnit - Application structure
2. Factory - Runtime object creation
3. Observer - Event notifications
4. Singleton - Global instances (sparingly)

### Thread Communication

1. `FrameStream<T>` - Producer-consumer queue (preferred)
2. `ObserverSubject` - Event broadcasts
3. `std::mutex` + `std::lock_guard` - Direct protection

---

**[→ Codebase Map](codebase-map.md)**  
**[→ Entry Points](entry-points.md)**  
**[→ Code Patterns](patterns-reference.md)**  
**[→ System Boundaries](system-boundaries.md)**  
**[→ Dependency Graph](dependency-graph.md)**  
**[→ Thread Safety Guide](thread-safety-guide.md)**

**[→ Back to AI Guide](AI-README.md)**

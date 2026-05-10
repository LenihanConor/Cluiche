# API Overview

**Last Updated:** 2026-04-01

Central hub for all Cluiche and Dia API documentation.

---

## Overview

This section documents the public API surface of Cluiche (application) and Dia (engine).

**Audience:**
- Game developers using Cluiche framework
- Engine developers extending Dia
- AI agents understanding API patterns

---

## Documentation Structure

### Dia Engine APIs

**Foundation:**
- [DiaApplicationFlow API](dia/application-api.md) - ProcessingUnit/Phase/Module framework
- [DiaCore API](dia/core-api.md) - Containers, patterns, type system

**Platform Abstractions:**
- [DiaMaths API](dia/maths-api.md) - Vectors, matrices, transforms, shapes
- [DiaGraphics API](dia/graphics-api.md) - ICanvas rendering interface
- [DiaWindow API](dia/window-api.md) - IWindow interface
- [DiaInput API](dia/input-api.md) - Input events
- [DiaUI API](dia/ui-api.md) - IUISystem interface
- [DiaIO API](dia/io-api.md) - File I/O

**Backend Implementations:**
- [DiaSFML API](dia/sfml-api.md) - SFML backend

**Specialized (Stubs):**
- [DiaPhysics API](dia/physics-api.md) - Physics simulation (stub)
- [DiaAI API](dia/ai-api.md) - AI pathfinding (stub)

---

### CluicheTest Application APIs

*API documentation in progress - see architecture docs for details:*

**Threading Architecture:**
- MainProcessingUnit - See [CluicheTest Application Architecture](../architecture/cluichetest-application.md)
- RenderProcessingUnit - See [Threading Model](../architecture/threading-model.md)
- SimProcessingUnit - See [Threading Model](../architecture/threading-model.md)

**Level System:**
- Level Interface - See [Level System](../architecture/level-system.md)

**Modules:**
- Module Catalog - See [Module System](../architecture/module-system.md)

<!-- TODO: Create dedicated Cluiche API documentation -->

---

## API Categories

### Foundation APIs (Layer 1)

**DiaCore - Core Utilities**
- Containers: `DynamicArray<T>`, `HashTable<K,V>`, `LinkList<T>`, `Graph<T>`
- Patterns: `Singleton<T>`, `Factory<T>`, `Observer`, `ObserverSubject`
- Type System: `StringCRC`, `TypeRegistry`, `TypeDefinition`
- Time: `TimeServer`, `TimeAbsolute`, `TimeRelative`
- Utilities: `DIA_ASSERT`, `DIA_LOG`, `FilePath`

**[→ DiaCore API Details](dia/core-api.md)**

---

### Framework APIs (Layer 2)

**DiaApplicationFlow - Application Framework**
- `ProcessingUnit` - Thread container
- `Phase` - Execution stage
- `Module` - Functional unit
- `StateObject` - Stateful object base
- `ILevel` - Level interface
- `LevelFactory` - Level creation
- `FrameStream<T>` - Thread-safe queue

**[→ DiaApplicationFlow API Details](dia/application-api.md)**

---

### Platform Abstraction APIs (Layer 2)

**DiaMaths - Math Library**
- Vectors: `Vector2D`, `Vector3D`, `Vector4D`
- Matrices: `Matrix22`, `Matrix33`, `Matrix44`
- Transforms: `Transform2D`, `Transform3D`
- Shapes: `Circle`, `AABB`, `Line`, `Polygon`
- Utilities: `Random`, `Lerp`, `InverseLerp`

**[→ DiaMaths API Details](dia/maths-api.md)**

**DiaGraphics - Graphics Abstraction**
- `ICanvas` - Rendering interface
- `Frame` - Frame data

**[→ DiaGraphics API Details](dia/graphics-api.md)**

**DiaWindow - Window Abstraction**
- `IWindow` - Window interface

**[→ DiaWindow API Details](dia/window-api.md)**

**DiaInput - Input Abstraction**
- `InputEvent` - Input events
- `InputSourceManager` - Input sources

**[→ DiaInput API Details](dia/input-api.md)**

**DiaUI - UI Abstraction**
- `IUISystem` - UI interface

**[→ DiaUI API Details](dia/ui-api.md)**

---

### Backend Implementation APIs (Layer 3)

**DiaSFML - SFML Integration**
- `DiaSFMLRenderWindow` - Window + Canvas
- `DiaSFMLInputSource` - Input handling
- `DiaSFMLSoundManager` - Audio

**[→ DiaSFML API Details](dia/sfml-api.md)**

---

### Application APIs (Layer 4)

**Cluiche - Application Framework**
- `MainProcessingUnit` - Main thread orchestrator
- `RenderProcessingUnit` - Render thread
- `SimProcessingUnit` - Sim thread
- Application modules (6 modules)
- Level system (DummyStage, UnitTestLevel)

**[→ Cluiche Architecture Details](../architecture/cluichetest-application.md)**

---

## API Design Principles

### 1. Explicit Over Implicit

```cpp
// ✅ Good: Explicit dependency
class MyModule : public Module {
    MyModule() {
        RegisterDependency(&GetDependency<OtherModule>());
    }
};

// ❌ Bad: Hidden dependency
class MyModule : public Module {
    void DoUpdate() {
        // Implicitly uses global OtherModule::Instance()
    }
};
```

---

### 2. Type Safety

```cpp
// ✅ Good: Type-safe retrieval
Module* mod = GetDependency<MyModule>();

// ❌ Bad: Stringly-typed
Module* mod = GetDependency("MyModule");  // Error-prone
```

---

### 3. Fail Fast

```cpp
// ✅ Good: Assert preconditions
void ProcessData(Data* data) {
    DIA_ASSERT(data != nullptr, "Data must not be null");
    // Process data
}
```

---

### 4. Const Correctness

```cpp
// ✅ Good: Const member functions
class Vector2D {
    float Magnitude() const;  // Doesn't modify
    void Normalize();         // Modifies
};
```

---

### 5. RAII (Resource Acquisition Is Initialization)

```cpp
// ✅ Good: RAII with lock_guard
void ThreadSafeOperation() {
    std::lock_guard<std::mutex> lock(mMutex);
    // Critical section
}  // Mutex automatically unlocked
```

---

## API Stability Matrix

### Stable APIs (Unlikely to Change)

| API | Status | Notes |
|-----|--------|-------|
| `DynamicArray<T>` | ✅ Stable | Core container, proven design |
| `HashTable<K,V>` | ✅ Stable | Core container |
| `Vector2D/3D` | ✅ Stable | Math fundamentals |
| `Matrix33/44` | ✅ Stable | Math fundamentals |
| `StringCRC` | ✅ Stable | Type system core |
| `ProcessingUnit` | ✅ Stable | Framework foundation |
| `Phase` | ✅ Stable | Framework foundation |
| `Module` | ✅ Stable | Framework foundation |

---

### Evolving APIs (May Change)

| API | Status | Notes |
|-----|--------|-------|
| `Transform2D` | 🚧 Evolving | Performance optimization planned (caching) |
| `ICanvas` | 🚧 Evolving | May add methods for new rendering features |
| `LevelFactory` | 🚧 Evolving | May add async level loading |
| `ComponentFactoryRegistry` | 🚧 Evolving | Component system under review |

---

### Experimental APIs

| API | Status | Notes |
|-----|--------|-------|
| `DiaPhysics` | ⚠️ Stub | Minimal implementation, API will change |
| `DiaAI` | ⚠️ Stub | Minimal implementation, API will change |

---

### Deprecated APIs

| API | Status | Replacement |
|-----|--------|-------------|
| `CollectionShit` (removed) | ❌ Removed | DiaCore/Architecture |
| `DynamicLinkList` (removed) | ❌ Removed | LinkList<T> |

---

## Common Patterns

### Creating a Module

```cpp
class MyModule : public Dia::Application::Module {
public:
    static const Dia::Core::StringCRC kUniqueId;
    MyModule();

private:
    virtual void DoStart() override;
    virtual void DoUpdate() override;
    virtual void DoStop() override;
};
```

**[→ Full Module Pattern](../ai-guides/patterns-reference.md#pattern-1-module)**

---

### Using Containers

```cpp
// Dynamic array
Dia::Core::DynamicArray<int> arr;
arr.PushBack(42);

// Hash table with StringCRC keys
Dia::Core::HashTable<Dia::Core::StringCRC, int> table;
table.Insert(Dia::Core::StringCRC("key"), 42);
```

**[→ Container Patterns](../ai-guides/patterns-reference.md#pattern-11-dynamicarray)**

---

### Math Operations

```cpp
// Vector operations
Dia::Maths::Vector2D a(1.0f, 2.0f);
Dia::Maths::Vector2D b(3.0f, 4.0f);
Dia::Maths::Vector2D result = a + b;

// Matrix transformation
Dia::Maths::Matrix33 m = Dia::Maths::Matrix33::Identity();
Dia::Maths::Vector2D transformed = m * a;
```

**[→ Math API Details](dia/maths-api.md)**

---

### Cross-Thread Communication

```cpp
// Producer (Main thread)
mFrameStream.Write(data);

// Consumer (Sim thread)
Data data;
while (mFrameStream.Read(data)) {
    ProcessData(data);
}
```

**[→ Threading Patterns](../ai-guides/thread-safety-guide.md#pattern-1-producer-consumer-framestream)**

---

## API Documentation Conventions

### Header Documentation

```cpp
/**
 * @brief One-line description
 *
 * Detailed description of what this class/function does.
 *
 * @tparam T Template parameter description
 * @param param Parameter description
 * @return Return value description
 *
 * @note Important usage notes
 * @warning Thread safety warnings
 *
 * Example:
 * @code
 * MyClass obj;
 * obj.DoSomething();
 * @endcode
 */
```

---

### Naming Conventions

**Classes:**
- PascalCase: `ProcessingUnit`, `DynamicArray`
- Interfaces: `I` prefix: `ICanvas`, `IWindow`

**Functions:**
- PascalCase: `Update()`, `GetValue()`
- Booleans: `Is`/`Has` prefix: `IsReady()`, `HasData()`

**Members:**
- `m` prefix: `mCurrentPhase`, `mModules`

**Constants:**
- `k` prefix: `kUniqueId`, `kMaxSize`

**[→ Full Naming Conventions](conventions.md)**

---

## Finding APIs

### By Category

**Need containers?** → [DiaCore API](dia/core-api.md)
**Need math?** → [DiaMaths API](dia/maths-api.md)
**Need rendering?** → [DiaGraphics API](dia/graphics-api.md)
**Need threading?** → [DiaApplicationFlow API](dia/application-api.md)

---

### By Use Case

| Use Case | API |
|----------|-----|
| **Create a module** | [Module](dia/application-api.md#module) |
| **Add a level** | [ILevel](../architecture/level-system.md) |
| **Store data** | [DynamicArray](dia/core-api.md#dynamicarray) |
| **Fast lookup** | [HashTable](dia/core-api.md#hashtable) |
| **2D math** | [Vector2D](dia/maths-api.md#vector2d) |
| **Transform hierarchy** | [Transform2D](dia/maths-api.md#transform2d) |
| **Draw shapes** | [ICanvas](dia/graphics-api.md#icanvas) |
| **Handle input** | [InputEvent](dia/input-api.md#inputevent) |
| **Cross-thread data** | [FrameStream](dia/application-api.md#framestream) |
| **Random numbers** | [Random](dia/maths-api.md#random) |

---

## External Dependencies

APIs from third-party libraries (used via abstractions):

| Library | Purpose | Direct Usage |
|---------|---------|--------------|
| **SFML** | Graphics/Audio/Input | Via DiaSFML only |
| **JsonCpp** | JSON parsing | Via DiaCore/Json only |

**Principle:** External dependencies accessed via Dia abstractions, not directly.

**[→ External Dependencies](../architecture/external-dependencies.md)**

---

## API Examples

Comprehensive examples available:
- **Code Patterns:** [Patterns Reference](../ai-guides/patterns-reference.md)
- **Entry Points:** [Common Tasks](../ai-guides/entry-points.md)
- **Sample Code:** `Cluiche/Stages/DummyStage/`
- **Tests:** `Cluiche/Levels/UnitTestLevel/`

---

## Summary

**Total APIs Documented:**
- Dia Engine: 11 subsystems
- CluicheTest Application: 3 processing units + modules + levels

**API Layers:**
1. Foundation (DiaCore)
2. Framework (DiaApplicationFlow) + Platform (DiaMaths, DiaGraphics, etc.)
3. Backend (DiaSFML)
4. Application (Cluiche)

**Design Principles:**
- Explicit over implicit
- Type safety
- Fail fast
- Const correctness
- RAII

**[→ API Conventions](conventions.md)**  
**[→ Dia APIs](dia/)**  
**[→ Cluiche Architecture](../architecture/cluichetest-application.md)**

**[→ Back to Documentation Index](../README.md)**

# Glossary

**Last Updated:** 2026-04-01

Definitions of terms, acronyms, and concepts used throughout the Cluiche codebase.

---

## Core Concepts

### Cluiche
- **Pronunciation:** KLOO-ha (Irish for "game")
- **Definition:** The application layer built on top of the Dia engine
- **Location:** `Cluiche/CluicheTest/`
- **Purpose:** Multi-threaded game framework demonstrating Dia engine usage

---

### Dia
- **Definition:** The foundational engine layer providing reusable systems
- **Location:** `Dia/`
- **Etymology:** Unknown origin, possibly short for "Diamond" or related term
- **Purpose:** Platform-agnostic game engine providing application framework, graphics, input, math, and utilities

---

## Architecture Patterns

### ProcessingUnit (PU)
- **Definition:** High-level execution container that owns modules and phases
- **Can Own:** Multiple modules, multiple phases
- **Can Run On:** Separate thread (optional)
- **Examples:** MainProcessingUnit, RenderProcessingUnit, SimProcessingUnit
- **Purpose:** Organizes application execution into logical, potentially parallelizable units

**[→ DiaApplication API](../api/dia/application-api.md)**

---

### Phase
- **Definition:** Execution stage within a ProcessingUnit with state transitions
- **Lifecycle:** Construct → Start → Running (Update loop) → Stopping → Stop → Destruct
- **Depends On:** Modules
- **Transitions:** Automatic (sequential) or manual (QueuePhaseTransition)
- **Examples:** InitPhase, UpdatePhase, ShutdownPhase
- **Purpose:** Manages application flow with explicit state machine

---

### Module
- **Definition:** Functional unit providing specific capability
- **Lifecycle:** Construct → Start → Update (every frame) → Stop → Destruct
- **Has:** Unique ID (StringCRC), dependencies on other modules
- **Examples:** RenderCanvasModule, InputModule, PhysicsModule
- **Purpose:** Encapsulates functionality with explicit dependencies and lifecycle

---

### Level
- **Definition:** Game state (e.g., menu, gameplay, level)
- **Interface:** ILevel (Load, Unload)
- **Factory:** LevelFactory (singleton, runtime creation)
- **Examples:** MainMenuLevel, GameLevel, UnitTestLevel
- **Purpose:** Swappable game states with load/unload semantics

---

## Threading

### Main Thread
- **Responsibilities:** Window events, input polling, UI updates
- **ProcessingUnit:** MainProcessingUnit
- **Key Modules:** MainKernelModule, MainInputModule, MainUIModule
- **Synchronization:** Produces events for Render and Sim threads

---

### Render Thread
- **Responsibilities:** Graphics rendering, frame presentation
- **ProcessingUnit:** RenderProcessingUnit
- **Key Modules:** RenderKernelModule, RenderCanvasModule
- **Synchronization:** Consumes frame data from Sim thread, produces visual output
- **Target:** 60 FPS (VSync-locked)

---

### Sim Thread
- **Responsibilities:** Game logic, physics, AI
- **ProcessingUnit:** SimProcessingUnit
- **Key Modules:** SimTimeServerModule, SimInputFrameModule
- **Synchronization:** Consumes input from Main thread, produces game state for Render thread
- **Target:** Variable frame rate (as fast as possible)

---

### FrameStream
- **Definition:** Thread-safe producer-consumer queue for cross-thread communication
- **Template:** `FrameStream<T>` (e.g., `FrameStream<InputEvent>`)
- **Operations:** Write() (producer), Read() (consumer)
- **Thread Safety:** Lock-free or mutex-protected (implementation detail)
- **Purpose:** Safe inter-thread data transfer without blocking

---

## Data Structures

### DynamicArray
- **Definition:** Resizable array (like `std::vector`)
- **Template:** `DynamicArray<T>`
- **Location:** `DiaCore/Containers/Arrays/`
- **Operations:** Add(), Remove(), Size(), operator[]
- **Purpose:** Most commonly used container in Cluiche

---

### HashTable
- **Definition:** Hash map (like `std::unordered_map`)
- **Template:** `HashTable<K, V>`
- **Optimized For:** StringCRC keys
- **Location:** `DiaCore/Containers/HashTables/`
- **Operations:** Insert(), Find(), Remove()
- **Purpose:** Fast key-value lookups

---

### LinkList
- **Definition:** Doubly-linked list
- **Template:** `LinkList<T>`
- **Location:** `DiaCore/Containers/LinkLists/`
- **Purpose:** Efficient insertion/removal, iteration

---

### Graph
- **Definition:** Graph data structure (nodes + edges)
- **Template:** `Graph<T>`
- **Location:** `DiaCore/Containers/Graphs/`
- **Purpose:** Dependency graphs, pathfinding

---

## Type System

### StringCRC
- **Definition:** Compile-time CRC32 hash of a string
- **Type:** `typedef unsigned int StringCRC`
- **Usage:** `StringCRC("MyString")` computes hash at compile-time
- **Size:** 4 bytes (vs. std::string's dynamic allocation)
- **Comparison:** O(1) integer comparison
- **Purpose:** Efficient string comparison without RTTI

**Example:**
```cpp
const StringCRC kModuleId = StringCRC("PhysicsModule");  // Compile-time

if (moduleId == StringCRC("PhysicsModule"))  // O(1) comparison
{
    // ...
}
```

---

### TypeRegistry
- **Definition:** Runtime type metadata system
- **Location:** `DiaCore/Type/TypeRegistry.h`
- **Features:** Type name, size, constructor, factory
- **Purpose:** Reflection and serialization without RTTI

---

## Design Patterns

### Singleton
- **Definition:** Single global instance with explicit lifetime
- **Template:** `Singleton<T>`
- **Usage:** `MyClass::Instance()`
- **Location:** `DiaCore/Architecture/Singleton/`
- **Note:** Explicit construction/destruction (not lazy-initialized)

---

### Observer
- **Definition:** Event notification pattern
- **Classes:** `Observer`, `ObserverSubject`
- **Location:** `DiaCore/Architecture/Observer/`
- **Thread Safety:** Thread-safe (uses mutex)
- **Purpose:** Decoupled event notifications

---

### Factory
- **Definition:** Object creation pattern
- **Examples:** LevelFactory, ComponentFactory
- **Purpose:** Runtime object creation by ID

---

### Proxy
- **Definition:** Cross-thread communication wrapper
- **Example:** SimUIProxy (Sim → Main UI updates)
- **Purpose:** Queue commands for execution on another thread

---

## Containers and Collections

### Component System
- **IComponent** - Base component interface
- **IComponentObject** - Object containing components
- **IComponentFactory** - Factory for creating components
- **ComponentFactoryRegistry** - Central registry of all component factories
- **StaticPooledComponentFactory** - Pre-allocated component pools

---

## File System

### FilePath
- **Definition:** Late-bound file path with alias resolution
- **Components:** Path alias + path amendment + filename
- **Example:** `FilePath(StringCRC("DATA"), "Textures/", "sprite.png")`
- **Resolution:** Resolves to full path at runtime (e.g., "C:/Game/Data/Textures/sprite.png")
- **Purpose:** Relocatable file paths

---

### Path Alias
- **Definition:** StringCRC identifier for a base path
- **Examples:** StringCRC("DATA"), StringCRC("CONFIG"), StringCRC("SAVE")
- **Registration:** Registered at application startup
- **Purpose:** Relocatable paths (change once, applies everywhere)

---

## Graphics

### ICanvas
- **Definition:** Abstract rendering interface
- **Methods:** DrawLine, DrawCircle, DrawRectangle, DrawSprite, DrawText, Clear, Present
- **Implementation:** DiaSFMLRenderWindow (SFML backend)
- **Thread:** Render thread only

---

### Frame
- **Definition:** Frame data structure (background color + view transform)
- **Location:** `DiaGraphics/Interface/Frame.h`
- **Purpose:** Render configuration per frame

---

## Input

### InputEvent
- **Definition:** Platform-agnostic input event
- **Types:** KeyPressed, KeyReleased, MouseButtonPressed, MouseMoved, etc.
- **Location:** `DiaInput/DiaInputEvent.h`
- **Thread:** Polled on Main thread, forwarded to Sim via FrameStream

---

### InputSourceManager
- **Definition:** Manages multiple input sources (keyboard, mouse, gamepad)
- **Method:** PollEvent(InputEvent& outEvent)
- **Purpose:** Unified input polling

---

## Math

### Vector2D / Vector3D
- **Definition:** 2D/3D vector (position, direction, velocity)
- **Members:** x, y (and z for 3D)
- **Operations:** +, -, *, /, Magnitude(), Normalize(), Dot(), Cross()

---

### Matrix33 / Matrix44
- **Definition:** 3x3 / 4x4 transformation matrix
- **Operations:** operator*, Transpose(), Inverse()
- **Factory Methods:** Identity(), Translation(), Rotation(), Scale()

---

### Transform2D
- **Definition:** Hierarchical 2D transform (position + rotation + scale)
- **Hierarchy:** Parent/child relationships
- **Matrices:** GetLocalMatrix(), GetWorldMatrix()
- **⚠️ Issue:** GetWorldMatrix() slow (no caching, traverses hierarchy every call)

---

## UI

### IUISystem
- **Definition:** Abstract UI interface
- **Methods:** Initialize, Update, LoadUI, ExecuteJavaScript, BindCallback
- **Implementation:** UISystem (Awesomium - deprecated)
- **Thread:** Main thread only

---

## Backend

### SFML (Simple and Fast Multimedia Library)
- **Version:** 2.5.1
- **Purpose:** Graphics, window, input, audio backend
- **Location:** `External/SFML-2.5.1/`
- **Wrapper:** DiaSFML

---

### Awesomium
- **Status:** ⚠️ DEPRECATED (no longer maintained)
- **Purpose:** Web-based UI (HTML/CSS/JS)
- **Location:** `External/Awesomium SDK/`
- **Replacement:** CEF (Chromium Embedded Framework) or ImGui

---

## Testing

### UnitTestLevel
- **Definition:** In-engine test harness
- **Location:** `Cluiche/CluicheTest/Levels/UnitTestLevel/`
- **Purpose:** Run unit tests within the application

---

### DIA_ASSERT
- **Definition:** Assertion macro (debug only)
- **Usage:** `DIA_ASSERT(condition, "message")`
- **Behavior:** Breaks into debugger if condition false
- **Location:** `DiaCore/Core/Assert.h`

---

## Build System

### .vcxproj
- **Definition:** Visual Studio C++ project file (XML)
- **Contains:** Source files, include directories, library dependencies, build settings
- **Example:** `Cluiche/CluicheTest/Cluiche.vcxproj`

---

### .vcxproj.filters
- **Definition:** Visual Studio project filters (for IDE organization)
- **Purpose:** Folder structure in Solution Explorer (does not affect build)

---

### MSBuild
- **Definition:** Microsoft Build Engine (command-line build tool)
- **Usage:** `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=Win32`

---

## Naming Conventions

### PascalCase
- **Used For:** Classes, structs, enums, functions
- **Examples:** `ProcessingUnit`, `DynamicArray`, `GetPosition()`

---

### camelCase
- **Used For:** Local variables, parameters
- **Examples:** `deltaTime`, `playerPosition`

---

### mCamelCase
- **Used For:** Member variables (private/protected)
- **Examples:** `mPosition`, `mVelocity`, `mCurrentPhase`
- **Prefix:** `m` for "member"

---

### kPascalCase
- **Used For:** Constants, static const, enum values
- **Examples:** `kUniqueId`, `kMaxSize`, `kPi`
- **Prefix:** `k` for "konstant" (borrowed from Google C++ Style Guide)

---

### IClassName
- **Used For:** Interfaces (abstract base classes)
- **Examples:** `ICanvas`, `IWindow`, `IInputSource`, `ILevel`
- **Prefix:** `I` for "interface"

---

## Acronyms

| Acronym | Full Term | Meaning |
|---------|-----------|---------|
| **PU** | ProcessingUnit | High-level execution container |
| **FSM** | Finite State Machine | State-based logic system |
| **CRC** | Cyclic Redundancy Check | Hash algorithm (used for StringCRC) |
| **RTTI** | Run-Time Type Information | C++ runtime type system (not used in Cluiche) |
| **SFML** | Simple and Fast Multimedia Library | Graphics/audio/input library |
| **CEF** | Chromium Embedded Framework | Web browser backend |
| **AABB** | Axis-Aligned Bounding Box | Collision shape |
| **FPS** | Frames Per Second | Frame rate metric |
| **VSync** | Vertical Synchronization | Frame rate limiting to monitor refresh |
| **UI** | User Interface | Menus, HUD, etc. |
| **HUD** | Heads-Up Display | In-game UI overlay |
| **AI** | Artificial Intelligence | Game AI (pathfinding, behaviors) |
| **API** | Application Programming Interface | Public interface of a library/module |

---

## Common Abbreviations

| Abbreviation | Full Term |
|--------------|-----------|
| **Vec** | Vector |
| **Mat** | Matrix |
| **Pos** | Position |
| **Vel** | Velocity |
| **Dir** | Direction |
| **Rot** | Rotation |
| **Mag** | Magnitude |
| **Min** | Minimum |
| **Max** | Maximum |
| **Init** | Initialize |
| **Deinit** | Deinitialize |
| **Ctor** | Constructor |
| **Dtor** | Destructor |
| **Ptr** | Pointer |
| **Ref** | Reference |
| **Idx** | Index |
| **Num** | Number |
| **Cnt** | Count |
| **Buf** | Buffer |
| **Src** | Source |
| **Dst** | Destination |

---

## Module IDs

**Common Module IDs (StringCRC):**
- `MainKernelModule`
- `RenderKernelModule`
- `SimKernelModule`
- `MainInputModule`
- `RenderCanvasModule`
- `SimTimeServerModule`
- `LevelFactoryModule`

---

## Phase IDs

**Common Phase IDs (StringCRC):**
- `InitPhase`
- `BootPhase`
- `UpdatePhase`
- `ShutdownPhase`

---

## Summary

**Key Terms:**
- **Cluiche** - Application layer (game)
- **Dia** - Engine layer (foundation)
- **ProcessingUnit** - Execution container (thread-owner)
- **Module** - Functional unit (capability provider)
- **Phase** - Execution stage (state machine)
- **Level** - Game state (menu, gameplay)
- **StringCRC** - Compile-time string hash (4 bytes)
- **FrameStream** - Thread-safe queue (cross-thread communication)

**Naming:**
- PascalCase (classes, functions)
- mCamelCase (members)
- kPascalCase (constants)
- IClassName (interfaces)

**Threads:**
- Main (window, input, UI)
- Render (graphics, 60 FPS)
- Sim (game logic, variable FPS)

**[→ Quickstart](quickstart.md)**  
**[→ Common Tasks](common-tasks.md)**  
**[→ Architecture](../architecture/architecture.md)**

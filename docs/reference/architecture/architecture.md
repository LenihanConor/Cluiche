# Cluiche Platform Architecture

**Last Updated:** 2026-04-12

This document provides a comprehensive architectural overview of the Cluiche game development platform, the Dia engine, and applications built on the platform.

---

## System Overview

The **Cluiche platform** supports multiple applications, all built on the **Dia engine**:

```
┌─────────────────────────────────────────────────────────┐
│  Cluiche Platform                                        │
│  - Multi-application game development platform          │
│  - Spec-driven development workflow                     │
│  - Shared engine infrastructure                         │
└─────────────────────────────────────────────────────────┘
                         ↓ contains
┌─────────────────────────────────────────────────────────┐
│  Applications                                            │
│  - Dia (Engine) - Shared engine infrastructure          │
│  - Cluiche (Game) - Demo game and testbed               │
│  - GoogleTest - Unit testing suite                      │
│  - Future Games - Your game projects                    │
└─────────────────────────────────────────────────────────┘
                         ↓ depend on
┌─────────────────────────────────────────────────────────┐
│  Dia Engine (13+ subsystems)                            │
│  - DiaCore, DiaMaths, DiaGraphics, DiaCLI          │
│  - DiaApplication (Module/Phase/ProcessingUnit)         │
│  - Graphics, Input, Physics, UI, Build Tools            │
└─────────────────────────────────────────────────────────┘
                         ↓ depends on
┌─────────────────────────────────────────────────────────┐
│  External Dependencies                                   │
│  - SFML (Graphics/Audio/Input)                          │
│  - JsonCpp (Configuration)                              │
│  - Awesomium (Web UI)                                   │
└─────────────────────────────────────────────────────────┘
```

**[→ Detailed system diagram](diagrams/system-overview.mmd)**

---

## Architectural Principles

The Cluiche architecture is guided by these core principles:

### 1. Module-Based Composition

- Functionality organized into **Modules** with explicit dependencies
- Modules are reusable, testable, and composable
- Dependency injection via module registration
- No hidden dependencies or global state

### 2. Phase-Driven Execution

- Execution organized into discrete **Phases** (Boot, BootStrap, Running)
- Clear lifecycle: Start → Update (loop) → Stop
- Phase transitions are explicit and traceable
- Modules can be retained across phase transitions

### 3. Multi-Threaded by Design

- Three independent threads: **Main**, **Render**, **Sim**
- Explicit thread boundaries and synchronization
- Thread-safe communication via mutex-protected state
- Independent update rates per thread (Render @ 60 FPS, Sim variable)

### 4. Type-Safe Reflection System

- Compile-time type IDs via `StringCRC`
- No runtime type information (RTTI) overhead
- Serialization support (JSON, text)
- Type registry for runtime queries

### 5. Platform Abstraction

- Dia engine abstracts platform-specific details
- Backend implementations (DiaSFML for SFML)
- Portable core (DiaCore, DiaMaths)
- Windows-primary, designed for cross-platform

---

## Component Architecture

### Dia Engine Application

**Purpose:** Shared game engine infrastructure providing all subsystems (DiaCore, DiaMaths, DiaGraphics, DiaCLI, etc.)

**Role:** While organized as an "application" in the spec hierarchy, Dia functionally serves as the shared engine code that all other applications (games, tools, tests) depend on.

### Cluiche Game Application

**Purpose:** Demo game and testbed built on Dia engine

**Entry Point:** `Cluiche/CluicheTest/Main.cpp`

```cpp
int main() {
    Cluiche::MainProcessingUnit mainPU;  // Main thread orchestrator
    mainPU.Start();                       // Initialize phases
    mainPU.Update();                      // Main loop
    mainPU.Stop();                        // Shutdown
}
```

**Key Components:**

#### Processing Units (Threads)

Three independent threads coordinate the application:

| Processing Unit | Thread | Phases | Update Rate | Responsibility |
|----------------|--------|---------|-------------|----------------|
| **MainProcessingUnit** | Main | Boot → BootStrap | As needed | Bootstrap, UI coordination, thread spawning |
| **RenderProcessingUnit** | Render | Running | 60 FPS | Graphics rendering, display management |
| **SimProcessingUnit** | Sim | Boot → BootStrap | Variable | Game simulation, physics, logic |

**[→ Threading model details](threading-model.md)**  
**[→ Threading diagram](diagrams/threading-model.mmd)**

#### Core Modules

Modules provide functionality to phases:

| Module | Thread | Purpose |
|--------|--------|---------|
| **MainKernelModule** | Main | Time server, input system, SFML window, canvas |
| **MainUIModule** | Main | Awesomium UI system initialization |
| **LevelFactoryModule** | Main | Level registry and factory |
| **SimTimeServerModule** | Sim | Simulation clock (independent from Main) |
| **SimInputFrameStreamModule** | Sim | Input event consumer (reads from Main) |
| **SimUIProxyModule** | Sim | Cross-thread UI bridge (to Main UI) |

**[→ Module system details](module-system.md)**  
**[→ Module dependencies diagram](diagrams/module-dependencies.mmd)**

#### Level System

Pluggable game states via `ILevel` interface:

- **DummyLevel** - Example level implementation
- **UnitTestLevel** - Testing harness for in-engine tests
- **Custom Levels** - Registered via `LevelFactory`, loaded at runtime

**[→ Level system details](level-system.md)**  
**[→ Level lifecycle diagram](diagrams/level-lifecycle.mmd)**

**[→ Cluiche application architecture details](cluichetest-application.md)**

---

### Dia Engine Layer

**Purpose:** Platform-agnostic game engine with 13 subsystems

**Philosophy:** Provide reusable, testable, composable building blocks for game development

#### Core Subsystems

##### 1. DiaApplication

**Module/Phase/ProcessingUnit Framework**

The foundational threading architecture:

- **ProcessingUnit** - Thread orchestrator with phase queue
- **Phase** - Execution stage with module management
- **Module** - Functional unit with dependency tracking
- **StateObject** - State machine base class

**Key Features:**
- Thread-safe phase transitions (queued with mutex)
- Dependency resolution and ordering
- Lifecycle management (Start → Update → Stop)
- Frequency control via `TimeThreadLimiter`

**[→ API Documentation](../api/dia/application-api.md)**  
<!-- TODO: Subsystem Deep Dive - see API docs for now -->

##### 2. DiaCore

**Core Utilities and Containers**

Foundation for all other subsystems:

- **Containers** - Custom data structures (Array, DynamicArray, HashTable, Graph, LinkList)
- **Type System** - Compile-time type IDs (StringCRC), reflection (TypeRegistry)
- **Time Management** - SystemClock, TimeAbsolute, TimeRelative, TimeServer
- **Memory Management** - Custom allocators, tracking
- **Strings** - Fixed-size strings (String64, String256)
- **Frame** - FrameStream for state capture
- **CRC** - String hashing utilities
- **FilePath** - Path management
- **Json** - JSON serialization (via JsonCpp)

**Why Custom Containers?**
- Explicit memory control (no hidden allocations)
- Platform portability (no STL dependency)
- Performance tuning (known memory layout)
- Debugging visibility (custom assertions)

**[→ API Documentation](../api/dia/core-api.md)**  
<!-- TODO: Subsystem Deep Dive - see API docs for now -->

##### 3. DiaGraphics

**Platform-Agnostic Rendering**

Rendering abstraction layer:

- **ICanvas** - Drawing interface (lines, circles, sprites, text)
- **IDrawable** - Drawable object interface
- **IRenderTarget** - Render target abstraction
- **IView** - Viewport abstraction
- **FrameData** - Graphics command buffer
- **DebugFrameData** - Debug visualization (visitor pattern)

**Backend:** DiaSFML implements ICanvas via SFML

**[→ API Documentation](../api/dia/graphics-api.md)**  
<!-- TODO: Subsystem Deep Dive - see API docs for now -->

##### 4. DiaMaths

**Mathematical Utilities**

Comprehensive math library:

- **Core** - Angle, Easing, Interpolation, Random, Trigonometry
- **Vector** - Vector2D, Vector3D, Vector4D
- **Matrix** - Matrix22, Matrix33, Matrix44
- **Transform** - Transform2D, Transform3D (hierarchy support)
- **Shape** - Circle, Rectangle, AABB, Sphere, intersection tests

**Known Issues:**
- Template specialization bugs (InverseLerp, MoveTowards for vectors)
- Transform2D multiple hierarchy traversals (not optimized)
- Random number generation recently fixed for thread safety

**[→ API Documentation](../api/dia/maths-api.md)**  
**[→ Known Issues](../subsystems/dia-maths/known-issues.md)**

##### 5. DiaInput

**Input Event System**

Input abstraction with multiple device support:

- **Event** - Base event class with EventData
- **InputSourceManager** - Aggregates multiple input sources
- **ConsoleGamepad** - Gamepad support
- **Keyboard/Mouse Events** - Standard input events
- **IInputSource** - Backend input interface

**Backend:** DiaSFML provides SFML input source

**[→ API Documentation](../api/dia/input-api.md)**

##### 6. DiaUI

**UI Framework**

UI system with web integration:

- **IUISystem** - Abstract UI system interface
- **Page** - UI page container
- **BoundMethod** - Method binding for UI callbacks
- **UIDataBuffer** - Data exchange with UI

**Backend:** DiaUIAwesomium provides Awesomium (web-based UI)

**[→ API Documentation](../api/dia/ui-api.md)**

##### 7. DiaWindow

**Window Management**

Platform-agnostic window abstraction:

- **IWindow** - Window interface
- Window creation, sizing, event handling

**Backend:** DiaSFML provides SFML window implementation

**[→ API Documentation](../api/dia/window-api.md)**

##### 8. DiaSFML

**SFML Backend**

SFML integration layer:

- **RenderWindow** - SFML render target wrapper
- **InputSource** - SFML input handler
- **RenderWindowFactory** - Factory for SFML windows
- Type conversion utilities (Dia ↔ SFML)

**[→ API Documentation](../api/dia/sfml-api.md)**

##### 9-13. Additional Subsystems

- **DiaIO** - File I/O (stub)
- **DiaPhysics** - Physics simulation (stub)
- **DiaAI** - AI pathfinding (stub)
- **DiaArchitecture** - Architectural patterns (Component system, Observer, Singleton)
- **DiaUIAwesomium** - Awesomium UI integration

**[→ Dia engine architecture details](dia-engine.md)**

---

### External Dependencies

**Purpose:** Third-party libraries for platform-specific functionality

| Library | Version | Purpose | Usage |
|---------|---------|---------|-------|
| **SFML** | 2.x | Graphics, audio, input, windowing | DiaSFML backend, primary rendering |
| **JsonCpp** | master | JSON parsing and serialization | Configuration files, save/load |
| **Awesomium SDK** | - | Web-based UI framework | DiaUIAwesomium, UI rendering |
| **Webix** | Multiple | JavaScript UI framework | UI pages, web console |
| **VisJS** | - | Data visualization | Debugging visualizations |

**[→ External dependencies details](external-dependencies.md)**  
**[→ External documentation links](../registry/external-links.md)**

---

## Threading Model

Cluiche uses **three independent threads** for clear separation of concerns:

```
Main Thread
    ├─ Bootstrap application
    ├─ Coordinate UI (Awesomium on main thread)
    ├─ Collect input events → InputToSimFrameStream
    ├─ Spawn RenderProcessingUnit thread
    └─ Spawn SimProcessingUnit thread

Render Thread
    ├─ Read SimToRenderFrameStream (graphics commands)
    ├─ Render to SFML canvas
    └─ Display at 60 FPS (TimeThreadLimiter)

Sim Thread
    ├─ Read InputToSimFrameStream (input events)
    ├─ Update game logic
    ├─ Run physics simulation
    └─ Write SimToRenderFrameStream (graphics commands)
```

### Thread Communication

**Main → Sim:** Input events via `InputToSimFrameStream<EventData>`  
**Sim → Render:** Graphics commands via `SimToRenderFrameStream<FrameData>`  
**Main ↔ Sim:** UI updates via `SimUIProxyModule` (observer pattern with mutex)

### Synchronization Mechanisms

- `std::mutex` - Phase transition queues, observer notifications, UI proxy access
- `FrameStream<T>` - Thread-safe temporal frame buffers
- Observer pattern - Thread-safe event notifications

**No lock-free data structures** - Uses standard mutexes for simplicity

**[→ Threading model deep dive](threading-model.md)**  
**[→ Threading sequence diagram](diagrams/threading-model.mmd)**

---

## Module System

**Module/Phase/ProcessingUnit** is Dia's core architectural pattern.

### Key Concepts

**Module** - Functional unit with explicit dependencies
```cpp
class MyModule : public Dia::Application::Module {
public:
    MyModule() {
        RegisterDependency(&GetDependency<OtherModule>());
    }
private:
    void DoStart() override;   // Initialize
    void DoUpdate() override;  // Per-frame update
    void DoStop() override;    // Cleanup
};
```

**Phase** - Execution stage containing modules
```cpp
class MyPhase : public Dia::Application::Phase {
public:
    MyPhase() {
        AddModule(&myModuleInstance);
    }
};
```

**ProcessingUnit** - Thread orchestrator running phases
```cpp
class MyProcessingUnit : public Dia::Application::ProcessingUnit {
public:
    MyProcessingUnit() {
        TransitionPhase(&myPhase);
    }
};
```

### Module Lifecycle

1. **Build Dependencies** - Modules declare dependencies in constructor
2. **Dependency Resolution** - Phase resolves order based on dependencies
3. **Start** - Modules started in dependency order
4. **Update** - Modules updated each frame
5. **Stop** - Modules stopped in reverse dependency order

### Phase Lifecycle

```
[Initial] → Boot → BootStrap → Running → Stopping → [Complete]
            ↑                      ↓
            └──── Can transition ──┘
```

**[→ Module system deep dive](module-system.md)**  
**[→ Phase execution diagram](diagrams/phase-execution.mmd)**  
**[→ Module dependencies diagram](diagrams/module-dependencies.mmd)**

---

## Level System

**Levels** = Pluggable game states managed by `LevelFactory`

### ILevel Interface

```cpp
class ILevel {
public:
    virtual void Start() = 0;
    virtual void Update() = 0;
    virtual void Stop() = 0;
};
```

### Level Registration

```cpp
// In MainBootPhase
LevelFactory::Instance().Register<DummyLevel>("DummyLevel");
LevelFactory::Instance().Register<UnitTestLevel>("UnitTests");
```

### Level Loading

Levels can contain:
- Custom phases (level-specific execution stages)
- Custom UI pages (level-specific interface)
- Custom modules (level-specific systems)

**[→ Level system deep dive](level-system.md)**  
**[→ Level lifecycle diagram](diagrams/level-lifecycle.mmd)**

---

## Key Design Patterns

Cluiche and Dia employ several design patterns:

### 1. Factory Pattern

**LevelFactory** - Creates levels by type
```cpp
ILevel* level = LevelFactory::Instance().Create("DummyLevel");
```

### 2. Observer Pattern

**ObserverSubject** - Notifies observers of events
```cpp
class UIModule : public ObserverSubject {
    void NotifyReady() { Notify(); }
};

class UIProxyModule : public Observer {
    void OnNotify() override { /* Handle event */ }
};
```

### 3. State Pattern

**Phase** - Represents execution state
```cpp
ProcessingUnit transitions between phases:
BootPhase → BootStrapPhase → RunningPhase
```

### 4. Proxy Pattern

**SimUIProxyModule** - Bridges Sim thread to Main UI
```cpp
// Sim thread can safely access UI via proxy
uiProxy.SendMessage("button_clicked");
```

### 5. Visitor Pattern

**DebugFrameDataVisitor** - Renders debug geometry
```cpp
frameData.Accept(renderVisitor);
```

**[→ Design patterns details](../design-rationale/design-patterns.md)**

---

## Module Architecture Files

Dia contains **56 module architecture files** (`.architecture.module.md`) with structured YAML metadata.

**Location:** `Dia/**/Docs/dia.<subsystem>.architecture.module.md`

**Schema:** `dia.module.v1`

**Example:**
```yaml
---
schema: dia.module.v1
module_id: dia.application
name: Application
dependencies:
  required:
    - dia.core.containers.arrays
public_api:
  headers:
    - Dia/DiaApplication/ApplicationModule.h
---
```

**Complete Registry:** [Module Registry](../registry/module-registry.md) lists all 56 modules

**Schema Documentation:** [Module Metadata Schema](../registry/module-metadata-schema.md)

---

## Architecture Diagrams

### Available Diagrams

1. **[System Overview](diagrams/system-overview.mmd)** - High-level three-layer structure
2. **[Threading Model](diagrams/threading-model.mmd)** - Main/Render/Sim thread interaction (sequence diagram)
3. **[Module Dependencies](diagrams/module-dependencies.mmd)** - Module relationships across threads
4. **[Phase Execution](diagrams/phase-execution.mmd)** - Phase state machine
5. **[Level Lifecycle](diagrams/level-lifecycle.mmd)** - Level state machine

All diagrams use **Mermaid** format and render in GitHub/markdown viewers.

---

## File Organization

### CluicheTest Application

```
Cluiche/CluicheTest/
├── Main.cpp                           # Entry point
├── ApplicationFlow/
│   ├── ProcessingUnits/               # Thread orchestrators
│   └── Phases/                        # Phase implementations
├── CluicheKernel/
│   ├── LevelFactory.h                 # Level registry
│   └── ApplicationFlow/Modules/       # Core modules
└── Levels/                            # Level implementations
```

### Dia Engine

```
Dia/
├── dia.root.architecture.module.md    # Root module definition
├── Dia<Subsystem>/                    # Subsystem directory
│   ├── Docs/                          # Architecture docs
│   ├── <Subsystem><Class>.h           # Public API
│   ├── <Subsystem><Class>.cpp         # Implementation
│   └── <Subsystem>.vcxproj            # Visual Studio project
```

**[→ Complete file locations](../registry/file-locations.md)**

---

## Build System

**Visual Studio Projects** (.sln/.vcxproj)

- **Main Solution:** `Cluiche/Cluiche.sln`
- **Per-Subsystem Projects:** Each Dia subsystem has `.vcxproj` file
- **No CMake** - Windows/Visual Studio only currently

**[→ Visual Studio guide](../development/visual-studio-guide.md)**

---

## Further Reading

### Architecture Deep Dives

- [CluicheTest Application Architecture](cluichetest-application.md) - Application layer details
- [Dia Engine Architecture](dia-engine.md) - Engine subsystems
- [Threading Model](threading-model.md) - Multi-threaded design
- [Module System](module-system.md) - Module/Phase/PU pattern
- [Level System](level-system.md) - Level loading and lifecycle
- [External Dependencies](external-dependencies.md) - Third-party integration

### Design Rationale

- [Design Philosophy](../design-rationale/design.md) - Why things are the way they are
- [Why Dia Engine?](../design-rationale/why-dia.md) - Engine design decisions
- [Why Module/Phase/PU?](../design-rationale/why-module-phase-pu.md) - Threading rationale

### API Documentation

- [API Overview](../api/api-overview.md) - Public interface reference
- [DiaApplication API](../api/dia/application-api.md) - Framework API
- [DiaCore API](../api/dia/core-api.md) - Core utilities API

### For AI Agents

- [AI-README](../ai-guides/AI-README.md) - AI-optimized entry point
- [Codebase Map](../ai-guides/codebase-map.md) - Structured navigation
- [Entry Points](../ai-guides/entry-points.md) - Task-based starting points

---

## Summary

Cluiche is a **multi-threaded game framework** built on the **Dia engine**, using a **phase-based, module-driven architecture** with three independent threads (Main, Render, Sim). The system prioritizes **explicit design**, **composition over inheritance**, **thread safety**, and **type safety**.

Key architectural features:
- ✅ Module/Phase/ProcessingUnit threading framework
- ✅ Three-thread design (Main/Render/Sim)
- ✅ Pluggable level system
- ✅ Platform abstraction (Dia engine)
- ✅ Type-safe reflection (StringCRC, TypeRegistry)
- ✅ Custom containers (explicit memory control)
- ✅ 56 structured module definitions

**Status:** Active development, Windows-primary, designed for cross-platform expansion
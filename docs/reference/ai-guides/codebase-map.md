# Codebase Map

**Last Updated:** 2026-04-01

Structured navigation guide for AI agents to understand and traverse the Cluiche codebase.

---

## Quick Navigation

**For Common Tasks:**
- Add a module → `Dia/Dia<Subsystem>/`
- Add a level → `Cluiche/Levels/`
- Modify threading → `Cluiche/ApplicationFlow/ProcessingUnits/`
- Fix math bug → `Dia/DiaMaths/`
- Update UI → `Cluiche/CluicheKernel/ApplicationFlow/Modules/MainUIModule.h`

**[→ Entry Points for Tasks](entry-points.md)**

---

## Directory Structure

### Root Level

```
C:\GitHub\Cluiche\
├── Cluiche/           # Main solution and application
├── Dia/               # Engine subsystems
├── External/          # Third-party dependencies
├── Tools/             # Build and development tools
└── docs/              # Documentation (this file)
```

---

## Cluiche/ - Application Layer

**Location:** `Cluiche/`

**Purpose:** Visual Studio solution and main executable application

### Key Directories

#### `Cluiche/CluicheTest/` - Main Executable

**Files:**
- `Main.cpp` - **ENTRY POINT** (creates MainProcessingUnit, runs main loop)
- `Cluiche.vcxproj` - Visual Studio project file
- `Cluiche.vcxproj.filters` - IDE file organization

**Subdirectories:**
- `ApplicationFlow/` - Threading architecture
- `Levels/` - Game levels
- `CluicheKernel/` - Core application modules

---

#### `Cluiche/CluicheTest/ApplicationFlow/` - Threading

**Purpose:** Processing units that run on separate threads

**Files:**
```
ApplicationFlow/
├── ProcessingUnits/
│   ├── MainProcessingUnit.h/cpp       # Main thread orchestrator
│   ├── RenderProcessingUnit.h/cpp     # Render thread (60 FPS)
│   └── SimProcessingUnit.h/cpp        # Sim thread (variable rate)
└── Phases/
    ├── MainBootPhase.h                # Main thread boot phase
    ├── MainBootStrapPhase.h           # Main thread bootstrap phase
    ├── RenderRunningPhase.h           # Render loop phase
    ├── SimBootPhase.h                 # Sim thread boot phase
    └── SimBootStrapPhase.h            # Sim thread bootstrap phase
```

**Key Classes:**
- `MainProcessingUnit` - Spawns Render and Sim threads, manages UI
- `RenderProcessingUnit` - Maintains 60 FPS, renders to window
- `SimProcessingUnit` - Updates game logic at variable rate

**When to Modify:**
- Adding a new thread
- Changing thread synchronization
- Modifying phase transitions

---

#### `Cluiche/Levels/` - Game Levels

**Purpose:** Pluggable game states using factory pattern

**Files:**
```
Levels/
├── DummyLevel/
│   ├── DummyLevel.h/cpp               # Simple example level
│   └── dia.cluiche.levels.dummylevel.architecture.module.md
└── UnitTestLevel/
    ├── UnitTestLevel.h/cpp            # In-engine test harness
    └── dia.cluiche.levels.unittestlevel.architecture.module.md
```

**Key Classes:**
- `ILevel` - Interface (defined in DiaApplication)
- `DummyLevel` - Simple demo level
- `UnitTestLevel` - Runs tests at startup

**When to Modify:**
- Creating a new game level
- Adding unit tests via UnitTestLevel

**[→ Level System Details](../architecture/level-system.md)**

---

#### `Cluiche/CluicheKernel/ApplicationFlow/Modules/` - Core Modules

**Purpose:** Functional modules used by phases

**Files:**
```
Modules/
├── MainKernelModule.h                 # Main thread kernel
├── MainUIModule.h                     # UI initialization
├── LevelFactoryModule.h               # Level factory setup
├── SimTimeServerModule.h              # Sim thread time management
├── SimInputFrameStreamModule.h        # Input forwarding to Sim
└── SimUIProxyModule.h                 # Sim → Main UI proxy
```

**Key Modules:**
- `MainKernelModule` - Entry point for Main thread phases
- `MainUIModule` - Initializes web-based UI
- `LevelFactoryModule` - Registers all levels
- `SimTimeServerModule` - Time source for simulation
- `SimInputFrameStreamModule` - Forwards input from Main to Sim
- `SimUIProxyModule` - Allows Sim to send UI messages

**When to Modify:**
- Adding functionality to a specific thread
- Changing module dependencies

---

#### `Cluiche/Tests/` - Test Projects

**Purpose:** Unit and integration tests (currently minimal)

**Structure:**
```
Tests/
├── UnitTests/                         # Unit test projects
└── [Other test projects]
```

**When to Modify:**
- Adding new tests
- Integrating Google Test

---

## Dia/ - Engine Layer

**Location:** `Dia/`

**Purpose:** Reusable engine subsystems (platform-agnostic)

### Subsystem Overview

| Subsystem | Purpose | Key Files |
|-----------|---------|-----------|
| **DiaApplication** | Application framework | `ProcessingUnit.h`, `Phase.h`, `Module.h` |
| **DiaCore** | Foundation library | `Containers/`, `Architecture/`, `Type/` |
| **DiaMaths** | Math library | `Vector/`, `Matrix/`, `Transform/`, `Shape/` |
| **DiaGraphics** | Graphics abstraction | `Interface/ICanvas.h` |
| **DiaWindow** | Window management | `Interface/IWindow.h` |
| **DiaInput** | Input handling | `DiaInputEvent.h` |
| **DiaUI** | UI abstraction | `DiaUIIUISystem.h` |
| **DiaSFML** | SFML backend | `DiaSFMLRenderWindow.h` |
| **DiaArchitecture** | Patterns | (deprecated, merged into DiaCore) |
| **DiaIO** | File I/O | `FilePath.h` |
| **DiaPhysics** | Physics (stub) | Minimal implementation |
| **DiaAI** | AI (stub) | Minimal implementation |

---

### DiaApplication/ - Application Framework

**Location:** `Dia/DiaApplication/`

**Purpose:** ProcessingUnit/Phase/Module pattern

**Key Files:**
```
DiaApplication/
├── ApplicationProcessingUnit.h        # Base class for all PUs
├── ApplicationPhase.h                 # Base class for all phases
├── ApplicationModule.h                # Base class for all modules
├── ApplicationStateObject.h           # Base for stateful objects
└── dia.application.architecture.module.md
```

**Key Classes:**
- `ProcessingUnit` - Thread container
- `Phase` - Execution stage within a ProcessingUnit
- `Module` - Functional unit that phases depend on
- `StateObject` - Base for objects with lifecycle

**When to Use:**
- Creating custom processing units
- Defining new phases
- Implementing modules

**Namespaces:** `Dia::Application::`

---

### DiaCore/ - Foundation Library

**Location:** `Dia/DiaCore/`

**Purpose:** Core utilities, containers, patterns, type system

**Structure:**
```
DiaCore/
├── Containers/                        # Custom data structures
│   ├── Arrays/
│   │   ├── Array.h                    # Fixed-size array
│   │   └── DynamicArray.h             # Dynamic array (like std::vector)
│   ├── HashTables/
│   │   └── HashTable.h                # Hash map (StringCRC keys)
│   ├── LinkLists/
│   │   └── LinkList.h                 # Doubly-linked list
│   ├── BitFlag/
│   │   └── BitFlag.h                  # Bit flags
│   ├── Graphs/
│   │   └── Graph.h                    # Graph data structure
│   └── Strings/
│       └── String.h                   # Custom string
├── Architecture/                      # Design patterns
│   ├── Singleton/
│   │   └── Singleton.h                # Singleton pattern
│   ├── Factory/
│   │   └── Factory.h                  # Factory pattern
│   ├── Observer/
│   │   └── Observer.h                 # Observer/Subject
│   ├── Functor/
│   │   └── Functor.h                  # Function objects
│   └── Components/
│       ├── IComponent.h               # Component interface
│       └── ComponentFactoryRegistry.h # Component factory registry
├── Type/                              # Type system
│   ├── TypeDefinition.h               # Type metadata
│   └── TypeRegistry.h                 # Type registry (singleton)
├── CRC/
│   └── CRC.h                          # StringCRC implementation
├── Time/
│   ├── TimeServer.h                   # Global time source (singleton)
│   └── TimeAbsolute.h                 # Absolute time
├── Timer/
│   └── TimeRelative.h                 # Relative time, timers
├── Core/
│   ├── Assert.h                       # Assertions (DIA_ASSERT)
│   ├── Log.h                          # Logging
│   └── CallStack.h                    # Call stack capture
├── Memory/
│   └── Memory.h                       # Memory utilities
├── FilePath/
│   └── FilePath.h                     # File path handling
├── Json/
│   └── JsonWrapper.h                  # jsoncpp wrapper
└── Deprecated/                        # Old code (not compiled)
```

**Key Classes:**
- `DynamicArray<T>` - Primary container (most used)
- `HashTable<K,V>` - Hash map (StringCRC keys)
- `StringCRC` - Compile-time string hashing
- `TypeRegistry` - Type reflection system
- `Singleton<T>` - Singleton pattern
- `Observer`/`ObserverSubject` - Event notifications

**When to Use:**
- Need a container → Use `DynamicArray` or `HashTable`
- Need string comparison → Use `StringCRC`
- Need type ID → Use `StringCRC` or `TypeDefinition`
- Need singleton → Inherit from `Singleton<T>`
- Need observer pattern → Inherit from `Observer`/`ObserverSubject`

**Namespaces:** `Dia::Core::`

---

### DiaMaths/ - Math Library

**Location:** `Dia/DiaMaths/`

**Purpose:** Vector, matrix, transform, shape math

**Structure:**
```
DiaMaths/
├── Vector/
│   ├── Vector2D.h                     # 2D vector
│   ├── Vector3D.h                     # 3D vector
│   └── Vector4D.h                     # 4D vector
├── Matrix/
│   ├── Matrix22.h                     # 2x2 matrix
│   ├── Matrix33.h                     # 3x3 matrix (2D transforms)
│   └── Matrix44.h                     # 4x4 matrix (3D transforms)
├── Transform/
│   ├── Transform2D.h                  # 2D transform with hierarchy
│   └── Transform3D.h                  # 3D transform
├── Shape/
│   ├── Circle.h                       # Circle shape
│   ├── AABB.h                         # Axis-aligned bounding box
│   ├── Line.h                         # Line segment
│   └── Polygon.h                      # Polygon
├── Core/
│   ├── Random.h                       # Random number generation (thread-safe)
│   ├── FloatMaths.h                   # Float utilities
│   └── Interpolation.h                # Lerp, InverseLerp, etc.
└── dia.maths.architecture.module.md
```

**Key Classes:**
- `Vector2D`/`Vector3D` - Vectors with operators (+, -, *, /)
- `Matrix33`/`Matrix44` - Matrices for transformations
- `Transform2D` - 2D transform with parent/child hierarchy
- `Circle`, `AABB`, `Line` - Collision shapes
- `Random` - Thread-safe random number generator

**Known Issues:**
- Template specialization missing for `InverseLerp(Vector2D)`
- `MoveTowards(Vector2D)` returns wrong type
- `Transform2D` hierarchy traversal performance (no caching)

**When to Use:**
- Vector math operations
- Matrix transformations
- Transform hierarchies (parent/child)
- Collision detection
- Random numbers (thread-safe)

**Namespaces:** `Dia::Maths::`

**[→ Known Issues](../subsystems/dia-maths/known-issues.md)**

---

### DiaGraphics/ - Graphics Abstraction

**Location:** `Dia/DiaGraphics/`

**Purpose:** Platform-agnostic rendering interface

**Structure:**
```
DiaGraphics/
├── Interface/
│   ├── ICanvas.h                      # Canvas interface
│   └── Frame.h                        # Frame data
└── dia.graphics.architecture.module.md
```

**Key Classes:**
- `ICanvas` - Abstract rendering interface (implemented by DiaSFML)
- `Frame` - Frame data (color, transform)

**When to Use:**
- Rendering shapes (lines, circles, sprites)
- Platform-agnostic graphics code

**Implementation:** `DiaSFML/DiaSFMLRenderWindow.h`

**Namespaces:** `Dia::Graphics::`

---

### DiaInput/ - Input Handling

**Location:** `Dia/DiaInput/`

**Purpose:** Input event abstraction

**Structure:**
```
DiaInput/
├── DiaInputEvent.h                    # Input events
├── DiaInputInputSourceManager.h       # Input source management
└── dia.input.architecture.module.md
```

**Key Classes:**
- `InputEvent` - Keyboard, mouse, gamepad events
- `InputSourceManager` - Manages input sources

**When to Use:**
- Handling keyboard/mouse/gamepad input
- Platform-agnostic input code

**Implementation:** `DiaSFML/DiaSFMLInputSource.h`

**Namespaces:** `Dia::Input::`

---

### DiaUI/ - UI Abstraction

**Location:** `Dia/DiaUI/`

**Purpose:** UI system interface

**Structure:**
```
DiaUI/
├── DiaUIIUISystem.h                   # UI system interface
└── dia.ui.architecture.module.md
```

**Key Classes:**
- `IUISystem` - Abstract UI interface

**Namespaces:** `Dia::UI::`

---

### DiaSFML/ - SFML Backend

**Location:** `Dia/DiaSFML/`

**Purpose:** SFML integration for graphics/audio/input

**Structure:**
```
DiaSFML/
├── DiaSFMLRenderWindow.h              # SFML window + canvas
├── DiaSFMLInputSource.h               # SFML input handling
└── dia.sfml.architecture.module.md
```

**Key Classes:**
- `DiaSFMLRenderWindow` - Implements `ICanvas` using SFML
- `DiaSFMLInputSource` - Implements input via SFML events

**When to Use:**
- Creating a window
- Rendering graphics
- Capturing input

**Namespaces:** `Dia::SFML::`

---

### Other Subsystems (Stubs)

**DiaWindow/** - Window management interface
**DiaIO/** - File I/O utilities
**DiaPhysics/** - Physics simulation (stub)
**DiaAI/** - AI pathfinding (stub)

---

## External/ - Third-Party Dependencies

**Location:** `External/`

**Purpose:** Third-party libraries and SDKs

**Structure:**
```
External/
├── SFML-2.5.1/                        # SFML graphics library
├── jsoncpp-master/                    # JSON parsing
├── Webix/                             # Web UI components
└── VisJS/                             # Visualization library
```

**When to Modify:**
- Upgrading dependencies
- Adding new libraries

**[→ External Dependencies Details](../architecture/external-dependencies.md)**

---

## Tools/ - Development Tools

**Location:** `Tools/`

**Purpose:** Build scripts and analysis tools

**Structure:**
```
Tools/
├── CLI/                               # MDK CLI (Python)
├── Console/                           # Blue Console (TypeScript)
│   ├── api/
│   ├── app/
│   ├── core/
│   └── server/
└── dia_modules.py                     # Module dependency analyzer
```

**Key Tools:**
- `dia_modules.py` - Analyzes module dependencies, generates graphs
- `CLI/` - Command-line tools (see `Tools/CLI/README.md`)
- `Console/` - Web-based console for debugging

**When to Use:**
- Analyzing module dependencies
- Validating architecture files
- Generating dependency graphs

---

## Module Architecture Files

**Pattern:** `dia.<parent>.<module>.architecture.module.md`

**Location:** Co-located with code (e.g., `Dia/DiaCore/Containers/Arrays/dia.core.containers.arrays.architecture.module.md`)

**Purpose:** Machine-readable module metadata (YAML frontmatter)

**Schema:** `dia.module.v1`

**Key Fields:**
```yaml
module_id: dia.parent.module
name: ModuleName
path: Dia/DiaParent/DiaModule
parent_module_id: dia.parent
responsibilities: [...]
dependent_modules: [...]
public_api:
  headers: [...]
  namespaces: [...]
dependencies:
  required: [...]
```

**Count:** 56 module architecture files

**[→ Module Metadata Schema](../registry/module-metadata-schema.md)**  
**[→ Module Registry](../registry/module-registry.md)**

---

## File Naming Conventions

### Headers and Source

**Pattern:** `<ClassName>.h` and `<ClassName>.cpp`

**Examples:**
- `Vector2D.h` / `Vector2D.cpp`
- `ProcessingUnit.h` / `ProcessingUnit.cpp`
- `DynamicArray.h` (header-only)

### Module Prefixes

**Pattern:** `Dia<Subsystem><Class>`

**Examples:**
- `DiaCore/Containers/Arrays/DynamicArray.h`
- `DiaMaths/Vector/Vector2D.h`
- `DiaSFML/DiaSFMLRenderWindow.h`

### Namespaces

**Pattern:** `Dia::<Subsystem>::`

**Examples:**
- `Dia::Core::DynamicArray`
- `Dia::Maths::Vector2D`
- `Dia::Application::ProcessingUnit`

---

## Build Files

### Visual Studio Projects

**Pattern:** `<ProjectName>.vcxproj` and `<ProjectName>.vcxproj.filters`

**Key Files:**
- `Cluiche/Cluiche.sln` - Main solution
- `Cluiche/CluicheTest/Cluiche.vcxproj` - Main executable project
- `Dia/DiaCore/DiaCore.vcxproj` - DiaCore library project
- `Dia/DiaMaths/DiaMaths.vcxproj` - DiaMaths library project

**When to Modify:**
- Adding new source files
- Changing include paths
- Updating dependencies

**[→ Visual Studio Guide](../development/visual-studio-guide.md)**

---

## Summary

**Quick Reference:**

| Task | Start Here |
|------|------------|
| **Understand architecture** | `docs/reference/architecture/architecture.md` |
| **Add a module** | `Dia/Dia<Subsystem>/` + module architecture file |
| **Add a level** | `Cluiche/Levels/` + register in `LevelFactoryModule` |
| **Modify threading** | `Cluiche/ApplicationFlow/ProcessingUnits/` |
| **Fix math bug** | `Dia/DiaMaths/` |
| **Update UI** | `Cluiche/CluicheKernel/ApplicationFlow/Modules/MainUIModule.h` |
| **Add tests** | `Cluiche/Levels/UnitTestLevel/` (in-engine) or `Cluiche/Tests/` (future) |
| **Analyze dependencies** | `python Tools/dia_modules.py` |

**Entry Points:**
- `Cluiche/CluicheTest/Main.cpp` - Application entry
- `Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.h` - Threading orchestrator
- `Dia/DiaApplication/` - Framework base classes
- `Dia/DiaCore/` - Foundation library

**[→ Entry Points for Common Tasks](entry-points.md)**  
**[→ Code Patterns Reference](patterns-reference.md)**  
**[→ System Boundaries](system-boundaries.md)**

**[→ Back to AI Guide](AI-README.md)**

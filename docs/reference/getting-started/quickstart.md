# 5-Minute Quickstart

Get oriented to the Cluiche codebase in 5 minutes.

---

## What is Cluiche?

**Cluiche** is a game development platform with multiple applications:

- **Platform: Cluiche** - Overall game development platform
- **Application: Dia** - The game engine (DiaCore, DiaMaths, DiaGraphics, DiaAPI, etc.) providing 13+ subsystems
- **Application: Cluiche (Game)** - Demo game showcasing the Dia engine with three independent threads:
  - **Main Thread** - Bootstraps the app, coordinates UI
  - **Render Thread** - Renders graphics at 60 FPS
  - **Sim Thread** - Runs game simulation and logic
- **Application: GoogleTest** - Unit testing suite
- **Future Applications** - Your game projects built on Dia

---

## Three Key Concepts

### 1. Module/Phase/ProcessingUnit Pattern

**ProcessingUnit** = A thread of execution
- Example: `MainProcessingUnit`, `RenderProcessingUnit`, `SimProcessingUnit`

**Phase** = A stage of execution within a thread
- Example: `BootPhase` → `BootStrapPhase` → `RunningPhase`

**Module** = A reusable component with dependencies
- Example: `MainKernelModule` (time, input, window), `MainUIModule` (Awesomium UI)

```
ProcessingUnit runs Phases
    ↓
Phases contain Modules
    ↓
Modules provide functionality
```

### 2. Three-Thread Architecture

```
Main Thread (MainProcessingUnit)
  │
  ├─→ Spawns: Render Thread (RenderProcessingUnit)
  │              └─→ Reads sim state, renders graphics
  │
  └─→ Spawns: Sim Thread (SimProcessingUnit)
                 └─→ Updates game logic, writes sim state
```

**Synchronization:** `std::mutex` protects shared state between threads.

### 3. Level System

**Levels** = Pluggable game states loaded via `LevelFactory`

- `DummyLevel` - Example level implementation
- `UnitTestLevel` - In-engine test harness
- Custom levels registered at startup

---

## Directory Structure (60 seconds)

```
C:\GitHub\Cluiche\
│
├── README.md                     ← You are here
├── docs/                         ← All documentation
│
├── Cluiche/                      ← CLUICHE GAME APPLICATION
│   ├── Cluiche.sln              ← Visual Studio solution
│   ├── Cluiche/                 ← Main game application project
│   │   ├── Main.cpp             ← Entry point (starts MainProcessingUnit)
│   │   ├── ApplicationFlow/     ← ProcessingUnits and Phases
│   │   ├── CluicheKernel/       ← Core modules (MainKernelModule, etc.)
│   │   └── Levels/              ← Level implementations
│   └── Tests/                   ← Test projects
│
├── Dia/                          ← DIA ENGINE APPLICATION (13+ subsystems)
│   ├── DiaApplication/          ← Module/Phase/ProcessingUnit framework
│   ├── DiaCore/                 ← Containers, Type system, Time
│   ├── DiaMaths/                ← Vector, Matrix, Transform, Shape
│   ├── DiaGraphics/             ← ICanvas rendering abstraction
│   ├── DiaInput/                ← Input events
│   ├── DiaUI/                   ← UI integration
│   ├── DiaSFML/                 ← SFML backend (graphics)
│   └── [6 more subsystems]      ← Physics, AI, IO, Window, etc.
│
├── External/                     ← THIRD-PARTY LIBRARIES
│   ├── SFML/                    ← Graphics/Audio/Input library
│   ├── jsoncpp-master/          ← JSON configuration
│   ├── Awesomium SDK/           ← Web-based UI
│   └── [2 more libraries]
│
└── Tools/                        ← DEVELOPMENT TOOLS
    ├── CLI/                     ← Python CLI (MDK)
    └── Console/                 ← TypeScript web console
```

---

## Code Flow (90 seconds)

### Entry Point: `Cluiche/CluicheTest/Main.cpp`

```cpp
int main() {
    MainProcessingUnit mainPU;  // Create main thread PU
    mainPU.Start();             // Initialize (boots phases)
    mainPU.Update();            // Main loop
    mainPU.Stop();              // Shutdown
}
```

### Initialization Flow

1. **MainProcessingUnit::Start()**
   - Starts `MainBootPhase`
     - Registers modules: `MainKernelModule`, `LevelFactoryModule`
     - Registers levels: `DummyLevel`, `UnitTestLevel`
   - Transitions to `MainBootStrapPhase`
     - Spawns `RenderProcessingUnit` thread
     - Spawns `SimProcessingUnit` thread
     - Shows launch UI

2. **RenderProcessingUnit (separate thread)**
   - Starts `RenderRunningPhase`
   - Reads graphics commands from `SimToRenderFrameStream`
   - Renders to SFML canvas at 60 FPS

3. **SimProcessingUnit (separate thread)**
   - Starts `SimBootPhase` → `SimBootStrapPhase`
   - Reads input from `InputToSimFrameStream`
   - Updates game logic
   - Writes graphics commands to `SimToRenderFrameStream`

### Thread Communication

**Main → Sim:** Input events via `InputToSimFrameStream`  
**Sim → Render:** Graphics commands via `SimToRenderFrameStream`  
**Main ↔ Sim:** UI updates via `SimUIProxyModule` (observer pattern with mutex)

---

## Key Files (2 minutes)

### Want to understand the architecture?

**Core Entry Points:**
- `Cluiche/CluicheTest/Main.cpp` - Application entry point
- `Cluiche/CluicheTest/ApplicationFlow/ProcessingUnits/MainProcessingUnit.h` - Main thread orchestrator
- `Dia/DiaApplication/ApplicationProcessingUnit.h` - ProcessingUnit base class
- `Dia/DiaApplication/ApplicationModule.h` - Module base class

**Core Modules:**
- `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h` - Time, input, window, canvas
- `Cluiche/CluicheTest/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h` - Render thread
- `Cluiche/CluicheTest/ApplicationFlow/ProcessingUnits/SimProcessingUnit.h` - Sim thread

**Level System:**
- `Cluiche/CluicheTest/CluicheKernel/LevelFactory.h` - Level registry and factory
- `Cluiche/CluicheTest/Levels/DummyLevel/DummyLevel.h` - Example level

### Want to understand Dia engine subsystems?

**Read the module architecture files:**
- `Dia/dia.root.architecture.module.md` - Root module overview
- `Dia/DiaApplication/Docs/dia.application.architecture.module.md` - Framework details
- `Dia/DiaCore/Docs/dia.core.architecture.module.md` - Core utilities
- [53 more .architecture.module.md files throughout Dia/]

**Module files use YAML frontmatter:**
```yaml
---
schema: dia.module.v1
module_id: unique.module.id
name: Human-Readable Name
dependencies:
  required:
    - other.module.id
public_api:
  headers:
    - Path/To/Header.h
---
```

See [Module Metadata Schema](../registry/module-metadata-schema.md) for details.

---

## Building the Project (30 seconds)

**Requirements:**
- Windows 10+
- Visual Studio 2015 or later
- No CMake (uses .sln/.vcxproj files)

**Steps:**
1. Open `Cluiche/Cluiche.sln` in Visual Studio
2. Set build configuration (Debug/Release)
3. Build Solution (Ctrl+Shift+B)
4. Run (F5)

See [Building the Project](building-the-project.md) for detailed instructions.

---

## Common Tasks (30 seconds)

### Add a new module
1. Create `.h/.cpp` files inheriting from `Dia::Application::Module`
2. Implement `DoStart()`, `DoUpdate()`, `DoStop()`
3. Declare dependencies in constructor
4. Register in appropriate `Phase`

### Add a new level
1. Create level folder in `Cluiche/Levels/<LevelName>/`
2. Implement `ILevel` interface
3. Register in `LevelFactory` during `MainBootPhase`

### Debug threading issues
- Check `std::mutex` usage around shared state
- Look for race conditions in observers
- See [Thread Safety Guide](../ai-guides/thread-safety-guide.md)

See [Common Tasks](common-tasks.md) for step-by-step guides.

---

## Where to Go Next

### For Humans:
- **[Architecture Overview](../architecture/architecture.md)** - Understand the full system
- **[Design Philosophy](../design-rationale/design.md)** - Understand why decisions were made
- **[Building the Project](building-the-project.md)** - Detailed build instructions

### For AI Agents:
- **[AI-README](../ai-guides/AI-README.md)** - AI-optimized entry point
- **[Codebase Map](../ai-guides/codebase-map.md)** - Structured navigation
- **[Entry Points](../ai-guides/entry-points.md)** - Task-based starting points

### Explore:
- [Full Documentation Index](../README.md) - All documentation
- [API Documentation](../api/api-overview.md) - Public interface reference
- [Module Registry](../registry/module-registry.md) - All 56 modules cataloged

---

## Quick Reference

**Namespace Convention:** `Dia::<Subsystem>::<Class>`  
**Example:** `Dia::Application::Module`, `Dia::Core::DynamicArray<T>`, `Dia::Maths::Vector2D`

**File Organization:**
- Headers: `Dia<Subsystem>/<Subsystem><Class>.h`
- Implementation: `Dia<Subsystem>/<Subsystem><Class>.cpp`
- Inline/Templates: `Dia<Subsystem>/<Subsystem><Class>.inl`

**Git Branch:** Development (4 commits ahead of master)

---

You're now oriented! Dive deeper with the [full documentation](../README.md).
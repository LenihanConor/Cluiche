# AI Agent Guide to Cluiche Codebase

**Project:** Cluiche game framework built on Dia engine  
**Language:** C++11  
**Build System:** Visual Studio 2015+ (Windows)  
**Threading:** 3 threads (Main, Render, Sim)  
**Architecture:** Module/Phase/ProcessingUnit pattern  
**Repository:** C:\GitHub\Cluiche

---

## Quick Orientation

This is a **multi-threaded game framework** with explicit thread boundaries and a pluggable module system. The codebase is organized into three layers:

1. **Cluiche (Application)** - Game framework using Dia (Main/Render/Sim threads)
2. **Dia (Engine)** - 13 subsystems providing platform abstraction
3. **External** - Third-party libraries (SFML, JsonCpp, etc.)

**Entry Point:** `Cluiche/CluicheTest/Main.cpp` → `MainProcessingUnit::Start()` → spawns Render + Sim threads

---

## For AI: How to Navigate This Codebase

### 1. Start Here for Common Tasks

**[→ Entry Points Guide](entry-points.md)** - Where to begin for specific tasks:

- Adding a new module → Read `ApplicationModule.h`, register in Phase
- Creating a new level → Implement `ILevel`, register in `LevelFactory`
- Extending the UI → Add page to `DiaUI`, bind methods via `BoundMethod`
- Adding a math function → Add to `DiaMaths`, follow existing patterns
- Implementing input handling → Add event type to `DiaInput`, wire to `InputSourceManager`
- Debugging thread issues → Check mutex usage, see [thread-safety-guide.md](thread-safety-guide.md)

### 2. System Boundaries

**[→ System Boundaries Matrix](system-boundaries.md)** - What each subsystem owns/doesn't own:

Quick summary:

| Subsystem | Responsibility | Does NOT Handle |
|-----------|---------------|-----------------|
| DiaApplicationFlow | Thread lifecycle, module orchestration | Game logic, rendering |
| DiaCore | Containers, type system, time | Threading, graphics |
| DiaGraphics | Rendering abstraction (ICanvas) | Window management, backend |
| DiaMaths | Vector/matrix/shape math | Physics simulation |
| DiaInput | Input events, device management | Event processing logic |
| Cluiche | Game framework, level system | Low-level engine services |

### 3. Code Patterns

**[→ Patterns Reference](patterns-reference.md)** - Common patterns with examples:

- **Module Registration** - How to create and register modules
- **Phase Lifecycle** - Boot → BootStrap → Running → Stop
- **Observer Pattern** - UIModule → UIProxyModule notifications
- **Factory Pattern** - LevelFactory creates levels by type
- **Type System** - StringCRC for compile-time type IDs

### 4. Codebase Map

**[→ Codebase Map](codebase-map.md)** - Structured directory navigation:

- Directory structure with purposes
- Key files and their roles
- Where to find specific functionality

### 5. Quick Reference

**[→ Quick Reference](quick-reference.md)** - Fast lookups:

- Namespace conventions: `Dia::<Subsystem>::<Class>`
- Naming patterns: `Dia<Subsystem>/<Subsystem><Class>.h`
- File organization rules
- Common macros and utilities

---

## Key Concepts for AI Agents

### Module/Phase/ProcessingUnit Pattern

**ProcessingUnit:** Thread of execution
- Owns phases and orchestrates lifecycle
- Can run on separate thread via `operator()()`
- Thread-safe phase transitions (queued with mutex)
- Frequency control via `TimeThreadLimiter`

**Phase:** Execution stage within a thread
- Contains modules
- Lifecycle: Start → Update (loop) → Stop
- Can transition to other phases
- Example: `BootPhase` → `BootStrapPhase` → `RunningPhase`

**Module:** Functional unit within a phase
- Has explicit dependencies on other modules
- Lifecycle: Build Dependencies → Start → Update → Stop
- Can be "retained" through phase transitions

**Code Example:**
```cpp
// Dia/DiaApplicationFlow/ApplicationProcessingUnit.h
class ProcessingUnit {
    void Start();   // Initialize first phase
    void Update();  // Run current phase update loop
    void Stop();    // Shutdown
    void TransitionPhase(Phase* newPhase);  // Queued, thread-safe
};

// Dia/DiaApplicationFlow/ApplicationModule.h
class Module {
    virtual void DoStart() = 0;
    virtual void DoUpdate() = 0;
    virtual void DoStop() = 0;
    void RegisterDependency(Module* dependency);
};
```

### Threading Model

**Three Independent Threads:**

```
Main Thread (MainProcessingUnit)
    ├─ Phases: MainBootPhase, MainBootStrapPhase
    ├─ Modules: MainKernelModule, MainUIModule, LevelFactoryModule
    ├─ Responsibilities: Bootstrap, UI coordination, input collection
    ├─ Spawns: RenderProcessingUnit (std::thread)
    └─ Spawns: SimProcessingUnit (std::thread)

Render Thread (RenderProcessingUnit)
    ├─ Phases: RenderRunningPhase
    ├─ Modules: RenderKernel, RenderCanvas
    ├─ Responsibilities: Graphics rendering, display @ 60 FPS
    └─ Reads: SimToRenderFrameStream<FrameData> (graphics commands)

Sim Thread (SimProcessingUnit)
    ├─ Phases: SimBootPhase, SimBootStrapPhase
    ├─ Modules: SimTimeServerModule, SimInputFrameStreamModule, SimUIProxyModule
    ├─ Responsibilities: Game simulation, physics, logic
    ├─ Reads: InputToSimFrameStream<EventData> (input events)
    └─ Writes: SimToRenderFrameStream<FrameData> (graphics commands)
```

**Synchronization:**
- `std::mutex` for phase transitions (queued via `mQueuedTransitionMutex`)
- `std::mutex` in `ObserverSubject` for notifications
- `std::mutex` in `SimUIProxyModule` for UI access
- No lock-free structures (uses standard mutexes)

**Thread Safety Critical Zones:**
- Observer pattern notifications
- State object access
- Transform hierarchy queries (known issue: multiple traversals not thread-safe)
- Random number generation (recently fixed to be thread-safe)

### Type System

**StringCRC:** Compile-time string hashing for type IDs

```cpp
// Usage in code:
static const Dia::Core::StringCRC kTypeId("MyClass");

// Benefits:
// - Compile-time hash generation (no runtime cost)
// - Fast O(1) type comparison
// - Serialization support
// - Used in TypeRegistry for reflection
```

**Type Registry:**
```cpp
Dia::Core::TypeRegistry::GetInstance()->RegisterType(typeDefinition);
Dia::Core::TypeRegistry::GetInstance()->FindType(crc);
```

**Use Cases:**
- Component type identification
- Factory pattern key lookup
- Serialization/deserialization (JSON, text)
- Module dependency resolution

### Custom Containers (NOT STL)

Dia provides custom containers instead of STL:

| Dia Container | STL Equivalent | Notes |
|---------------|----------------|-------|
| `Array<T, N>` | `std::array<T, N>` | Fixed-size array |
| `DynamicArray<T>` | `std::vector<T>` | Dynamic array with capacity control |
| `DynamicArrayC<T, Capacity>` | `std::vector<T>` | Dynamic array with compile-time capacity hint |
| `HashTable<K, V>` | `std::unordered_map<K, V>` | Hash map with CRC keys |
| `Graph<N, V, E, U>` | (none) | Graph data structure |
| `LinkList<T>` | `std::list<T>` | Doubly-linked list |

**Why Custom Containers?**
- Explicit memory control (no hidden allocations)
- Platform portability (no STL dependency)
- Performance tuning (known memory layout)
- Debugging visibility (custom assertions)

**Namespace:** `Dia::Core::Containers::*`

---

## File Organization Patterns

### Dia Engine

```
Dia/
├── Dia<Subsystem>/                      # Subsystem directory
│   ├── Docs/                            # Architecture documentation
│   │   └── dia.<subsystem>.architecture.module.md
│   ├── <Subsystem><Class>.h             # Public API header
│   ├── <Subsystem><Class>.cpp           # Implementation
│   ├── <Subsystem><Class>.inl           # Inline/template impl
│   ├── <Subsystem>.vcxproj              # Visual Studio project
│   └── <Subsystem>.vcxproj.filters      # Visual Studio filters
│
└── dia.root.architecture.module.md      # Root module definition
```

**Example:** `Dia/DiaCore/DiaCoreDynamicArray.h` defines `Dia::Core::Containers::DynamicArray<T>`

### CluicheTest Application

```
Cluiche/CluicheTest/
├── Main.cpp                             # Entry point
├── ApplicationFlow/
│   ├── ProcessingUnits/                 # Thread orchestrators
│   │   ├── MainProcessingUnit.h/cpp
│   │   ├── RenderProcessingUnit.h/cpp
│   │   └── SimProcessingUnit.h/cpp
│   └── Phases/                          # Phase implementations
│       ├── MainBootPhase.h/cpp
│       ├── MainBootStrapPhase.h/cpp
│       ├── RenderRunningPhase.h/cpp
│       ├── SimBootPhase.h/cpp
│       └── SimBootStrapPhase.h/cpp
├── CluicheKernel/
│   ├── LevelFactory.h/cpp               # Level registry
│   └── ApplicationFlow/Modules/         # Core modules
│       ├── MainKernelModule.h/cpp
│       ├── MainUIModule.h/cpp
│       ├── LevelFactoryModule.h/cpp
│       ├── SimTimeServerModule.h/cpp
│       ├── SimInputFrameStreamModule.h/cpp
│       └── SimUIProxyModule.h/cpp
└── Levels/
    └── <LevelName>/                     # Level implementation
        ├── <LevelName>.h/cpp            # Level class
        ├── LevelFlow/Phases/            # Level-specific phases
        └── UI/                          # Level-specific UI pages
```

### External Dependencies

```
External/
├── SFML/                                # Graphics/Audio/Input library
├── jsoncpp-master/                      # JSON configuration
├── Webix/                               # UI framework (multiple versions)
└── VisJS/                               # Visualization library
```

---

## Parsing Module Architecture Files

**56 `.architecture.module.md` files** throughout Dia subsystems use YAML frontmatter:

**Schema:** `dia.module.v1`

**Example:**
```yaml
---
schema: dia.module.v1
module_id: dia.core.containers.arrays
parent_module_id: dia.core.containers
name: Arrays
path: Dia/DiaCore/Containers/Arrays
language: cpp
status: active
maturity: stable
layer: foundation

summary: Fixed and dynamic array containers

intent: >
  Provide memory-controlled array containers as alternatives to STL.

responsibilities:
  - Fixed-size Array<T, N>
  - Dynamic DynamicArray<T>
  - Dynamic with capacity hint DynamicArrayC<T, Capacity>

non_responsibilities:
  - Thread safety (caller's responsibility)
  - Iterator invalidation protection

dependent_modules:
  - dia.application.processing_unit

public_api:
  headers:
    - Dia/DiaCore/Containers/Arrays/DiaCoreDynamicArray.h
    - Dia/DiaCore/Containers/Arrays/DiaCoreArray.h
  entry_points:
    - Dia::Core::Containers::DynamicArray<T>
    - Dia::Core::Containers::Array<T, N>

dependencies:
  required:
    - dia.core.core
  optional: []
---

[Markdown body with detailed documentation]
```

**Key Fields for AI:**
- `module_id` - Unique hierarchical identifier (e.g., `dia.core.containers.arrays`)
- `parent_module_id` - Parent in hierarchy (e.g., `dia.core.containers`)
- `path` - Relative path from repository root
- `public_api.headers` - What headers to include
- `public_api.entry_points` - Key classes/functions
- `dependencies.required` - Modules this depends on
- `responsibilities` - What it does
- `non_responsibilities` - What it doesn't do

**Complete Module Registry:** See [module-registry.md](../registry/module-registry.md) for all 56 modules.

---

## Common AI Tasks

### Task: Add a New DiaCore Container

1. **Read codebase map** → [codebase-map.md](codebase-map.md) → Find `Dia/DiaCore/Containers/`
2. **Check existing pattern** → Read `Dia/DiaCore/Containers/Arrays/DiaCoreDynamicArray.h`
3. **Create new container:**
   ```cpp
   // Dia/DiaCore/Containers/MyContainer/DiaCoreMyContainer.h
   namespace Dia { namespace Core { namespace Containers {
       template<typename T>
       class MyContainer {
           // Implementation following existing patterns
       };
   }}}
   ```
4. **Update module file** → Add to `Dia/DiaCore/Containers/dia.core.containers.architecture.module.md`
5. **Add to Visual Studio project** → See [visual-studio-guide.md](../development/visual-studio-guide.md)

### Task: Add a New Cluiche Module

1. **Read patterns** → [patterns-reference.md](patterns-reference.md) → Module pattern
2. **Check existing** → `Cluiche/CluicheTest/CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h`
3. **Implement `Dia::Application::Module`:**
   ```cpp
   class MyModule : public Dia::Application::Module {
   public:
       MyModule();
       ~MyModule();
   private:
       void DoStart() override;
       void DoUpdate() override;
       void DoStop() override;
   };
   ```
4. **Register dependencies** in constructor:
   ```cpp
   MyModule::MyModule() {
       RegisterDependency(&GetDependency<OtherModule>());
   }
   ```
5. **Register in Phase:**
   ```cpp
   // In appropriate Phase's constructor or Start()
   AddModule(&myModuleInstance);
   ```

### Task: Debug Threading Issue

1. **Read thread safety guide** → [thread-safety-guide.md](thread-safety-guide.md)
2. **Check mutex usage:**
   - Phase transitions: `ProcessingUnit::mQueuedTransitionMutex`
   - Observer notifications: `ObserverSubject` has internal mutex
   - Shared state: Look for `std::mutex` members
3. **Check known issues** → [known-issues.md](../development/known-issues.md)
   - Transform2D multiple traversals
   - Random number generation (recently fixed)
4. **Add thread safety test** → See [thread-safety-testing.md](../testing/thread-safety-testing.md)

### Task: Fix a Bug in DiaMaths

1. **Read known issues** → [known-issues.md](../subsystems/dia-maths/known-issues.md)
   - Template specialization bugs (InverseLerp, MoveTowards)
   - Dead code in IntersectionTests
2. **Locate source** → `Dia/DiaMaths/Core/` or `Dia/DiaMaths/Shape/`
3. **Check for template issues:**
   - Missing specializations for Vector2D/Vector3D
   - Incorrect return types in templates
4. **Add unit test** → `Tests/DiaMaths/<TestFile>.cpp`
5. **Update documentation** → Add to [changelog.md](../development/changelog.md)

---

## AI-Friendly File Formats

### Structured Data

- **`.architecture.module.md`** - YAML frontmatter (56 files) → parseable with YAML parser
- **`.json`** - Configuration files (pathStoreConfig.json)
- **`.vcxproj`** / **`.vcxproj.filters`** - XML project files → parseable but complex

### Documentation

- **Markdown** - Consistent structure with headers, code blocks, tables
- **Mermaid diagrams** - `.mmd` files in `docs/reference/architecture/diagrams/` → parseable as text
- **Code examples** - Fenced with language identifiers (\`\`\`cpp)

### Code

- **Namespace structure:** `Dia::<Subsystem>::<Class>`
- **Header guards:** `#pragma once` (not traditional `#ifndef`)
- **Include paths:** Relative to project root (e.g., `#include "Dia/DiaCore/DiaCoreDynamicArray.h"`)

---

## Dependency Graph Summary

### High-Level Dependencies

```
CluicheTest Application
    ↓ depends on
Dia Engine
    ↓ depends on
External Libraries (SFML, JsonCpp, etc.)
    ↓ depends on
C++ Standard Library (std::chrono, std::mutex, std::thread)
```

### Within Dia Engine

```
DiaApplicationFlow (Module/Phase/PU framework)
    ↓ depends on
DiaCore (Containers, Type, Time, Memory)

DiaGraphics (ICanvas abstraction)
    ↓ depends on
DiaSFML (SFML backend)
    ↓ depends on
External/SFML

DiaMaths ← (independent, minimal dependencies)
    └─ depends on → DiaCore (minimal: assertions, functors)
```

**Complete Dependency Graph:** See [dependency-graph.md](dependency-graph.md)

---

## Known Issues for AI Awareness

**Thread Safety:**
- Transform2D hierarchy traversals NOT thread-safe (multiple traversals issue)
- Random number generation recently fixed (was NOT thread-safe before 2026-03)

**DiaMaths Bugs:**
- Template specialization issues in Interpolation.h (InverseLerp, MoveTowards for vectors)
- Dead code in IntersectionTests.h (unreachable branches)

**Test Coverage:**
- Overall coverage <30%
- DiaApplicationFlow <20%
- DiaCore ~30-40%
- DiaMaths ~40%

**Deprecated Code:**
- `Dia/DiaCore/Deprecated/CollectionShit/` - Unused utility classes (being removed)
- `Dia/DiaCore/Deprecated/LinkLists/DynamicLinkList` - Unused (being removed)

**Detailed Issues:** See [known-issues.md](../development/known-issues.md)

---

## Documentation Conventions

- **Bold** for emphasis
- `Code font` for identifiers (classes, functions, files)
- [Links] for cross-references
- Status indicators: ✅ ❌ 🚧 ⚠️
- Priority: P0 (Critical) | P1 (High) | P2 (Medium) | P3 (Low)

---

## Getting Help

### For Humans

- [Human documentation index](../README.md)
- [Architecture overview](../architecture/architecture.md)
- [Design philosophy](../design-rationale/design.md)

### For AI

- [Codebase map](codebase-map.md) - Structured navigation
- [Entry points](entry-points.md) - Task-based starting points
- [Patterns reference](patterns-reference.md) - Code patterns
- [Quick reference](quick-reference.md) - Fast lookups

### Module Details

- [Module registry](../registry/module-registry.md) - All 56 modules
- Module architecture files: `Dia/**/Docs/*.architecture.module.md`

---

## Quick Lookup Tables

### Namespace Conventions

```
Dia::<Subsystem>::<Class>
```

**Examples:**
- `Dia::Application::Module`
- `Dia::Core::Containers::DynamicArray<T>`
- `Dia::Maths::Vector2D`
- `Dia::Graphics::ICanvas`
- `Dia::Input::Event`

### File Naming Patterns

```
Dia<Subsystem>/<Subsystem><Class>.h      # Header
Dia<Subsystem>/<Subsystem><Class>.cpp    # Implementation
Dia<Subsystem>/<Subsystem><Class>.inl    # Inline/Template
```

**Examples:**
- `Dia/DiaCore/DiaCoreDynamicArray.h`
- `Dia/DiaMaths/DiaMathsVector2D.h`
- `Dia/DiaGraphics/DiaGraphicsICanvas.h`

### Common Macros

```cpp
DIA_ASSERT(condition)               # Debug assertion
DIA_STATIC_ASSERT(condition)        # Compile-time assertion
DIA_DEFINE_COMPONENT_TYPE_ID(name)  # Type ID generation
DIA_FUNCTOR(name)                   # Functor helper
```

---

## Meta: About This Documentation

**Generated:** 2026-03-31  
**For:** AI agents and human developers  
**Coverage:** Cluiche app + Dia engine + External tools (lightweight)  
**Status:** Living document (Phase 1 of 10 complete)  
**Repository:** C:\GitHub\Cluiche

**Progress Tracking:** See [DOCUMENTATION_TODO.md](../../DOCUMENTATION_TODO.md)

---

## Next Steps for AI Agents

After reading this guide:

1. **Understand the structure** → Read [codebase-map.md](codebase-map.md)
2. **Learn the patterns** → Read [patterns-reference.md](patterns-reference.md)
3. **Identify entry points** → Read [entry-points.md](entry-points.md)
4. **Parse module files** → See [module-registry.md](../registry/module-registry.md)
5. **Start working** → Use entry points for your specific task

**For complex tasks:** Combine codebase map, patterns, and entry points to navigate efficiently.
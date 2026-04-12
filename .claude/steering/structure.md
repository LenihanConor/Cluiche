# Codebase Structure

## Top-Level Layout

```
C:\GitHub\Cluiche\
├── Dia/                          # Dia engine modules (shared platform code)
│   ├── DiaCore/                  # Core utilities and containers
│   ├── DiaMaths/                 # Math library
│   ├── DiaGraphics/              # Graphics abstraction
│   ├── DiaWindow/                # Window management
│   ├── DiaInput/                 # Input handling
│   ├── DiaApplication/           # Application framework
│   ├── DiaUI/                    # UI base
│   ├── DiaUIAwesomium/           # Awesomium UI implementation
│   ├── DiaSFML/                  # SFML integration layer
│   └── ...                       # Other subsystems
│
├── Cluiche/                      # Main solution directory
│   ├── Cluiche.sln               # Visual Studio solution
│   ├── Cluiche/                  # Main executable project
│   │   ├── Main.cpp              # Entry point
│   │   ├── ApplicationFlow/      # Main processing unit implementation
│   │   └── Levels/               # Game levels (DummyLevel, UnitTestLevel)
│   └── Tests/                    # Test projects
│       ├── UnitTests/            # Unit test project
│       └── GoogleTests/          # Google Test suites
│
├── External/                     # Third-party dependencies
│   ├── SFML/                     # Graphics/window/multimedia
│   ├── Awesomium SDK/            # HTML UI framework
│   ├── jsoncpp-master/           # JSON parsing
│   ├── Webix/                    # Web UI framework
│   └── VisJS/                    # Visualization framework
│
├── Tools/                        # Build and analysis tools
│   └── dia_modules.py            # Module dependency analyzer
│
└── docs/                         # Documentation
    ├── specs/                    # Spec-driven development
    │   ├── platform/             # Platform spec
    │   ├── applications/         # Application specs
    │   ├── systems/              # System specs
    │   └── features/             # Feature specs
    └── reference/                # Reference documentation
        ├── getting-started/      # Onboarding
        ├── architecture/         # Architecture docs
        ├── api/                  # API reference
        └── ...                   # Other reference docs
```

## Module / Package Structure

### Dia Module Organization

Each Dia module follows this structure:

```
Dia/DiaModuleName/
├── dia.parent.module.architecture.module.md  # Module documentation (YAML)
├── ModuleClass.h                             # Primary header
├── ModuleClass.cpp                           # Implementation
├── SubsystemA/                               # Subsystem folders
│   ├── ClassA.h
│   ├── ClassA.cpp
│   └── ...
├── SubsystemB/
│   └── ...
├── DiaModuleName.vcxproj                     # Visual Studio project
└── DiaModuleName.vcxproj.filters             # VS project filters (IDE organization)
```

**Key Points:**
- Each module has `dia.*.architecture.module.md` with YAML frontmatter defining:
  - Module ID, dependencies, public API, responsibilities
- Module dependencies are acyclic (enforced by validation tool)
- Public headers use full paths: `#include <DiaCore/Containers/Arrays/Array.h>`

### Module Architecture Files

56+ architecture module files document the platform:
- Format: `dia.[parent].[module].architecture.module.md`
- Location: Root of each module directory
- Validation: `python Tools/dia_modules.py --validate`
- Registry: See `docs/reference/registry/module-registry.md`

## Shared vs App-Specific Code

**Shared Platform Code** (reusable across applications):
- **Location**: `Dia/` directory
- **Purpose**: Engine modules, infrastructure, utilities
- **Examples**: DiaCore, DiaMaths, DiaGraphics, DiaApplication
- **Ownership**: Platform team (all contributors)
- **Testing**: Unit tests in `Cluiche/Tests/UnitTests/`

**Application-Specific Code** (Cluiche game):
- **Location**: `Cluiche/Cluiche/` directory
- **Purpose**: Game-specific logic, levels, content
- **Examples**: Main.cpp, ApplicationFlow/, Levels/
- **Ownership**: Application developers
- **Testing**: Integration tests, level-specific tests

## Key Patterns

### 1. ProcessingUnit / Phase / Module Architecture

Application execution organized hierarchically:

```
ProcessingUnit (e.g., MainProcessingUnit)
  ├─ Module (e.g., RenderModule, PhysicsModule)
  │    └─ Provides functionality consumed by phases
  └─ Phase (e.g., InitPhase, UpdatePhase, ShutdownPhase)
       └─ Execution stages with state transitions
```

**Key Points:**
- ProcessingUnits can run on separate threads (Main/Render/Sim)
- Phases transition based on defined state machines
- Modules provide services that phases depend on
- Thread-safe phase queueing via `QueuePhaseTransition()`

### 2. Component System

Component-based entity architecture:

```
IComponentObject
  └─ Contains multiple IComponent instances
       └─ Created via IComponentFactory
            └─ Registered with ComponentFactoryRegistry
```

**Factory Types:**
- `DynamicComponentFactory` - Heap-allocated components
- `StaticPooledComponentFactory` - Pre-allocated object pools

### 3. Type System

Runtime type reflection and serialization:

```
DiaCore/Type/
  ├─ Type information extracted via reflection
  ├─ Serialization support for all typed objects
  └─ TypeSerialization for format conversion
```

### 4. String IDs with CRC

Compile-time string hashing for efficient comparisons:

```cpp
// Define constant IDs
static const StringCRC kUniqueId = "MyComponent";

// Used for entity/component identification
if (component->GetId() == kUniqueId) { ... }
```

### 5. Singleton Pattern

Thread-safe singletons via template:

```cpp
class MyManager : public Dia::Core::Singleton<MyManager> {
    // Implementation
};

// Usage
MyManager::Instance().DoSomething();
```

### 6. Observer Pattern

Event notification system:

```cpp
class Listener : public Observer { ... };
class EventSource : public ObserverSubject { ... };

source.Attach(&listener);
source.Notify();  // Calls Update() on all observers
```

## Project File Management

Visual Studio project files (`.vcxproj`) are the **source of truth** for builds:

- **When adding files**: Update both `.vcxproj` (build rules) and `.vcxproj.filters` (IDE organization)
- **Use relative paths**: From the `.vcxproj` location
- **Maintain consistency**: Follow existing patterns in project files
- **Validation**: Build after changes to verify project file correctness

### Adding a New Module

1. Create module directory: `Dia/DiaCore/NewModule/`
2. Create architecture doc: `dia.[parent].[module].architecture.module.md` with YAML
3. Add module to parent's `.vcxproj` and `.vcxproj.filters`
4. Update parent module's `dependent_modules` list
5. Verify: `python Tools/dia_modules.py --validate`

## Include Paths

Module include directories configured in `.vcxproj` files. Use full module paths:

```cpp
// Correct
#include <DiaCore/Containers/Arrays/Array.h>
#include <DiaMaths/Vector/Vector2D.h>

// Incorrect
#include "Array.h"           // Ambiguous
#include "../../Array.h"     // Brittle
```

## Thread Safety Considerations

Multi-threaded architecture requires careful synchronization:

- **Shared State**: Use `std::mutex` for protection
- **Phase Transitions**: `QueuePhaseTransition()` is thread-safe; `TransitionPhase()` is not
- **Module Access**: Review phase ordering and module dependencies for race conditions
- **Testing**: See `docs/reference/testing/thread-safety-testing.md`

---
include: conditional
---

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
│   ├── DiaApplicationFlow/           # Application framework
│   ├── DiaUI/                    # UI base
│   ├── DiaSFML/                  # SFML integration layer
│   └── ...                       # Other subsystems
│
├── Cluiche/                      # Main solution directory
│   ├── Cluiche.sln               # Visual Studio solution
│   ├── Cluiche/                  # Main executable project
│   │   ├── Main.cpp              # Entry point
│   │   ├── ApplicationFlow/      # Main processing unit implementation
│   │   └── Stages/               # Game stages (DummyStage, UnitTestLevel)
│   └── Tests/                    # Test projects
│       ├── UnitTests/            # Unit test project
│       └── GoogleTests/          # Google Test suites
│
├── External/                     # Third-party dependencies
│   ├── SFML/                     # Graphics/window/multimedia
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

## Key Patterns

### Component System

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

### Type System

Runtime type reflection and serialization:

```
DiaCore/Type/
  ├─ Type information extracted via reflection
  ├─ Serialization support for all typed objects
  └─ TypeSerialization for format conversion
```

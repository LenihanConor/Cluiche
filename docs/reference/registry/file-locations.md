# File Locations

**Last Updated:** 2026-04-01

Quick reference for where things live in the Cluiche codebase.

---

## Directory Structure

```
C:\GitHub\Cluiche\
├── Cluiche/                      # Main solution and application
├── Dia/                          # Engine subsystems
├── External/                     # Third-party dependencies
├── Tools/                        # Development tools
└── docs/                         # Documentation
```

---

## Cluiche/ - Application

### Root Files
- `Cluiche/Cluiche.sln` - Visual Studio solution
- `Cluiche/README.md` - Solution documentation

### Main Executable
- `Cluiche/CluicheTest/` - Main application project
  - `Main.cpp` - **Entry point**
  - `Cluiche.vcxproj` - Project file
  - `Cluiche.vcxproj.filters` - IDE file organization

### Threading Architecture
- `Cluiche/ApplicationFlow/ProcessingUnits/`
  - `MainProcessingUnit.h/.cpp`
  - `RenderProcessingUnit.h/.cpp`
  - `SimProcessingUnit.h/.cpp`

### Phases
- `Cluiche/ApplicationFlow/Phases/`
  - `MainBootPhase.h`
  - `MainBootStrapPhase.h`
  - `RenderRunningPhase.h`
  - `SimBootPhase.h`
  - `SimBootStrapPhase.h`

### Application Modules
- `Cluiche/CluicheKernel/ApplicationFlow/Modules/`
  - `MainKernelModule.h`
  - `MainUIModule.h`
  - `LevelFactoryModule.h`
  - `SimTimeServerModule.h`
  - `SimInputFrameStreamModule.h`
  - `SimUIProxyModule.h`

### Game Levels
- `Cluiche/Levels/`
  - `DummyStage/DummyStage.h/.cpp`
  - `UnitTestLevel/UnitTestLevel.h/.cpp`

### Tests
- `Cluiche/Tests/` - Test projects
- `Cluiche/UnitTests/` - Unit test projects

### Build Output
- `Cluiche/bin/exe/` - Executable output
  - `Debug/Cluiche.exe`
  - `Release/Cluiche.exe`
- `Cluiche/bin/lib/` - Library output

---

## Dia/ - Engine

### Application Framework
- `Dia/DiaApplicationFlow/`
  - `ApplicationProcessingUnit.h` - ProcessingUnit base
  - `ApplicationPhase.h` - Phase base
  - `ApplicationModule.h` - Module base
  - `ApplicationStateObject.h` - StateObject base
  - `FrameStream.h` - Thread-safe queue

### Core Library
- `Dia/DiaCore/`

#### Containers
- `Dia/DiaCore/Containers/`
  - `Arrays/`
    - `Array.h` - Fixed-size array
    - `DynamicArray.h` - Dynamic array
  - `HashTables/`
    - `HashTable.h` - Hash map
  - `LinkLists/`
    - `LinkList.h` - Linked list
  - `BitFlag/`
    - `BitFlag.h` - Bit flags
  - `Graphs/`
    - `Graph.h` - Graph structure
  - `Strings/`
    - `String.h` - String class

#### Architecture Patterns
- `Dia/DiaCore/Architecture/`
  - `Singleton/Singleton.h` - Singleton pattern
  - `Factory/Factory.h` - Factory pattern
  - `Observer/Observer.h` - Observer pattern
  - `Functor/Functor.h` - Function objects
  - `Components/`
    - `IComponent.h` - Component interface
    - `ComponentFactoryRegistry.h` - Component registry
    - `StaticPooledComponentFactory.h` - Pooled factory

#### Type System
- `Dia/DiaCore/CRC/`
  - `CRC.h` - StringCRC implementation
- `Dia/DiaCore/Type/`
  - `TypeDefinition.h` - Type metadata
  - `TypeRegistry.h` - Type registry

#### Time
- `Dia/DiaCore/Time/`
  - `TimeServer.h` - Global time source
  - `TimeAbsolute.h` - Absolute time
- `Dia/DiaCore/Timer/`
  - `TimeRelative.h` - Relative time

#### Utilities
- `Dia/DiaCore/Core/`
  - `Assert.h` - DIA_ASSERT macro
  - `Log.h` - DIA_LOG macro
  - `CallStack.h` - Call stack capture
- `Dia/DiaCore/Memory/`
  - `Memory.h` - Memory utilities
- `Dia/DiaCore/FilePath/`
  - `FilePath.h` - File path handling
- `Dia/DiaCore/Json/`
  - `JsonWrapper.h` - JsonCpp wrapper

#### Deprecated
- `Dia/DiaCore/Deprecated/` - Old code (not compiled)

### Math Library
- `Dia/DiaMaths/`
  - `Vector/`
    - `Vector2D.h` - 2D vector
    - `Vector3D.h` - 3D vector
    - `Vector4D.h` - 4D vector
  - `Matrix/`
    - `Matrix22.h` - 2x2 matrix
    - `Matrix33.h` - 3x3 matrix
    - `Matrix44.h` - 4x4 matrix
  - `Transform/`
    - `Transform2D.h` - 2D transform
    - `Transform3D.h` - 3D transform
  - `Shape/`
    - `Circle.h` - Circle shape
    - `AABB.h` - Axis-aligned box
    - `Line.h` - Line segment
    - `Polygon.h` - Polygon
  - `Core/`
    - `Random.h` - Random numbers
    - `FloatMaths.h` - Float utilities
    - `Interpolation.h` - Lerp functions

### Graphics
- `Dia/DiaGraphics/`
  - `Interface/`
    - `ICanvas.h` - Canvas interface
    - `Frame.h` - Frame data

### Window
- `Dia/DiaWindow/`
  - `Interface/`
    - `IWindow.h` - Window interface

### Input
- `Dia/DiaInput/`
  - `DiaInputEvent.h` - Input events
  - `DiaInputInputSourceManager.h` - Input sources

### UI
- `Dia/DiaUI/`
  - `DiaUIIUISystem.h` - UI interface

### SFML Backend
- `Dia/DiaSFML/`
  - `DiaSFMLRenderWindow.h` - SFML window + canvas
  - `DiaSFMLInputSource.h` - SFML input
  - `DiaSFMLSoundManager.h` - SFML audio

### File I/O
- `Dia/DiaIO/`
  - File I/O utilities

### Physics (Stub)
- `Dia/DiaPhysics/`
  - Minimal implementation

### AI (Stub)
- `Dia/DiaAI/`
  - Minimal implementation

---

## External/ - Third-Party

### SFML
- `External/SFML-2.5.1/`
  - `include/` - SFML headers
  - `lib/` - SFML libraries
  - `bin/` - SFML DLLs

### JsonCpp
- `External/jsoncpp-master/`
  - `include/` - JSON headers
  - `src/` - JSON source

### Web UI Components
- `External/Webix/` - Web UI library
- `External/VisJS/` - Graph visualization

---

## Tools/

### Python Tools
- `Tools/dia_modules.py` - Module dependency analyzer
  - Validates module architecture files
  - Generates dependency graphs
  - Checks for circular dependencies

### CLI Tools
- `Tools/CLI/` - MDK CLI (Python)
  - Command-line development tools
  - See `Tools/CLI/README.md`

### Blue Console
- `Tools/Console/` - Web-based console (TypeScript)
  - `api/` - API layer
  - `app/` - Application
  - `core/` - Core functionality
  - `server/` - Server

---

## docs/ - Documentation

### Main Documentation
- `docs/README.md` - Documentation hub
- `docs/DOCUMENTATION_TODO.md` - Progress tracker

### Getting Started
- `docs/00-getting-started/`
  - `quickstart.md` - 5-minute orientation

### Architecture
- `docs/reference/architecture/`
  - `architecture.md` - PRIMARY architecture doc
  - `threading-model.md`
  - `module-system.md`
  - `level-system.md`
  - `diagrams/*.md` - Mermaid diagram wrappers

### Design Rationale
- `docs/reference/design-rationale/`
  - `design.md` - PRIMARY design doc
  - `why-module-phase-pu.md`
  - `why-dia.md`
  - `historical-decisions.md`

### Requirements (As-Built)
- `docs/reference/requirements-as-built/`
  - `requirements.md` - PRIMARY requirements doc

### Testing
- `docs/reference/testing/`
  - `test.md` - PRIMARY testing doc
  - Unit, integration, performance, thread safety testing guides

### API
- `docs/reference/api/`
  - `api-overview.md` - API hub
  - `dia/` - Dia subsystem APIs (application, core, maths, graphics, etc.)

### AI Guides
- `docs/reference/ai-guides/`
  - `AI-README.md` - AI entry point
  - `codebase-map.md`
  - `entry-points.md`
  - `patterns-reference.md`
  - `system-boundaries.md`
  - `dependency-graph.md`
  - `thread-safety-guide.md`
  - `quick-reference.md`

### Subsystems
- `docs/reference/subsystems/`
  - `dia-maths/` - DiaMaths deep dives (known-issues, thread-safety-notes)

### Development
- `docs/reference/development/`
  - Development guides (contributing, coding-standards, debugging, changelog)

### Registry
- `docs/reference/registry/`
  - `module-registry.md` - All 56 modules
  - `module-metadata-schema.md` - Schema docs
  - `file-locations.md` - This file
  - `external-links.md` - External docs

---

## Root-Level Files

### Documentation
- `README.md` - Repository entry point
- `CLAUDE.md` - Claude Code instructions

### Working Files (Archived)
- `BUG_REPORT.md` → Migrated to [DiaMaths Known Issues](../subsystems/dia-maths/known-issues.md)
- `THREAD_SAFE_RANDOM.md` → Migrated to [DiaMaths Thread Safety Notes](../subsystems/dia-maths/thread-safety-notes.md)
- `DIACORE_CLEANUP_ANALYSIS.md` → Migrated to [DiaCore Cleanup Analysis](../development/DIACORE_CLEANUP_ANALYSIS.md)
- Other working files → Moved to `archive/`

**Status:** Historical files archived, content migrated to reference documentation

---

## Module Architecture Files

**Pattern:** `dia.<parent>.<module>.architecture.module.md`

**Location:** Co-located with code

**Count:** 56 files

**Examples:**
- `Dia/DiaCore/Containers/Arrays/dia.core.containers.arrays.architecture.module.md`
- `Dia/DiaMaths/Vector/dia.maths.vector.architecture.module.md`
- `Cluiche/Stages/DummyStage/dia.cluiche.levels.dummylevel.architecture.module.md`

**[→ Module Registry](module-registry.md)**

---

## Build Files

### Visual Studio
- `*.sln` - Solution files
- `*.vcxproj` - Project files
- `*.vcxproj.filters` - IDE file organization

**Locations:**
- `Cluiche/Cluiche.sln` - Main solution
- `Cluiche/CluicheTest/Cluiche.vcxproj` - Main exe
- `Dia/DiaCore/DiaCore.vcxproj` - DiaCore lib
- `Dia/DiaMaths/DiaMaths.vcxproj` - DiaMaths lib
- (And more for each subsystem)

---

## File Naming Conventions

### C++ Code
- Headers: `ClassName.h`
- Source: `ClassName.cpp`
- Example: `Vector2D.h`, `Vector2D.cpp`

### Module Architecture
- Pattern: `dia.<parent>.<module>.architecture.module.md`
- Example: `dia.core.containers.arrays.architecture.module.md`

### Documentation
- Lowercase with hyphens: `threading-model.md`
- PRIMARY docs: `architecture.md`, `design.md`, `requirements.md`, `test.md`

---

## Quick Lookup

| What | Where |
|------|-------|
| **Entry point** | `Cluiche/CluicheTest/Main.cpp` |
| **Main thread** | `Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.h` |
| **DynamicArray** | `Dia/DiaCore/Containers/Arrays/DynamicArray.h` |
| **Vector2D** | `Dia/DiaMaths/Vector/Vector2D.h` |
| **ICanvas** | `Dia/DiaGraphics/Interface/ICanvas.h` |
| **Module base** | `Dia/DiaApplicationFlow/ApplicationModule.h` |
| **Phase base** | `Dia/DiaApplicationFlow/ApplicationPhase.h` |
| **StringCRC** | `Dia/DiaCore/CRC/CRC.h` |
| **TimeServer** | `Dia/DiaCore/Time/TimeServer.h` |
| **SFML headers** | `External/SFML-2.5.1/include/` |
| **Tools** | `Tools/dia_modules.py` |
| **Tests** | `Cluiche/Levels/UnitTestLevel/` |

---

## Finding Files

### By Name
```bash
# Find specific file
find . -name "Vector2D.h"

# Find all headers
find . -name "*.h"

# Find module files
find . -name "*.architecture.module.md"
```

### By Pattern
```bash
# All DiaCore containers
ls Dia/DiaCore/Containers/**/*.h

# All phases
ls Cluiche/ApplicationFlow/Phases/*.h

# All modules
ls Cluiche/CluicheKernel/ApplicationFlow/Modules/*.h
```

### By Content
```bash
# Find files containing string
grep -r "ProcessingUnit" --include="*.h"

# Find module dependencies
python Tools/dia_modules.py --list dia.core.containers
```

---

## Summary

**Main Areas:**
- `Cluiche/` - Application code and solution
- `Dia/` - Engine subsystems (13 subsystems)
- `External/` - Third-party libraries
- `Tools/` - Development tools
- `docs/` - Documentation

**Key Files:**
- Entry point: `Cluiche/CluicheTest/Main.cpp`
- Solution: `Cluiche/Cluiche.sln`
- Module tool: `Tools/dia_modules.py`
- Documentation hub: `docs/README.md`

**File Patterns:**
- Headers: `ClassName.h`
- Module files: `dia.<parent>.<module>.architecture.module.md`
- Documentation: `lowercase-with-hyphens.md`

**[→ Module Registry](module-registry.md)**  
**[→ External Links](external-links.md)**  
**[→ Back to Documentation Index](../README.md)**

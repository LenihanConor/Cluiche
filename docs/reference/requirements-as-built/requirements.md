# System Requirements

**Last Updated:** 2026-04-01

Comprehensive requirements checklist for Cluiche and Dia with traceability to implementation.

---

## Requirements Overview

This document tracks functional and non-functional requirements with status, priority, and implementation traceability.

**Format:** `[ID] [Status] [Priority] Description → Implementation`

**Status:**
- ✅ Complete - Fully implemented and working
- 🚧 In Progress - Partially implemented
- ❌ Not Started - Not yet implemented
- ⚠️ Blocked - Cannot proceed (dependency or issue)

**Priority:**
- **P0** - Critical (must have)
- **P1** - High (should have)
- **P2** - Medium (nice to have)
- **P3** - Low (future)

---

## Functional Requirements

### Application Framework (Cluiche)

#### CF-001: Multi-Threaded Application
**✅ P0: Support multi-threaded application with Main/Render/Sim threads**

→ Implementation:
- `Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.h`
- `Cluiche/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h`
- `Cluiche/ApplicationFlow/ProcessingUnits/SimProcessingUnit.h`

**Verification:**
- Main thread spawns Render and Sim threads
- Each thread runs independently
- Synchronization via mutexes and FrameStreams

---

#### CF-002: Pluggable Level System
**✅ P0: Support pluggable level system with factory pattern**

→ Implementation:
- `Cluiche/CluicheKernel/LevelFactory.h`
- `Cluiche/Levels/DummyLevel/DummyLevel.h`
- `Cluiche/Levels/UnitTestLevel/UnitTestLevel.h`

**Verification:**
- Levels registered via `LevelFactory::Register<T>()`
- Levels created at runtime by name
- New levels can be added without modifying core code

---

#### CF-003: Module-Based Architecture
**✅ P1: Support module-based architecture for extensibility**

→ Implementation:
- `Cluiche/CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h`
- `Cluiche/CluicheKernel/ApplicationFlow/Modules/MainUIModule.h`
- `Cluiche/CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h`
- `Cluiche/CluicheKernel/ApplicationFlow/Modules/SimTimeServerModule.h`
- `Cluiche/CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h`
- `Cluiche/CluicheKernel/ApplicationFlow/Modules/SimUIProxyModule.h`

**Verification:**
- Modules declare dependencies explicitly
- Dependency resolution automatic (topological sort)
- Modules have clear lifecycle (Start/Update/Stop)

---

#### CF-004: Phase-Based Execution
**✅ P1: Support phase-based execution flow within threads**

→ Implementation:
- `Cluiche/ApplicationFlow/Phases/MainBootPhase.h`
- `Cluiche/ApplicationFlow/Phases/MainBootStrapPhase.h`
- `Cluiche/ApplicationFlow/Phases/RenderRunningPhase.h`
- `Cluiche/ApplicationFlow/Phases/SimBootPhase.h`
- `Cluiche/ApplicationFlow/Phases/SimBootStrapPhase.h`

**Verification:**
- Phases transition explicitly (Boot → BootStrap → Running)
- Modules can be retained across phase transitions
- Phase hooks provide customization points

---

#### CF-005: Module Dependency Visualization
**🚧 P2: Generate module dependency graph for debugging**

→ Implementation:
- `MainProcessingUnit::GenerateModuleDependecyGraph()` (partially implemented)

**Verification:**
- Can export dependency graph
- Graph shows module relationships
- Useful for debugging initialization order

**Status:** Partially implemented, needs completion

---

### Dia Engine Requirements

#### DE-001: Platform-Agnostic Framework
**✅ P0: Provide platform-agnostic application framework**

→ Implementation:
- `Dia/DiaApplication/ApplicationProcessingUnit.h`
- `Dia/DiaApplication/ApplicationPhase.h`
- `Dia/DiaApplication/ApplicationModule.h`

**Verification:**
- Core framework has no platform-specific code
- Backends pluggable (DiaSFML can be swapped)

---

#### DE-002: Core Container Library
**✅ P0: Provide core container library (arrays, hash tables, graphs)**

→ Implementation:
- `Dia/DiaCore/Containers/Arrays/DiaCoreArray.h`
- `Dia/DiaCore/Containers/Arrays/DiaCoreDynamicArray.h`
- `Dia/DiaCore/Containers/HashTables/DiaCoreHashTable.h`
- `Dia/DiaCore/Containers/Graphs/DiaCoreGraph.h`
- `Dia/DiaCore/Containers/LinkLists/DiaCoreLinkList.h`

**Verification:**
- Containers work without STL
- Explicit memory control
- Type-safe templates

---

#### DE-003: Type Reflection System
**✅ P0: Provide type reflection system with compile-time IDs**

→ Implementation:
- `Dia/DiaCore/CRC/DiaCoreCRC.h` (StringCRC)
- `Dia/DiaCore/Type/DiaCoreTypeRegistry.h`
- `Dia/DiaCore/Type/DiaCoreTypeDefinition.h`

**Verification:**
- StringCRC generates compile-time hashes
- TypeRegistry stores type metadata
- Can serialize types to JSON

---

#### DE-004: Cross-Platform Graphics
**✅ P0: Provide cross-platform graphics abstraction**

→ Implementation:
- `Dia/DiaGraphics/Interface/ICanvas.h`
- `Dia/DiaSFML/DiaSFMLRenderWindow.h` (SFML backend)

**Verification:**
- ICanvas abstracts rendering
- Backend can be swapped (SFML → SDL/Direct3D)

---

#### DE-005: Math Library
**✅ P1: Provide comprehensive math library (vectors, matrices, transforms, shapes)**

→ Implementation:
- `Dia/DiaMaths/Vector/DiaMathsVector2D.h`
- `Dia/DiaMaths/Vector/DiaMathsVector3D.h`
- `Dia/DiaMaths/Matrix/DiaMathsMatrix33.h`
- `Dia/DiaMaths/Matrix/DiaMathsMatrix44.h`
- `Dia/DiaMaths/Transform/DiaMathsTransform2D.h`
- `Dia/DiaMaths/Shape/Circle.h`
- `Dia/DiaMaths/Shape/AABB.h`

**Verification:**
- Vector math operations (dot, cross, magnitude)
- Matrix operations (multiply, inverse, transpose)
- Transform hierarchies (parent/child)
- Intersection tests

---

#### DE-006: Thread-Safe Math
**⚠️ P1: Ensure thread-safe math operations**

→ Implementation:
- `Dia/DiaMaths/Core/DiaMathsRandom.h` (fixed 2026-03)

**Known Issues:**
- ✅ Random number generation thread-safe (mutex-protected)
- ⚠️ Transform2D hierarchy traversal not thread-safe
- ⚠️ Multiple traversals performance issue

**Verification:**
- Random generators can be called from multiple threads
- No race conditions in math operations

**Status:** Partially complete, see [known issues](../subsystems/dia-maths/known-issues.md)

---

#### DE-007: Input Event System
**✅ P1: Provide input event system**

→ Implementation:
- `Dia/DiaInput/DiaInputEvent.h`
- `Dia/DiaInput/DiaInputInputSourceManager.h`
- `Dia/DiaSFML/DiaSFMLInputSource.h` (SFML backend)

**Verification:**
- Events for keyboard, mouse, gamepad
- Multiple input sources supported
- Backend-agnostic event format

---

#### DE-008: UI Integration
**✅ P2: Provide UI integration (web-based)**

→ Implementation:
- `Dia/DiaUI/DiaUIIUISystem.h`

**Verification:**
- Can load HTML/CSS/JavaScript pages
- Can bind C++ methods to JavaScript

---

#### DE-009: Physics Simulation
**✅ P2: Provide basic physics simulation**

→ Implementation:
- `Dia/DiaPhysics/` (stub implementation)

**Verification:**
- Subsystem exists (basic structure)

**Status:** Stub only, minimal functionality

---

#### DE-010: AI Pathfinding
**✅ P3: Provide AI pathfinding support**

→ Implementation:
- `Dia/DiaAI/` (stub implementation)

**Verification:**
- Subsystem exists (basic structure)

**Status:** Stub only, minimal functionality

---

### External Tools

#### ET-001: Command-Line Tools
**✅ P1: Provide command-line development tools**

→ Implementation:
- `Tools/CLI/` (MDK CLI in Python)

**Verification:**
- CLI tools available
- See `Tools/CLI/README.md`

---

#### ET-002: Web Console
**✅ P2: Provide web-based console for runtime debugging**

→ Implementation:
- `Tools/Console/` (Blue Console in TypeScript)

**Verification:**
- Web console available
- Multiple packages (api, app, core, server)

---

## Non-Functional Requirements

### Performance

#### NF-001: Render Frame Rate
**✅ P0: Render thread maintains 60 FPS target**

→ Implementation:
- `RenderProcessingUnit::SetThreadLimiter(new TimeThreadLimiter(60.0f))`

**Verification:**
- TimeThreadLimiter enforces 60 FPS
- Sleeps thread if frame completes early
- Consistent frame rate independent of sim speed

**Metrics:**
- Target: 60 FPS (16.67ms/frame)
- Actual: ~59-60 FPS typical

---

#### NF-002: Container Performance
**✅ P1: Efficient container operations (O(1) hash table lookup)**

→ Implementation:
- `Dia/DiaCore/Containers/HashTables/DiaCoreHashTable.h`

**Verification:**
- HashTable uses CRC32 keys (O(1) lookup)
- No linear searches in hot paths

**Metrics:**
- HashTable lookup: O(1) average
- DynamicArray access: O(1)

---

#### NF-003: Transform Hierarchy Performance
**⚠️ P1: Minimize hierarchy traversals in Transform system**

→ Implementation:
- `Dia/DiaMaths/Transform/DiaMathsTransform2D.h`

**Known Issue:**
- GetWorldTransform() traverses hierarchy multiple times
- No caching (dirty flag not implemented)

**Verification:**
- Transform hierarchy traversal works (correctness)
- Performance suboptimal (multiple traversals)

**Status:** Known performance issue, see [known issues](../subsystems/dia-maths/known-issues.md)

---

### Reliability

#### NF-004: Thread-Safe Synchronization
**🚧 P0: Provide thread-safe synchronization primitives**

→ Implementation:
- `std::mutex` in ProcessingUnit phase transitions
- `std::mutex` in ObserverSubject notifications
- `std::mutex` in FrameStream operations
- `std::mutex` in SimUIProxyModule

**Verification:**
- No race conditions in phase transitions
- Observer notifications thread-safe
- Frame streams thread-safe

**Status:** Core systems thread-safe, needs comprehensive audit

---

#### NF-005: Type-Safe Communication
**✅ P1: Ensure type-safe module communication**

→ Implementation:
- Template-based GetDependency<T>()
- Compile-time type checking

**Verification:**
- Module dependencies type-safe
- Compiler errors for wrong types
- No void* casts in module system

---

#### NF-006: Unit Test Coverage
**❌ P2: Comprehensive unit test coverage**

→ Implementation:
- `Cluiche/Levels/UnitTestLevel/` (in-engine test harness)
- `Cluiche/Tests/` (test projects)
- `Cluiche/UnitTests/` (unit test projects)

**Verification:**
- Test framework exists
- Some tests implemented

**Status:** Infrastructure exists, coverage gaps remain

**Target Coverage:**
- DiaCore: 70% (current ~30%)
- DiaMaths: 70% (current ~40%)
- DiaApplication: 80% (current <20%)

**[→ Test coverage targets](../testing/test-coverage-targets.md)**

---

### Maintainability

#### NF-007: Modular Architecture
**✅ P0: Support modular architecture for independent subsystem updates**

→ Implementation:
- Module/Phase/ProcessingUnit pattern
- Explicit dependencies

**Verification:**
- Can update subsystem without affecting others
- Dependencies explicit (no hidden coupling)
- Modules testable in isolation

---

#### NF-008: Consistent Naming
**✅ P1: Maintain consistent naming conventions**

→ Implementation:
- Namespace: `Dia::<Subsystem>::<Class>`
- Files: `Dia<Subsystem>/<Subsystem><Class>.h`

**Verification:**
- Naming consistent across codebase
- Easy to find files by convention

---

#### NF-009: Comprehensive Documentation
**🚧 P1: Provide comprehensive documentation**

→ Implementation:
- 56 `.architecture.module.md` files (module metadata)
- `docs/` directory (this documentation)

**Verification:**
- Architecture documented
- Design rationale documented
- API documentation (in progress)

**Status:** Foundation complete, API docs in progress

---

### Portability

#### NF-010: Windows Support
**✅ P1: Full Windows platform support**

→ Implementation:
- Visual Studio 2015+ projects
- Tested on Windows 10/11

**Verification:**
- Builds and runs on Windows
- No platform-specific crashes

---

#### NF-011: Linux Support
**❌ P2: Linux platform support**

→ Implementation:
- None (Windows-only currently)

**Blockers:**
- No CMake build system
- Not tested

**Status:** Designed for portability, not yet implemented

---

#### NF-012: macOS Support
**❌ P3: macOS platform support**

→ Implementation:
- None (Windows-only currently)

**Blockers:**
- No CMake build system
- Not tested

**Status:** Designed for portability, not yet implemented

---

## Requirements Traceability

### High-Level to Implementation

**Application Framework:**
```
CF-001 (Multi-Threading) → MainPU/RenderPU/SimPU
CF-002 (Level System) → LevelFactory + ILevel implementations
CF-003 (Modules) → 6 core modules
CF-004 (Phases) → 5 phase implementations
```

**Dia Engine:**
```
DE-001 (Framework) → DiaApplication subsystem
DE-002 (Containers) → DiaCore/Containers/*
DE-003 (Type System) → StringCRC + TypeRegistry
DE-004 (Graphics) → DiaGraphics + DiaSFML
DE-005 (Math) → DiaMaths subsystem
```

**[→ Complete traceability matrix](traceability-matrix.md)**

---

## Requirements Sources

**Where requirements come from:**

1. **Original project goals**
   - Multi-threaded game framework
   - Modular architecture
   - Educational value (learn engine architecture)

2. **Current implementation analysis**
   - What already exists
   - What works well
   - What needs improvement

3. **Bug reports and fixes**
   - DiaMaths template bugs
   - Thread safety issues
   - Performance problems

4. **Developer workflows**
   - Common tasks (add module, create level)
   - Pain points (Visual Studio updates, manual registration)
   - Future needs (cross-platform, better UI)

---

## Requirements Management

### Adding New Requirements

1. Assign unique ID (CF-XXX, DE-XXX, NF-XXX, ET-XXX)
2. Set status (✅/🚧/❌/⚠️) and priority (P0/P1/P2/P3)
3. Document clearly (what, why, acceptance criteria)
4. Link to implementation (files, classes)
5. Update traceability matrix

### Updating Status

**When to Update:**
- ✅ Complete: Feature fully implemented and tested
- 🚧 In Progress: Partial implementation
- ❌ Not Started: Not yet begun
- ⚠️ Blocked: Cannot proceed (document blocker)

### Deprecating Requirements

**When requirement no longer relevant:**
1. Move to "Deprecated" section
2. Document why deprecated
3. Note replacement (if any)

---

## Summary Statistics

**Total Requirements:** 28

**By Status:**
- ✅ Complete: 18 (64%)
- 🚧 In Progress: 3 (11%)
- ❌ Not Started: 6 (21%)
- ⚠️ Blocked: 1 (4%)

**By Priority:**
- P0 Critical: 8 (100% complete)
- P1 High: 11 (73% complete)
- P2 Medium: 7 (43% complete)
- P3 Low: 2 (0% complete)

**By Category:**
- Functional: 15 (80% complete)
- Non-Functional: 12 (50% complete)
- Tools: 2 (100% complete)

---

## Next Steps

**High Priority (P0/P1):**
1. Complete thread safety audit (NF-004)
2. Fix DiaMaths bugs (DE-006)
3. Improve test coverage (NF-006)
4. Complete module dependency visualization (CF-005)

**Medium Priority (P2):**
5. Linux support (NF-011)
6. Choose UI backend replacement (DE-008 improvement)

**Low Priority (P3):**
7. macOS support (NF-012)
8. Expand Physics/AI subsystems (DE-009, DE-010)

---

**[→ Functional Requirements Details](functional-requirements.md)**  
**[→ Non-Functional Requirements Details](non-functional-requirements.md)**  
**[→ Requirements Traceability Matrix](traceability-matrix.md)**  
**[→ Testing Strategy](../testing/test.md)**

**[→ Back to Documentation Index](../README.md)**
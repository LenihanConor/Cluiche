# Functional Requirements

**Last Updated:** 2026-04-01

Detailed functional requirements for Cluiche application and Dia engine.

---

## Overview

This document provides detailed descriptions and acceptance criteria for all functional requirements.

**Related Documents:**
- **[→ Main Requirements](requirements.md)** - Complete requirements list
- **[→ Non-Functional Requirements](non-functional-requirements.md)** - Performance, reliability, maintainability
- **[→ Traceability Matrix](traceability-matrix.md)** - Requirements → Implementation mapping

---

## Cluiche Application Framework

### CF-001: Multi-Threaded Application ✅ P0

**Description:**
The application must support concurrent execution across three independent threads: Main, Render, and Sim.

**Rationale:**
- Separate render rate (60 FPS, VSync-locked) from simulation rate (variable)
- Non-blocking UI updates on Main thread
- Maximize CPU utilization through parallelism

**Acceptance Criteria:**
- [ ] Main thread spawns Render and Sim threads at startup
- [ ] Each thread runs independent update loop
- [ ] Threads synchronize via mutexes and FrameStreams
- [ ] Application shuts down cleanly (joins all threads)
- [ ] No deadlocks or race conditions in normal operation

**Implementation:**
- `MainProcessingUnit` - Main thread container
- `RenderProcessingUnit` - Render thread container
- `SimProcessingUnit` - Sim thread container
- `FrameStream<T>` - Thread-safe communication

**Test Cases:**
- Thread creation and shutdown
- Cross-thread communication
- Concurrent module updates
- Shutdown under load

**[→ Threading Model](../architecture/threading-model.md)**

---

### CF-002: Pluggable Level System ✅ P0

**Description:**
Support runtime-swappable game levels (menus, gameplay, test levels) via factory pattern.

**Rationale:**
- Levels are independent game states
- Support modding and extensibility
- Clean separation of concerns

**Acceptance Criteria:**
- [ ] Levels implement ILevel interface (Load, Unload)
- [ ] Levels registered with LevelFactory at startup
- [ ] Can transition between levels at runtime
- [ ] Previous level fully unloaded before new level loads
- [ ] No memory leaks during level transitions

**Implementation:**
- `ILevel` - Level interface
- `LevelFactory` - Singleton factory
- `DummyLevel`, `UnitTestLevel` - Example levels

**Test Cases:**
- Level registration
- Level creation by name
- Level transition (A → B → A)
- Level load/unload lifecycle
- Memory leak detection

**[→ Level System](../architecture/level-system.md)**

---

### CF-003: Module-Based Architecture ✅ P1

**Description:**
Support composable modules with explicit dependencies and automatic resolution.

**Rationale:**
- Modular design promotes reusability
- Explicit dependencies prevent initialization bugs
- Automatic resolution simplifies complex systems

**Acceptance Criteria:**
- [ ] Modules declare dependencies via StringCRC IDs
- [ ] Dependency graph automatically resolved (topological sort)
- [ ] Circular dependencies detected and rejected
- [ ] Modules have clear lifecycle (Construct → Start → Update → Stop → Destruct)
- [ ] Modules can be added without modifying core code

**Implementation:**
- `Module` base class
- `Phase::AddDependency()` - Declare dependencies
- Automatic topological sort in Phase

**Test Cases:**
- Simple dependency chain (A → B → C)
- Diamond dependency (A → B,C → D)
- Circular dependency detection
- Missing dependency detection

**[→ Module System](../architecture/module-system.md)**

---

### CF-004: Phase-Based Execution ✅ P1

**Description:**
Support explicit phase transitions for application flow control.

**Rationale:**
- Explicit state machine prevents undefined states
- Phases can share modules (avoid reconstruction)
- Clear initialization/shutdown order

**Acceptance Criteria:**
- [ ] Phases follow state machine (Construct → Start → Running → Stopping → Stop → Destruct)
- [ ] Can transition between phases explicitly
- [ ] Modules can persist across phase transitions
- [ ] Invalid transitions rejected
- [ ] Shutdown cleans up all phases

**Implementation:**
- `Phase` base class with state machine
- `Phase::TransitionTo()` - Immediate transition
- `Phase::QueuePhaseTransition()` - Deferred transition (thread-safe)

**Test Cases:**
- Basic phase transitions (Boot → Running → Shutdown)
- Module persistence across transitions
- Invalid transition rejection
- Queued transition execution

**[→ Module System](../architecture/module-system.md)**

---

### CF-005: Module Dependency Visualization 🚧 P2

**Description:**
Generate visual dependency graph for debugging module initialization order.

**Rationale:**
- Dependency bugs hard to diagnose without visualization
- Graph helps understand complex module relationships
- Useful for documentation

**Acceptance Criteria:**
- [ ] Can export dependency graph to DOT format (Graphviz)
- [ ] Graph shows all modules and dependencies
- [ ] Graph highlights circular dependencies
- [ ] Graph can be generated at runtime or build time

**Implementation:**
- `MainProcessingUnit::GenerateModuleDependecyGraph()` (partial)
- Export to `module_dependencies.dot`
- Render with Graphviz or integrate with Tools/dia_modules.py

**Test Cases:**
- Graph generation
- DOT format validation
- Graph rendering (manual)

**Status:** Partially implemented, needs completion

---

## Dia Engine Requirements

### DE-001: Platform-Agnostic Framework ✅ P0

**Description:**
Core framework must not depend on platform-specific APIs.

**Rationale:**
- Cross-platform support (Windows, Linux, macOS)
- Backend can be swapped (SFML → SDL → custom)
- Core logic independent of platform

**Acceptance Criteria:**
- [ ] No Windows.h or platform headers in Dia core
- [ ] All platform-specific code in backends (DiaSFML)
- [ ] Core APIs work with any backend
- [ ] Can build without backend (core only)

**Implementation:**
- `DiaApplication`, `DiaCore`, `DiaMaths` - Platform-agnostic
- `DiaGraphics::ICanvas` - Abstract interface
- `DiaSFML` - SFML backend (Windows/Linux/macOS)

**Test Cases:**
- Build DiaCore without SFML
- Swap backends at compile time
- Run on multiple platforms

**[→ Dia Engine Architecture](../architecture/dia-engine.md)**

---

### DE-002: Core Container Library ✅ P0

**Description:**
Provide STL-alternative containers with explicit memory control.

**Rationale:**
- Avoid STL dependencies (portability, control)
- Explicit allocators for memory tracking
- Debugging-friendly (no iterator obfuscation)

**Acceptance Criteria:**
- [ ] DynamicArray (like std::vector)
- [ ] HashTable (like std::unordered_map)
- [ ] LinkList (doubly-linked list)
- [ ] Graph (node/edge graph)
- [ ] BitFlag (bit array)
- [ ] All containers type-safe (templates)
- [ ] Containers support custom allocators

**Implementation:**
- `DiaCore/Containers/Arrays/DynamicArray.h`
- `DiaCore/Containers/HashTables/HashTable.h`
- `DiaCore/Containers/LinkLists/LinkList.h`
- `DiaCore/Containers/Graphs/Graph.h`
- `DiaCore/Containers/BitFlags/BitFlag.h`

**Test Cases:**
- Add/remove elements
- Iteration
- Memory allocation tracking
- Edge cases (empty, single element, large)

**[→ DiaCore API](../api/dia/core-api.md)**

---

### DE-003: Type Reflection System ✅ P0

**Description:**
Provide compile-time type IDs and runtime type metadata without RTTI.

**Rationale:**
- RTTI adds overhead and binary size
- StringCRC provides O(1) type comparison
- TypeRegistry enables serialization

**Acceptance Criteria:**
- [ ] StringCRC generates compile-time CRC32 hashes
- [ ] TypeRegistry stores type metadata (name, size)
- [ ] Can register types at runtime
- [ ] Can serialize/deserialize types to JSON
- [ ] No RTTI (/GR- in Visual Studio)

**Implementation:**
- `StringCRC` - Compile-time CRC32
- `TypeRegistry` - Runtime type database
- `TypeDefinition` - Type metadata

**Test Cases:**
- StringCRC compile-time evaluation
- Type registration
- Type lookup by ID
- JSON serialization

**[→ Type System Design](../design-rationale/why-type-system.md)**

---

### DE-004: Cross-Platform Graphics ✅ P0

**Description:**
Provide abstract graphics interface independent of rendering backend.

**Rationale:**
- Support multiple backends (SFML, SDL, Direct3D, OpenGL)
- Core game logic independent of rendering
- Easy to test (mock canvas)

**Acceptance Criteria:**
- [ ] ICanvas abstract interface
- [ ] DrawLine, DrawCircle, DrawRectangle, DrawSprite, DrawText
- [ ] Clear and Present for frame control
- [ ] Backend implements ICanvas
- [ ] Can swap backends at compile time

**Implementation:**
- `DiaGraphics::ICanvas` - Abstract interface
- `DiaSFML::DiaSFMLRenderWindow` - SFML implementation

**Test Cases:**
- Draw all primitive types
- Color and transparency
- Clear and present
- Backend swap (compile test)

**[→ DiaGraphics API](../api/dia/graphics-api.md)**

---

### DE-005: Math Library ✅ P1

**Description:**
Comprehensive 2D/3D math library for game development.

**Rationale:**
- Game math requirements (vectors, matrices, transforms)
- Consistent math across codebase
- Optimized for common operations

**Acceptance Criteria:**
- [ ] Vector2D, Vector3D, Vector4D
- [ ] Matrix22, Matrix33, Matrix44
- [ ] Transform2D, Transform3D (hierarchical)
- [ ] Shapes (Circle, AABB, Line, Polygon)
- [ ] Intersection tests
- [ ] Random number generation (thread-safe)
- [ ] Interpolation (Lerp, SmoothStep)
- [ ] Trigonometry utilities

**Implementation:**
- `DiaMaths/Vector/` - Vectors
- `DiaMaths/Matrix/` - Matrices
- `DiaMaths/Transform/` - Transforms
- `DiaMaths/Shape/` - Collision shapes
- `DiaMaths/Core/Random.h` - RNG

**Test Cases:**
- Vector operations (add, subtract, dot, cross)
- Matrix operations (multiply, inverse, transpose)
- Transform hierarchy (parent/child)
- Intersection tests (circle-circle, AABB-AABB)

**[→ DiaMaths API](../api/dia/maths-api.md)**

---

### DE-006: Thread-Safe Math ⚠️ P1

**Description:**
Ensure math operations safe for multi-threaded use.

**Rationale:**
- Math used across all threads
- Race conditions in shared state cause bugs
- Random number generation especially vulnerable

**Acceptance Criteria:**
- [ ] Random class thread-safe (uses mutex)
- [ ] Transform hierarchy thread-safe (if shared)
- [ ] No static mutable state in math functions
- [ ] Vector/matrix operations safe (if not shared)

**Implementation:**
- `DiaMaths/Core/Random.h` - Thread-safe RNG (std::mutex)

**Known Issues:**
- Transform2D::GetWorldMatrix() not thread-safe (hierarchy traversal)
- Workaround: Cache world matrix or use locks

**Test Cases:**
- Concurrent RNG calls from multiple threads
- Concurrent transform updates (stress test)

**Status:** Partially complete (Random fixed, Transform needs work)

**[→ Thread Safety Notes](../subsystems/dia-maths/thread-safety-notes.md)**

---

### DE-007: Input Abstraction ✅ P1

**Description:**
Platform-agnostic input handling (keyboard, mouse, gamepad).

**Rationale:**
- Input backend can be swapped
- Consistent input handling across platforms
- Easy to mock for testing

**Acceptance Criteria:**
- [ ] InputEvent structure (keyboard, mouse, gamepad, text)
- [ ] KeyCode enum (platform-agnostic)
- [ ] MouseButton enum
- [ ] InputSourceManager (manages multiple sources)
- [ ] Backend converts platform events to InputEvent

**Implementation:**
- `DiaInput::InputEvent` - Event structure
- `DiaInput::InputSourceManager` - Source management
- `DiaSFML::DiaSFMLInputSource` - SFML backend

**Test Cases:**
- Keyboard event conversion
- Mouse event conversion
- Multiple input sources
- Backend swap (compile test)

**[→ DiaInput API](../api/dia/input-api.md)**

---

### DE-008: UI Integration ✅ P1

**Description:**
Abstract UI system for HTML/CSS/JS or native UI.

**Rationale:**
- UI can use web tech (CEF) or native (ImGui)
- Core logic independent of UI backend
- Support for modding (custom UIs)

**Acceptance Criteria:**
- [ ] IUISystem abstract interface
- [ ] LoadUI, ExecuteJavaScript, BindCallback methods
- [ ] Backend implements IUISystem
- [ ] Can swap backends at compile time

**Implementation:**
- `DiaUI::IUISystem` - Abstract interface

**Test Cases:**
- Load UI
- Execute JavaScript
- Bind C++ callbacks
- Backend swap (compile test)

**Status:** Needs UI backend implementation (CEF or ImGui)

**[→ DiaUI API](../api/dia/ui-api.md)**

---

### DE-009: Window Management ✅ P1

**Description:**
Abstract window management (creation, fullscreen, VSync).

**Rationale:**
- Window backend can be swapped
- Consistent window API across platforms

**Acceptance Criteria:**
- [ ] IWindow abstract interface
- [ ] Create, Close, Display, Clear methods
- [ ] Fullscreen toggle
- [ ] VSync control
- [ ] Frame rate limiting

**Implementation:**
- `DiaWindow::IWindow` - Abstract interface
- `DiaSFML::DiaSFMLRenderWindow` - SFML backend (implements IWindow + ICanvas)

**Test Cases:**
- Window creation/destruction
- Fullscreen toggle
- VSync enable/disable
- Frame rate limiting

**[→ DiaWindow API](../api/dia/window-api.md)**

---

### DE-010: Physics API ❌ P2

**Description:**
Physics simulation with collision detection and response.

**Rationale:**
- Common game requirement
- Simplifies gameplay programming
- Performance-critical (needs optimization)

**Acceptance Criteria:**
- [ ] IPhysicsWorld interface
- [ ] Rigid body dynamics (2D minimum, 3D nice-to-have)
- [ ] Collision detection (circles, boxes, polygons)
- [ ] Collision response (elastic, inelastic)
- [ ] Constraints (distance, revolute, prismatic)

**Implementation:**
- Not yet implemented (stub)
- Planned backend: Box2D (2D) or Bullet Physics (3D)

**Test Cases:**
- Body creation/destruction
- Force application
- Collision detection
- Collision response
- Constraints

**Status:** Not started (P2 priority)

**[→ Physics API Stub](../api/dia/physics-api.md)**

---

### DE-011: AI/Pathfinding API ❌ P2

**Description:**
AI utilities including pathfinding, steering, FSM, behavior trees.

**Rationale:**
- Common game requirement
- Complex to implement from scratch
- Reusable across projects

**Acceptance Criteria:**
- [ ] A* pathfinding (minimum)
- [ ] Navigation mesh (nice-to-have)
- [ ] Steering behaviors (seek, flee, wander)
- [ ] FSM or Behavior Tree (one or both)

**Implementation:**
- Not yet implemented (stub)
- Planned backend: Recast/Detour (navmesh) or custom

**Test Cases:**
- Pathfinding (start → goal)
- Steering behaviors
- FSM state transitions
- Behavior tree execution

**Status:** Not started (P2 priority)

**[→ AI API Stub](../api/dia/ai-api.md)**

---

## Summary

**Total Functional Requirements:** 16
- **Cluiche (CF-*):** 5 requirements
- **Dia (DE-*):** 11 requirements

**Status Breakdown:**
- ✅ Complete: 11 (69%)
- 🚧 In Progress: 1 (6%)
- ⚠️ Blocked: 1 (6%)
- ❌ Not Started: 3 (19%)

**Priority Breakdown:**
- P0 (Critical): 7
- P1 (High): 7
- P2 (Medium): 2

**Next Steps:**
1. Complete CF-005 (module dependency visualization)
2. Fix DE-006 (Transform thread safety)
3. Plan DE-010 (physics) and DE-011 (AI) for future release

**[→ Main Requirements](requirements.md)**  
**[→ Non-Functional Requirements](non-functional-requirements.md)**  
**[→ Traceability Matrix](traceability-matrix.md)**

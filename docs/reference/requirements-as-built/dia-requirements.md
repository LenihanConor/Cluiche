# Dia Engine Requirements

**Last Updated:** 2026-04-01

Engine-specific requirements for the Dia framework.

---

## Overview

This document details requirements specific to the Dia engine layer (foundation for Cluiche and future applications).

**Scope:**
- `Dia/` engine subsystems
- Platform abstractions
- Core utilities
- Engine architecture

**Out of Scope:**
- Application requirements (see [Cluiche Requirements](cluiche-requirements.md))
- Game-specific features

**Related Documents:**
- **[→ Main Requirements](requirements.md)** - Complete requirements list
- **[→ Functional Requirements](functional-requirements.md)** - All functional requirements
- **[→ Cluiche Requirements](cluiche-requirements.md)** - Application requirements

---

## Core Systems (DiaCore)

### DR-001: Custom Containers ✅ P0

**Requirement:**
Provide STL-alternative containers with explicit memory control.

**Rationale:**
- Avoid STL dependencies (control, portability)
- Explicit allocators for tracking
- Debugging-friendly implementations

**Acceptance Criteria:**
- [ ] DynamicArray<T> (like std::vector)
- [ ] HashTable<K,V> (like std::unordered_map)
- [ ] LinkList<T> (doubly-linked list)
- [ ] Graph<T> (node/edge graph)
- [ ] BitFlag<N> (bit array)
- [ ] Array<T,N> (fixed-size array)
- [ ] All type-safe (templates)

**Implementation:**
- `DiaCore/Containers/Arrays/DynamicArray.h`
- `DiaCore/Containers/HashTables/HashTable.h`
- `DiaCore/Containers/LinkLists/LinkList.h`
- `DiaCore/Containers/Graphs/Graph.h`
- `DiaCore/Containers/BitFlags/BitFlag.h`
- `DiaCore/Containers/Arrays/Array.h`

**Test Cases:**
- Add, remove, iterate operations
- Edge cases (empty, single element, large)
- Memory allocation tracking
- Performance vs STL

**Status:** ✅ Complete

**[→ DiaCore API](../reference/api/dia/core-api.md)**

---

### DR-002: StringCRC Type System ✅ P0

**Requirement:**
Compile-time string hashing for O(1) string comparison without RTTI.

**Rationale:**
- RTTI adds overhead and binary size
- String comparison O(1) instead of O(n)
- 4 bytes instead of dynamic allocation

**Acceptance Criteria:**
- [ ] StringCRC computes CRC32 at compile-time
- [ ] Used for module IDs, type IDs, string identifiers
- [ ] constexpr evaluation
- [ ] No runtime overhead

**Implementation:**
- `DiaCore/CRC/StringCRC.h`
- Compile-time CRC32 algorithm

**Test Cases:**
- Compile-time evaluation verification
- Hash collision testing
- Comparison performance

**Status:** ✅ Complete

**[→ Type System Design](../reference/design-rationale/why-type-system.md)**

---

### DR-003: Type Registry ✅ P0

**Requirement:**
Runtime type metadata for serialization and reflection without RTTI.

**Rationale:**
- Enable JSON serialization
- Runtime type queries
- No RTTI overhead

**Acceptance Criteria:**
- [ ] TypeRegistry stores type metadata (name, size, ID)
- [ ] Can register types at runtime
- [ ] Can query types by StringCRC
- [ ] Supports serialization/deserialization

**Implementation:**
- `DiaCore/Type/TypeRegistry.h`
- `DiaCore/Type/TypeDefinition.h`

**Test Cases:**
- Type registration
- Type lookup by ID
- JSON serialization
- Type metadata queries

**Status:** ✅ Complete

---

### DR-004: Component System ✅ P1

**Requirement:**
Component-based architecture with factories and pooling.

**Rationale:**
- Composition over inheritance
- Object pools for performance
- Runtime component attachment

**Acceptance Criteria:**
- [ ] IComponent interface
- [ ] IComponentObject (contains components)
- [ ] IComponentFactory (creates components)
- [ ] ComponentFactoryRegistry (central registry)
- [ ] StaticPooledComponentFactory (pre-allocated pools)

**Implementation:**
- `DiaCore/Architecture/Components/IComponent.h`
- `DiaCore/Architecture/Components/IComponentObject.h`
- `DiaCore/Architecture/Components/IComponentFactory.h`
- `DiaCore/Architecture/Components/ComponentFactoryRegistry.h`
- `DiaCore/Architecture/Components/StaticPooledComponentFactory.h`

**Test Cases:**
- Component creation
- Component attachment/detachment
- Factory registration
- Pool allocation

**Status:** ✅ Complete

---

### DR-005: Singleton Pattern ✅ P1

**Requirement:**
Explicit-lifetime singleton with controlled initialization.

**Rationale:**
- Avoid static initialization order fiasco
- Explicit construction/destruction
- Thread-safe access

**Acceptance Criteria:**
- [ ] Singleton<T> template
- [ ] Explicit Create/Destroy methods
- [ ] Instance() accessor
- [ ] Asserts if accessed before creation

**Implementation:**
- `DiaCore/Architecture/Singleton/Singleton.h`

**Test Cases:**
- Create/Destroy lifecycle
- Instance access
- Access before creation (assertion)

**Status:** ✅ Complete

---

### DR-006: Observer Pattern ✅ P1

**Requirement:**
Thread-safe observer pattern for event notifications.

**Rationale:**
- Decoupled event notifications
- Support cross-thread events
- Type-safe callbacks

**Acceptance Criteria:**
- [ ] Observer class (subscribes to subject)
- [ ] ObserverSubject class (notifies observers)
- [ ] Thread-safe (uses mutex)
- [ ] Automatic unsubscribe on destruction

**Implementation:**
- `DiaCore/Architecture/Observer.h`

**Test Cases:**
- Subscribe/unsubscribe
- Notification delivery
- Concurrent notifications (thread safety)

**Status:** ✅ Complete

---

## Application Framework (DiaApplication)

### DR-007: ProcessingUnit Pattern ✅ P0

**Requirement:**
High-level execution containers that own modules and phases.

**Rationale:**
- Organize application into logical units
- Support multi-threading (one PU per thread)
- Manage module and phase lifetimes

**Acceptance Criteria:**
- [ ] ProcessingUnit base class
- [ ] Owns modules and phases
- [ ] Manages module lifecycle
- [ ] Manages phase transitions
- [ ] Can run on separate thread

**Implementation:**
- `DiaApplication/ApplicationProcessingUnit.h`
- Derived classes for Main, Render, Sim threads

**Test Cases:**
- PU creation and destruction
- Module management
- Phase transitions
- Multi-threaded execution

**Status:** ✅ Complete

**[→ DiaApplication API](../reference/api/dia/application-api.md)**

---

### DR-008: Module Pattern ✅ P0

**Requirement:**
Functional units with explicit dependencies and lifecycle.

**Rationale:**
- Modular design for extensibility
- Explicit dependencies prevent initialization bugs
- Clear lifecycle (Start/Update/Stop)

**Acceptance Criteria:**
- [ ] Module base class
- [ ] Unique ID (StringCRC)
- [ ] Lifecycle methods (OnConstruct, OnStart, OnUpdate, OnStop, OnDestruct)
- [ ] Dependency declaration via AddDependency()

**Implementation:**
- `DiaApplication/ApplicationModule.h`

**Test Cases:**
- Module lifecycle
- Dependency resolution
- Module update order

**Status:** ✅ Complete

---

### DR-009: Phase Pattern ✅ P0

**Requirement:**
Execution stages with state machine transitions.

**Rationale:**
- Explicit application flow control
- Clear initialization/shutdown order
- Modules can persist across phases

**Acceptance Criteria:**
- [ ] Phase base class
- [ ] State machine (Construct → Start → Running → Stopping → Stop → Destruct)
- [ ] Owns modules
- [ ] Automatic dependency resolution
- [ ] Transition methods (TransitionTo, QueuePhaseTransition)

**Implementation:**
- `DiaApplication/ApplicationPhase.h`

**Test Cases:**
- Phase transitions
- State machine enforcement
- Module management
- Invalid transition rejection

**Status:** ✅ Complete

---

### DR-010: FrameStream Communication ✅ P0

**Requirement:**
Thread-safe producer-consumer queue for cross-thread data transfer.

**Rationale:**
- Safe cross-thread communication
- Lock-free or mutex-protected
- FIFO ordering

**Acceptance Criteria:**
- [ ] FrameStream<T> template
- [ ] Write() method (producer)
- [ ] Read() method (consumer)
- [ ] Thread-safe
- [ ] No data loss

**Implementation:**
- `DiaApplication/FrameStream.h`

**Test Cases:**
- Concurrent writes/reads
- FIFO ordering
- No data loss
- Stress test (high throughput)

**Status:** ✅ Complete

---

### DR-011: Level System ✅ P0

**Requirement:**
Pluggable level system with factory pattern.

**Rationale:**
- Runtime-swappable game states
- Modding support
- Clean separation of concerns

**Acceptance Criteria:**
- [ ] ILevel interface (Load, Unload)
- [ ] LevelFactory singleton
- [ ] Runtime level creation by name
- [ ] Level transitions

**Implementation:**
- `DiaApplication/ILevel.h`
- `DiaApplication/LevelFactory.h`

**Test Cases:**
- Level registration
- Level creation
- Level transitions
- Load/unload lifecycle

**Status:** ✅ Complete

---

## Math Library (DiaMaths)

### DR-012: Vector Math ✅ P0

**Requirement:**
2D/3D vector math for game development.

**Rationale:**
- Fundamental for position, direction, velocity
- Standard game math operations
- Performance-critical

**Acceptance Criteria:**
- [ ] Vector2D (x, y)
- [ ] Vector3D (x, y, z)
- [ ] Vector4D (x, y, z, w)
- [ ] Arithmetic operators (+, -, *, /)
- [ ] Dot product, cross product
- [ ] Magnitude, normalize
- [ ] Distance, lerp

**Implementation:**
- `DiaMaths/Vector/Vector2D.h`
- `DiaMaths/Vector/Vector3D.h`
- `DiaMaths/Vector/Vector4D.h`

**Test Cases:**
- All operations
- Edge cases (zero vector, normalization)
- Performance

**Status:** ✅ Complete

**[→ DiaMaths API](../reference/api/dia/maths-api.md)**

---

### DR-013: Matrix Math ✅ P0

**Requirement:**
2D/3D matrix math for transformations.

**Rationale:**
- Transformations (translate, rotate, scale)
- Camera matrices
- Hierarchical transforms

**Acceptance Criteria:**
- [ ] Matrix22, Matrix33, Matrix44
- [ ] Matrix multiplication
- [ ] Transpose, inverse
- [ ] Factory methods (Identity, Translation, Rotation, Scale)
- [ ] Transform vectors

**Implementation:**
- `DiaMaths/Matrix/Matrix22.h`
- `DiaMaths/Matrix/Matrix33.h`
- `DiaMaths/Matrix/Matrix44.h`

**Test Cases:**
- Matrix operations
- Transformation correctness
- Inverse verification
- Performance

**Status:** ✅ Complete

---

### DR-014: Transform Hierarchies ⚠️ P1

**Requirement:**
Hierarchical transforms for parent/child relationships.

**Rationale:**
- Scene graphs
- Skeletal animation
- Relative positioning

**Acceptance Criteria:**
- [ ] Transform2D, Transform3D
- [ ] Parent/child relationships
- [ ] Local and world matrices
- [ ] Efficient hierarchy traversal

**Implementation:**
- `DiaMaths/Transform/Transform2D.h`
- `DiaMaths/Transform/Transform3D.h`

**Known Issue:**
- GetWorldMatrix() slow (no caching, traverses hierarchy every call)

**Test Cases:**
- Parent/child hierarchy
- Local/world position
- Hierarchy traversal
- Performance (large hierarchies)

**Status:** ⚠️ Blocked (performance issue, needs optimization)

**[→ Transform Performance Notes](../reference/subsystems/dia-maths/performance-notes.md)**

---

### DR-015: Collision Shapes ✅ P1

**Requirement:**
Basic collision shapes for intersection testing.

**Rationale:**
- Game physics and collision detection
- Spatial queries
- Foundational for physics system

**Acceptance Criteria:**
- [ ] Circle
- [ ] AABB (axis-aligned bounding box)
- [ ] Line
- [ ] Polygon
- [ ] Intersection tests (circle-circle, AABB-AABB, etc.)

**Implementation:**
- `DiaMaths/Shape/Circle.h`
- `DiaMaths/Shape/AABB.h`
- `DiaMaths/Shape/Line.h`
- `DiaMaths/Shape/Polygon.h`

**Test Cases:**
- Intersection tests
- Edge cases (touching, overlapping, separate)
- Performance

**Status:** ✅ Complete

---

### DR-016: Thread-Safe Random ✅ P1

**Requirement:**
Thread-safe random number generation.

**Rationale:**
- RNG used across all threads
- Non-thread-safe RNG causes race conditions
- Predictable seeding for testing

**Acceptance Criteria:**
- [ ] Random class
- [ ] Thread-safe (uses std::mutex)
- [ ] RandomFloat, RandomInt, RandomBool methods
- [ ] Seedable

**Implementation:**
- `DiaMaths/Core/Random.h`

**Test Cases:**
- Concurrent RNG calls
- Seeding
- Distribution verification

**Status:** ✅ Complete (fixed 2026-03)

**[→ Thread Safety Notes](../reference/subsystems/dia-maths/thread-safety-notes.md)**

---

## Graphics Abstraction (DiaGraphics)

### DR-017: Abstract Canvas ✅ P0

**Requirement:**
Platform-agnostic rendering interface.

**Rationale:**
- Backend can be swapped (SFML, SDL, Direct3D)
- Core game logic independent of rendering
- Easy to test (mock canvas)

**Acceptance Criteria:**
- [ ] ICanvas interface
- [ ] DrawLine, DrawCircle, DrawRectangle, DrawSprite, DrawText
- [ ] Clear and Present for frame control
- [ ] Color struct (RGBA)

**Implementation:**
- `DiaGraphics/Interface/ICanvas.h`
- `DiaGraphics/Interface/ICanvas.cpp` (stub implementations)

**Test Cases:**
- All drawing methods
- Color and transparency
- Backend swap (compile test)

**Status:** ✅ Complete

**[→ DiaGraphics API](../reference/api/dia/graphics-api.md)**

---

### DR-018: Frame Data Structure ✅ P1

**Requirement:**
Frame data structure for renderer configuration.

**Rationale:**
- Encapsulate frame state
- Support view transforms
- Background color configuration

**Acceptance Criteria:**
- [ ] Frame struct
- [ ] Background color
- [ ] View transform (Matrix33)

**Implementation:**
- `DiaGraphics/Interface/Frame.h`

**Test Cases:**
- Frame creation
- Transform application

**Status:** ✅ Complete

---

## Input Abstraction (DiaInput)

### DR-019: Input Events ✅ P0

**Requirement:**
Platform-agnostic input event system.

**Rationale:**
- Input backend can be swapped
- Consistent input handling
- Easy to test (mock input)

**Acceptance Criteria:**
- [ ] InputEvent struct
- [ ] Event types (keyboard, mouse, gamepad, text)
- [ ] KeyCode enum (platform-agnostic)
- [ ] MouseButton enum

**Implementation:**
- `DiaInput/DiaInputEvent.h`
- `DiaInput/DiaInputKeyCode.h`
- `DiaInput/DiaInputMouseButton.h`

**Test Cases:**
- Event structure
- Enum completeness
- Backend conversion (SFML → DiaInput)

**Status:** ✅ Complete

**[→ DiaInput API](../reference/api/dia/input-api.md)**

---

### DR-020: Input Source Manager ✅ P1

**Requirement:**
Manage multiple input sources (keyboard, mouse, gamepad).

**Rationale:**
- Support multiple input devices
- Unified polling interface
- Backend abstraction

**Acceptance Criteria:**
- [ ] InputSourceManager class
- [ ] Add/remove sources
- [ ] PollEvent() unified polling
- [ ] IInputSource interface

**Implementation:**
- `DiaInput/DiaInputInputSourceManager.h`
- `DiaInput/IInputSource.h`

**Test Cases:**
- Multiple sources
- Polling
- Source add/remove

**Status:** ✅ Complete

---

## UI Abstraction (DiaUI)

### DR-021: UI System Interface ⚠️ P1

**Requirement:**
Platform-agnostic UI system for HTML/CSS/JS or native UI.

**Rationale:**
- UI backend can be swapped (Awesomium, CEF, ImGui)
- Core logic independent of UI
- Support for modding

**Acceptance Criteria:**
- [ ] IUISystem interface
- [ ] Initialize, Update, Shutdown methods
- [ ] LoadUI, ExecuteJavaScript, BindCallback methods
- [ ] Show/Hide visibility control

**Implementation:**
- `DiaUI/Interface/IUISystem.h`

**Test Cases:**
- UI lifecycle
- JavaScript execution
- Callback binding
- Backend swap (compile test)

**Status:** ⚠️ Blocked (Awesomium deprecated, needs replacement)

**[→ DiaUI API](../reference/api/dia/ui-api.md)**

---

## Window Abstraction (DiaWindow)

### DR-022: Window Interface ✅ P0

**Requirement:**
Platform-agnostic window management.

**Rationale:**
- Window backend can be swapped
- Consistent window API
- Fullscreen, VSync control

**Acceptance Criteria:**
- [ ] IWindow interface
- [ ] Create, Close, Display, Clear methods
- [ ] Fullscreen toggle
- [ ] VSync control
- [ ] Frame rate limiting

**Implementation:**
- `DiaWindow/Interface/IWindow.h`

**Test Cases:**
- Window lifecycle
- Fullscreen toggle
- VSync control
- Backend swap (compile test)

**Status:** ✅ Complete

**[→ DiaWindow API](../reference/api/dia/window-api.md)**

---

## Backend Implementation (DiaSFML)

### DR-023: SFML Backend ✅ P0

**Requirement:**
SFML implementation of graphics, window, input, and audio.

**Rationale:**
- Cross-platform (Windows, Linux, macOS)
- Mature library (2.5.1)
- Comprehensive features

**Acceptance Criteria:**
- [ ] DiaSFMLRenderWindow (ICanvas + IWindow)
- [ ] DiaSFMLInputSource (IInputSource)
- [ ] DiaSFMLSoundManager (audio)

**Implementation:**
- `DiaSFML/DiaSFMLRenderWindow.h`
- `DiaSFML/DiaSFMLInputSource.h`
- `DiaSFML/DiaSFMLSoundManager.h`

**Test Cases:**
- All backend operations
- SFML → Dia conversions
- Cross-platform build (Windows, Linux)

**Status:** ✅ Complete

**[→ DiaSFML API](../reference/api/dia/sfml-api.md)**

---

## File I/O (DiaCore/FilePath)

### DR-024: Path Aliases ✅ P1

**Requirement:**
Path alias system for relocatable file paths.

**Rationale:**
- Paths relocatable (change once, applies everywhere)
- Support modding (custom asset paths)
- Platform-independent paths

**Acceptance Criteria:**
- [ ] Path class with aliases (StringCRC)
- [ ] FilePath late-bound path resolution
- [ ] Path registration at startup
- [ ] Path normalization (clean slashes)

**Implementation:**
- `DiaCore/FilePath/Path.h`
- `DiaCore/FilePath/FilePath.h`

**Test Cases:**
- Path registration
- Path resolution
- Path normalization

**Status:** ✅ Complete

**[→ IO API](../reference/api/dia/io-api.md)**

---

### DR-025: File Loading ✅ P1

**Requirement:**
Synchronous file loading with error handling.

**Rationale:**
- Load assets at runtime
- Error handling (missing files)
- Cross-platform (abstract file I/O)

**Acceptance Criteria:**
- [ ] IFileLoad interface
- [ ] LoadNow() synchronous loading
- [ ] Return codes (Success, FileNotFound, BufferTooSmall, ReadError)
- [ ] LoadAsync() (stub/future)

**Implementation:**
- `DiaCore/FilePath/IFileLoad.h`
- `DiaCore/FilePath/FileLoad.h`

**Test Cases:**
- Load existing file
- Load missing file
- Buffer too small
- Error handling

**Status:** ✅ Complete (synchronous only, async stub)

---

## Future Subsystems

### DR-026: Physics Engine ❌ P2

**Requirement:**
Physics simulation with collision detection and response.

**Rationale:**
- Common game requirement
- Performance-critical
- Complex to implement from scratch

**Acceptance Criteria:**
- [ ] IPhysicsWorld interface
- [ ] Rigid body dynamics
- [ ] Collision detection (2D minimum)
- [ ] Backend options (Box2D, Bullet, PhysX)

**Implementation:**
- Not yet implemented (stub)

**Status:** ❌ Not Started (P2 priority)

**[→ Physics API Stub](../reference/api/dia/physics-api.md)**

---

### DR-027: AI/Pathfinding ❌ P2

**Requirement:**
AI utilities (pathfinding, steering, FSM, behavior trees).

**Rationale:**
- Common game requirement
- Complex to implement from scratch
- Reusable across projects

**Acceptance Criteria:**
- [ ] A* pathfinding
- [ ] Navigation mesh (optional)
- [ ] Steering behaviors
- [ ] FSM or Behavior Tree

**Implementation:**
- Not yet implemented (stub)

**Status:** ❌ Not Started (P2 priority)

**[→ AI API Stub](../reference/api/dia/ai-api.md)**

---

## Summary

**Total Dia Requirements:** 27

**Status Breakdown:**
- ✅ Complete: 22 (81%)
- ⚠️ Blocked: 2 (7%)
- ❌ Not Started: 2 (7%)
- 🚧 In Progress: 1 (4%)

**Priority Breakdown:**
- P0 (Critical): 14
- P1 (High): 11
- P2 (Medium): 2

**Key Gaps:**
- DR-014: Transform hierarchies (performance issue)
- DR-021: UI system (Awesomium deprecated)
- DR-026: Physics engine (not started)
- DR-027: AI/Pathfinding (not started)

**Next Steps:**
1. Fix Transform2D performance (DR-014)
2. Replace Awesomium UI backend (DR-021)
3. Plan physics integration (DR-026)
4. Plan AI integration (DR-027)

**[→ Main Requirements](requirements.md)**  
**[→ Cluiche Requirements](cluiche-requirements.md)**  
**[→ Functional Requirements](functional-requirements.md)**

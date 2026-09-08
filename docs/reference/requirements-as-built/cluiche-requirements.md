# Cluiche Application Requirements

**Last Updated:** 2026-04-01

Application-specific requirements for the Cluiche game framework.

---

## Overview

This document details requirements specific to the Cluiche application layer (built on top of Dia engine).

**Scope:**
- `Cluiche/CluicheTest/` application code
- Application-level modules and phases
- Level implementations
- Application architecture

**Out of Scope:**
- Dia engine requirements (see [Dia Requirements](dia-requirements.md))
- Game-specific content (this is a framework demo)

**Related Documents:**
- **[→ Main Requirements](requirements.md)** - Complete requirements list
- **[→ Functional Requirements](functional-requirements.md)** - All functional requirements
- **[→ Dia Requirements](dia-requirements.md)** - Engine requirements

---

## Application Architecture

### CR-001: Three-Thread Architecture ✅ P0

**Requirement:**
Application must use three independent threads: Main, Render, Sim.

**Rationale:**
- Main thread: Window events, input, UI (non-blocking)
- Render thread: Graphics rendering (60 FPS, VSync-locked)
- Sim thread: Game logic (variable rate, as fast as possible)

**Acceptance Criteria:**
- [ ] MainProcessingUnit spawns Render and Sim threads
- [ ] Each ProcessingUnit runs on separate thread
- [ ] Threads synchronize via FrameStreams and mutexes
- [ ] Graceful shutdown (join all threads)

**Implementation:**
- `MainProcessingUnit` - Main thread
- `RenderProcessingUnit` - Render thread
- `SimProcessingUnit` - Sim thread

**Test Cases:**
- Thread creation at startup
- Concurrent execution verification
- Clean shutdown

**Status:** ✅ Complete

**[→ Threading Model](../architecture/threading-model.md)**

---

### CR-002: Module Composition ✅ P0

**Requirement:**
Each ProcessingUnit must compose modules for specific functionality.

**Rationale:**
- Modular design for extensibility
- Explicit dependencies prevent initialization bugs
- Reusable modules across processing units

**Acceptance Criteria:**
- [ ] Main thread has MainKernelModule, MainInputModule, MainUIModule, LevelFactoryModule
- [ ] Render thread has RenderKernelModule, RenderCanvasModule
- [ ] Sim thread has SimKernelModule, SimTimeServerModule, SimInputFrameStreamModule, SimUIProxyModule

**Implementation:**
- `ApplicationFlow/Modules/Main*.h` - Main thread modules
- `ApplicationFlow/Modules/Render*.h` - Render thread modules
- `ApplicationFlow/Modules/Sim*.h` - Sim thread modules

**Test Cases:**
- Module initialization order
- Dependency resolution
- Module lifecycle

**Status:** ✅ Complete

**[→ Cluiche Architecture](../architecture/cluichetest-application.md)**

---

### CR-003: Phase-Based Flow Control ✅ P1

**Requirement:**
Application flow controlled via explicit phases.

**Rationale:**
- Clear state machine for application lifecycle
- Phases can share modules (avoid reconstruction)
- Explicit transitions prevent undefined states

**Acceptance Criteria:**
- [ ] Main thread phases: MainBootPhase, MainBootStrapPhase, MainRunningPhase, MainShutdownPhase
- [ ] Render thread phases: RenderBootPhase, RenderRunningPhase, RenderShutdownPhase
- [ ] Sim thread phases: SimBootPhase, SimBootStrapPhase, SimRunningPhase, SimShutdownPhase

**Implementation:**
- `ApplicationFlow/Phases/Main*.h` - Main thread phases
- `ApplicationFlow/Phases/Render*.h` - Render thread phases
- `ApplicationFlow/Phases/Sim*.h` - Sim thread phases

**Test Cases:**
- Phase transitions (Boot → Running → Shutdown)
- Module persistence across phases
- Invalid transition rejection

**Status:** ✅ Complete

---

## Cross-Thread Communication

### CR-004: FrameStream Communication ✅ P0

**Requirement:**
Threads must communicate via FrameStreams for thread-safe data transfer.

**Rationale:**
- FrameStreams provide lock-free or mutex-protected queues
- Producer-consumer pattern
- No race conditions

**Acceptance Criteria:**
- [ ] Main → Sim: InputEvent (via SimInputFrameStreamModule)
- [ ] Sim → Main: UI updates (via SimUIProxyModule)
- [ ] Sim → Render: Frame data (conceptual, not yet implemented)

**Implementation:**
- `FrameStream<InputEvent>` - Input events
- UIProxy pattern for Sim → Main UI updates

**Test Cases:**
- Concurrent writes/reads
- No data loss
- FIFO ordering

**Status:** ✅ Complete

**[→ DiaApplicationFlow API](../api/dia/application-api.md)**

---

### CR-005: Thread-Safe UI Updates 🚧 P1

**Requirement:**
Sim thread must update UI on Main thread safely.

**Rationale:**
- UI APIs not thread-safe (must call from Main thread)
- Sim thread produces UI updates (scores, health)
- Proxy pattern queues commands for Main thread

**Acceptance Criteria:**
- [ ] SimUIProxyModule queues UI commands
- [ ] MainUIModule executes queued commands
- [ ] No UI calls from Sim thread directly

**Implementation:**
- `SimUIProxyModule` - Sim thread proxy
- Command pattern for UI updates

**Test Cases:**
- UI updates from Sim thread
- No direct UI calls from Sim
- UI updates appear correctly

**Status:** 🚧 In Progress (basic proxy exists, needs expansion)

---

## Level System

### CR-006: Level Factory ✅ P0

**Requirement:**
Support runtime level creation via factory pattern.

**Rationale:**
- Levels pluggable (add new without modifying core)
- Modding support
- Test levels for validation

**Acceptance Criteria:**
- [ ] LevelFactory singleton manages level creation
- [ ] Levels registered at startup
- [ ] Can create levels by StringCRC name
- [ ] Can transition between levels

**Implementation:**
- `LevelFactory` - Singleton factory
- `ILevel` - Level interface
- `DummyStage`, `UnitTestLevel` - Example levels

**Test Cases:**
- Level registration
- Level creation by name
- Level transitions
- Unknown level handling

**Status:** ✅ Complete

**[→ Level System](../architecture/level-system.md)**

---

### CR-007: Level Lifecycle ✅ P0

**Requirement:**
Levels must have clear Load/Unload lifecycle.

**Rationale:**
- Resource management (load assets, unload when done)
- Clean state transitions
- No memory leaks

**Acceptance Criteria:**
- [ ] Levels implement ILevel interface
- [ ] Load() called before level active
- [ ] Unload() called after level inactive
- [ ] Previous level fully unloaded before next loads

**Implementation:**
- `ILevel::Load()` - Load assets, initialize state
- `ILevel::Unload()` - Cleanup, free resources

**Test Cases:**
- Level load/unload
- Multiple level transitions
- Memory leak detection (load/unload 100 times)

**Status:** ✅ Complete

---

### CR-008: Example Levels ✅ P1

**Requirement:**
Provide example levels demonstrating framework capabilities.

**Rationale:**
- Serves as documentation
- Validates framework design
- Starting point for new developers

**Acceptance Criteria:**
- [ ] DummyStage - Minimal level (baseline)
- [ ] UnitTestLevel - Automated testing harness
- [ ] Example gameplay level (future)

**Implementation:**
- `Stages/DummyStage/` - Minimal level
- `Levels/UnitTestLevel/` - Test harness

**Test Cases:**
- Load each example level
- Verify expected behavior

**Status:** ✅ Complete (DummyStage, UnitTestLevel exist)

---

## Input Handling

### CR-009: Input Pipeline ✅ P0

**Requirement:**
Input flow: Main (poll) → Sim (process) pipeline.

**Rationale:**
- Input polled on Main thread (SFML requirement)
- Game logic processes input on Sim thread
- Decouples input source from processing

**Acceptance Criteria:**
- [ ] MainInputModule polls input from SFML
- [ ] Input forwarded to Sim via FrameStream
- [ ] SimInputFrameStreamModule reads input
- [ ] Game logic processes input events

**Implementation:**
- `MainInputModule` - Polls input
- `SimInputFrameStreamModule` - Reads input
- `FrameStream<InputEvent>` - Transport

**Test Cases:**
- Keyboard input forwarding
- Mouse input forwarding
- Input processing on Sim thread

**Status:** ✅ Complete

---

### CR-010: Input State Tracking 🚧 P2

**Requirement:**
Track input state for continuous input (e.g., held keys).

**Rationale:**
- Events only fire on press/release
- Game logic needs "is key down?" queries
- Simplifies movement input

**Acceptance Criteria:**
- [ ] InputState class tracks key/mouse state
- [ ] IsKeyDown() queries
- [ ] IsMouseButtonDown() queries
- [ ] GetMousePosition() queries

**Implementation:**
- `InputState` class (planned)
- ProcessEvent() updates state
- Query methods for game logic

**Test Cases:**
- Key press/release state tracking
- Mouse button state tracking
- Mouse position tracking

**Status:** 🚧 In Progress (pattern documented, not implemented)

**[→ Input API](../api/dia/input-api.md)**

---

## Rendering

### CR-011: Render Loop ✅ P0

**Requirement:**
Render thread runs independent loop at 60 FPS.

**Rationale:**
- Consistent frame rate (VSync-locked)
- Decouples rendering from simulation
- Non-blocking UI

**Acceptance Criteria:**
- [ ] Render thread runs at 60 FPS
- [ ] Clear → Draw → Present pattern
- [ ] VSync enabled by default
- [ ] No render thread blocking

**Implementation:**
- `RenderProcessingUnit` - Render loop
- `RenderCanvasModule` - Drawing logic
- `DiaSFMLRenderWindow` - Backend

**Test Cases:**
- Frame rate measurement
- VSync verification
- Frame time profiling

**Status:** ✅ Complete

---

### CR-012: Debug Rendering 🚧 P2

**Requirement:**
Support debug rendering (FPS, bounding boxes, etc.).

**Rationale:**
- Essential for development
- Debugging visual issues
- Performance monitoring

**Acceptance Criteria:**
- [ ] FPS counter
- [ ] Bounding box visualization
- [ ] Transform hierarchy visualization
- [ ] Toggle on/off (F-key)

**Implementation:**
- Debug rendering in RenderCanvasModule
- Conditional compilation (#ifdef DEBUG)

**Test Cases:**
- FPS counter accuracy
- Bounding box rendering
- Debug toggle

**Status:** 🚧 In Progress (FPS counter exists, needs expansion)

---

## UI Integration

### CR-013: Web-Based UI ⚠️ P1

**Requirement:**
Support web-based UI (HTML/CSS/JS).

**Rationale:**
- Rich UI capabilities (CSS, animations)
- Familiar web tech stack
- Modding support (edit HTML files)

**Acceptance Criteria:**
- [ ] Load HTML UI
- [ ] Execute JavaScript from C++
- [ ] Bind C++ callbacks to JavaScript
- [ ] Update UI from game state

**Implementation:**
- `MainUIModule` - UI management

**Test Cases:**
- Load UI
- Execute JavaScript
- Bind callbacks
- UI updates

**Status:** ⚠️ Needs UI backend implementation

**[→ UI API](../api/dia/ui-api.md)**

---

## Testing

### CR-014: UnitTestLevel ✅ P1

**Requirement:**
In-engine test harness for automated testing.

**Rationale:**
- Tests run in real environment
- Integration testing
- CI/CD support (future)

**Acceptance Criteria:**
- [ ] UnitTestLevel loads and runs tests
- [ ] Test results logged
- [ ] Failed tests break execution
- [ ] Can add new tests easily

**Implementation:**
- `UnitTestLevel` - Test harness level
- Tests registered at startup

**Test Cases:**
- Test registration
- Test execution
- Pass/fail reporting

**Status:** ✅ Complete

**[→ Testing Overview](../testing/test.md)**

---

### CR-015: Module Dependency Graph ❌ P2

**Requirement:**
Generate module dependency graph for debugging.

**Rationale:**
- Visualize complex dependencies
- Debug initialization order issues
- Documentation

**Acceptance Criteria:**
- [ ] Export graph to DOT format (Graphviz)
- [ ] Graph shows all modules and dependencies
- [ ] Can generate at runtime or build time

**Implementation:**
- `MainProcessingUnit::GenerateModuleDependecyGraph()` (partial)
- Export to file
- Render with Graphviz or `Tools/dia_modules.py`

**Test Cases:**
- Graph generation
- DOT format validation
- Graph rendering

**Status:** ❌ Not Started (partial implementation exists)

---

## Performance

### CR-016: 60 FPS Target ✅ P0

**Requirement:**
Maintain 60 FPS in typical gameplay scenarios.

**Rationale:**
- Smooth gameplay experience
- Industry standard
- User expectation

**Acceptance Criteria:**
- [ ] 60 FPS with 100 entities
- [ ] 60 FPS with UI active
- [ ] No frame drops during normal gameplay

**Measurement:**
- FPS counter
- Visual Studio Profiler
- Frame time histogram

**Implementation:**
- Optimized render loop
- Deferred rendering
- Object pooling

**Test Cases:**
- Empty scene
- 100 entities
- 1000 entities
- UI + entities

**Status:** ✅ Complete (60 FPS maintained)

---

## Summary

**Total Cluiche Requirements:** 16

**Status Breakdown:**
- ✅ Complete: 11 (69%)
- 🚧 In Progress: 3 (19%)
- ⚠️ Blocked: 1 (6%)
- ❌ Not Started: 1 (6%)

**Priority Breakdown:**
- P0 (Critical): 8
- P1 (High): 6
- P2 (Medium): 2

**Key Gaps:**
- CR-005: Thread-safe UI updates (needs expansion)
- CR-010: Input state tracking (pattern only)
- CR-012: Debug rendering (FPS only)
- CR-013: Web UI (needs backend implementation)
- CR-015: Module dependency graph (partial)

**Next Steps:**
1. Implement UI backend with CEF or ImGui (CR-013)
2. Expand debug rendering (CR-012)
3. Complete module dependency graph (CR-015)
4. Implement input state tracking (CR-010)

**[→ Main Requirements](requirements.md)**  
**[→ Dia Requirements](dia-requirements.md)**  
**[→ Functional Requirements](functional-requirements.md)**

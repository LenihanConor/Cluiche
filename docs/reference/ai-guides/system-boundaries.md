# System Boundaries

**Last Updated:** 2026-04-01

Responsibility matrix for AI agents to understand what each subsystem owns and doesn't own.

---

## Overview

This document defines clear boundaries between subsystems to help AI agents:
- Know where functionality belongs
- Avoid introducing inappropriate dependencies
- Understand the separation of concerns

**Format:**
- ✅ **Owns:** What this subsystem is responsible for
- ❌ **Doesn't Own:** What this subsystem explicitly does NOT handle

---

## CluicheTest Application Layer

### Cluiche/Main.cpp

**✅ Owns:**
- Application entry point (`main()`)
- Creating MainProcessingUnit
- Running main loop
- Top-level exception handling

**❌ Doesn't Own:**
- Threading logic (delegated to MainProcessingUnit)
- Module initialization (delegated to Phases)
- UI rendering (delegated to RenderProcessingUnit)
- Game logic (delegated to SimProcessingUnit)

---

### MainProcessingUnit

**✅ Owns:**
- Main thread orchestration
- Spawning Render and Sim threads
- UI thread management (Awesomium requires main thread)
- Phase transitions for Main thread

**❌ Doesn't Own:**
- Rendering (RenderProcessingUnit)
- Game logic (SimProcessingUnit)
- Specific modules (delegated to Phases)

---

### RenderProcessingUnit

**✅ Owns:**
- Render thread execution
- 60 FPS frame rate enforcement (TimeThreadLimiter)
- Graphics rendering
- Window presentation
- Phase transitions for Render thread

**❌ Doesn't Own:**
- Game logic (SimProcessingUnit)
- Input handling (Main thread)
- UI events (Main thread)

---

### SimProcessingUnit

**✅ Owns:**
- Sim thread execution
- Game logic updates
- Variable rate execution
- Phase transitions for Sim thread

**❌ Doesn't Own:**
- Rendering (RenderProcessingUnit)
- UI management (Main thread)
- Frame rate limiting (runs as fast as possible)

---

### Levels (DummyLevel, UnitTestLevel, etc.)

**✅ Owns:**
- Level-specific behavior
- Level state management
- Level initialization and cleanup

**❌ Doesn't Own:**
- Level registration (LevelFactoryModule)
- Level creation (LevelFactory)
- Framework lifecycle (ILevel interface defines contract)

---

### Modules

#### MainKernelModule

**✅ Owns:**
- Main thread entry point for phases
- Coordinating Main thread initialization

**❌ Doesn't Own:**
- Specific functionality (other modules provide that)

---

#### MainUIModule

**✅ Owns:**
- UI system initialization (Awesomium)
- Loading HTML pages
- Binding C++ methods to JavaScript
- Notifying observers when UI ready

**❌ Doesn't Own:**
- HTML/CSS/JavaScript content (External/Webix/)
- Cross-thread UI communication (SimUIProxyModule)
- UI rendering (Awesomium handles that)

---

#### LevelFactoryModule

**✅ Owns:**
- Registering all available levels
- LevelFactory initialization

**❌ Doesn't Own:**
- Level implementation (individual level files)
- Level creation logic (LevelFactory)
- Level lifecycle (Phase manages that)

---

#### SimTimeServerModule

**✅ Owns:**
- Time source for Sim thread
- Delta time calculation
- TimeServer initialization for Sim thread

**❌ Doesn't Own:**
- Time tracking implementation (DiaCore/Time/)
- Frame rate limiting (TimeThreadLimiter)

---

#### SimInputFrameStreamModule

**✅ Owns:**
- Forwarding input events from Main to Sim thread
- Reading from input FrameStream

**❌ Doesn't Own:**
- Input event generation (Main thread / SFML)
- Input processing (game logic in Sim thread)

---

#### SimUIProxyModule

**✅ Owns:**
- Proxying UI messages from Sim to Main thread
- Thread-safe communication with MainUIModule
- Observer pattern (observes MainUIModule)

**❌ Doesn't Own:**
- UI implementation (MainUIModule)
- UI rendering (Awesomium)

---

## Dia Engine Layer

### DiaApplication

**✅ Owns:**
- ProcessingUnit base class
- Phase base class
- Module base class
- StateObject base class
- ILevel interface
- LevelFactory
- FrameStream (cross-thread communication)

**❌ Doesn't Own:**
- Specific application logic (Cluiche)
- Platform-specific code (DiaSFML)
- Containers (DiaCore)
- Math (DiaMaths)

**Dependencies:**
- DiaCore (containers, type system, time)

---

### DiaCore

**✅ Owns:**
- Custom containers (DynamicArray, HashTable, LinkList, etc.)
- Design patterns (Singleton, Factory, Observer)
- Type system (StringCRC, TypeRegistry)
- Component system (IComponent, factories)
- Time tracking (TimeServer, TimeAbsolute, TimeRelative)
- Assertions and logging
- File I/O (FilePath)
- JSON parsing wrapper

**❌ Doesn't Own:**
- Application framework (DiaApplication)
- Math operations (DiaMaths)
- Graphics (DiaGraphics)
- Platform-specific code (DiaSFML)

**Dependencies:**
- jsoncpp (external, wrapped)

---

### DiaMaths

**✅ Owns:**
- Vector math (Vector2D, Vector3D, Vector4D)
- Matrix math (Matrix22, Matrix33, Matrix44)
- Transform hierarchies (Transform2D, Transform3D)
- Shapes (Circle, AABB, Line, Polygon)
- Interpolation (Lerp, InverseLerp, MoveTowards, etc.)
- Random number generation (thread-safe)
- Float utilities

**❌ Doesn't Own:**
- Rendering (DiaGraphics)
- Physics simulation (DiaPhysics)
- Collision resolution (DiaPhysics)
- Game logic (Cluiche)

**Dependencies:**
- DiaCore (assertions, utilities)

**Known Issues:**
- Template bugs (InverseLerp, MoveTowards)
- Transform2D performance (no caching)

---

### DiaGraphics

**✅ Owns:**
- Platform-agnostic rendering interface (ICanvas)
- Frame data structures
- Drawing primitives (lines, circles, sprites)

**❌ Doesn't Own:**
- Platform-specific rendering (DiaSFML)
- Window management (DiaWindow)
- Math (DiaMaths)

**Dependencies:**
- DiaMaths (Vector2D, Transform2D)
- DiaCore (containers)

---

### DiaWindow

**✅ Owns:**
- Platform-agnostic window interface (IWindow)
- Window creation, resizing, closing

**❌ Doesn't Own:**
- Platform-specific windowing (DiaSFML)
- Rendering (DiaGraphics)
- Input handling (DiaInput)

**Dependencies:**
- DiaCore

---

### DiaInput

**✅ Owns:**
- Platform-agnostic input events
- InputEvent structure (keyboard, mouse, gamepad)
- InputSourceManager

**❌ Doesn't Own:**
- Platform-specific input capture (DiaSFML)
- Input processing (game logic)
- UI input (DiaUI)

**Dependencies:**
- DiaCore

---

### DiaUI

**✅ Owns:**
- Platform-agnostic UI interface (IUISystem)

**❌ Doesn't Own:**
- UI implementation (DiaUIAwesomium)
- HTML/CSS/JavaScript content (External)
- UI rendering (Awesomium)

**Dependencies:**
- DiaCore

---

### DiaSFML

**✅ Owns:**
- SFML integration
- ICanvas implementation (DiaSFMLRenderWindow)
- IWindow implementation
- Input event translation (SFML → DiaInput)
- Audio playback (DiaSFMLSoundManager)

**❌ Doesn't Own:**
- Application framework (DiaApplication)
- Game logic (Cluiche)
- UI (DiaUI)

**Dependencies:**
- SFML (external)
- DiaGraphics (implements ICanvas)
- DiaWindow (implements IWindow)
- DiaInput (translates to InputEvent)
- DiaCore

---

### DiaUIAwesomium

**✅ Owns:**
- Awesomium integration
- IUISystem implementation
- C++ ↔ JavaScript binding
- Web page rendering

**❌ Doesn't Own:**
- HTML/CSS/JavaScript content (External/Webix/)
- Application logic (Cluiche)
- Cross-thread communication (SimUIProxyModule)

**Dependencies:**
- Awesomium SDK (external, **DEPRECATED**)
- DiaUI (implements IUISystem)
- DiaCore

**Status:** **DEPRECATED** - Should be replaced with CEF or ImGui

---

### DiaIO

**✅ Owns:**
- File path manipulation
- File reading/writing
- Directory traversal

**❌ Doesn't Own:**
- Asset loading (not implemented yet)
- Serialization (DiaCore/Json/)

**Dependencies:**
- DiaCore

---

### DiaPhysics

**✅ Owns:**
- Physics simulation (stub)
- Collision detection (future)
- Rigid body dynamics (future)

**❌ Doesn't Own:**
- Math operations (DiaMaths)
- Rendering (DiaGraphics)

**Dependencies:**
- DiaMaths
- DiaCore

**Status:** **STUB** - Minimal implementation

---

### DiaAI

**✅ Owns:**
- AI pathfinding (stub)
- Behavior trees (future)
- Navigation meshes (future)

**❌ Doesn't Own:**
- Math operations (DiaMaths)
- Game logic (Cluiche)

**Dependencies:**
- DiaMaths
- DiaCore

**Status:** **STUB** - Minimal implementation

---

## External Dependencies

### SFML

**✅ Owns:**
- Window creation
- OpenGL context
- Input capture (keyboard, mouse, joystick)
- Audio playback
- Image loading
- Basic 2D rendering

**❌ Doesn't Own:**
- Application architecture (DiaApplication)
- Custom containers (DiaCore)
- Game logic (Cluiche)

**Used By:** DiaSFML

---

### JsonCpp

**✅ Owns:**
- JSON parsing
- JSON serialization

**❌ Doesn't Own:**
- Type reflection (DiaCore/Type/)
- File I/O (DiaIO)

**Used By:** DiaCore/Json/ (wrapper)

---

### Awesomium

**✅ Owns:**
- HTML rendering
- CSS layout
- JavaScript execution
- C++ ↔ JavaScript binding

**❌ Doesn't Own:**
- Application logic (Cluiche)
- HTML/CSS/JavaScript content (External/Webix/)

**Used By:** DiaUIAwesomium

**Status:** **DEPRECATED** - No longer maintained

---

### Webix

**✅ Owns:**
- Web UI components (JavaScript library)
- Dashboard layouts
- Data grids, charts, etc.

**❌ Doesn't Own:**
- C++ integration (Awesomium)
- Application logic (Cluiche)

**Used By:** MainUIModule (loads Webix pages via Awesomium)

---

### VisJS

**✅ Owns:**
- Graph visualization (JavaScript library)
- Network diagrams

**❌ Doesn't Own:**
- C++ integration (Awesomium)
- Dependency analysis logic (Tools/dia_modules.py)

**Used By:** Blue Console (Tools/Console/)

---

## Cross-Cutting Concerns

### Threading

**✅ Owned By:**
- ProcessingUnit (thread lifecycle)
- FrameStream (cross-thread communication)
- std::mutex (synchronization primitives)

**❌ Not Owned By Any Subsystem:**
- Global thread management (each ProcessingUnit manages its own)

---

### Memory Management

**✅ Owned By:**
- DiaCore/Memory/ (utilities)
- Each container (DynamicArray, HashTable manage their own)

**❌ Not Owned By Any Subsystem:**
- Global allocator (uses C++ new/delete)
- Memory pooling (StaticPooledComponentFactory is per-component)

---

### Logging

**✅ Owned By:**
- DiaCore/Core/Log.h (DIA_LOG macro)

**❌ Not Owned By Any Subsystem:**
- Log output destination (hardcoded to stdout/debugger)

---

### Error Handling

**✅ Owned By:**
- DiaCore/Core/Assert.h (DIA_ASSERT macro)

**❌ Not Owned By Any Subsystem:**
- Exception handling (minimal usage, prefer assertions)

---

## Dependency Rules

### Allowed Dependencies

```
Cluiche → Dia (any subsystem)
Dia subsystems → DiaCore
Dia subsystems → DiaMaths
DiaSFML → DiaGraphics, DiaWindow, DiaInput
DiaUIAwesomium → DiaUI
DiaApplication → DiaCore
```

### Forbidden Dependencies

```
❌ DiaCore → DiaApplication (layer violation)
❌ DiaCore → DiaMaths (layer violation)
❌ DiaMaths → DiaGraphics (separation of concerns)
❌ DiaGraphics → DiaSFML (abstraction violation)
❌ DiaApplication → Cluiche (engine shouldn't know app)
```

---

## Responsibility Summary

| Subsystem | Primary Responsibility |
|-----------|------------------------|
| **Cluiche** | Application logic, threading orchestration, levels |
| **DiaApplication** | Application framework (PU/Phase/Module pattern) |
| **DiaCore** | Foundation (containers, patterns, type system) |
| **DiaMaths** | Math operations (vectors, matrices, transforms) |
| **DiaGraphics** | Rendering abstraction |
| **DiaWindow** | Window abstraction |
| **DiaInput** | Input abstraction |
| **DiaUI** | UI abstraction |
| **DiaSFML** | SFML backend implementation |
| **DiaUIAwesomium** | Awesomium UI implementation |
| **DiaIO** | File I/O |
| **DiaPhysics** | Physics simulation (stub) |
| **DiaAI** | AI systems (stub) |

---

## Common Mistakes to Avoid

### Mistake 1: Adding Math to DiaCore

**❌ Wrong:**
```cpp
// Dia/DiaCore/Utilities/MathUtils.h
namespace Dia::Core {
    float Distance(Vector2D a, Vector2D b);  // NO!
}
```

**✅ Correct:**
```cpp
// Dia/DiaMaths/Core/FloatMaths.h
namespace Dia::Maths {
    float Distance(Vector2D a, Vector2D b);  // YES
}
```

**Why:** Math belongs in DiaMaths, not DiaCore.

---

### Mistake 2: Adding Application Logic to Dia

**❌ Wrong:**
```cpp
// Dia/DiaApplication/GameManager.h
class GameManager {  // NO! Game-specific
    void UpdatePlayer();
};
```

**✅ Correct:**
```cpp
// Cluiche/GameManager.h
class GameManager {  // YES! App-specific
    void UpdatePlayer();
};
```

**Why:** Dia is engine, Cluiche is application. Keep app logic in Cluiche.

---

### Mistake 3: Direct SFML Usage in Cluiche

**❌ Wrong:**
```cpp
// Cluiche/MyModule.cpp
#include <SFML/Graphics.hpp>  // NO!
sf::RenderWindow window;
```

**✅ Correct:**
```cpp
// Cluiche/MyModule.cpp
#include "DiaGraphics/Interface/ICanvas.h"  // YES!
Dia::Graphics::ICanvas* canvas = GetCanvas();
```

**Why:** Use abstraction layer (DiaGraphics), not SFML directly.

---

### Mistake 4: Awesomium on Non-Main Thread

**❌ Wrong:**
```cpp
// SimProcessingUnit.cpp
mUISystem->LoadPage("page.html");  // NO! Awesomium requires main thread
```

**✅ Correct:**
```cpp
// MainUIModule.cpp (Main thread)
mUISystem->LoadPage("page.html");  // YES!

// Or use SimUIProxyModule to send messages from Sim → Main
```

**Why:** Awesomium requires main thread, cannot be used from Sim/Render threads.

---

## Quick Reference

**Where does it go?**

| Feature | Subsystem |
|---------|-----------|
| New container | DiaCore/Containers/ |
| Math function | DiaMaths/Core/ |
| Rendering primitive | DiaGraphics/Interface/ |
| Input event | DiaInput/ |
| Threading logic | DiaApplication/ (ProcessingUnit/Phase) |
| Application module | Cluiche/CluicheKernel/ApplicationFlow/Modules/ |
| Game level | Cluiche/Levels/ |
| Platform implementation | DiaSFML/ |

**[→ Entry Points](entry-points.md)**  
**[→ Code Patterns](patterns-reference.md)**  
**[→ Back to AI Guide](AI-README.md)**

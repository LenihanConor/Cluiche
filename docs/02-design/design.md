# Design Philosophy

**Last Updated:** 2026-04-01

This document captures the "why" behind Cluiche and Dia's architectural decisions.

---

## Core Design Intent

### Why Does Cluiche Exist?

**Problem:** Building games is complex
- Threading is hard to get right
- Dependencies between systems are often hidden
- Initialization order is unclear
- Testing isolated systems is difficult
- Code becomes tightly coupled over time

**Solution:** Explicit, modular architecture
- Clear threading model (Main/Render/Sim)
- Explicit dependencies (no hidden coupling)
- Defined lifecycle (Start → Update → Stop)
- Testable modules (inject dependencies)
- Composition over inheritance

**Goal:** Make game development **predictable and maintainable**

---

## Design Principles

### 1. Explicit over Implicit

**Principle:** No hidden behaviors or magic

**Examples:**

✅ **Explicit Dependencies:**
```cpp
class MyModule : public Module {
public:
    MyModule() {
        // Dependencies declared upfront
        RegisterDependency(&GetDependency<TimeServerModule>());
        RegisterDependency(&GetDependency<InputModule>());
    }
};
```

❌ **Hidden Dependencies (Avoided):**
```cpp
class MyModule : public Module {
    void DoUpdate() {
        // Hidden dependency on global
        float time = GlobalTimeServer::GetTime();  // BAD
    }
};
```

✅ **Explicit Thread Boundaries:**
```cpp
// Clear: This runs on Main thread
class MainProcessingUnit : public ProcessingUnit { };

// Clear: This runs on Sim thread  
class SimProcessingUnit : public ProcessingUnit { };
```

❌ **Hidden Threading (Avoided):**
```cpp
// Unclear: Which thread does this run on?
void Update() {
    mData++;  // RACE CONDITION?
}
```

**Why:**
- Easier to understand code flow
- Easier to debug (no surprises)
- Easier to maintain (explicit contracts)

**[→ More details](why-module-phase-pu.md)**

---

### 2. Composition over Inheritance

**Principle:** Prefer aggregating modules over deep class hierarchies

**Examples:**

✅ **Composition (Modules):**
```cpp
class MainKernelModule : public Module {
    TimeServer mTimeServer;          // Composed
    InputSourceManager mInputManager; // Composed
    Window* mWindow;                  // Composed
};
```

❌ **Deep Inheritance (Avoided):**
```cpp
class GameObject : public Entity {};
class Actor : public GameObject {};
class Character : public Actor {};
class Player : public Character {};  // Deep hierarchy
```

✅ **Modular Phase:**
```cpp
class MyPhase : public Phase {
    MyPhase() {
        AddModule(&mModuleA);  // Compose functionality
        AddModule(&mModuleB);
        AddModule(&mModuleC);
    }
};
```

**Why:**
- More flexible (change composition at runtime)
- Easier to test (mock individual modules)
- Avoids fragile base class problem
- Clearer ownership

**Trade-off:** More boilerplate (module registration)

---

### 3. Thread Safety by Design

**Principle:** Threading built into architecture, not bolted on

**Examples:**

✅ **Separate Thread Contexts:**
```cpp
// Main thread has its own TimeServer
MainKernelModule::mTimeServer  // 30 Hz

// Sim thread has independent TimeServer
SimTimeServerModule::mTimeServer  // Variable Hz
```

✅ **Explicit Synchronization:**
```cpp
void SimUIProxyModule::SendMessage(const char* msg) {
    std::lock_guard<std::mutex> lock(mMutex);  // Explicit
    mUISystem->SendMessage(msg);
}
```

✅ **Thread-Safe Communication:**
```cpp
// FrameStream internally mutex-protected
simToRenderFrameStream->PushFrame(frameData);  // Safe
```

❌ **Unsafe Shared State (Avoided):**
```cpp
// BAD: Multiple threads accessing without protection
int gSharedCounter;
void Update() { gSharedCounter++; }  // RACE
```

**Why:**
- Prevents data races
- Makes concurrency explicit
- Easier to reason about threading
- Catches issues at compile time (where possible)

**[→ Threading design details](why-module-phase-pu.md)**

---

### 4. Fail Fast

**Principle:** Detect errors as early as possible

**Examples:**

✅ **Assertions:**
```cpp
DIA_ASSERT(index < mArray.Size());  // Debug catch
DIA_STATIC_ASSERT(sizeof(T) > 0);   // Compile-time catch
```

✅ **Type Safety:**
```cpp
template<typename T>
T* GetDependency();  // Compile-time type safety

// vs stringly-typed:
void* GetDependency(const char* name);  // Runtime error
```

✅ **Dependency Cycles Detected:**
```cpp
// Cycle: A depends on B, B depends on A
// Result: Assertion failure in debug builds
DIA_ASSERT(!"Circular dependency detected");
```

**Why:**
- Bugs caught during development (not in production)
- Easier to debug (closer to root cause)
- Safer codebase (fewer silent failures)

---

### 5. Locality

**Principle:** Keep related code together

**Examples:**

✅ **Level-Specific Code:**
```
Levels/DummyLevel/
├── DummyLevel.h/cpp           # Level logic
├── LevelFlow/Phases/          # Level phases
└── UI/                        # Level UI
```

✅ **Module Co-Location:**
```
DiaCore/
├── Containers/                # Container code
│   ├── Arrays/
│   ├── HashTables/
│   └── Graphs/
```

❌ **Scattered Code (Avoided):**
```
# BAD: Level code scattered
src/levels/dummy_level.cpp
include/levels/dummy_level.h
ui/pages/dummy_level.html
assets/levels/dummy/
```

**Why:**
- Easier to find related code
- Easier to understand system scope
- Easier to delete features (all in one place)

---

## Why Dia Engine?

### Problem: Game Engine Complexity

Traditional game engines have issues:
- **Hidden dependencies:** Systems tightly coupled
- **Global state:** Hard to test, hard to reason about
- **Unclear initialization:** Order matters but isn't explicit
- **Threading added later:** Bolted on, not designed in
- **Platform-specific:** Hard to port

### Solution: Dia's Approach

**1. Module/Phase/ProcessingUnit Pattern**
- Clear lifecycle (Start → Update → Stop)
- Explicit dependencies (topological sort)
- Thread boundaries explicit (ProcessingUnit per thread)

**[→ Why this pattern?](why-module-phase-pu.md)**

**2. Platform Abstraction**
- Core engine portable (DiaCore, DiaMaths)
- Backends pluggable (DiaSFML, can swap for SDL/Direct3D)
- No platform #ifdefs in core code

**[→ Why Dia vs existing engines?](why-dia.md)**

**3. Type Safety**
- Compile-time type IDs (StringCRC)
- No string-based lookups in hot paths
- Serialization built on types

**[→ Why custom type system?](why-type-system.md)**

---

## Why Three Threads?

### Problem: Single-Threaded Game Loop

Traditional approach:
```cpp
while (running) {
    processInput();    // Can block
    updateGame();      // Variable time
    renderGraphics();  // Must be consistent for smooth visuals
}
```

**Issues:**
- Slow simulation blocks rendering (choppy visuals)
- Awesomium requires main thread (UI blocks game loop)
- Can't utilize multi-core CPUs effectively

### Solution: Three Independent Threads

**Main Thread:** Bootstrap, UI coordination
- Awesomium **must** run on main thread (web engine requirement)
- Collects input, spawns worker threads
- Update rate: As needed (~30 Hz)

**Render Thread:** Graphics rendering
- Reads graphics commands from Sim
- Renders at consistent 60 FPS (TimeThreadLimiter)
- Never blocks on slow simulation

**Sim Thread:** Game logic, physics
- Reads input from Main
- Updates at variable rate (depends on complexity)
- Writes graphics commands to Render

**Benefits:**
- ✅ Consistent frame rate (Render independent of Sim)
- ✅ Non-blocking UI (Main thread available for Awesomium)
- ✅ Multi-core utilization (3 threads on 3+ cores)
- ✅ Clear separation of concerns

**Trade-offs:**
- ❌ Synchronization complexity (mutexes, frame streams)
- ❌ Potential data races (if not careful)
- ❌ Memory overhead (per-thread stacks, buffers)

**[→ Threading design deep dive](why-module-phase-pu.md)**

---

## Why Custom Containers?

### Problem: STL Hidden Costs

Standard Template Library (STL) issues:
- Hidden allocations (hard to track memory)
- Platform variations (subtle bugs)
- Debug iterators slow (can't ship with assertions)
- No control over growth strategy

### Solution: Custom Containers

**DiaCore provides:**
- `Array<T, N>` - Fixed-size (no allocation)
- `DynamicArray<T>` - Like std::vector but explicit
- `HashTable<K, V>` - Hash map with CRC keys
- `Graph<N, V, E, U>` - Graph data structure
- `LinkList<T>` - Doubly-linked list

**Benefits:**
- ✅ Explicit memory control (know when allocation happens)
- ✅ Platform portable (consistent behavior)
- ✅ Performance tuning (optimize for game usage)
- ✅ Debug visibility (custom assertions)

**Trade-offs:**
- ❌ Maintenance burden (we own the code)
- ❌ No std::algorithm compatibility
- ❌ Learning curve (different API than STL)

**Example:**
```cpp
// STL: Hidden allocation
std::vector<int> vec;
vec.push_back(1);  // May allocate (when?)

// Dia: Explicit allocation
DynamicArray<int> arr;
arr.Reserve(10);   // Explicit allocation
arr.PushBack(1);   // No allocation (reserved)
```

**Why Worth It:**
- Games need predictable performance
- Memory tracking is critical
- Platform portability matters

---

## Why Type System?

### Problem: Runtime Type Information (RTTI)

C++ RTTI issues:
- Runtime overhead (typeid, dynamic_cast)
- Platform-specific (name mangling varies)
- Limited (can't add metadata)
- No serialization support

### Solution: Compile-Time Type IDs

**StringCRC approach:**
```cpp
// Compile-time hash generation
static const StringCRC kTypeId("MyClass");

// Fast O(1) comparison (integer comparison)
if (obj->GetTypeId() == kTypeId) {
    // Type match
}
```

**Benefits:**
- ✅ Zero runtime overhead (compile-time hash)
- ✅ Fast comparison (integer equality)
- ✅ Serialization support (CRC stable across builds)
- ✅ Extensible (add metadata via TypeRegistry)

**Use Cases:**
- Component type identification
- Factory pattern keys
- Save/load systems
- Network serialization

**[→ Type system design](why-type-system.md)**

---

## Design Evolution

### Historical Context

**Phase 1: Single-Threaded (Early)**
- Simple main loop
- All systems updated sequentially
- Easy to understand, but limited

**Phase 2: Threading Added (Mid)**
- Separate render thread for consistent FPS
- Global state caused race conditions
- Hard to debug threading issues

**Phase 3: Module/Phase/ProcessingUnit (Current)**
- Explicit thread boundaries
- Modules with dependencies
- Phase-based lifecycle
- Testable, maintainable architecture

**[→ Historical decisions](historical-decisions.md)**

---

## Design Anti-Patterns Avoided

### 1. God Objects

**Anti-pattern:** One class does everything
```cpp
class GameManager {
    void UpdateInput();
    void UpdatePhysics();
    void UpdateGraphics();
    void UpdateAI();
    void UpdateUI();
    // ... 50 more methods
};
```

**Dia's Solution:** Module decomposition
```cpp
// Separate modules
InputModule
PhysicsModule
GraphicsModule
AIModule
UIModule
```

---

### 2. Singletons Everywhere

**Anti-pattern:** Everything is a singleton
```cpp
InputManager::Instance()
PhysicsManager::Instance()
GraphicsManager::Instance()
// Hard to test, hidden dependencies
```

**Dia's Solution:** Dependency injection
```cpp
class MyModule : public Module {
public:
    MyModule() {
        RegisterDependency(&GetDependency<InputModule>());
    }
};
```

**Note:** Some singletons still used (TimeServer, LevelFactory) where appropriate

---

### 3. Global Mutable State

**Anti-pattern:** Global variables everywhere
```cpp
int gScore = 0;
float gPlayerHealth = 100.0f;
// Any code can modify, hard to track
```

**Dia's Solution:** State ownership
```cpp
class GameState {
    int mScore;         // Owned by GameState
    float mHealth;      // Clear ownership
};
```

---

### 4. Stringly-Typed APIs

**Anti-pattern:** String-based lookups
```cpp
void* GetComponent(const char* name);  // Error-prone
```

**Dia's Solution:** Type-safe templates
```cpp
template<typename T>
T* GetDependency();  // Compile-time safety
```

---

## Future Design Direction

### Potential Improvements

**1. Entity Component System (ECS)**
- Current: Custom component system
- Future: Full ECS (entities are IDs, components are data)
- Benefits: Cache-friendly, data-oriented

**2. Job-Based Threading**
- Current: Dedicated threads (Main/Render/Sim)
- Future: Job system with thread pool
- Benefits: Better CPU utilization, scalable

**3. Data-Oriented Design**
- Current: Object-oriented (classes and modules)
- Future: Data-oriented (transform cache, component arrays)
- Benefits: Better cache locality, performance

**4. Lock-Free Data Structures**
- Current: std::mutex for synchronization
- Future: Lock-free queues, atomics
- Benefits: Reduced contention, better performance

**[→ Future directions](future-directions.md)**

---

## Design Trade-Offs

Every design decision involves trade-offs:

### Module/Phase/ProcessingUnit Pattern

**Benefits:**
- ✅ Explicit dependencies
- ✅ Clear lifecycle
- ✅ Testable modules

**Costs:**
- ❌ Boilerplate code (DoStart/DoUpdate/DoStop)
- ❌ Learning curve
- ❌ Overhead for simple systems

**Verdict:** Worth it for maintainability

---

### Custom Containers

**Benefits:**
- ✅ Explicit memory control
- ✅ Platform portable
- ✅ Performance tuning

**Costs:**
- ❌ Maintenance burden
- ❌ No std::algorithm
- ❌ Different API

**Verdict:** Worth it for games (predictable performance matters)

---

### Three-Thread Architecture

**Benefits:**
- ✅ Consistent frame rate
- ✅ Non-blocking UI
- ✅ Multi-core utilization

**Costs:**
- ❌ Synchronization complexity
- ❌ Memory overhead
- ❌ Potential race conditions

**Verdict:** Worth it for performance (but requires discipline)

---

## Design for Humans

### Code Should Be Readable

**Principle:** Code is read more than written

**Examples:**

✅ **Clear Names:**
```cpp
MainProcessingUnit  // Clear: Main thread
RenderProcessingUnit  // Clear: Render thread
```

❌ **Unclear Names:**
```cpp
PU1  // What is this?
PU2  // What does this do?
```

✅ **Self-Documenting:**
```cpp
void RegisterDependency(Module* dependency);  // Clear intent
```

❌ **Cryptic:**
```cpp
void RegDep(void* ptr);  // What?
```

---

### Architecture Should Be Discoverable

**Principle:** Easy to find what you're looking for

**Examples:**

✅ **Consistent Structure:**
```
Dia/<Subsystem>/<Subsystem><Class>.h
```

✅ **Co-Located:**
```
Levels/DummyLevel/
├── DummyLevel.h/cpp
├── Phases/
└── UI/
```

---

## Summary

Cluiche and Dia prioritize:

**Core Values:**
1. **Explicit over Implicit** - No hidden behaviors
2. **Composition over Inheritance** - Module aggregation
3. **Thread Safety by Design** - Threading built-in, not bolted on
4. **Fail Fast** - Detect errors early
5. **Locality** - Keep related code together

**Key Design Decisions:**
- **Module/Phase/ProcessingUnit** - Explicit lifecycle and dependencies
- **Three threads** - Main/Render/Sim for performance
- **Custom containers** - Explicit memory control
- **Type system** - Compile-time type IDs for performance

**Trade-Offs Accepted:**
- Boilerplate code for maintainability
- Maintenance burden for control
- Synchronization complexity for performance

**Result:** Predictable, maintainable game architecture

**[→ Why specific patterns?](why-module-phase-pu.md)**  
**[→ Why Dia engine?](why-dia.md)**  
**[→ Future improvements](future-directions.md)**

---

**[→ Back to Documentation Index](../README.md)**
# Future Directions

**Last Updated:** 2026-04-01

Planned improvements and evolution of the Cluiche and Dia architecture.

---

## Overview

This document outlines potential future improvements, ordered by priority and feasibility.

**Note:** These are possibilities, not commitments. Will evolve based on actual needs.

---

## High Priority (Should Do)

### 1. Fix DiaMaths Template Bugs

**Current Issue:**
```cpp
// Broken: Template specialization missing
float InverseLerp(const Vector2D& a, const Vector2D& b, const Vector2D& value);
// Doesn't compile!

Vector2D MoveTowards(const Vector2D& current, const Vector2D& target, float maxDelta);
// Wrong return type!
```

**Plan:**
- Fix InverseLerp template for vectors
- Fix MoveTowards return type
- Add unit tests for all template instantiations
- Document template patterns

**Benefits:**
- ✅ Correctness (bugs fixed)
- ✅ Usability (APIs work as expected)
- ✅ Confidence (tested)

**Effort:** Low (few days)

**Priority:** **P0** (correctness bug)

**[→ Bug details](../subsystems/dia-maths/known-issues.md)**

---

### 2. Implement UI Backend

**Current Status:**
- UI system abstraction exists (DiaUI/IUISystem)
- No active UI backend (Awesomium has been removed)

**Options:**

**Option A: CEF (Chromium Embedded Framework)**
- ✅ Modern Chromium
- ✅ Actively maintained
- ✅ Supports latest web standards
- ❌ Large binary size
- ❌ Complex integration

**Option B: WebView2 (Windows only)**
- ✅ Native Windows integration
- ✅ Smaller (uses system Chromium)
- ✅ Microsoft-supported
- ❌ Windows-only
- ❌ Requires Windows 10+

**Option C: ImGui**
- ✅ Lightweight (<100KB)
- ✅ Immediate-mode (simple)
- ✅ Great for tools
- ❌ Not web-based (different paradigm)
- ❌ Less flexible layouts

**Recommendation:** CEF for main UI, ImGui for dev tools

**Benefits:**
- ✅ Security (maintained)
- ✅ Modern web standards
- ✅ Future-proof

**Effort:** Medium-High (few weeks)

**Priority:** **P1** (required for UI functionality)

---

### 3. Optimize Transform2D Hierarchy

**Current Issue:**
```cpp
// GetWorldMatrix() traverses hierarchy multiple times
Matrix33 Transform2D::GetWorldMatrix() const {
    Matrix33 local = GetLocalMatrix();
    if (mParent) {
        return mParent->GetWorldMatrix() * local;  // Recursive (slow)
    }
    return local;
}

// Called multiple times per frame:
GetWorldPosition();  // Traverses hierarchy
GetWorldRotation();  // Traverses hierarchy again
GetWorldScale();     // Traverses hierarchy again!
```

**Plan:**
- Cache world transform
- Invalidate on change (dirty flag)
- Update once per frame

**Benefits:**
- ✅ Performance (single traversal)
- ✅ Scalability (deep hierarchies OK)

**Effort:** Low-Medium (few days)

**Priority:** **P1** (performance)

---

## Medium Priority (Nice to Have)

### 4. Entity Component System (ECS)

**Current:**
- Custom component system (IComponent, IComponentFactory)
- Object-oriented (pointers, vtables)
- Not cache-friendly

**Future:**
- Full ECS architecture
- Entities are IDs (integers)
- Components are data (arrays)
- Systems process components

**Example:**
```cpp
// Current (OOP)
class Transform : public IComponent {
    Vector3D position;
    Quaternion rotation;
};

// Future (ECS)
struct TransformComponent {
    Vector3D position;
    Quaternion rotation;
};

// Stored in contiguous array (cache-friendly)
Array<TransformComponent> transforms;

// System processes all transforms
class TransformSystem : public System {
    void Update() {
        for (TransformComponent& t : transforms) {
            // Process (cache-friendly iteration)
        }
    }
};
```

**Benefits:**
- ✅ Cache-friendly (better performance)
- ✅ Data-oriented (scalable)
- ✅ Parallelizable (systems independent)

**Challenges:**
- ❌ Major refactor (entire component system)
- ❌ Learning curve (new paradigm)
- ❌ Current system works (not broken)

**Effort:** High (months)

**Priority:** **P2** (optimization, not critical)

**References:**
- EnTT (C++ ECS library)
- Unity DOTS
- Flecs

---

### 5. Job-Based Threading

**Current:**
- Dedicated threads (Main, Render, Sim)
- Limited to 3 cores
- Fixed responsibilities

**Future:**
- Job system with thread pool
- Work-stealing queue
- Dynamic work distribution

**Example:**
```cpp
// Current (dedicated thread)
void SimProcessingUnit::Update() {
    UpdatePhysics();
    UpdateAI();
    UpdateLogic();
}

// Future (job-based)
void SimProcessingUnit::Update() {
    JobSystem::Schedule([&]() { UpdatePhysics(); });
    JobSystem::Schedule([&]() { UpdateAI(); });
    JobSystem::Schedule([&]() { UpdateLogic(); });
    JobSystem::WaitAll();
}

// Jobs distributed across N threads automatically
```

**Benefits:**
- ✅ Better CPU utilization (scales to N cores)
- ✅ Load balancing (work-stealing)
- ✅ Fine-grained parallelism

**Challenges:**
- ❌ More complex (job dependencies)
- ❌ Harder to debug (many threads)
- ❌ Current 3-thread model works

**Effort:** High (months)

**Priority:** **P2** (optimization, not critical)

**References:**
- Intel TBB
- Enkisoft TaskScheduler
- Naughty Dog's Fiber Job System

---

### 6. Better Graphics Backend

**Current:**
- SFML (OpenGL 2.x)
- 2D only
- Limited features

**Future Options:**

**Option A: Vulkan**
- ✅ Modern API
- ✅ Multi-threaded
- ✅ Cross-platform
- ❌ Complex (verbose)
- ❌ Large learning curve

**Option B: Direct3D 12 (Windows)**
- ✅ Modern API
- ✅ Good tooling (PIX)
- ✅ Microsoft-supported
- ❌ Windows-only

**Option C: SDL2 + OpenGL ES 3.0**
- ✅ Simple upgrade from SFML
- ✅ Cross-platform
- ✅ Sufficient for 2D/simple 3D
- ❌ Still old API

**Recommendation:** Stick with SFML for now (good enough for 2D)

**Benefits:**
- ✅ Better performance
- ✅ Modern features (compute shaders, etc.)
- ✅ 3D support

**Effort:** Very High (months)

**Priority:** **P3** (low, 2D sufficient)

---

## Low Priority (Maybe Someday)

### 7. Cross-Platform Build System

**Current:**
- Visual Studio only (.sln/.vcxproj)
- Windows-only

**Future:**
- CMake build system
- Linux support
- macOS support

**Benefits:**
- ✅ Cross-platform
- ✅ CI/CD friendly
- ✅ Wider audience

**Challenges:**
- ❌ UI backend not cross-platform (depends on implementation choice)
- ❌ Testing on multiple platforms
- ❌ Additional maintenance

**Effort:** Medium (weeks)

**Priority:** **P3** (nice-to-have)

---

### 8. Visual Editor

**Current:**
- Code-driven (no editor)
- Levels defined in C++

**Future:**
- Visual level editor
- Drag-and-drop entities
- Property inspector

**Benefits:**
- ✅ Faster iteration (designers)
- ✅ Visual feedback
- ✅ Non-programmers can create content

**Challenges:**
- ❌ Large undertaking (editor is its own project)
- ❌ Current code workflow works fine
- ❌ Team is programmers (less benefit)

**Effort:** Very High (many months)

**Priority:** **P3** (low, code workflow sufficient)

---

### 9. Networking

**Current:**
- Single-player only
- No networking support

**Future:**
- Client-server architecture
- State replication
- Prediction/reconciliation

**Benefits:**
- ✅ Multiplayer games
- ✅ Online features

**Challenges:**
- ❌ Complex (networking is hard)
- ❌ Not needed for current projects
- ❌ Major architecture changes

**Effort:** Very High (many months)

**Priority:** **P3** (low, not needed)

---

### 10. Asset Pipeline

**Current:**
- Manual asset loading
- No automatic importing
- No optimization

**Future:**
- Automatic asset importing
- Format conversion (PNG → optimized)
- Compression
- Asset hot-reloading

**Benefits:**
- ✅ Faster iteration
- ✅ Smaller builds
- ✅ Better workflow

**Challenges:**
- ❌ Additional tooling needed
- ❌ Current manual process works
- ❌ Maintenance overhead

**Effort:** High (months)

**Priority:** **P3** (nice-to-have)

---

## Research Areas

### Data-Oriented Design (DOD)

**Current:**
- Object-oriented (classes, inheritance)
- Pointer-heavy (cache misses)

**Explore:**
- Structure of arrays (SoA) vs array of structures (AoS)
- Cache-friendly memory layout
- Minimize indirection

**Example:**
```cpp
// Current (AoS)
struct Entity {
    Vector3D position;
    Vector3D velocity;
    float health;
};
Array<Entity*> entities;  // Pointers (cache misses)

// Future (SoA)
struct Entities {
    Array<Vector3D> positions;   // Contiguous
    Array<Vector3D> velocities;  // Contiguous
    Array<float> healths;        // Contiguous
};

// Better cache locality for bulk operations
```

**Benefits:**
- ✅ Better performance (cache-friendly)
- ✅ Easier to parallelize (no aliasing)

---

### Modern C++ Features

**Current:**
- C++11 (move semantics, lambdas)

**Explore:**
- C++17 (std::optional, std::variant, structured bindings)
- C++20 (concepts, ranges, coroutines)

**Benefits:**
- ✅ Cleaner code
- ✅ Better type safety (concepts)
- ✅ Easier async (coroutines)

**Challenges:**
- ❌ Compiler support (Visual Studio 2015+)
- ❌ Learning curve
- ❌ Code compatibility

---

### Lock-Free Data Structures

**Current:**
- std::mutex for all synchronization
- Potential contention

**Explore:**
- Lock-free queues (Boost.Lockfree)
- Atomic operations
- Memory ordering

**Benefits:**
- ✅ Better performance (no blocking)
- ✅ Scalability (no contention)

**Challenges:**
- ❌ Complex (memory ordering hard)
- ❌ Current mutexes work fine
- ❌ Premature optimization?

---

## Evolution Strategy

### Principles

1. **Don't Fix What Isn't Broken**
   - Current architecture works
   - Only change when actual problem

2. **Incremental Evolution**
   - Small improvements over time
   - Don't rewrite everything at once

3. **Measure Before Optimizing**
   - Profile before changing
   - Prove performance benefit

4. **Maintainability > Performance**
   - Readable code more important
   - Optimize hot paths only

---

### Roadmap

**2026 Q2:**
- Fix DiaMaths template bugs (P0)
- Optimize Transform2D (P1)

**2026 Q3-Q4:**
- Implement UI backend: CEF or ImGui (P1)
- Research ECS (P2)

**2027:**
- Implement ECS if proven beneficial
- Explore job-based threading
- Consider cross-platform support

**Long-Term:**
- Visual editor (if team grows)
- Networking (if needed)
- Asset pipeline (if workflow pain)

---

## Open Questions

### Question 1: ECS Worth It?

**Pros:**
- Cache-friendly
- Data-oriented
- Scalable

**Cons:**
- Major refactor
- Learning curve
- Current system works

**Need:** Profiling data to prove benefit

---

### Question 2: Job System vs Dedicated Threads?

**Pros:**
- Better CPU utilization
- Scalable to N cores

**Cons:**
- More complex
- Harder to debug
- 3 threads sufficient?

**Need:** Real-world performance bottlenecks

---

### Question 3: Keep SFML or Switch?

**Pros of Switching:**
- Modern API (Vulkan)
- Better performance
- More features

**Cons of Switching:**
- Huge effort
- SFML works well for 2D
- Not needed currently

**Need:** 3D requirement or performance issues

---

## Summary

**High Priority (Should Do):**
- ✅ Fix DiaMaths bugs (P0)
- ✅ Implement UI backend (P1)
- ✅ Optimize Transform2D (P1)

**Medium Priority (Nice to Have):**
- 🤔 Entity Component System (P2)
- 🤔 Job-based threading (P2)
- 🤔 Better graphics backend (P3)

**Low Priority (Maybe Someday):**
- 💭 Cross-platform builds
- 💭 Visual editor
- 💭 Networking
- 💭 Asset pipeline

**Philosophy:**
- Fix bugs first (correctness)
- Address tech debt (UI backend)
- Optimize where proven beneficial (profile first)
- Don't fix what isn't broken
- Incremental evolution, not revolution

**Next Steps:**
1. Fix DiaMaths template bugs
2. Profile Transform2D performance
3. Evaluate UI backend options (CEF vs ImGui)
4. Research ECS benefits

**[→ Back to Design Philosophy](design.md)**  
**[→ Historical Decisions](historical-decisions.md)**
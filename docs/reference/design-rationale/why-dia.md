# Why Dia Engine?

**Last Updated:** 2026-04-01

Rationale for creating a custom game engine rather than using existing solutions.

---

## The Question

**"Why build your own engine? Just use Unity/Unreal/Godot!"**

Valid question. Let's explore the reasoning.

---

## Existing Engine Landscape

### Commercial Engines

**Unity:**
- ✅ Mature, feature-rich
- ✅ Great tooling (editor)
- ✅ Large community
- ❌ C# (not C++)
- ❌ Black box (source hidden unless $$$)
- ❌ Runtime overhead (garbage collection)
- ❌ Opinionated (Unity way or highway)

**Unreal Engine:**
- ✅ AAA quality
- ✅ C++ based
- ✅ Source available
- ❌ Massive (10+ GB SDK)
- ❌ Complex (steep learning curve)
- ❌ Opinionated architecture (UObject, BluePrints)
- ❌ Heavy (slow compile times)

---

### Open Source Engines

**Godot:**
- ✅ Open source
- ✅ Lightweight
- ✅ Good 2D support
- ❌ GDScript (custom language)
- ❌ Less mature C++ integration
- ❌ Smaller ecosystem

**SDL2 / SFML:**
- ✅ Simple multimedia libraries
- ✅ Cross-platform
- ✅ Lightweight
- ❌ Not engines (just libraries)
- ❌ Must build everything yourself
- ❌ No high-level structure

---

## Why Dia?

### 1. Learning and Control

**Goal:** Understand game engine architecture deeply

**With Existing Engine:**
```
Black Box
┌──────────────────┐
│ Unity / Unreal   │ ← How does this work?
│ (Hidden magic)   │ ← Can't change core systems
└──────────────────┘
```

**With Dia:**
```
Full Understanding
┌──────────────────┐
│ Module System    │ ← Implemented by us
│ Threading        │ ← We understand every line
│ Type System      │ ← Full control
└──────────────────┘
```

**Benefits:**
- ✅ Learn architecture patterns (Module/Phase/PU)
- ✅ Understand threading models
- ✅ No magic (everything explicit)
- ✅ Can modify any system

---

### 2. No Bloat

**Existing Engines:**
```
Unreal Engine: 10+ GB
├── Everything AAA games need
├── Features you'll never use
├── Complex subsystems
└── Slow compile times
```

**Dia Engine: Minimal**
```
Dia: ~50 MB
├── Only what we need
├── Fast compile times
├── Lean dependencies
└── Simple subsystems
```

**Trade-off:** Fewer features, but faster iteration

---

### 3. Explicit Design

**Unity/Unreal Philosophy:**
```csharp
// Unity: GameObject-centric (implicit behavior)
public class Player : MonoBehaviour {
    void Update() {
        // When does this run?
        // What thread?
        // What dependencies?
        // Not clear from code
    }
}
```

**Dia Philosophy:**
```cpp
// Dia: Module-centric (explicit behavior)
class PlayerModule : public Module {
public:
    PlayerModule() {
        RegisterDependency(&GetDependency<InputModule>());
        RegisterDependency(&GetDependency<PhysicsModule>());
    }
    
    void DoUpdate() override {
        // Runs on SimProcessingUnit (explicit)
        // After Input and Physics (explicit)
        // Dependencies clear
    }
};
```

**Benefits:**
- ✅ No hidden behaviors
- ✅ Clear thread boundaries
- ✅ Explicit dependencies
- ✅ Easier to debug

---

### 4. Lightweight Threading

**Unreal Engine Threading:**
```cpp
// Complex task graph system
FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
    [&]() { /* work */ },
    TStatId(),
    nullptr,
    ENamedThreads::GameThread
);
```

**Dia Threading:**
```cpp
// Simple: ProcessingUnit per thread
MainProcessingUnit mainPU;    // Main thread
RenderProcessingUnit renderPU; // Render thread
SimProcessingUnit simPU;       // Sim thread

std::thread renderThread(renderPU);
std::thread simThread(simPU);
```

**Benefits:**
- ✅ Easy to understand (3 threads)
- ✅ Clear responsibilities
- ✅ Low overhead
- ✅ Manageable synchronization

---

### 5. Custom Container Control

**Unity:**
```csharp
List<GameObject> objects = new List<GameObject>();
// Garbage collected (unpredictable GC pauses)
```

**Unreal:**
```cpp
TArray<AActor*> Actors;
// Better, but still opaque (UE4-specific)
```

**Dia:**
```cpp
DynamicArray<GameObject*> objects;
objects.Reserve(100);  // Explicit allocation
// Full control, no GC, predictable performance
```

**Benefits:**
- ✅ Explicit memory management
- ✅ No garbage collection pauses
- ✅ Predictable performance
- ✅ Can optimize for specific use cases

---

## What Dia Doesn't Provide

### Features We Don't Have (Yet)

❌ **Visual Editor** (Unity/Unreal have)
- No drag-and-drop level editor
- Code-driven workflow
- Trade-off: Faster iteration for programmers

❌ **Asset Pipeline** (Unity/Unreal have)
- No automatic asset importing
- Manual resource management
- Trade-off: Simpler, more explicit

❌ **High-Level Abstractions** (Unity/Unreal have)
- No GameObject/Actor base class
- No Component system (yet)
- Trade-off: More flexible, less opinionated

❌ **Networking** (Unity/Unreal have)
- No built-in multiplayer
- Would need to implement
- Trade-off: Not needed for current projects

❌ **Advanced Graphics** (Unreal has)
- No PBR materials
- No dynamic lighting
- Trade-off: SFML is simple, good enough for 2D

---

## When to Use Dia vs Existing Engines

### Use Dia When:

✅ **Learning:** Want to understand engine architecture  
✅ **Control:** Need full control over every system  
✅ **Lightweight:** Don't need AAA features  
✅ **2D:** SFML is excellent for 2D games  
✅ **Explicit:** Prefer explicit over implicit  
✅ **C++:** Want modern C++ patterns  

### Use Existing Engine When:

✅ **Time-Constrained:** Need to ship fast  
✅ **3D/AAA:** Need advanced rendering  
✅ **Team:** Large team with engine expertise  
✅ **Tooling:** Need visual editors  
✅ **Ecosystem:** Need asset store, plugins  

---

## Dia's Unique Advantages

### 1. Understandable Codebase

**Lines of Code:**
- Dia: ~50,000 lines (readable in a week)
- Unity/Unreal: Millions of lines (impossible to fully grasp)

**Result:** Can understand entire engine

---

### 2. Fast Compile Times

**Build Times:**
- Dia: ~30 seconds (full rebuild)
- Unreal: 10+ minutes (incremental)

**Result:** Faster iteration

---

### 3. No License Concerns

**Licensing:**
- Dia: Your code, your rules
- Unity: Revenue share for Pro
- Unreal: 5% royalty after $1M

**Result:** Full ownership

---

### 4. Perfect for Teaching

**Educational Value:**
- See how engines work internally
- Learn threading patterns
- Understand module systems
- Apply to any future engine work

**Result:** Transferable knowledge

---

## Real-World Example: Cluiche

### What Cluiche Needs

- ✅ 2D graphics (SFML sufficient)
- ✅ Input handling (keyboard, mouse, gamepad)
- ✅ Basic UI (web-based)
- ✅ Game loop with consistent frame rate
- ✅ Pluggable levels
- ❌ 3D rendering (not needed)
- ❌ Physics (basic 2D sufficient)
- ❌ Networking (single-player)

**Verdict:** Dia is perfect fit (no bloat, full control)

---

## Evolution Path

### Current: Foundation

- ✅ Module/Phase/ProcessingUnit framework
- ✅ Threading model (Main/Render/Sim)
- ✅ Basic graphics (SFML)
- ✅ Input system
- ✅ Type system

### Near Future: Gameplay Systems

- 🚧 Entity Component System (ECS)
- 🚧 Better physics (2D rigid bodies)
- 🚧 Audio system
- 🚧 Serialization

### Far Future: Advanced Features

- ⏳ Job system (replace dedicated threads)
- ⏳ Better graphics backend (Vulkan/Direct3D)
- ⏳ Editor tools
- ⏳ Asset pipeline

**Philosophy:** Add features as needed, not preemptively

---

## Comparisons

### Dia vs SDL2

**SDL2:**
- Simple library (not an engine)
- Must build everything yourself
- More boilerplate

**Dia:**
- Framework provided (Module/Phase/PU)
- Threading built-in
- Type system included

**Verdict:** Dia saves time on structure

---

### Dia vs Raylib

**Raylib:**
- Simple C-based library
- Immediate mode (no scene graph)
- Great for prototypes

**Dia:**
- C++ based (OOP, templates)
- Module-based (retained mode)
- Better for larger projects

**Verdict:** Depends on project size

---

### Dia vs Custom from Scratch

**From Scratch:**
- Total freedom
- Must design everything
- Easy to make mistakes

**Dia:**
- Proven patterns (Module/Phase/PU)
- Threading model designed
- Type system ready

**Verdict:** Dia provides foundation

---

## Lessons from Building Dia

### What Worked Well

1. **Module/Phase/PU Pattern**
   - Clear lifecycle
   - Explicit dependencies
   - Testable

2. **Custom Containers**
   - Predictable performance
   - No GC pauses
   - Debuggable

3. **Type System**
   - Fast (compile-time CRC)
   - Serialization support
   - No RTTI overhead

---

### What We'd Do Differently

1. **Start with ECS**
   - Current component system is custom
   - ECS would be more cache-friendly
   - Lesson: Data-oriented earlier

2. **Job System from Start**
   - Dedicated threads work, but limited
   - Job system more scalable
   - Lesson: Plan for parallelism

3. **Better Asset Pipeline**
   - Manual asset loading tedious
   - Need automatic resource management
   - Lesson: Tools matter

---

## Summary

**Why Dia Engine?**

✅ **Learning** - Understand engine architecture deeply  
✅ **Control** - Full control over every system  
✅ **Explicit** - No hidden behaviors or magic  
✅ **Lightweight** - Fast compile times, small footprint  
✅ **Tailored** - Exactly what we need, no bloat  

**Trade-Offs:**

❌ Fewer features than Unity/Unreal  
❌ No visual tools (code-driven)  
❌ Must implement more yourself  

**Verdict:**

Perfect for:
- Learning game engine architecture
- Projects needing full control
- 2D games
- Educational purposes
- Avoiding licensing concerns

**Not suitable for:**
- AAA 3D games
- Time-critical projects
- Teams needing visual editors

**Philosophy:** Build what you need, understand what you build

**[→ Back to Design Philosophy](design.md)**  
**[→ Dia Engine Architecture](../architecture/dia-engine.md)**
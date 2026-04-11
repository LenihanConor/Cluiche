# Why Module/Phase/ProcessingUnit?

**Last Updated:** 2026-04-01

Deep dive into the design rationale for Dia's Module/Phase/ProcessingUnit threading architecture.

---

## The Problem

### Traditional Game Loop Issues

**Classic single-threaded game loop:**
```cpp
void GameLoop() {
    while (running) {
        // Process input
        HandleInput();
        
        // Update game logic
        UpdatePhysics(deltaTime);
        UpdateAI(deltaTime);
        UpdateGameLogic(deltaTime);
        
        // Render
        RenderScene();
        SwapBuffers();
    }
}
```

**Problems:**

1. **Hidden Dependencies**
   - UpdateAI might depend on Physics
   - UpdateGameLogic might depend on AI
   - Order matters but isn't explicit
   - Change order → subtle bugs

2. **Initialization Order Unclear**
   ```cpp
   // Which order?
   InitPhysics();
   InitAI();
   InitGameLogic();
   
   // AI needs Physics to be initialized first
   // Not obvious from code
   ```

3. **Hard to Test**
   - Systems tightly coupled
   - Can't test AI without Physics
   - Must initialize entire engine for tests

4. **Threading Added as Afterthought**
   ```cpp
   // Later: "Let's make this multi-threaded"
   std::thread renderThread(RenderLoop);  // Race conditions!
   ```

5. **No Lifecycle Management**
   - Shutdown order matters
   - Easy to leak resources
   - No cleanup guarantees

---

## The Solution: Module/Phase/ProcessingUnit

### Core Concepts

**ProcessingUnit** = Thread orchestrator
- Manages thread lifecycle
- Runs phases in sequence
- Handles phase transitions (thread-safe)

**Phase** = Execution stage
- Contains modules
- Resolves dependencies
- Provides lifecycle hooks

**Module** = Functional unit
- Declares dependencies explicitly
- Has defined lifecycle (Start → Update → Stop)
- Testable in isolation

---

## Why This Pattern?

### 1. Explicit Dependencies

**Problem: Hidden Coupling**
```cpp
// Traditional: Hidden dependency
class AISystem {
    void Update() {
        // Uses global physics (hidden!)
        Physics::Instance()->RaycastQuery(...);
    }
};
```

**Solution: Declared Dependencies**
```cpp
// Module/Phase/PU: Explicit
class AIModule : public Module {
public:
    AIModule() {
        RegisterDependency(&GetDependency<PhysicsModule>());
    }
    
private:
    void DoUpdate() override {
        PhysicsModule* physics = GetDependency<PhysicsModule>();
        physics->RaycastQuery(...);
    }
};
```

**Benefits:**
- ✅ Dependencies visible in constructor
- ✅ Compiler error if dependency missing
- ✅ Automatic dependency ordering (topological sort)
- ✅ Can't accidentally create cycles

---

### 2. Automatic Initialization Order

**Problem: Order Matters**
```cpp
// Which is correct?
InitPhysics();
InitAI();

// vs

InitAI();
InitPhysics();  // AI crashes because Physics not ready!
```

**Solution: Dependency Resolution**
```cpp
Phase phase;
phase.AddModule(&aiModule);      // Depends on Physics
phase.AddModule(&physicsModule);  // No dependencies

phase.Start();
// Automatically starts: Physics → AI (dependency order)
```

**Benefits:**
- ✅ Correct order automatically determined
- ✅ Can't mess up initialization
- ✅ Add/remove modules without breaking order
- ✅ Shutdown in reverse order (cleanup safety)

---

### 3. Thread Boundaries Explicit

**Problem: Unclear Threading**
```cpp
// Traditional: Which thread is this on?
void Update() {
    mData++;  // RACE CONDITION?
}
```

**Solution: ProcessingUnit per Thread**
```cpp
// Main thread
class MainProcessingUnit : public ProcessingUnit {
    // Everything here runs on Main thread
};

// Render thread
class RenderProcessingUnit : public ProcessingUnit {
    // Everything here runs on Render thread
};
```

**Benefits:**
- ✅ Clear: This module runs on this thread
- ✅ No accidental cross-thread access
- ✅ Synchronization explicit (mutexes, FrameStreams)
- ✅ Easier to reason about threading

---

### 4. Testability

**Problem: Can't Test in Isolation**
```cpp
// Traditional: Must initialize everything
void TestAI() {
    InitEngine();      // Needs graphics drivers!
    InitPhysics();     // Needs physics DLL!
    InitAI();
    
    // Finally can test...
}
```

**Solution: Dependency Injection**
```cpp
// Module/Phase/PU: Mock dependencies
void TestAI() {
    MockPhysicsModule mockPhysics;
    AIModule aiModule;
    
    // Inject mock
    aiModule.RegisterDependency(&mockPhysics);
    
    // Test in isolation
    aiModule.Start();
    aiModule.Update();
    aiModule.Stop();
}
```

**Benefits:**
- ✅ Test modules independently
- ✅ Fast tests (no heavy initialization)
- ✅ Mock dependencies easily
- ✅ Can run tests headless (no graphics)

---

### 5. Lifecycle Management

**Problem: Cleanup Error-Prone**
```cpp
// Traditional: Easy to forget cleanup
void Shutdown() {
    delete physics;
    delete ai;
    // Forgot to delete gameLogic! Memory leak
}
```

**Solution: Guaranteed Lifecycle**
```cpp
// Module/Phase/PU: Automatic cleanup
Phase phase;
phase.AddModule(&physics);
phase.AddModule(&ai);
phase.AddModule(&gameLogic);

phase.Start();   // Starts all (dependency order)
phase.Update();  // Updates all
phase.Stop();    // Stops all (reverse order) - guaranteed
```

**Benefits:**
- ✅ Stop always called (even if exception)
- ✅ Reverse order (dependencies cleaned up last)
- ✅ Can't forget to clean up
- ✅ Resources freed automatically

---

## Why Three Threads?

### Single-Threaded Limitations

**Problem 1: Variable Frame Rate**
```cpp
while (running) {
    Update();   // Takes 10ms
    Render();   // Takes 16ms
}
// Result: 38 FPS (10+16 = 26ms/frame)
```

If Update takes 30ms (complex frame):
```cpp
Update();   // 30ms
Render();   // 16ms
// Result: 21 FPS (46ms/frame) - choppy!
```

**Problem 2: UI Blocks Everything**
```cpp
while (running) {
    ProcessUIEvents();  // Awesomium requires main thread
    Update();           // Blocked until UI finishes
    Render();           // Blocked until Update finishes
}
```

---

### Three-Thread Solution

**Main Thread:**
- Bootstrap application
- Coordinate UI (Awesomium **requires** main thread)
- Collect input @ 30Hz
- Spawn worker threads

**Render Thread:**
- Read graphics commands from Sim
- Render @ 60 FPS (TimeThreadLimiter)
- Never blocks on slow simulation
- Consistent frame rate

**Sim Thread:**
- Read input from Main
- Update game logic (variable rate)
- Write graphics commands to Render
- Can run slower without affecting visuals

**Benefits:**
- ✅ **Consistent visuals** (Render at 60 FPS even if Sim slow)
- ✅ **Responsive UI** (Main thread available for Awesomium)
- ✅ **CPU utilization** (3 threads on multi-core)
- ✅ **Decoupled concerns** (simulation independent of rendering)

---

## Design Alternatives Considered

### Alternative 1: Single-Threaded

**Pros:**
- Simple (no synchronization)
- Easy to debug (single call stack)
- No race conditions

**Cons:**
- ❌ Variable frame rate (sim blocks rendering)
- ❌ UI blocks game loop (Awesomium requirement)
- ❌ Wastes CPU (single core utilized)

**Verdict:** Not suitable for modern games

---

### Alternative 2: Two Threads (Main + Worker)

**Pros:**
- Simpler than three threads
- UI doesn't block game loop

**Cons:**
- ❌ Sim and Render still coupled (slow sim → choppy visuals)
- ❌ Can't independently control frame rates

**Verdict:** Insufficient separation

---

### Alternative 3: Job System (Many Threads)

**Pros:**
- Maximum CPU utilization
- Scalable to N cores
- Fine-grained parallelism

**Cons:**
- ❌ More complex (job scheduling, dependencies)
- ❌ Overhead for small tasks
- ❌ Harder to debug (many threads)

**Verdict:** Future improvement, too complex initially

---

### Alternative 4: Four+ Threads (Physics, AI, etc.)

**Pros:**
- Maximum separation of concerns
- Each system has dedicated thread

**Cons:**
- ❌ Thread overhead (context switching)
- ❌ Synchronization complexity increases
- ❌ Diminishing returns on typical hardware

**Verdict:** Over-engineered for current needs

---

## Why Chosen: Three Threads + Module/Phase/PU

**Sweet Spot:**
- ✅ Sufficient parallelism (3 cores utilized)
- ✅ Clear separation (Main/Render/Sim)
- ✅ Manageable complexity (3 sync points)
- ✅ Consistent frame rate (Render independent)
- ✅ Responsive UI (Main available)

**Scalable:**
- Can add more threads later (Physics, AI)
- Can evolve to job system
- Module/Phase/PU pattern supports any threading model

---

## Trade-Offs Accepted

### 1. Boilerplate Code

**Cost:**
```cpp
// Every module needs DoStart/DoUpdate/DoStop
class MyModule : public Module {
private:
    void DoStart() override { /* ... */ }
    void DoUpdate() override { /* ... */ }
    void DoStop() override { /* ... */ }
};
```

**Benefit:**
- Clear lifecycle
- Guaranteed cleanup
- Easy to understand flow

**Verdict:** Worth it for maintainability

---

### 2. Synchronization Complexity

**Cost:**
```cpp
// Must protect shared state
std::lock_guard<std::mutex> lock(mMutex);
```

**Benefit:**
- Thread safety explicit
- No hidden race conditions
- Easier to debug (know where locks are)

**Verdict:** Worth it for correctness

---

### 3. Memory Overhead

**Cost:**
- ~1MB stack per thread
- FrameStream buffers (~10-20 frames)
- Total: ~5-10MB overhead

**Benefit:**
- Consistent 60 FPS rendering
- Non-blocking UI
- Multi-core utilization

**Verdict:** Negligible on modern hardware

---

## Real-World Impact

### Before Module/Phase/PU

**Issues:**
- Hidden dependencies caused crashes
- Initialization order bugs
- Threading added later → race conditions
- Hard to test systems in isolation
- Unclear which thread code ran on

**Developer Experience:**
- "Why does AI crash on startup?" (Physics not initialized)
- "Why is UI freezing?" (Blocking main thread)
- "Why is frame rate inconsistent?" (Sim blocks rendering)

---

### After Module/Phase/PU

**Benefits:**
- Dependencies explicit → correct order automatic
- Thread boundaries clear → no race conditions
- Modules testable → fast tests
- Lifecycle guaranteed → no leaks

**Developer Experience:**
- "Add new module, declare dependencies, done"
- "UI always responsive"
- "Consistent 60 FPS"

---

## Lessons Learned

### What Works Well

1. **Explicit Dependencies**
   - Prevents coupling
   - Documents relationships
   - Catches errors at compile time

2. **Phase Hooks**
   - Flexible (BeforeModulesStart, AfterModulesUpdate, etc.)
   - Can inject logic at any point
   - Clean separation

3. **Thread-per-ProcessingUnit**
   - Clear threading model
   - Easy to reason about
   - Scales to more threads if needed

---

### What Could Be Better

1. **Boilerplate**
   - Every module needs 3 virtual methods
   - Could use macros (but prefer explicit)
   - Trade-off for safety

2. **Learning Curve**
   - New developers need to understand pattern
   - More complex than simple game loop
   - Worth it for maintainability

3. **Performance Overhead**
   - Virtual function calls (DoStart/DoUpdate/DoStop)
   - Negligible in practice
   - Benefits outweigh cost

---

## Summary

**Module/Phase/ProcessingUnit pattern provides:**

✅ **Explicit Dependencies** - No hidden coupling  
✅ **Automatic Ordering** - Correct initialization/cleanup  
✅ **Thread Safety** - Clear thread boundaries  
✅ **Testability** - Mock dependencies, test in isolation  
✅ **Lifecycle Management** - Guaranteed cleanup  
✅ **Consistent Frame Rate** - Render independent of Sim  
✅ **Responsive UI** - Main thread available  

**Why Three Threads:**
- Main: UI and input
- Render: 60 FPS graphics
- Sim: Variable-rate logic

**Trade-Offs:**
- More boilerplate (worth it for maintainability)
- Synchronization complexity (worth it for correctness)
- Memory overhead (negligible on modern hardware)

**Verdict:** Excellent foundation for game architecture

**[→ Back to Design Philosophy](design.md)**  
**[→ Module System Architecture](../01-architecture/module-system.md)**  
**[→ Threading Model Architecture](../01-architecture/threading-model.md)**
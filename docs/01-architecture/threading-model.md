# Threading Model

**Last Updated:** 2026-03-31

Cluiche uses a **three-thread architecture** with explicit boundaries and synchronization.

---

## Overview

The threading model separates concerns across three independent threads:

| Thread | Purpose | Update Rate | Synchronization |
|--------|---------|-------------|-----------------|
| **Main** | Bootstrap, UI coordination, input collection | As needed (~30Hz) | Spawns other threads, mutex-protected state |
| **Render** | Graphics rendering, display management | 60 FPS (target) | Reads sim state (read-only) |
| **Sim** | Game simulation, physics, logic | Variable rate | Writes sim state (mutex-protected) |

**[→ Threading sequence diagram](diagrams/threading-model.mmd)**

---

## Design Rationale

### Why Three Threads?

**Problem:** Single-threaded game loops must balance:
- Consistent render rate (60 FPS for smooth visuals)
- Variable simulation rate (depends on complexity)
- Non-blocking UI (Awesomium requires main thread)

**Solution:** Three independent threads with clear responsibilities:

1. **Main Thread** - Handles UI (Awesomium must run on main thread), bootstraps app, spawns worker threads
2. **Render Thread** - Dedicated rendering at consistent 60 FPS, independent of sim complexity
3. **Sim Thread** - Game logic and physics at variable rate, doesn't block rendering

**Benefits:**
- ✅ Consistent frame rate even when simulation is slow
- ✅ Non-blocking UI (main thread available for Awesomium events)
- ✅ Clear separation of concerns (rendering vs simulation)
- ✅ Independent thread profiling and optimization

**Trade-offs:**
- ❌ Synchronization complexity (mutexes, frame streams)
- ❌ Potential data races if not careful
- ❌ Increased memory usage (per-thread stacks, buffers)

**[→ Design rationale details](../02-design/why-module-phase-pu.md)**

---

## Thread Details

### Main Thread (MainProcessingUnit)

**File:** `Cluiche/Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.h`

**Responsibility:** Bootstrap the application and coordinate UI

**Phases:**
1. **MainBootPhase** - Register core modules, register levels
2. **MainBootStrapPhase** - Show launch UI, spawn threads, wait for user input

**Key Modules:**
- `MainKernelModule` - Time server @ 30Hz, input source manager, SFML window, canvas
- `MainUIModule` - Awesomium UI system (observer subject)
- `LevelFactoryModule` - Level registry and factory access

**Update Loop:**
```cpp
void MainProcessingUnit::Update() {
    while (mRunning) {
        // Collect input from SFML
        inputSourceManager->Update();
        
        // Write input to Sim thread
        inputToSimFrameStream->PushFrame(inputEvents);
        
        // Update UI system (Awesomium)
        uiSystem->Update();
        
        // Update time server @ 30Hz
        timeServer->Update();
        
        // Phase update
        currentPhase->Update();
    }
}
```

**Spawns Threads:**
```cpp
// In MainBootStrapPhase
std::thread renderThread(renderPU);
std::thread simThread(simPU);
```

**Waits for Threads:**
```cpp
// On shutdown
renderThread.join();
simThread.join();
```

---

### Render Thread (RenderProcessingUnit)

**File:** `Cluiche/Cluiche/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h`

**Responsibility:** Render graphics at consistent 60 FPS

**Phases:**
1. **RenderRunningPhase** - Main rendering loop

**Key Modules:**
- `RenderKernel` - Frame management
- `RenderCanvas` - SFML rendering, display

**Update Loop:**
```cpp
void RenderProcessingUnit::Update() {
    TimeThreadLimiter limiter(60.0f);  // 60 FPS target
    
    while (mRunning) {
        limiter.Start();  // Begin frame timing
        
        // Read graphics commands from Sim thread
        FrameData* frameData = simToRenderFrameStream->PeekFrame();
        
        if (frameData) {
            // Render to SFML canvas
            canvas->Clear();
            frameData->Render(canvas);
            canvas->Display();
        }
        
        // Phase update
        currentPhase->Update();
        
        limiter.End();   // Sleep if frame completed early
    }
}
```

**Frame Rate Control:**
- `TimeThreadLimiter` ensures consistent 60 FPS
- Uses `std::chrono::high_resolution_clock`
- Sleeps thread if frame completes early
- No busy-waiting (CPU efficient)

**Read-Only Access:**
- Render thread **only reads** from `SimToRenderFrameStream`
- No writes to shared state (thread-safe by design)

---

### Sim Thread (SimProcessingUnit)

**File:** `Cluiche/Cluiche/ApplicationFlow/ProcessingUnits/SimProcessingUnit.h`

**Responsibility:** Update game simulation and physics at variable rate

**Phases:**
1. **SimBootPhase** - Initialize simulation modules
2. **SimBootStrapPhase** - Wait for UI ready, start simulation

**Key Modules:**
- `SimTimeServerModule` - Independent simulation clock
- `SimInputFrameStreamModule` - Consumes input from Main thread
- `SimUIProxyModule` - Cross-thread UI bridge (mutex-protected observer)

**Update Loop:**
```cpp
void SimProcessingUnit::Update() {
    while (mRunning) {
        // Read input from Main thread
        EventData* inputEvents = inputToSimFrameStream->ConsumeFrame();
        
        if (inputEvents) {
            // Process input events
            ProcessInput(inputEvents);
        }
        
        // Update simulation
        simTimeServer->Update();
        currentLevel->Update();
        physicsSystem->Update();
        
        // Write graphics commands to Render thread
        FrameData frameData = GenerateFrameData();
        simToRenderFrameStream->PushFrame(frameData);
        
        // Phase update
        currentPhase->Update();
    }
}
```

**Variable Rate:**
- No frame rate limiter (runs as fast as possible)
- Can drop frames if simulation is slow (Render shows last valid frame)
- Independent of render rate (decoupled for performance)

**Write Access:**
- Sim thread **writes** to `SimToRenderFrameStream` (mutex-protected)
- **Reads** from `InputToSimFrameStream` (mutex-protected)

---

## Thread Communication

### Frame Streams

**Type:** `Dia::Core::FrameStream<T>`

**Purpose:** Thread-safe temporal frame buffers for inter-thread communication

#### InputToSimFrameStream

**Direction:** Main → Sim

**Data Type:** `EventData` (input events)

**Usage:**
```cpp
// Main thread (write)
inputToSimFrameStream->PushFrame(inputEvents);

// Sim thread (read)
EventData* events = inputToSimFrameStream->ConsumeFrame();
```

**Synchronization:** Internal mutex protects push/consume operations

#### SimToRenderFrameStream

**Direction:** Sim → Render

**Data Type:** `FrameData` (graphics commands)

**Usage:**
```cpp
// Sim thread (write)
simToRenderFrameStream->PushFrame(frameData);

// Render thread (read)
FrameData* frame = simToRenderFrameStream->PeekFrame();
```

**Synchronization:** Internal mutex protects push/peek operations

**Frame Garbage Collection:**
- FrameStream maintains buffer of recent frames
- Old frames automatically cleaned up
- Render always has valid frame (even if Sim is slow)

---

### Observer Pattern (Main ↔ Sim UI)

**Purpose:** Notify Sim thread when Main UI is ready

**Pattern:** `ObserverSubject` → `Observer`

**Implementation:**

```cpp
// MainUIModule (Main thread) - Subject
class MainUIModule : public ObserverSubject {
    void DoStart() override {
        InitializeUI();
        Notify();  // Notify observers UI is ready (mutex-protected)
    }
};

// SimUIProxyModule (Sim thread) - Observer
class SimUIProxyModule : public Observer {
    void OnNotify() override {
        std::lock_guard<std::mutex> lock(mMutex);
        mUIReady = true;  // Safe to use UI proxy
    }
};
```

**Synchronization:**
- `ObserverSubject::Notify()` uses internal mutex
- `SimUIProxyModule` uses mutex for UI access
- Thread-safe notifications

---

## Synchronization Mechanisms

### Mutexes

**Type:** `std::mutex` (C++ standard library)

**Usage Locations:**

1. **Phase Transition Queue** (`ProcessingUnit::mQueuedTransitionMutex`)
   ```cpp
   void ProcessingUnit::TransitionPhase(Phase* newPhase) {
       std::lock_guard<std::mutex> lock(mQueuedTransitionMutex);
       mQueuedPhaseTransition = newPhase;
   }
   ```

2. **Observer Notifications** (`ObserverSubject` internal mutex)
   ```cpp
   void ObserverSubject::Notify() {
       std::lock_guard<std::mutex> lock(mObserverMutex);
       for (Observer* obs : mObservers) {
           obs->OnNotify();
       }
   }
   ```

3. **UI Proxy Access** (`SimUIProxyModule::mMutex`)
   ```cpp
   void SimUIProxyModule::SendMessage(const char* msg) {
       std::lock_guard<std::mutex> lock(mMutex);
       if (mUIReady) {
           mUISystem->SendMessage(msg);
       }
   }
   ```

4. **Frame Stream Operations** (`FrameStream<T>` internal mutex)
   - Protects push/peek/consume operations
   - Ensures atomic buffer access

**No Lock-Free Structures:**
- System uses standard mutexes for simplicity
- No lock-free queues or atomic operations (currently)
- Trade-off: Simplicity vs potential contention

---

## Thread Safety Guidelines

### Safe Patterns

✅ **Read-only access** (Render thread reading sim state)
```cpp
// Render thread - safe (read-only)
FrameData* frame = simToRenderFrameStream->PeekFrame();
```

✅ **Mutex-protected writes** (Sim thread writing sim state)
```cpp
// Sim thread - safe (mutex-protected)
simToRenderFrameStream->PushFrame(frameData);
```

✅ **Thread-local state** (each thread has own TimeServer)
```cpp
// Main thread has MainKernelModule::TimeServer
// Sim thread has SimTimeServerModule::TimeServer (independent)
```

✅ **Observer pattern with mutex** (cross-thread notifications)
```cpp
ObserverSubject::Notify();  // Internal mutex protects
```

### Unsafe Patterns

❌ **Direct shared state access** (no mutex)
```cpp
// BAD: Multiple threads accessing without mutex
int sharedCounter++;  // RACE CONDITION
```

❌ **Multiple hierarchy traversals** (Transform2D known issue)
```cpp
// BAD: Transform2D::GetWorldTransform() traverses hierarchy multiple times
// Not thread-safe if hierarchy modified concurrently
```

❌ **Non-thread-safe math operations** (fixed in recent update)
```cpp
// FIXED (2026-03): Random number generation now thread-safe
// Previously was NOT thread-safe
```

**[→ Thread safety guide for AI agents](../06-ai-guides/thread-safety-guide.md)**  
**[→ Thread safety testing](../04-testing/thread-safety-testing.md)**

---

## Performance Characteristics

### Frame Rates

**Target Rates:**
- Main: ~30 Hz (input collection, UI updates)
- Render: 60 FPS (consistent frame rate via TimeThreadLimiter)
- Sim: Variable (depends on simulation complexity)

**Actual Rates (typical):**
- Main: 30-60 Hz (depends on input events)
- Render: 60 FPS (locked via limiter)
- Sim: 30-120 Hz (varies with load)

### Thread Overhead

**Memory:**
- Stack per thread: ~1 MB default (Windows)
- Frame stream buffers: ~10-20 frames buffered
- Total overhead: ~5-10 MB

**CPU:**
- Main: Low (event-driven, sleeps between input)
- Render: Medium (60 FPS, sleeps between frames)
- Sim: High (no limiter, runs continuously)

**Context Switching:**
- Minimal (3 threads on modern multi-core CPUs)
- Mutex contention is low (different data access patterns)

---

## Known Issues

### Thread Safety Issues

1. **Transform2D Hierarchy Traversals**
   - **Issue:** `GetWorldTransform()` traverses hierarchy multiple times
   - **Impact:** Not thread-safe if hierarchy modified during traversal
   - **Status:** Known bug, not yet fixed
   - **[→ Details](../07-subsystems/dia-maths/known-issues.md)**

2. **Random Number Generation**
   - **Issue:** Random generators were NOT thread-safe
   - **Impact:** Concurrent calls could corrupt state
   - **Status:** **FIXED (2026-03)** - Now thread-safe
   - **[→ Details](../../THREAD_SAFE_RANDOM.md)**

### Performance Issues

1. **Sim Thread Runs Unbounded**
   - **Issue:** No frame rate limiter on Sim thread
   - **Impact:** Can consume 100% CPU if simulation is fast
   - **Workaround:** Sim thread naturally limited by complexity
   - **Status:** By design (not a bug)

2. **FrameStream Garbage Collection**
   - **Issue:** Old frames not immediately freed
   - **Impact:** Slight memory overhead (~10-20 frames)
   - **Status:** Acceptable trade-off for temporal consistency

---

## Debugging Threading Issues

### Tools

**Visual Studio:**
- Threads window: View all threads, call stacks
- Parallel Stacks: Visualize thread relationships
- Concurrency Visualizer: Profile thread activity

**Logging:**
```cpp
DIA_LOG("Main Thread", "Spawning Render thread");
DIA_LOG("Render Thread", "Frame %d rendered", frameNum);
```

**Assertions:**
```cpp
DIA_ASSERT(std::this_thread::get_id() == mMainThreadId);
```

### Common Debugging Steps

1. **Identify thread** - Which thread is the problem on?
2. **Check mutexes** - Are all shared state accesses protected?
3. **Check observers** - Are notifications thread-safe?
4. **Check frame streams** - Are push/peek/consume calls balanced?
5. **Profile** - Use Concurrency Visualizer to find contention

**[→ Debugging tips](../09-development/debugging-tips.md)**

---

## Future Improvements

### Potential Enhancements

1. **Job-Based Threading**
   - Replace dedicated threads with job system
   - Better CPU utilization (thread pool)
   - More granular parallelism

2. **Lock-Free Data Structures**
   - Replace mutexes with lock-free queues
   - Reduce contention
   - Better performance on high core counts

3. **Thread Pool for Sim**
   - Parallelize simulation tasks
   - Physics, AI, logic on separate jobs
   - Scale with available cores

**[→ Future directions](../02-design/future-directions.md)**

---

## Summary

Cluiche uses a **three-thread architecture** with clear separation:
- **Main** - Bootstrap, UI, input (30Hz)
- **Render** - Graphics display (60 FPS)
- **Sim** - Game simulation (variable rate)

**Synchronization:**
- `std::mutex` for shared state
- `FrameStream<T>` for inter-thread data
- Observer pattern for cross-thread events

**Benefits:** Consistent frame rate, non-blocking UI, clear separation  
**Trade-offs:** Synchronization complexity, memory overhead

**[→ Back to Architecture Overview](architecture.md)**
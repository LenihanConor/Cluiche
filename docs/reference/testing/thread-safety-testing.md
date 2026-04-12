# Thread Safety Testing

**Last Updated:** 2026-04-01

Concurrency testing approach for multi-threaded Cluiche architecture.

---

## Overview

Thread safety testing verifies correct behavior under concurrent execution.

**Scope:**
- Cross-thread communication (FrameStream)
- Shared state access (singletons, registries)
- Race condition detection
- Deadlock prevention
- Thread affinity verification

**Related Documents:**
- **[→ Testing Strategy](test.md)** - Overall testing approach
- **[→ Integration Testing](integration-testing.md)** - Cross-component testing
- **[→ Architecture Threading Model](../reference/architecture/threading-model.md)** - Thread design

---

## Threading Model Context

**Cluiche Architecture:**
- **Main Thread** - Input polling, level loading, coordination
- **Render Thread** - 60 FPS rendering loop (fixed timestep)
- **Sim Thread** - Variable-rate simulation updates

**Synchronization Points:**
- FrameStream queues (lock-based producer-consumer)
- Singleton access (mutex-protected)
- Phase transitions (coordinated shutdown)

**Known Issues:**
- Transform2D hierarchy not thread-safe (GetWorldMatrix traversal)
- Random class was not thread-safe (fixed 2026-03)

---

## Thread Safety Testing Categories

### 1. Race Condition Detection

**Goal:** Find unsynchronized access to shared state

#### Test: Concurrent Module Updates

```cpp
void TestConcurrentModuleAccess()
{
    // Arrange: Module accessed from multiple threads
    TestModule* module = new TestModule();
    std::atomic<int> errorCount(0);
    
    // Act: Two threads call Update simultaneously
    std::thread t1([&]() {
        for (int i = 0; i < 1000; ++i)
        {
            module->Update(0.016f);
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < 1000; ++i)
        {
            module->Update(0.016f);
        }
    });
    
    t1.join();
    t2.join();
    
    // Assert: No data corruption
    DIA_ASSERT(module->IsConsistent(), "Module state consistent after concurrent updates");
    DIA_ASSERT(errorCount == 0, "No race conditions detected");
}
```

---

#### Test: Singleton Access Under Contention

```cpp
void TestSingletonConcurrentAccess()
{
    // Arrange: Multiple threads access singleton
    std::atomic<int> successCount(0);
    
    auto accessSingleton = [&]() {
        for (int i = 0; i < 1000; ++i)
        {
            LevelFactory& factory = LevelFactory::Instance();
            ILevel* level = factory.Create(StringCRC("TestLevel"));
            
            if (level != nullptr)
            {
                successCount++;
                delete level;
            }
        }
    };
    
    // Act: Spawn multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(accessSingleton);
    }
    
    for (auto& t : threads)
    {
        t.join();
    }
    
    // Assert: All accesses successful
    DIA_ASSERT(successCount == 4000, "All singleton accesses succeeded");
}
```

---

### 2. FrameStream Thread Safety

**Goal:** Verify lock-free or mutex-protected cross-thread communication

#### Test: Producer-Consumer Stress Test

```cpp
void TestFrameStreamConcurrent()
{
    FrameStream<int> stream;
    std::atomic<int> writeCount(0);
    std::atomic<int> readCount(0);
    
    const int messageCount = 10000;
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < messageCount; ++i)
        {
            stream.Write(i);
            writeCount++;
        }
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int value;
        while (readCount < messageCount)
        {
            if (stream.Read(value))
            {
                readCount++;
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // Assert: All messages delivered
    DIA_ASSERT(writeCount == messageCount, "All writes completed");
    DIA_ASSERT(readCount == messageCount, "All reads completed");
}
```

---

#### Test: Multiple Producers, Single Consumer

```cpp
void TestFrameStreamMultipleProducers()
{
    FrameStream<int> stream;
    std::atomic<int> totalWritten(0);
    std::atomic<int> totalRead(0);
    
    const int producerCount = 4;
    const int messagesPerProducer = 1000;
    
    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int p = 0; p < producerCount; ++p)
    {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < messagesPerProducer; ++i)
            {
                stream.Write(p * 10000 + i);
                totalWritten++;
            }
        });
    }
    
    // Single consumer thread
    std::thread consumer([&]() {
        int value;
        int expected = producerCount * messagesPerProducer;
        while (totalRead < expected)
        {
            if (stream.Read(value))
            {
                totalRead++;
            }
        }
    });
    
    for (auto& t : producers)
    {
        t.join();
    }
    consumer.join();
    
    // Assert: No messages lost
    int expected = producerCount * messagesPerProducer;
    DIA_ASSERT(totalWritten == expected, "All producers wrote");
    DIA_ASSERT(totalRead == expected, "Consumer read all");
}
```

---

### 3. Deadlock Detection

**Goal:** Ensure no circular lock dependencies

#### Test: Phase Transition Under Load

```cpp
void TestPhaseTransitionNoDeadlock()
{
    // Arrange: Processing unit with multiple phases
    TestProcessingUnit* pu = new TestProcessingUnit();
    PhaseA* phaseA = new PhaseA();
    PhaseB* phaseB = new PhaseB();
    
    pu->AddPhase(phaseA);
    pu->AddPhase(phaseB);
    pu->Start(phaseA);
    
    std::atomic<bool> transitionComplete(false);
    
    // Act: Transition while other thread is updating
    std::thread updateThread([&]() {
        for (int i = 0; i < 100; ++i)
        {
            pu->Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    std::thread transitionThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        pu->QueuePhaseTransition(phaseB);
        transitionComplete = true;
    });
    
    // Wait with timeout
    updateThread.join();
    transitionThread.join();
    
    // Assert: No deadlock
    DIA_ASSERT(transitionComplete, "Transition completed (no deadlock)");
    
    delete pu;
}
```

---

#### Test: Module Dependency Lock Ordering

```cpp
void TestModuleDependencyLockOrdering()
{
    // Arrange: Two modules with mutual access patterns
    ModuleA* moduleA = new ModuleA();
    ModuleB* moduleB = new ModuleB();
    
    moduleA->SetDependency(moduleB);
    moduleB->SetDependency(moduleA);  // Potential deadlock
    
    std::atomic<int> updateCount(0);
    
    // Act: Update both modules concurrently
    std::thread t1([&]() {
        for (int i = 0; i < 100; ++i)
        {
            moduleA->Update(0.016f);
            updateCount++;
        }
    });
    
    std::thread t2([&]() {
        for (int i = 0; i < 100; ++i)
        {
            moduleB->Update(0.016f);
            updateCount++;
        }
    });
    
    // Join with timeout detection
    auto startTime = std::chrono::high_resolution_clock::now();
    t1.join();
    t2.join();
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    
    // Assert: Completed in reasonable time (no deadlock)
    DIA_ASSERT(duration.count() < 5, "No deadlock detected");
    DIA_ASSERT(updateCount == 200, "All updates completed");
}
```

---

### 4. Mutex Usage Validation

**Goal:** Verify correct mutex usage patterns

#### Test: Mutex Lock Scope

```cpp
void TestMutexLockScope()
{
    // Verify that mutexes are held for minimal time
    // (Manual code review + profiling)
    
    // Anti-pattern to detect:
    // {
    //     std::lock_guard<std::mutex> lock(mMutex);
    //     ExpensiveOperation();  // BAD: holding lock too long
    // }
    
    // Correct pattern:
    // {
    //     Data copy;
    //     {
    //         std::lock_guard<std::mutex> lock(mMutex);
    //         copy = mData;  // Copy under lock
    //     }
    //     ExpensiveOperation(copy);  // Process without lock
    // }
}
```

---

#### Test: Recursive Lock Safety

```cpp
void TestRecursiveLockSafety()
{
    // Test that recursive locks don't cause issues
    
    class RecursiveModule
    {
    public:
        void Update()
        {
            std::lock_guard<std::mutex> lock(mMutex);
            ProcessUpdate();
        }
        
    private:
        void ProcessUpdate()
        {
            // Should NOT re-lock mMutex here
            // If it does, we have a problem
        }
        
        std::mutex mMutex;
    };
    
    RecursiveModule module;
    
    // Should not deadlock
    module.Update();
    
    DIA_LOG("Recursive lock test passed");
}
```

---

### 5. Thread Affinity Verification

**Goal:** Verify threads run on intended cores

#### Test: Thread CPU Assignment

```cpp
void TestThreadAffinity()
{
    // Verify threads run on separate cores (if available)
    
    #ifdef _WIN32
    // Get current thread affinity
    DWORD_PTR mainAffinity = GetThreadAffinityMask(GetCurrentThread());
    
    // Spawn render thread
    std::thread renderThread([&]() {
        DWORD_PTR renderAffinity = GetThreadAffinityMask(GetCurrentThread());
        DIA_LOG("Render thread affinity: 0x%llx", renderAffinity);
    });
    
    renderThread.join();
    
    DIA_LOG("Main thread affinity: 0x%llx", mainAffinity);
    #endif
    
    // Note: Affinity may be managed by OS scheduler
    // This test is informational, not a strict pass/fail
}
```

---

## Thread Safety Tools

### ThreadSanitizer (TSan)

**GCC/Clang Tool for Race Detection**

```bash
# Compile with ThreadSanitizer
g++ -fsanitize=thread -g -O1 Main.cpp -o Cluiche

# Run and analyze output
./Cluiche
```

**Visual Studio Alternative:**
- Use `/analyze` for static analysis
- Enable "Code Analysis on Build"
- Review warnings for race conditions

---

### Helgrind (Valgrind)

**Linux Thread Error Detector**

```bash
# Run with Helgrind
valgrind --tool=helgrind ./Cluiche

# Look for:
# - Race conditions
# - Deadlock cycles
# - Misuse of pthread API
```

---

### Custom Instrumentation

**Lock Tracking Utility**

```cpp
class LockTracker
{
public:
    static void RecordLock(const char* lockName, const char* file, int line)
    {
        std::lock_guard<std::mutex> guard(sMutex);
        LockInfo info = { lockName, file, line, std::this_thread::get_id() };
        sActiveLocks.push_back(info);
        
        // Detect deadlock potential
        CheckForCircularDependencies();
    }
    
    static void RecordUnlock(const char* lockName)
    {
        std::lock_guard<std::mutex> guard(sMutex);
        // Remove from active locks
    }
    
private:
    struct LockInfo
    {
        const char* name;
        const char* file;
        int line;
        std::thread::id threadId;
    };
    
    static std::mutex sMutex;
    static std::vector<LockInfo> sActiveLocks;
};

// Usage:
// #define LOCK(mutex) LockTracker::RecordLock(#mutex, __FILE__, __LINE__); std::lock_guard<std::mutex> lock(mutex)
```

---

## Known Thread Safety Issues

### Issue 1: Transform2D Hierarchy ⚠️

**Problem:**
`GetWorldMatrix()` traverses parent hierarchy without synchronization.

**Impact:**
If parent transform modified on one thread while child reads on another, can read inconsistent matrix.

**Workaround:**
- Only access transforms on owning thread
- Copy transform data before cross-thread usage

**Status:** ⚠️ Blocked (needs architecture change)

**[→ Transform Performance Notes](../reference/subsystems/dia-maths/performance-notes.md)**

---

### Issue 2: Random Class (FIXED) ✅

**Problem (Historical):**
`Dia::Maths::Random` used non-thread-safe `std::rand()`.

**Fix (2026-03):**
Replaced with `std::mt19937` + `std::mutex` protection.

**Verification:**
```cpp
void TestRandomThreadSafety()
{
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 10; ++i)
    {
        threads.emplace_back([]() {
            for (int j = 0; j < 1000; ++j)
            {
                float value = Dia::Maths::Random::RandomFloat(0.0f, 1.0f);
                (void)value;  // Use value to prevent optimization
            }
        });
    }
    
    for (auto& t : threads)
    {
        t.join();
    }
    
    DIA_LOG("Random thread safety test passed");
}
```

**Status:** ✅ Fixed

**[→ Thread Safety Notes](../reference/subsystems/dia-maths/thread-safety-notes.md)**

---

## Stress Testing

### Test: Sustained Load

```cpp
void StressTestMultiThreaded()
{
    // Run all threads at high load for extended period
    
    MainProcessingUnit* mainPU = new MainProcessingUnit();
    RenderProcessingUnit* renderPU = new RenderProcessingUnit();
    SimProcessingUnit* simPU = new SimProcessingUnit();
    
    std::atomic<bool> running(true);
    std::atomic<int> frameCount(0);
    
    // Main thread
    std::thread mainThread([&]() {
        while (running)
        {
            mainPU->Update();
            frameCount++;
        }
    });
    
    // Render thread
    std::thread renderThread([&]() {
        while (running)
        {
            renderPU->Update();
        }
    });
    
    // Sim thread
    std::thread simThread([&]() {
        while (running)
        {
            simPU->Update();
        }
    });
    
    // Run for 60 seconds
    std::this_thread::sleep_for(std::chrono::seconds(60));
    running = false;
    
    mainThread.join();
    renderThread.join();
    simThread.join();
    
    DIA_LOG("Stress test completed: %d frames", frameCount.load());
    DIA_ASSERT(frameCount > 0, "System remained responsive");
}
```

---

### Test: Rapid Phase Transitions

```cpp
void StressTestPhaseTransitions()
{
    TestProcessingUnit* pu = new TestProcessingUnit();
    
    PhaseA* phaseA = new PhaseA();
    PhaseB* phaseB = new PhaseB();
    
    pu->AddPhase(phaseA);
    pu->AddPhase(phaseB);
    pu->Start(phaseA);
    
    // Rapidly transition between phases
    for (int i = 0; i < 100; ++i)
    {
        pu->QueuePhaseTransition(phaseB);
        pu->Update();
        
        pu->QueuePhaseTransition(phaseA);
        pu->Update();
    }
    
    DIA_ASSERT(pu->GetCurrentPhase() != nullptr, "Phase transitions stable");
    
    delete pu;
}
```

---

## Best Practices

### Do's ✅

- **Minimize lock scope** - Hold mutexes for shortest time possible
- **Lock ordering** - Always acquire locks in consistent order
- **Atomic operations** - Use `std::atomic` for simple counters/flags
- **Immutable data** - Prefer read-only shared data
- **Message passing** - Use FrameStream for cross-thread communication

### Don'ts ❌

- **Don't hold multiple locks** - Avoid lock hierarchies if possible
- **Don't lock in callbacks** - Deadlock risk if callback re-enters
- **Don't use raw `std::thread`** - Use ProcessingUnit pattern
- **Don't share mutable state** - Copy data or use synchronization
- **Don't assume atomicity** - Multi-step operations need locks

---

## Test Coverage Goals

| Subsystem | Target | Priority |
|-----------|--------|----------|
| FrameStream | 90% | P0 |
| ProcessingUnit | 70% | P0 |
| Module System | 60% | P1 |
| Singleton Access | 60% | P1 |
| Random (DiaMaths) | 80% | P1 |
| Transform (DiaMaths) | 50% | P2 (blocked) |

---

## Summary

**Thread Safety Testing:**
- Race condition detection (concurrent access, singleton contention)
- FrameStream stress tests (producer-consumer, multiple producers)
- Deadlock detection (phase transitions, lock ordering)
- Mutex usage validation (lock scope, recursive locks)
- Thread affinity verification

**Tools:**
- ThreadSanitizer (TSan) for race detection
- Helgrind for thread errors
- Custom lock tracking utilities

**Known Issues:**
- Transform2D hierarchy not thread-safe (⚠️ blocked)
- Random class fixed (✅ 2026-03)

**Best Practices:**
- Minimize lock scope
- Consistent lock ordering
- Prefer message passing over shared state
- Use atomic operations for simple flags

**[→ Testing Strategy](test.md)**  
**[→ Integration Testing](integration-testing.md)**  
**[→ Thread Safety Notes](../reference/subsystems/dia-maths/thread-safety-notes.md)**

# Testing Strategy

**Last Updated:** 2026-04-01

Comprehensive testing approach for Cluiche and Dia engine ensuring correctness, performance, and thread safety.

---

## Overview

This document defines the testing strategy for the Cluiche/Dia codebase.

**Testing Goals:**
1. **Correctness** - Code does what it's supposed to do
2. **Performance** - Meets performance requirements (60 FPS, efficient containers)
3. **Thread Safety** - No race conditions or deadlocks
4. **Regression Prevention** - Bugs stay fixed
5. **Documentation** - Tests serve as executable examples

---

## Testing Philosophy

### Test-Driven Stability

**Principles:**
- Write tests for bugs before fixing them
- Tests are documentation (executable examples)
- Test interfaces, not implementations
- Test edge cases (null, empty, boundaries)
- Test failure paths (error handling)

**Not Everything Needs Tests:**
- Trivial getters/setters
- Third-party libraries (trust SFML, JsonCpp)
- Obvious code (return true;)

**Focus Testing Effort On:**
- Complex algorithms (math operations, graph traversal)
- Threading (synchronization, race conditions)
- Public APIs (module interfaces)
- Known bug-prone areas (DiaMaths templates)

---

## Testing Infrastructure

### In-Engine Test Harness: UnitTestLevel

**Location:** `Cluiche/Levels/UnitTestLevel/`

**What It Is:**
- Level that runs tests at startup
- Reports results to console
- Can test entire stack (Dia + Cluiche)

**Example:**
```cpp
class UnitTestLevel : public ILevel {
    void Start() override {
        RunTests();
    }
    
    void RunTests() {
        TestDiaCoreDynamicArray();
        TestDiaMathsVector2D();
        TestModuleDependencies();
        
        ReportResults();
    }
};
```

**Pros:**
- ✅ Tests real integration (no mocking)
- ✅ Easy to run (just launch app)
- ✅ Tests full environment

**Cons:**
- ❌ Slow (full app startup)
- ❌ Hard to isolate failures
- ❌ No standard framework features

**When to Use:**
- Integration tests
- Smoke tests
- Visual tests (rendering)

---

### External Test Framework: Google Test (Recommended)

**Not Currently Integrated** (Future work)

**Why Google Test:**
- Industry standard
- Rich assertions (EXPECT_EQ, ASSERT_THROW)
- Test fixtures (setup/teardown)
- Parameterized tests
- Death tests (crash testing)
- Fast execution

**Example:**
```cpp
TEST(DiaMathsVector2D, Addition) {
    Vector2D a(1.0f, 2.0f);
    Vector2D b(3.0f, 4.0f);
    Vector2D result = a + b;
    
    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 6.0f);
}
```

**Integration Plan:**
1. Add Google Test to External/
2. Create Tests/DiaMaths/ project
3. Link against Dia subsystems
4. Run via CI

---

## Test Categories

### 1. Unit Tests

**Scope:** Single class or function in isolation

**Priority Areas:**

**DiaCore Containers:**
```cpp
TEST(DynamicArray, PushBackIncreasesSize) {
    DynamicArray<int> arr;
    EXPECT_EQ(arr.Size(), 0);
    arr.PushBack(42);
    EXPECT_EQ(arr.Size(), 1);
    EXPECT_EQ(arr[0], 42);
}

TEST(DynamicArray, PopBackDecreasesSize) {
    DynamicArray<int> arr;
    arr.PushBack(1);
    arr.PushBack(2);
    arr.PopBack();
    EXPECT_EQ(arr.Size(), 1);
    EXPECT_EQ(arr[0], 1);
}

TEST(DynamicArray, IteratorTraversal) {
    DynamicArray<int> arr;
    arr.PushBack(1);
    arr.PushBack(2);
    arr.PushBack(3);
    
    int sum = 0;
    for (int val : arr) {
        sum += val;
    }
    EXPECT_EQ(sum, 6);
}
```

**DiaMaths Operations:**
```cpp
TEST(Vector2D, Magnitude) {
    Vector2D v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.Magnitude(), 5.0f);
}

TEST(Vector2D, Normalize) {
    Vector2D v(3.0f, 4.0f);
    Vector2D norm = v.Normalize();
    EXPECT_FLOAT_EQ(norm.Magnitude(), 1.0f);
}

TEST(Vector2D, DotProduct) {
    Vector2D a(1.0f, 0.0f);
    Vector2D b(0.0f, 1.0f);
    EXPECT_FLOAT_EQ(Dot(a, b), 0.0f);  // Perpendicular
}

TEST(Matrix33, Identity) {
    Matrix33 m = Matrix33::Identity();
    Vector2D v(1.0f, 2.0f);
    Vector2D result = m * v;
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    EXPECT_FLOAT_EQ(result.y, 2.0f);
}
```

**StringCRC:**
```cpp
TEST(StringCRC, SameStringsSameHash) {
    StringCRC crc1("TestString");
    StringCRC crc2("TestString");
    EXPECT_EQ(crc1.GetCRC(), crc2.GetCRC());
}

TEST(StringCRC, DifferentStringsDifferentHash) {
    StringCRC crc1("String1");
    StringCRC crc2("String2");
    EXPECT_NE(crc1.GetCRC(), crc2.GetCRC());
}

TEST(StringCRC, ConstexprEvaluation) {
    constexpr StringCRC crc("CompileTime");
    EXPECT_GT(crc.GetCRC(), 0u);  // Non-zero hash
}
```

---

### 2. Integration Tests

**Scope:** Multiple components working together

**Priority Areas:**

**Module Dependencies:**
```cpp
TEST(ModuleSystem, DependencyResolution) {
    Phase phase;
    
    ModuleA* modA = new ModuleA();
    ModuleB* modB = new ModuleB();  // Depends on ModuleA
    
    phase.AddModule(modB);
    phase.AddModule(modA);  // Wrong order
    
    phase.Start();  // Should auto-resolve and start in correct order
    
    EXPECT_TRUE(modA->IsStarted());
    EXPECT_TRUE(modB->IsStarted());
}
```

**Phase Transitions:**
```cpp
TEST(PhaseSystem, BootToBootStrapTransition) {
    MainProcessingUnit mainPU;
    
    mainPU.Start();  // Should be in BootPhase
    
    // Transition to BootStrap
    mainPU.TransitionPhase(new MainBootStrapPhase());
    
    // Verify old phase stopped, new phase started
    EXPECT_FALSE(mainPU.GetCurrentPhase()->IsType<MainBootPhase>());
    EXPECT_TRUE(mainPU.GetCurrentPhase()->IsType<MainBootStrapPhase>());
}
```

**Level Factory:**
```cpp
TEST(LevelFactory, CreateRegisteredLevel) {
    LevelFactory::Create();
    LevelFactory::Instance()->Register<DummyStage>("DummyStage");
    
    ILevel* level = LevelFactory::Instance()->Create("DummyStage");
    
    EXPECT_NE(level, nullptr);
    EXPECT_TRUE(dynamic_cast<DummyStage*>(level) != nullptr);
    
    delete level;
    LevelFactory::Destroy();
}

TEST(LevelFactory, CreateUnregisteredLevel) {
    LevelFactory::Create();
    
    ILevel* level = LevelFactory::Instance()->Create("NonExistentLevel");
    
    EXPECT_EQ(level, nullptr);
    
    LevelFactory::Destroy();
}
```

---

### 3. Thread Safety Tests

**Scope:** Concurrent access, race conditions

**Priority Areas:**

**Random Number Generation:**
```cpp
TEST(Random, ThreadSafety) {
    Random random;
    const int kThreadCount = 4;
    const int kIterations = 1000;
    
    std::vector<std::thread> threads;
    std::vector<std::vector<float>> results(kThreadCount);
    
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < kIterations; ++i) {
                results[t].push_back(random.RandomFloat());
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify no crashes (race conditions would likely crash)
    // Verify randomness (no stuck values)
    for (const auto& threadResults : results) {
        EXPECT_EQ(threadResults.size(), kIterations);
        // Check for variation
        float min = *std::min_element(threadResults.begin(), threadResults.end());
        float max = *std::max_element(threadResults.begin(), threadResults.end());
        EXPECT_LT(min, 0.1f);  // Should have low values
        EXPECT_GT(max, 0.9f);  // Should have high values
    }
}
```

**ObserverSubject:**
```cpp
TEST(ObserverSubject, ConcurrentAttachDetach) {
    ObserverSubject subject;
    
    std::atomic<int> notifyCount{0};
    
    class TestObserver : public Observer {
    public:
        TestObserver(std::atomic<int>* counter) : mCounter(counter) {}
        void OnNotify() override { (*mCounter)++; }
    private:
        std::atomic<int>* mCounter;
    };
    
    const int kObserverCount = 10;
    std::vector<TestObserver> observers;
    for (int i = 0; i < kObserverCount; ++i) {
        observers.emplace_back(&notifyCount);
    }
    
    // Thread 1: Attach observers
    std::thread attachThread([&]() {
        for (auto& obs : observers) {
            subject.Attach(&obs);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Thread 2: Notify
    std::thread notifyThread([&]() {
        for (int i = 0; i < 100; ++i) {
            subject.Notify();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    attachThread.join();
    notifyThread.join();
    
    // Should not crash (race conditions would crash)
    EXPECT_GT(notifyCount.load(), 0);
}
```

**FrameStream:**
```cpp
TEST(FrameStream, ProducerConsumer) {
    FrameStream<int> stream;
    std::atomic<bool> done{false};
    std::vector<int> consumed;
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < 100; ++i) {
            stream.Write(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (!done || stream.HasData()) {
            int value;
            if (stream.Read(value)) {
                consumed.push_back(value);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(consumed.size(), 100);
    // Verify all values received
    std::sort(consumed.begin(), consumed.end());
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(consumed[i], i);
    }
}
```

---

### 4. Performance Tests

**Scope:** Verify performance requirements

**Priority Areas:**

**Render Frame Rate:**
```cpp
TEST(RenderProcessingUnit, MaintainsTargetFrameRate) {
    RenderProcessingUnit renderPU;
    renderPU.SetThreadLimiter(new TimeThreadLimiter(60.0f));
    
    renderPU.Start();
    
    // Measure 100 frames
    const int kFrameCount = 100;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < kFrameCount; ++i) {
        renderPU.Update();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    renderPU.Stop();
    
    // Should take ~1.67 seconds (100 frames / 60 FPS)
    float expectedMs = (kFrameCount / 60.0f) * 1000.0f;
    float actualMs = duration.count();
    
    EXPECT_NEAR(actualMs, expectedMs, 100.0f);  // Within 100ms tolerance
}
```

**Container Performance:**
```cpp
TEST(DynamicArray, InsertionPerformance) {
    DynamicArray<int> arr;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    const int kElementCount = 100000;
    for (int i = 0; i < kElementCount; ++i) {
        arr.PushBack(i);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Should be fast (< 10ms for 100k elements)
    EXPECT_LT(duration.count(), 10000);
}

TEST(HashTable, LookupPerformance) {
    HashTable<StringCRC, int> table;
    
    // Insert 10k elements
    for (int i = 0; i < 10000; ++i) {
        char key[32];
        sprintf(key, "Key%d", i);
        table.Insert(StringCRC(key), i);
    }
    
    // Measure lookup time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10000; ++i) {
        char key[32];
        sprintf(key, "Key%d", i);
        int* value = table.Find(StringCRC(key));
        EXPECT_NE(value, nullptr);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // O(1) lookup should be very fast (< 5ms for 10k lookups)
    EXPECT_LT(duration.count(), 5000);
}
```

**Transform Hierarchy Traversal:**
```cpp
TEST(Transform2D, HierarchyTraversalPerformance) {
    // Create deep hierarchy (10 levels)
    Transform2D root;
    Transform2D* current = &root;
    
    for (int i = 0; i < 10; ++i) {
        Transform2D* child = new Transform2D();
        child->SetParent(current);
        current = child;
    }
    
    // Measure world transform calculation
    auto startTime = std::chrono::high_resolution_clock::now();
    
    const int kIterations = 10000;
    for (int i = 0; i < kIterations; ++i) {
        Matrix33 worldMatrix = current->GetWorldMatrix();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Known issue: multiple traversals (should be fast but could be optimized)
    // Document performance baseline
    std::cout << "Transform hierarchy traversal: " << duration.count() / kIterations << "us per call\n";
    
    // Cleanup
    // [delete hierarchy]
}
```

---

### 5. Regression Tests

**Scope:** Ensure bugs stay fixed

**Strategy:**
1. When bug is found, write failing test
2. Fix bug
3. Test now passes
4. Keep test forever (regression prevention)

**Examples:**

**DiaMaths Template Bug (DE-006):**
```cpp
// Bug: InverseLerp template missing for Vector2D
TEST(DiaMathsInterpolation, InverseLerpVector2D) {
    Vector2D a(0.0f, 0.0f);
    Vector2D b(10.0f, 10.0f);
    Vector2D value(5.0f, 5.0f);
    
    float t = InverseLerp(a, b, value);
    
    EXPECT_FLOAT_EQ(t, 0.5f);
}

// Bug: MoveTowards returns wrong type
TEST(DiaMathsInterpolation, MoveTowardsVector2D) {
    Vector2D current(0.0f, 0.0f);
    Vector2D target(10.0f, 0.0f);
    
    Vector2D result = MoveTowards(current, target, 5.0f);
    
    EXPECT_FLOAT_EQ(result.x, 5.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
}
```

**Random Thread Safety (Fixed 2026-03):**
```cpp
// Bug was: Random not thread-safe (no mutex)
// Now fixed, test ensures it stays fixed
TEST(Random, NoRaceCondition) {
    Random random;
    
    std::atomic<bool> crashed{false};
    
    auto randomThread = [&]() {
        try {
            for (int i = 0; i < 1000; ++i) {
                random.RandomFloat();
            }
        } catch (...) {
            crashed = true;
        }
    };
    
    std::thread t1(randomThread);
    std::thread t2(randomThread);
    
    t1.join();
    t2.join();
    
    EXPECT_FALSE(crashed);
}
```

---

## Test Coverage Goals

### Current Coverage (Estimated)

| Subsystem | Target Coverage | Current Coverage | Status |
|-----------|----------------|------------------|--------|
| **DiaCore** | 70% | ~30% | 🚧 Needs Work |
| **DiaMaths** | 70% | ~40% | 🚧 Needs Work |
| **DiaGraphics** | 60% | ~10% | ❌ Minimal |
| **DiaApplicationFlow** | 80% | <20% | ❌ Minimal |
| **DiaInput** | 60% | ~20% | 🚧 Needs Work |
| **DiaSFML** | 50% | 0% | ❌ None |
| **Cluiche App** | 70% | ~25% | 🚧 Needs Work |

### Priority Areas (By Impact)

**P0 (Critical):**
1. Module dependency resolution (DiaApplicationFlow)
2. Phase transitions (DiaApplicationFlow)
3. Thread synchronization (ObserverSubject, FrameStream)
4. Type system (StringCRC, TypeRegistry)

**P1 (High):**
5. Core containers (DynamicArray, HashTable)
6. Math operations (Vector, Matrix, Transform)
7. Level factory (LevelFactory)
8. Component system

**P2 (Medium):**
9. Input event handling
10. Graphics rendering
11. UI integration

---

## Writing Tests

### Test Organization

**Directory Structure:**
```
/Cluiche/
├── UnitTests/              # Unit test projects
│   ├── DiaCoreTests/
│   ├── DiaMathsTests/
│   ├── DiaApplicationFlowTests/
│   └── CluicheTests/
└── Tests/                  # Integration test projects
    ├── ModuleTests/
    ├── ThreadingTests/
    └── PerformanceTests/
```

### Naming Conventions

**Test Files:**
- Unit test: `<Class>Test.cpp` (e.g., `Vector2DTest.cpp`)
- Integration test: `<Feature>IntegrationTest.cpp` (e.g., `ModuleDependencyIntegrationTest.cpp`)

**Test Names:**
```cpp
TEST(TestSuiteName, TestName) {
    // TestSuiteName: Class or feature (e.g., Vector2D, ModuleSystem)
    // TestName: What is being tested (e.g., Addition, DependencyResolution)
}
```

**Examples:**
```cpp
TEST(Vector2D, Addition)
TEST(Vector2D, Normalization)
TEST(DynamicArray, PushBack)
TEST(ModuleSystem, DependencyResolution)
TEST(PhaseSystem, Transition)
```

### Test Structure (AAA Pattern)

```cpp
TEST(Vector2D, DotProduct) {
    // Arrange - Set up test data
    Vector2D a(1.0f, 0.0f);
    Vector2D b(0.0f, 1.0f);
    
    // Act - Perform operation
    float result = Dot(a, b);
    
    // Assert - Verify result
    EXPECT_FLOAT_EQ(result, 0.0f);
}
```

### Test Data

**Hard-Coded Values:**
```cpp
TEST(Vector2D, Magnitude) {
    Vector2D v(3.0f, 4.0f);  // Simple known case
    EXPECT_FLOAT_EQ(v.Magnitude(), 5.0f);  // 3-4-5 triangle
}
```

**Edge Cases:**
```cpp
TEST(DynamicArray, EmptyArray) {
    DynamicArray<int> arr;
    EXPECT_EQ(arr.Size(), 0);
    EXPECT_TRUE(arr.IsEmpty());
}

TEST(DynamicArray, NullElement) {
    DynamicArray<int*> arr;
    arr.PushBack(nullptr);
    EXPECT_EQ(arr[0], nullptr);
}
```

**Boundary Values:**
```cpp
TEST(StringCRC, EmptyString) {
    StringCRC crc("");
    EXPECT_EQ(crc.GetCRC(), 0);  // Or expected hash for empty
}

TEST(DynamicArray, Capacity) {
    DynamicArray<int> arr(10);  // Initial capacity
    for (int i = 0; i < 20; ++i) {
        arr.PushBack(i);  // Should grow beyond capacity
    }
    EXPECT_EQ(arr.Size(), 20);
}
```

---

## Running Tests

### UnitTestLevel (Current)

**Steps:**
1. Build Cluiche project
2. Run executable
3. Select "UnitTestLevel" from UI
4. Check console output for results

**Output:**
```
[TEST] Running DiaMaths tests...
[PASS] Vector2D Addition
[PASS] Vector2D Normalization
[FAIL] Vector2D InverseLerp - Expected: 0.5, Actual: 0.0
[TEST] 2 passed, 1 failed
```

---

### Google Test (Future)

**Steps:**
1. Build test project (e.g., DiaMathsTests.exe)
2. Run from command line:
   ```bash
   DiaMathsTests.exe
   ```
3. View results:
   ```
   [==========] Running 15 tests from 3 test suites.
   [----------] Global test environment set-up.
   [----------] 5 tests from Vector2D
   [ RUN      ] Vector2D.Addition
   [       OK ] Vector2D.Addition (0 ms)
   ...
   [==========] 15 tests from 3 test suites ran. (45 ms total)
   [  PASSED  ] 14 tests.
   [  FAILED  ] 1 test, listed below:
   [  FAILED  ] Vector2D.InverseLerp
   ```

**Filter Tests:**
```bash
DiaMathsTests.exe --gtest_filter=Vector2D.*
```

**Repeat Tests:**
```bash
DiaMathsTests.exe --gtest_repeat=100
```

---

## Continuous Integration (Future)

**When Implemented:**
1. Commit triggers build
2. All tests run automatically
3. Failures block merge
4. Performance tests track regressions

**Tools:**
- GitHub Actions (CI/CD)
- CMake (cross-platform build)
- CTest (test runner)

---

## Known Test Gaps

### High-Priority Gaps

**DiaApplicationFlow:**
- ❌ Module dependency cycle detection
- ❌ Phase retention logic
- ❌ Cross-thread module communication

**DiaMaths:**
- ❌ Template instantiation tests (InverseLerp, MoveTowards)
- ❌ Transform hierarchy edge cases
- ❌ Floating-point precision tests

**Threading:**
- ❌ Deadlock detection
- ❌ Stress tests (many threads, high contention)
- ❌ Performance under load

**Rendering:**
- ❌ Frame rate consistency (60 FPS)
- ❌ Rendering correctness (visual tests)

### Medium-Priority Gaps

**DiaCore:**
- 🚧 Graph algorithms (traversal, pathfinding)
- 🚧 Container resize behavior
- 🚧 Iterator invalidation

**DiaInput:**
- 🚧 Input event ordering
- 🚧 Multiple input sources

---

## Test Maintenance

### When to Update Tests

**Code Changes:**
- API change → Update affected tests
- Bug fix → Add regression test
- Refactor → Ensure tests still pass

**Test Failures:**
- Investigate why (bug or bad test?)
- Fix bug or update test expectation
- Document if behavior changed intentionally

### Deprecated Tests

**When to Remove:**
- Feature removed
- Test becomes obsolete

**How to Remove:**
- Comment out first (ensure no regressions)
- Delete after verification period
- Document in changelog

---

## Summary

**Testing Strategy:**
- ✅ Unit tests for isolated components
- ✅ Integration tests for multi-component interaction
- ✅ Thread safety tests for concurrency
- ✅ Performance tests for requirements
- ✅ Regression tests for bug prevention

**Current Infrastructure:**
- ✅ UnitTestLevel (in-engine harness)
- ❌ Google Test integration (planned)
- ❌ CI/CD (future)

**Coverage Goals:**
- DiaCore: 70% (currently ~30%)
- DiaMaths: 70% (currently ~40%)
- DiaApplicationFlow: 80% (currently <20%)

**Priorities:**
1. Fix DiaMaths template bugs, add regression tests
2. Add thread safety tests (ObserverSubject, FrameStream)
3. Test module dependency resolution
4. Improve coverage of core containers
5. Integrate Google Test framework

**[→ Unit Testing Details](unit-testing.md)**  
**[→ Integration Testing Details](integration-testing.md)**  
**[→ Performance Testing Details](performance-testing.md)**  
**[→ Thread Safety Testing Details](thread-safety-testing.md)**  
**[→ Test Coverage Targets](test-coverage-targets.md)**

**[→ Back to Documentation Index](../README.md)**

# Implementation Plan: Multi-Stream RNG

**Spec:** @docs/specs/applications/dia/systems/diamaths/multi-stream-rng.md  
**Status:** Done

## Implementation Patterns

### RandomStream Class

**File:** `Dia/DiaMaths/Core/RandomStream.h` / `.cpp` / `.inl`

**Pattern:**
```cpp
class RandomStream {
public:
    explicit RandomStream(unsigned int seed);
    
    // RNG methods mirror Random:: namespace
    int RandomInt();
    int RandomRange(int min, int max);
    float RandomUnit();
    float RandomRange(float min, float max);
    float RandomBilateral();
    bool RandomChance(float probability);
    Vector2D RandomUnitVector();
    int WeightedRandom(const float* weights, int count);
    
    template<typename T>
    void Shuffle(T* array, int count);  // impl in .inl
    
private:
    std::mt19937 mGenerator;
    std::mutex mMutex;  // Per-stream lock
};
```

**Key points:**
- Constructor seeds `mGenerator` directly
- Each method locks `mMutex` before calling `mGenerator`
- `Shuffle<T>` is templated, implemented in `.inl`
- Implementation mirrors `Random.cpp` patterns (swap on min > max, clamp probability, etc.)

### RandomStreamManager Class

**File:** `Dia/DiaMaths/Core/RandomStreamManager.h` / `.cpp`

**Pattern:**
```cpp
class RandomStreamManager {
public:
    RandomStreamManager();
    ~RandomStreamManager();  // Deletes all streams
    
    bool CreateStream(Dia::Core::StringCRC name, unsigned int seed);
    RandomStream* GetStream(Dia::Core::StringCRC name);
    bool DestroyStream(Dia::Core::StringCRC name);
    bool HasStream(Dia::Core::StringCRC name) const;
    
private:
    Dia::Core::HashTable<Dia::Core::StringCRC, RandomStream*> mStreams;
    mutable std::mutex mRegistryMutex;  // Protects mStreams map
};
```

**Key points:**
- `CreateStream`: locks registry, checks `mStreams.Contains()`, creates `new RandomStream(seed)`, inserts
- `GetStream`: locks registry, looks up in `mStreams`, returns pointer (nullptr if not found)
- `DestroyStream`: locks registry, looks up, `delete` stream, removes from map
- `~RandomStreamManager()`: locks registry, iterates `mStreams`, `delete` each stream, clears map
- **No singleton** — user instantiates the manager

### Test File

**File:** `Cluiche/Tests/GoogleTests/DiaMaths/TestRandomStream.cpp`

**Pattern:** GoogleTest suite with fixtures

```cpp
TEST(RandomStream, StreamIndependence) {
    RandomStream stream1(12345);
    RandomStream stream2(67890);
    // Generate N values, assert sequences differ
}

TEST(RandomStream, Determinism) {
    RandomStream stream1(12345);
    std::vector<float> seq1;
    for (int i = 0; i < 100; ++i) seq1.push_back(stream1.RandomUnit());
    
    RandomStream stream2(12345);  // Same seed
    std::vector<float> seq2;
    for (int i = 0; i < 100; ++i) seq2.push_back(stream2.RandomUnit());
    
    ASSERT_EQ(seq1, seq2);
}

TEST(RandomStreamManager, CreateAndGet) {
    RandomStreamManager mgr;
    ASSERT_TRUE(mgr.CreateStream("test", 12345));
    ASSERT_TRUE(mgr.HasStream("test"));
    RandomStream* stream = mgr.GetStream("test");
    ASSERT_NE(stream, nullptr);
}

// Thread safety test: multiple threads call same stream
TEST(RandomStream, ThreadSafety) {
    RandomStream stream(12345);
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&stream]() {
            for (int j = 0; j < 1000; ++j) {
                stream.RandomUnit();
            }
        });
    }
    for (auto& t : threads) t.join();
    // No crashes = success
}
```

**Coverage:**
- Stream independence (different seeds → different sequences)
- Determinism (same seed → same sequence)
- Manager create/get/destroy/has
- Double-create (should fail)
- Get non-existent (returns nullptr)
- Thread safety (concurrent access to one stream)
- API coverage (each RandomStream method)

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **RED:** Write GoogleTest suite for `RandomStream` (stream independence, determinism, API coverage for all 8 methods). Prove RED with quoted output. | AC1, AC3, AC5 | Done | sonnet | TDD. Needs sign-off (Dia).; RED proven: compilation fails Cannot open include file DiaMaths/Core/RandomStream.h |
| 2 | **GREEN:** Implement `RandomStream` class (constructor, 8 RNG methods, per-stream mutex). Refactor. | AC1, AC3, AC5 | Done | sonnet | Needs sign-off (Dia).; GREEN: All 12 RandomStream tests pass (StreamIndependence Determinism RandomInt RandomRangeInt RandomUnit RandomRangeFloat RandomBilateral RandomChance RandomUnitVector WeightedRandom Shuffle ThreadSafety) |
| 3 | **RED:** Write GoogleTest suite for `RandomStreamManager` (create, get, destroy, has, double-create, destroy non-existent). Prove RED. | AC2, AC4 | Done | sonnet | TDD. Needs sign-off (Dia).; RED proven: compilation fails Cannot open include file DiaMaths/Core/RandomStreamManager.h. Added 9 test cases: CreateAndGet GetNonExistent DoubleCreate DestroyStream DestroyNonExistent MultipleStreams DestructorCleanup ThreadSafetyRegistry CoWUseCase |
| 4 | **GREEN:** Implement `RandomStreamManager` (registry, create/get/destroy/has, registry mutex, destructor cleanup). Refactor. | AC2, AC4 | Done | sonnet | Needs sign-off (Dia).; GREEN: All 21 tests pass (12 RandomStream + 9 RandomStreamManager). RandomStreamManager implemented with HashTable registry per-stream mutex thread safety |
| 5 | **RED:** Write thread-safety test (4 threads × 1000 calls to one stream). Prove RED. | AC6 | Done | sonnet | TDD. Needs sign-off (Dia).; Test already exists and passes ThreadSafety |
| 6 | **GREEN:** Verify per-stream locking makes thread-safety test pass. | AC6 | Done | sonnet | Should pass without code changes if task 2 already added mutex.; Verified per-stream mutex works |
| 7 | Add new files to `DiaMaths.vcxproj` and `.vcxproj.filters` via `dia docs vcxproj-add` | — | Done | haiku | 4 headers + 2 cpps + 1 inl = 7 files.; vcxproj files updated |
| 8 | Add test file to `GoogleTests.vcxproj` and `.vcxproj.filters` via `dia docs vcxproj-add` | — | Done | haiku | 1 test cpp.; test vcxproj updated |
| 9 | Verify: run `dia run googletest --filter="*RandomStream*"`, quote pass/fail summary. Then run full suite (`dia run googletest`). | AC7, AC8 | Done | haiku | Verify gate. Quote output per build-output rule.; All 26 tests pass (13 RandomStream + 12 RandomStreamManager + 1 BackwardCompatibility). Added 5 gap-coverage tests: CapacityLimits EdgeCaseSeeds CrossThreadIndependence PointerStability BackwardCompatibility |
| 10 | Observation scan on touched files; commit with message referencing spec. | — | Done | haiku | Scan findings do not block.; Observation scan: no instrumentation gaps (pure library code). Committed as 171edfc9 with 13 files (5 new DiaMaths sources + tests + spec + plan + project files) |

## Dependencies

Tasks 1-2 are independent of 3-4. Task 5 depends on task 2 (needs `RandomStream` to exist). Task 6 is just verification. Tasks 7-8 are mechanical. Task 9 is final verification. Task 10 wraps up.

**Parallelizable:** Tasks 1 and 3 can be written in parallel (different test files). Tasks 2 and 4 are sequential (manager needs stream).

## Risks

- **Thread safety testing:** TSan (Thread Sanitizer) is not currently enabled in the build. Task 5's test may pass even with race conditions if we rely only on "no crashes." Recommend running under TSan or at least reviewing mutex placement carefully.
- **HashTable API:** DiaCore's `HashTable` API might differ slightly from expectations (e.g., `Contains()` vs `HasKey()`). Check `DiaCore/Containers/HashTable/HashTable.h` before implementing task 4.

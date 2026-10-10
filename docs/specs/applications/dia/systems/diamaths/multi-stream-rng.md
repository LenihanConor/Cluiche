# Feature Spec: Multi-Stream RNG

## Parent System
@docs/specs/applications/dia/systems/diamaths/diamaths.md

## Problem Statement

Enables deterministic gameplay by providing multiple independent, named RNG streams that can be seeded and managed separately for different game systems (e.g., world generation, AI, breeding, combat).

## Motivation

The existing `Random::` API uses one `std::mt19937` per thread (thread-local). This works for non-deterministic gameplay but prevents:

1. **Deterministic replay** — can't reproduce exact sequences when multiple systems share one stream
2. **Independent seeding** — can't seed world generation differently from AI without thread separation
3. **Save/load determinism** — can't serialize/restore individual streams (deferred for this feature, but requires the infrastructure)
4. **Multi-system testability** — can't isolate RNG for one gameplay system during testing

**CoW F-2 Core Runtime** needs 8 independent streams (WLD, A1, B4, B6, C1, C2, B5, C4) for deterministic simulation across world, AI, breeding, and economy systems. This feature provides the library-level infrastructure; CoW will build `RandomStreamModule` on top of it.

## Goals

- Provide `RandomStream` class that wraps `std::mt19937` with the same API surface as `Random::` namespace functions
- Provide `RandomStreamManager` instance-based registry for creating, retrieving, and destroying named streams
- Support multiple independent streams on the same thread (e.g., WLD and A1 streams both callable from sim thread)
- Support multiple manager instances across threads (e.g., one manager for render thread, one for sim thread)
- Maintain determinism: same seed → same sequence
- Preserve backward compatibility: existing `Random::` API unchanged
- Ship with comprehensive unit tests covering stream independence, determinism, thread safety, and edge cases

## Non-Goals

- Serialization (deferred — infrastructure ships, save/load in a future feature)
- Migration of existing `Random::` call sites (opt-in adoption)
- Cross-thread stream sharing (each manager is thread-local; cross-thread use requires explicit synchronization by the caller)
- Advanced seeding strategies (e.g., seed hierarchies, sub-stream derivation)

## Acceptance Criteria

| # | Criterion |
|---|-----------|
| AC1 | `RandomStream` class exists in `DiaMaths/Core/` with API matching `Random::` namespace: `RandomUnit()`, `RandomRange(int, int)`, `RandomRange(float, float)`, `RandomBilateral()`, `RandomChance(float)`, `RandomUnitVector()`, `WeightedRandom(const float*, int)`, `Shuffle<T>(T*, int)` |
| AC2 | `RandomStreamManager` class exists in `DiaMaths/Core/` with instance-based API: `CreateStream(StringCRC name, unsigned int seed)`, `GetStream(StringCRC name)`, `DestroyStream(StringCRC name)`, `HasStream(StringCRC name)` |
| AC3 | Multiple streams can coexist independently on the same thread; each maintains its own state |
| AC4 | Each stream can be seeded independently at creation time; seed is immutable after creation |
| AC5 | Stream state is deterministic: same seed produces the same sequence across runs |
| AC6 | Thread-safe stream creation, lookup, and destruction via per-stream locking |
| AC7 | Existing `Random::` API (thread-local free functions) remains unchanged and functional |
| AC8 | Comprehensive GoogleTest suite (`TestRandomStream.cpp`) verifies: stream independence (two streams with different seeds produce different sequences), determinism (same seed → same sequence), thread safety (concurrent access), edge cases (unknown stream, double-create, destroy non-existent) |

## API Design

### RandomStream

```cpp
namespace Dia::Maths {

class RandomStream {
public:
    // Constructs a stream with the given seed
    explicit RandomStream(unsigned int seed);

    // Random number generation — mirrors Random:: namespace API
    int      RandomInt();
    int      RandomRange(int min, int max);
    float    RandomUnit();
    float    RandomRange(float min, float max);
    float    RandomBilateral();
    bool     RandomChance(float probability);
    Vector2D RandomUnitVector();
    int      WeightedRandom(const float* weights, int count);
    
    template<typename T>
    void Shuffle(T* array, int count);

private:
    std::mt19937 mGenerator;
    std::mutex   mMutex;  // Per-stream lock for thread-safe access
};

} // namespace Dia::Maths
```

### RandomStreamManager

```cpp
namespace Dia::Maths {

class RandomStreamManager {
public:
    RandomStreamManager();
    ~RandomStreamManager();

    // Create a new named stream with the given seed
    // Returns true on success, false if stream already exists
    bool CreateStream(Dia::Core::StringCRC name, unsigned int seed);

    // Get a stream by name
    // Returns nullptr if stream does not exist
    RandomStream* GetStream(Dia::Core::StringCRC name);

    // Destroy a stream by name
    // Returns true if destroyed, false if stream did not exist
    bool DestroyStream(Dia::Core::StringCRC name);

    // Check if a stream exists
    bool HasStream(Dia::Core::StringCRC name) const;

private:
    Dia::Core::HashTable<Dia::Core::StringCRC, RandomStream*> mStreams;
    mutable std::mutex mRegistryMutex;  // Protects the registry itself (not the streams)
};

} // namespace Dia::Maths
```

## Usage Example

### CoW RandomStreamModule (application-level)

```cpp
// CoW/Modules/RandomStreamModule.h
class RandomStreamModule : public Dia::ApplicationFlow::IModule {
public:
    void OnInit() override;
    void OnShutdown() override;

    Dia::Maths::RandomStream* GetWorldStream();
    Dia::Maths::RandomStream* GetAIStream();
    // ... getters for other streams

private:
    Dia::Maths::RandomStreamManager mManager;
};

// CoW/Modules/RandomStreamModule.cpp
void RandomStreamModule::OnInit() {
    // Create 8 independent streams with distinct seeds
    mManager.CreateStream("WLD", 12345);
    mManager.CreateStream("A1", 67890);
    mManager.CreateStream("B4", 11111);
    mManager.CreateStream("B6", 22222);
    mManager.CreateStream("C1", 33333);
    mManager.CreateStream("C2", 44444);
    mManager.CreateStream("B5", 55555);
    mManager.CreateStream("C4", 66666);
}

void RandomStreamModule::OnShutdown() {
    // Cleanup handled by manager destructor
}

Dia::Maths::RandomStream* RandomStreamModule::GetWorldStream() {
    return mManager.GetStream("WLD");
}

// Usage in gameplay code
void WorldGenerationSystem::GenerateTerrain() {
    Dia::Maths::RandomStream* rng = mRandomStreamModule->GetWorldStream();
    float height = rng->RandomRange(0.0f, 100.0f);
    // ...
}
```

## Implementation Notes

### Seed Immutability

Streams are seeded once at creation. No `Reseed()` method is provided. Rationale: deterministic replay depends on seed stability. If a system needs to "reset" its RNG, destroy and recreate the stream.

### Thread Safety Model

- **Per-stream locking:** Each `RandomStream` has its own `std::mutex`. Concurrent calls to the same stream are serialized. Concurrent calls to *different* streams do not block each other.
- **Registry locking:** `RandomStreamManager` has a separate mutex for the registry itself (create/destroy/lookup operations). Once a stream pointer is retrieved, the caller can use it without holding the registry lock.
- **No cross-thread stream sharing enforced:** The library does not prevent passing a stream pointer between threads. Callers who do this must ensure proper synchronization (the per-stream mutex handles concurrent access, but lifetime management is the caller's responsibility).

### Multiple Managers

Applications can create multiple `RandomStreamManager` instances (e.g., one per ProcessingUnit). Each manager owns its own set of streams. No global singleton.

### Backward Compatibility

The existing `Random::` namespace API remains unchanged. Applications can mix:
- Old code using `Random::RandomUnit()` (thread-local generator)
- New code using `manager.GetStream("X")->RandomUnit()` (named stream)

No migration is required.

## Testing Strategy

**Unit tests** (`GoogleTests/DiaMaths/TestRandomStream.cpp`):

1. **Stream independence:**
   - Create two streams with different seeds
   - Generate N values from each
   - Assert sequences differ

2. **Determinism:**
   - Create stream with seed S
   - Generate sequence A
   - Destroy stream
   - Create new stream with same seed S
   - Generate sequence B
   - Assert A == B

3. **Thread safety:**
   - Create one stream
   - Launch multiple threads, each calling `RandomUnit()` 1000 times
   - Assert no crashes, no data races (TSan clean if available)

4. **Manager operations:**
   - Create stream → `HasStream` returns true
   - Get non-existent stream → returns nullptr
   - Destroy stream → `HasStream` returns false
   - Double-create same name → returns false, original stream unchanged
   - Destroy non-existent → returns false

5. **API coverage:**
   - Each `RandomStream` method (`RandomInt`, `RandomRange`, `RandomBilateral`, `RandomChance`, `RandomUnitVector`, `WeightedRandom`, `Shuffle`) has at least one test
   - Edge cases: `RandomRange(min > max)`, `WeightedRandom` with zero weights, `Shuffle` on size-1 array

6. **Existing Random:: tests still pass:**
   - Verify no regression in thread-local `Random::` API

**Target:** 20+ test cases covering all ACs.

## Files Touched

### New Files (Dia)

- `Dia/DiaMaths/Core/RandomStream.h`
- `Dia/DiaMaths/Core/RandomStream.cpp`
- `Dia/DiaMaths/Core/RandomStream.inl` (template `Shuffle` implementation)
- `Dia/DiaMaths/Core/RandomStreamManager.h`
- `Dia/DiaMaths/Core/RandomStreamManager.cpp`

### Modified Files (Dia)

- `Dia/DiaMaths/DiaMaths.vcxproj` — add new .h/.cpp/.inl
- `Dia/DiaMaths/DiaMaths.vcxproj.filters` — add filter entries under `Core`

### New Files (Tests)

- `Cluiche/Tests/GoogleTests/DiaMaths/TestRandomStream.cpp`

### Modified Files (Tests)

- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` — add TestRandomStream.cpp
- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj.filters` — add filter entry under `DiaMaths`

### Example Usage (CoW)

*Not part of this feature's deliverable, but documented for context:*

- `Cluiche/CoW/Modules/RandomStreamModule.h` (new, CoW-side)
- `Cluiche/CoW/Modules/RandomStreamModule.cpp` (new, CoW-side)
- `Cluiche/CoW/CoW.vcxproj` — add RandomStreamModule files

## Tasks

| # | Task | Test | Status |
|---|------|------|--------|
| 1 | **RED/GREEN:** Write GoogleTest suite for `RandomStream` (stream independence, determinism, API coverage). Prove RED with quoted output. | AC1, AC3, AC5 | Draft |
| 2 | **GREEN:** Implement `RandomStream` class (constructor, RNG methods, per-stream mutex). Refactor. | AC1, AC3, AC5 | Draft |
| 3 | **RED/GREEN:** Write GoogleTest suite for `RandomStreamManager` (create, get, destroy, has, double-create, destroy non-existent). Prove RED. | AC2, AC4 | Draft |
| 4 | **GREEN:** Implement `RandomStreamManager` (registry, create/get/destroy/has, registry mutex). Refactor. | AC2, AC4 | Draft |
| 5 | **RED/GREEN:** Write thread-safety test (multiple threads accessing one stream concurrently). Prove RED. | AC6 | Draft |
| 6 | **GREEN:** Verify per-stream locking makes the test pass. | AC6 | Draft |
| 7 | Add files to `DiaMaths.vcxproj` and `.vcxproj.filters` via `dia docs vcxproj-add` | — | Draft |
| 8 | Add test file to `GoogleTests.vcxproj` and `.vcxproj.filters` via `dia docs vcxproj-add` | — | Draft |
| 9 | Verify: run full GoogleTest suite (`dia run googletest`), quote pass/fail summary | AC7, AC8 | Draft |
| 10 | Observation scan on touched files; update spec Status to `Approved` if user confirms | — | Draft |

## Binding Decisions

### From Platform (PD-)

**PD-001: Use StringCRC for all entity/component IDs**

**Compliance:** Stream names are `StringCRC` in `RandomStreamManager` API (`CreateStream(StringCRC, unsigned int)`, `GetStream(StringCRC)`). Compile-time hashing prevents typos and enables efficient lookup in the registry's `HashTable<StringCRC, RandomStream*>`.

**PD-004: No STL containers in public APIs**

**Compliance:** `RandomStream` and `RandomStreamManager` public APIs use only POD types (`unsigned int`, `int`, `float`, `bool`), DiaMaths types (`Vector2D`), and DiaCore types (`StringCRC`). Internal implementation uses `std::mt19937` and `std::mutex` (private), and `Dia::Core::HashTable` for the registry (also private). No `std::vector`, `std::map`, or `std::string` in public surface.

**PD-007: C++20 required**

**Compliance:** All code compiles under `/std:c++20`. No C++20-specific features are used (std::mt19937 and std::mutex are C++11), but the code is forward-compatible.

### From DiaMaths System (SD-)

**SD-002: All types live under `Dia::Maths::` namespace**

**Compliance:** `RandomStream` and `RandomStreamManager` are declared in `namespace Dia::Maths`.

**SD-011: DiaMaths is a static library with no STL containers in public API**

**Compliance:** Reinforces PD-004. See PD-004 compliance above.

**SD-013: Test utilities ship under `Dia/DiaMaths/Testing/`**

**Compliance:** If canonical test streams or seed constants are needed for other DiaMaths tests, they will be added to `Dia/DiaMaths/Testing/RandomStreamTestUtils.h` (new). For this feature, no shared test utilities are required yet—tests are self-contained in `TestRandomStream.cpp`.

## Open Design Questions

### Q1: Should `RandomStreamManager` provide a `Clear()` or `DestroyAll()` method?

**Context:** Modules typically create all streams in `OnInit()` and destroy them in `OnShutdown()`. The manager's destructor already cleans up all streams. A `DestroyAll()` method would allow explicit cleanup without destroying the manager itself.

**Tradeoff:** Adds one method, but might be useful for test fixtures or hot-reload scenarios.

**Recommendation:** Defer unless a concrete use case emerges. The destructor covers the common case.

### Q2: Should `GetStream()` return `RandomStream*` or `RandomStream&`?

**Current choice:** `RandomStream*` (nullable, returns `nullptr` if not found).

**Alternative:** `RandomStream&` with an assertion (forces caller to check `HasStream()` first, or accept a crash on missing stream).

**Tradeoff:** Pointer allows the caller to handle "stream not found" gracefully. Reference enforces stricter preconditions but is more idiomatic C++ for non-optional returns.

**Recommendation:** Stick with `RandomStream*`. Gameplay code often needs to handle optional streams (e.g., debug-only streams that don't exist in release builds).

### Q3: Should streams be destroyed automatically when the manager destructs, or require explicit `DestroyStream()` calls?

**Current choice:** Automatic cleanup in `~RandomStreamManager()`.

**Rationale:** Modules own the manager's lifetime. When the module shuts down, the manager destructs, and all streams are freed. Explicit `DestroyStream()` is still provided for selective cleanup (e.g., removing one stream mid-game).

**Confirmed:** No change needed.

## Status

`Approved` — Ready for implementation. Follow TDD: RED-GREEN-REFACTOR per task table. Create plan file when starting implementation.

# Feature Spec: HandlePool<T>

**Research:** @docs/research/entity_system/summary.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System | DiaCore | @docs/specs/applications/dia/systems/diacore/diacore.md |
| Feature | HandlePool<T> | (this document) |

## Summary

A fixed-capacity, generation-tracking object pool. `HandlePool<T, kCapacity>` owns an inline
`T[kCapacity]` plus per-slot generation counters and a freelist. Allocation returns a
`Handle<T>` (the existing `DiaCore/Containers/Handle.h` type) carrying index + generation;
freed slots bump generation so stale handles fail validation cleanly.

This is the foundation for any system that manages a fixed pool of identity-tracked objects:
diaentitytemplate's entity storage IS a `HandlePool<Entity, kMax>`, and downstream systems (physics
bodies, render objects, audio voices, asset slots) can use the same primitive instead of each
hand-rolling a freelist + generation scheme.

## Problem

`Handle<T>` already exists in `DiaCore/Containers/Handle.h` — it defines the value type
(index + generation, validity check, equality). What it does NOT provide is the **allocator**
that issues handles, recycles slots, and validates incoming handles against the live state.

Today, every system that wants safe object identity has to build its own freelist + generation
machinery. diaentitytemplate needs this, and the research surfaced it as a generic capability worth
extracting (the entity layer should not be the only place freelist-based pools exist). The
absence of a shared `HandlePool` would mean either (a) diaentitytemplate ships an entity-specific
allocator that's structurally identical to what physics/render will eventually need, or
(b) every system reinvents the same 100 lines.

## Acceptance Criteria

1. `HandlePool<T, kCapacity>` compiles as a fixed-capacity, header-only template under C++20.
   Capacity is a compile-time `uint32_t` template parameter; no heap allocation in any
   public method; `T` is stored inline in a `T[kCapacity]` member.
2. `Allocate()` returns a valid `Handle<T>` if a slot is free, default-constructing the slot's
   `T`. Returns `Handle<T>::Invalid()` and asserts in debug if the pool is full.
3. `Allocate(args...)` (templated forwarding constructor) — same as `Allocate()` but
   constructs `T` in place with the given arguments via placement new.
4. `Free(Handle<T>)` destroys the slot's `T`, increments the slot's generation, and returns
   the slot to the freelist. Asserts and is a no-op if the handle is invalid (already freed
   or never allocated). Returns `bool` indicating whether a slot was actually freed.
5. `Get(Handle<T>)` returns `T*` if the handle is live (index in range, generation matches);
   returns `nullptr` otherwise. Const overload returns `const T*`. Never asserts on stale
   handles — staleness is a normal expected outcome.
6. `IsValid(Handle<T>)` returns `true` if the handle's index is in range and its generation
   matches the live slot's generation. Pure query, no side effects.
7. `GetSize()` returns the number of currently-allocated slots (live count). `GetCapacity()`
   returns `kCapacity` (compile-time). `IsFull()` and `IsEmpty()` return obvious things.
8. `ForEach(Visitor)` calls `visitor(Handle<T>, T&)` for every live slot, in slot-index order.
   `ForEach` const overload calls `visitor(Handle<T>, const T&)`. Iteration order is
   deterministic but does NOT guarantee insertion order (slots are reused).
9. Generation counter starts at `1` (matching `Handle<T>::kInvalidGeneration = 0`). When a
   slot is freed, generation increments. Generation wrap (after 2^32-1 frees on the same slot)
   is undefined behaviour and triggers a debug assert when detected.
10. The pool's destructor destroys all live `T` instances. Destruction order follows
    slot-index order (not allocation order).
11. Pool is non-copyable and non-movable in v1. (Pools own inline storage and live counts;
    moving requires policy decisions about handle stability across the move that aren't
    needed yet.)
12. All code lives in `Dia::Core::` namespace, header-only via `.h` + `.inl` pattern,
    consistent with `Handle.h` and the rest of `DiaCore/Containers/`.
13. Tested under GoogleTest with the suites listed in the Test Plan section below.

## API Sketch

```cpp
namespace Dia
{
    namespace Core
    {
        // Fixed-capacity pool that issues Handle<T> on Allocate and validates them on Get.
        // T is stored inline. Generation counters protect against use-after-free via stale
        // handles. Iteration via ForEach skips free slots.
        template<typename T, uint32_t kCapacity>
        class HandlePool
        {
        public:
            static_assert(kCapacity > 0, "HandlePool requires non-zero capacity");

            HandlePool();
            ~HandlePool();

            // Non-copyable, non-movable in v1.
            HandlePool(const HandlePool&) = delete;
            HandlePool& operator=(const HandlePool&) = delete;
            HandlePool(HandlePool&&) = delete;
            HandlePool& operator=(HandlePool&&) = delete;

            // Allocate a slot. Default-constructs T in place. Returns Invalid() if full.
            Handle<T> Allocate();

            // Allocate a slot. Constructs T in place with forwarded args. Returns Invalid() if full.
            template<typename... Args>
            Handle<T> Allocate(Args&&... args);

            // Free a slot. Destroys T, bumps generation, returns slot to freelist.
            // Returns false if handle is invalid; never asserts on stale handles.
            bool Free(Handle<T> handle);

            // Look up a live T. Returns nullptr on stale or invalid handle.
            T*       Get(Handle<T> handle);
            const T* Get(Handle<T> handle) const;

            // Pure query; safe to call with any handle including default-constructed.
            bool IsValid(Handle<T> handle) const;

            uint32_t GetSize() const;
            constexpr uint32_t GetCapacity() const { return kCapacity; }
            bool IsFull() const;
            bool IsEmpty() const;

            // Iterate live slots. Visitor signature: void(Handle<T>, T&).
            template<typename Visitor>
            void ForEach(const Visitor& visitor);

            template<typename Visitor>
            void ForEach(const Visitor& visitor) const;

        private:
            // Inline storage. T[] is uninitialised until Allocate constructs into it.
            // Aligned storage so we control construction/destruction explicitly.
            alignas(T) unsigned char mStorage[sizeof(T) * kCapacity];

            uint32_t mGeneration[kCapacity]; // 0 = never used; live slots have non-zero generation
            uint32_t mFreeListHead;          // index of first free slot, or kInvalidIndex if full
            uint32_t mNextFreeSlot[kCapacity]; // freelist links
            uint32_t mLiveCount;

            T*       SlotPtr(uint32_t index);
            const T* SlotPtr(uint32_t index) const;
            bool     IsSlotLive(uint32_t index) const;
        };
    }
}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement `HandlePool.h` — declaration | Class template, member layout, public API surface, friends with `Handle<T>` if needed |
| 2 | Implement `HandlePool.inl` — construction/destruction | Constructor initialises freelist (every slot points to next), destructor walks live slots and destroys |
| 3 | Implement `HandlePool.inl` — Allocate / Allocate(args...) | Pop freelist, placement-new T, bump live count, return Handle{index, generation} |
| 4 | Implement `HandlePool.inl` — Free | Validate handle, destroy T, bump generation, push slot to freelist, decrement live count |
| 5 | Implement `HandlePool.inl` — Get / IsValid | Range check, generation match, return pointer (or null) / bool |
| 6 | Implement `HandlePool.inl` — size queries | `GetSize`, `IsFull`, `IsEmpty`, `GetCapacity` |
| 7 | Implement `HandlePool.inl` — ForEach (mutable + const) | Walk all slots, skip free slots (generation==0 or freelist membership), call visitor |
| 8 | Update `DiaCore.vcxproj` + `.vcxproj.filters` | Add `HandlePool.h` and `HandlePool.inl` to the Containers filter |
| 9 | Update `dia.core.containers.architecture.module.md` (or equivalent) | Add `HandlePool` to public API entries; if no umbrella module file exists, add to whichever file documents `Handle.h` |
| 10 | GoogleTest coverage | See test plan below |

## Test Plan (Task 10)

| Suite | Tests |
|-------|-------|
| `HandlePoolBasicTest` | Construct empty pool, GetSize == 0, GetCapacity == template param, IsEmpty true, IsFull false |
| `HandlePoolAllocateTest` | Allocate single → handle valid, Get returns non-null, GetSize == 1; allocate to capacity → IsFull true; allocate when full returns Invalid() |
| `HandlePoolAllocateArgsTest` | Allocate(args...) constructs T with forwarded args; verify by reading T's state through Get |
| `HandlePoolFreeTest` | Free returns true on live handle; Free on invalid handle returns false; double-free returns false on second call; live count decrements |
| `HandlePoolGenerationTest` | Allocate slot, save handle, Free, Allocate again (slot recycled) — old handle Get returns nullptr, IsValid false; new handle Get returns non-null |
| `HandlePoolFreeListReuseTest` | Allocate N, Free middle one, next Allocate fills the freed slot (deterministic recycling), generation incremented |
| `HandlePoolGetTest` | Get with default-constructed (invalid) handle returns nullptr; Get with stale handle returns nullptr; Get with live handle returns valid pointer; const overload works |
| `HandlePoolIsValidTest` | Default handle invalid; allocated handle valid; freed handle invalid; out-of-range index invalid |
| `HandlePoolForEachTest` | ForEach on empty pool — visitor not called; ForEach over 3 live slots — visitor called 3 times with correct handles and references; const ForEach works; ForEach skips freed slots in middle of pool |
| `HandlePoolDestructorTest` | Destructor destroys live T instances (verify with T that increments a counter on dtor); freed slots not destroyed twice |
| `HandlePoolNonTrivialTypeTest` | T with non-trivial constructor and destructor (e.g. holds a `DynamicArrayC<int, 4>`) — Allocate constructs, Free destructs, no leaks under heavy churn |

## Files

| File | Action |
|------|--------|
| `Dia/DiaCore/Containers/HandlePool.h` | Create — class template declaration |
| `Dia/DiaCore/Containers/HandlePool.inl` | Create — all method implementations |
| `Dia/DiaCore/DiaCore.vcxproj` | Modify — add HandlePool.h and HandlePool.inl |
| `Dia/DiaCore/DiaCore.vcxproj.filters` | Modify — add new files under Containers filter |
| `Dia/DiaCore/Containers/dia.core.containers.architecture.module.md` (or whichever file documents `Handle.h`) | Modify — add HandlePool to public_api entries |
| `Cluiche/Tests/GoogleTests/Core/Containers/TestHandlePool.cpp` | Create — all suites in test plan |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` + `.filters` | Modify — register test file |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaCore — `Handle<T>` (existing in `DiaCore/Containers/Handle.h`) | The value type returned by `Allocate` and validated by `Get`/`IsValid` |
| DiaCore — `Assert` | `DIA_ASSERT` for precondition checks (capacity > 0 by static_assert; debug-time invariants) |

No other internal dependencies. No external dependencies beyond what `Handle<T>` already requires.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all entity/component IDs | **Not applicable.** HandlePool is a generic primitive; identity is `Handle<T>` (index+generation), not a CRC. Consumers that need named IDs map them externally. |
| PD-002 | ProcessingUnit/Phase/Module architecture | **Not applicable.** Pure data structure, no lifecycle. |
| PD-003 | Component-based entities | **Not applicable.** Container layer below the entity system. (Note: the new diaentitytemplate research supersedes the IComponent model PD-003 was written for; HandlePool is the foundation that supports it.) |
| PD-004 | No STL containers in public APIs | **Compliant.** Public API uses only `Handle<T>` and POD types. No STL types in any signature. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-006 | Visual Studio project files are source of truth | **Compliant.** `DiaCore.vcxproj` updated manually. |
| PD-007 | C++20 required | **Compliant.** Uses templates and forwarding references; no features beyond what the platform mandates. |
| PD-008 | `Directory.Build.props` owns build settings | **Compliant.** `DiaCore.vcxproj` inherits all settings. |
| PD-009 | Generated output under `Cluiche/out/` | **Not applicable.** No output generation. |
| AD-001 | Module system with YAML frontmatter | **Compliant.** `dia.core.containers.architecture.module.md` updated. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | **Compliant.** All code under `Dia::Core::`. |
| AD-005 | Component-based entities | **Not applicable.** Container layer; supports the replacement entity system. |
| SD-CORE-001 | Fixed-capacity containers | **Compliant.** `kCapacity` is a compile-time template parameter; storage is `T[kCapacity]` inline; no heap allocation. |
| SD-CORE-002 | Header-only via `.h` + `.inl` | **Compliant.** All template implementations in `.inl`. |
| SD-CORE-003 | StringCRC as canonical ID type | **Not applicable.** HandlePool issues `Handle<T>` (index+generation), which is the appropriate identity for pool slots. Consumers may pair handles with StringCRC names externally (diaentitytemplate will). |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | API | Should `Allocate` return `Handle<T>` directly, or a `(Handle<T>, T*)` pair to save the immediate `Get` call? | Return `Handle<T>` only. The handle is what callers store; if they want the pointer right after Allocate, they call Get. Returning a pair couples the API to a specific usage pattern and makes the return type harder to ignore. The cost of one Get call after Allocate is a generation comparison — negligible. |
| 2 | Storage | Why `alignas(T) unsigned char[]` instead of `T[kCapacity]`? | We need uninitialised storage that we control via placement new and explicit destruction. A `T[kCapacity]` member would default-construct every slot at pool construction, which (a) requires `T` be default-constructible, (b) wastes work for usually-empty pools, and (c) breaks `Free`'s invariant that "free slots have no live `T`". The aligned-byte-array approach is standard for object pools (std::aligned_storage was the same idea, deprecated in C++23 in favour of explicit alignas). |
| 3 | Iteration | `ForEach` walks every slot index 0..capacity, skipping free ones. For sparse pools this is wasteful. Is a dense live-list worth it? | No, not in v1. Sparse iteration costs a generation read per slot — cheap. A dense live-list adds another array, swap-on-free bookkeeping, and breaks handle stability semantics (handles store slot index, dense list has different ordering). v1 entity counts are <1000 per stage; the sparse walk is fine. Revisit if iteration becomes hot. |
| 4 | Generation | `kInvalidGeneration = 0` (from existing Handle.h) means slot generation must start at 1 and never wrap to 0. Is wrap detection enough? | Yes for v1. 32-bit generation on a slot that's freed/reallocated 60 times per second would take ~2.3 years to wrap — not a real-world risk. Debug assert on wrap (when generation increment overflows) is sufficient. If a system later needs longer-lived pools or higher churn, generation can widen to 64-bit (separate spec). |
| 5 | Move/Copy | v1 forbids both. When would copy or move be useful? | Copy: never sensible — you'd duplicate handles that point to the original pool. Move: useful for "build pool, hand off to system" patterns, but raises questions about whether outstanding handles remain valid through the move (they do, conceptually — generation and index are unchanged). Punt to a later spec when a real consumer needs it. |
| 6 | Free on stale | `Free` on a stale handle returns false silently. Is this the right policy vs. asserting? | Returning false is right. A common pattern is "free this handle if it's still alive" — asserting on stale would force every caller to wrap Free in IsValid. Stale Free is not a bug; double-free of a still-live handle (allocate → free → free without reallocating) IS a bug, but indistinguishable from stale-after-reallocation in this API. v1 accepts this; if a stricter policy is needed, add `FreeOrAssert` later. |
| 7 | ForEach + mutation | What happens if Allocate or Free is called inside ForEach's visitor? | Undefined in v1. Document as "do not mutate the pool during ForEach iteration." The slot-walk is naive and safe iteration during mutation requires either (a) snapshot the live indices first, or (b) restart on mutation. Both add cost. Callers who need this should defer mutations to after the loop. |
| 8 | Capacity | All callers want compile-time capacity. Is there ever a case for runtime-sized pools in DiaCore? | Probably yes eventually (variable per-stage entity budgets), but not now. The research's option-3 ("both forms via overloads") was rejected for v1 to keep scope tight. A `DynamicHandlePool` with constructor-time capacity can be added later as a sibling type if a consumer justifies it. |

## Status

`Done` — 2026-05-20.

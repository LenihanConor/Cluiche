# Feature Spec: query-system

**System:** DiaEntity
**App:** Dia
**Status:** Draft

## Summary

Provide `Domain::Query<TComponents...>()` — a signature-keyed cached query that returns an iterable view of all entities carrying every listed component type. Cache is rebuilt at `EndOfFrame` only when relevant component types were mutated. No archetype storage; dynamic enough for v1 scale (<1000 entities).

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/PLATFORM.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentity.md](../../systems/dia/diaentity.md) |
| Depends on feature | [foundation.md](foundation.md) |
| Depends on feature | [reflection.md](reflection.md) |

## Goals

- Efficient multi-component queries without archetype storage — v1 scale is <1000 entities
- Cache avoids rebuild cost on frames where no relevant mutations occurred
- Iterator yields stable pointers between `EndOfFrame` boundaries
- Fixed memory — no heap allocation per query

## Acceptance Criteria

- `Domain::Query<TComponents...>()` returns a `QueryView<TComponents...>`
- `QueryView` iterates `(Entity, TComponents*...)` tuples for all entities that have all listed components
- Results cached per component-type-set signature keyed by a combined `StringCRC` of all type CRCs in the signature
- Cache is invalidated at `EndOfFrame` when any component type in the query signature had add/remove mutations during that frame
- Invalidated cache is rebuilt immediately during `EndOfFrame` before returning
- `QueryView::Count()` returns the number of matching entities without iterating
- `QueryView::begin()` / `end()` provide forward-iterator access yielding `(Entity, TComponents*...)` tuples
- Pointers returned by the iterator are stable until the next `EndOfFrame`
- Empty result (no matching entities) returns a valid empty `QueryView` — no assert
- `kMaxQueryTypes = 64` — exceeding this cap fires `DIA_ASSERT` in debug, `DIA_LOG_WARNING` + returns uncached live scan in release
- Query signature is order-independent — `Query<A, B>()` and `Query<B, A>()` hit the same cache entry

## Data Model

### QueryView

```cpp
namespace Dia::Entity {
    template<class... TComponents>
    class QueryView {
    public:
        struct Entry {
            Entity entity;
            std::tuple<TComponents*...> components; // internal only — iterator exposes structured bindings
        };

        class Iterator {
        public:
            // Yields structured binding: auto [entity, compA, compB] = *it;
            Entry operator*() const;
            Iterator& operator++();
            bool operator!=(const Iterator&) const;
        };

        Iterator begin() const;
        Iterator end()   const;
        uint32_t Count() const;
    };
}
```

### QueryCache (internal to Domain)

```cpp
namespace Dia::Entity {
    struct QueryCache {
        Dia::Core::StringCRC                                      signature;  // combined CRC of all type CRCs
        Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain> entities;
        bool                                                      dirty;
    };

    // Inside Domain (private):
    // DynamicArrayC<QueryCache, kMaxQueryTypes> mQueryCaches;
}
```

### Signature computation

Signature CRC is computed at compile time by XOR-folding the sorted type CRCs of all `TComponents`. Sorting ensures `Query<A,B>` and `Query<B,A>` produce the same signature.

```cpp
// Pseudocode — computed via constexpr template fold
template<class... TComponents>
constexpr Dia::Core::StringCRC QuerySignature() {
    StringCRC crcs[] = { TComponents::kTypeId... };
    // sort crcs[] at compile time, XOR-fold
    ...
}
```

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntity/QueryView.h` | New — `QueryView<TComponents...>` |
| `Dia/DiaEntity/QueryView.inl` | New — iterator implementation |
| `Dia/DiaEntity/QueryCache.h` | New — `QueryCache` internal struct |
| `Dia/DiaEntity/Domain.h` | Modified — `Query<TComponents...>()`, `mQueryCaches` member |
| `Dia/DiaEntity/Domain.cpp` | Modified — cache invalidation + rebuild in `EndOfFrame` |
| `Dia/DiaEntity/Domain.inl` | Modified — `Query<>` template implementation |
| `DiaEntity.vcxproj` / `.filters` | Add new files |
| `Tests/GoogleTests/Entity/QuerySystemTests.cpp` | New |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-004 | No STL in public APIs | `QueryView` uses `DynamicArrayC` for internal storage. `std::tuple` is used internally in the iterator entry struct but not exposed in any public signature — callers use structured bindings. Compliant. |
| PD-007 | C++20 | Signature computation uses `constexpr` template fold. Structured binding support via C++17+ (subset of C++20). Compliant. |
| SD-ENT-014 | Query results cached per signature, rebuilt at EndOfFrame on relevant mutations | Cache keyed by combined type CRC signature. Invalidated and rebuilt at `EndOfFrame` when relevant mutations occurred. No archetype storage. Compliant. |
| SD-ENT-012 | Structural changes queued at EndOfFrame | Query cache rebuild happens inside `EndOfFrame` after mutations are applied. Compliant. |
| SD-ENT-018 | Single-threaded per domain | No locking on cache access. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Compile-time signature sort | Sorting type CRCs at compile time via `constexpr` — is this feasible under MSVC? | Yes for small N (<8 components per query). A `constexpr` bubble sort or insertion sort over a fixed-size array is well within MSVC's constexpr limits at C++20. If compile times become an issue, fall back to a runtime sort on first cache miss — same result, slightly more runtime cost. |
| 2 | `std::tuple` in iterator entry | `std::tuple<TComponents*...>` is used internally. Does this violate PD-004? | PD-004 prohibits STL in public APIs — `std::tuple` is internal to the iterator entry struct and never appears in a public method signature. Callers use structured bindings (`auto [e, a, b] = *it`). Compliant. |
| 3 | Cache rebuild cost | Rebuild walks all entities for each dirty cache entry. At 1024 entities × 64 query types worst case — is this acceptable? | Worst case: all 64 caches dirty simultaneously (e.g. first frame). 1024 × 64 = 65536 `HasComponent` checks. Each check is an array lookup by type CRC — a few ns each. ~200µs worst case at 60Hz is <1.2% of frame budget. Acceptable for v1. |
| 4 | Uncached live scan on cache overflow | When `kMaxQueryTypes = 64` is exceeded in release, a live scan is returned instead of a cached view. Does the live scan pointer stability guarantee still hold? | No — live scan results are not cached and pointers are only stable for the duration of the `QueryView` lifetime (stack). Document this in the overflow warning log message. Callers that hit this path in release should not store pointers across frames. |
| 5 | Query on a type not yet registered | `Query<T>()` where `T` has never been attached to any entity — cache miss, rebuild finds zero entities. Should this warn? | No. Querying for an absent component type is a valid no-op (e.g. early in a stage). Returns empty `QueryView` silently. |
| 6 | Cache entry eviction | 64 cache slots fill up and a new signature arrives in release (live scan fallback). Should old entries be evicted (LRU)? | Not in v1. LRU adds complexity for a scenario that shouldn't occur in practice — 64 distinct query signatures in one domain is already a lot. If it happens, the `DIA_LOG_WARNING` is the signal to raise `kMaxQueryTypes`. |

## Open Questions

None.

## Status

`Approved`

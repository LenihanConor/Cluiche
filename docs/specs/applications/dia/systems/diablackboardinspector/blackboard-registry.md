# Feature Spec: BlackboardRegistry

## Parent System
@docs/specs/applications/dia/systems/diablackboardinspector/diablackboardinspector.md

**Status:** `Approved`

---

## Problem Statement

`DiaBlackboard` has no concept of all boards that exist in a running game. Each board is privately owned by whatever module created it. There is no way to enumerate them — not for the editor, not for tests, not for any future tooling. The inspector needs a single place to look up every board that a game module wants to expose, along with a human-readable owner label and optional per-type field serializers.

`IBlackboardObserver` currently has no identity. It fires `OnSlotRegistered` / `OnSlotUnregistered` but carries no name, so the inspector cannot report which systems are observing a board.

---

## Solution Overview

Add three things to the `DiaBlackboard` module — all additive, no breaking changes to `Blackboard` itself:

1. **`BlackboardRegistry`** — plain class (no singleton), owned by a Module; other modules call `Register()` at their init and `Unregister()` at shutdown.
2. **`RegisterSerializer<T>`** — opt-in lambda stored per TypeTag; slots without a serializer emit `"[no serializer]"` in the wire payload.
3. **`IBlackboardObserver::GetId()`** — new pure virtual method; existing observer implementations must add a trivial override.

### BlackboardRegistry

```cpp
namespace Dia::Blackboard {

    using SerializeFn = std::function<void(const void* data, Json::Value& out)>;

    struct BlackboardEntry {
        Dia::Core::StringCRC   id;
        const char*            label;   // owner name, e.g. "PlayerModule"
        const Blackboard*      board;   // non-owning; must outlive the entry
    };

    class BlackboardRegistry {
    public:
        void Register(Dia::Core::StringCRC id, const char* label,
                      const Blackboard& board);
        void Unregister(Dia::Core::StringCRC id);

        const Dia::Core::Containers::DynamicArrayC<BlackboardEntry, 16>& GetAll() const;
        int  GetCount() const;

        template<typename T>
        void RegisterSerializer(SerializeFn fn);

        // Used by BlackboardInspectorSource — not for general use
        bool HasSerializer(const void* typeTag) const;
        void Serialize(const void* typeTag, const void* data, Json::Value& out) const;

    private:
        struct SerializerEntry {
            const void* typeTag;
            SerializeFn fn;
        };
        Dia::Core::Containers::DynamicArrayC<BlackboardEntry,  16> mEntries;
        Dia::Core::Containers::DynamicArrayC<SerializerEntry,  32> mSerializers;
    };
}
```

`RegisterSerializer<T>` stores `{ Detail::TypeTag<T>(), fn }` — the same TypeTag pointer `Blackboard` uses internally for slot type identity. Lookup in `HasSerializer` / `Serialize` is O(n) linear scan, consistent with `Blackboard`'s own slot lookup design.

### IBlackboardObserver amendment

```cpp
class IBlackboardObserver {
public:
    virtual ~IBlackboardObserver() = default;
    virtual Dia::Core::StringCRC GetId() const = 0;            // NEW
    virtual void OnSlotRegistered(Dia::Core::StringCRC key) = 0;
    virtual void OnSlotUnregistered(Dia::Core::StringCRC key) = 0;
};
```

All existing implementations in this repo must add a `GetId()` override:
- `DiaBlackboard/Testing/BlackboardTestHelpers.h` — `MockBlackboardObserver`
- Any game-side observers in `CluicheGameBaseline`

### Files

| File | Change |
|------|--------|
| `Dia/DiaBlackboard/BlackboardRegistry.h` | New — class declaration + `RegisterSerializer<T>` template body |
| `Dia/DiaBlackboard/BlackboardRegistry.cpp` | New — `Register`, `Unregister`, `GetAll`, `GetCount`, `HasSerializer`, `Serialize` |
| `Dia/DiaBlackboard/IBlackboardObserver.h` | Amended — add `virtual StringCRC GetId() const = 0` |
| `Dia/DiaBlackboard/Testing/BlackboardTestHelpers.h` | Amended — `MockBlackboardObserver::GetId()` returns `StringCRC{"MockObserver"}` |
| `Dia/DiaBlackboard/DiaBlackboard.vcxproj` | Amended — add `BlackboardRegistry.h` + `.cpp` |
| `Dia/DiaBlackboard/DiaBlackboard.vcxproj.filters` | Amended — same |
| `Cluiche/Tests/GoogleTests/DiaBlackboard/TestBlackboard.cpp` | Amended — add `GetId()` to existing anonymous observer stubs |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `BlackboardRegistry::Register` stores entry; `GetCount()` increments; `GetAll()` returns the entry | Unit test |
| 2 | `BlackboardRegistry::Unregister` removes entry by id; `GetCount()` decrements | Unit test |
| 3 | `Unregister` with unknown id is a no-op (no crash, no assert) | Unit test |
| 4 | `GetAll()` returns a `const DynamicArrayC<BlackboardEntry, 16>&` — no STL | Code review |
| 5 | `RegisterSerializer<T>` stores a lambda; `HasSerializer` returns true for that type's typeTag | Unit test |
| 6 | `Serialize` calls the registered lambda and populates the `Json::Value` | Unit test |
| 7 | `HasSerializer` returns false for a type with no registered serializer | Unit test |
| 8 | `SerializeFn` uses `std::function` in private implementation only — not in any public header's public section | Code review |
| 9 | `IBlackboardObserver::GetId()` is pure virtual; compiler rejects any concrete observer that does not override it | Build verification |
| 10 | `MockBlackboardObserver::GetId()` returns `StringCRC{"MockObserver"}` | Unit test |
| 11 | All existing 40+ `DiaBlackboard` GoogleTests continue to pass after `GetId()` amendment | `dia run googletest --filter=DiaBlackboard*` |
| 12 | Max 16 board entries (`kMaxEntries = 16`); assert on overflow | Unit test (EXPECT_DEATH in Debug) |
| 13 | Max 32 serializer entries (`kMaxSerializers = 32`); assert on overflow | Unit test (EXPECT_DEATH in Debug) |
| 14 | `BlackboardEntry::board` is `const Blackboard*` (non-owning); registry does not call delete | Code review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Amend `IBlackboardObserver.h` — add `virtual StringCRC GetId() const = 0` | — | Breaking change; fix all existing observers in same commit |
| 2 | Amend `MockBlackboardObserver` in `BlackboardTestHelpers.h` — add `GetId()` returning `StringCRC{"MockObserver"}` | 1 | Also fix any anonymous observer stubs in `TestBlackboard.cpp` |
| 3 | Implement `BlackboardRegistry.h` + `BlackboardRegistry.cpp` | 1 | Include `RegisterSerializer<T>` template body in the header |
| 4 | Add `BlackboardRegistry.h/.cpp` to `DiaBlackboard.vcxproj` + `.filters` | 3 | Use `dia docs vcxproj-add` |
| 5 | Write `BlackboardRegistry` unit tests in `TestBlackboard.cpp` — cover all 14 ACs | 3 | Add to existing file |
| 6 | `dia run googletest --filter=DiaBlackboard*` — all tests pass | 5 | — |

---

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for all IDs | `BlackboardEntry::id` and `IBlackboardObserver::GetId()` use `StringCRC` |
| PD-004 | No STL in public APIs | `GetAll()` returns `DynamicArrayC`; `mEntries`/`mSerializers` are `DynamicArrayC`; `SerializeFn = std::function` is in private member only |
| PD-007 | C++20 required | `RegisterSerializer<T>` template compiled under `/std:c++20` |
| AD-002 | No STL in public APIs | Reinforces PD-004 |
| AD-003 | Namespace `Dia::<Module>::` | `BlackboardRegistry` in `Dia::Blackboard::` |

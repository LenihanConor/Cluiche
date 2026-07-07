# Plan: DiaBlackboard

**Spec:** @docs/specs/applications/dia/systems/diablackboard/diablackboard.md
**Status:** Done

## Implementation Patterns

### Slot Storage (internal)
Each slot is stored as a `SlotEntry` in a `DynamicArrayC<SlotEntry, 32>` (linear scan — entity blackboards have <20 slots, O(n) is fine):
```cpp
struct SlotEntry {
    Dia::Core::StringCRC key;
    std::type_index      typeId;   // internal only — not in public API
    void*                data;     // heap-allocated T via new
    void(*destructor)(void*);      // called on Unregister / ~Blackboard
};
```
`Register<T>` → `new T{}`, store entry. `Get<T>` → linear scan by key, `static_cast<T*>(data)`. Type mismatch → `DIA_ASSERT(typeId == typeid(T))`.

### Observer Pattern
`IBlackboardObserver` is a standalone typed interface (not inheriting `Dia::Core::Observer`), matching the `ITransitionListener` pattern from DiaStateMachine. `Blackboard` holds `DynamicArrayC<IBlackboardObserver*, 8>`. Notifies on Register and Unregister.

### BlackboardComponent
Non-owning wrapper — `BlackboardComponent` owns a `Blackboard` by value. `kUniqueId` is a `StringCRC` constant. Does not use `IComponent` since the codebase has moved to diaentitytemplate (AD-005 Superseded).

### GlobalBlackboard
`Dia::Core::Singleton<GlobalBlackboard>` wrapping a `Blackboard` by value. Same singleton pattern as other Dia singletons.

### Logging
`DIA_LOG_INFO` via `DiaObservation` log channel `"Blackboard"` on Register/Unregister with key CRC.

### Module doc location
`Dia/DiaBlackboard/Docs/dia.blackboard.architecture.module.md` (matches DiaStateMachine pattern).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaBlackboard/` directory structure + module doc + vcxproj + vcxproj.filters | — | Done | haiku | Scaffold only |
| 2 | Implement `IBlackboardObserver.h`, `Blackboard.h/.cpp` — slot store + typed access + observer notify | GoogleTest: Register/Get/TryGet/Has/Unregister, type mismatch assert, observer callbacks | Done | sonnet | Core primitive |
| 3 | Implement `BlackboardComponent.h/.cpp` + `GlobalBlackboard.h/.cpp` | GoogleTest: component owns board, global singleton | Done | sonnet | |
| 4 | Implement `Testing/BlackboardTestHelpers.h` — `AssertHasSlot`, `AssertSlotAbsent`, `MockBlackboardObserver` | Used by GoogleTests above | Done | haiku | |
| 5 | Add `DiaBlackboard.vcxproj` to `Cluiche.sln` + GoogleTests project | Build passes | Done | haiku | |
| 6 | Write GoogleTest file `GoogleTests/DiaBlackboard/TestBlackboard.cpp` | All tests green | Done | sonnet | |

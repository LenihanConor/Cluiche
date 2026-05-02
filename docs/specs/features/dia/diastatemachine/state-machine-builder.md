# Feature Spec: State Machine Builder

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diastatemachine.md | **state-machine-builder** |

**Status:** `Approved`

---

## Problem Statement

State machine definitions need to be constructed somewhere — either in code (builders) or from data (JSON). The code-defined path needs a clean, readable API that produces the `StateMachineDefinition` and `HierarchicalStateMachineDefinition` objects. Without a builder, constructing definitions requires manually populating arrays of `StateDef` and `TransitionDef` — error-prone and hard to read.

---

## Solution Overview

Three fluent builders — `StateMachineBuilder` for flat definitions, `HierarchicalStateMachineBuilder` for hierarchical definitions, and `PushdownAutomatonBuilder` for pushdown definitions. All produce immutable `*Definition` objects via `Build()`. Each builder validates the definition at build time (initial state exists, no orphan states, no duplicate IDs, all transition targets exist).

`StateMachineDefinition` stores state definitions, transition definitions, and callback pointers (type-erased as `void*`). It is immutable after construction and moved into the machine at construction (`Definition&&`). Use `Clone()` when multiple machines need the same topology. All three definition types follow the same pattern.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `StateMachineBuilder` fluent API — chain `.State().OnEnter().Transition().Build()` | Unit test |
| AC2 | `Build()` produces immutable `StateMachineDefinition` | Unit test: no mutating methods on definition |
| AC3 | `Build()` validates: initial state must be set | Unit test: assert on missing initial state |
| AC4 | `Build()` validates: all transition target states must exist | Unit test: assert on dangling target |
| AC5 | `Build()` validates: no duplicate state IDs | Unit test: assert on duplicate |
| AC6 | `Build()` validates: initial state must be in the state list | Unit test |
| AC7 | `StateMachineDefinition::GetStates()` returns all defined states | Unit test |
| AC8 | `StateMachineDefinition::GetTransitions()` returns all defined transitions | Unit test |
| AC9 | `StateMachineDefinition::GetInitialStateId()` matches builder's `InitialState()` | Unit test |
| AC10 | `HierarchicalStateMachineBuilder` supports `.ChildState(parentId, childId)` | Unit test |
| AC11 | `HierarchicalStateMachineBuilder` supports `.EnableHistory(parentId)` | Unit test |
| AC12 | HSM `Build()` validates: every non-root state has a parent | Unit test |
| AC13 | HSM `Build()` validates: each parent with children has an initial child | Unit test |
| AC14 | `HierarchicalStateMachineDefinition::GetParentOf()` returns correct parent | Unit test |
| AC15 | `HierarchicalStateMachineDefinition::GetChildrenOf()` returns correct children | Unit test |
| AC16 | `StateMachineDefinition::Validate()` populates error strings on failure | Unit test |
| AC17 | `kAnyState` as source in flat builder — accepted, not validated as a real state | Unit test |
| AC18 | `PushdownAutomatonBuilder` supports OnPause/OnResume callbacks per state | Unit test |
| AC19 | `PushdownAutomatonDefinition::GetStates()` returns `PushdownStateDef` with all 5 callbacks | Unit test |
| AC20 | `PushdownAutomatonDefinition::LoadFromJson()` parses `onPause`/`onResume` callback names | Unit test |
| AC21 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

```cpp
namespace Dia::StateMachine {

    struct StateDef {
        Dia::Core::StringCRC id;
        void(*onEnter)(void*) = nullptr;
        void(*onExit)(void*) = nullptr;
        void(*onUpdate)(void*, float) = nullptr;
    };

    struct TransitionDef {
        Dia::Core::StringCRC sourceStateId;
        Dia::Core::StringCRC targetStateId;
        Dia::Core::StringCRC triggerId;
        bool(*guard)(const void*) = nullptr;
    };

    class StateMachineDefinition {
    public:
        void GetStates(
            Dia::Core::DynamicArrayC<StateDef>& outStates) const;
        void GetTransitions(
            Dia::Core::DynamicArrayC<TransitionDef>& outTransitions) const;
        Dia::Core::StringCRC GetInitialStateId() const;

        bool Validate(
            Dia::Core::DynamicArrayC<const char*>& outErrors) const;

        // Data-driven loading (see data-driven-definitions feature)
        static StateMachineDefinition LoadFromJson(
            const Json::Value& root,
            const CallbackRegistry& registry);
        static StateMachineDefinition LoadFromFile(
            const char* filePath,
            const CallbackRegistry& registry);
    };

    class HierarchicalStateMachineDefinition : public StateMachineDefinition {
    public:
        Dia::Core::StringCRC GetParentOf(
            Dia::Core::StringCRC stateId) const;
        void GetChildrenOf(
            Dia::Core::StringCRC parentId,
            Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& outChildren) const;
        bool HasHistory(Dia::Core::StringCRC parentStateId) const;

        static HierarchicalStateMachineDefinition LoadFromJson(
            const Json::Value& root,
            const CallbackRegistry& registry);
        static HierarchicalStateMachineDefinition LoadFromFile(
            const char* filePath,
            const CallbackRegistry& registry);
    };

    // Fluent builder for flat definitions
    class StateMachineBuilder {
    public:
        StateMachineBuilder& State(Dia::Core::StringCRC stateId);
        StateMachineBuilder& InitialState(Dia::Core::StringCRC stateId);
        StateMachineBuilder& OnEnter(void(*action)(void*));
        StateMachineBuilder& OnExit(void(*action)(void*));
        StateMachineBuilder& OnUpdate(void(*action)(void*, float));
        StateMachineBuilder& Transition(
            Dia::Core::StringCRC targetStateId,
            Dia::Core::StringCRC triggerId);
        StateMachineBuilder& Guard(bool(*predicate)(const void*));

        StateMachineDefinition Build() const;
    };

    // Fluent builder for hierarchical definitions
    class HierarchicalStateMachineBuilder {
    public:
        HierarchicalStateMachineBuilder& State(
            Dia::Core::StringCRC stateId);
        HierarchicalStateMachineBuilder& InitialState(
            Dia::Core::StringCRC stateId);
        HierarchicalStateMachineBuilder& ChildState(
            Dia::Core::StringCRC parentId,
            Dia::Core::StringCRC childId);
        HierarchicalStateMachineBuilder& OnEnter(void(*action)(void*));
        HierarchicalStateMachineBuilder& OnExit(void(*action)(void*));
        HierarchicalStateMachineBuilder& OnUpdate(
            void(*action)(void*, float));
        HierarchicalStateMachineBuilder& Transition(
            Dia::Core::StringCRC targetStateId,
            Dia::Core::StringCRC triggerId);
        HierarchicalStateMachineBuilder& Guard(
            bool(*predicate)(const void*));
        HierarchicalStateMachineBuilder& EnableHistory(
            Dia::Core::StringCRC parentStateId);

        HierarchicalStateMachineDefinition Build() const;
    };

    // Pushdown state definition — adds OnPause/OnResume callbacks
    struct PushdownStateDef {
        Dia::Core::StringCRC id;
        void(*onEnter)(void*) = nullptr;
        void(*onExit)(void*) = nullptr;
        void(*onUpdate)(void*, float) = nullptr;
        void(*onPause)(void*) = nullptr;
        void(*onResume)(void*) = nullptr;
    };

    class PushdownAutomatonDefinition {
    public:
        void GetStates(
            Dia::Core::DynamicArrayC<PushdownStateDef>& outStates) const;
        Dia::Core::StringCRC GetInitialStateId() const;

        bool Validate(
            Dia::Core::DynamicArrayC<const char*>& outErrors) const;

        static PushdownAutomatonDefinition LoadFromJson(
            const Json::Value& root,
            const CallbackRegistry& registry);
        static PushdownAutomatonDefinition LoadFromFile(
            const char* filePath,
            const CallbackRegistry& registry);
    };

    // Fluent builder for pushdown definitions
    class PushdownAutomatonBuilder {
    public:
        PushdownAutomatonBuilder& State(Dia::Core::StringCRC stateId);
        PushdownAutomatonBuilder& InitialState(
            Dia::Core::StringCRC stateId);
        PushdownAutomatonBuilder& OnEnter(void(*action)(void*));
        PushdownAutomatonBuilder& OnExit(void(*action)(void*));
        PushdownAutomatonBuilder& OnUpdate(void(*action)(void*, float));
        PushdownAutomatonBuilder& OnPause(void(*action)(void*));
        PushdownAutomatonBuilder& OnResume(void(*action)(void*));

        PushdownAutomatonDefinition Build() const;
    };

} // namespace Dia::StateMachine
```

---

## Implementation Notes

- Builder methods are additive — `State()` sets the "current state" context, subsequent `OnEnter`/`OnExit`/`Transition` calls apply to that state.
- `Guard()` applies to the most recently added `Transition()`.
- `Build()` is const — builder can be reused to produce multiple definitions.
- Definition stores states and transitions in contiguous arrays for cache-friendly iteration.
- **Immutability enforcement:** `StateMachineDefinition` exposes only `const` accessor methods (`GetStates`, `GetTransitions`, `GetInitialStateId`, `Validate`). Internal data members are private. There are no mutating methods after construction. Attempting to modify a definition after `Build()` or `LoadFromJson()` is a compile error. `Clone()` produces a deep copy for when multiple machines need the same topology.
- `Validate()` checks: initial state set, initial state exists, no duplicate state IDs, all transition targets exist, all transition sources exist (or are `kAnyState`). HSM additionally: every non-root state has a parent, each parent with children has an initial child marked.
- **Type-erased callback bridge:** Builder and `StateMachineDefinition` store `void*` callbacks (`void(*)(void*)`, `bool(*)(const void*)`). The typed machines (`FlatStateMachine<TContext>`, etc.) invoke these by passing `static_cast<void*>(&context)` to the stored pointer. Callback authors write functions that cast back: `void MyOnEnter(void* ctx) { auto& c = *static_cast<MyContext*>(ctx); ... }`. This is the standard C callback pattern — type safety is the caller's responsibility. The typed aliases (`ActionFn = void(*)(TContext&)`) on the machine classes are convenience documentation only; the stored type is always `void*`-based.
- A `StateMachineDefinition` can be `Clone()`'d for use with different `TContext` types if callbacks are compatible — the definition doesn't know or care about the context type. Each machine owns its own copy after move.

---

## Dependencies

### Required Modules
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `DIA_ASSERT`, `Json`

### Dependent Features
- **flat-state-machine** — consumes `StateMachineDefinition`
- **hierarchical-state-machine** — consumes `HierarchicalStateMachineDefinition`
- **data-driven-definitions** — `LoadFromJson()` / `LoadFromFile()` on definitions

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestStateMachineBuilder.cpp`)

**Flat builder:**
1. Build 3-state machine — `GetStates()` returns 3, `GetInitialStateId()` correct
2. Build with transitions — `GetTransitions()` returns correct count and source/target
3. Build with guard on transition — `TransitionDef.guard` is non-null
4. Build without initial state — DIA_ASSERT
5. Build with duplicate state ID — DIA_ASSERT
6. Build with transition to nonexistent state — DIA_ASSERT
7. Wildcard transition (`kAnyState` source) — accepted, no validation error
8. `Validate()` on good definition — returns true, no errors
9. `Validate()` on bad definition — returns false, error strings populated

**Hierarchical builder:**
10. Build with parent/child nesting — `GetParentOf()` and `GetChildrenOf()` correct
11. `EnableHistory()` — `HasHistory()` returns true for that parent
12. Build without initial child for parent with children — DIA_ASSERT
13. Orphan state (no parent, not root) — DIA_ASSERT

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | All identifiers are StringCRC |
| PD-004 | Platform | No STL in public APIs | DiaCore containers |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::` |
| SD-005 | System | Machine owns definition | Immutable after Build(), moved into machine. Clone() for explicit sharing |
| SD-015 | System | kAnyState wildcard | Accepted in flat builder as source |

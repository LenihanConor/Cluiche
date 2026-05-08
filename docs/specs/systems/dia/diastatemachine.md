# System Spec: DiaStateMachine

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaStateMachine is the generic state machine library for the Dia engine. It provides reusable state machine infrastructure for gameplay, AI, animation, UI flows, and any other system that needs explicit stateful behavior — replacing ad-hoc state management with a shared, inspectable, debuggable abstraction.

The library offers two core implementations optimized for different use cases:
- **`FlatStateMachine<TContext>`** — A lightweight, cache-friendly flat FSM for high-volume simple machines (AI agents, gameplay triggers, UI state)
- **`HierarchicalStateMachine<TContext>`** — A nested state machine with parent-child state hierarchy and shallow history for complex single-instance machines (player controllers, animation state, application flow)

Both types share a common **`IStateMachineInspectable`** interface that enables universal tooling — logging, editor visualization, and debugging work identically across both types.

Additionally provides:
- **`PushdownAutomaton<TContext>`** — A stack-based variant for interrupt-and-resume patterns (menus, dialog, tutorials)
- **`StateMachineComponent`** — An `IComponent` wrapper for attaching state machines to entities
- **Logging and tracing** — Dedicated DiaLogger channel with structured transition tracing

DiaStateMachine is a behavioral building block — it does not own application scheduling, entity management, rendering, or editor visualization. Those concerns belong to DiaApplication, game code, DiaGraphics, and a future DiaStateMachineEditor system respectively.

**Research:** @docs/research/state_machine/summary.md

**Dependency chain:**
`DiaStateMachine → DiaCore (containers, StringCRC, Events, Components, Graph, Json)`
`DiaStateMachine → DiaLogger (logging channel, ISink)`

## Responsibilities

- Provide a `CallbackRegistry` for binding named action/guard callbacks to StringCRC keys — explicitly constructed and passed by reference, not a singleton
- Provide a `FlatStateMachine<TContext>` with states, transitions, guard predicates, and entry/exit/update actions
- Support wildcard transitions (`kAnyState` source) in flat FSMs — transitions that can fire from any state without per-state duplication
- Provide a `HierarchicalStateMachine<TContext>` with nested states, inherited transitions, and shallow history
- Provide a `PushdownAutomaton<TContext>` with push/pop state stack semantics
- Provide a `StateMachineBuilder` fluent API for constructing state machines in code
- Provide `StateMachineDefinition`, `HierarchicalStateMachineDefinition`, and `PushdownAutomatonDefinition` data objects — moved into machine at construction (machine owns its definition)
- Load `StateMachineDefinition` and `HierarchicalStateMachineDefinition` from JSON files, enabling data-driven machine authoring without recompilation
- Validate loaded definitions (reachability, no orphan states, initial state exists, no duplicate IDs)
- Provide an `IStateMachineInspectable` interface for querying current state, transition history, and topology from external tools
- Provide a `StateMachineComponent` (`IComponent`) for attaching state machines to `IComponentObject` entities via `ComponentFactoryRegistry`
- Emit structured transition logs to a `StateMachine` DiaLogger channel: source state, target state, trigger event, guard results, timestamp, machine instance ID
- Provide a `StateMachineTracer` wrapper that intercepts transitions for logging without modifying the FSM's own code
- Provide configurable log verbosity per tracer instance: `kTransitionsOnly`, `kTransitionsAndGuards`, `kFull` (transitions + guards + timing + state durations)
- Provide test utilities under `DiaStateMachine/Testing/`: `AssertInState`, `AssertTransitionSequence`, `FireAndExpect`, and mock context helpers — shipped with the library, consumer opt-in via include
- Identify all states and transitions via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (PD-004)
- Provide `dia.statemachine.architecture.module.md` YAML module documentation
- Provide `DiaStateMachine.vcxproj` static library registered in `Cluiche.sln`

## Non-Responsibilities

- Editor visualization or visual debugging — future DiaStateMachineEditor system
- Design-time visual state machine editing — future DiaStateMachineEditor system
- Application phase management — DiaApplication owns ProcessingUnit/Phase/Module lifecycle
- Entity/component ownership — game code owns entities; state machines are attached via component
- Animation blending, clip playback, or animation-specific state semantics — future animation system
- Behavior trees — separate concern; may integrate via shared blackboard in future
- Thread safety within state action callbacks — caller's responsibility; FSM dispatch is single-threaded
- Network synchronization of state machine state — out of scope

## Public Interfaces

### IStateMachineInspectable

```cpp
namespace Dia::StateMachine {
    struct StateInfo {
        Dia::Core::StringCRC id;
        bool isActive;
        bool isLeaf;                    // true for states with no children (HSM)
        Dia::Core::StringCRC parentId;  // kInvalidCRC for root/flat states
    };

    struct TransitionInfo {
        Dia::Core::StringCRC sourceStateId;
        Dia::Core::StringCRC targetStateId;
        Dia::Core::StringCRC triggerId;  // kInvalidCRC for unconditional
        bool hasGuard;
    };

    struct TransitionRecord {
        Dia::Core::StringCRC sourceStateId;
        Dia::Core::StringCRC targetStateId;
        Dia::Core::StringCRC triggerId;
        float timestamp;
    };

    class IStateMachineInspectable {
    public:
        virtual ~IStateMachineInspectable() = default;

        virtual Dia::Core::StringCRC GetMachineId() const = 0;
        virtual Dia::Core::StringCRC GetCurrentStateId() const = 0;

        virtual void GetAllStates(
            Dia::Core::DynamicArrayC<StateInfo>& outStates) const = 0;
        virtual void GetAllTransitions(
            Dia::Core::DynamicArrayC<TransitionInfo>& outTransitions) const = 0;
        virtual void GetTransitionHistory(
            Dia::Core::DynamicArrayC<TransitionRecord>& outHistory,
            int maxEntries = 32) const = 0;
    };
}
```

### FlatStateMachine

```cpp
namespace Dia::StateMachine {
    template<typename TContext>
    class FlatStateMachine : public IStateMachineInspectable {
    public:
        using ActionFn = void(*)(TContext&);
        using GuardFn = bool(*)(const TContext&);
        using UpdateFn = void(*)(TContext&, float deltaTime);

        explicit FlatStateMachine(Dia::Core::StringCRC machineId,
                                  StateMachineDefinition&& definition,
                                  TContext& context);

        // State query
        Dia::Core::StringCRC GetCurrentStateId() const override;
        bool IsInState(Dia::Core::StringCRC stateId) const;

        // Triggering transitions
        bool Fire(Dia::Core::StringCRC triggerId);
        void Update(float deltaTime);

        // IStateMachineInspectable
        Dia::Core::StringCRC GetMachineId() const override;
        void GetAllStates(
            Dia::Core::DynamicArrayC<StateInfo>& outStates) const override;
        void GetAllTransitions(
            Dia::Core::DynamicArrayC<TransitionInfo>& outTransitions) const override;
        void GetTransitionHistory(
            Dia::Core::DynamicArrayC<TransitionRecord>& outHistory,
            int maxEntries) const override;
    };
}
```

### HierarchicalStateMachine

```cpp
namespace Dia::StateMachine {
    template<typename TContext>
    class HierarchicalStateMachine : public IStateMachineInspectable {
    public:
        using ActionFn = void(*)(TContext&);
        using GuardFn = bool(*)(const TContext&);
        using UpdateFn = void(*)(TContext&, float deltaTime);

        explicit HierarchicalStateMachine(
            Dia::Core::StringCRC machineId,
            HierarchicalStateMachineDefinition&& definition,
            TContext& context);

        // State query
        Dia::Core::StringCRC GetCurrentStateId() const override;
        Dia::Core::StringCRC GetCurrentLeafStateId() const;
        bool IsInState(Dia::Core::StringCRC stateId) const;
        bool IsDescendantActive(Dia::Core::StringCRC ancestorStateId) const;

        // Triggering
        bool Fire(Dia::Core::StringCRC triggerId);
        void Update(float deltaTime);

        // History
        Dia::Core::StringCRC GetHistoryState(
            Dia::Core::StringCRC parentStateId) const;

        // IStateMachineInspectable
        Dia::Core::StringCRC GetMachineId() const override;
        void GetAllStates(
            Dia::Core::DynamicArrayC<StateInfo>& outStates) const override;
        void GetAllTransitions(
            Dia::Core::DynamicArrayC<TransitionInfo>& outTransitions) const override;
        void GetTransitionHistory(
            Dia::Core::DynamicArrayC<TransitionRecord>& outHistory,
            int maxEntries) const override;
    };
}
```

### PushdownAutomaton

```cpp
namespace Dia::StateMachine {
    template<typename TContext>
    class PushdownAutomaton : public IStateMachineInspectable {
    public:
        using ActionFn = void(*)(TContext&);
        using UpdateFn = void(*)(TContext&, float deltaTime);

        explicit PushdownAutomaton(Dia::Core::StringCRC machineId,
                                   PushdownAutomatonDefinition&& definition,
                                   TContext& context,
                                   int maxStackDepth = 16);

        // Stack operations
        bool Push(Dia::Core::StringCRC stateId);
        bool Pop();
        Dia::Core::StringCRC GetCurrentStateId() const override;
        int GetStackDepth() const;

        void Update(float deltaTime);

        // IStateMachineInspectable
        Dia::Core::StringCRC GetMachineId() const override;
        void GetAllStates(
            Dia::Core::DynamicArrayC<StateInfo>& outStates) const override;
        void GetAllTransitions(
            Dia::Core::DynamicArrayC<TransitionInfo>& outTransitions) const override;
        void GetTransitionHistory(
            Dia::Core::DynamicArrayC<TransitionRecord>& outHistory,
            int maxEntries) const override;
    };
}
```

### CallbackRegistry

```cpp
namespace Dia::StateMachine {
    // Explicitly constructed registry for binding named callbacks to function pointers.
    // NOT a singleton — caller creates, owns, and passes by reference.
    // Used by data-driven definitions to resolve JSON-declared callback names to code.
    class CallbackRegistry {
    public:
        using ActionCallback = void(*)(void*);
        using GuardCallback = bool(*)(const void*);
        using UpdateCallback = void(*)(void*, float);

        void RegisterAction(Dia::Core::StringCRC name, ActionCallback callback);
        void RegisterGuard(Dia::Core::StringCRC name, GuardCallback callback);
        void RegisterUpdate(Dia::Core::StringCRC name, UpdateCallback callback);

        ActionCallback FindAction(Dia::Core::StringCRC name) const;
        GuardCallback FindGuard(Dia::Core::StringCRC name) const;
        UpdateCallback FindUpdate(Dia::Core::StringCRC name) const;

        bool HasAction(Dia::Core::StringCRC name) const;
        bool HasGuard(Dia::Core::StringCRC name) const;
        bool HasUpdate(Dia::Core::StringCRC name) const;
    };
}
```

### Wildcard Transitions (Flat FSM)

```cpp
namespace Dia::StateMachine {
    // Sentinel source state for transitions that apply from any state.
    // HSM gets this for free via parent-state transitions; flat FSM needs it explicitly.
    static constexpr Dia::Core::StringCRC kAnyState{"*"};

    // Usage in builder:
    // builder.State(kAnyState)
    //        .Transition("Dead", "OnHealthZero");
    //
    // Usage in JSON:
    // { "source": "*", "target": "Dead", "trigger": "OnHealthZero" }
    //
    // Wildcard transitions are evaluated after state-specific transitions (lower priority).
}
```

### StateMachineBuilder

```cpp
namespace Dia::StateMachine {
    // Fluent builder for constructing StateMachineDefinition in code
    class StateMachineBuilder {
    public:
        StateMachineBuilder& State(Dia::Core::StringCRC stateId);
        StateMachineBuilder& InitialState(Dia::Core::StringCRC stateId);
        StateMachineBuilder& OnEnter(void(*action)(void*));
        StateMachineBuilder& OnExit(void(*action)(void*));
        StateMachineBuilder& OnUpdate(void(*action)(void*, float));
        StateMachineBuilder& Transition(Dia::Core::StringCRC targetStateId,
                                        Dia::Core::StringCRC triggerId);
        StateMachineBuilder& Guard(bool(*predicate)(const void*));

        StateMachineDefinition Build() const;
    };

    // Hierarchical builder adds nesting
    class HierarchicalStateMachineBuilder {
    public:
        HierarchicalStateMachineBuilder& State(Dia::Core::StringCRC stateId);
        HierarchicalStateMachineBuilder& InitialState(
            Dia::Core::StringCRC stateId);
        HierarchicalStateMachineBuilder& ChildState(
            Dia::Core::StringCRC parentId,
            Dia::Core::StringCRC childId);
        HierarchicalStateMachineBuilder& OnEnter(void(*action)(void*));
        HierarchicalStateMachineBuilder& OnExit(void(*action)(void*));
        HierarchicalStateMachineBuilder& OnUpdate(void(*action)(void*, float));
        HierarchicalStateMachineBuilder& Transition(
            Dia::Core::StringCRC targetStateId,
            Dia::Core::StringCRC triggerId);
        HierarchicalStateMachineBuilder& Guard(bool(*predicate)(const void*));
        HierarchicalStateMachineBuilder& EnableHistory(
            Dia::Core::StringCRC parentStateId);

        HierarchicalStateMachineDefinition Build() const;
    };
}
```

### StateMachineDefinition

```cpp
namespace Dia::StateMachine {
    // Machine topology definition — moved into machine at construction.
    // Use Clone() when multiple machines need the same topology.
    struct StateDef {
        Dia::Core::StringCRC id;
        // Action function pointers stored per-state
    };

    struct TransitionDef {
        Dia::Core::StringCRC sourceStateId;
        Dia::Core::StringCRC targetStateId;
        Dia::Core::StringCRC triggerId;
        // Guard function pointer (nullable)
    };

    class StateMachineDefinition {
    public:
        void GetStates(
            Dia::Core::DynamicArrayC<StateDef>& outStates) const;
        void GetTransitions(
            Dia::Core::DynamicArrayC<TransitionDef>& outTransitions) const;
        Dia::Core::StringCRC GetInitialStateId() const;

        // Deep copy for sharing topology across multiple machines
        StateMachineDefinition Clone() const;

        // Data-driven loading (registry resolves named callbacks to function pointers)
        static StateMachineDefinition LoadFromJson(
            const Json::Value& root,
            const CallbackRegistry& registry);
        static StateMachineDefinition LoadFromFile(
            const char* filePath,
            const CallbackRegistry& registry);

        // Validation (returns false + populates errors on failure)
        bool Validate(
            Dia::Core::DynamicArrayC<const char*>& outErrors) const;
    };

    class HierarchicalStateMachineDefinition : public StateMachineDefinition {
    public:
        Dia::Core::StringCRC GetParentOf(
            Dia::Core::StringCRC stateId) const;
        void GetChildrenOf(
            Dia::Core::StringCRC parentId,
            Dia::Core::DynamicArrayC<Dia::Core::StringCRC>& outChildren) const;
        bool HasHistory(Dia::Core::StringCRC parentStateId) const;

        // Data-driven loading
        static HierarchicalStateMachineDefinition LoadFromJson(
            const Json::Value& root,
            const CallbackRegistry& registry);
        static HierarchicalStateMachineDefinition LoadFromFile(
            const char* filePath,
            const CallbackRegistry& registry);
    };
}
```

### StateMachineComponent

```cpp
namespace Dia::StateMachine {
    class StateMachineComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"StateMachineComponent"};

        // Access the underlying inspectable interface
        IStateMachineInspectable* GetInspectable() const;

        // Typed access when the caller knows the machine type
        template<typename TMachine>
        TMachine* GetMachine() const;
    };
}
```

### Logging

```cpp
namespace Dia::StateMachine {
    // Log channel constant
    static constexpr Dia::Core::StringCRC kLogChannel{"StateMachine"};

    // Configurable verbosity levels
    enum class TraceVerbosity {
        kTransitionsOnly,       // Source → target state, trigger ID
        kTransitionsAndGuards,  // + guard names, pass/fail results
        kFull                   // + timing (transition duration, state residence time)
    };

    // Wraps any IStateMachineInspectable and logs transitions
    class StateMachineTracer {
    public:
        explicit StateMachineTracer(IStateMachineInspectable& machine);

        void SetVerbosity(TraceVerbosity verbosity);
        TraceVerbosity GetVerbosity() const;
        void SetRateLimit(int maxEntriesPerSecond);
        void Enable();
        void Disable();
        bool IsEnabled() const;
    };
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Inspection Interface | `IStateMachineInspectable` shared interface for querying state, transitions, topology, and history from any machine type. Foundation for all tooling. | [inspection-interface.md](../../features/dia/diastatemachine/inspection-interface.md) | Done |
| Flat State Machine | `FlatStateMachine<TContext>` — lightweight FSM with states, event-triggered transitions, guard predicates, entry/exit/update actions. Cache-friendly for high-volume use. | [flat-state-machine.md](../../features/dia/diastatemachine/flat-state-machine.md) | Done |
| Hierarchical State Machine | `HierarchicalStateMachine<TContext>` — nested states with inherited transitions, entry/exit propagation through hierarchy, shallow history. | [hierarchical-state-machine.md](../../features/dia/diastatemachine/hierarchical-state-machine.md) | Done |
| Pushdown Automaton | `PushdownAutomaton<TContext>` — stack-based push/pop state machine for interrupt-and-resume patterns. Bounded stack depth. | [pushdown-automaton.md](../../features/dia/diastatemachine/pushdown-automaton.md) | Done |
| State Machine Builder | `StateMachineBuilder`, `HierarchicalStateMachineBuilder`, and `PushdownAutomatonBuilder` — fluent APIs for constructing machine definitions in code. Produces `*Definition` objects. | [state-machine-builder.md](../../features/dia/diastatemachine/state-machine-builder.md) | Done |
| Data-Driven Definitions | Load `StateMachineDefinition` and `HierarchicalStateMachineDefinition` from JSON files. Validation (reachability, orphan states, duplicate IDs). Enables designer iteration without recompilation. | [data-driven-definitions.md](../../features/dia/diastatemachine/data-driven-definitions.md) | Done |
| State Machine Component | `StateMachineComponent` (`IComponent`) wrapper for entity integration via `ComponentFactoryRegistry`. Component owns the attached machine. | [state-machine-component.md](../../features/dia/diastatemachine/state-machine-component.md) | Done |
| Transition Logging & Tracing | Dedicated `StateMachine` DiaLogger channel. `StateMachineTracer` wrapper with configurable verbosity (`kTransitionsOnly` / `kTransitionsAndGuards` / `kFull`), rate limiting. NDJSON output for post-hoc analysis. | [transition-logging.md](../../features/dia/diastatemachine/transition-logging.md) | Done |
| Test Utilities | `DiaStateMachine/Testing/` — `AssertInState`, `FireAndExpect`, `AssertTransitionSequence`, mock context helpers. Ships with library; consumer opt-in via include. | [test-utilities.md](../../features/dia/diastatemachine/test-utilities.md) | Done |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — StringCRC (state/transition identity), DynamicArrayC/HashTable (containers), Graph (transition topology), Event/EventDispatcher (trigger mechanism), IComponent/IComponentObject/ComponentFactoryRegistry (entity integration), Json (JSON loading for data-driven definitions), DIA_ASSERT
- **DiaLogger** — ISink, LogLevel, Logger singleton, log channel registration

**Explicitly excluded:**
- **DiaApplication** — no dependency; state machines operate within phases/modules but don't depend on the Phase system
- **DiaGraphics** — no rendering; visual debugging belongs to future DiaStateMachineEditor
- **DiaMaths** — no math needed; state machines are purely logical

**Dependents:**
- Game code (CluicheTest and future games) — creates state machines for gameplay, AI, UI
- Future DiaStateMachineEditor — uses `IStateMachineInspectable` for visualization
- Future animation system — may specialize `HierarchicalStateMachine` for animation blending
- DiaApplication (potential future) — Phase system could optionally use generic FSM internally

## Out of Scope

- Visual debugging or editor tooling — separate DiaStateMachineEditor system spec
- Design-time visual editor for creating state machines graphically
- Behavior trees or utility AI — separate concern
- Animation-specific features (blend durations, interruption priority, pose caching)
- Thread-safe FSM dispatch — machines are single-threaded; caller synchronizes
- Refactoring DiaApplication Phase system to use generic FSM — deferred for separate discussion
- Network synchronization of machine state
- Coroutine-based state machines (C++20 co_await)
- General-purpose blackboard/shared data store — belongs in DiaCore, not DiaStateMachine (future spec item)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Two separate implementations (Flat + HSM) sharing `IStateMachineInspectable` | Flat stays lean and cache-friendly for high-volume cases; HSM handles complex nesting; shared interface makes tooling universal. A unified implementation forces hierarchy overhead on simple machines. | All features | Accepted | Yes |
| SD-002 | States identified by `StringCRC`, not enums or template types | Consistent with PD-001; enables runtime inspection without RTTI; works across code-defined and future data-defined machines. Enum-based would require per-machine enum types. Template-based prevents runtime inspection. | All features | Accepted | Yes |
| SD-003 | Action callbacks use raw function pointers, not `std::function` | Zero allocation overhead; no hidden heap allocations in hot paths. Context object (`TContext&`) provides all state needed by callbacks. Lambdas with captures can use a static wrapper + context pattern. | Flat FSM, HSM | Accepted | Yes |
| SD-004 | Single-threaded dispatch; no internal locking | Most gameplay FSMs run on one thread. Internal locking adds overhead and gives false safety (state action callbacks still need external synchronization). Matches DiaRigidBody2D approach. | All features | Accepted | Yes |
| SD-005 | `StateMachineDefinition` moved into machine at construction (machine owns definition) | Machine takes definition by `Definition&&` — one owner, clean lifetime. For sharing across instances, definitions expose `Clone()` for explicit copy. Simplifies ownership vs. flyweight (no non-owning pointer chains). | Builder, Component, All machine types | Accepted | Yes |
| SD-006 | Transition history stored as fixed-size ring buffer per machine instance | Bounded memory; no allocation during transitions; recent history sufficient for debugging. Default 32 entries. Configurable at construction. | Inspection Interface | Accepted | Yes |
| SD-007 | Guard predicates are first-match semantics with explicit priority | When multiple transitions from the same state match the same trigger, the first registered transition whose guard passes wins. Deterministic; no ambiguity. Document clearly in builder API. | Flat FSM, HSM | Accepted | Yes |
| SD-008 | Max transitions per frame guard (default 8) | Prevents infinite transition loops (A → B → A with always-true guards). Fires DIA_ASSERT in debug; silently stops in release. Configurable. | All machine types | Accepted | Yes |
| SD-009 | HSM supports shallow history only (not deep history) | Shallow history (remember last active direct child) covers 90%+ of use cases. Deep history (remember entire subtree) adds significant complexity for marginal benefit. Can be added as a future feature if needed. | HSM | Accepted | Yes |
| SD-010 | Pushdown Automaton has bounded stack depth (default 16) | Prevents unbounded memory growth from push-without-pop bugs. DIA_ASSERT on overflow in debug; returns false in release. | Pushdown Automaton | Accepted | Yes |
| SD-011 | Definitions loadable from JSON (data-driven) | Enables designer iteration without recompilation; definitions validated on load (reachability, orphans, duplicate IDs). Code-defined via Builder and data-defined via JSON produce the same `StateMachineDefinition` type. Action callbacks registered separately by StringCRC name since function pointers cannot be serialized. | Builder, Data-Driven Definitions | Accepted | Yes |
| SD-012 | Tracer verbosity is configurable per instance | Three levels: `kTransitionsOnly` (default, cheapest), `kTransitionsAndGuards` (includes guard evaluation results), `kFull` (adds timing/duration). Allows coarse tracing in production and detailed tracing during debugging without code changes. | Transition Logging | Accepted | Yes |
| SD-013 | `CallbackRegistry` is explicitly constructed, not a singleton | Avoids hidden global state. Caller creates, owns, and passes by reference to `LoadFromJson()`. Trivially testable with isolated registries. Same injection pattern as `ISpatialStructure` in DiaRigidBody2D. | Data-Driven Definitions | Accepted | Yes |
| SD-014 | `Fire()` returns bool; no exceptions; DIA_ASSERT for programmer errors | `Fire()` returns `false` when no matching transition exists (often intentional — not all states handle all events). Debug builds log failed `Fire()` at `kDebug` level. Guards must not throw — DIA_ASSERT in debug, treated as `false` in release. Invalid operations (fire on destroyed machine, push on full stack) → DIA_ASSERT in debug, return false in release. | All machine types | Accepted | Yes |
| SD-015 | Flat FSM supports wildcard transitions via `kAnyState` sentinel | `kAnyState` as source means "from any state." Wildcard transitions evaluated after state-specific transitions (lower priority). HSM gets this for free via parent-state transitions. Prevents N-way duplication for common patterns like "death on zero health." | Flat FSM | Accepted | Yes |
| SD-016 | Definitions are reload-on-next-construct, not hot-patched | Immutable definitions (SD-005) mean active machines are never live-patched. When a JSON file changes, the next machine constructed from that file gets the new version. Safe, simple, no mid-state topology mutation. | Data-Driven Definitions | Accepted | Yes |
| SD-017 | Test utilities ship inside `DiaStateMachine/Testing/` | Test helpers (`AssertInState`, `FireAndExpect`, `AssertTransitionSequence`, mock context) live in the library, not in GoogleTests. Consumers opt in via `#include <DiaStateMachine/Testing/...>`. Platform-wide pattern for all Dia modules. | Testing | Accepted | Yes |
| SD-018 | Tracer hooks via `ITransitionListener` on `IStateMachineInspectable` | Nullable listener pointer — zero overhead when null. Machines call `OnTransition(TransitionEvent)` after each successful transition and `OnTransitionFailed()` on failed `Fire()`. `TransitionEvent` carries guard evaluation results for `kTransitionsAndGuards` verbosity. Stack-allocated, valid only during callback. | Inspection Interface, Transition Logging | Accepted | Yes |
| SD-019 | Callbacks are type-erased `void*` in definitions; machines cast via `static_cast` | Standard C callback pattern. Builder/Definition store `void(*)(void*)`. Typed machine passes `static_cast<void*>(&context)` on invocation. Type safety is caller's responsibility. Enables definition sharing across context types. | All machine types | Accepted | Yes |
| SD-020 | `PushdownAutomaton` uses `PushdownAutomatonDefinition` (consistent with Flat/HSM) | All three machine types follow the same Definition ownership pattern (moved into machine at construction). Pushdown adds `OnPause`/`OnResume` callbacks per state. Built via `PushdownAutomatonBuilder` or loaded from JSON. | Pushdown Automaton | Accepted | Yes |
| SD-021 | HSM does not support `kAnyState` wildcards — use parent-state transitions | Parent-state transitions are the hierarchical equivalent of wildcards. Adding `kAnyState` to HSM would create ambiguity about which level the wildcard applies at. | HSM | Accepted | Yes |
| SD-022 | Definition internal arrays are heap-allocated with configurable default capacity (64) | Definitions store states and transitions in `DynamicArrayC` with default initial capacity 64. Allows growth beyond static limits without recompilation. Capacity is a construction-time parameter, not a compile-time constant. | Builder, Data-Driven Definitions | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All state IDs, transition IDs, trigger IDs, and machine IDs use StringCRC. `StateMachineComponent::kUniqueId` is a StringCRC constant. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | State machines operate *within* phases/modules, not as a parallel lifecycle. No dependency on DiaApplication. |
| PD-003 | Platform | Component-based entities (IComponent/IComponentObject) | `StateMachineComponent` implements `IComponent`; registered via `ComponentFactoryRegistry` with both dynamic and pooled factory variants. |
| PD-004 | Platform | No STL containers in public APIs | All output parameters use DiaCore containers (DynamicArrayC, HashTable). Internal implementation may use STL. |
| PD-005 | Platform | x64 only | DiaStateMachine.vcxproj targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaStateMachine.vcxproj and .vcxproj.filters created and manually maintained. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. Can use concepts to constrain TContext and state types. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | DiaStateMachine.vcxproj must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any generated transition logs or trace output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.statemachine.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::StateMachine::` namespace. |
| AD-005 | Dia App | Component-based entities | Reinforces PD-003 for `StateMachineComponent`. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Architecture | Should `FlatStateMachine` and `HierarchicalStateMachine` share a common base class beyond `IStateMachineInspectable`? | No — they share the inspection interface only. A common base would either bloat the flat type or constrain the HSM. Template concepts can enforce shared behavioral contracts at compile time. |
| 2 | Triggers | How do event-based triggers integrate with DiaCore's `Event`/`EventDispatcher` system? | State machines accept `Fire(StringCRC triggerId)` directly. Integration with EventDispatcher is the caller's responsibility — subscribe to events and call `Fire()` in the handler. This keeps the FSM decoupled from any specific event source. |
| 3 | Context | What constraints apply to `TContext`? Can it be any type? | TContext must be an lvalue reference type (stored by reference, not copied). C++20 concept should require it to be a non-const reference. No other constraints — the context is opaque to the FSM. |
| 4 | HSM Entry/Exit | When transitioning between states in different branches of the hierarchy, what is the entry/exit order? | Exit from current leaf up to Lowest Common Ancestor (LCA), then enter from LCA down to target leaf. Standard UML statechart semantics. LCA itself is neither exited nor entered. |
| 5 | HSM Wildcards | Does HSM support `kAnyState` wildcard transitions? | No — HSM uses parent-state transitions instead. A transition on a parent state already applies to all descendants, which is the hierarchical equivalent of wildcards. Adding `kAnyState` to HSM would create ambiguity about which level the wildcard applies at (SD-021). |
| 6 | Guards | Can guard predicates access the trigger event's payload, or only the context? | v1: guards receive `const TContext&` only. Trigger payloads would require type-erased event data, adding complexity. If needed, callers can write trigger payload into the context before calling `Fire()`. |
| 7 | Component | Should `StateMachineComponent` support multiple state machines per entity? | Yes — an entity might have separate FSMs for AI behavior and animation. The component holds one machine; attach multiple `StateMachineComponent` instances to the entity, each with a different machine ID. |
| 8 | Performance | What is the expected per-transition overhead for `FlatStateMachine`? | Target: <1us per transition (guard evaluation + exit action + state swap + enter action). No heap allocation. Linear scan of transitions from current state (bounded by max transitions per state, typically <10). |
| 9 | Logging | Should transition logging be opt-in or opt-out? How granular? | Opt-in via `StateMachineTracer` wrapper — zero overhead for unwrapped machines. Verbosity is configurable per tracer instance: `kTransitionsOnly` (default), `kTransitionsAndGuards`, `kFull` (timing/durations). Rate limiting prevents log flooding from high-frequency machines. |
| 10 | Definition Ownership | How is definition lifetime managed? | Machine takes definition by move (`Definition&&`) — one owner, clean lifetime. Definitions are immutable after `Build()` or `LoadFromJson()`. For multiple machines with the same topology, use `Clone()` to produce an explicit copy. No shared pointers, no non-owning references (SD-005). |
| 11 | Data-Driven | How do JSON-defined machines bind to C++ action/guard callbacks? | JSON defines topology (states, transitions, triggers, guard names). Action and guard callbacks are registered in a code-side `CallbackRegistry` keyed by StringCRC name. At machine construction, the definition's named callbacks are resolved against the registry. Missing callbacks are a validation error. |
| 12 | Data-Driven | What JSON schema should state machine definitions use? | Flat: `{ "initialState": "Idle", "states": [...], "transitions": [...] }`. Hierarchical adds `"parent"` and `"children"` fields per state, plus `"history": true/false`. Schema validated on load. Spec'd in the data-driven-definitions feature spec. |
| 13 | Wildcards | How do wildcard transitions interact with guard predicates? | Wildcard transitions can have guards, same as state-specific transitions. Evaluation order: all state-specific transitions first (first-match), then wildcard transitions (first-match). A state-specific transition always beats a wildcard for the same trigger. |
| 14 | Error Model | Should failed `Fire()` calls be noisy or silent by default? | Silent — returns `false`. It's common and intentional for events to be fired that only some states handle (e.g., "OnJump" while already jumping). Debug-level log when a `StateMachineTracer` is attached; no log otherwise. DIA_ASSERT only for programmer errors (null machine, invalid state ID). |
| 15 | Hot Reload | What happens to the `CallbackRegistry` when definitions are reloaded? | Registry is independent of definitions — it persists across loads. New definitions reference the same registry. If a new JSON version references a callback name not in the registry, validation fails at load time, not at runtime. |
| 16 | Blackboard | Should DiaStateMachine provide a blackboard/shared data store? | No — a general-purpose `Dia::Core::Blackboard` is more useful across the engine (AI, animation, gameplay). State machines take `TContext&` which can be a blackboard if the caller wants. Future DiaCore spec item. |

## Status

`Done` — All 9 features implemented and done 2026-05-01. 29 tests passing (12 flat + 9 HSM + 8 PDA). JSON data-driven loading removed; CallbackRegistry ships, consumers own JSON parsing.\n\n**Plan:** [diastatemachine.plan.md](diastatemachine.plan.md)

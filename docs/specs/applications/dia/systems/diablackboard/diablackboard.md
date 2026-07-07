# System Spec: DiaBlackboard

## Parent Application
@docs/specs/applications/dia/dia.md

## Purpose

DiaBlackboard is a named typed-blob store for the Dia engine. It provides a per-entity and optionally global shared memory store where gameplay and AI systems register, access, and unregister typed struct slots identified by StringCRC keys.

Rather than a flat key-value map, the blackboard holds **named struct slots** — each slot is a typed aggregate (`FireDamageBoard`, `ThreatBoard`, etc.) owned by the blackboard and associated with a StringCRC key. Systems register their struct on entity init and unregister on shutdown, giving clean subsystem lifecycle management and zero field-collision risk between systems.

Observer callbacks notify interested parties when slots are registered or unregistered — enabling reactive AI without polling. Per-frame mutation is poll-only.

**Dependency chain:**
`DiaBlackboard → DiaCore (containers, StringCRC, Observer)`

## Responsibilities

- Provide a `Blackboard` class that owns a collection of named typed slots identified by `Dia::Core::StringCRC` keys
- Support `Register<T>(StringCRC key)` — allocate and store a default-constructed `T` slot; assert if key already exists
- Support `Unregister(StringCRC key)` — destroy the slot and notify observers
- Support `Get<T>(StringCRC key)` — return a typed reference to an existing slot; assert if key absent or type mismatched
- Support `TryGet<T>(StringCRC key)` — return a nullable pointer; null if key absent or type mismatched (no assert)
- Support `Has(StringCRC key)` — query whether a slot exists
- Support `IBlackboardObserver` — observer interface notified on `OnSlotRegistered(StringCRC key)` and `OnSlotUnregistered(StringCRC key)`
- Support `AddObserver` / `RemoveObserver` using `Dia::Core::Observer` / `ObserverSubject`
- Provide a `BlackboardComponent` (`IComponent`) for attaching a blackboard to an entity via `ComponentFactoryRegistry`
- Provide an optional global `GlobalBlackboard` singleton — wraps a `Blackboard`, accessible via `GlobalBlackboard::Get()`
- Emit `DIA_LOG` on slot registration and unregistration (lifecycle events only — not per-frame reads/writes)
- Provide test utilities under `DiaBlackboard/Testing/`: `AssertHasSlot`, `AssertSlotAbsent`, mock blackboard helpers — shipped with the library, consumer opt-in via include
- Identify all slot keys via `StringCRC` (PD-001)
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.blackboard.architecture.module.md` YAML module documentation
- Provide `DiaBlackboard.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- Per-frame change notifications or mutation events — callers poll `Get<T>()` each tick
- Serialisation of slot contents — struct layout is caller's concern; DiaBlackboard does not inspect struct fields
- Thread safety — blackboard is single-threaded; caller synchronises if accessed across PUs
- Visual debugger widget — deferred to a future `DiaBlackboardVisualDebugger` system
- Global world-state AI planning (GOAP, HTN) — those systems build on top of this primitive
- Entity ownership or lifecycle — `BlackboardComponent` attaches to entities; it does not own them

## Public Interfaces

### IBlackboardObserver

```cpp
namespace Dia::Blackboard {
    class IBlackboardObserver {
    public:
        virtual ~IBlackboardObserver() = default;
        virtual void OnSlotRegistered(Dia::Core::StringCRC key) = 0;
        virtual void OnSlotUnregistered(Dia::Core::StringCRC key) = 0;
    };
}
```

### Blackboard

```cpp
namespace Dia::Blackboard {
    class Blackboard {
    public:
        // Slot registration
        template<typename T>
        T& Register(Dia::Core::StringCRC key);   // DIA_ASSERT if key exists

        void Unregister(Dia::Core::StringCRC key); // DIA_ASSERT if key absent; notifies observers

        // Slot access
        template<typename T>
        T& Get(Dia::Core::StringCRC key);          // DIA_ASSERT if absent or type mismatch

        template<typename T>
        T* TryGet(Dia::Core::StringCRC key);       // null if absent or type mismatch; no assert

        bool Has(Dia::Core::StringCRC key) const;

        // Observer registration
        void AddObserver(IBlackboardObserver& observer);
        void RemoveObserver(IBlackboardObserver& observer);
    };
}
```

### BlackboardComponent

```cpp
namespace Dia::Blackboard {
    class BlackboardComponent : public Dia::Core::IComponent {
    public:
        static constexpr Dia::Core::StringCRC kUniqueId{"BlackboardComponent"};

        Blackboard& GetBlackboard();
        const Blackboard& GetBlackboard() const;
    };
}
```

### GlobalBlackboard

```cpp
namespace Dia::Blackboard {
    // Optional global board for shared world-state knowledge.
    // Singleton — wraps a Blackboard. Use per-entity BlackboardComponent for entity-local state.
    class GlobalBlackboard : public Dia::Core::Singleton<GlobalBlackboard> {
    public:
        Blackboard& GetBoard();
        const Blackboard& GetBoard() const;
    };
}
```

### Log Channel

```cpp
namespace Dia::Blackboard {
    static constexpr Dia::Core::StringCRC kLogChannel{"Blackboard"};
    // DIA_LOG_INFO on Register/Unregister with slot key.
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Named Typed Slot Store | `Register<T>` / `Unregister` / `Get<T>` / `TryGet<T>` / `Has` on a `Blackboard` instance. StringCRC keys, type-safe access, DIA_ASSERT on misuse. | inline | Draft |
| Slot Lifecycle Observer | `IBlackboardObserver` + `AddObserver` / `RemoveObserver` via `Dia::Core::Observer`. Notified on register and unregister only. | inline | Draft |
| BlackboardComponent | `IComponent` wrapper for attaching a `Blackboard` to an entity via `ComponentFactoryRegistry`. | inline | Draft |
| GlobalBlackboard | Optional singleton wrapping a `Blackboard` for shared world-state. Use sparingly — prefer per-entity component. | inline | Draft |
| Lifecycle Logging | `DIA_LOG_INFO` on slot register/unregister. No per-frame logging. | inline | Draft |
| Test Utilities | `DiaBlackboard/Testing/` — `AssertHasSlot`, `AssertSlotAbsent`, mock helpers. Ships with library; consumer opt-in via include. | inline | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `StringCRC` (slot key identity), `DynamicArrayC` (observer list, slot registry), `IComponent` / `IComponentObject` / `ComponentFactoryRegistry` (entity integration), `Observer` / `ObserverSubject` (slot lifecycle events), `Singleton<T>` (GlobalBlackboard), `DIA_ASSERT`, `DIA_LOG_*`

**Explicitly excluded:**
- **DiaStateMachine** — no dependency; AI systems that use both will wire them at the game layer
- **DiaStreams** — slot lifecycle events go through Observer, not streams; avoids frame-latency on what are synchronous lifecycle calls
- **DiaApplicationFlow** — blackboard is a data primitive; it operates within phases/modules but does not depend on the phase system
- **DiaMaths / DiaGeometry2D** — blackboard is agnostic to stored types; spatial data lives in registered structs

**Dependents (future):**
- `DiaCommand` — command queue may read from entity blackboard for targeting data
- `DiaPathfinding` — pathfinding cost providers may read terrain data from a global board
- `DiaSensor` (C32) — sensor framework writes perception data into blackboard slots
- `DiaRules` (C5) — rule/condition system evaluates conditions against blackboard slots
- `DiaUtilityAI` (C2) — utility scorer reads blackboard slots to compute action scores
- `DiaBehaviourTree` (C3) — BT conditions and tasks read/write blackboard slots

## Out of Scope

- Per-frame mutation notifications — polling is intentional; streams overhead is not justified for hot-path reads
- Serialisation of stored structs — out of scope for this module; callers own struct serialisation
- Thread-safe blackboard access — single-threaded; caller responsibility
- Visual debug widget — deferred to `DiaBlackboardVisualDebugger`
- Type registration or reflection of slot contents — DiaBlackboard stores opaque typed blobs; DiaReflect is separate

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| BD-001 | Named struct slots, not flat key-value pairs | Slots group related data into typed aggregates with clean subsystem lifecycle (register on init, unregister on shutdown). Eliminates field collision between systems. Matches how game subsystems think about data ownership. | All features | Accepted | Yes |
| BD-002 | No pointers in the public API; struct fields use `EntityHandle` not raw pointers | Raw pointers break serialisation, replay, and cross-frame safety. Callers store handles and resolve at use-time. Blackboard does not enforce this for struct field contents, but it is the expected convention. | All features | Accepted | Yes |
| BD-003 | Observer for slot lifecycle (register/unregister); poll-only for mutations | Lifecycle events are infrequent and synchronous — Observer is zero-overhead when no listeners. Per-frame mutation notifications would be constant-rate and are better served by polling `Get<T>()` each tick. | Slot Lifecycle Observer | Accepted | Yes |
| BD-004 | `Get<T>()` asserts on absent key; `TryGet<T>()` returns null | Two access patterns for two caller contexts: code that has a required dependency (`Get`) vs code probing for optional data (`TryGet`). Clear programmer-error semantics in both cases. | Named Typed Slot Store | Accepted | Yes |
| BD-005 | `GlobalBlackboard` is an opt-in singleton; default usage is per-entity `BlackboardComponent` | Per-entity boards keep AI state isolated; global board is for genuine world-state (e.g., threat level, game phase). Providing both avoids callers abusing the global for per-entity data. | GlobalBlackboard | Accepted | Yes |
| BD-006 | Log slot register/unregister only; no per-frame read/write logging | Per-frame logging would flood the log. Lifecycle logging is sufficient to diagnose init/shutdown ordering bugs, which are the primary failure mode. | Lifecycle Logging | Accepted | Yes |
| BD-007 | Type identity stored as `std::type_index`; mismatched `Get<T>()` is a DIA_ASSERT | Type safety is enforced at the assertion boundary in debug. Release skips the check for zero overhead. `TryGet<T>()` type-checks silently and returns null on mismatch — safe for probe patterns. | Named Typed Slot Store | Accepted | Yes |
| BD-008 | Test utilities ship inside `DiaBlackboard/Testing/` | Platform-wide pattern (established by DiaStateMachine SD-017). Helpers live in the library; consumers opt in via include. | Test Utilities | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All slot keys and `BlackboardComponent::kUniqueId` use `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Blackboard operates *within* phases/modules; no dependency on DiaApplicationFlow. |
| PD-003 | Platform | Component-based entities | `BlackboardComponent` implements `IComponent`; registered via `ComponentFactoryRegistry`. |
| PD-004 | Platform | No STL containers in public APIs | Observer list and slot registry use `DiaCore::DynamicArrayC`. Internal slot storage may use type-erased allocation. |
| PD-005 | Platform | x64 only | `DiaBlackboard.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaBlackboard.vcxproj` and `.vcxproj.filters` created and maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. Template `Register<T>` / `Get<T>` can use concepts to constrain `T`. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaBlackboard.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any log output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.blackboard.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Blackboard::` namespace. |
| AD-005 | Dia App | Component-based entities | Reinforces PD-003 for `BlackboardComponent`. |

## Status

`Done`

**Plan:** [diablackboard.plan.md](diablackboard.plan.md)

# Feature Spec: State Machine Component

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diastatemachine.md | **state-machine-component** |

**Status:** `Done`

---

## Problem Statement

State machines need to be attached to game entities. The Dia engine uses `IComponent`/`IComponentObject` for entity composition (PD-003). Without a component wrapper, game code would need ad-hoc patterns for associating machines with entities — breaking the component model and preventing the factory/registry system from managing machine lifecycle.

---

## Solution Overview

`StateMachineComponent` implements `IComponent` and wraps any state machine type (flat, HSM, or pushdown). It registers with `ComponentFactoryRegistry` via both `DynamicComponentFactory` (heap-allocated) and `StaticPooledComponentFactory` (pre-allocated pool). Multiple `StateMachineComponent` instances can be attached to a single entity for independent FSMs (e.g., separate AI and animation machines).

The component owns the attached machine (which in turn owns its definition). It exposes the `IStateMachineInspectable` interface for tooling and typed access for game code that knows the specific machine type.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `StateMachineComponent` implements `IComponent` | Compile check |
| AC2 | `kUniqueId` is `StringCRC{"StateMachineComponent"}` | Unit test |
| AC3 | Registered with `ComponentFactoryRegistry` via `DynamicComponentFactory` | Unit test: create via registry |
| AC4 | Registered with `ComponentFactoryRegistry` via `StaticPooledComponentFactory` | Unit test: create via pool |
| AC5 | `GetInspectable()` returns non-null `IStateMachineInspectable*` | Unit test |
| AC6 | `GetMachine<FlatStateMachine<T>>()` returns typed pointer when machine is flat | Unit test |
| AC7 | `GetMachine<HierarchicalStateMachine<T>>()` returns typed pointer when machine is HSM | Unit test |
| AC8 | `GetMachine<PushdownAutomaton<T>>()` returns typed pointer when machine is pushdown | Unit test |
| AC9 | `GetMachine<WrongType>()` returns nullptr | Unit test |
| AC10 | Multiple `StateMachineComponent` instances on same entity — each independent | Unit test |
| AC11 | Two components from `Clone()`'d definitions — independent machines, both functional | Unit test: two components with cloned definitions transition independently |
| AC12 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

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

        // Machine identity (delegates to inspectable)
        Dia::Core::StringCRC GetMachineId() const;
    };

} // namespace Dia::StateMachine
```

---

## Implementation Notes

- Internally stores a type-erased pointer to the machine and a type tag enum (`kFlat`, `kHierarchical`, `kPushdown`) for `GetMachine<T>()` to validate the cast.
- `GetMachine<T>()` checks the type tag against `T` — returns nullptr on mismatch rather than undefined behavior.
- **Factory pattern:** `StateMachineComponent` is NOT created through a generic factory. Instead, game code constructs the typed machine externally and attaches it to the component via move:
  ```cpp
  // Game code creates the typed machine (definition moved in)
  auto def = StateMachineDefinition::LoadFromFile("enemy_ai.json", registry);
  auto machine = FlatStateMachine<EnemyCtx>(machineId, std::move(def), context);
  
  // Attach to component — component takes ownership
  auto* comp = entity->AddComponent<StateMachineComponent>();
  comp->AttachMachine(std::move(machine), MachineType::kFlat);
  ```
- `AttachMachine(void* machine, MachineType type)` takes ownership of the machine. The component destroys the machine when the component is destroyed.
- `DynamicComponentFactory` and `StaticPooledComponentFactory` create empty `StateMachineComponent` shells. The machine is attached after construction.
- Multiple components per entity: each component has its own machine ID. Game code queries by machine ID to find the right component.
- **Ownership chain:** Component owns machine → machine owns definition. Clean, single-owner lifetime. No non-owning pointers.

---

## Dependencies

### Required Features
- **inspection-interface** — `IStateMachineInspectable`
- **flat-state-machine**, **hierarchical-state-machine**, **pushdown-automaton** — wrapped machine types
- **state-machine-builder** — `StateMachineDefinition` (definition ownership)

### Required Modules
- **DiaCore** — `IComponent`, `IComponentObject`, `IComponentFactory`, `ComponentFactoryRegistry`, `DynamicComponentFactory`, `StaticPooledComponentFactory`, `StringCRC`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestStateMachineComponent.cpp`)

1. Create via `DynamicComponentFactory` — component valid, `GetInspectable()` non-null
2. Create via `StaticPooledComponentFactory` — component valid
3. `GetMachine<FlatStateMachine<TestCtx>>()` on flat component — returns typed pointer
4. `GetMachine<HierarchicalStateMachine<TestCtx>>()` on flat component — returns nullptr
5. Attach two `StateMachineComponent` to same entity — both function independently
6. Both components with `Clone()`'d definitions — transitions produce different runtime states
7. `GetMachineId()` returns correct ID for each component

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | `kUniqueId`, machine IDs all StringCRC |
| PD-003 | Platform | Component-based entities | Implements `IComponent`, uses `ComponentFactoryRegistry` |
| PD-004 | Platform | No STL in public APIs | No STL exposed |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::` |
| AD-005 | Dia App | Component-based entities | Both factory types registered |
| SD-005 | System | Machine owns definition | Component owns machine → machine owns definition. Clean single-owner chain |

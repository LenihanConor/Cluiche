# Feature Spec: Data-Driven Definitions

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diastatemachine.md | **data-driven-definitions** |

**Status:** `Done`

---

## Problem Statement

Code-defined state machines require recompilation for any structural change — adding a state, modifying a transition, tweaking a guard. For gameplay and AI iteration, designers need to modify machine topology without touching C++ code. Additionally, state machine definitions stored as data can be versioned, diffed, and validated independently from the code that executes them.

---

## Solution Overview

`StateMachineDefinition` and `HierarchicalStateMachineDefinition` gain `LoadFromJson()` and `LoadFromFile()` static factory methods. JSON defines the machine topology (states, transitions, triggers, guard/action names). C++ code provides the actual function pointers via a `CallbackRegistry` — an explicitly constructed, non-singleton object passed by reference to the load methods.

At load time, the JSON-declared callback names are resolved against the registry. Missing callbacks are a validation error caught immediately, not at runtime when a transition fires.

Definitions loaded from JSON are identical to definitions produced by the builder — the runtime machine doesn't know or care where its definition came from.

Definitions are immutable after load (SD-005). When a JSON file changes, the next machine constructed from that file gets the new version — no live patching of active machines (SD-016).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `StateMachineDefinition::LoadFromJson(root, registry)` produces valid definition | Unit test |
| AC2 | `StateMachineDefinition::LoadFromFile(path, registry)` loads file and produces definition | Unit test |
| AC3 | JSON states parsed: each state has `id` field (string → StringCRC) | Unit test |
| AC4 | JSON transitions parsed: `source`, `target`, `trigger` fields | Unit test |
| AC5 | JSON `onEnter`, `onExit`, `onUpdate` fields resolve to registry callbacks by name | Unit test |
| AC6 | JSON `guard` field resolves to registry guard callback by name | Unit test |
| AC7 | Missing callback name in registry → validation error, definition not created | Unit test |
| AC8 | `initialState` field sets the initial state | Unit test |
| AC9 | Wildcard source `"*"` parsed as `kAnyState` | Unit test |
| AC10 | `HierarchicalStateMachineDefinition::LoadFromJson()` parses `parent`/`children`/`history` | Unit test |
| AC11 | Loaded definition passes `Validate()` — reachability, no orphans, no duplicate IDs | Unit test |
| AC12 | Invalid JSON (missing required fields) → validation error with descriptive messages | Unit test |
| AC13 | `CallbackRegistry` is non-singleton — two registries with different callbacks produce different definitions | Unit test |
| AC14 | Definition from JSON identical in behavior to definition from builder (same states/transitions) | Unit test: both produce machines that behave identically |
| AC15 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

### CallbackRegistry

```cpp
namespace Dia::StateMachine {

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

} // namespace Dia::StateMachine
```

### JSON Schema — Flat

```json
{
    "type": "flat",
    "initialState": "Idle",
    "states": [
        {
            "id": "Idle",
            "onEnter": "Enemy.OnEnterIdle",
            "onExit": "Enemy.OnExitIdle",
            "onUpdate": "Enemy.UpdateIdle"
        },
        {
            "id": "Chase",
            "onEnter": "Enemy.OnEnterChase"
        }
    ],
    "transitions": [
        {
            "source": "Idle",
            "target": "Chase",
            "trigger": "OnSeePlayer",
            "guard": "Enemy.CanSeePlayer"
        },
        {
            "source": "*",
            "target": "Dead",
            "trigger": "OnHealthZero"
        }
    ]
}
```

### JSON Schema — Hierarchical

```json
{
    "type": "hierarchical",
    "initialState": "Alive",
    "states": [
        {
            "id": "Alive",
            "initialChild": "Idle",
            "history": true,
            "children": ["Idle", "Moving"],
            "onEnter": "Player.OnEnterAlive"
        },
        {
            "id": "Idle",
            "onUpdate": "Player.UpdateIdle"
        },
        {
            "id": "Moving",
            "initialChild": "Walking",
            "children": ["Walking", "Running"]
        },
        {
            "id": "Walking"
        },
        {
            "id": "Running"
        },
        {
            "id": "Dead",
            "onEnter": "Player.OnEnterDead"
        }
    ],
    "transitions": [
        {
            "source": "Idle",
            "target": "Walking",
            "trigger": "OnMove"
        },
        {
            "source": "Alive",
            "target": "Dead",
            "trigger": "OnDie"
        }
    ]
}
```

---

## Implementation Notes

- `CallbackRegistry` stores three `HashTable<StringCRC, FunctionPointer>` internally — one each for actions, guards, and updates.
- Callback name convention: `"Module.FunctionName"` using dot-namespacing. This is convention only, not enforced — the registry just stores StringCRC → pointer mappings.
- **Error handling:** `LoadFromJson()` first parses JSON, then resolves callback names against registry, then runs `Validate()`. If any step fails, the method returns a default-constructed (invalid) definition. Callers must call `Validate()` on the result and check for errors before passing it to a machine constructor. Constructing a machine with an invalid definition is a `DIA_ASSERT` in debug and undefined behavior in release. Missing callback names produce a descriptive validation error string (e.g., `"callback 'Enemy.OnEnterChase' not found in registry"`). The load is atomic — partial definitions are never returned.
- `LoadFromFile()` reads file via `DiaCore/FilePath/` utilities, parses with jsoncpp, delegates to `LoadFromJson()`.
- JSON `"type"` field (`"flat"` or `"hierarchical"`) determines which definition type to produce. Mismatched call (e.g., `StateMachineDefinition::LoadFromJson` on a `"hierarchical"` JSON) is a validation error.
- Null callback fields (omitted `onEnter`, `onExit`, etc.) produce null function pointers — the machine simply doesn't call anything for that hook.

---

## Dependencies

### Required Features
- **state-machine-builder** — `StateMachineDefinition`, `HierarchicalStateMachineDefinition`, `StateDef`, `TransitionDef`

### Required Modules
- **DiaCore** — `StringCRC`, `HashTable`, `DynamicArrayC`, `Json`, `FilePath`, `DIA_ASSERT`

### Dependent Features
- **flat-state-machine** — consumes definitions loaded from JSON
- **hierarchical-state-machine** — consumes definitions loaded from JSON
- **state-machine-component** — loads definitions for entity attachment

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestDataDrivenDefinitions.cpp`)

**CallbackRegistry:**
1. Register action + guard + update — Find returns correct pointers
2. Find unregistered name — returns nullptr
3. Has returns true/false correctly
4. Two separate registries — independent, no cross-contamination

**Flat JSON loading:**
5. Load valid flat JSON — definition has correct states and transitions
6. Callback names resolve to registry pointers
7. Missing callback name — validation error
8. Wildcard source `"*"` → `kAnyState`
9. Missing `initialState` — validation error
10. Missing `type` field — validation error

**Hierarchical JSON loading:**
11. Load valid hierarchical JSON — parent/child relationships correct
12. History flag parsed correctly
13. `initialChild` parsed for parent states

**Equivalence:**
14. Build identical machine via builder and via JSON — both definitions produce same state/transition topology

**File loading:**
15. `LoadFromFile()` with valid file path — succeeds
16. `LoadFromFile()` with nonexistent path — error

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | JSON string IDs converted to StringCRC on load |
| PD-004 | Platform | No STL in public APIs | DiaCore containers and HashTable |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::` |
| SD-005 | System | Machine owns definition | Immutable after LoadFromJson, moved into machine. Clone() for explicit sharing |
| SD-011 | System | Data-driven from JSON | Full JSON schema with callback resolution |
| SD-013 | System | No singleton registry | CallbackRegistry explicitly constructed and passed |
| SD-016 | System | Reload on next construct | No live patching; new definition on next load |

# Feature Spec: Hierarchical State Machine

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diastatemachine.md | **hierarchical-state-machine** |

**Status:** `Done`

---

## Problem Statement

Complex behaviors — player controllers, animation state, multi-phase AI — suffer from state explosion in flat FSMs. A player might have Idle, Walking, Running, Jumping, Falling, Attacking, Blocking, and Dead states, but many share common transitions (e.g., "from any grounded state, jump" or "from any alive state, die"). In a flat FSM, each shared transition must be duplicated per state. Hierarchical nesting lets parent states own shared transitions, and child states inherit them — dramatically reducing duplication and making the machine easier to reason about.

---

## Solution Overview

`HierarchicalStateMachine<TContext>` extends the flat model with state nesting. States can contain child states, forming a tree. The machine is always in exactly one leaf state, but the active state path includes all ancestors up to the root.

Key semantics (UML Statechart compliant):
- **Inherited transitions:** A transition on a parent state applies to all descendants. Child-level transitions take priority over parent-level for the same trigger.
- **Entry/exit propagation:** Transitioning between states in different branches exits from leaf up to Lowest Common Ancestor (LCA), then enters from LCA down to target leaf. LCA itself is neither exited nor entered.
- **Shallow history:** Parent states can opt into remembering their last active direct child. When re-entered, they resume that child instead of the default initial child.

Constructed from a `HierarchicalStateMachineDefinition&&` (move semantics — machine owns its definition). Implements `IStateMachineInspectable`.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Construct HSM — enters root's initial child state, calling OnEnter down the hierarchy | Unit test |
| AC2 | `Fire()` on leaf-level transition — transitions between siblings, OnExit/OnEnter called | Unit test |
| AC3 | `Fire()` on parent-level transition — triggers from any descendant of that parent | Unit test |
| AC4 | Child transition takes priority over parent transition for same trigger | Unit test |
| AC5 | Cross-branch transition: exit from leaf to LCA, enter from LCA to target leaf | Unit test with ordered callback tracking |
| AC6 | LCA is neither exited nor entered during cross-branch transition | Unit test |
| AC7 | `GetCurrentStateId()` returns the deepest active leaf state | Unit test |
| AC8 | `GetCurrentLeafStateId()` same as `GetCurrentStateId()` | Unit test |
| AC9 | `IsInState(parentId)` returns `true` when a descendant is active | Unit test |
| AC10 | `IsDescendantActive(ancestorId)` returns correct value | Unit test |
| AC11 | Shallow history: re-entering parent resumes last active child instead of default | Unit test |
| AC12 | Shallow history disabled (default) — re-entering parent goes to initial child | Unit test |
| AC13 | `Update()` calls OnUpdate on all active states (leaf + ancestors) bottom-up | Unit test |
| AC14 | Guard predicates work same as flat FSM — first-match wins (SD-007) | Unit test |
| AC15 | Max transitions per frame guard (SD-008) | Unit test |
| AC16 | `GetAllStates()` via inspection — `parentId` and `isLeaf` correct for nested states | Unit test |
| AC17 | Transition history records leaf-to-leaf transitions | Unit test |
| AC18 | No heap allocation during `Fire()` or `Update()` | Code review |
| AC19 | Full solution builds clean | `msbuild Cluiche.sln /p:Configuration=Debug /p:Platform=x64` |

---

## Public API

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

} // namespace Dia::StateMachine
```

---

## Implementation Notes

- **Wildcard transitions:** HSM does NOT support `kAnyState` — use parent-state transitions instead. A transition on a parent state already applies to all descendants, which is the hierarchical equivalent of wildcards. This avoids ambiguity about which level a wildcard applies at.
- **Transition resolution order:** Start at current leaf state. Check all matching transitions at that level in registration order — first guard that passes wins. If no transition matches at that level, walk up to parent and check all its transitions in registration order. Repeat until root or a match is found. A match at a deeper level always wins — the walk stops as soon as any transition at a level is found, even if it fires from that level (no fall-through to grandparent if a guard fails at parent level; all transitions at the parent level are tried first).
- **LCA computation:** Given source and target states, find their Lowest Common Ancestor by walking both paths to root and finding the first shared node. Cache LCA per transition definition for O(1) lookup at runtime.
- **Entry/exit sequence:** Exit: leaf → leaf.parent → ... → LCA.child (stop before LCA). Enter: LCA.child → ... → target.parent → target. Each step calls OnExit/OnEnter with context.
- **History storage:** `HashTable<StringCRC, StringCRC>` mapping parent state ID → last active direct child ID. Updated on every transition that exits a parent. Only stored for parents with history enabled.
- **Active state path:** Stored as a stack/array from root to leaf. Allows O(1) `IsInState()` check via scanning the path. Rebuilt on each transition.
- **OnUpdate order:** `Update()` calls OnUpdate on all states in the active path from leaf to root (bottom-up). If an OnUpdate action causes a `Fire()` that changes state, the current `Update()` completes for the remaining active path states, and the new state's OnUpdate is called on the next frame — not mid-iteration.
- **Shallow history (SD-009):** `GetHistoryState()` returns the last direct child that was active when the parent was exited. History is updated whenever a transition causes exit from a parent state. If a parent has never been exited, `GetHistoryState()` returns `kInvalidCRC` and the initial child is used on entry. History persists for the machine's lifetime — there is no `ClearHistory()` in v1. If the active child changes within a parent (sibling-to-sibling transition), history is updated to reflect the new child.
- Shallow history only — no deep history. `GetHistoryState()` returns the last direct child, not the full subtree path.

---

## Dependencies

### Required Features
- **inspection-interface** — `IStateMachineInspectable`
- **state-machine-builder** — produces `HierarchicalStateMachineDefinition`

### Required Modules
- **DiaCore** — `StringCRC`, `DynamicArrayC`, `HashTable`, `DIA_ASSERT`

### Dependent Features
- **state-machine-component** — wraps HSM instances
- **transition-logging** — traces via `IStateMachineInspectable`

---

## Testing Strategy

### Unit Tests (`Cluiche/Tests/GoogleTests/StateMachine/TestHierarchicalStateMachine.cpp`)

**Basic hierarchy (3 levels: Root → {Alive, Dead}, Alive → {Idle, Moving}, Moving → {Walking, Running}):**
1. Construct — enters Root → Alive → Idle (deepest initial child)
2. Fire "StartWalk" (Idle → Walking transition) — OnExit(Idle), OnEnter(Walking)
3. Fire "Die" (Alive-level transition) — exits Walking → Moving → Alive, enters Dead

**Inherited transitions:**
4. "Die" trigger from Idle — works (inherited from Alive parent)
5. "Die" trigger from Running — works (inherited from Alive parent)
6. Child-specific "Die" on Running overrides parent — child version fires

**LCA transitions:**
7. Transition from Walking to Idle — LCA is Alive (not exited/entered)
8. Transition from Walking to Dead — LCA is Root, exits Walking → Moving → Alive, enters Dead

**History:**
9. History enabled on Alive — enter Alive → go to Running → exit to Dead → re-enter Alive → resumes Running (not Idle)
10. History disabled on Alive — re-enter always goes to Idle

**Update propagation:**
11. In state Walking (child of Moving, child of Alive) — `Update()` calls OnUpdate for Walking, Moving, Alive (bottom-up)

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for IDs | All state/transition IDs are StringCRC |
| PD-004 | Platform | No STL in public APIs | DiaCore containers |
| PD-007 | Platform | C++20 | Concept constraint on TContext |
| AD-003 | Dia App | Namespace | `Dia::StateMachine::` |
| SD-001 | System | Separate HSM implementation | Full hierarchy support without flat overhead |
| SD-005 | System | Machine owns definition | Constructor takes `Definition&&` — move semantics |
| SD-003 | System | Raw function pointers | All callbacks are raw pointers |
| SD-007 | System | First-match guard semantics | Matches at deepest level first |
| SD-009 | System | Shallow history only | GetHistoryState returns direct child only |

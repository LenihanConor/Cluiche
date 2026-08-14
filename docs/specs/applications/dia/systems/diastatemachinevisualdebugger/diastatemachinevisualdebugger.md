# System Spec: DiaStateMachineVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaStateMachineVisualDebugger is the `IDebugDomain` implementation that makes state machine execution visible in `DiaDebugPanel`. It is a panel-only domain (`HasWorldDrawers() = false`) — state machine topology is logical, not spatial.

The domain card shows: all states with the current state highlighted, a transition history log, and the guard pass/fail results from the most recent transition attempt. Guard results are captured via `ITransitionListener` registered at construction.

`IStateMachineInspectable` was designed for exactly this use case. No new accessors are required in `DiaStateMachine`.

Following the `DiaXxxVisualDebugger` contract, `DiaStateMachine` has zero compile-time dependency on `DiaStateMachineVisualDebugger`.

## Responsibilities

- Implement `IDebugDomain` — all 16-AC contract methods
- Implement `ITransitionListener` — caches `TransitionEvent.guardResults` from `OnTransition()` for JSON emission
- `HasWorldDrawers()` returns `false`; `Register`/`Unregister` are no-ops
- Two logical drawers: `StateList` (all states + current), `TransitionHistory` (last N transitions + guard results)
- `GetJSONState()` emits: states array, history array, cached guard results, machine stats
- `OnCommand("toggle", {drawer: name})` enables/disables `StateList` or `TransitionHistory` section
- `OnCommand("setScale", ...)` — no-op
- Registers itself as `ITransitionListener` via `mInspectable.SetTransitionListener(this)` on construction; clears on destruction
- Entire module guarded by `#ifdef DIA_DEBUG`
- `DiaStateMachineVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- `dia.diastatemachinevisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- State machine execution — DiaStateMachine
- Guard condition evaluation — DiaCondition
- World-space entity labels — DiaEntityVisualDebugger concern
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Public Interfaces

```cpp
// DiaStateMachineVisualDebugger/StateMachineVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaStateMachine/IStateMachineInspectable.h>

namespace Dia::StateMachine
{
    class StateMachineVisualDebugger
        : public Dia::VisualDebugger::IDebugDomain
        , public Dia::StateMachine::ITransitionListener
    {
    public:
        // Registers as ITransitionListener on construction.
        // inspectable must outlive this object.
        explicit StateMachineVisualDebugger(IStateMachineInspectable& inspectable);
        ~StateMachineVisualDebugger();

        // IDebugDomain — identity
        Dia::Core::StringCRC  GetDomainId()      const override; // "statemachine"
        const char*           GetDisplayName()   const override; // "State Machine"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "AIBehavior"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kAIBehavior

        bool HasWorldDrawers() const override { return false; }

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Dia::Core::JsonValue& args) override;

        // ITransitionListener
        void OnTransition(const TransitionEvent& event) override;
        void OnTransitionFailed(Dia::Core::StringCRC machineId,
                                Dia::Core::StringCRC currentStateId,
                                Dia::Core::StringCRC triggerId) override;

    private:
        IStateMachineInspectable& mInspectable;
        bool mStateListEnabled      = true;
        bool mTransitionHistEnabled = true;

        // Cached from most recent OnTransition call (Sim PU only — no cross-PU access).
        struct CachedGuardResult {
            Dia::Core::StringCRC guardName;
            bool passed;
        };
        Dia::Core::Containers::DynamicArrayC<CachedGuardResult, 8> mLastGuardResults;
        Dia::Core::StringCRC mLastTransitionSource;
        Dia::Core::StringCRC mLastTransitionTarget;
        bool mLastTransitionFailed = false;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"State machine — states, transition history, guard pass/fail"` (60 chars ≤ 80 ✓)

### JSON State Schema

```json
{
  "drawers": [
    { "name": "StateList",        "enabled": true },
    { "name": "TransitionHistory","enabled": true }
  ],
  "stats": {
    "stateCount":      4,
    "currentState":    "patrol",
    "historyLength":   3
  },
  "states": [
    { "id": "idle",         "active": false, "isLeaf": true  },
    { "id": "patrol",       "active": true,  "isLeaf": true  },
    { "id": "alert",        "active": false, "isLeaf": true  },
    { "id": "combat",       "active": false, "isLeaf": false }
  ],
  "history": [
    { "from": "idle",   "to": "patrol", "trigger": "start_patrol", "timestamp": 1.23 },
    { "from": "patrol", "to": "alert",  "trigger": "enemy_spotted","timestamp": 4.56 },
    { "from": "alert",  "to": "patrol", "trigger": "lost_target",  "timestamp": 7.89 }
  ],
  "lastGuardResults": [
    { "guard": "health_ok",     "passed": true  },
    { "guard": "enemy_visible", "passed": false }
  ],
  "lastTransitionFailed": false
}
```

`"lastGuardResults"` is an empty array when no transition has fired since construction. `"lastTransitionFailed"` is `true` if the most recent event was `OnTransitionFailed`. `"states"` array uses `GetAllStates()` cross-referenced against `GetCurrentStateId()` for the `"active"` flag.

### Panel Card Specification

- **Group:** AI / Behavior — accent `DebugGroupAccents::kAIBehavior` (`#ef4444`) via `var(--accent)`
- **Domain stat line:** `"State: <currentState>"` or `"No state"` if machine has no active state
- **Expanded body — StateList section:** states table; active state highlighted in accent colour, leaf states shown differently from composite states
- **Expanded body — TransitionHistory section:** chronological list of recent transitions; `from → to (trigger)` format; failed transitions shown in a warning colour
- **Guard results row:** appears below TransitionHistory for the most recent transition; passed guards in green, failed in red/warning colour
- **Drawer toggles:** StateList, TransitionHistory

## Tests

`Tests/GoogleTests/DiaStateMachineVisualDebugger/TestStateMachineVisualDebugger.cpp`

### Mandatory shapes (AC-15)

| Shape | Description |
|-------|-------------|
| DrawerGate | Disabling `StateList` section removes `states` key from JSON output |
| PrimitiveType | `HasWorldDrawers()` returns `false`; `GetDrawerCount()` returns 0 |
| ScaleSensitivity | N/A (no world-space output) — satisfied by verifying `OnCommand("setScale",...)` is a no-op and does not crash |
| JSONRoundTrip | `GetJSONState()` drawer entries match current enabled state for both drawers |
| OnCommandRoundTrip | `OnCommand("toggle", {drawer:"StateList"})` toggles `mStateListEnabled`; second call reverts |

### Domain-specific shapes

| Shape | Description |
|-------|-------------|
| `States_CountMatchesGetAllStates` | `stats.stateCount` equals size of `GetAllStates()` output |
| `States_CurrentState_MarkedActive` | State with ID == `GetCurrentStateId()` has `"active": true`; all others `false` |
| `History_MatchesGetTransitionHistory` | `"history"` array length == `GetTransitionHistory()` count; entries match |
| `GuardResults_CapturedOnTransition` | After `OnTransition()` with 2 guard results, `lastGuardResults` has 2 entries with correct pass/fail |
| `GuardResults_Empty_BeforeAnyTransition` | On fresh domain, `lastGuardResults` is empty array |
| `TransitionFailed_FlagSet` | After `OnTransitionFailed()`, `lastTransitionFailed == true` in JSON |
| `TransitionFailed_FlagCleared_AfterSuccess` | After `OnTransition()` following a failed one, `lastTransitionFailed == false` |
| `Toggle_TransitionHistory` | `OnCommand("toggle", {drawer:"TransitionHistory"})` toggles `mTransitionHistEnabled` |
| `DrawerGate_TransitionHistory` | `mTransitionHistEnabled == false` removes `history` key from JSON |
| `NoStates_EmptyArrays_NoAssert` | Empty machine → empty `states` + `history` arrays, no crash |
| `Destructor_ClearsListener` | Destroying domain calls `mInspectable.SetTransitionListener(nullptr)` |
| `SetScale_NoOp` | `OnCommand("setScale", ...)` returns without crashing |

## Dependencies on Other Systems

**Required:**
- **DiaStateMachine** — `IStateMachineInspectable`, `ITransitionListener`, `TransitionEvent`, `StateInfo`, `TransitionRecord`
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`, `DynamicArrayC`

**Explicitly excluded:**
- **ImGui**, **DiaEntity**, **DiaApplicationFlow**, **DiaCondition**, **DiaAIBudget**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply.

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaStateMachineVisualDebugger.vcxproj` |
| AC-2 | `DiaStateMachine` has zero dep on `DiaStateMachineVisualDebugger` |
| AC-3 | Deps: DiaStateMachine + DiaVisualDebugger + DiaCore only |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 60 chars ✓ |
| AC-6 | No world drawers — palette rule N/A |
| AC-7 | No world drawers — scale rule N/A |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `IStateMachineInspectable&` read via const accessors; `ITransitionListener` callback writes to Sim-PU-owned cache only |
| AC-10 | Emits `drawers` + `stats` minimum |
| AC-11 | `OnCommand("toggle", {drawer})` handled for both drawers |
| AC-12 | `OnCommand("setScale", ...)` is no-op; drawer enables use `std::atomic<bool>` |
| AC-13/14 | `Tests/GoogleTests/DiaStateMachineVisualDebugger/TestStateMachineVisualDebugger.cpp` |
| AC-15 | All 5 mandatory shapes present — see Tests section |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | Domain implements `ITransitionListener` to receive live guard results | `IStateMachineInspectable` has no `GetLastGuardResults()` accessor; the listener callback is the intended mechanism. No new DiaStateMachine API required. | Accepted | Yes |
| SD-002 | Guard results cached in Sim-PU-only member | `OnTransition` fires on Sim PU; `GetJSONState` is called on Sim PU by `VisualDebuggerModule`. No cross-PU access — direct member write is safe. | Accepted | Yes |
| SD-003 | `OnCommand` drawer booleans use `std::atomic<bool>` | `OnCommand` arrives on Render PU (JS bridge); drawer state is read on Sim PU during `GetJSONState`. Atomic is the minimum safe primitive. | Accepted | Yes |
| SD-004 | `"drawers"` are logical sections (StateList, TransitionHistory), not world-space `IVisualDebugger` instances | Panel-only domain; drawers represent panel card sections that can be independently hidden. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, drawer names, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` used; no `std::vector` return values |
| PD-005 | Platform | x64 only | vcxproj targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diastatemachinevisualdebugger.architecture.module.md` required |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::StateMachine::` namespace |

## Open Design Questions

1. **`SetTransitionListener` ownership** — if a machine already has a registered listener at construction time, setting `this` will silently replace it. Consider asserting no existing listener in debug, or chaining via a multi-listener wrapper. Decide at integration based on whether existing callsites register their own listeners.

2. **Guard result capacity** — `mLastGuardResults` is `DynamicArrayC<CachedGuardResult, 8>`. If a transition has more than 8 guards, excess are silently truncated. Add a `truncated` flag to the JSON if needed.

## Status

**Status:** Approved

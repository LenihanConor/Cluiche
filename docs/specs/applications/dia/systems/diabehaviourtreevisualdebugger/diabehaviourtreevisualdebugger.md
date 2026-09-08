# System Spec: DiaBehaviourTreeVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaBehaviourTreeVisualDebugger is the `IDebugDomain` implementation that makes behaviour tree execution state visible in `DiaDebugPanel`. It is a panel-only domain (`HasWorldDrawers() = false`) — node trees have no canonical world-space anchor.

The domain card shows: per-node tick result list ordered by visit sequence, a stat line with total node count and last tree result, and a toggleable "TreeView" drawer. All state is surfaced via `GetJSONState()` to the panel's BehaviourTree card template in `debug-panel.html`. Live node visit events are captured via `IBehaviourTreeEventListener`, eliminating the need to poll the component on each render tick.

Following the `DiaXxxVisualDebugger` contract (`debugger-contract.md`), `DiaBehaviourTree` has zero compile-time dependency on `DiaBehaviourTreeVisualDebugger`.

## Responsibilities

- Implement `IDebugDomain` — all pure virtual methods
- `HasWorldDrawers()` returns `false` — no world-space drawers; `Register`/`Unregister` are no-ops
- Attach as `IBehaviourTreeEventListener` on construction; detach on destruction
- Accumulate per-node visit records (`OnNodeEntered` / `OnNodeCompleted`) into an internal ring buffer per tick; flush on `GetJSONState()`
- `GetJSONState()` emits node traversal list with per-node result, stats block, root node ID, and drawers array (see JSON Schema)
- `OnCommand("toggle", {drawer: "TreeView"})` — enables/disables the node traversal list in the panel card
- `OnCommand("setScale", ...)` — no-op (no world-space sizes)
- Reads exclusively from `BehaviourTreeComponent`'s public read API — no write access
- Entire module guarded by `#ifdef DIA_DEBUG`
- Provide `DiaBehaviourTreeVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- Provide `dia.diabehaviourtreevisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Behaviour tree execution — `BehaviourTreeComponent` / caller
- Blackboard value writing — `DiaBehaviourTree` + `DiaBlackboard`
- Entity position tracking or world-space node labels — panel-only domain
- Full tree topology serialisation — `GetJSONState()` emits visited nodes only (see Open Design Question 2)
- Automatic domain registration — caller registers with `DiaDebugDomainRegistry`
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Public Interfaces

```cpp
// DiaBehaviourTreeVisualDebugger/BehaviourTreeVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>

namespace Dia::BehaviourTree { class BehaviourTreeComponent; }

namespace Dia::BehaviourTree
{
    class BehaviourTreeVisualDebugger
        : public Dia::VisualDebugger::IDebugDomain
        , public Dia::BehaviourTree::IBehaviourTreeEventListener
    {
    public:
        explicit BehaviourTreeVisualDebugger(const BehaviourTreeComponent& component);
        ~BehaviourTreeVisualDebugger() override;

        // IDebugDomain — identity
        Dia::Core::StringCRC  GetDomainId()      const override; // "behaviour-tree"
        const char*           GetDisplayName()   const override; // "Behaviour Tree"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "AIBehavior"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kAIBehavior

        bool HasWorldDrawers() const override { return false; }

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd,
                       const Dia::Core::JsonValue& args) override;

        // IBehaviourTreeEventListener
        void OnNodeEntered(Dia::Core::StringCRC nodeId) override;
        void OnNodeCompleted(Dia::Core::StringCRC nodeId, NodeResult result) override;
        void OnTreeCompleted(NodeResult result) override;

    private:
        struct NodeVisit
        {
            Dia::Core::StringCRC nodeId;
            NodeResult           result;   // kRunning used for "Entered but not yet completed"
            bool                 entered;  // true = OnNodeEntered fired; false = OnNodeCompleted fired
        };

        const BehaviourTreeComponent& mComponent;
        bool                          mTreeViewEnabled = true;

        // Accumulates node visit events for the current tick; flushed on GetJSONState()
        static constexpr int kMaxVisitsPerTick = 128;
        NodeVisit mPendingVisits[kMaxVisitsPerTick];
        int       mPendingCount = 0;

        // Snapshot committed at GetJSONState() time
        NodeVisit mLastTickNodes[kMaxVisitsPerTick];
        int       mLastTickCount = 0;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"Behaviour tree — active node, per-node tick result, entity cursor, resume state"` (79 chars ≤ 80 ✓)

### JSON State Schema

`GetJSONState()` emits the following structure. This is the normative C++↔panel contract for the BehaviourTree card template in `debug-panel.html`.

```json
{
  "drawers": [
    { "name": "TreeView", "enabled": true }
  ],
  "stats": {
    "nodeCount": 12,
    "lastResult": "Running"
  },
  "rootNodeId": "root_sequence",
  "lastTickNodes": [
    { "nodeId": "root_sequence",   "result": "Entered"  },
    { "nodeId": "check_in_range",  "result": "Success"  },
    { "nodeId": "move_to_target",  "result": "Running"  }
  ]
}
```

**`lastTickNodes` values:**
- `"Entered"` — `OnNodeEntered` fired; `OnNodeCompleted` not yet fired for this node in this tick
- `"Running"` — `OnNodeCompleted` fired with `kRunning`
- `"Success"` — `OnNodeCompleted` fired with `kSuccess`
- `"Failure"` — `OnNodeCompleted` fired with `kFailure`

`"lastTickNodes"` is ordered by visit sequence (chronological order of event callbacks). An empty array indicates no tick has occurred yet. `"stats.lastResult"` mirrors `component.LastResult()` mapped to the same four string values; `"None"` is used when no tick has completed. `"rootNodeId"` is the CRC name string of the root node from `BehaviourTreeAsset::GetRootNodeId()`; omitted when `!HasAsset()`.

### Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** AI / Behavior — accent `DebugGroupAccents::kAIBehavior` (`#ef4444`) via `var(--accent)`
- **Domain stat line:** `"Nodes: N  Last: Success/Failure/Running/None"` where N = `stats.nodeCount`
- **Expanded body:** node traversal list — each row shows node ID name + result badge (green=Success, red=Failure, yellow=Running, grey=Entered); rows ordered by visit sequence, most-recent tick only; "TreeView" toggle hides the list; domain card header always visible
- **Drawer toggle:** "TreeView" — hides/shows the node traversal list

## Dependencies on Other Systems

**Required:**
- **DiaBehaviourTree** — `BehaviourTreeComponent`, `IBehaviourTreeEventListener`, `BehaviourTreeAsset`, `NodeResult`
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`

**Explicitly excluded:**
- **ImGui** — retired; no `ImGui::*` calls in this module
- **DiaApplicationFlow**, **DiaEntity**, **DiaAIBudget**, **DiaCondition**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply to this module:

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaBehaviourTreeVisualDebugger.vcxproj` |
| AC-2 | `DiaBehaviourTree` has zero `#include` or link dep on `DiaBehaviourTreeVisualDebugger` |
| AC-3 | Deps: `DiaBehaviourTree` + `DiaVisualDebugger` + `DiaCore` |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 79 chars ✓ |
| AC-6 | No world drawers → palette rule N/A |
| AC-7 | No world drawers → scale rule N/A |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const BehaviourTreeComponent&` — read-only; event callbacks are const-correct; no shared mutable state |
| AC-10 | `GetJSONState()` emits `{ "drawers": [...], "stats": {} }` minimum ✓ |
| AC-11 | `OnCommand("toggle", {drawer: "TreeView"})` handled |
| AC-12 | `OnCommand("setScale", ...)` handled (no-op) |
| AC-13/14 | `Tests/GoogleTests/DiaBehaviourTreeVisualDebugger/TestBehaviourTreeVisualDebugger.cpp` |
| AC-15 | Mandatory test shapes: toggle gate, JSON round-trip, OnCommand round-trip, OnNodeEntered/OnNodeCompleted accumulation |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `HasWorldDrawers() = false` — panel-only domain | Behaviour tree node execution has no world-space anchor; tree structure is logical not spatial. If per-entity world labels prove useful, add as a second drawer with `HasWorldDrawers() = true`. | Accepted | Yes |
| SD-002 | Listener-driven accumulation via `IBehaviourTreeEventListener` | Polling `BehaviourTreeComponent` on each render tick would miss intermediate node results in a single game tick. Event callbacks capture the full traversal sequence. | Accepted | Yes |
| SD-003 | No entity selector — one instance per component | Same pattern as `DiaHTNVisualDebugger`. Multi-entity comparison is a `DiaAIInspector` (editor) concern. The visual debugger serves the per-entity use case where game code knows which entity to inspect. | Accepted | Yes |
| SD-004 | Fixed-size ring buffer (`kMaxVisitsPerTick = 128`) for pending visits | Avoids heap allocation on the sim thread during event callbacks. 128 visits covers all practical behaviour trees; if exceeded, oldest entries are silently dropped. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `JsonWriter` and fixed-size arrays used; no `std::vector` return values |
| PD-005 | Platform | x64 only | `DiaBehaviourTreeVisualDebugger.vcxproj` targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | Compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diabehaviourtreevisualdebugger.architecture.module.md` created with correct YAML frontmatter |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::BehaviourTree::` namespace |

## Open Design Questions

1. **Per-entity selector** — One debugger instance per component, same pattern as `DiaHTNVisualDebugger`. If multi-entity comparison is needed, the caller creates multiple instances and registers each with a distinct domain ID suffix. This is a `DiaAIInspector` concern and out of scope here.

2. **Tree structure rendering** — `GetJSONState()` emits `lastTickNodes` (visited sequence) but not the full tree topology. If the panel needs to render the tree as a graph (with parent-child edges), `BehaviourTreeAsset` node descriptors (`children`, `childId`) would need to be serialised into the JSON. Defer until panel work begins; the `rootNodeId` field is included now to anchor any future topology walk.

3. **`IBehaviourTreeEventListener` availability** — `IBehaviourTreeEventListener.h` is being added in Task 8 of the DiaBehaviourTree plan. If the interface is not yet merged when implementation of this module begins, stub out listener registration and emit static JSON (`lastTickNodes: []`, `stats.lastResult: "None"`) until the interface lands.

## Status

**Status:** `Approved`

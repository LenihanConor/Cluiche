# System Spec: DiaHTNVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaHTNVisualDebugger is the `IDebugDomain` implementation that makes HTN plan state visible in `DiaDebugPanel`. It is a panel-only domain (`HasWorldDrawers() = false`) — plan execution has no canonical world-space anchor.

The domain card shows: active plan task list with cursor, remaining-task stat, diverged/replan warning badge, and plan cost. All state is surfaced via `GetJSONState()` to the panel's HTN card template in `debug-panel.html`.

Following the `DiaXxxVisualDebugger` contract (`debugger-contract.md`), `DiaHTN` has zero compile-time dependency on `DiaVisualDebugger`.

## Responsibilities

- Implement `IDebugDomain` — all pure virtual methods
- `HasWorldDrawers()` returns `false` — no world-space drawers; `Register`/`Unregister` are no-ops
- `GetJSONState()` emits plan list with per-task status, current task index, diverged flag, and cost (see JSON Schema)
- `OnCommand("toggle", {drawer: "PlanView"})` — enables/disables the plan list section in the panel card
- `OnCommand("setScale", ...)` — no-op (no world-space sizes)
- Reads exclusively from `HTNPlannerComponent`'s public read API — no write access
- Entire module guarded by `#ifdef DIA_DEBUG`
- Provide `DiaHTNVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- Provide `dia.diahtnvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- HTN plan execution — `HTNPlannerComponent` / caller
- World-state data writing — DiaHTN + DiaCondition
- Entity position tracking or world-space labels — deferred (see Open Design Question 1)
- Automatic domain registration — caller registers with `DiaDebugDomainRegistry`
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Public Interfaces

```cpp
// DiaHTNVisualDebugger/HTNVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace Dia::HTN { class HTNPlannerComponent; }

namespace Dia::HTN
{
    class HTNVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        explicit HTNVisualDebugger(const HTNPlannerComponent& component);

        // IDebugDomain — identity
        Dia::Core::StringCRC  GetDomainId()      const override; // "htn"
        const char*           GetDisplayName()   const override; // "HTN"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "AIBehavior"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kAIBehavior

        bool HasWorldDrawers() const override { return false; }

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd,
                       const Dia::Core::JsonValue& args) override;

    private:
        const HTNPlannerComponent& mComponent;
        bool mPlanViewEnabled = true;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"HTN planner — active plan, task cursor, divergence and replan state"` (62 chars ≤ 80 ✓)

### JSON State Schema

`GetJSONState()` emits the following structure. This is the normative C++↔panel contract for the HTN card template in `debug-panel.html`.

```json
{
  "drawers": [
    { "name": "PlanView", "enabled": true }
  ],
  "stats": {
    "current": 2,
    "total": 5
  },
  "plan": [
    { "index": 1, "name": "navigate_to_patrol_point", "status": "done"    },
    { "index": 2, "name": "wait_at_point",             "status": "current" },
    { "index": 3, "name": "pick_next_waypoint",        "status": "pending" },
    { "index": 4, "name": "navigate_to_patrol_point",  "status": "pending" },
    { "index": 5, "name": "report_clear",              "status": "pending" }
  ],
  "diverged": false,
  "planCost": 14.2
}
```

**Status values:** `"done"` — completed; `"current"` — cursor is here; `"pending"` — not yet reached. `"plan"` is an empty array when `!HasActivePlan()`. `"diverged": true` triggers a `⚠ Replan needed` badge in the card header. `"planCost"` is omitted when no plan is active (see Open Design Question 3).

### Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** AI / Behavior — accent `DebugGroupAccents::kAIBehavior` (`#ef4444`) via `var(--accent)`
- **Domain stat line:** `"Task N/M"` where N = current index, M = total
- **Expanded body:** numbered task list — done rows greyed + strikethrough, current row highlighted with `▶` cursor in accent color, pending rows dimmed; `"diverged": true` adds a `⚠ Replan needed` badge at the bottom
- **Drawer toggle:** "PlanView" — hides/shows the task list; domain card header always visible

## Dependencies on Other Systems

**Required:**
- **DiaHTN** — `HTNPlannerComponent`, `HTNPlan`, `HTNTask`
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`

**Explicitly excluded:**
- **ImGui** — retired; no `ImGui::*` calls in this module
- **DiaApplicationFlow**, **DiaEntity**, **DiaCondition**, **DiaAIBudget**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply to this module:

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaHTNVisualDebugger.vcxproj` |
| AC-2 | `DiaHTN` has zero `#include` or link dep on `DiaHTNVisualDebugger` |
| AC-3 | Deps: `DiaHTN` + `DiaVisualDebugger` + `DiaCore` |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 62 chars ✓ |
| AC-6 | No world drawers → palette rule N/A |
| AC-7 | No world drawers → scale rule N/A |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const HTNPlannerComponent&` — read-only, no shared mutable state |
| AC-10 | `GetJSONState()` emits `{ "drawers": [...], "stats": {} }` minimum ✓ |
| AC-11 | `OnCommand("toggle", {drawer: "PlanView"})` handled |
| AC-12 | `OnCommand("setScale", ...)` handled (no-op) |
| AC-13/14 | `Tests/GoogleTests/DiaHTNVisualDebugger/TestHTNVisualDebugger.cpp` |
| AC-15 | Mandatory test shapes: toggle gate, JSON round-trip, OnCommand round-trip |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `HasWorldDrawers() = false` — panel-only domain | HTN plan execution has no world-space anchor. The old `HTNWorldStateDrawer` (entity label showing current operator) is deferred — entity world labels belong in `DiaEntityVisualDebugger`. | Accepted | Yes |
| SD-002 | Single `HTNVisualDebugger` class replaces two-drawer design | The prior two-class split (`HTNPlanDrawer` + `HTNWorldStateDrawer`) mapped to ImGui vs. world-space separation. Under `IDebugDomain` all data flows through `GetJSONState()` to one panel card. | Accepted | Yes |
| SD-003 | No entity selector — one instance per component | Multi-entity view is a `DiaAIInspector` (editor) concern. The visual debugger serves the per-entity use case where game code knows which entity to inspect. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `JsonWriter` and `DynamicArrayC` used; no `std::vector` return values |
| PD-005 | Platform | x64 only | `DiaHTNVisualDebugger.vcxproj` targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | Compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diahtnvisualdebugger.architecture.module.md` updated to reflect new interface |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::HTN::` namespace |

## Open Design Questions

1. **World-space entity label** — The old `HTNWorldStateDrawer` showed `"<operator> [N]"` above the entity's world position. In the new design this is dropped (`HasWorldDrawers() = false`). If per-entity world labels prove useful during ArenaTestStage work, re-introduce as a second drawer: set `HasWorldDrawers() = true`, add `HTNWorldLabelDrawer` via `GetDrawer()`, wire entity world position at construction.

2. **`HTNPlan::GetCurrentTaskIndex()`** — `GetJSONState()` needs to emit the current task index to populate the `"current"` status field. `HTNPlan` currently has `GetTaskCount()` and `IsComplete()` but no `GetCurrentTaskIndex()`. Add a minimal `GetCurrentTaskIndex()` accessor to `HTNPlan` in `DiaHTN` at implementation time.

3. **Plan cost availability** — `"planCost"` in the JSON schema assumes `HTNPlan` exposes a `GetCost()` or equivalent. If no such accessor exists, omit `planCost` from the emitted JSON and remove the `Cost:` badge from the panel card template. Resolve at implementation.

## Status

**Status:** `Done`

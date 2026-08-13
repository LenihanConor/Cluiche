# System Spec: DiaRulesVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaRulesVisualDebugger is the `IDebugDomain` implementation that makes rule evaluation visible in `DiaDebugPanel`. It is a panel-only domain (`HasWorldDrawers() = false`) — rule evaluation has no world-space anchor.

The domain card shows: a per-rule fired/not-fired table for the current frame's `Evaluate()` call, including dispatched actions per fired rule. Data comes from a prerequisite debug accessor on `RuleSet`.

Following the `DiaXxxVisualDebugger` contract, `DiaRules` has zero compile-time dependency on `DiaVisualDebugger`.

## Responsibilities

- **Prerequisite — add to `DiaRules`**: `RuleSet::GetLastFireReport()` and `RuleSet::GetAllRuleIds()` accessors (`#ifdef DIA_DEBUG`), populated during `Evaluate()` (see Prerequisite section below)
- Implement `IDebugDomain` — all pure virtual methods
- `HasWorldDrawers()` returns `false`
- `GetJSONState()` emits current-frame rule evaluation state (see JSON Schema)
- `OnCommand("toggle", {drawer: "FireLog"})` — enables/disables the fire log section
- `OnCommand("setScale", ...)` — no-op
- Reads exclusively from `RuleSetComponent` public API
- Entire module guarded by `#ifdef DIA_DEBUG`
- Provide `DiaRulesVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.0-Gameplay`
- Provide `dia.diarulesvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Rule condition evaluation — DiaCondition
- Action dispatch — DiaRules `Evaluate()` (already handles this)
- Cooldown or scoring logic — DiaUtilityAI
- Entity position or world-space drawing — no world-space anchor for rule evaluation
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Prerequisite: `RuleSet` Debug Accessors

These changes live in `DiaRules`, not in `DiaRulesVisualDebugger`. Both must be implemented before this domain can be built.

```cpp
// DiaRules/RuleSet.h  (additions, DIA_DEBUG only)
#ifdef DIA_DEBUG

struct RuleFireEntry {
    Dia::Core::StringCRC ruleId;
    Dia::Core::DynamicArrayC<Dia::Core::StringCRC, 8> actions;
};

// Populated after the most recent Evaluate() call.
// Returns count of rules that fired.
int GetLastFireReport(
    Dia::Core::DynamicArrayC<RuleFireEntry, 16>& outEntries) const;

// All rule IDs registered in the set (fired or not).
// Enables the panel to show the full rule inventory, not just fired rules.
int GetAllRuleIds(
    Dia::Core::DynamicArrayC<Dia::Core::StringCRC, 32>& outIds) const;

#endif
```

`Evaluate()` clears `mLastFireReport`, appends one `RuleFireEntry` per fired rule (including action IDs), then leaves the report in place until the next call. `GetAllRuleIds()` returns all registered rule IDs in definition order.

## Public Interfaces

```cpp
// DiaRulesVisualDebugger/RulesVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace Dia::Rules { class RuleSetComponent; }

namespace Dia::Rules
{
    class RulesVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        explicit RulesVisualDebugger(const RuleSetComponent& component);

        // IDebugDomain — identity
        Dia::Core::StringCRC  GetDomainId()      const override; // "rules"
        const char*           GetDisplayName()   const override; // "Rules"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "AIBehavior"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kAIBehavior

        bool HasWorldDrawers() const override { return false; }

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd,
                       const Dia::Core::JsonValue& args) override;

    private:
        const RuleSetComponent& mComponent;
        bool mFireLogEnabled = true;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"Rule evaluation — fired rules, guard results, dispatched actions"` (60 chars ≤ 80 ✓)

### JSON State Schema

`GetJSONState()` emits the following structure. This is the normative C++↔panel contract for the Rules card template in `debug-panel.html`.

```json
{
  "drawers": [
    { "name": "FireLog", "enabled": true }
  ],
  "stats": {
    "ruleCount": 6,
    "fired": 2
  },
  "rules": [
    { "id": "on_low_health",       "fired": true,  "actions": ["seek_health", "broadcast_danger"] },
    { "id": "on_enemy_nearby",     "fired": false, "actions": ["enter_combat"]                   },
    { "id": "on_player_visible",   "fired": false, "actions": ["alert_squad"]                    },
    { "id": "on_waypoint_reached", "fired": true,  "actions": ["pick_next_waypoint"]              }
  ]
}
```

`"rules"` is the full rule inventory cross-referenced against the last `Evaluate()` result. Unfired rules are included (with `"fired": false`) so the panel shows which rules exist, not just which fired. `"stats.ruleCount"` is total rules in the set; `"stats.fired"` is how many fired this frame.

Rules with no ID (`kInvalidCRC`) are emitted as `{ "id": "(unnamed)", "fired": ..., "actions": [...] }`.

### Panel Card Specification

Per `debugger-contract.md` AC-16 and `docs/research/visual_debugger_redesign/mockup.html`:

- **Group:** AI / Behavior — accent `DebugGroupAccents::kAIBehavior` (`#ef4444`) via `var(--accent)`
- **Domain stat line:** `"Rules: N"` where N = total rule count
- **Expanded body:** rule table — fired rules in accent color with a filled dot indicator; unfired rules greyed with an empty dot; dispatched actions inline as `→ action1, action2`
- **Drawer toggle:** "FireLog" — hides/shows the rule table; domain card header always visible

## Dependencies on Other Systems

**Required:**
- **DiaRules** — `RuleSetComponent`, `RuleSet`, `RuleFireEntry` (new type), `GetLastFireReport()`, `GetAllRuleIds()`
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`, `DynamicArrayC`

**Explicitly excluded:**
- **ImGui** — retired; no `ImGui::*` calls in this module
- **DiaApplicationFlow**, **DiaEntity**, **DiaCondition**, **DiaAIBudget**, **DiaHTN**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply to this module:

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaRulesVisualDebugger.vcxproj` |
| AC-2 | `DiaRules` has zero `#include` or link dep on `DiaRulesVisualDebugger` |
| AC-3 | Deps: `DiaRules` + `DiaVisualDebugger` + `DiaCore` |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 60 chars ✓ |
| AC-6 | No world drawers → palette rule N/A |
| AC-7 | No world drawers → scale rule N/A |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const RuleSetComponent&` — read-only, no shared mutable state |
| AC-10 | `GetJSONState()` emits `{ "drawers": [...], "stats": {} }` minimum ✓ |
| AC-11 | `OnCommand("toggle", {drawer: "FireLog"})` handled |
| AC-12 | `OnCommand("setScale", ...)` handled (no-op) |
| AC-13/14 | `Tests/GoogleTests/DiaRulesVisualDebugger/TestRulesVisualDebugger.cpp` |
| AC-15 | Mandatory test shapes: toggle gate, JSON round-trip, OnCommand round-trip |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `GetLastFireReport()` lives in `DiaRules`, not the visual debugger module | The debugger reads data it doesn't produce. Matches the `UtilitySet::GetLastFrameScores()` pattern. | Accepted | Yes |
| SD-002 | Single `RulesVisualDebugger` class replaces `RulesFireDrawer` | The old `RulesFireDrawer` was coupled to ImGui. Under `IDebugDomain` all data flows through `GetJSONState()` to the panel card. | Accepted | Yes |
| SD-003 | Include unfired rules in `GetJSONState()` (not fired-only) | The panel shows the full rule inventory so developers can see which rules exist and which fired this frame. Fired-only hides the "did it even evaluate?" question. Requires `GetAllRuleIds()` prerequisite. | Accepted | Yes |
| SD-004 | Current-frame snapshot only — no ring-buffer history in C++ | History accumulation is a panel concern. `GetJSONState()` emits the last `Evaluate()` result; the panel JS can maintain a frame history if needed. Keeps the C++ domain simple. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, rule IDs, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` used throughout; no `std::vector` return values |
| PD-005 | Platform | x64 only | `DiaRulesVisualDebugger.vcxproj` targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | Compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diarulesvisualdebugger.architecture.module.md` updated to reflect new interface |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::Rules::` namespace |

## Open Design Questions

1. **`RuleFireEntry` location** — the struct is defined in `DiaRules/RuleSet.h` under `#ifdef DIA_DEBUG`. Should it be in a separate `DiaRules/Debug/RuleFireEntry.h` so `DiaRulesVisualDebugger` can include it without pulling in the full `RuleSet` header? Decide at implementation time based on what headers the domain actually needs.

2. **Unnamed rules display** — rules with no ID (`kInvalidCRC`) show as `"(unnamed)"` in the panel table. An alternative is to show the guard expression as a summary string. This requires `ConditionExpr::ToString()` (currently absent). Keep `"(unnamed)"` for v1.

3. **`GetAllRuleIds()` ordering** — the prerequisite spec says "definition order". If `RuleSet` stores rules in a hash table (unordered), definition order may not be recoverable without a separate insertion-order list. Clarify at implementation: either maintain insertion order or accept arbitrary order in the panel (fired rules always highlighted regardless of position).

## Status

**Status:** `Approved`

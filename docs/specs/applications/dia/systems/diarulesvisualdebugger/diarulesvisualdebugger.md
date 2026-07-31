# System Spec: DiaRulesVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaRulesVisualDebugger is a separate debug static library that makes rule evaluation visible in the `DiaVisualDebuggerConsole`. It provides one `IVisualDebugger` implementation:

- **`RulesFireDrawer`** — an ImGui-only inspector showing the last N rule evaluations for a single `RuleSetComponent`: which rules fired, which actions were dispatched, and at which frame

Because `RuleSet` is currently stateless after `Evaluate()`, a prerequisite `GetLastFireReport()` accessor must be added to `RuleSet` under `#ifdef DIA_DEBUG` before this drawer can be implemented. That accessor stores, per `Evaluate()` call, the list of rule IDs that fired and the actions that were dispatched.

The entire module is `#ifdef DIA_DEBUG` guarded. Following `DiaUtilityAIVisualDebugger` (SD-007), `DiaRules` has zero dependency on `DiaVisualDebugger`.

## Responsibilities

- **Prerequisite — add to `DiaRules`**: `RuleSet::GetLastFireReport()` accessor (`#ifdef DIA_DEBUG`), populated during `Evaluate()`; see "Prerequisite" section below
- Provide `RulesFireDrawer` — `IVisualDebugger` with ImGui-only `DrawImGui()`; `Draw()` is a no-op
- `RulesFireDrawer::DrawImGui()` renders: ring buffer of last 32 evaluations, each row showing frame number, rule ID, and comma-separated actions dispatched; empty state message when `Evaluate()` has not been called
- Register layer name `rules.fired` in `DebugLayerNames.h` under a new `Rules` section (priority tier 50+)
- Entire public API guarded by `#ifdef DIA_DEBUG`
- Provide `DiaRulesVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.0-Gameplay`
- Provide `dia.diarulesvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Rule condition evaluation — DiaCondition
- Action dispatch — DiaRules `Evaluate()` (already handles this)
- Cooldown or scoring logic — DiaUtilityAI
- Entity position or world-space drawing — no world-space anchor for rule evaluation
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Prerequisite: `RuleSet::GetLastFireReport()`

This change lives in `DiaRules`, not in `DiaRulesVisualDebugger`. It must be implemented first.

```cpp
// DiaRules/RuleSet.h  (addition, DIA_DEBUG only)
#ifdef DIA_DEBUG
struct RuleFireEntry {
    Dia::Core::StringCRC ruleId;                          // kInvalidCRC if rule has no ID
    Dia::Core::DynamicArrayC<Dia::Core::StringCRC, 8> actions;
};

// Populated after the most recent Evaluate() call.
// Returns count of rules that fired.
int GetLastFireReport(
    Dia::Core::DynamicArrayC<RuleFireEntry, 16>& outEntries) const;
#endif
```

`Evaluate()` clears `mLastFireReport`, appends one `RuleFireEntry` per fired rule (including action IDs), then leaves the report in place until the next call. Storage is in `RuleSet`'s internal pimpl, conditionally compiled.

## Public Interfaces

```cpp
// DiaRulesVisualDebugger/RulesFireDrawer.h
#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Rules { class RuleSetComponent; }

namespace Dia::Rules
{
    // ImGui-only fire history inspector.
    // Draw() is a no-op — rule evaluation has no world-space anchor.
    class RulesFireDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit RulesFireDrawer(const RuleSetComponent& component);

        Dia::Core::StringCRC GetLayerName() const override; // "rules.fired"
        void Draw(Dia::Core::IDebugDraw& draw) override;    // no-op
        void DrawImGui() override;

    private:
        static constexpr int kHistoryDepth = 32;

        struct HistoryEntry {
            unsigned int frame;
            Dia::Core::StringCRC ruleId;
            Dia::Core::DynamicArrayC<Dia::Core::StringCRC, 8> actions;
        };

        const RuleSetComponent& mComponent;
        Dia::Core::DynamicArrayC<HistoryEntry, kHistoryDepth> mHistory;
        unsigned int mCurrentFrame{ 0 };
    };
}
#endif // DIA_DEBUG
```

### Layer name additions to `DebugLayerNames.h`

```cpp
// Rules (priority tier 50+)
inline const Dia::Core::StringCRC kRulesFired { "rules.fired" };

// Stage tag used to register all rules layers (creates "Rules" console tab)
inline const Dia::Core::StringCRC kRulesStageTag { "Rules" };
```

### `RulesFireDrawer::DrawImGui()` behaviour

1. Call `mComponent.GetRuleSet()->GetLastFireReport(entries)` to snapshot the latest evaluation
2. If entries > 0 and `mCurrentFrame` has advanced (i.e. new evaluation occurred), append to `mHistory` ring buffer (oldest entry overwritten when full), increment `mCurrentFrame`
3. If `mHistory` is empty — render `ImGui::TextDisabled("No evaluations (Evaluate() not called yet)")`; return
4. Render `ImGui::BeginTable("rules_fire", 3, BordersOuter | RowBg | ScrollY)` with columns `"Frame"`, `"Rule"`, `"Actions"`
5. Walk `mHistory` newest-first; for each entry render frame number, rule ID (or `"(unnamed)"` if `kInvalidCRC`), and comma-separated action list

### Frame counter advancement

`RulesFireDrawer` does not have access to a global frame counter. Instead, it compares the previous `GetLastFireReport()` snapshot against the current one to detect a new evaluation: if the count or any entry differs, a new evaluation has occurred. `mCurrentFrame` is incremented locally as a display-only counter.

## Dependencies on Other Systems

**Required:**
- **DiaRules** — `RuleSetComponent`, `RuleSet`, `RuleFireEntry` (new type), `GetLastFireReport()`
- **DiaCore** — `IVisualDebugger`, `IDebugDraw`, `StringCRC`, `DynamicArrayC`

**Explicitly excluded:**
- **DiaVisualDebugger** — no direct dependency; `DiaRules` must remain free of it
- **DiaApplicationFlow**, **DiaEntity**, **DiaCondition**, **DiaAIBudget**, **DiaHTN**

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `GetLastFireReport()` lives in `DiaRules`, not the visual debugger module | The debugger reads data it doesn't produce. Keeping the report inside `RuleSet` follows `UtilitySet::GetLastFrameScores()` exactly. `DiaRules` already conditionally compiles debug state; one more struct is minimal. | Accepted | Yes |
| SD-002 | Ring buffer history of 32 evaluations in the drawer, not in `RuleSet` | `RuleSet` is stateless — only one evaluation's result is kept. History accumulation belongs in the drawer, which has display context. 32 entries is enough for frame-by-frame inspection. | Accepted | Yes |
| SD-003 | `Draw()` is a no-op — no world-space anchor for rules | Rule evaluation happens against an abstract context; there is no canonical entity position to anchor a world-space label. An entity label (if wanted) belongs in `DiaHTNVisualDebugger` or `DiaEntityVisualDebugger`. | Accepted | Yes |
| SD-004 | Frame advancement via snapshot comparison, not a frame counter dependency | Avoids a dependency on any clock or frame service. Two identical consecutive reports are treated as "no change" — safe because rule evaluation is deterministic for the same world state. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Layer names and rule/action IDs are `StringCRC` |
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC` used throughout |
| PD-005 | Platform | x64 only | `DiaRulesVisualDebugger.vcxproj` targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | Compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain config | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | Provide `dia.diarulesvisualdebugger.architecture.module.md` |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::Rules::` namespace |

## Open Design Questions

1. **`RuleFireEntry` location** — the struct is defined in `DiaRules/RuleSet.h` under `#ifdef DIA_DEBUG`. Should it be in a separate `DiaRules/Debug/RuleFireEntry.h` so `DiaRulesVisualDebugger` can include it without pulling in the full `RuleSet` header? Decide at implementation time based on what headers the drawer actually needs.

2. **Unnamed rules display** — rules with no ID (`kInvalidCRC`) show as `"(unnamed)"` in the table. An alternative is to show the guard expression as a summary string. This requires `ConditionExpr` to have a `ToString()` method (it currently doesn't). Keep `"(unnamed)"` for v1; raise a DiaCondition improvement if the unnamed-rule case proves confusing in practice.

## Status

**Status:** `Approved`

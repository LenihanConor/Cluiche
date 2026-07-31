# System Spec: DiaHTNVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaHTNVisualDebugger is a separate debug static library that makes HTN plan state visible in the `DiaVisualDebuggerConsole`. It provides two `IVisualDebugger` implementations:

- **`HTNPlanDrawer`** — an ImGui-only inspector that shows the active plan as a numbered task list, highlights the current task, shows the total remaining task count, and flags diverged plans with a warning
- **`HTNWorldStateDrawer`** — a world-space text label rendered above a 2D entity position showing the current operator name and remaining task count

Both drawers read exclusively from `HTNPlannerComponent`'s public read API (`GetActivePlan()`, `HasActivePlan()`, `HasDiverged()`) — no write access, no private friendship.

The entire module is `#ifdef DIA_DEBUG` guarded. Following `DiaUtilityAIVisualDebugger` (SD-007), `DiaHTN` has zero dependency on `DiaVisualDebugger`.

## Responsibilities

- Provide `HTNPlanDrawer` — `IVisualDebugger` with ImGui-only `DrawImGui()`; `Draw()` is a no-op
- Provide `HTNWorldStateDrawer` — `IVisualDebugger` with a world-space text label in `Draw()`; `DrawImGui()` is a no-op
- `HTNPlanDrawer::DrawImGui()` renders: numbered operator list, current task highlighted, remaining task count, diverged warning banner
- `HTNWorldStateDrawer::Draw()` renders: `RequestDrawText()` at entity world position — `"<operator> [N]"` format; diverged appended as `" [DIVERGED]"`
- Register layer names `htn.plan` and `htn.world_state` in `DebugLayerNames.h` under a new `HTN` section (priority tier 50+)
- Entire public API guarded by `#ifdef DIA_DEBUG`
- Provide `DiaHTNVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.0-Gameplay`
- Provide `dia.diahtnvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- HTN plan execution — `HTNPlannerComponent` / caller
- World state writing or snapshot management — DiaHTN + DiaCondition
- Entity position tracking — caller provides `Dia::Maths::Vector2` at construction
- Automatic registration into `DebugLayerManager` — caller registers
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Public Interfaces

```cpp
// DiaHTNVisualDebugger/HTNPlanDrawer.h
#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia::HTN { class HTNPlannerComponent; }

namespace Dia::HTN
{
    // ImGui-only inspector for HTN plan state.
    // Draw() is a no-op — plan state has no world-space anchor.
    class HTNPlanDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        explicit HTNPlanDrawer(const HTNPlannerComponent& component);

        Dia::Core::StringCRC GetLayerName() const override; // "htn.plan"
        void Draw(Dia::Core::IDebugDraw& draw) override;    // no-op
        void DrawImGui() override;

    private:
        const HTNPlannerComponent& mComponent;
    };
}
#endif // DIA_DEBUG
```

```cpp
// DiaHTNVisualDebugger/HTNWorldStateDrawer.h
#ifdef DIA_DEBUG

#include <DiaCore/DebugDraw/IVisualDebugger.h>
#include <DiaMaths/Core/Vector2.h>

namespace Dia::HTN { class HTNPlannerComponent; }

namespace Dia::HTN
{
    // World-space text label: "<current operator> [N remaining]"
    // Caller provides the entity's world position (updated each frame by reference).
    class HTNWorldStateDrawer : public Dia::Debug::IVisualDebugger
    {
    public:
        HTNWorldStateDrawer(const HTNPlannerComponent& component,
                            const Dia::Maths::Vector2& worldPosition);

        Dia::Core::StringCRC GetLayerName() const override; // "htn.world_state"
        void Draw(Dia::Core::IDebugDraw& draw) override;
        void DrawImGui() override;                          // no-op

    private:
        const HTNPlannerComponent& mComponent;
        const Dia::Maths::Vector2& mWorldPosition;
    };
}
#endif // DIA_DEBUG
```

### Layer name additions to `DebugLayerNames.h`

```cpp
// HTN (priority tier 50+)
inline const Dia::Core::StringCRC kHTNPlan       { "htn.plan"        };
inline const Dia::Core::StringCRC kHTNWorldState { "htn.world_state" };

// Stage tag used to register all HTN layers (creates "HTN" console tab)
inline const Dia::Core::StringCRC kHTNStageTag   { "HTN" };
```

### `HTNPlanDrawer::DrawImGui()` behaviour

1. If `!HasActivePlan()` — render `ImGui::TextDisabled("No active plan")`; return
2. If `HasDiverged()` — render `ImGui::TextColored(ImVec4(1,0.5,0,1), "⚠ Plan diverged — re-plan needed")`
3. Retrieve `GetActivePlan()` reference
4. Render header: `"Tasks: N remaining"` (where N = `plan.GetTaskCount() - cursor`)
5. Render `ImGui::BeginTable("htn_plan", 2, BordersOuter | RowBg)` with columns `"#"` and `"Operator"`
6. Walk all tasks; highlight current task row with `ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ...)`

### `HTNWorldStateDrawer::Draw()` behaviour

1. If `!HasActivePlan()` — no draw
2. Build label: `"<currentOperator> [<remaining>]"` — append `" [DIVERGED]"` if `HasDiverged()`
3. Call `draw.RequestDrawText(mWorldPosition, label, colour)`

## Dependencies on Other Systems

**Required:**
- **DiaHTN** — `HTNPlannerComponent`, `HTNPlan`, `HTNTask`
- **DiaCore** — `IVisualDebugger`, `IDebugDraw`, `StringCRC`
- **DiaMaths** — `Vector2` (world position for `HTNWorldStateDrawer`)

**Explicitly excluded:**
- **DiaVisualDebugger** — no direct dependency; `DiaHTN` must remain free of it
- **DiaApplicationFlow**, **DiaEntity**, **DiaCondition**, **DiaAIBudget**

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | Two separate drawers rather than one combined class | Each drawer can be enabled/disabled independently in the console. Matches the multi-drawer pattern in `DiaEntityVisualDebugger`. | Accepted | Yes |
| SD-002 | `HTNWorldStateDrawer` takes `const Vector2&` (reference, not value) | Entity positions change each frame; binding by reference means the drawer always reads current position without needing an update API. Caller must guarantee lifetime. | Accepted | Yes |
| SD-003 | No `HTNPlanDrawer` entity selector — one drawer per component instance | Multi-entity view is a `DiaAIInspector` (editor) concern. The visual debugger serves the per-entity debugging use case where the game code knows which entity to inspect. | Accepted | Yes |
| SD-004 | `Draw()` is a no-op on `HTNPlanDrawer`; `DrawImGui()` is a no-op on `HTNWorldStateDrawer` | Separation of concerns: the caller decides which combination to register. No forced coupling between ImGui and world-space output. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Layer names are `StringCRC` constants in `DebugLayerNames.h` |
| PD-004 | Platform | No STL containers in public APIs | All public methods use DiaCore containers or primitives |
| PD-005 | Platform | x64 only | `DiaHTNVisualDebugger.vcxproj` targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` created and maintained |
| PD-007 | Platform | C++20 | Compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain config | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | Provide `dia.diahtnvisualdebugger.architecture.module.md` |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::HTN::` namespace |

## Open Design Questions

1. **`HTNPlanDrawer` task count display** — `HTNPlan` exposes `GetTaskCount()` (total) and `IsComplete()`. There is no public `GetCurrentTaskIndex()`. Should remaining count be derived as `plan.GetTaskCount() - currentIndex` (requires new accessor), or should the drawer simply show total count with the current task highlighted, leaving the remaining count implicit? Resolve when implementing — add a minimal accessor to `HTNPlan` if needed rather than exposing the full cursor state.

2. **World-space text colour scheme** — normal plan label, diverged label, and no-plan state each need distinct colours. Should these match the entity inspector accent colours (defined in `DiaCore/DebugDraw/DebugColours.h` if it exists), or are they drawer-local constants? Check existing drawers for the pattern before hardcoding.

## Status

**Status:** `Approved`

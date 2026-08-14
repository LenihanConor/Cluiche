# System Spec: DiaAIBudgetVisualDebugger

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai

## Purpose

DiaAIBudgetVisualDebugger is the `IDebugDomain` implementation that makes AI budget scheduling visible in `DiaDebugPanel`. It is a panel-only domain (`HasWorldDrawers() = false`).

The domain card shows: a budget fill bar (`usedMs / budgetMs`), a systems-run/deferred count, and a per-system timing table. Per-system timing requires a small extension to `AIBudgetResult` — see Prerequisite.

Following the `DiaXxxVisualDebugger` contract, `DiaAIBudget` has zero compile-time dependency on `DiaAIBudgetVisualDebugger`.

## Responsibilities

- **Prerequisite — extend `AIBudgetResult`**: add per-system timing array (`#ifdef DIA_DEBUG`) — see Prerequisite section
- **Prerequisite — expose `budgetMs`**: `AIBudgetScheduler` must store and expose the budget value passed to `Update()` for the fill-bar denominator
- Implement `IDebugDomain` — all 16-AC contract methods
- `HasWorldDrawers()` returns `false`; `Register`/`Unregister` are no-ops
- Two logical drawers: `BudgetBar`, `SystemTimings`
- `GetJSONState()` emits: budget fill stats, systems run/deferred, per-system timing entries
- `OnCommand("toggle", {drawer: name})` enables/disables the named section
- `OnCommand("setScale", ...)` — no-op
- Entire module guarded by `#ifdef DIA_DEBUG`
- `DiaAIBudgetVisualDebugger.vcxproj` static library registered in `Cluiche.sln` under `3.1-Gameplay-Tools`
- `dia.diaaibudgetvisualdebugger.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Budget scheduling — DiaAIBudget
- Per-system AI logic — individual `IAIBudgetedSystem` implementations
- Any runtime behaviour in Release — entire module excluded by `#ifdef DIA_DEBUG`

## Prerequisite: `AIBudgetResult` and `AIBudgetScheduler` Extensions

Changes live in `DiaAIBudget`. Both must be done before building the debugger.

```cpp
// DiaAIBudget/AIBudgetResult.h (additions, DIA_DEBUG only)
#ifdef DIA_DEBUG
struct SystemTimingEntry {
    Dia::Core::StringCRC systemId;
    float                timeMs;  // wall-clock time this system consumed this tick
    bool                 ran;     // false = deferred (received zero budget)
};
#endif

struct AIBudgetResult {
    int   systemsRun;
    int   systemsDeferred;
    float usedMs;
#ifdef DIA_DEBUG
    Dia::Core::Containers::DynamicArrayC<SystemTimingEntry, kMaxSystems> perSystem;
#endif
};

// DiaAIBudget/AIBudgetScheduler.h (addition)
float GetLastBudgetMs() const;   // returns the budgetMs passed to the most recent Update()
```

`AIBudgetScheduler::Update()` populates `perSystem` in DIA_DEBUG builds by timing each system's `Tick()` call with a high-resolution clock and recording its ID and run/deferred status.

## Public Interfaces

```cpp
// DiaAIBudgetVisualDebugger/AIBudgetVisualDebugger.h
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>

namespace Dia::AIBudget {
    class AIBudgetScheduler;
    struct AIBudgetResult;
}

namespace Dia::AIBudget
{
    class AIBudgetVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        // scheduler is the live scheduler instance; caller updates result each frame.
        // result reference must stay valid for the lifetime of this object.
        AIBudgetVisualDebugger(const AIBudgetScheduler& scheduler,
                               const AIBudgetResult&    result);

        Dia::Core::StringCRC  GetDomainId()      const override; // "aibudget"
        const char*           GetDisplayName()   const override; // "AI Budget"
        const char*           GetDescription()   const override; // see below
        Dia::Core::StringCRC  GetGroup()          const override; // "AIBehavior"
        Dia::Core::ColourRGBA GetAccentColour()   const override; // DebugGroupAccents::kAIBehavior

        bool HasWorldDrawers() const override { return false; }

        void GetJSONState(Dia::Core::JsonWriter& writer) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Dia::Core::JsonValue& args) override;

    private:
        const AIBudgetScheduler& mScheduler;
        const AIBudgetResult&    mResult;
        bool mBudgetBarEnabled    = true;
        bool mSystemTimingsEnabled = true;
    };
}
#endif // DIA_DEBUG
```

`GetDescription()` returns: `"AI budget — scheduler fill bar, per-system run/deferred timings"` (64 chars ≤ 80 ✓)

### JSON State Schema

```json
{
  "drawers": [
    { "name": "BudgetBar",     "enabled": true },
    { "name": "SystemTimings", "enabled": true }
  ],
  "stats": {
    "usedMs":         2.1,
    "budgetMs":       5.0,
    "fillPct":        42,
    "systemsRun":     3,
    "systemsDeferred":1,
    "registeredCount":4
  },
  "systems": [
    { "id": "UtilityAI",    "timeMs": 0.8,  "ran": true  },
    { "id": "HTNPlanner",   "timeMs": 0.9,  "ran": true  },
    { "id": "Pathfinding",  "timeMs": 0.4,  "ran": true  },
    { "id": "StateMachine", "timeMs": 0.0,  "ran": false }
  ]
}
```

`fillPct` = `round(usedMs / budgetMs × 100)`, clamped 0–100. `"systems"` is empty when `perSystem` array has no entries (e.g. if DIA_DEBUG was stripped from `AIBudgetResult`). `registeredCount` = `AIBudgetScheduler::GetRegisteredCount()`.

### Panel Card Specification

- **Group:** AI / Behavior — accent `DebugGroupAccents::kAIBehavior` (`#ef4444`) via `var(--accent)`
- **Domain stat line:** `"Budget: X/Y ms (Z%)"` where X = usedMs, Y = budgetMs, Z = fillPct
- **BudgetBar section:** horizontal fill bar; green < 60%, amber 60–85%, red > 85%; `systemsRun` + `systemsDeferred` count line below
- **SystemTimings section:** per-system table; deferred rows greyed; time bar proportional to `budgetMs`
- **Drawer toggles:** BudgetBar, SystemTimings

## Tests

`Tests/GoogleTests/DiaAIBudgetVisualDebugger/TestAIBudgetVisualDebugger.cpp`

### Mandatory shapes (AC-15)

| Shape | Description |
|-------|-------------|
| DrawerGate | Disabling `BudgetBar` removes `stats.fillPct` from JSON |
| PrimitiveType | `HasWorldDrawers()` returns `false`; `GetDrawerCount()` returns 0 |
| ScaleSensitivity | N/A — `OnCommand("setScale", ...)` is no-op, does not crash |
| JSONRoundTrip | `GetJSONState()` drawer entries match current enabled state |
| OnCommandRoundTrip | `OnCommand("toggle", {drawer:"BudgetBar"})` toggles; second call reverts |

### Domain-specific shapes

| Shape | Description |
|-------|-------------|
| `Stats_UsedMs_MatchesResult` | `stats.usedMs` matches `AIBudgetResult.usedMs` |
| `Stats_BudgetMs_MatchesScheduler` | `stats.budgetMs` matches `AIBudgetScheduler::GetLastBudgetMs()` |
| `Stats_FillPct_Computed` | `fillPct == round(usedMs / budgetMs × 100)` for various values |
| `Stats_FillPct_ClampedAt100` | `usedMs > budgetMs` clamps `fillPct` to 100 |
| `Stats_SystemsRun_Matches` | `stats.systemsRun` matches `AIBudgetResult.systemsRun` |
| `Stats_SystemsDeferred_Matches` | `stats.systemsDeferred` matches `AIBudgetResult.systemsDeferred` |
| `Systems_CountMatchesPerSystem` | `systems` array length == `AIBudgetResult.perSystem.GetCount()` |
| `Systems_RanFlag_Accurate` | Each entry `ran` flag matches `SystemTimingEntry.ran` |
| `Systems_TimeMs_Accurate` | Each entry `timeMs` matches `SystemTimingEntry.timeMs` |
| `DrawerGate_SystemTimings` | Disabling `SystemTimings` removes `systems` key from JSON |
| `Toggle_SystemTimings` | `OnCommand("toggle", {drawer:"SystemTimings"})` toggles it |
| `NoSystems_EmptyArray_NoAssert` | Empty `perSystem` → `systems` is empty array, no crash |
| `BudgetZero_NoAssert` | `budgetMs == 0` does not cause divide-by-zero; `fillPct` = 0 |

## Dependencies on Other Systems

**Required:**
- **DiaAIBudget** — `AIBudgetScheduler`, `AIBudgetResult`, `SystemTimingEntry` (prereq), `GetLastBudgetMs()` (prereq)
- **DiaVisualDebugger** — `IDebugDomain`, `DebugGroupAccents`
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`, `DynamicArrayC`

**Explicitly excluded:**
- **ImGui**, **DiaEntity**, **DiaApplicationFlow**, **DiaHTN**, **DiaSteering**

## Contract Compliance

All 16 ACs from `debugger-contract.md` apply.

| AC | Notes |
|----|-------|
| AC-1 | Standalone `DiaAIBudgetVisualDebugger.vcxproj` |
| AC-2 | `DiaAIBudget` has zero dep on `DiaAIBudgetVisualDebugger` |
| AC-3 | Deps: DiaAIBudget + DiaVisualDebugger + DiaCore only |
| AC-4 | Implements all `IDebugDomain` pure virtuals |
| AC-5 | `GetDescription()` = 64 chars ✓ |
| AC-6 | No world drawers — palette rule N/A |
| AC-7 | No world drawers — scale rule N/A |
| AC-8 | No `ImGui::*` calls |
| AC-9 | `const AIBudgetScheduler&` + `const AIBudgetResult&` — read-only |
| AC-10 | Emits `drawers` + `stats` minimum |
| AC-11 | `OnCommand("toggle", {drawer})` handled for both drawers |
| AC-12 | `OnCommand("setScale", ...)` is no-op; drawer enables use `std::atomic<bool>` |
| AC-13/14 | `Tests/GoogleTests/DiaAIBudgetVisualDebugger/TestAIBudgetVisualDebugger.cpp` |
| AC-15 | All 5 mandatory shapes present — see Tests section |
| AC-16 | Panel card complies with standard spacing; accent via `var(--accent)` |

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | `perSystem` added to `AIBudgetResult` under `DIA_DEBUG` | Keeps Release struct size unchanged. Debug timing is only useful with the visual debugger active. | Accepted | Yes |
| SD-002 | `GetLastBudgetMs()` added as non-debug accessor | The budget target is a runtime value needed by callers beyond the debugger (e.g. tests). Not DIA_DEBUG-gated. | Accepted | Yes |
| SD-003 | Debugger takes `const AIBudgetResult&` — caller owns update cadence | The caller updates `mResult` each frame after `AIBudgetScheduler::Update()`. Debugger reads it on demand. No polling inside the debugger. | Accepted | Yes |
| SD-004 | Fill bar colour tiers (green/amber/red) defined in panel JS, not C++ | Colour thresholds are presentation logic; keeping them in the HTML template avoids recompiling C++ for tuning. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Domain ID, group, drawer names, command keys use `StringCRC` |
| PD-004 | Platform | No STL in public APIs | `DynamicArrayC` used; no `std::vector` |
| PD-005 | Platform | x64 only | vcxproj targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | `dia.diaaibudgetvisualdebugger.architecture.module.md` required |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All code in `Dia::AIBudget::` namespace |

## Open Design Questions

1. **`perSystem` timing granularity** — `SystemTimingEntry.timeMs` is wall-clock time per `Tick()` call. If a system's update is split across multiple calls within one budget cycle, the times should be summed. Clarify whether `AIBudgetScheduler::Update()` calls each system exactly once per tick or may call it multiple times.

2. **System ID source** — `SystemTimingEntry.systemId` assumes each `IAIBudgetedSystem` exposes a stable `StringCRC` ID. Confirm `IAIBudgetedSystem` has a `GetId()` or equivalent; if not, add it as part of the prerequisite.

## Status

**Status:** Approved

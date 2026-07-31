# System Spec: DiaAIInspector

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai, debug

## Purpose

DiaAIInspector is a CluicheEditor plugin that gives live, per-entity visibility into the AI decision layer while the game is running. It connects to the running game over WebSocket (via the existing `DebugServer` / `GameConnectionManager` channel) and displays four tabs — Budget, UtilityAI, Rules, and HTN — each backed by a dedicated `IInspectorDataSource` on the game side.

The plugin follows the exact pattern of `DiaBlackboardInspector` and `DiaEntityInspector`: a `LiveConnectionPluginBase` subclass with per-topic controllers, and `IInspectorDataSource` implementations in `CluicheGameBaseline` that push structured JSON payloads over named topics.

**Mockup:** @docs/specs/applications/dia/systems/diaaiinspector/mock_ai_inspector.html

**Dependency chain:**
```
DiaAIInspector (editor plugin) → DiaEditor (LiveConnectionPluginBase, WebUIBridge)
DiaAIInspector → DiaCore (StringCRC, Json)

CluicheGameBaseline sources:
  AIBudgetInspectorSource    → DiaAIBudget (AIBudgetModule)
  UtilityAIInspectorSource   → DiaUtilityAI (UtilitySetComponent, UtilitySet::GetLastFrameScores)
  RulesInspectorSource       → DiaRules (RuleSetComponent, RuleSet::GetLastFireReport — DIA_DEBUG)
  HTNInspectorSource         → DiaHTN (HTNPlannerComponent, HTNPlan)
```

## Responsibilities

### Editor plugin (Dia/DiaAIInspector/)

- Provide `DiaAIInspectorPlugin` — `LiveConnectionPluginBase` subclass with four controllers
- Register topic `"ai_budget.state"` → `AIBudgetController`
- Register topic `"utility_ai.state"` → `UtilityAIController`
- Register topic `"rules.state"` → `RulesController`
- Register topic `"htn.state"` → `HTNController`
- Forward each incoming payload to the browser UI via `WebUIBridge::NotifyUIDataChanged`
- Provide `DiaAIInspector.vcxproj` registered in `Cluiche.sln` under `4.0-Editor`
- Provide `dia.diaaiinspector.architecture.module.md` YAML module documentation

### Game-side data sources (CluicheGameBaseline/Modules/InspectorSources/)

- `AIBudgetInspectorSource` — `PeriodicSourceBase` (1 Hz), reads `AIBudgetModule::GetScheduler()`, emits budget metrics + 60-frame rolling history
- `UtilityAIInspectorSource` — `ChangeDetectedSourceBase`, iterates `IEntityInspectable` for entities with `UtilitySetComponent`, emits per-entity action scores + winner
- `RulesInspectorSource` — `ChangeDetectedSourceBase` (DIA_DEBUG only), iterates entities with `RuleSetComponent`, emits per-entity last-fire report; requires `RuleSet::GetLastFireReport()` (see `DiaRulesVisualDebugger` prerequisite)
- `HTNInspectorSource` — `ChangeDetectedSourceBase`, iterates entities with `HTNPlannerComponent`, emits per-entity active plan + diverged flag

## Non-Responsibilities

- In-game visual debugger overlays — `DiaHTNVisualDebugger`, `DiaRulesVisualDebugger`
- Rule condition evaluation logic — DiaCondition / DiaRules
- Plan execution — DiaHTN / caller
- Budget scheduling — DiaAIBudget
- Live field editing of AI component data — deferred (read-only inspector in v1, consistent with SD-ENT-016 Tier (c) structural edit disabled)

## Public Interfaces

### Plugin (editor side)

```cpp
// Dia/DiaAIInspector/DiaAIInspectorPlugin.h
#pragma once
#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>
#include "DiaAIInspector/Controllers/AIBudgetController.h"
#include "DiaAIInspector/Controllers/UtilityAIController.h"
#include "DiaAIInspector/Controllers/RulesController.h"
#include "DiaAIInspector/Controllers/HTNController.h"

namespace Dia::AIInspector {

class DiaAIInspectorPlugin final : public Dia::Editor::LiveConnectionPluginBase
{
public:
    DiaAIInspectorPlugin();

protected:
    void OnLivePluginLoad()   override;
    void OnLivePluginUnload() override;
    void OnGameConnected()    override;
    void OnGameDisconnected() override;

private:
    AIBudgetController   mBudgetController;
    UtilityAIController  mUtilityAIController;
    RulesController      mRulesController;
    HTNController        mHTNController;
};

} // namespace Dia::AIInspector
```

### Controllers (editor side)

Each controller is a thin forwarder — receives a `Json::Value` payload, calls `GetBridge()->NotifyUIDataChanged(uiTopic, payload)`.

```cpp
// DiaAIInspector/Controllers/AIBudgetController.h
class AIBudgetController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload);  // forwards to "ai_inspector.budget"
};

// DiaAIInspector/Controllers/UtilityAIController.h
class UtilityAIController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload);  // forwards to "ai_inspector.utility_ai"
};

// DiaAIInspector/Controllers/RulesController.h
class RulesController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload);  // forwards to "ai_inspector.rules"
};

// DiaAIInspector/Controllers/HTNController.h
class HTNController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload);  // forwards to "ai_inspector.htn"
};
```

### Game-side data sources

```cpp
// CluicheGameBaseline/Modules/InspectorSources/AIBudgetInspectorSource.h
class AIBudgetInspectorSource : public Dia::DebugServer::PeriodicSourceBase {
public:
    explicit AIBudgetInspectorSource(const Dia::AIBudget::AIBudgetModule& module);
    Dia::Core::StringCRC           GetTopic()  const override; // "ai_budget.state"
    Dia::DebugServer::SourcePolicy GetPolicy() const override; // kPeriodic, 1.0f Hz
protected:
    void        AccumulateSample(float deltaTime) override;
    Json::Value BuildPayload()                    override;
    void        Reset()                           override;
private:
    const Dia::AIBudget::AIBudgetModule& mModule;
    // rolling 60-frame ring buffer of { used_us, systems_run, systems_deferred }
};

// CluicheGameBaseline/Modules/InspectorSources/UtilityAIInspectorSource.h
class UtilityAIInspectorSource : public Dia::DebugServer::ChangeDetectedSourceBase {
public:
    UtilityAIInspectorSource(const Dia::Entity::IEntityInspectable& domain);
    Dia::Core::StringCRC           GetTopic()  const override; // "utility_ai.state"
    Dia::DebugServer::SourcePolicy GetPolicy() const override; // kChangeDetected
protected:
    unsigned int CollectAndHash(Json::Value& payload) override;
private:
    const Dia::Entity::IEntityInspectable& mDomain;
};

// CluicheGameBaseline/Modules/InspectorSources/RulesInspectorSource.h
// NOTE: GetLastFireReport() is DIA_DEBUG only; this source compiles but sends empty
//       data in Release. Wrap CollectAndHash body in #ifdef DIA_DEBUG.
class RulesInspectorSource : public Dia::DebugServer::ChangeDetectedSourceBase {
public:
    RulesInspectorSource(const Dia::Entity::IEntityInspectable& domain);
    Dia::Core::StringCRC           GetTopic()  const override; // "rules.state"
    Dia::DebugServer::SourcePolicy GetPolicy() const override; // kChangeDetected
protected:
    unsigned int CollectAndHash(Json::Value& payload) override;
private:
    const Dia::Entity::IEntityInspectable& mDomain;
};

// CluicheGameBaseline/Modules/InspectorSources/HTNInspectorSource.h
class HTNInspectorSource : public Dia::DebugServer::ChangeDetectedSourceBase {
public:
    HTNInspectorSource(const Dia::Entity::IEntityInspectable& domain);
    Dia::Core::StringCRC           GetTopic()  const override; // "htn.state"
    Dia::DebugServer::SourcePolicy GetPolicy() const override; // kChangeDetected
protected:
    unsigned int CollectAndHash(Json::Value& payload) override;
private:
    static constexpr int kPlanHistoryDepth = 8;

    // Detect plan replacement by hashing the operator sequence of the current plan.
    // On mismatch: archive old entry (set endFrame), push new entry. Reason inferred:
    //   "initial" if no previous plan existed, "diverged" if HasDiverged() was true
    //   on the prior tick, "replan" otherwise.
    // STL allowed in .cpp internals (PD-004).
    const Dia::Entity::IEntityInspectable& mDomain;
    // Per-entity ring buffers stored in .cpp as std::unordered_map<uint32_t, PlanHistoryRing>
};
```

### JSON payload schemas

**`ai_budget.state`**
```json
{
  "frame": 4821,
  "budget_us": 1000,
  "used_us": 312,
  "systems_run": 4,
  "systems_deferred": 1,
  "history": [
    { "frame": 4821, "used_us": 312, "systems_run": 4, "systems_deferred": 1 }
  ]
}
```

**`utility_ai.state`**
```json
{
  "frame": 4821,
  "entities": [
    {
      "id": 7,
      "name": "Soldier_3",
      "winner": "Attack",
      "actions": [
        { "id": "Attack",  "score": 0.82, "eligible": true,  "cooldown_remaining": 0.0 },
        { "id": "Flee",    "score": 0.41, "eligible": true,  "cooldown_remaining": 0.0 },
        { "id": "Patrol",  "score": 0.0,  "eligible": false, "cooldown_remaining": 0.3 }
      ]
    }
  ]
}
```

**`rules.state`** (DIA_DEBUG only; empty entities array in Release)
```json
{
  "frame": 4821,
  "entities": [
    {
      "id": 7,
      "name": "Soldier_3",
      "rules_fired": 2,
      "rules": [
        { "id": "AttackRule", "fired": true,  "actions": ["FireArrow", "FaceTarget"] },
        { "id": "FlankRule",  "fired": true,  "actions": ["MoveToFlank"]             },
        { "id": "IdleRule",   "fired": false, "actions": []                          }
      ]
    }
  ]
}
```

**`htn.state`**
```json
{
  "frame": 4821,
  "entities": [
    {
      "id": 7,
      "name": "Soldier_3",
      "has_plan": true,
      "diverged": true,
      "current_task": "AttackTarget",
      "current_task_index": 3,
      "task_count": 7,
      "tasks": [
        { "index": 0, "operator": "MoveToPosition", "params": ["staging_point_A"], "done": true  },
        { "index": 1, "operator": "AssembleUnit",    "params": ["archer"],          "done": true  },
        { "index": 2, "operator": "AssembleUnit",    "params": ["swordsman"],       "done": true  },
        { "index": 3, "operator": "AttackTarget",    "params": ["enemy_base_1"],    "done": false },
        { "index": 4, "operator": "HoldPosition",    "params": ["enemy_base_1"],    "done": false },
        { "index": 5, "operator": "Reinforce",       "params": ["flank_route_B"],   "done": false },
        { "index": 6, "operator": "ReportStatus",    "params": [],                  "done": false }
      ],
      "plan_history": [
        {
          "plan_index": 0,
          "start_frame": 4612,
          "end_frame": 4740,
          "reason": "initial",
          "task_count": 4,
          "tasks": ["Scout", "MoveToStaging", "AssembleUnit", "AttackTarget"]
        },
        {
          "plan_index": 1,
          "start_frame": 4741,
          "end_frame": 4820,
          "reason": "diverged",
          "task_count": 7,
          "tasks": ["MoveToPosition", "AssembleUnit", "AssembleUnit", "AttackTarget", "HoldPosition", "Reinforce", "ReportStatus"]
        }
      ]
    }
  ]
}
```

## Tab Designs

### Budget tab (entity-agnostic)

- 60-frame sparkline: `used_us` vs `budget_us` ceiling
- Stat tiles: budget limit, used this frame (µs + %), systems run, systems deferred
- Updates at 1 Hz via `PeriodicSourceBase`

### UtilityAI tab (entity selector)

- Entity selector: all entities with `UtilitySetComponent`
- Horizontal bar chart: one bar per action, 0–1, winner accent-coloured
- Cooldown badge under bar when `cooldown_remaining > 0`
- Ineligible actions shown greyed-out (failed prerequisite or cooldown)
- **Winner history** (client-side): compact timeline below the bar chart showing the last N winner transitions with frame number and score. Accumulated in the browser while the inspector is open; resets on reconnect. Entries are appended whenever `winner` changes between payloads.

### Rules tab (entity selector)

- Entity selector: all entities with `RuleSetComponent`
- Table: Rule ID | Fired | Actions dispatched (current frame)
- Fired rows green-highlighted; skipped rows dimmed
- Release-build notice when `DIA_DEBUG` not defined: "Rules fire data unavailable in Release"
- **Fire rate history** (client-side): secondary table below showing, for each rule, the fire count and fire-rate % across all frames received since the inspector opened. Accumulated in the browser; resets on reconnect. Useful for spotting rules that fire every frame vs never.

### HTN tab (entity selector)

- Entity selector: all entities with `HTNPlannerComponent`
- Diverged warning banner when `diverged == true`
- Task list: numbered rows, completed tasks dimmed, current task highlighted
- "No active plan" empty state
- **Plan history** (server-side ring buffer, depth 8): collapsible list of past plans below the current plan, each showing plan index, start/end frame, reason for replacement (`initial` / `diverged` / `replan`), and the operator sequence as a compact tag list. Server-side so history is preserved before the inspector connects.

## Dependencies on Other Systems

**Editor plugin requires:**
- `DiaEditor` — `LiveConnectionPluginBase`, `WebUIBridge`, `EditorPluginRegistrationMacros`
- `DiaCore` — `StringCRC`, `DynamicArrayC`

**Game-side sources require:**
- `DiaAIBudget` — `AIBudgetModule`, `AIBudgetResult`
- `DiaUtilityAI` — `UtilitySetComponent`, `UtilitySet::GetLastFrameScores()` (DIA_DEBUG)
- `DiaRules` — `RuleSetComponent`, `RuleSet::GetLastFireReport()` (DIA_DEBUG)
- `DiaHTN` — `HTNPlannerComponent`, `HTNPlan`
- `DiaEntity` — `IEntityInspectable` (entity enumeration)
- `DiaDebugServer` — `ChangeDetectedSourceBase`, `PeriodicSourceBase`, `DebugServer`

**Explicitly excluded:**
- `DiaVisualDebugger` — in-game overlay, separate concern
- `DiaCondition`, `DiaStateMachine` — no inspector surface needed in v1

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SD-001 | Four controllers, one per topic | Matches `DiaEntityInspector` (four controllers). Each tab evolves independently; keeping them separate avoids a god-controller. | Accepted | Yes |
| SD-002 | `UtilityAIInspectorSource` uses `GetLastFrameScores()` (DIA_DEBUG only) | Score data only exists in debug builds. In Release the `utility_ai.state` payload emits entities but with empty action arrays — the UI shows a Release notice. | Accepted | Yes |
| SD-003 | `RulesInspectorSource` wraps `CollectAndHash` body in `#ifdef DIA_DEBUG` | Symmetric with UtilityAI. Avoids a separate Release/Debug source class; one class, conditional body. | Accepted | Yes |
| SD-004 | Read-only inspector in v1 — no live field editing | SD-ENT-016 Tier (c) structural edit disabled in all inspector panels. AI component fields (curve shapes, rule guards) are data-driven from JSON, not runtime-tweakable in v1. | Accepted | Yes |
| SD-005 | `AIBudgetInspectorSource` uses `PeriodicSourceBase` (1 Hz), not change-detected | Budget metrics change every frame; change-detection would broadcast at frame rate. A 1 Hz summary is sufficient for human-readable display and reduces WebSocket traffic. | Accepted | Yes |
| SD-006 | `UtilityAIInspectorSource`, `RulesInspectorSource`, `HTNInspectorSource` use `ChangeDetectedSourceBase` | These are structural: entity list, action/rule/plan content. Hash on entity count + winner/fired-count + task-count covers meaningful changes without serializing every field every frame. | Accepted | Yes |
| SD-007 | Plugin in `Dia/DiaAIInspector/`; sources in `CluicheGameBaseline/` | Plugin is reusable engine code (any game could use it). Sources are game-specific wiring (which entity domain, which module to read from). Same split as `DiaBlackboardInspector` + `BlackboardInspectorSource`. | Accepted | Yes |
| SD-008 | HTN plan history lives server-side (C++ ring buffer, depth 8 per entity) | Plans are generated before the inspector connects; a client-side accumulator would miss all prior replans. 8 plans per entity covers normal debugging sessions without unbounded memory growth. Reason tag (`initial`/`diverged`/`replan`) is inferred from `HasDiverged()` state on the prior tick. | Accepted | Yes |
| SD-009 | UtilityAI winner history and Rules fire-rate history live client-side (browser) | These are derived from the stream of payloads; no historical state is needed before the inspector opens. Client-side accumulation avoids C++ ring buffers for two more sources and resets cleanly on reconnect (expected behaviour — you're watching live activity). | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Topic names and entity IDs use `StringCRC` |
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC` throughout; STL allowed in .cpp internals |
| PD-005 | Platform | x64 only | `DiaAIInspector.vcxproj` targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | Compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain config | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | Provide `dia.diaaiinspector.architecture.module.md` |
| AD-003 | Dia App | `Dia::<Module>::` namespace | Plugin in `Dia::AIInspector::` namespace |
| SD-ENT-016 | DiaEntityInspector | Tier (c) structural edit disabled | No live field editing in v1 |

## Open Design Questions

1. **Entity enumeration for AI components** — `UtilityAIInspectorSource`, `RulesInspectorSource`, and `HTNInspectorSource` all need to iterate entities with a specific component type. `IEntityInspectable` supports entity enumeration and component field serialization, but does it support component-type filtering (e.g. "give me all entities with a `UtilitySetComponent`")? If not, the source must iterate all entities and skip those without the component. Confirm `IEntityInspectable` API surface before implementing; extend minimally if needed.

2. **Score data in Release builds** — `GetLastFrameScores()` and `GetLastFireReport()` are `#ifdef DIA_DEBUG`. The inspector sources compile in Release but emit empty action/rule arrays. The UI should show a "Debug build required for score/fire data" notice rather than silently showing blank tabs. Decide where this notice is best placed: in the JSON payload (a `"debug_only": true` field), or in the browser UI layer.

3. **HTN plan history detail level** — `plan_history` entries carry the full operator list (`tasks` array of strings). For very long plans this could be noisy in the UI. Should the history entries show only the first and last operator as a summary, or the full list? The mock shows the full list as a compact tag row; if plans grow beyond ~10 tasks the tag row wraps unpleasantly. Consider a "show full" expand toggle per history entry at implementation time.

## Status

**Status:** `In Progress`

**Plan:** @docs/specs/applications/dia/systems/diaaiinspector/diaaiinspector.plan.md

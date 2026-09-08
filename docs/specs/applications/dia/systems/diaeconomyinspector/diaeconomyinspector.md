# System Spec: DiaEconomyInspector

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** economy, debug, editor

**Mockup:** @docs/specs/applications/dia/systems/diaeconomyinspector/mock_economy_inspector.html

**Dependency chain:**
```
DiaEconomyInspector (editor plugin) → DiaEditor (LiveConnectionPluginBase, WebUIBridge)
DiaEconomyInspector → DiaCore (StringCRC, Json)

CluicheGameBaseline sources:
  EconomyInstancesSource   → DiaEconomy (EconomySystem, EconomyInstance pools/rates)
  EconomyModifiersSource   → DiaEconomy (active modifier stack per instance)
  EconomyEventsSource      → DiaEconomy (Earn/Spend/Clamped/Transfer observer events)
  EconomySchemaSource      → DiaEconomy (EconomySchema JSON asset — one-shot on connect)
```

## Purpose

DiaEconomyInspector is a CluicheEditor plugin that gives live, per-faction visibility into the economy simulation while the game is running. It answers the core developer questions:

- *"Why did my gold drop?"* — rolling event log (Earn/Spend/Clamped/Transfer) with source tags
- *"Is my income rate healthy?"* — per-resource pool fill bars, gross income vs spend breakdown, and sparklines of pool value over time
- *"What's capping my pool?"* — active modifier stack per instance and per resource
- *"What does building X cost?"* — read-only cost table loaded once from the EconomySchema

The plugin follows the exact pattern of `DiaAIInspector` and `DiaBlackboardInspector`: a `LiveConnectionPluginBase` subclass with per-topic controllers, and `IInspectorDataSource` implementations in `CluicheGameBaseline` that push structured JSON payloads over named topics.

Read-only in v1. No runtime resource injection or modifier editing.

## Responsibilities

### Editor plugin (Dia/DiaEconomyInspector/)

- Provide `DiaEconomyInspectorPlugin` — `LiveConnectionPluginBase` subclass with four controllers
- Register topic `"economy.instances"` → `EconomyInstancesController`
- Register topic `"economy.modifiers"` → `EconomyModifiersController`
- Register topic `"economy.events"` → `EconomyEventsController`
- Register topic `"economy.schema"` → `EconomySchemaController`
- Forward each incoming payload to the browser UI via `WebUIBridge::NotifyUIDataChanged`
- Render a dockable React UI (React + Vite + `@dia/editor-ui`) with four tabs: Resources, Events, Modifiers, Schema
- Provide `DiaEconomyInspector.vcxproj` registered in `Cluiche.sln` under `4.0-Editor`
- Provide `dia.diaeconomyinspector.architecture.module.md` YAML module documentation

### Game-side data sources (CluicheGameBaseline/Modules/InspectorSources/)

- `EconomyInstancesSource` — `ChangeDetectedSourceBase`; iterates all `EconomyInstance` objects via `EconomySystem`; emits per-instance pool values, caps, net income rate, gross income/spend breakdown, and a ring buffer of pool-value history samples taken on each economy tick (not per frame). Server-side ring default depth: 600 ticks (~60 seconds at 10Hz) — covers the pre-connect window. Once connected, the editor accumulates all ticks received with no cap (PC memory is the only bound). Includes derived resource values and their computation sources.
- `EconomyModifiersSource` — `ChangeDetectedSourceBase`; emits active modifier stack per instance per resource — type (multiply_income / multiply_cap / flat_income), value, source tag, and whether any DiaCondition guard is currently satisfied
- `EconomyEventsSource` — push-on-event; subscribes to `IEconomyObserver` on the game side; every Earn/Spend/Clamped/Transfer event is appended to a server-side ring buffer (depth 500). Full ring replayed on initial connect so events that fired before the inspector opened are visible. Once connected, the editor accumulates all subsequent events indefinitely — no client-side cap — so the full session history is browsable for the duration of the connection.
- `EconomySchemaSource` — one-shot on connect; serialises the loaded `EconomySchema` (resource definitions, income rules, cost tables) to JSON and sends once. No further updates unless schema hot-reloads.

### Cap saturation and starvation detection (game-side, part of EconomyInstancesSource)

- Cap saturation: when a pool is at max and Clamped events are firing continuously, compute and emit `capped_duration_s` (seconds pool has been at max with income being wasted). Displayed as a warning badge in the Resources tab.
- Starvation: when a pool has been at 0 for N consecutive economy ticks (default N=3), emit `starved_duration_s`. Displayed as a warning badge in the Resources tab.

## Non-Responsibilities

- Economy simulation logic — DiaEconomy
- Modifier evaluation — DiaCondition / DiaEconomy
- Runtime resource injection or modifier editing — deferred (read-only in v1, per EI-004)
- Editor layout / docking infrastructure — DiaEditor
- WebSocket connection lifecycle — LiveConnectionPluginBase / DiaDebugServer
- Game connection toolbar (connect/disconnect action) — DiaEditor shell

## Public Interfaces

### Plugin (editor side)

```cpp
// Dia/DiaEconomyInspector/DiaEconomyInspectorPlugin.h
#pragma once
#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>
#include "DiaEconomyInspector/Controllers/EconomyInstancesController.h"
#include "DiaEconomyInspector/Controllers/EconomyModifiersController.h"
#include "DiaEconomyInspector/Controllers/EconomyEventsController.h"
#include "DiaEconomyInspector/Controllers/EconomySchemaController.h"

namespace Dia::EconomyInspector {

class DiaEconomyInspectorPlugin final : public Dia::Editor::LiveConnectionPluginBase
{
public:
    DiaEconomyInspectorPlugin();

protected:
    void OnLivePluginLoad()   override;
    void OnLivePluginUnload() override;
    void OnGameConnected()    override;
    void OnGameDisconnected() override;

private:
    EconomyInstancesController  mInstancesController;
    EconomyModifiersController  mModifiersController;
    EconomyEventsController     mEventsController;
    EconomySchemaController     mSchemaController;
};

} // namespace Dia::EconomyInspector

REGISTER_EDITOR_PLUGIN(Dia::EconomyInspector::DiaEconomyInspectorPlugin, "EconomyInspector")
```

### Controllers (editor side)

Each controller is a thin forwarder — receives a `Json::Value` payload, calls `GetBridge()->NotifyUIDataChanged(uiTopic, payload)`.

```cpp
// Controllers/EconomyInstancesController.h
class EconomyInstancesController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload); // forwards to "economy_inspector.instances"
};

// Controllers/EconomyModifiersController.h
class EconomyModifiersController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload); // forwards to "economy_inspector.modifiers"
};

// Controllers/EconomyEventsController.h
class EconomyEventsController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload); // forwards to "economy_inspector.events"
};

// Controllers/EconomySchemaController.h
class EconomySchemaController {
public:
    void Init(Dia::Editor::WebUIBridge* bridge);
    void OnPayload(const Json::Value& payload); // forwards to "economy_inspector.schema"
};
```

### Game-side data sources

```cpp
// CluicheGameBaseline/Modules/InspectorSources/EconomyInstancesSource.h
class EconomyInstancesSource : public Dia::DebugServer::ChangeDetectedSourceBase {
public:
    explicit EconomyInstancesSource(const Dia::Economy::EconomySystem& system, int historyDepth = 600);
    Dia::Core::StringCRC           GetTopic()  const override; // "economy.instances"
    Dia::DebugServer::SourcePolicy GetPolicy() const override; // kChangeDetected
protected:
    unsigned int CollectAndHash(Json::Value& payload) override;
    void         OnEconomyTick();  // called by EconomySystem observer; appends history sample
private:
    const Dia::Economy::EconomySystem& mSystem;
    int mHistoryDepth;
    // Per-instance, per-resource ring buffers stored in .cpp
    // Sample written once per economy tick, not per frame
};

// CluicheGameBaseline/Modules/InspectorSources/EconomyModifiersSource.h
class EconomyModifiersSource : public Dia::DebugServer::ChangeDetectedSourceBase {
public:
    explicit EconomyModifiersSource(const Dia::Economy::EconomySystem& system);
    Dia::Core::StringCRC           GetTopic()  const override; // "economy.modifiers"
    Dia::DebugServer::SourcePolicy GetPolicy() const override; // kChangeDetected
protected:
    unsigned int CollectAndHash(Json::Value& payload) override;
private:
    const Dia::Economy::EconomySystem& mSystem;
};

// CluicheGameBaseline/Modules/InspectorSources/EconomyEventsSource.h
// Subscribes to IEconomyObserver. Buffers events server-side.
// On connect: sends full ring. Subsequent pushes: delta since last send.
class EconomyEventsSource : public Dia::DebugServer::IInspectorDataSource,
                            public Dia::Economy::IEconomyObserver {
public:
    explicit EconomyEventsSource(Dia::Economy::EconomySystem& system, int ringDepth = 500);
    Dia::Core::StringCRC GetTopic() const override; // "economy.events"
    // IEconomyObserver
    void OnEarn(const EconomyEventArgs& args)     override;
    void OnSpend(const EconomyEventArgs& args)    override;
    void OnClamped(const EconomyEventArgs& args)  override;
    void OnTransfer(const EconomyTransferArgs& args) override;
private:
    // Ring buffer of serialised event objects; stored in .cpp
    int mRingDepth;
    Dia::Economy::EconomySystem& mSystem;
};

// CluicheGameBaseline/Modules/InspectorSources/EconomySchemaSource.h
// Sent once on connect; no change detection needed.
class EconomySchemaSource : public Dia::DebugServer::IInspectorDataSource {
public:
    explicit EconomySchemaSource(const Dia::Economy::EconomySchema& schema);
    Dia::Core::StringCRC GetTopic() const override; // "economy.schema"
private:
    const Dia::Economy::EconomySchema& mSchema;
};
```

### JSON payload schemas

**`economy.instances`** (change-detected; emitted when any pool value, rate, or saturation state changes)
```json
{
  "frame": 4821,
  "instances": [
    {
      "id": "PlayerFaction",
      "resources": [
        {
          "name": "Gold",
          "type": "base",
          "current": 847,
          "cap": 1000,
          "net_rate": 12.4,
          "gross_income": 18.0,
          "gross_spend": 5.6,
          "capped_duration_s": 3.2,
          "starved_duration_s": 0.0,
          "history": [820, 831, 839, 847]
        },
        {
          "name": "CombatPower",
          "type": "derived",
          "current": 1270,
          "cap": -1,
          "net_rate": 0.0,
          "gross_income": 0.0,
          "gross_spend": 0.0,
          "capped_duration_s": 0.0,
          "starved_duration_s": 0.0,
          "history": [1200, 1230, 1260, 1270]
        }
      ]
    }
  ]
}
```
*`cap: -1` signals no cap (derived resources). `history` is a flat array of pool values sampled once per economy tick, newest last. Length up to `historyDepth`.*

**`economy.modifiers`** (change-detected)
```json
{
  "frame": 4821,
  "instances": [
    {
      "id": "PlayerFaction",
      "resources": [
        {
          "name": "Gold",
          "modifiers": [
            { "type": "multiply_income", "value": 1.20, "source": "trade_bonus_passive", "condition": null, "active": true },
            { "type": "flat_income",     "value": 5.0,  "source": "mine_upgrade_tier2",  "condition": null, "active": true },
            { "type": "multiply_income", "value": 1.25, "source": "research_economy_1",  "condition": "gold_deficit", "active": false }
          ]
        }
      ]
    }
  ]
}
```
*`condition: null` = unconditional. `active: false` = condition guard is currently unsatisfied.*

**`economy.events`** (push-on-event; initial connect sends full ring, subsequent pushes send delta)
```json
{
  "full_ring": false,
  "events": [
    { "frame": 4821, "type": "Earn",     "instance": "PlayerFaction", "resource": "Gold",   "amount": 18,   "source": "harvest_site_3" },
    { "frame": 4820, "type": "Spend",    "instance": "PlayerFaction", "resource": "Wood",   "amount": -45,  "source": "barracks_construction" },
    { "frame": 4819, "type": "Clamped",  "instance": "PlayerFaction", "resource": "Gold",   "attempted": 18, "actual": 0, "source": "harvest_site_3" },
    { "frame": 4817, "type": "Transfer", "instance": "PlayerFaction", "resource": "Gold",   "amount": -100, "destination": "EnemyFaction", "source": "trade_agreement" }
  ]
}
```

**`economy.schema`** (one-shot on connect)
```json
{
  "resources": [
    { "name": "Gold",         "type": "base",    "base_cap": 1000, "income_rule": "harvest_rate * tile_count" },
    { "name": "CombatPower",  "type": "derived",  "base_cap": -1,  "income_rule": "sum(unit_strength)" }
  ],
  "cost_table": [
    { "action": "Archer Unit",       "Gold": 50,  "Wood": 0,   "Food": 10,  "Supply": 1 },
    { "action": "Barracks",          "Gold": 150, "Wood": 100, "Food": 0,   "Supply": 0 }
  ]
}
```

## Tab Designs

### Resources tab (instance/faction selector)

- **Instance selector**: chip bar listing all active `EconomyInstance` IDs
- Per-resource card layout (one row per resource):
  - Name + type badge (`base` in accent, `derived` in purple)
  - Pool fill bar: `current / cap` (fill bar hidden for derived resources with `cap: -1`)
  - Net income rate: green if positive, red if negative, `—` for derived
  - Gross breakdown below: `↑ {gross_income}/s  ↓ {gross_spend}/s` in dim text (hidden for derived)
  - Inline sparkline SVG of `history` array (pool value over last N economy ticks)
  - Warning badges:
    - `⚠ capped {N}s` (warn colour) when `capped_duration_s > 0`
    - `⚠ starved {N}s` (red) when `starved_duration_s > 0`
- History label: `"Pool history — last {depth} ticks (server-side ring buffer)"` with server pill

### Events tab (instance/faction selector)

- **Instance selector**: chip bar
- Stat tiles: events logged, server ring depth, last event frame
- Event log table: Frame | Type | Resource | Amount | Source
  - Type badges colour-coded: Earn=green, Spend=accent, Clamped=warn (row highlighted), Transfer=purple
  - Transfer rows show destination instance
  - Clamped rows show attempted vs actual amount
- History label: `"Event log — last {ringDepth} events (server-side ring buffer)"` with server pill

### Modifiers tab (instance + resource selector)

- **Instance selector**: chip bar
- **Resource selector**: secondary chip bar (one chip per resource in selected instance)
- Stat tiles: active modifier count, net income multiplier, net cap multiplier
- Modifier stack table: Type | Value | Source | Condition | Active
  - Inactive rows (condition guard unsatisfied) shown at 50% opacity
  - `DIA_CONDITION:` prefix on condition cell
  - multiply_income / multiply_cap / flat_income badge coloring
- Note: `"Conditional modifiers show active/inactive state. Read-only in v1."`

### Schema tab (no instance selector — schema is global)

- Cosmetic search bar (filtering deferred to implementation)
- **Cost table**: Action/Unit | {resource columns…} — `—` for absent costs
- **Resource definitions table**: Name | Type | Base Cap | Income Rule | Notes
  - Derived resources marked with purple type badge, cap shown as `—`
- Loaded once on connect; shows a `"Not connected"` empty state until schema arrives

## Features

| # | Feature | Size | Description | Spec | Status |
|---|---------|------|-------------|------|--------|
| 1 | Resources tab — pool state | M | Instance selector, per-resource pool bar + net rate + gross breakdown | inline | Approved |
| 2 | Resources tab — sparklines | M | Per-resource pool history sparkline (server-side ring, sampled per economy tick) | inline | Approved |
| 3 | Resources tab — saturation/starvation badges | S | Cap saturation + starvation duration badges on resource rows | inline | Approved |
| 4 | Resources tab — derived resource display | S | Derived resources distinguished with purple badge, no fill bar, no income rate | inline | Approved |
| 5 | Events tab — event log | M | Rolling event log (Earn/Spend/Clamped/Transfer), server-side ring depth 500 | inline | Approved |
| 6 | Events tab — sparkline/log linkage | M | Hovering a sparkline data point highlights events in the log around that frame | inline | Approved |
| 7 | Modifiers tab | M | Per-resource modifier stack with type/value/source/condition/active-state | inline | Approved |
| 8 | Schema tab | S | Read-only cost table + resource definitions, loaded once on connect | inline | Approved |
| 9 | Connection lifecycle | S | Disconnected overlay; grey-out panels; auto-reactivate on reconnect | inline | Approved |
| 10 | Game-side data sources | L | Four IInspectorDataSource implementations in CluicheGameBaseline | inline | Approved |

## Dependencies on Other Systems

**Editor plugin requires:**
- `DiaEditor` — `LiveConnectionPluginBase`, `WebUIBridge`, `EditorPluginRegistrationMacros`, `PluginServiceLocator`
- `DiaCore` — `StringCRC`, `DynamicArrayC`, `Json`
- `DiaEditorUI` — React component library (`@dia/editor-ui`), `useBridge` hook, `ConnectionStatus`, `TabBar`

**Game-side sources require:**
- `DiaEconomy` — `EconomySystem`, `EconomyInstance`, `EconomySchema`, `IEconomyObserver`, `EconomyEventArgs`, `EconomyTransferArgs`
- `DiaDebugServer` — `ChangeDetectedSourceBase`, `IInspectorDataSource`, `DebugServer`
- `DiaCondition` — condition name/state surfaced via EconomyModifiersSource (modifier guard resolution)

**Explicitly excluded:**
- `DiaVisualDebugger` — in-game overlay, separate concern
- `DiaEntity` / `IEntityInspectable` — economy is instance-based, not entity-component per se
- `DiaApplicationFlow` — no compile-time dependency on phase system

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| EI-001 | Four controllers, one per topic | Each tab evolves independently; matches DiaAIInspector / DiaBlackboardInspector pattern. Avoids a god-controller. | Accepted | Yes |
| EI-002 | History sampled per economy tick; server ring pre-buffers 600 ticks; editor accumulates unboundedly after connect | Economy pools change on events, not every render frame. Per-tick sampling keeps memory proportional to economy update rate (~10 Hz). The server ring (600 ticks ≈ 60s) covers the pre-connect window. Once connected, the editor (PC) accumulates all ticks with no cap — the full session sparkline is available for the entire play session. | Accepted | Yes |
| EI-003 | EconomyEventsSource: server ring (500 events) for pre-connect history; editor accumulates all events unboundedly after connect | "Why did my gold drop?" requires seeing events that fired before the inspector opened (server ring), and also full session browsing after connect (editor accumulation, no cap). Same pre-connect rationale as HTN plan history (SD-008). | Accepted | Yes |
| EI-004 | Read-only in v1 — no runtime resource injection or modifier editing | Consistent with SD-ENT-016 Tier (c) across all inspector panels. Runtime resource tweaking is a separate authoring tool concern. | Accepted | Yes |
| EI-005 | Cap saturation and starvation duration computed on game side | Duration counters require contiguous tick observation. Simpler to track in C++ (increment on each tick the condition holds) than to reconstruct from the event log on the browser side. | Accepted | Yes |
| EI-006 | Derived resources clearly distinguished — purple badge, no fill bar, no income rate row | Derived resources do not earn/spend; showing a fill bar or income rate would be misleading. Visual distinction (purple) signals "computed, not accumulated." | Accepted | Yes |
| EI-007 | EconomySchemaSource sends once on connect; no change detection | Schema is a design-time JSON asset. It does not change at runtime (no hot-reload in v1). One-shot removes unnecessary hashing overhead. | Accepted | Yes |
| EI-008 | Sparkline/event-log linkage is UI-only (no extra payload) | The `history` array already carries frame-indexed samples; events already carry frame numbers. The browser can correlate them without any additional C++ work. | Accepted | Yes |
| EI-009 | Plugin in `Dia/DiaEconomyInspector/`; sources in `CluicheGameBaseline/` | Plugin is reusable engine code (any game with DiaEconomy could use it). Sources are game-specific wiring (which EconomySystem instance to read from). Same split as DiaAIInspector (SD-007). | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Topic names, instance IDs, and resource names use `StringCRC` |
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC` throughout public APIs; STL allowed in .cpp internals |
| PD-005 | Platform | x64 only | `DiaEconomyInspector.vcxproj` targets x64 |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | Compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain config | vcxproj does not override OutDir/IntDir/toolset |
| PD-009 | Platform | Generated output under `Cluiche/out/<AppName>/` | Plugin output under `Cluiche/out/CluicheEditor/EconomyInspector/` (per SED-020) |
| AD-001 | Dia App | Module YAML docs | Provide `dia.diaeconomyinspector.architecture.module.md` |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All plugin code in `Dia::EconomyInspector::` namespace |
| SED-003 | DiaEditor | Plugin lives at `Dia/DiaEconomyInspector/` (top-level peer) | Not a subdirectory of `DiaEconomy/` |
| SED-004 | DiaEditor | WebSocket protocol uses JSON | All payloads are JSON; no binary encoding |
| SED-010 | DiaEditor | Use DiaDebugProtocol for all editor-game wire types | Shared structs via DiaDebugProtocol; no ad-hoc structs in plugin or source |
| SED-015 | DiaEditor | DiaEditor is a pure C++ library — no DiaApplicationFlow dependency | Plugin does not subclass Module/Phase |
| SED-016 | DiaEditor | GameConnectionManager boots clean; Connect() is explicit | Inspector panel shows disconnected state on startup; no hang if game not running |
| SED-020 | DiaEditor | Plugin output under `Cluiche/out/CluicheEditor/<PluginName>/` | Output path: `Cluiche/out/CluicheEditor/EconomyInspector/` |
| SED-023 | DiaEditor | Plugins requiring game connection MUST derive from LiveConnectionPluginBase | `DiaEconomyInspectorPlugin` extends `LiveConnectionPluginBase`; consistent connection lifecycle and disconnected overlay |
| SD-ENT-016 | DiaEntityInspector | Tier (c) structural edit disabled | No live field editing or resource injection in v1 |

## Open Design Questions

1. **History depth memory scaling** — At 600 samples × N resources × M instances, memory grows with economy complexity. For a game with 5 factions × 8 resources, the ring is 600 × 5 × 8 × 4 bytes = 96 KB — acceptable. Should depth be auto-scaled by instance+resource count (e.g. cap total ring memory at 512 KB), or is the caller responsible for a sensible depth at construction? Auto-scaling avoids surprises as the economy grows.

2. **EconomyEventsSource — delta protocol** — On initial connect, the full ring is sent (`full_ring: true`). Subsequent pushes send only new events (`full_ring: false`). The browser must handle both cases correctly. Decide whether the delta carries a sequence number or frame watermark so the browser can detect dropped events (e.g. if a push is missed due to WebSocket backpressure).

3. **Sparkline/log linkage implementation** — The Resources tab sparkline and Events tab log are on different tabs. Consider whether the linkage should be: (a) a hover-tooltip on the sparkline showing event count at that tick, or (b) a "Jump to events" action that switches tab and scrolls the log to that frame. Option (b) is more useful but requires cross-tab state management in the React UI.

## Status

`Approved`

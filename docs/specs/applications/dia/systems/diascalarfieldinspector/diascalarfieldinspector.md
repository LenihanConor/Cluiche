# System Spec: DiaScalarFieldInspector

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai, pathfinding, spatial, debug

## Purpose

DiaScalarFieldInspector is a dockable CluicheEditor panel for inspecting scalar field state published by the running game. It follows the same `LiveConnectionPluginBase` + React/WebUIBridge pattern as `DiaBlackboardInspector` and `DiaEconomyInspector`. The game side publishes scalar field state as JSON over the live connection; this plugin receives and forwards it to a vanilla-JS web UI.

The inspector provides: a selectable list of registered fields, per-field cell statistics (min/max/mean), a heatmap grid, cell hover tooltips, a JS-side frame history ring, and write authoring controls (WritePoint / WriteRadial / WriteBox) that dispatch commands back to the game.

`IDiaScalarField` and `DiaScalarFieldAdapter` (defined in `DiaScalarField`) are **game-side serialization tools** used by a future `DiaScalarFieldBroadcaster` module to publish state over `GameConnectionManager`. They are not held by the editor plugin.

## Responsibilities

- Provide `DiaScalarFieldInspectorPlugin : public Dia::Editor::LiveConnectionPluginBase`
  - Subscribe to game topic `"scalarfield.state"` and forward payload to UI via `NotifyUIDataChanged`
  - Register WebUIBridge request handlers for write commands:
    - `"scalarfield.write_point"`, `"scalarfield.write_radial"`, `"scalarfield.write_box"`
  - Each write handler calls `GetGameConnection()->SendCommand(...)` to dispatch to game
- Provide `UI/index.html` — vanilla JS + CSS web UI (same tech as `DiaBlackboardInspector/UI/index.html`)
  - Field selector list (name, cell count)
  - Stats row: min / max / mean across all cells
  - Heatmap grid: coloured table of cell values (blue=0 → red=1)
  - Cell hover tooltip: index + exact float value
  - Frame history ring (last 60 received state payloads stored in JS, scrub slider to navigate)
  - Write authoring: WritePoint / WriteRadial / WriteBox form panels with Apply buttons
- Register plugin via `REGISTER_EDITOR_PLUGIN(DiaScalarFieldInspectorPlugin, "DiaScalarFieldInspector")`
- Provide `DiaScalarFieldInspector.vcxproj` static library registered in `Cluiche.sln` under `3.0-Gameplay`
- Provide `dia.diascalarfieldinspector.architecture.module.md` YAML module documentation

## Non-Responsibilities

- Scalar field propagation logic — DiaScalarField
- Game-side broadcasting / serialisation — future `DiaScalarFieldBroadcaster` module
- Editor layout / docking infrastructure — DiaEditor
- `IDiaScalarField` / `DiaScalarFieldAdapter` — defined in DiaScalarField module
- In-game runtime heatmap overlay — DiaScalarFieldVisualDebugger

## Public Interface

### C++ Plugin

```cpp
// DiaScalarFieldInspector/DiaScalarFieldInspectorPlugin.h
#pragma once
#include <DiaEditor/Plugin/LiveConnectionPluginBase.h>

namespace Dia::ScalarField {

class DiaScalarFieldInspectorPlugin final : public Dia::Editor::LiveConnectionPluginBase
{
public:
    DiaScalarFieldInspectorPlugin();

protected:
    void OnLivePluginLoad()   override;
    void OnLivePluginUnload() override;
    void OnGameConnected()    override;
    void OnGameDisconnected() override;

private:
    void OnScalarFieldStateUpdate(const Json::Value& payload);
    Json::Value HandleWritePoint (const Json::Value& req);
    Json::Value HandleWriteRadial(const Json::Value& req);
    Json::Value HandleWriteBox   (const Json::Value& req);
};

} // namespace Dia::ScalarField
```

### Game-side JSON topic schema (`"scalarfield.state"`)

Published by game-side broadcaster (future `DiaScalarFieldBroadcaster`):

```json
{
  "fields": [
    {
      "name":      "danger",
      "width":     20,
      "height":    20,
      "cellCount": 400,
      "cells":     [0.0, 0.12, 0.5, ...]
    }
  ]
}
```

`cells` is a flat row-major array: index = y * width + x.

### Write command request schema (UI → C++ → game)

```json
// scalarfield.write_point
{ "field": "danger", "x": 5, "y": 3, "value": 0.8 }

// scalarfield.write_radial
{ "field": "danger", "cx": 10, "cy": 10, "radius": 4.0, "peak": 1.0, "falloff": "linear" }
// falloff: "linear" | "quadratic" | "inverse"

// scalarfield.write_box
{ "field": "danger", "x": 2, "y": 2, "w": 5, "h": 5, "value": 0.6 }
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| C++ plugin | LiveConnectionPluginBase, subscribes to scalarfield.state, registers write handlers, REGISTER_EDITOR_PLUGIN | inline | Approved |
| Field selector | Left/top panel: list of received fields with name + cell count, click to select | inline | Approved |
| Statistics display | Min / max / mean across all cells in selected field | inline | Approved |
| Heatmap grid | Coloured HTML table: blue (0) → red (1) per cell; max 40×40 display (downsample larger fields) | inline | Approved |
| Cell hover tooltip | Exact float value + row/col index on mouse hover over heatmap | inline | Approved |
| Frame history ring | Store last 60 state payloads in JS; scrub slider to navigate history | inline | Approved |
| Write authoring | WritePoint / WriteRadial / WriteBox form panels, Apply dispatches to game via WebUIBridge request | inline | Approved |

## Dependencies on Other Systems

**Required:**
- **DiaEditor** — `LiveConnectionPluginBase`, `WebUIBridge`, plugin registry, docking infrastructure
- **DiaCore** — `StringCRC`, `DIA_LOG_*`
- **DiaJson** — JSON payload handling

**Game-side (not compile-time deps of this module):**
- **DiaScalarField** — `IDiaScalarField`, `DiaScalarFieldAdapter` (used by broadcaster)
- **GameConnectionManager** — topic subscription / command dispatch

**Explicitly excluded:**
- **DiaMaths** — no compile-time dependency needed in the editor plugin
- **DiaVisualDebugger** — inspector is editor-side only
- **DiaApplicationFlow** — no compile-time dependency on phase system

## Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| SFI-001 | ~~History ring buffer lives on IDiaScalarField~~ → History ring kept in JS state | Plugin uses LiveConnectionPluginBase (live-connection pattern). The editor panel does not hold local C++ field references — data arrives as JSON. JS state (last 60 `scalarfield.state` payloads) is simpler and avoids cross-thread concerns. | Accepted | Yes |
| SFI-002 | Default history depth 60 frames | At 60Hz, 1 second of history. Configurable in JS store. | Accepted | Yes |
| SFI-003 | Heatmap rendered as HTML coloured table, not GPU texture | Vanilla JS / CSS grid. Acceptable for debug inspector with bounded cell counts. For fields wider than 40 cells, display is downsampled to 40×40. | Accepted | Yes |
| SFI-004 | Write authoring dispatches via WebUIBridge request handlers → GameConnectionManager::SendCommand | Keeps write path consistent with the live-connection architecture: UI → C++ handler → game. C++ plugin registers three handlers at load time. | Accepted | Yes |
| SFI-005 | Plugin follows LiveConnectionPluginBase + vanilla JS index.html pattern | Matches DiaBlackboardInspector and DiaEconomyInspector. ImGui is for IVisualDebugger drawers, not CluicheEditor panels. | Accepted | Yes |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Topic and request type keys are StringCRC |
| PD-004 | Platform | No STL containers in public APIs | Public plugin method signatures use Dia types |
| PD-005 | Platform | x64 only | vcxproj targets x64 exclusively |
| PD-006 | Platform | vcxproj is source of truth | `.vcxproj` + `.vcxproj.filters` maintained |
| PD-007 | Platform | C++20 | Compiled under `/std:c++20` |
| PD-008 | Platform | Directory.Build.props owns toolchain | vcxproj does not override OutDir/IntDir/toolset |
| AD-001 | Dia App | Module YAML docs | Provide `dia.diascalarfieldinspector.architecture.module.md` |
| AD-003 | Dia App | `Dia::<Module>::` namespace | All C++ code in `Dia::ScalarField::` namespace |

## Open Design Questions

1. **Write authoring round-trip** — After a write command is sent, the game's next `scalarfield.state` publish will show the updated values. There is inherently one tick of latency. Is this acceptable, or should the plugin optimistically update the UI immediately while the game confirms?

2. **Large field cells array** — A 200×200 field at 60Hz pushes 40k floats per frame over the live connection. At what cell count should the game-side broadcaster switch to publishing only a downsampled version? This should be documented in the future `DiaScalarFieldBroadcaster` spec.

3. **Field identification for write commands** — Write commands include `"field": "name"`. If two fields share a name, the game-side handler would need to disambiguate. Should field names be enforced unique by the broadcaster, or should the protocol use a numeric field ID instead?

## Status

**Status:** `Approved`

**Plan:** @docs/specs/applications/dia/systems/diascalarfieldinspector/diascalarfieldinspector.plan.md

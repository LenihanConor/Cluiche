# System Spec: DiaApplicationFlowInspector

## Parent Application
@docs/specs/applications/dia.md

## Mockup
@Dia/DiaApplicationFlowInspector/UI/mockup.html

## Purpose

DiaApplicationFlowInspector is a dockable CluicheEditor plugin that provides live runtime inspection of a connected game's application flow. It connects to a running game via the shared GameConnectionManager, subscribes to telemetry topics, and presents actionable debugging information.

This plugin is the **runtime counterpart** to [DiaApplicationFlowEditor](diaapplicationfloweditor.md) (the asset editor). The Editor handles static `.diaapp` file editing; the Inspector handles live game state observation and control.

**Location:** `Dia/DiaApplicationFlowInspector/`
**Layout:** `kDockable`

## Responsibilities

### Connection Lifecycle
- Own the `live.*` bridge request handlers (connect, disconnect, getStatus, transitionTo, shutdown)
- Call `GameConnectionManager::Connect/Disconnect` and manage the connection callback
- Subscribe to `app.state`, `app.modules`, `app.streams` game topics
- Publish pushed topics (`live.connectionStatus`, `live.state`, `live.modules`, `live.streams`) to the shared WebUIBridge bus so the Editor can consume them read-only

### Runtime Observation (derived from existing pushes, no module author work)
- **Stage Timeline** — Accumulate `app.state` pushes into a timestamped breadcrumb trail (Boot → Menu → Gameplay with durations)
- **Module Lifecycle** — Track state transition timestamps per module; derive "in Loading for 3.4s / 5.0s timeout" from time-in-state vs manifest-declared timeout
- **Dependency Blocking** — Cross-reference manifest dependencies with live states: "AudioModule waiting on AssetLoader (Loading)"
- **Event Log** — Ring buffer of all state changes, warnings, errors. Monospace, selectable, copyable text.

### Runtime Observation (requires ~25 lines of framework code)
- **Stream Backpressure** — Display queue fill % (currentSize / capacity), drops/sec. Requires stream runtime to broadcast `currentSize` and `dropsTotal` in its telemetry.
- **PU Frame Budget** — Display tick duration vs target period (e.g. 14.2ms / 16.6ms = 85%). Requires PU runtime to broadcast `lastTickMs` in its telemetry.

### Commands
- **Stage Transition** — Trigger `transition_to` command on connected game
- **Shutdown** — Send graceful `shutdown` command

## Public Interfaces

### Plugin Class

```cpp
namespace Dia::Editor {
    class DiaApplicationFlowInspectorPlugin : public IEditorPlugin {
    public:
        const char* GetName() const override        { return "Application Flow Inspector"; }
        const char* GetVersion() const override     { return "1.0"; }
        const char* GetDescription() const override { return "Live runtime inspection panel for connected game instances"; }
        const char* GetUIPath() const override      { return "dia://plugins/diaapplicationflowinspector/index.html"; }
        LayoutMode GetLayoutMode() const override   { return LayoutMode::kDockable; }
    };
}
```

### Bridge Request Handlers

| Request | Purpose |
|---------|---------|
| `live.connect` | Initiate WebSocket connection to game (host, port) |
| `live.disconnect` | Graceful disconnect |
| `live.getStatus` | Query connection state + whether live data is active |
| `live.transitionTo` | Send `transition_to` command to game |
| `live.shutdown` | Send `shutdown` command to game |

### Topics Published (to shared WebUIBridge bus)

| Topic | Data | Consumers |
|-------|------|-----------|
| `live.connectionStatus` | `{connected, host, port}` | Editor (connection indicator), Inspector UI |
| `live.state` | `{stage, transitioning, targetStage}` | Editor (grid highlight), Inspector (breadcrumb) |
| `live.modules` | Module state array | Editor (traffic lights), Inspector (module cards) |
| `live.streams` | Stream metrics array | Editor (throughput), Inspector (backpressure bars) |

## Dependencies

| Module | Purpose |
|--------|---------|
| DiaEditor | IEditorPlugin, WebUIBridge, GameConnectionManager, PluginServiceLocator |
| DiaCore | StringCRC, containers |
| DiaJson | JSON parsing for WebSocket messages |
| DiaObservation | Metrics, logging |

**Does NOT depend on:** DiaApplicationFlow (no manifest access needed — all data comes from live pushes)

## Non-Responsibilities

- **Manifest editing** — DiaApplicationFlowEditor handles that
- **File I/O** — Inspector has no files to save
- **Validation** — manifest validation is the Editor's domain
- **Module-specific diagnostics** — Inspector shows generic lifecycle info only; module internals require per-module opt-in (out of scope)

## Features

| Feature | Description | Status |
|---------|-------------|--------|
| Connection Lifecycle | `live.*` handlers, GameConnectionManager integration, topic publishing | To implement (extracted from Editor) |
| Stage Breadcrumb | Timestamped trail of stage transitions with durations | To implement |
| Module Lifecycle Cards | State badge + time-in-state + timeout progress bar + dependency blocking | To implement |
| Stream Backpressure | Queue fill bars (%), drops/sec, capacity display | To implement (needs ~5 lines in stream runtime) |
| PU Frame Budget | Tick duration gauge vs target frequency | To implement (needs ~10 lines in PU runtime) |
| Event Log | Selectable monospace ring buffer — timestamps, state changes, warnings, errors | To implement |
| Stage Transition Trigger | Dropdown + Trigger button, feedback on completion | To implement (extracted from Editor) |
| Shutdown Command | Button to send graceful shutdown to game | To implement (extracted from Editor) |

## Decisions

| ID | Decision | Rationale |
|----|----------|-----------|
| DAFI-001 | Inspector is `kDockable`, not fullscreen | Matches pattern of DiaAssetRuntimeInspector; sits alongside Editor |
| DAFI-002 | All derived data (timeline, lifecycle durations, dependency blocking) computed Inspector-side from existing pushes | Zero changes to game-side code for these features |
| DAFI-003 | Stream `currentSize` + `dropsTotal` broadcast added to generic stream pump | Every stream gets backpressure for free; no module author action |
| DAFI-004 | PU `lastTickMs` broadcast added to ProcessingUnit::Update | Every PU gets frame budget for free; no module author action |
| DAFI-005 | Event log is plain-text, selectable, copyable | Primary debug artifact — must be pasteable into bug reports |
| DAFI-006 | No shared npm package with Editor; duplicate `bridge.ts` + `TrafficLightDot.tsx` | Both are ~36 lines, never change; avoids workspace complexity |
| DAFI-007 | Inspector UI deps: react + zustand only (no d3, no reactflow, no codemirror) | Much lighter bundle; Inspector is a panel, not a full-tab app |
| DAFI-008 | Framework telemetry always measures; broadcasts only when subscribed | Measurement cost (~200ns/frame total) is noise; no compile flag needed. If profiling shows an issue later, revisit. |
| DAFI-009 | GameConnectionManager always exists (framework-level); Inspector owns connect/disconnect lifecycle but Editor can query IsConnected() at any time | Connection state is framework-owned, not plugin-owned. Inspector being closed doesn't remove the connection object. |
| DAFI-010 | Event log ring buffer: 1024 entries | Balances memory and scroll UX. Oldest entries drop silently. |

## Framework Changes Required

These are the only changes needed outside of `Dia/DiaApplicationFlowInspector/`:

| Location | Change | Lines |
|----------|--------|-------|
| Stream runtime (generic pump) | Broadcast `currentSize` and `dropsTotal` in stream telemetry topic | ~5 |
| `ProcessingUnit::Update()` | Measure and broadcast `lastTickMs` in PU telemetry topic | ~10 |
| `DiaApplicationEditor` (now `DiaApplicationFlowEditor`) | Remove `HandleLive*` methods, `LiveStateStore` member, `mGameConnection->Update()` | Deletion only |

## Inherited Binding Decisions

| ID | Source | Decision | Implication |
|----|--------|----------|-------------|
| PD-001 | Platform | StringCRC for all IDs | Module/stream/stage IDs arrive as CRC strings |
| ED-007 | Editor | React + CEF frontend | Consistent with CluicheEditor |
| ED-008 | Editor | Single TrafficLightDot primitive | Reuse same visual vocabulary |

## Status

**Status:** Done — 2026-06-07. All 49 plan tasks complete. Inspector extracts live connection lifecycle from Editor; 4-tab UI (Modules/Streams/Timing/Log); framework telemetry; Editor renamed to DiaApplicationFlowEditor.

**Plan:** [diaapplicationflowinspector.plan.md](diaapplicationflowinspector.plan.md)

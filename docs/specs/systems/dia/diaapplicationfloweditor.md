# System Spec: DiaApplicationFlowEditor

## Parent Application
@docs/specs/applications/dia.md

## Supersedes
@docs/specs/systems/dia/diaapplicationeditor.md

## Research
@docs/research/diapp_simplif/summary.md

## Mockup
@docs/research/diapp_simplif/editor_mockup_v4.html

## Purpose

DiaApplicationFlowEditor is a CluicheEditor plugin that provides visual editing of `.diaapp` v2 manifests and live runtime inspection of DiaApplicationFlow state. It replaces the original DiaApplicationEditor which was designed around the Phase-based model.

The editor serves two modes:
1. **Static Mode** — Load, visualize, edit, validate, and save `.diaapp` manifest files. No running game required.
2. **Live Mode** — Connect to a running game via WebSocket/DiaAPI. Overlay runtime state (current stage, module states, stream throughput) onto the static view. Trigger transitions and inspect state.

**Location:** `Dia/DiaApplicationEditor/` — implements IEditorPlugin from DiaEditor framework.

## Responsibilities

### Static Editing
- Load and save `.diaapp` v2 manifests (via DiaSerializer)
- Display PU graph with stream connections as edges
- Display module inspector for selected PU (deps, timeouts, stream handles, stage membership, provenance)
- Display stream inspector for selected stream (type, from/to PU, readers/writers)
- Display full-tab module presence grid (modules × stages matrix)
- Edit PU properties (frequency, thread, startup order)
- Edit stage configuration (add/remove stages, auto/manual trigger)
- Edit module configuration (add/remove, assign stages, set deps, configure timeouts)
- Edit stream configuration (add/remove, set kind/payloadType/from/to, capacity, maxReaders)
- Real-time validation with error/warning display
- Undo/redo for all edits
- File conflict detection (external modification)
- Dirty state tracking and backup on save (.bak)
- Type discovery from TypeRegistry (available module types for add)

### Live Mode
- Connect to running game via DiaEditor's GameConnectionManager (WebSocket)
- Overlay current stage (highlighted in UI) on static graph
- Show per-module state (running/loading/stopped/failed) badges
- Show transition progress (which modules are starting/stopping)
- Trigger `transition_to` command via DiaAPI
- Query `get_app_state` and `get_active_modules`
- Block-wait `wait_stage_ready` for automation scripts

### Integration
- Implement IEditorPlugin interface (DiaEditor framework)
- Register as panel in CluicheEditor
- React + CEF frontend (consistent with other editor panels)

## Public Interfaces

### Plugin
```cpp
namespace Dia::ApplicationFlow::Editor {
    class DiaApplicationFlowEditorPlugin : public Dia::Editor::IEditorPlugin {
    public:
        static constexpr Dia::Core::StringCRC kPluginId{"DiaApplicationEditor"};

        const char* GetName() const override;
        const char* GetPanelTitle() const override;
        void Initialize(Dia::Editor::IEditorContext* context) override;
        void Shutdown() override;
        void OnActivate() override;
        void OnDeactivate() override;
    };
}
```

### DiaAPI Commands (consumed from game via WebSocket)
| Command | Direction | Purpose |
|---------|-----------|---------|
| `get_app_state` | Editor → Game | Returns current stage, PU list, transition state |
| `get_active_modules` | Editor → Game | Returns per-PU module state list |
| `transition_to` | Editor → Game | Trigger app-wide stage transition |
| `wait_stage_ready` | Editor → Game | Blocking — responds when transition completes |
| `get_stream_info` | Editor → Game | Returns stream metadata and throughput |
| `request_shutdown` | Editor → Game | Trigger graceful shutdown |

### Frontend (React Components)
| Component | Tab | Description |
|-----------|-----|-------------|
| `GraphView` | Graph | PU nodes, stream edges, click-to-select, drag-to-reposition |
| `ModulePresenceGrid` | Presence | Full-tab modules × stages matrix with active/inactive indicators |
| `PUInspector` | (sidebar) | Properties, stages, modules for selected PU |
| `ModuleInspector` | (sidebar) | Details for selected module (deps, streams, timeouts, provenance) |
| `StreamInspector` | (sidebar) | Details for selected stream (kind, payloadType, from/to PU, capacity, maxReaders, readers/writers) |
| `StageConfig` | (sidebar) | Stage properties (auto/manual, ordering) |
| `ValidationBar` | (footer) | Errors/warnings with click-to-navigate |
| `LiveOverlay` | (all views) | Runtime state badges overlaid when connected |

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Graph View | PU nodes with stream edges, click-select, drag-reposition, startup order badges | [graph-view.md](../../features/dia/diaapplicationfloweditor/graph-view.md) | Approved |
| Module Presence Grid | Full-tab modules × stages matrix, infrastructure vs stage-specific visual distinction, per-PU filtering | [module-presence-grid.md](../../features/dia/diaapplicationfloweditor/module-presence-grid.md) | Approved |
| PU Inspector | Editable PU properties (frequency, thread, order), stage list, module list with cards | [pu-inspector.md](../../features/dia/diaapplicationfloweditor/pu-inspector.md) | Approved |
| Module Inspector | Deps, stream handles, timeout config, stage presence dots, provenance from .diastage | [module-inspector.md](../../features/dia/diaapplicationfloweditor/module-inspector.md) | Approved |
| Stream Inspector | Type, from/to PU, readers/writers list, add/remove/edit | [stream-inspector.md](../../features/dia/diaapplicationfloweditor/stream-inspector.md) | Approved |
| Stage Configuration | Add/remove stages, auto/manual trigger, ordering | [stage-configuration.md](../../features/dia/diaapplicationfloweditor/stage-configuration.md) | Approved |
| Stages Tab | First top-level tab — left-to-right stage transition graph; auto-edges only; live overlay (active-stage pulse + animated auto edge); read-only (edits stay in StageConfiguration sidebar) | [stages-tab.md](../../features/dia/diaapplicationfloweditor/stages-tab.md) | Approved |
| Manifest Load/Save | Load .diaapp v2, dirty tracking, .bak backup, format validation on save | [manifest-load-save.md](../../features/dia/diaapplicationfloweditor/manifest-load-save.md) | Approved |
| Real-Time Validation | Dependency cycles, orphaned modules, missing/undeclared stream refs (`UNKNOWN_STREAM_IN_READS/WRITES`), orphan reader/writer streams (`ORPHAN_*`), missing payload type (`PAYLOAD_TYPE_MISSING`), stage coverage gaps | [real-time-validation.md](../../features/dia/diaapplicationfloweditor/real-time-validation.md) | Approved |
| Validation Fix Suggestions | Click-to-navigate from issues to target PU/module/stream; one-click "Fix" buttons for mechanical resolutions (remove unknown read/write, remove orphan stream, assign stage) | [validation-fix-suggestions.md](../../features/dia/diaapplicationfloweditor/validation-fix-suggestions.md) | Approved |
| Undo/Redo | Command pattern for all edits, Ctrl+Z/Y, history display | [undo-redo.md](../../features/dia/diaapplicationfloweditor/undo-redo.md) | Approved |
| File Conflict Detection | Watch .diaapp file for external changes, prompt reload/overwrite | [file-conflict-detection.md](../../features/dia/diaapplicationfloweditor/file-conflict-detection.md) | Approved |
| Type Discovery | Query TypeRegistry for available module/PU types, autocomplete in add dialogs | [type-discovery.md](../../features/dia/diaapplicationfloweditor/type-discovery.md) | Approved |
| Live Connection | WebSocket connect/disconnect, connection status indicator | [live-connection.md](../../features/dia/diaapplicationfloweditor/live-connection.md) | Approved |
| Live State Overlay | Current stage highlight, module state badges (running/loading/stopped/failed), transition progress | [live-state-overlay.md](../../features/dia/diaapplicationfloweditor/live-state-overlay.md) | Approved |
| Live Transition Trigger | "Transition To" button/command, stage selector, wait-for-ready feedback | [live-transition-trigger.md](../../features/dia/diaapplicationfloweditor/live-transition-trigger.md) | Approved |
| Risky Change Warnings | Warn when removing modules with dependents, breaking stream connections, etc. | [risky-change-warnings.md](../../features/dia/diaapplicationfloweditor/risky-change-warnings.md) | Approved |

## Platform Primitives Used

- **DiaEditor** — IEditorPlugin, IEditorContext, GameConnectionManager
- **DiaSerializer** — ISerializer for .diaapp file I/O
- **DiaDebugProtocol** — Shared message types for WebSocket communication
- **DiaCore/CRC** — StringCRC for ID matching between editor and runtime
- **CEF** — Chromium Embedded Framework for React UI rendering
- **React** — Frontend framework (consistent with other CluicheEditor panels)

## Dependencies on Other Systems

**Required:**
- **DiaEditor** — Plugin framework, panel hosting, WebSocket connection management
- **DiaSerializer** — File I/O for .diaapp manifests
- **DiaApplicationFlow** — Manifest schema definition, IApplicationInspectable (for live mode)
- **DiaDebugProtocol** — Message types for editor-game communication
- **DiaDebugServer** — Game-side WebSocket server that handles commands

**Consumers:**
- **CluicheEditor** — Hosts this plugin as a panel
- **Developers** — Primary users editing application flow

## Out of Scope

- **Module code editing** — this is a manifest editor, not an IDE
- **Asset pipeline integration** — asset loading is AssetRuntime's concern
- **Performance profiling** — separate profiler tool
- **Visual debugging of game state** — DiaVisualDebugger handles that
- **Creating new module classes** — scaffolding is DiaCLI's job
- **Multi-user editing** — single editor instance

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| ED-001 | Module Presence Grid is a full tab, not a panel | Scales to many modules (20+). Needs horizontal space for stages and vertical for module list. | Presence Grid | Accepted | Yes |
| ED-002 | Three-tab layout: Graph, Presence, Streams | Clean separation — Graph for PU topology, Presence for module×stage matrix, Streams for dataflow. Inspector sidebar adapts to selection. | All features | Accepted | Yes |
| ED-003 | Live mode is overlay on static view, not separate mode | Same layout with runtime badges/highlights. No mode-switching confusion. Single mental model. | Live features | Accepted | Yes |
| ED-004 | Validation runs on every edit (debounced 500ms) | Catches errors immediately. No "validate" button needed for normal workflow (button exists for explicit re-run). | Validation | Accepted | Yes |
| ED-005 | "all" modules visually distinct from stage-specific | Green "all stages" badge vs. individual dots. Instantly distinguishes infrastructure from game modules. | Presence Grid, Inspector | Accepted | Yes |
| ED-006 | Provenance shown for stage-imported modules | "from: dummy_stage.diastage" in gold — so you know which file to edit for that module. | Inspector | Accepted | Yes |
| ED-007 | React + CEF frontend, consistent with CluicheEditor | Reuse existing editor infrastructure. No new framework. | All features | Accepted | Yes |
| ED-008 | Single `.tl` traffic-light dot primitive (grey/amber/green/red + `pulse`) used across all views | One visual vocabulary for state everywhere — PU graph, presence grid, stream detail, sidebar. No alternate state indicators. | All features | Accepted | Yes |
| ED-009 | Live button is 3-state in the header: grey ("Connect Live") → amber+pulse ("Connecting…") → green+pulse ("Live: \<AppName\>") | Single control for connection lifecycle. State is always visible in header regardless of active tab. | Live features | Accepted | Yes |
| ED-010 | Stream labels on the graph navigate to Streams tab on click — no stream inspector panel on the graph | Streams tab is the sole stream detail surface. No dual surface. Hint text ("→ Streams tab") shown on hover. | Graph View, Streams | Accepted | Yes |
| ED-011 | Add PU affordance is a dashed ghost node on the graph canvas, not a toolbar button | In-context affordance — developer sees where the new PU will appear relative to existing topology. | Graph View | Accepted | Yes |
| ED-012 | Dependency Order section in the PU inspector is collapsed by default | Reduces visual noise. Advanced detail available on demand. | Module Inspector, PU Inspector | Accepted | Yes |
| ED-013 | No explicit Validate button — validation is always-on (debounced 500ms) | Supersedes ED-004's "button exists for explicit re-run." Errors appear immediately; no manual trigger needed. | Real-Time Validation | Accepted | Yes |
| ED-014 | Presence Grid has two visual modes — Offline (all columns: green=required, grey=not assigned) and Live (active-stage column: green/amber/red+pulse = runtime state; inactive columns: outline-green=required, grey=not assigned; active-stage header highlighted) | Clean separation of config truth vs runtime state. Offline mode is always available; Live mode overlays runtime without replacing the config view. | Module Presence Grid | Accepted | Yes |
| ED-015 | Four-tab layout: **Stages, Process Units, Modules, Streams** (Stages is the default active tab) | Supersedes ED-002. Stages is the outermost layer of the application's mental model — stages contain modules, modules use streams. Reading the tab bar left-to-right gives a natural top-down decomposition. Stages renders a transition graph (read-only); editing remains in the sidebar `StageConfiguration`. | All features (tab-bar wiring); Stages Tab | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all IDs | Editor displays and edits StringCRC-based IDs. Manifest stores string names that map to CRC. |
| PD-004 | Platform | No STL in public APIs | C++ plugin API uses DiaCore containers. Frontend (React/JS) is exempt. |
| PD-006 | Platform | VS project files are source of truth | DiaApplicationEditor.vcxproj manually maintained. |
| PD-007 | Platform | C++20 required | Plugin code uses C++20. |
| PD-010 | Platform | .diagame is root, .diastage declares stage metadata | Editor understands file hierarchy. Shows provenance for stage-imported modules. |
| AD-001 | Dia App | YAML frontmatter module docs | dia.applicationeditor.architecture.module.md maintained. |
| AD-003 | Dia App | Namespace Dia::<Module>:: | Plugin code in Dia::ApplicationFlow::Editor:: namespace. |
| SD-001 | DiaApp v2 | Config is sole source of truth | Editor IS the config editor. What you see is what the runtime loads. |
| SD-002 | DiaApp v2 | Stages replace Phases | Editor visualizes stages, not phases. |
| SD-006 | DiaApp v2 | Streams in config, framework-owned | Editor visualizes and edits stream declarations. |
| SD-014 | DiaApp v2 | Full validation at load | Editor validates same rules as runtime. Same errors surfaced. |
| SD-018 | DiaApp v2 | Reserved `$`-prefix streams are framework-auto-created; user `$`-prefix forbidden | Editor shows `$`-streams as read-only; blocks creation of streams with `$` prefix. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Layout | Should the sidebar inspector be fixed-width or resizable? | Resizable with a sensible default (~580px). Collapse button for full-width graph/presence view. |
| 2 | Live Mode | Should live mode auto-connect when a game is detected, or require manual connect? | Manual connect via button. Auto-connect could be surprising if multiple games running. |
| 3 | Presence Grid | How should the grid handle 50+ modules? | Virtual scrolling for rows. Column headers (stages) sticky. Row grouping by PU with collapsible sections. |
| 4 | Validation | Should validation block save, or just warn? | Errors block save (manifest would fail at runtime). Warnings allow save with confirmation dialog. |
| 5 | Undo/Redo | How deep should the undo stack be? | 100 operations. Matches typical editor conventions. Stack cleared on file reload. |
| 6 | Streams tab | What does the Streams tab show that the Graph doesn't? | Detailed list view: all streams with type, from/to, all readers/writers, throughput (live mode). Graph shows topology; Streams tab shows details. |
| 7 | Type Discovery | Where does the editor get the list of available module types? | From TypeRegistry at editor startup (if game connected) or from a generated `types.json` file (static mode). DiaCLI can generate this file. |
| 8 | File format | Should the editor preserve JSON formatting/comments or reformat on save? | Reformat to canonical style on save. JSON doesn't support comments. Consistent output enables diffing. |

## Status

`Done` — 2026-05-20. Supersedes DiaApplicationEditor v1. Plan: [diaapplicationfloweditor.plan.md](diaapplicationfloweditor.plan.md)
